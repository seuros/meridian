/**
 * \file fsw_efi.c
 * EFI host environment code.
**/

/**
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/
/**
 * Changes by Roderick Smith are licensed under the preceding terms.
**/
/**
** Modified for RefindPlus
** Copyright (c) 2021-2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the MIT License.
**/


#include "fsw_efi.h"
#include "fsw_core.h"
#ifdef __MAKEWITH_GNUEFI
#include "edk2/DriverBinding.h"
#include "edk2/ComponentName.h"
#define gMyEfiSimpleFileSystemProtocolGuid FileSystemProtocol
#else
#define REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL EFI_DRIVER_BINDING_PROTOCOL
#define REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL EFI_COMPONENT_NAME_PROTOCOL
#define REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL_GUID EFI_COMPONENT_NAME_PROTOCOL_GUID
#define REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL_GUID EFI_DRIVER_BINDING_PROTOCOL_GUID
#define REFINDPLUS_EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL
#define EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID    \
    { 0xDB47D7D3,0xFE81, 0x11d3, {0x9A, 0x35, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D} }
#define gMyEfiSimpleFileSystemProtocolGuid gEfiSimpleFileSystemProtocolGuid
#endif

#include "../include/refit_call_wrapper.h"
#include "../include/version.h"

#define DEBUG_LEVEL 0

#ifndef FSTYPE
/** The file system type name to use. */
#define FSTYPE ext2
#endif

EFI_GUID gMyEfiDriverBindingProtocolGuid       = REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID gMyEfiComponentNameProtocolGuid       = REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID gMyEfiDiskIoProtocolGuid              = REFINDPLUS_EFI_DISK_IO_PROTOCOL_GUID;
EFI_GUID gMyEfiBlockIoProtocolGuid             = REFINDPLUS_EFI_BLOCK_IO_PROTOCOL_GUID;
EFI_GUID gMyEfiFileInfoGuid                    = EFI_FILE_INFO_ID;
EFI_GUID gMyEfiFileSystemInfoGuid              = EFI_FILE_SYSTEM_INFO_ID;
EFI_GUID gMyEfiFileSystemVolumeLabelInfoIdGuid = EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID;

/** Helper macro for stringification. */
#define FSW_EFI_STRINGIFY(x) #x
/** Expands to the UEFI driver name given the file system type name. */
#define FSW_EFI_DRIVER_NAME(t) L"RefindPlus v" REFINDPLUS_VERSION L" Filesystem Driver:- " FSW_EFI_STRINGIFY(t)

// function prototypes

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Supported (
    IN REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN REFINDPLUS_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
);
EFI_STATUS EFIAPI fsw_efi_DriverBinding_Start (
    IN REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN REFINDPLUS_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
);
EFI_STATUS EFIAPI fsw_efi_DriverBinding_Stop (
    IN REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN UINTN                                    NumberOfChildren,
    IN EFI_HANDLE                              *ChildHandleBuffer
);
EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName (
    IN  REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL   *This,
    IN  CHAR8                                    *Language,
    OUT CHAR16                                  **DriverName
);
EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName (
    IN  REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL  *This,
    IN  EFI_HANDLE                               ControllerHandle,
    IN  EFI_HANDLE                               ChildHandle  OPTIONAL,
    IN  CHAR8                                   *Language,
    OUT CHAR16                                 **ControllerName
);
void EFIAPI fsw_efi_change_blocksize (
    struct fsw_volume *vol,
    fsw_u32 old_phys_blocksize,
    fsw_u32 old_log_blocksize,
    fsw_u32 new_phys_blocksize,
    fsw_u32 new_log_blocksize
);
fsw_status_t EFIAPI fsw_efi_read_block (
    struct fsw_volume *vol,
    fsw_u64 phys_bno,
    void *buffer
);
EFI_STATUS fsw_efi_map_status (
    fsw_status_t     fsw_status,
    FSW_VOLUME_DATA *Volume
);
EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume (
    IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
    OUT EFI_FILE_PROTOCOL              **Root
);
EFI_STATUS fsw_efi_dnode_to_FileHandle (
    IN  struct fsw_dnode   *dno,
    OUT EFI_FILE_PROTOCOL **NewFileHandle
);
EFI_STATUS fsw_efi_file_read (
    IN FSW_FILE_DATA *File,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);
EFI_STATUS fsw_efi_file_getpos (
    IN FSW_FILE_DATA *File,
    OUT UINT64 *Position
);
EFI_STATUS fsw_efi_file_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64 Position
);
EFI_STATUS fsw_efi_dir_open (
    IN FSW_FILE_DATA        *File,
    OUT EFI_FILE_PROTOCOL  **NewHandle,
    IN CHAR16               *FileName,
    IN UINT64                OpenMode,
    IN UINT64                Attributes
);
EFI_STATUS fsw_efi_dir_read (
    IN FSW_FILE_DATA *File,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);
EFI_STATUS fsw_efi_dir_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64 Position
);
EFI_STATUS fsw_efi_dnode_getinfo (
    IN FSW_FILE_DATA *File,
    IN EFI_GUID *InformationType,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);
EFI_STATUS fsw_efi_dnode_fill_FileInfo (
    IN FSW_VOLUME_DATA *Volume,
    IN struct fsw_dnode *dno,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);

/**
 * Structure for holding disk cache data.
 */

