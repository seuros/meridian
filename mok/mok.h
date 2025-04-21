#include "../include/PeImage.h"
#include "../include/PeImage2.h"

#define SHIM_LOCK_GUID \
   { 0x605dab50, 0xe046, 0x4300, {0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23} }

typedef struct _SHIM_LOCK
{
   EFI_STATUS (*shim_verify) (VOID *buffer, UINT32 size);
   EFI_STATUS (*generate_hash) (char *data, int datasize, GNUEFI_PE_COFF_LOADER_IMAGE_CONTEXT *context,
                                UINT8 *sha256hash, UINT8 *sha1hash);
   EFI_STATUS (*read_header) (void *data, unsigned int datasize, GNUEFI_PE_COFF_LOADER_IMAGE_CONTEXT *context);
} SHIM_LOCK;

//EFI_STATUS get_variable (CHAR16 *name, EFI_GUID guid, UINT32 *attributes, UINTN *size, VOID **buffer);
BOOLEAN ShimLoaded(void);
BOOLEAN ShimValidate (VOID *data, UINT32 size);
BOOLEAN secure_mode (VOID);
