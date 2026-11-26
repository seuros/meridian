// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cydia.h"
#include <Library/UefiApplicationEntryPoint.h>

static EFI_STATUS Scan(CY_REPORT *R)
{
    CyScanMemory(R);
    CyScanStorage(R);
    CyScanDisplay(R);
    CyScanBattery(R);
    CyScanElectrical(R);
    CyScanNetwork(R);
    CyScanThermal(R);
    CyScanShutdown(R);
    if (!R->IsApple) {
        CyScanTpm(R);
    }
    CyScanUsb(R);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    return CyRunReport(ImageHandle, FALSE, Scan);
}