#define CACHE_SIZE 131072 /* 128KiB */
struct cache_data {
   fsw_u8           *Cache;
   fsw_u64           CacheStart;
   BOOLEAN           CacheValid;
   FSW_VOLUME_DATA  *Volume;     // NOTE: Do not deallocate; copied here to ID volume
};

#define NUM_CACHES 2 /* Do not increase without modifying fsw_efi_read_block() */
static struct cache_data    Caches[NUM_CACHES];
static int LastRead = -1;

/**
 * Interface structure for the UEFI Driver Binding protocol.
 */

REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL fsw_efi_DriverBinding_table = {
    fsw_efi_DriverBinding_Supported,
    fsw_efi_DriverBinding_Start,
    fsw_efi_DriverBinding_Stop,
    0x10, NULL, NULL
};

/**
 * Interface structure for the EFI Component Name protocol.
 */

REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL fsw_efi_ComponentName_table = {
    fsw_efi_ComponentName_GetDriverName,
    fsw_efi_ComponentName_GetControllerName,
    (CHAR8*) "eng"
};

/**
 * Dispatch table for our FSW host driver.
 */

struct fsw_host_table   fsw_efi_host_table = {
    FSW_STRING_TYPE_UTF16,
    fsw_efi_change_blocksize,
    fsw_efi_read_block
};

extern struct fsw_fstype_table   FSW_FSTYPE_TABLE_NAME(FSTYPE);


VOID EFIAPI fsw_efi_clear_cache (VOID) {
   int i;

   // clear the cache
   for (i = 0; i < NUM_CACHES; i++) {
      if (Caches[i].Cache != NULL) {
         FreePool (Caches[i].Cache);
         Caches[i].Cache = NULL;
      } // if

      Caches[i].CacheStart = 0;
      Caches[i].CacheValid = FALSE;
      Caches[i].Volume     = NULL;
   }
   LastRead = -1;
} // VOID EFIAPI fsw_efi_clear_cache();

/**
 * Image entry point. Installs the Driver Binding and Component Name protocols
 * on the image's handle. Actually mounting a file system is initiated through
 * the Driver Binding protocol at the firmware's request.
 */
EFI_STATUS EFIAPI fsw_efi_main (
    IN EFI_HANDLE          ImageHandle,
    IN EFI_SYSTEM_TABLE   *SystemTable
) {
    EFI_STATUS  Status;

#ifndef __MAKEWITH_TIANO
    // Not available in EDK2 toolkit
    InitializeLib (ImageHandle, SystemTable);
#endif

    // complete Driver Binding protocol instance
    fsw_efi_DriverBinding_table.ImageHandle          = ImageHandle;
    fsw_efi_DriverBinding_table.DriverBindingHandle  = ImageHandle;
    // install Driver Binding protocol
    Status = REFIT_CALL_4_WRAPPER(
        gBS->InstallProtocolInterface, &fsw_efi_DriverBinding_table.DriverBindingHandle,
        &gMyEfiDriverBindingProtocolGuid, EFI_NATIVE_INTERFACE, &fsw_efi_DriverBinding_table
    );
    if (EFI_ERROR(Status)) return Status;

    // install Component Name protocol
    Status = REFIT_CALL_4_WRAPPER(
        gBS->InstallProtocolInterface, &fsw_efi_DriverBinding_table.DriverBindingHandle,
        &gMyEfiComponentNameProtocolGuid, EFI_NATIVE_INTERFACE, &fsw_efi_ComponentName_table
    );

    return Status;
}

#ifdef __MAKEWITH_GNUEFI
EFI_DRIVER_ENTRY_POINT(fsw_efi_main)
#endif

/**
 * Driver Binding EFI protocol, Supported function. This function is called by EFI
 * to test if this driver can handle a certain device. Our implementation only checks
 * if the device is a disk (i.e. that it supports the Block I/O and Disk I/O protocols)
 * and implicitly checks if the disk is already in use by another driver.
 */

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Supported (
    IN REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN REFINDPLUS_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
) {
    EFI_STATUS            Status;
    EFI_DISK_IO_PROTOCOL *DiskIo;

    // we check for both DiskIO and BlockIO protocols

    // first, open DiskIO
    Status = REFIT_CALL_6_WRAPPER(
        gBS->OpenProtocol, ControllerHandle,
        &gMyEfiDiskIoProtocolGuid, (VOID **) &DiskIo,
        This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_BY_DRIVER
    );
    if (EFI_ERROR(Status)) return Status;

    // we were just checking, close it again
    REFIT_CALL_4_WRAPPER(
        gBS->CloseProtocol, ControllerHandle,
        &gMyEfiDiskIoProtocolGuid, This->DriverBindingHandle, ControllerHandle
    );

    // next, check BlockIO without actually opening it
    Status = REFIT_CALL_6_WRAPPER(
        gBS->OpenProtocol, ControllerHandle,
        &gMyEfiBlockIoProtocolGuid, NULL,
        This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_TEST_PROTOCOL
    );

    return Status;
}

/**
 * Driver Binding EFI protocol, Start function. This function is called by EFI
 * to start driving the given device. It is still possible at this point to
 * return EFI_UNSUPPORTED, and in fact we will do so if the file system driver
 * cannot find the superblock signature (or equivalent) that it expects.
 *
 * This function allocates memory for a per-volume structure, opens the
 * required protocols (just Disk I/O in our case, Block I/O is only looked
 * at to get the MediaId field), and lets the FSW core mount the file system.
 * If successful, an EFI Simple File System protocol is exported on the
 * device handle.
 */

