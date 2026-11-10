// nn::audio AudioOut (audout:u): a PCM stream the game feeds directly, next to the audio renderer. Scarlet opens the
// default output from the main module and from subsdk0. Every public AudioOut call is overridden, so the host owns
// both objects: AudioOut word 0 holds the host stream, and AudioOutBuffer words 0-2 hold data, capacity and size
// (SetAudioOutBufferInfo). As on the hardware, an appended buffer is read when it plays, not when it is appended
// (Bink, the movie player, may fill a buffer after appending it): the SDL3 device callback copies from the queued
// buffers' memory and releases each one it finishes. Without a device (TOTK_AUDIO=none, or no output), playback is
// timed by the clock so the game sees buffers come back at the real rate.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "cfg.h"
#include "host.h"

#define OUT_RATE 48000
#define FMT_PCM16 2                    // nn::audio::SampleFormat_PcmInt16
#define STATE_STARTED 0                // nn::audio::AudioOutState
#define STATE_STOPPED 1
#define MAX_QUEUED 64

typedef struct { uint64_t data, capacity, size; } OutBuffer;   // the host's AudioOutBuffer layout

typedef struct {
    PlatMutex lock;
    SDL_AudioStream *stream;
    int channels, rate, state;
    uint64_t played, clock_ns;                // bytes played; clock playback: time accounted up to
    OutBuffer *queued[MAX_QUEUED], *released[MAX_QUEUED];
    uint64_t head_used;                       // bytes of queued[0] already played
    int nqueued, nreleased;
    uint64_t appends;
} Out;

static Out *out_of(Cpu *c, int i) {
    uint64_t *o = arg_p(c, i);
    if (!o || !*o) rt_fatal("nn::audio: AudioOut is not open");
    return (Out *)(uintptr_t)*o;
}

static int frame_bytes(const Out *o) { return 2 * o->channels; }

// Plays up to n bytes from the queued buffers: copies them to dst (NULL: clock playback, nothing copied), releases
// each finished buffer. Returns the bytes taken; lock held.
static uint64_t consume(Out *o, uint8_t *dst, uint64_t n) {
    uint64_t done = 0;
    while (done < n && o->nqueued) {
        OutBuffer *b = o->queued[0];
        uint64_t left = b->size > o->head_used ? b->size - o->head_used : 0, k = n - done < left ? n - done : left;
        if (dst && k) memcpy(dst + done, (const uint8_t *)(uintptr_t)b->data + o->head_used, k);
        done += k;
        o->head_used += k;
        if (o->head_used >= b->size) {
            memmove(o->queued, o->queued + 1, (size_t)--o->nqueued * sizeof *o->queued);
            o->head_used = 0;
            if (o->nreleased < MAX_QUEUED) o->released[o->nreleased++] = b;
        }
    }
    o->played += done;
    return done;
}

// clock playback: plays what the elapsed time allows; lock held
static void clock_advance(Out *o) {
    if (o->stream || o->state != STATE_STARTED) return;
    uint64_t now = plat_now_ns(), fb = (uint64_t)frame_bytes(o);
    uint64_t frames = (now - o->clock_ns) * (uint64_t)o->rate / 1000000000u;
    if (!frames) return;
    o->clock_ns += frames * 1000000000u / (uint64_t)o->rate;
    consume(o, NULL, frames * fb);
}

// SDL device callback: the device wants `additional` more bytes
static void SDLCALL device_pull(void *ud, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    Out *o = ud;
    uint8_t tmp[8192];
    while (additional > 0) {
        int want = additional < (int)sizeof tmp ? additional : (int)sizeof tmp;
        want -= want % frame_bytes(o);
        if (want <= 0) break;
        plat_mutex_lock(&o->lock);
        uint64_t got = o->state == STATE_STARTED ? consume(o, tmp, (uint64_t)want) : 0;
        plat_mutex_unlock(&o->lock);
        if (!got) break;   // underrun: the device plays silence
        SDL_PutAudioStreamData(stream, tmp, (int)got);
        additional -= (int)got;
    }
}

