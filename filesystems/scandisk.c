/*
 * scandisk.c
 * Scan disks for BtrFS multi-devices
 * by Samuel Liao
 *
 * Copyright (c) 2013 Tencent, Inc.
 */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/**
** Modified for RefindPlus
** Copyright (c) 2020-2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the preceding terms.
**/

#include "fsw_efi.h"
#ifdef __MAKEWITH_GNUEFI
#include "edk2/DriverBinding.h"
#include "edk2/ComponentName.h"
extern EFI_GUID gMyEfiDiskIoProtocolGuid;
extern EFI_GUID gMyEfiBlockIoProtocolGuid;
#else
#define gMyEfiBlockIoProtocolGuid gEfiBlockIoProtocolGuid
#define gMyEfiDiskIoProtocolGuid gEfiDiskIoProtocolGuid
#endif

#include "../include/refit_call_wrapper.h"

extern struct fsw_host_table   fsw_efi_host_table;
static void dummy_volume_free (struct fsw_volume *vol) { }
static struct fsw_fstype_table   dummy_fstype = {
    { FSW_STRING_TYPE_UTF8, 4, 4, "dummy" },
    sizeof (struct fsw_volume),
    sizeof (struct fsw_dnode),

    NULL, //volume_mount,
    dummy_volume_free, //volume_free,
    NULL, //volume_stat,
    NULL, //dnode_fill,
    NULL, //dnode_free,
    NULL, //dnode_stat,
    NULL, //get_extent,
    NULL, //dir_lookup,
    NULL, //dir_read,
    NULL, //readlink,
};

static
struct fsw_volume * dsk_btrfs_create_dummy_volume (
    EFI_DISK_IO_PROTOCOL *diskio,
    UINT32 mediaid
) {
    fsw_status_t err;
    struct fsw_volume *vol;
    FSW_VOLUME_DATA *Volume;

    err = fsw_alloc_zero (sizeof (struct fsw_volume), (void **) &vol);
    if (err) return NULL;

    err = fsw_alloc_zero (sizeof (FSW_VOLUME_DATA), (void **) &Volume);
    if (err) {
        FSW_DO_FREE(vol);
        return NULL;
    }
    /* fstype_table->volume_free for fsw_unmount */
    vol->fstype_table = &dummy_fstype;
    /* host_data needded to fsw_block_get()/fsw_efi_read_block() */
    Volume->DiskIo = diskio;
    Volume->MediaId = mediaid;

    vol->host_data = Volume;
    vol->host_table = &fsw_efi_host_table;
    return vol;
}

static
struct fsw_volume * dsk_btrfs_clone_dummy_volume (
    struct fsw_volume *vol
) {
    FSW_VOLUME_DATA *Volume = (FSW_VOLUME_DATA *)vol->host_data;
    return dsk_btrfs_create_dummy_volume(Volume->DiskIo, Volume->MediaId);
}

static
void dsk_btrfs_free_dummy_volume (
    struct fsw_volume *vol
) {
    FSW_DO_FREE(vol->host_data);
    fsw_unmount (vol);
}

static
int dsk_btrfs_scan_disks (
    int (*hook)(
        struct fsw_volume *,
        struct fsw_volume *
    ), struct fsw_volume *master
) {
    EFI_STATUS  Status;
    EFI_HANDLE *Handles;
    UINTN       i;
    UINTN       HandleCount = 0;
    UINTN       scanned = 0;

    // DA-TAG: Investigate This (From Upstream - Likely a Memory Conflict)
    //
    // Driver hangs if compiled with GNU-EFI without 'Print()' statement.
#if defined(__MAKEWITH_GNUEFI)
    Print(L" ");
#endif

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "SCANDISK: dsk_btrfs_scan_disks ... Scanning Disks\n"
        )
    ));

    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gMyEfiDiskIoProtocolGuid, NULL,
        &HandleCount, &Handles
    );
    if (Status == EFI_NOT_FOUND) {
        return -1;  // No filesystems ... Strange, but true!
    }

    for (i = 0; i < HandleCount; i++) {
        EFI_DISK_IO_PROTOCOL *diskio;
        EFI_BLOCK_IO_PROTOCOL *blockio;

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Handles[i],
            &gMyEfiDiskIoProtocolGuid, (VOID **) &diskio
        );
        if (Status != 0) {
            continue;
        }

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Handles[i],
            &gMyEfiBlockIoProtocolGuid, (VOID **) &blockio
        );
        if (Status != 0) {
            continue;
        }

        struct fsw_volume *vol = dsk_btrfs_create_dummy_volume (
            diskio, blockio->Media->MediaId
        );

        if (vol) {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "SCANDISK: dsk_btrfs_scan_disks ... Checking Disk %d\n"
                ), i
            ));

            if (hook(master, vol) == FSW_SUCCESS) {
                scanned++;
            }
            dsk_btrfs_free_dummy_volume (vol);
        }
    }

    return scanned;
}