EFI_STATUS
EFIAPI
fsw_efi_DriverBinding_Start (
    IN REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN REFINDPLUS_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
) {
    EFI_STATUS             Status;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_DISK_IO_PROTOCOL  *DiskIo;
    FSW_VOLUME_DATA       *Volume;

    // open consumed protocols
    Status = REFIT_CALL_6_WRAPPER(
        gBS->OpenProtocol, ControllerHandle,
        &gMyEfiBlockIoProtocolGuid, (VOID **) &BlockIo,
        This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );   // NOTE: we only want to look at the MediaId
    if (EFI_ERROR(Status)) return Status;

    Status = REFIT_CALL_6_WRAPPER(
        gBS->OpenProtocol, ControllerHandle,
        &gMyEfiDiskIoProtocolGuid, (VOID **) &DiskIo,
        This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_BY_DRIVER
    );
    if (EFI_ERROR(Status)) return Status;

    // Allocate volume structure
    Volume = AllocateZeroPool (sizeof (FSW_VOLUME_DATA));
    if (Volume == NULL) return EFI_BUFFER_TOO_SMALL;

    Volume->Signature       = FSW_VOLUME_DATA_SIGNATURE;
    Volume->Handle          = ControllerHandle;
    Volume->DiskIo          = DiskIo;
    Volume->MediaId         = BlockIo->Media->MediaId;
    Volume->LastIOStatus    = EFI_SUCCESS;

    // Mount filesystem
    Status = fsw_efi_map_status (
        fsw_mount (
            Volume,
            &fsw_efi_host_table,
            &FSW_FSTYPE_TABLE_NAME(FSTYPE),
            &Volume->vol
        ),
        Volume
    );
    if (!EFI_ERROR(Status)) {
        // Register SimpleFileSystem protocol
        Volume->FileSystem.Revision     = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION;
        Volume->FileSystem.OpenVolume   = fsw_efi_FileSystem_OpenVolume;

        Status = REFIT_CALL_4_WRAPPER(
            gBS->InstallMultipleProtocolInterfaces, &ControllerHandle,
            &gMyEfiSimpleFileSystemProtocolGuid, &Volume->FileSystem, NULL
        );
    }

    // Close opened protocols on error
    if (EFI_ERROR(Status)) {
        if (Volume->vol != NULL) {
            fsw_unmount (Volume->vol);
        }
        FreePool (Volume);

        REFIT_CALL_4_WRAPPER(
            gBS->CloseProtocol, ControllerHandle,
            &gMyEfiDiskIoProtocolGuid, This->DriverBindingHandle, ControllerHandle
        );
    }

    return Status;
}

