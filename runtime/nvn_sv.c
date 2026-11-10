// NVN procs Scarlet/Violet ask for that TotK never called: getters of objects the front end already stores
// (nvn_obj.h), debug and checkpoint controls with no host effect, and waits the host's in-order submission already
// satisfies. Procs that do GPU work (tessellation levels, indirect multi-draw, tiled downsample)
// are not here on purpose: they keep the weak stub in nvn_procs.c, which stops with the proc's name, so the first
// game path that needs one is found instead of drawing wrong.
#include <stdint.h>
#include <string.h>

#include "gfx.h"
#include "host.h"
#include "nvn_api.h"

// ---------- Texture and sampler pools (dpool_t)
#define DP(c) ((const dpool_t *)arg_p(c, 0))
// const NVNmemoryPool *nvnTexturePoolGetMemoryPool(const NVNtexturePool *)
NVN_API(nvnTexturePoolGetMemoryPool) { ret_x(c, (uint64_t)(uintptr_t)DP(c)->pool); }
// ptrdiff_t nvnTexturePoolGetMemoryOffset(const NVNtexturePool *)
NVN_API(nvnTexturePoolGetMemoryOffset) { ret_x(c, (uint64_t)DP(c)->off); }
// int nvnTexturePoolGetSize(const NVNtexturePool *): the descriptor count it was initialized with
NVN_API(nvnTexturePoolGetSize) { ret_x(c, (uint64_t)(int64_t)DP(c)->n); }
NVN_API(nvnSamplerPoolGetMemoryPool) { ret_x(c, (uint64_t)(uintptr_t)DP(c)->pool); }
NVN_API(nvnSamplerPoolGetMemoryOffset) { ret_x(c, (uint64_t)DP(c)->off); }
NVN_API(nvnSamplerPoolGetSize) { ret_x(c, (uint64_t)(int64_t)DP(c)->n); }
#undef DP
// void nvnTexturePoolRegisterImage(const NVNtexturePool *, int id, const NVNtexture *, const NVNtextureView *): image
// (storage) descriptors share the texture table in the back end; the shader's slot type decides how it is bound.
NVN_API(nvnTexturePoolRegisterImage) { gfx_backend()->texture_register((int)arg_x(c, 1), arg_p(c, 2), arg_p(c, 3)); }
// void nvnSamplerPoolRegisterSamplerBuilder(const NVNsamplerPool *, int id, const NVNsamplerBuilder *)
NVN_API(nvnSamplerPoolRegisterSamplerBuilder) {
    gfx_backend()->sampler_register((int)arg_x(c, 1), &((const samp_builder_t *)arg_p(c, 2))->p);
}

// ---------- Memory pool builder (pool_builder_t)
#define PB(c) ((const pool_builder_t *)arg_p(c, 0))
NVN_API(nvnMemoryPoolBuilderGetDevice) { ret_x(c, (uint64_t)(uintptr_t)PB(c)->dev); }
NVN_API(nvnMemoryPoolBuilderGetFlags) { ret_x(c, PB(c)->flags); }
NVN_API(nvnMemoryPoolBuilderGetMemory) { ret_x(c, (uint64_t)(uintptr_t)PB(c)->mem); }
NVN_API(nvnMemoryPoolBuilderGetSize) { ret_x(c, PB(c)->size); }
#undef PB

