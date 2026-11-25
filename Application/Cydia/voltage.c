// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cydia.h"

// Match FreeBSD asmc_sensor_read: ui16 is already mV; the signed formats
// encode volts and differ in their number of fractional bits.
static UINT16 VoltageDivisor(CONST CHAR8 Type[4])
{
    static CONST struct
    {
        CHAR8 Type[5];
        UINT16 Divisor;
    } Formats[] = {{"sp78", 256}, {"sp87", 128}, {"sp4b", 2048}, {"sp5a", 1024},
                   {"sp69", 512}, {"sp96", 64},  {"sp2d", 8192}, {"ui16", 1}};
    for (UINTN i = 0; i < ARRAY_SIZE(Formats); i++) {
        if (CompareMem(Type, Formats[i].Type, 4) == 0) {
            return Formats[i].Divisor;
        }
    }
    return 0;
}

VOID CyScanVoltages(IN OUT CY_REPORT *R)
{
    UINT8 Bytes[4];
    CHAR8 Key[5] = {0}, Type[4];
    if (!R->IsApple) {
        return;
    }
    R->VoltageNote = "SMC key directory unavailable";
    if (!CySmcRead("#KEY", Bytes, 4)) {
        return;
    }
    UINT32 Count = SwapBytes32(ReadUnaligned32((CONST UINT32 *)(CONST VOID *)Bytes));
    if (Count == 0 || Count > 65536) {
        return;
    }

    // SMC key names are sorted. Find the start of V rather than walking the
    // whole directory at boot, then stop at the first non-V key.
    UINT32 Low = 0, High = Count;
    while (Low < High) {
        UINT32 Mid = Low + (High - Low) / 2;
        if (!CySmcKeyAt(Mid, Key)) {
            return;
        }
        if ((UINT8)Key[0] < 'V') {
            Low = Mid + 1;
        }
        else {
            High = Mid;
        }
    }
    R->VoltageNote = NULL;
    for (UINT32 i = Low; i < Count; i++) {
        UINT8 Len;
        if (!CySmcKeyAt(i, Key)) {
            R->VoltageNote = "incomplete: SMC key directory read failed";
            break;
        }
        if (Key[0] != 'V') {
            break;
        }
        if (i - Low >= 128 || R->VoltageCount == CY_MAX_VOLTAGES) {
            R->VoltageNote = "incomplete: voltage scan limit reached";
            break;
        }
        if (!CySmcKeyInfo(Key, &Len, Type)) {
            R->VoltageNote = "incomplete: some SMC key metadata unreadable";
            continue;
        }
        UINT16 Divisor = VoltageDivisor(Type);
        if (Len != 2 || Divisor == 0) {
            continue; // V-prefixed configuration keys are not necessarily voltages.
        }
        CY_VOLTAGE *V = &R->Voltages[R->VoltageCount++];
        CopyMem(V->Key, Key, sizeof(V->Key));
        V->Readable = CySmcRead(Key, Bytes, 2);
        if (V->Readable) {
            UINT16 Raw = SwapBytes16(ReadUnaligned16((CONST UINT16 *)(CONST VOID *)Bytes));
            V->Millivolts = Divisor == 1 ? (INT32)Raw : (INT32)(INT16)Raw * 1000 / Divisor;
        }
    }
    if (R->VoltageCount == 0 && R->VoltageNote == NULL) {
        R->VoltageNote = "no supported voltage sensors";
    }
}