/**
 * Driver Binding EFI protocol, Stop function. This function is called by EFI
 * to stop the driver on the given device. This translates to an unmount
 * call for the FSW core.
 *
 * We assume that all file handles on the volume have been closed before
 * the driver is stopped. At least with the uEFI shell, that is actually the
 * case; it closes all file handles between commands.
 */

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Stop (
    IN  REFINDPLUS_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN  EFI_HANDLE                   ControllerHandle,
    IN  UINTN                        NumberOfChildren,
    IN  EFI_HANDLE                  *ChildHandleBuffer
) {
    EFI_STATUS                       Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    FSW_VOLUME_DATA                 *Volume;

    // Get installed SimpleFileSystem interface
    Status = REFIT_CALL_6_WRAPPER(
        gBS->OpenProtocol, ControllerHandle,
        &gMyEfiSimpleFileSystemProtocolGuid, (VOID **) &FileSystem,
        This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if (EFI_ERROR(Status)) return EFI_UNSUPPORTED;

    // Get private data structure
    Volume = FSW_VOLUME_FROM_FILE_SYSTEM(FileSystem);

    // Uninstall SimpleFileSystem protocol
    Status = REFIT_CALL_4_WRAPPER(
        gBS->UninstallMultipleProtocolInterfaces, ControllerHandle,
        &gMyEfiSimpleFileSystemProtocolGuid, &Volume->FileSystem, NULL
    );
    if (EFI_ERROR(Status)) return Status;

    // Release private data structure
    if (Volume->vol != NULL) {
        fsw_unmount (Volume->vol);
    }
    FreePool (Volume);

    // Close consumed protocols
    Status = REFIT_CALL_4_WRAPPER(
        gBS->CloseProtocol, ControllerHandle,
        &gMyEfiDiskIoProtocolGuid, This->DriverBindingHandle, ControllerHandle
    );

    // Clear cache
    fsw_efi_clear_cache();

    return Status;
}

/**
 * Component Name EFI protocol, GetDriverName function. Used by the EFI
 * environment to inquire the name of this driver. The name returned is
 * based on the file system type actually used in compilation.
 */

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName (
    IN  REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL  *This,
    IN  CHAR8                                   *Language,
    OUT CHAR16                                 **DriverName
) {
    if (Language == NULL || DriverName == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (Language[0] == 'e' && Language[1] == 'n' && Language[2] == 'g' && Language[3] == 0) {
        *DriverName = FSW_EFI_DRIVER_NAME (FSTYPE);
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}

/**
 * Component Name EFI protocol, GetControllerName function. Not implemented
 * because this is not a "bus" driver in the sense of the UEFI Driver Model.
 */

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName (
    IN  REFINDPLUS_EFI_COMPONENT_NAME_PROTOCOL    *This,
    IN  EFI_HANDLE                                 ControllerHandle,
    IN  EFI_HANDLE                                 ChildHandle  OPTIONAL,
    IN  CHAR8                                     *Language,
    OUT CHAR16                                   **ControllerName
) {
    return EFI_UNSUPPORTED;
}

/**
 * FSW interface function for block size changes. This function is called by the FSW core
 * when the file system driver changes the block sizes for the volume.
 */

void EFIAPI fsw_efi_change_blocksize (
    struct fsw_volume *vol,
    fsw_u32 old_phys_blocksize,
    fsw_u32 old_log_blocksize,
    fsw_u32 new_phys_blocksize,
    fsw_u32 new_log_blocksize)
{
    // Nothing to do
    return;
}

/**
 * FSW interface function to read data blocks. This function is called by the FSW core
 * to read a block of data from the device. The buffer is allocated by the core code.
 * Two caches are maintained, so as to improve performance on some systems. (VirtualBox
 * is particularly susceptible to performance problems with an uncached driver -- the
 * ext2 driver can take 200 seconds to load a Linux kernel under VirtualBox, whereas
 * the time is more like 3 seconds with a cache!) Two independent caches are maintained
 * because the ext2fs driver tends to alternate between accessing two parts of the
 * disk.
 */

fsw_status_t EFIAPI fsw_efi_read_block (
    struct fsw_volume *vol,
    fsw_u64            phys_bno,
    void              *buffer
) {
   int              i, ReadCache = -1;
   FSW_VOLUME_DATA  *Volume = (FSW_VOLUME_DATA *)vol->host_data;
   EFI_STATUS       Status = EFI_SUCCESS;
   BOOLEAN          ReadOneBlock = FALSE;
   UINT64           StartRead = (UINT64) phys_bno * (UINT64) vol->phys_blocksize;

   if (buffer == NULL) {
       FSW_MSG_LEVEL_3((
           FSW_MSG_STR(
               "FSW_EFI: fsw_efi_read_block ... Leaving with Status: 'EFI_BAD_BUFFER_SIZE'\n"
           )
       ));

       return (fsw_status_t) EFI_BAD_BUFFER_SIZE;
   }


   // Initialize static data structures, if necessary.
   if (LastRead < 0) fsw_efi_clear_cache();

   // Look for cache hit on current query.
   i = 0;
   do {
      if ((Caches[i].Volume == Volume) &&
          Caches[i].CacheValid &&
          (StartRead >= Caches[i].CacheStart) &&
          ((StartRead + vol->phys_blocksize) <= (Caches[i].CacheStart + CACHE_SIZE))) {
         ReadCache = i;
      }
      i++;
   } while ((i < NUM_CACHES) && (ReadCache < 0));

   // No cache hit ... Load new cache and pass on.
   if (ReadCache < 0) {
      if (LastRead == -1) LastRead = 1;
      ReadCache = 1 - LastRead; // NOTE: If NUM_CACHES > 2, this must become more complex

      Caches[ReadCache].CacheValid = FALSE;
      if (Caches[ReadCache].Cache == NULL) {
          Caches[ReadCache].Cache = AllocatePool (CACHE_SIZE);
      }

      if (Caches[ReadCache].Cache == NULL) {
         ReadOneBlock = TRUE;
      }
      else {
         // DA-TAG: Investigate This
         //
         // Call below hangs on a 32-bit Mac Mini when compiled with GNU-EFI.
         // The same binary is fine under VirtualBox, and the call is fine when
         // compiled with Tianocore. Further clue: Omitting "Status =" avoids the
         // hang but produces a failure to mount the filesystem, even when the same
         // change is made to later similar call. Calling Volume->DiskIo->ReadDisk()
         // directly (without REFIT_CALL_5_WRAPPER()) changes nothing. Placing Print()
         // statements at the start and end of the function, and before and after the
         // ReadDisk() call, suggests that when it fails, the program is executing
         // code starting mid-function. There moght be an issue in how the
         // function is being called.
         Status = REFIT_CALL_5_WRAPPER(
             Volume->DiskIo->ReadDisk, Volume->DiskIo,
             Volume->MediaId, StartRead,
             (UINTN) CACHE_SIZE, (VOID*) Caches[ReadCache].Cache
         );

         if (EFI_ERROR(Status)) {
            ReadOneBlock = TRUE;
         }
         else {
            Caches[ReadCache].CacheStart = StartRead;
            Caches[ReadCache].CacheValid = TRUE;
            Caches[ReadCache].Volume     = Volume;
            LastRead                     = ReadCache;
         }
      } // if cache memory allocated
   } // if (ReadCache < 0)

   if (vol->phys_blocksize > 0      &&
       Caches[ReadCache].CacheValid &&
       Caches[ReadCache].Cache != NULL
   ) {
      CopyMem (
          buffer,
          &Caches[ReadCache].Cache[StartRead - Caches[ReadCache].CacheStart],
          vol->phys_blocksize
      );
   }
   else {
      ReadOneBlock = TRUE;
   }

   if (ReadOneBlock) {
       // Something failed ... Try a simple disk read of one block.
      Status = REFIT_CALL_5_WRAPPER(
          Volume->DiskIo->ReadDisk, Volume->DiskIo,
          Volume->MediaId, phys_bno * vol->phys_blocksize,
          (UINTN) vol->phys_blocksize, (VOID*) buffer
      );
   }

   Volume->LastIOStatus = Status;

   FSW_MSG_LEVEL_3((
       FSW_MSG_STR(
           "FSW_EFI: fsw_efi_read_block ... Leaving with Status: '%r'\n"
       ), Status
   ));

   return Status;
} // fsw_status_t *fsw_efi_read_block()

/**
 * Map FSW status codes to EFI status codes. The FSW_IO_ERROR code is only
 * produced by fsw_efi_read_block, so we map it back to the EFI status code
 * remembered from the last I/O operation.
 */

EFI_STATUS fsw_efi_map_status (
    fsw_status_t fsw_status,
    FSW_VOLUME_DATA *Volume
) {
    switch (fsw_status) {
        case FSW_SUCCESS:          return EFI_SUCCESS;
        case FSW_NOT_FOUND:        return EFI_NOT_FOUND;
        case FSW_UNSUPPORTED:      return EFI_UNSUPPORTED;
        case FSW_IO_ERROR:         return Volume->LastIOStatus;
        case FSW_OUT_OF_MEMORY:    return EFI_VOLUME_CORRUPTED;
        case FSW_VOLUME_CORRUPTED: return EFI_VOLUME_CORRUPTED;
        default:                   return EFI_DEVICE_ERROR;
    }
}

/**
 * File System EFI protocol, OpenVolume function. Creates a file handle for
 * the root directory and returns it. Note that this function may be called
 * multiple times and returns a new file handle each time. Each returned
 * handle is closed by the client using it.
 */

EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume (
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
    OUT EFI_FILE_PROTOCOL               **Root
) {
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = FSW_VOLUME_FROM_FILE_SYSTEM(This);

    fsw_efi_clear_cache();
    Status = fsw_efi_dnode_to_FileHandle (
        Volume->vol->root, Root
    );

    return Status;
}

/**
 * File Handle EFI protocol, Open function.
 * Dispatches call based on file handle type.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Open (
    IN  EFI_FILE_PROTOCOL  *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN  CHAR16             *FileName,
    IN  UINT64              OpenMode,
    IN  UINT64              Attributes
) {
    EFI_STATUS          Status;
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_FileHandle_Open ... Open File Handle: '%s'\n"
        ), FileName
    ));

    if (File->Type != FSW_EFI_FILE_TYPE_DIR) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_FileHandle_Open ... Error: 'File->Type != FSW_EFI_FILE_TYPE_DIR'\n"
            )
        ));

        // Not supported for regular files
        Status = EFI_UNSUPPORTED;
    }
    else {
        Status = fsw_efi_dir_open (
            File, NewHandle, FileName,
            OpenMode, Attributes
        );
    }

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_FileHandle_Open ... Leaving with Status: '%r'\n"
        ), Status
    ));

    return Status;
}

/**
 * File Handle EFI protocol, Close function. Closes the FSW shandle
 * and frees the memory used for the structure.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Close (
    IN EFI_FILE_PROTOCOL *This
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    fsw_shandle_close (&File->shand);
    FreePool (File);

    return EFI_SUCCESS;
}

/**
 * File Handle EFI protocol, Delete function. Calls through to Close
 * and returns a warning because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Delete (
    IN EFI_FILE_PROTOCOL *This
) {
    EFI_STATUS          Status;

    Status = REFIT_CALL_1_WRAPPER(This->Close, This);
    if (!EFI_ERROR(Status)) {
        // This driver is read-only
        Status = EFI_WARN_DELETE_FAILURE;
    }

    return Status;
}

/**
 * File Handle EFI protocol, Read function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Read (
    IN     EFI_FILE_PROTOCOL *This,
    IN OUT UINTN             *BufferSize,
       OUT VOID              *Buffer
) {
    EFI_STATUS          Status = EFI_UNSUPPORTED;
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);


    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_FileHandle_Read ... Read File Handle: '%s'\n"
        ), File->shand.dnode->name.data
    ));


    if (File->Type == FSW_EFI_FILE_TYPE_FILE) {
        Status = fsw_efi_file_read (File, BufferSize, Buffer);
    }
    else {
        if (File->Type == FSW_EFI_FILE_TYPE_DIR) {
            Status = fsw_efi_dir_read (File, BufferSize, Buffer);
        }
    }

    #if FSW_DEBUG_LEVEL >= 2
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_2((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_FileHandle_Read ... Leaving with Status: '%r'\n"
            ), Status
        ));
    }
    else {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_FileHandle_Read ... Leaving with Status: 'SUCCESS'\n"
            )
        ));
    }
    #endif

    return Status;
}

/**
 * File Handle EFI protocol, Write function. Returns unsupported status
 * because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Write (
    IN     EFI_FILE_PROTOCOL *This,
    IN OUT UINTN             *BufferSize,
    IN     VOID              *Buffer
) {
    // This driver is read-only
    return EFI_WRITE_PROTECTED;
}

/**
 * File Handle EFI protocol, GetPosition function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_GetPosition (
    IN  EFI_FILE_PROTOCOL *This,
    OUT UINT64            *Position
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE) {
        return fsw_efi_file_getpos (File, Position);
    }

    // Not defined for directories
    return EFI_UNSUPPORTED;
}

/**
 * File Handle EFI protocol, SetPosition function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_SetPosition (
    IN EFI_FILE_PROTOCOL *This,
    IN UINT64             Position
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE) {
        return fsw_efi_file_setpos (File, Position);
    }
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR) {
        return fsw_efi_dir_setpos (File, Position);
    }

    return EFI_UNSUPPORTED;
}

/**
 * File Handle EFI protocol, GetInfo function. Dispatches to the common
 * function implementing this.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_GetInfo (
    IN     EFI_FILE_PROTOCOL *This,
    IN     EFI_GUID          *InformationType,
    IN OUT UINTN             *BufferSize,
    OUT    VOID              *Buffer
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    return fsw_efi_dnode_getinfo (
        File, InformationType,
        BufferSize, Buffer
    );
}

/**
 * File Handle EFI protocol, SetInfo function. Returns unsupported status
 * because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_SetInfo (
    IN EFI_FILE_PROTOCOL *This,
    IN EFI_GUID          *InformationType,
    IN UINTN              BufferSize,
    IN VOID              *Buffer
) {
    // This driver is read-only
    return EFI_WRITE_PROTECTED;
}

/**
 * File Handle EFI protocol, Flush function. Returns unsupported status
 * because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Flush (
    IN EFI_FILE_PROTOCOL *This
) {
    // This driver is read-only
    return EFI_WRITE_PROTECTED;
}

/**
 * Set up a file handle for a dnode. This function allocates a data structure
 * for a file handle, opens a FSW shandle and populates the EFI_FILE_PROTOCOL
 * structure with the interface functions.
 */

EFI_STATUS fsw_efi_dnode_to_FileHandle (
    IN  struct fsw_dnode   *dno,
    OUT EFI_FILE_PROTOCOL **NewFileHandle
) {
    EFI_STATUS          Status;
    FSW_FILE_DATA       *File;

    // Ensure dnode has complete info
    Status = fsw_efi_map_status (
        fsw_dnode_fill (dno),
        (FSW_VOLUME_DATA *) dno->vol->host_data
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_FileHandle ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    // Check type
    if (dno->type != FSW_DNODE_TYPE_DIR &&
        dno->type != FSW_DNODE_TYPE_FILE

    ) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_FileHandle ... Leaving with Status: 'EFI_UNSUPPORTED'\n"
            )
        ));

        return EFI_UNSUPPORTED;
    }

    // Allocate file structure
    File = AllocateZeroPool (sizeof (FSW_FILE_DATA));
    if (File == NULL) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_FileHandle ... Leaving with Status: 'EFI_BUFFER_TOO_SMALL'\n"
            )
        ));

        return EFI_BUFFER_TOO_SMALL;
    }

    File->Signature = FSW_FILE_DATA_SIGNATURE;
    if (dno->type == FSW_DNODE_TYPE_FILE) {
        File->Type = FSW_EFI_FILE_TYPE_FILE;
    }
    else {
        if (dno->type == FSW_DNODE_TYPE_DIR) {
            File->Type = FSW_EFI_FILE_TYPE_DIR;
        }
    }

    // Open shandle
    Status = fsw_efi_map_status (
        fsw_shandle_open (
            dno, &File->shand
        ),
        (FSW_VOLUME_DATA *) dno->vol->host_data
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_FileHandle ... Leaving with Status: '%r'\n"
            ), Status
        ));

        FreePool (File);
        return Status;
    }

    // Populate file handle
    File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
    File->FileHandle.Open        = fsw_efi_FileHandle_Open;
    File->FileHandle.Close       = fsw_efi_FileHandle_Close;
    File->FileHandle.Delete      = fsw_efi_FileHandle_Delete;
    File->FileHandle.Read        = fsw_efi_FileHandle_Read;
    File->FileHandle.Write       = fsw_efi_FileHandle_Write;
    File->FileHandle.GetPosition = fsw_efi_FileHandle_GetPosition;
    File->FileHandle.SetPosition = fsw_efi_FileHandle_SetPosition;
    File->FileHandle.GetInfo     = fsw_efi_FileHandle_GetInfo;
    File->FileHandle.SetInfo     = fsw_efi_FileHandle_SetInfo;
    File->FileHandle.Flush       = fsw_efi_FileHandle_Flush;

    *NewFileHandle = &File->FileHandle;

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_dnode_to_FileHandle ... Leaving with Status: 'EFI_SUCCESS'\n"
        )
    ));

    return EFI_SUCCESS;
}

