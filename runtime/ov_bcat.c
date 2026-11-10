// nn::bcat (delivery cache: the data Nintendo pushes to a game, such as Scarlet/Violet's Tera Raid events and Mystery
// Gift distributions). The host answers as a console that synced and received nothing: Initialize and the sync
// succeed, the progress object reads Done at once, the cache storage mounts and holds no directory, and every
// directory or file open reports not found. In the lifted SDK each of these opens the bcat:u service the host does
// not have. A later server phase can serve real deliveries from here.
//
// The SDK classes (DeliveryCacheFile, DeliveryCacheDirectory, DeliveryCacheProgress) are game-owned memory; with every
// method overridden, the host keeps its own state in their first words: an open flag, or the progress status and result.
#include <stdint.h>
#include <string.h>

#include "host.h"

#define BCAT_R_NOT_FOUND ((2u << 9) | 122u)           // nn::bcat::ResultNotFound (module 122, description 2)
#define BCAT_R_NOT_OPEN ((6u << 9) | 122u)            // a read or close on an object that holds nothing open
#define PROGRESS_DONE 9u                               // DeliveryCacheProgressStatus_Done

static int bcat_mounted;

// Result Initialize()
static void ov_bcat_init(Cpu *c) {
    rt_log("bcat: no deliveries on this host (the delivery cache mounts empty)");
    ret_x(c, 0);
}
// Result MountDeliveryCacheStorage() / void UnmountDeliveryCacheStorage()
static void ov_bcat_mount(Cpu *c) { bcat_mounted = 1; ret_x(c, 0); }
static void ov_bcat_unmount(Cpu *c) { (void)c; bcat_mounted = 0; }
// Result EnumerateDeliveryCacheDirectory(int *count, DirectoryName *out, int max): no directories
static void ov_bcat_enum(Cpu *c) {
    int32_t *count = arg_p(c, 0);
    if (count) *count = 0;
    ret_x(c, 0);
}

// DeliveryCacheProgress: word 0 status, word 1 result
// DeliveryCacheProgress::DeliveryCacheProgress() / ~DeliveryCacheProgress()
static void ov_progress_ctor(Cpu *c) {
    uint32_t *p = arg_p(c, 0);
    if (p) p[0] = 0, p[1] = 0;
}
static void ov_progress_dtor(Cpu *c) { (void)c; }
// Result RequestSyncDeliveryCache(DeliveryCacheProgress *): finished at once, nothing new
static void ov_bcat_sync(Cpu *c) {
    uint32_t *p = arg_p(c, 0);
    if (p) p[0] = PROGRESS_DONE, p[1] = 0;
    ret_x(c, 0);
}
// void DeliveryCacheProgress::Update()
static void ov_progress_update(Cpu *c) { (void)c; }
// DeliveryCacheProgressStatus DeliveryCacheProgress::GetStatus() const / Result GetResult() const
static void ov_progress_status(Cpu *c) {
    const uint32_t *p = arg_p(c, 0);
    ret_x(c, p ? p[0] : PROGRESS_DONE);
}
static void ov_progress_result(Cpu *c) {
    const uint32_t *p = arg_p(c, 0);
    ret_x(c, p ? p[1] : 0);
}