// ---------- Queue builder (queue_builder_t) and queue
#define QB(c) ((const queue_builder_t *)arg_p(c, 0))
NVN_API(nvnQueueBuilderGetDevice) { ret_x(c, (uint64_t)(uintptr_t)QB(c)->dev); }
NVN_API(nvnQueueBuilderGetFlags) { ret_x(c, QB(c)->flags); }
// NVNboolean nvnQueueBuilderGetCommandMemorySize(const NVNqueueBuilder *, int *size): 1 when set
static void qb_size(Cpu *c, uint64_t v) {
    int32_t *out = arg_p(c, 1);
    if (out) *out = (int32_t)v;
    ret_x(c, 1);
}
NVN_API(nvnQueueBuilderGetCommandMemorySize) { qb_size(c, QB(c)->cmd_sz); }
NVN_API(nvnQueueBuilderGetComputeMemorySize) { qb_size(c, QB(c)->comp_sz); }
NVN_API(nvnQueueBuilderGetControlMemorySize) { qb_size(c, QB(c)->ctl_sz); }
NVN_API(nvnQueueBuilderGetCommandFlushThreshold) { qb_size(c, QB(c)->flush); }
NVN_API(nvnQueueBuilderGetMemory) { ret_x(c, (uint64_t)(uintptr_t)QB(c)->mem); }
// size_t nvnQueueBuilderGetMemorySize(const NVNqueueBuilder *)
NVN_API(nvnQueueBuilderGetMemorySize) { ret_x(c, QB(c)->mem_sz); }
#undef QB
// The queue priority orders queues inside the Switch GPU's scheduler; the host has one Vulkan queue.
static uint32_t queue_priority = 1;   // NVN_QUEUE_PRIORITY_NORMAL
NVN_API(nvnQueueBuilderSetQueuePriority) { queue_priority = (uint32_t)arg_x(c, 1); }
NVN_API(nvnQueueBuilderGetQueuePriority) { (void)c; ret_x(c, queue_priority); }
// Queue memory usage counters: the host keeps no queue command memory.
NVN_API(nvnQueueGetTotalCommandMemoryUsed) { ret_x(c, 0); }
NVN_API(nvnQueueGetTotalComputeMemoryUsed) { ret_x(c, 0); }
NVN_API(nvnQueueGetTotalControlMemoryUsed) { ret_x(c, 0); }
NVN_API(nvnQueueResetMemoryUsageCounts) { (void)c; }
// void nvnQueueWaitSync(NVNqueue *, const NVNsync *): later work on this queue waits for the sync's fence. The host
// replays every queue's submissions in order on one Vulkan queue, so the fence is already ahead of anything submitted
// after this call (the same reasoning as nvnCommandBufferWaitSync in nvn_api_cmd.c).
NVN_API(nvnQueueWaitSync) { (void)c; }

// ---------- Device and device builder (dev_t_, dev_builder_t)
NVN_API(nvnDeviceBuilderGetFlags) { ret_x(c, ((const dev_builder_t *)arg_p(c, 0))->flags); }
NVN_API(nvnDeviceGetDepthMode) { ret_x(c, ((const dev_t_ *)arg_p(c, 0))->depth_mode); }
NVN_API(nvnDeviceGetWindowOriginMode) { ret_x(c, ((const dev_t_ *)arg_p(c, 0))->origin); }
// GPU crash checkpoints, debug domains and the debug database are NVN debugging aids; the host has its own
// (TOTK_PASS_CRUMBS, the flight recorder).
NVN_API(nvnDeviceSetAutomaticCheckpointsByDebugGroup) { (void)c; }
NVN_API(nvnDeviceSetAutomaticCheckpointsEnable) { (void)c; }
NVN_API(nvnDeviceSetAutomaticCheckpointsSamplingInterval) { (void)c; }
NVN_API(nvnDeviceSetCheckpointCallStacksEnable) { (void)c; }
NVN_API(nvnDeviceSetCheckpointMaxCallStacksDepth) { (void)c; }
NVN_API(nvnDeviceSetIntermediateShaderCache) { (void)c; }
static int debug_domain;
NVN_API(nvnDeviceGenerateDebugDomainId) { (void)c; ret_x(c, (uint64_t)__atomic_add_fetch(&debug_domain, 1, __ATOMIC_RELAXED)); }
// size_t nvnDeviceWalkDebugDatabase(const NVNdevice *, NVNdebugObjectType, NVNwalkDebugDatabaseCallback, void *)
NVN_API(nvnDeviceWalkDebugDatabase) { ret_x(c, 0); }
NVN_API(nvnBufferGetDebugID) { ret_x(c, 0); }

// ---------- Event builder and event (ev_builder_t, ev_t)
NVN_API(nvnEventBuilderGetMemoryPool) { ret_x(c, (uint64_t)(uintptr_t)((const ev_builder_t *)arg_p(c, 0))->pool); }
NVN_API(nvnEventBuilderGetMemoryOffset) { ret_x(c, (uint64_t)((const ev_builder_t *)arg_p(c, 0))->off); }
// const NVNmemoryPool *nvnEventBuilderGetStorage(const NVNeventBuilder *, ptrdiff_t *offset)
NVN_API(nvnEventBuilderGetStorage) {
    const ev_builder_t *b = arg_p(c, 0);
    int64_t *off = arg_p(c, 1);
    if (off) *off = b->off;
    ret_x(c, (uint64_t)(uintptr_t)b->pool);
}