/**
 * Data read function for regular files.
 */

EFI_STATUS fsw_efi_file_read (
    IN     FSW_FILE_DATA *File,
    IN OUT UINTN         *BufferSize,
    OUT    VOID          *Buffer
) {
    EFI_STATUS          Status;
    fsw_u32             buffer_size;
    fsw_status_t        fsw_status;


    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_file_read ... Read File: '%s'\n"
        ), File->shand.dnode->name.data
    ));

    buffer_size = (fsw_u32)*BufferSize;
    fsw_status = fsw_shandle_read (
        &File->shand, &buffer_size, Buffer
    );
    Status = fsw_efi_map_status (
        fsw_status,
        (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data
    );
    *BufferSize = buffer_size;


    #if FSW_DEBUG_LEVEL >= 1
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_1((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_file_read ... Leaving with Status: '%r'\n"
            ), Status
        ));
    }
    else {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_file_read ... Leaving with Status: 'SUCCESS'\n"
            )
        ));
    }
    #endif


    return Status;
}

/**
 * Get file position for regular files.
 */

EFI_STATUS fsw_efi_file_getpos (
    IN  FSW_FILE_DATA *File,
    OUT UINT64        *Position
) {
    *Position = File->shand.pos;
    return EFI_SUCCESS;
}

/**
 * Set file position for regular files. EFI specifies the 'all-ones'
 * value to be a special value for the end of the file.
 */

