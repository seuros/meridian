// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "../Cydia/cydia.h"
#include <Library/UefiApplicationEntryPoint.h>

VOID ElaterScanNvme(CY_REPORT *R);

typedef struct
{
    UINT8 Enclosure;
    UINTN Packs;
    UINT32 DesignMwh;
} PC_INVENTORY;

static BOOLEAN Inventory(SMBIOS_STRUCTURE *Hdr, VOID *Context)
{
    PC_INVENTORY *Pc = Context;
    if (Hdr->Type == 3 && Hdr->Length > OFFSET_OF(SMBIOS_TABLE_TYPE3, Type) && Pc->Enclosure == 0) {
        Pc->Enclosure = ((SMBIOS_TABLE_TYPE3 *)Hdr)->Type & 0x7fU; // bit 7: enclosure lock
    }
    else if (Hdr->Type == 22 && Hdr->Length >= OFFSET_OF(SMBIOS_TABLE_TYPE22, DesignVoltage)) {
        SMBIOS_TABLE_TYPE22 *Pack = (SMBIOS_TABLE_TYPE22 *)Hdr;
        Pc->Packs++;
        UINT8 Multiplier = Hdr->Length > OFFSET_OF(SMBIOS_TABLE_TYPE22, DesignCapacityMultiplier)
                               ? Pack->DesignCapacityMultiplier
                               : 1;
        Pc->DesignMwh = (UINT32)Pack->DeviceCapacity * Multiplier;
    }
    return FALSE;
}

static VOID ScanEnclosure(CY_REPORT *R)
{
    PC_INVENTORY Pc = {0};
    CONST CHAR8 *Chassis = NULL;
    BOOLEAN Stationary = FALSE;
    CyWalkSmbios(Inventory, &Pc);
    switch (Pc.Enclosure) {
    case MiscChassisMiniPc:
        Chassis = "Mini PC";
        Stationary = TRUE;
        break;
    case MiscChassisTypeDeskTop:
    case MiscChassisTypeLowProfileDesktop:
    case MiscChassisTypeMiniTower:
    case MiscChassisTypeTower:
    case MiscChassisTypeSpaceSaving:
        Chassis = "Desktop";
        Stationary = TRUE;
        break;
    case MiscChassisTypeAllInOne:
        Chassis = "All-in-one";
        Stationary = TRUE;
        break;
    case MiscChassisTypeNotebook:
    case MiscChassisTypeSubNotebook:
    case MiscChassisTypeLapTop:
    case MiscChassisTypePortable:
        Chassis = "Notebook";
        break;
    case MiscChassisConvertible:
        Chassis = "Convertible";
        break;
    case MiscChassisDetachable:
        Chassis = "Detachable";
        break;
    case MiscChassisTablet:
        Chassis = "Tablet";
        break;
    default:
        break; // Unrecognized enclosure: do not infer battery policy.
    }
    CY_COMPONENT *C = CyAdd(R, "Chassis", Chassis != NULL ? CY_PRESENT : CY_UNKNOWN);
    AsciiSPrint(C->Detail, sizeof(C->Detail), "%a (SMBIOS type 3, code 0x%02x)",
                Chassis != NULL ? Chassis : "not classified", Pc.Enclosure);
    if (Pc.Packs != 0) {
        C = CyAdd(R, "Battery", CY_PRESENT);
        UINTN Used = AsciiSPrint(C->Detail, sizeof(C->Detail), "%u firmware battery record(s)",
                                 (UINT32)Pc.Packs);
        if (Pc.Packs == 1 && Pc.DesignMwh != 0) {
            AsciiSPrint(C->Detail + Used, sizeof(C->Detail) - Used, ", design %u mWh",
                        Pc.DesignMwh);
        }
        AsciiStrCatS(C->Detail, sizeof(C->Detail), "; live charge/health not measured");
    }
    else {
        C = CyAdd(R, "Battery", Stationary ? CY_NOT_APPLICABLE : CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail),
                     Stationary
                         ? "stationary chassis; no portable battery expected"
                         : "firmware supplies no portable battery record; presence not verified");
    }
}

static EFI_STATUS Scan(CY_REPORT *R)
{
    UINT32 Vendor[3];
    AsmCpuid(0, NULL, &Vendor[0], &Vendor[2], &Vendor[1]);
    if (CompareMem(Vendor, "GenuineIntel", sizeof(Vendor)) != 0 || R->IsApple) {
        Print(L"Elater requires a non-Apple Intel PC. Use Cydia for Intel Macs.\n");
        return EFI_UNSUPPORTED; // NOLINT(bugprone-signed-bitwise): EDK II ENCODE_ERROR
    }
    ScanEnclosure(R);
    CyScanMemory(R);
    CyScanStorage(R);
    CyScanDisplay(R);
    ElaterScanNvme(R);
    CyScanNetwork(R);
    CyScanTpm(R);
    CyScanUsb(R);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    return CyRunReport(ImageHandle, TRUE, Scan);
}