// void InitializeAudioOutParameter(AudioOutParameter *): int sample rate, u16 channel count (0: the defaults)
static void ov_param_init(Cpu *c) {
    uint8_t *p = arg_p(c, 0);
    if (p) memset(p, 0, 8);
}

// Result OpenDefaultAudioOut(AudioOut *, const AudioOutParameter &)
static void ov_open(Cpu *c) {
    uint64_t *obj = arg_p(c, 0);
    const uint8_t *p = arg_p(c, 1);
    if (!obj) rt_fatal("nn::audio::OpenDefaultAudioOut: null AudioOut");
    Out *o = calloc(1, sizeof *o);
    if (!o) rt_fatal("nn::audio::OpenDefaultAudioOut: out of memory");
    plat_mutex_init(&o->lock);
    int32_t rate = 0;
    uint16_t ch = 0;
    if (p) memcpy(&rate, p, 4), memcpy(&ch, p + 4, 2);
    o->rate = rate > 0 ? rate : OUT_RATE;
    o->channels = ch == 6 ? 6 : 2;
    o->state = STATE_STOPPED;
    const char *mode = cfg_get("TOTK_AUDIO");
    if (!((mode && !strcmp(mode, "none")) || cfg_get("TOTK_AUDIO_OFF"))) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            SDL_AudioSpec spec = {SDL_AUDIO_S16, o->channels, o->rate};
            o->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, device_pull, o);
            if (!o->stream) SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        if (!o->stream) rt_log("audout: no output device (%s): timed by the clock", SDL_GetError());
    }
    rt_log("audout: open %d Hz, %d channels%s", o->rate, o->channels, o->stream ? "" : " (no device)");
    *obj = (uint64_t)(uintptr_t)o;
    ret_x(c, 0);
}

// void CloseAudioOut(AudioOut *)
static void ov_close(Cpu *c) {
    uint64_t *obj = arg_p(c, 0);
    if (!obj || !*obj) return;
    Out *o = (Out *)(uintptr_t)*obj;
    if (o->stream) SDL_DestroyAudioStream(o->stream), SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(o);
    *obj = 0;
}

// Result StartAudioOut(AudioOut *) / void StopAudioOut(AudioOut *)
static void ov_start(Cpu *c) {
    Out *o = out_of(c, 0);
    plat_mutex_lock(&o->lock);
    if (o->state != STATE_STARTED) {
        o->clock_ns = plat_now_ns();
        o->state = STATE_STARTED;
        if (o->stream) SDL_ResumeAudioStreamDevice(o->stream);
    }
    plat_mutex_unlock(&o->lock);
    ret_x(c, 0);
}
static void ov_stop(Cpu *c) {
    Out *o = out_of(c, 0);
    plat_mutex_lock(&o->lock);
    if (o->state == STATE_STARTED) {
        clock_advance(o);
        o->state = STATE_STOPPED;
        if (o->stream) SDL_PauseAudioStreamDevice(o->stream);
    }
    plat_mutex_unlock(&o->lock);
}

static void ov_state(Cpu *c) { ret_x(c, (uint64_t)out_of(c, 0)->state); }
static void ov_channels(Cpu *c) { ret_x(c, (uint64_t)out_of(c, 0)->channels); }
static void ov_rate(Cpu *c) { ret_x(c, (uint64_t)out_of(c, 0)->rate); }
static void ov_format(Cpu *c) { (void)out_of(c, 0); ret_x(c, FMT_PCM16); }

// uint64_t GetAudioOutPlayedSampleCount(const AudioOut *)
static void ov_played_samples(Cpu *c) {
    Out *o = out_of(c, 0);
    plat_mutex_lock(&o->lock);
    clock_advance(o);
    uint64_t n = o->played / (uint64_t)frame_bytes(o);
    plat_mutex_unlock(&o->lock);
    ret_x(c, n);
}