EFI_STATUS fsw_efi_file_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64         Position
) {
    File->shand.pos = (
        Position != 0xFFFFFFFFFFFFFFFFULL
    ) ? Position : File->shand.dnode->size;

    return EFI_SUCCESS;
}

/**
 * Open function used to open new file handles relative to a directory.
 * In EFI, the "open file" function is implemented by directory file handles
 * and is passed a relative or volume-absolute path to the file or directory
 * to open. We use fsw_dnode_lookup_path to find the node plus an additional
 * call to fsw_dnode_resolve because EFI has no concept of symbolic links.
 */

EFI_STATUS fsw_efi_dir_open (
    IN  FSW_FILE_DATA          *File,
    OUT EFI_FILE_PROTOCOL     **NewHandle,
    IN  CHAR16                 *FileName,
    IN  UINT64                  OpenMode,
    IN  UINT64                  Attributes
) {
    EFI_STATUS          Status;
    FSW_VOLUME_DATA    *Volume;
    struct fsw_dnode   *dno;
    struct fsw_dnode   *target_dno;
    struct fsw_string   lookup_path;


    if (OpenMode != EFI_FILE_MODE_READ) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_open ... Leaving with Status: '%r'\n"
            ), EFI_WRITE_PROTECTED
        ));

        return EFI_WRITE_PROTECTED;
    }


    lookup_path.type = FSW_STRING_TYPE_UTF16;
    lookup_path.len  = (int)StrLen (FileName);
    lookup_path.size = lookup_path.len * sizeof (fsw_u16);
    lookup_path.data = FileName;

    Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;

    // Resolve the path (symlinks along the way are automatically resolved)
    Status = fsw_efi_map_status (
        fsw_dnode_lookup_path (
            File->shand.dnode,
            &lookup_path,
            '\\', &dno
        ),
        Volume
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_open ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    // if the final node is a symlink, also resolve it
    Status = fsw_efi_map_status (
        fsw_dnode_resolve (
            dno, &target_dno
        ),
        Volume
    );
    fsw_dnode_release (dno);
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_open ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }
    dno = target_dno;

    // Make a new EFI handle for the target dnode
    Status = fsw_efi_dnode_to_FileHandle (dno, NewHandle);
    fsw_dnode_release (dno);
    return Status;
}

