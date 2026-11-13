// Minimal BCAT delivery cache client. Neither libnx nor libounce has one.
//
// Command IDs and layouts are from sdb's own bcat:s client proxies (23.0.1):
//   IServiceCreator               2  CreateDeliveryCacheStorageServiceWithApplicationId(u64) -> object
//   IDeliveryCacheStorageService  0  CreateFileService -> object
//                                 1  CreateDirectoryService -> object
//                                10  EnumerateDeliveryCacheDirectory(out buf of 0x20 names) -> s32
//   IDeliveryCacheFileService     0  Open(0x20 dir name, 0x20 file name)
//                                 1  Read(u64 offset, out buf) -> u64
//                                 2  GetSize -> u64
//                                 3  GetDigest -> 0x10
//   IDeliveryCacheDirectoryService 0 Open(0x20 dir name)
//                                 1  Read(out buf of 0x38 entries) -> s32
//                                 2  GetCount -> s32
#pragma once
#include <switch.h>

typedef struct {
    char name[0x20];
    u64 size;
    u8 digest[0x10];
} BcatDirEntry;

_Static_assert(sizeof(BcatDirEntry) == 0x38, "");

typedef struct {
    Service creator;
    Service storage;
    char service_name[8];
} BcatStorage;

// Tries bcat:s, bcat:a, bcat:m, bcat:u in that order and opens the delivery
// cache of id. Returns the last error if none works.
Result bcatOpenStorage(BcatStorage *s, u64 id);
void bcatCloseStorage(BcatStorage *s);

Result bcatListDirectories(BcatStorage *s, char (*names)[0x20], s32 max, s32 *count);
Result bcatListFiles(BcatStorage *s, const char *dir, BcatDirEntry *entries, s32 max, s32 *count);
// Reads a whole file. *out is malloc'd, caller frees.
Result bcatReadFile(BcatStorage *s, const char *dir, const char *file, u8 **out, u64 *size);
