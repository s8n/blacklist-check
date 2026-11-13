#include "bcat.h"
#include <stdlib.h>
#include <string.h>

#define OUT_BUF (SfBufferAttr_HipcMapAlias | SfBufferAttr_Out)

Result bcatOpenStorage(BcatStorage *s, u64 id) {
    static const char *names[] = {"bcat:s", "bcat:a", "bcat:m", "bcat:u"};
    Result rc = 0;
    memset(s, 0, sizeof *s);
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        rc = smGetService(&s->creator, names[i]);
        if (R_FAILED(rc)) continue;
        rc = serviceDispatchIn(&s->creator, 2, id,
            .out_num_objects = 1,
            .out_objects = &s->storage,
        );
        if (R_SUCCEEDED(rc)) {
            strcpy(s->service_name, names[i]);
            return 0;
        }
        serviceClose(&s->creator);
    }
    return rc;
}

void bcatCloseStorage(BcatStorage *s) {
    serviceClose(&s->storage);
    serviceClose(&s->creator);
}

static void set_name(char out[0x20], const char *in) {
    memset(out, 0, 0x20);
    strncpy(out, in, 0x1f);
}

Result bcatListDirectories(BcatStorage *s, char (*names)[0x20], s32 max, s32 *count) {
    return serviceDispatchOut(&s->storage, 10, *count,
        .buffer_attrs = {OUT_BUF},
        .buffers = {{names, (size_t)max * 0x20}},
    );
}

Result bcatListFiles(BcatStorage *s, const char *dir, BcatDirEntry *entries, s32 max, s32 *count) {
    Service d;
    Result rc = serviceDispatch(&s->storage, 1, .out_num_objects = 1, .out_objects = &d);
    if (R_FAILED(rc)) return rc;
    char name[0x20];
    set_name(name, dir);
    rc = serviceDispatchIn(&d, 0, name);
    if (R_SUCCEEDED(rc))
        rc = serviceDispatchOut(&d, 1, *count,
            .buffer_attrs = {OUT_BUF},
            .buffers = {{entries, (size_t)max * sizeof *entries}},
        );
    serviceClose(&d);
    return rc;
}

Result bcatReadFile(BcatStorage *s, const char *dir, const char *file, u8 **out, u64 *size) {
    Service f;
    *out = NULL;
    *size = 0;
    Result rc = serviceDispatch(&s->storage, 0, .out_num_objects = 1, .out_objects = &f);
    if (R_FAILED(rc)) return rc;

    struct { char dir[0x20]; char file[0x20]; } in;
    set_name(in.dir, dir);
    set_name(in.file, file);
    rc = serviceDispatchIn(&f, 0, in);
    u64 total = 0;
    if (R_SUCCEEDED(rc)) rc = serviceDispatchOut(&f, 2, total);
    if (R_SUCCEEDED(rc) && total > 16 * 1024 * 1024) rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    u8 *buf = NULL;
    if (R_SUCCEEDED(rc) && !(buf = malloc(total + 1))) rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

    u64 done = 0;
    while (R_SUCCEEDED(rc) && done < total) {
        u64 got = 0;
        rc = serviceDispatchInOut(&f, 1, done, got,
            .buffer_attrs = {OUT_BUF},
            .buffers = {{buf + done, total - done}},
        );
        if (R_SUCCEEDED(rc) && got == 0) break;
        done += got;
    }
    serviceClose(&f);
    if (R_FAILED(rc)) { free(buf); return rc; }
    buf[done] = 0;
    *out = buf;
    *size = done;
    return 0;
}