/**
 * Read function for directories. A file handle read on a directory
 * retrieves the next directory entry.
 */

EFI_STATUS fsw_efi_dir_read (
    IN     FSW_FILE_DATA *File,
    IN OUT UINTN         *BufferSize,
    OUT    VOID          *Buffer
) {
    EFI_STATUS           Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;


    // Read the next entry
    Status = fsw_efi_map_status (
        fsw_dnode_dir_read (
            &File->shand, &dno
        ), Volume
    );
    if (Status == EFI_NOT_FOUND) {
        // End of directory
        *BufferSize = 0;

        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... no more entries\n"
            )
        ));

        return EFI_SUCCESS;
    }
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    // Get info into buffer
    Status = fsw_efi_dnode_fill_FileInfo (
        Volume, dno,
        BufferSize, Buffer
    );

    fsw_dnode_release (dno);

    #if FSW_DEBUG_LEVEL >= 1
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_1((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... Leaving with Status: '%r'\n"
            ), Status
        ));
    }
    else {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... Leaving with Status: 'SUCCESS'\n"
            )
        ));
    }
    #endif

    return Status;
}

/**
 * Set file position for directories. The only allowed set position operation
 * for directories is to rewind the directory completely by setting the
 * position to zero.
 */

EFI_STATUS fsw_efi_dir_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64         Position
) {
    if (Position == 0) {
        File->shand.pos = 0;
        return EFI_SUCCESS;
    }
    else {
        // Directories can only rewind to the start
        return EFI_UNSUPPORTED;
    }
}

/**
 * Get file or volume information. This function implements the GetInfo call
 * for all file handles. Control is dispatched according to the type of
 * information requested by the caller.
 */