// DeliveryCacheDirectory and DeliveryCacheFile: word 0 is 1 while open (never, here)
static void ov_obj_ctor(Cpu *c) {
    uint32_t *p = arg_p(c, 0);
    if (p) p[0] = 0;
}
static void ov_obj_dtor(Cpu *c) { (void)c; }
// Result Open(const char *) / Open(const DirectoryName &): nothing was delivered
static void ov_obj_open(Cpu *c) {
    if (rt_trace & RT_TRACE_FS) {
        const char *name = arg_p(c, 1);
        rt_log("bcat: open %.32s: not delivered", name ? name : "(null)");
    }
    ret_x(c, BCAT_R_NOT_FOUND);
}
// void Close()
static void ov_obj_close(Cpu *c) {
    uint32_t *p = arg_p(c, 0);
    if (p) p[0] = 0;
}
// Result DeliveryCacheDirectory::Read(int *count, DeliveryCacheDirectoryEntry *, int)
static void ov_dir_read(Cpu *c) {
    int32_t *count = arg_p(c, 1);
    if (count) *count = 0;
    ret_x(c, BCAT_R_NOT_OPEN);
}
// Result DeliveryCacheFile::Read(size_t *read, int64_t offset, void *, size_t)
static void ov_file_read(Cpu *c) {
    uint64_t *got = arg_p(c, 1);
    if (got) *got = 0;
    ret_x(c, BCAT_R_NOT_OPEN);
}
// int64_t DeliveryCacheFile::GetSize() const
static void ov_file_size(Cpu *c) { ret_x(c, 0); }

HOST_OVERRIDE("_ZN2nn4bcat10InitializeEv", ov_bcat_init);
HOST_OVERRIDE("_ZN2nn4bcat25MountDeliveryCacheStorageEv", ov_bcat_mount);
HOST_OVERRIDE("_ZN2nn4bcat27UnmountDeliveryCacheStorageEv", ov_bcat_unmount);
HOST_OVERRIDE("_ZN2nn4bcat31EnumerateDeliveryCacheDirectoryEPiPNS0_13DirectoryNameEi", ov_bcat_enum);
HOST_OVERRIDE("_ZN2nn4bcat24RequestSyncDeliveryCacheEPNS0_21DeliveryCacheProgressE", ov_bcat_sync);
HOST_OVERRIDE("_ZN2nn4bcat21DeliveryCacheProgressC1Ev", ov_progress_ctor);
HOST_OVERRIDE("_ZN2nn4bcat21DeliveryCacheProgressD1Ev", ov_progress_dtor);
HOST_OVERRIDE("_ZN2nn4bcat21DeliveryCacheProgress6UpdateEv", ov_progress_update);
HOST_OVERRIDE("_ZNK2nn4bcat21DeliveryCacheProgress9GetStatusEv", ov_progress_status);
HOST_OVERRIDE("_ZNK2nn4bcat21DeliveryCacheProgress9GetResultEv", ov_progress_result);
HOST_OVERRIDE("_ZN2nn4bcat22DeliveryCacheDirectoryC1Ev", ov_obj_ctor);
HOST_OVERRIDE("_ZN2nn4bcat22DeliveryCacheDirectoryD1Ev", ov_obj_dtor);
HOST_OVERRIDE("_ZN2nn4bcat22DeliveryCacheDirectory4OpenEPKc", ov_obj_open);
HOST_OVERRIDE("_ZN2nn4bcat22DeliveryCacheDirectory4OpenERKNS0_13DirectoryNameE", ov_obj_open);
HOST_OVERRIDE("_ZN2nn4bcat22DeliveryCacheDirectory4ReadEPiPNS0_27DeliveryCacheDirectoryEntryEi", ov_dir_read);
HOST_OVERRIDE("_ZN2nn4bcat22DeliveryCacheDirectory5CloseEv", ov_obj_close);
HOST_OVERRIDE("_ZN2nn4bcat17DeliveryCacheFileC1Ev", ov_obj_ctor);
HOST_OVERRIDE("_ZN2nn4bcat17DeliveryCacheFileD1Ev", ov_obj_dtor);
HOST_OVERRIDE("_ZN2nn4bcat17DeliveryCacheFile4OpenEPKc", ov_obj_open);
HOST_OVERRIDE("_ZN2nn4bcat17DeliveryCacheFile4ReadEPmlPvm", ov_file_read);
HOST_OVERRIDE("_ZN2nn4bcat17DeliveryCacheFile5CloseEv", ov_obj_close);
HOST_OVERRIDE("_ZN2nn4bcat17DeliveryCacheFile7GetSizeEv", ov_file_size);