// ---------- Buffer builder (buf_builder_t, nvn_api.h)
#define BB(c) ((const buf_builder_t *)arg_p(c, 0))
NVN_API(nvnBufferBuilderGetDevice) { ret_x(c, (uint64_t)(uintptr_t)BB(c)->dev); }
NVN_API(nvnBufferBuilderGetMemoryPool) { ret_x(c, (uint64_t)(uintptr_t)BB(c)->pool); }
NVN_API(nvnBufferBuilderGetMemoryOffset) { ret_x(c, (uint64_t)BB(c)->off); }
NVN_API(nvnBufferBuilderGetSize) { ret_x(c, BB(c)->size); }
#undef BB

// ---------- Multisample state (ms_st): stored and read back. Coverage modulation, coverage-to-color and the sample
// locations grid only change output with mixed-sample targets (raster samples above the color samples), which the
// Vulkan back end does not create, so they have no draw effect there.
#define MS(c) ((ms_st *)arg_p(c, 0))
NVN_API(nvnMultisampleStateSetCoverageModulationMode) { MS(c)->cov_mod = (uint8_t)arg_x(c, 1); }
NVN_API(nvnMultisampleStateGetCoverageModulationMode) { ret_x(c, MS(c)->cov_mod); }
NVN_API(nvnMultisampleStateSetCoverageToColorEnable) { MS(c)->cov2color_en = nvn_arg_bool(c, 1) != 0; }
NVN_API(nvnMultisampleStateGetCoverageToColorEnable) { ret_x(c, MS(c)->cov2color_en); }
NVN_API(nvnMultisampleStateSetCoverageToColorOutput) { MS(c)->cov2color_out = (uint8_t)arg_x(c, 1); }
NVN_API(nvnMultisampleStateGetCoverageToColorOutput) { ret_x(c, MS(c)->cov2color_out); }
NVN_API(nvnMultisampleStateSetSampleLocationsGridEnable) { MS(c)->sl_grid = nvn_arg_bool(c, 1) != 0; }
NVN_API(nvnMultisampleStateGetSampleLocationsGridEnable) { ret_x(c, MS(c)->sl_grid); }
NVN_API(nvnMultisampleStateGetSampleLocationsEnable) { ret_x(c, MS(c)->sl_en); }
NVN_API(nvnMultisampleStateGetAlphaToCoverageDither) { ret_x(c, MS(c)->a2c_dither); }
#undef MS

// ---------- Blend state: only advanced mode "none" exists (nvn_ext.c stops on any other), so its options read 0
NVN_API(nvnBlendStateGetAdvancedMode) { ret_x(c, 0); }
NVN_API(nvnBlendStateGetAdvancedOverlap) { ret_x(c, 0); }
NVN_API(nvnBlendStateGetAdvancedPremultipliedSrc) { ret_x(c, 0); }
NVN_API(nvnBlendStateGetAdvancedNormalizedDst) { ret_x(c, 0); }

// ---------- OpenGL interop: there is no GL context; a texture builder never names a GL texture
NVN_API(nvnTextureBuilderSetGLTextureName) { (void)c; }
NVN_API(nvnTextureBuilderGetGLTextureName) { ret_x(c, 0); }

// ---------- Transform feedback. Not captured yet: NVN takes the varying layout from the compiled program's control
// section, which the front end does not parse, and the translator is not asked for XFB outputs. Scarlet records
// transform feedback passes at boot; until capture is implemented, binds and begin/pause/resume/end change nothing and
// DrawTransformFeedback draws nothing, logged once so the gap stays visible (docs/phase3-boot.md).
static void xfb_gap(const char *what) {
    static int logged;
    if (!__atomic_exchange_n(&logged, 1, __ATOMIC_RELAXED))
        rt_log("nvn: transform feedback is not captured yet (%s); its passes write nothing", what);
}
NVN_API(nvnCommandBufferBindTransformFeedbackBuffer) { (void)c; }
NVN_API(nvnCommandBufferBindTransformFeedbackBuffers) { (void)c; }
NVN_API(nvnCommandBufferBeginTransformFeedback) { (void)c; xfb_gap("BeginTransformFeedback"); }
NVN_API(nvnCommandBufferPauseTransformFeedback) { (void)c; }
NVN_API(nvnCommandBufferResumeTransformFeedback) { (void)c; }
NVN_API(nvnCommandBufferEndTransformFeedback) { (void)c; }
NVN_API(nvnCommandBufferDrawTransformFeedback) { (void)c; xfb_gap("DrawTransformFeedback"); }