EFI_STATUS fsw_efi_dnode_getinfo (
    IN     FSW_FILE_DATA *File,
    IN     EFI_GUID      *InformationType,
    IN OUT UINTN         *BufferSize,
    OUT    VOID          *Buffer
) {
    EFI_STATUS             Status;
    FSW_VOLUME_DATA       *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    EFI_FILE_SYSTEM_INFO  *FSInfo;
    UINTN                  RequiredSize;
    struct fsw_volume_stat vsb;


    if (CompareGuid (InformationType, &gMyEfiFileInfoGuid)) {
        Status = fsw_efi_dnode_fill_FileInfo (
            Volume, File->shand.dnode,
            BufferSize, Buffer
        );
    }
    else if (
        CompareGuid (
            InformationType,
            &gMyEfiFileSystemInfoGuid
        )
    ) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_getinfo ... FILE_SYSTEM_INFO\n"
            )
        ));

        // Check buffer size
        RequiredSize = fsw_efi_strsize (
            &Volume->vol->label
        ) + SIZE_OF_EFI_FILE_SYSTEM_INFO;
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }

        // Fill structure
        FSInfo = (EFI_FILE_SYSTEM_INFO *)Buffer;
        FSInfo->Size        = RequiredSize;
        FSInfo->ReadOnly    = TRUE;
        FSInfo->BlockSize   = Volume->vol->log_blocksize;
        fsw_efi_strcpy (FSInfo->VolumeLabel, &Volume->vol->label);

        // Get missing info from the fs driver
        ZeroMem (&vsb, sizeof (struct fsw_volume_stat));
        Status = fsw_efi_map_status (
            fsw_volume_stat (
                Volume->vol, &vsb
            ),
            Volume
        );
        if (EFI_ERROR(Status)) {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_EFI: fsw_efi_dnode_getinfo ... Leaving with Status: '%r'\n"
                ), Status
            ));

            return Status;
        }

        FSInfo->VolumeSize  = vsb.total_bytes;
        FSInfo->FreeSpace   = vsb.free_bytes;

        // Prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;

    }
    else if (
        CompareGuid (
            InformationType,
            &gMyEfiFileSystemVolumeLabelInfoIdGuid
        )
    ) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_getinfo ... FILE_SYSTEM_VOLUME_LABEL\n"
            )
        ));

        // Check buffer size
        RequiredSize = fsw_efi_strsize (
            &Volume->vol->label
        ) + SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO;
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }

        // Copy volume label
        fsw_efi_strcpy (
            ((EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *) Buffer)->VolumeLabel,
            &Volume->vol->label
        );

        // Prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
    }
    else {
        Status = EFI_UNSUPPORTED;
    }

    return Status;
}

/**
 * Time mapping callback for the fsw_dnode_stat call. This function converts
 * a Posix style timestamp into an EFI_TIME structure and writes it to the
 * appropriate member of the EFI_FILE_INFO structure that we are filling.
 */

void fsw_store_time_posix (
    struct fsw_dnode_stat *sb,
    int                    which,
    fsw_u32                posix_time
) {
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    if (0);
    else if (which == FSW_DNODE_STAT_CTIME) fsw_efi_decode_time (&FileInfo->CreateTime,       posix_time);
    else if (which == FSW_DNODE_STAT_MTIME) fsw_efi_decode_time (&FileInfo->ModificationTime, posix_time);
    else if (which == FSW_DNODE_STAT_ATIME) fsw_efi_decode_time (&FileInfo->LastAccessTime,   posix_time);
}

/**
 * Mode mapping callback for the fsw_dnode_stat call. This function looks at
 * the Posix mode passed by the file system driver and makes appropriate
 * adjustments to the EFI_FILE_INFO structure that we are filling.
 */

void fsw_store_attr_posix (
    struct fsw_dnode_stat *sb,
    fsw_u16                posix_mode
) {
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    if ((posix_mode & S_IWUSR) == 0) {
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
    }
}

void fsw_store_attr_efi (
    struct fsw_dnode_stat *sb,
    fsw_u16                attr
) {
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    FileInfo->Attribute |= attr;
}

/**
 * Common function to fill an EFI_FILE_INFO with information about a dnode.
 */

EFI_STATUS fsw_efi_dnode_fill_FileInfo (
    IN FSW_VOLUME_DATA  *Volume,
    IN struct fsw_dnode *dno,
    IN OUT UINTN        *BufferSize,
    OUT VOID            *Buffer
) {
    EFI_STATUS            Status;
    EFI_FILE_INFO        *FileInfo;
    UINTN                 RequiredSize;
    struct fsw_dnode_stat sb;

    // Ensure dnode has complete info
    Status = fsw_efi_map_status (
        fsw_dnode_fill (dno), Volume
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    // TODO: Check/Assert dno's name is UTF16

    // Check buffer size
    RequiredSize = SIZE_OF_EFI_FILE_INFO + fsw_efi_strsize (
        &dno->name
    );
    if (*BufferSize < RequiredSize) {
        Status = EFI_BUFFER_TOO_SMALL;

        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Leaving with Status: '%r'\n"
            ), Status
        ));

        // TODO: Wind back the directory in this case
        *BufferSize = RequiredSize;

        return Status;
    }

    // Fill structure
    ZeroMem (Buffer, RequiredSize);
    FileInfo = (EFI_FILE_INFO *)Buffer;
    FileInfo->Size              = RequiredSize;
    FileInfo->FileSize          = dno->size;
    FileInfo->Attribute         = 0;

    if (dno->type == FSW_DNODE_TYPE_DIR) {
        FileInfo->Attribute |= EFI_FILE_DIRECTORY;
    }

    fsw_efi_strcpy (FileInfo->FileName, &dno->name);

    // Get the missing info from the fs driver
    ZeroMem (&sb, sizeof (struct fsw_dnode_stat));
    sb.host_data = FileInfo;

    Status = fsw_efi_map_status (
        fsw_dnode_stat (dno, &sb),
        Volume
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    FileInfo->PhysicalSize = sb.used_bytes;

    // Prepare for return
    *BufferSize = RequiredSize;
    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Returning '%s'\n"
        ), FileInfo->FileName
    ));

    return EFI_SUCCESS;
}
