// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "../Cydia/cydia.h"
#include <IndustryStandard/Nvme.h>
#include <Protocol/NvmExpressPassthru.h>

// Controller-wide SMART / Health Information, not a self-test. Retain any
// pending asynchronous event: the diagnostic must not acknowledge OS events.
static EFI_STATUS ReadHealth(EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Nvme,
                             NVME_SMART_HEALTH_INFO_LOG *Log)
{
    EFI_NVM_EXPRESS_COMMAND Cmd = {0};
    EFI_NVM_EXPRESS_COMPLETION Completion = {0};
    EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET Pkt = {0};
    Cmd.Cdw0.Opcode = NVME_ADMIN_GET_LOG_PAGE_CMD;
    Cmd.Nsid = MAX_UINT32;
    Cmd.Cdw10 = ((sizeof(*Log) / 4U - 1U) << 16U) | (UINT32)BIT15 | 0x02U;
    Cmd.Flags = CDW10_VALID;
    Pkt.NvmeCmd = &Cmd;
    Pkt.NvmeCompletion = &Completion;
    Pkt.QueueType = NVME_ADMIN_QUEUE;
    Pkt.CommandTimeout = EFI_TIMER_PERIOD_SECONDS(2);
    Pkt.TransferBuffer = Log;
    Pkt.TransferLength = sizeof(*Log);
    EFI_STATUS Status = Nvme->PassThru(Nvme, Cmd.Nsid, &Pkt, NULL);
    if (!EFI_ERROR(Status) &&
        (Pkt.TransferLength != sizeof(*Log) || (Completion.DW3 & 0x0ffe0000U) != 0)) {
        return EFI_DEVICE_ERROR; // NOLINT(bugprone-signed-bitwise): EDK II ENCODE_ERROR
    }
    return Status;
}

VOID ElaterScanNvme(CY_REPORT *R)
{
    EFI_HANDLE *Handles = NULL;
    UINTN Count = 0;
    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, &gEfiNvmExpressPassThruProtocolGuid, NULL,
                                          &Count, &Handles))) {
        CY_COMPONENT *C = CyAdd(R, "NVMe SMART", CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "firmware exposes no NVMe pass-through");
        return;
    }
    for (UINTN i = 0; i < Count && R->Count < CY_MAX_COMPONENTS - 3; ++i) {
        EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Nvme;
        CY_COMPONENT *C = CyAdd(R, "NVMe SMART", CY_UNKNOWN);
        AsciiSPrint(C->Detail, sizeof(C->Detail), "controller %u: health log unavailable",
                    (UINT32)i + 1);
        if (EFI_ERROR(gBS->HandleProtocol(Handles[i], &gEfiNvmExpressPassThruProtocolGuid,
                                          (VOID **)&Nvme)) ||
            Nvme->Mode == NULL) {
            continue;
        }
        UINTN Align = Nvme->Mode->IoAlign;
        if (Align > SIZE_1MB || (Align && (Align & (Align - 1)) != 0)) {
            continue;
        }
        NVME_SMART_HEALTH_INFO_LOG *Log = AllocateAlignedPages(1, Align);
        if (Log == NULL) {
            continue;
        }
        ZeroMem(Log, sizeof(*Log));
        EFI_STATUS Status = ReadHealth(Nvme, Log);
        if (!EFI_ERROR(Status)) {
            UINT8 Warnings = *(UINT8 *)Log;
            C->State = Warnings ? CY_DEGRADED : CY_PRESENT;
            UINTN Used =
                AsciiSPrint(C->Detail, sizeof(C->Detail),
                            "controller %u: warning 0x%02x, spare %u%%, wear estimate %u%%",
                            (UINT32)i + 1, Warnings, Log->AvailableSpare, Log->PercentageUsed);
            if (Log->CompositeTemp != 0 && Log->CompositeTemp != MAX_UINT16) {
                AsciiSPrint(C->Detail + Used, sizeof(C->Detail) - Used, ", %d C",
                            (INT32)Log->CompositeTemp - 273);
            }
        }
        else {
            AsciiSPrint(C->Detail, sizeof(C->Detail), "controller %u: health log unavailable (%r)",
                        (UINT32)i + 1, Status);
        }
        FreeAlignedPages(Log, 1);
    }
    FreePool(Handles);
}