// bool AppendAudioOutBuffer(AudioOut *, AudioOutBuffer *): queued; its memory is read when it plays
static void ov_append(Cpu *c) {
    Out *o = out_of(c, 0);
    OutBuffer *b = arg_p(c, 1);
    if (!b) rt_fatal("nn::audio::AppendAudioOutBuffer: null buffer");
    plat_mutex_lock(&o->lock);
    clock_advance(o);
    int ok = o->nqueued + o->nreleased < MAX_QUEUED;
    if (ok) o->queued[o->nqueued++] = b;
    uint64_t n = ++o->appends;
    plat_mutex_unlock(&o->lock);
    if (n == 1 || n == 100 || n % 3000 == 0)   // a sign of life while the stream runs
        rt_log("audout: %llu buffers appended (this one %llu bytes), %llu bytes played", (unsigned long long)n,
               (unsigned long long)b->size, (unsigned long long)o->played);
    ret_x(c, (uint64_t)ok);
}

// AudioOutBuffer *GetReleasedAudioOutBuffer(AudioOut *): the oldest buffer the device has played through
static void ov_released(Cpu *c) {
    Out *o = out_of(c, 0);
    OutBuffer *b = NULL;
    plat_mutex_lock(&o->lock);
    clock_advance(o);
    if (o->nreleased) {
        b = o->released[0];
        memmove(o->released, o->released + 1, (size_t)--o->nreleased * sizeof *o->released);
    }
    plat_mutex_unlock(&o->lock);
    ret_x(c, (uint64_t)(uintptr_t)b);
}

// void SetAudioOutBufferInfo(AudioOutBuffer *, void *data, size_t capacity, size_t size)
static void ov_buffer_info(Cpu *c) {
    OutBuffer *b = arg_p(c, 0);
    if (!b) rt_fatal("nn::audio::SetAudioOutBufferInfo: null buffer");
    b->data = arg_x(c, 1);
    b->capacity = arg_x(c, 2);
    b->size = arg_x(c, 3);
}
static void ov_buffer_data(Cpu *c) { const OutBuffer *b = arg_p(c, 0); ret_x(c, b ? b->data : 0); }
static void ov_buffer_size(Cpu *c) { const OutBuffer *b = arg_p(c, 0); ret_x(c, b ? b->size : 0); }

HOST_OVERRIDE("_ZN2nn5audio27InitializeAudioOutParameterEPNS0_17AudioOutParameterE", ov_param_init);
HOST_OVERRIDE("_ZN2nn5audio19OpenDefaultAudioOutEPNS0_8AudioOutERKNS0_17AudioOutParameterE", ov_open);
HOST_OVERRIDE("_ZN2nn5audio13CloseAudioOutEPNS0_8AudioOutE", ov_close);
HOST_OVERRIDE("_ZN2nn5audio13StartAudioOutEPNS0_8AudioOutE", ov_start);
HOST_OVERRIDE("_ZN2nn5audio12StopAudioOutEPNS0_8AudioOutE", ov_stop);
HOST_OVERRIDE("_ZN2nn5audio16GetAudioOutStateEPKNS0_8AudioOutE", ov_state);
HOST_OVERRIDE("_ZN2nn5audio23GetAudioOutChannelCountEPKNS0_8AudioOutE", ov_channels);
HOST_OVERRIDE("_ZN2nn5audio21GetAudioOutSampleRateEPKNS0_8AudioOutE", ov_rate);
HOST_OVERRIDE("_ZN2nn5audio23GetAudioOutSampleFormatEPKNS0_8AudioOutE", ov_format);
HOST_OVERRIDE("_ZN2nn5audio28GetAudioOutPlayedSampleCountEPKNS0_8AudioOutE", ov_played_samples);
HOST_OVERRIDE("_ZN2nn5audio20AppendAudioOutBufferEPNS0_8AudioOutEPNS0_14AudioOutBufferE", ov_append);
HOST_OVERRIDE("_ZN2nn5audio25GetReleasedAudioOutBufferEPNS0_8AudioOutE", ov_released);
HOST_OVERRIDE("_ZN2nn5audio21SetAudioOutBufferInfoEPNS0_14AudioOutBufferEPvmm", ov_buffer_info);
HOST_OVERRIDE("_ZN2nn5audio28GetAudioOutBufferDataPointerEPKNS0_14AudioOutBufferE", ov_buffer_data);
HOST_OVERRIDE("_ZN2nn5audio25GetAudioOutBufferDataSizeEPKNS0_14AudioOutBufferE", ov_buffer_size);
