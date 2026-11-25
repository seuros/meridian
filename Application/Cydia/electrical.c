// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cydia.h"

// FreeBSD asmc_sensor_read treats ui16 as already in milli-units (mV/mA/mW).
// Signed fixed-point formats encode whole units with type-specific scaling.
static UINT16 SensorDivisor(CONST CHAR8 Type[4])
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

static VOID ScanGroup(CY_SENSOR_GROUP *Group, UINT32 Count, CHAR8 Prefix)
{
    UINT8 Bytes[2];
    CHAR8 Key[5] = {0}, Type[4];
    Group->Note = "SMC key directory unavailable";
    if (Count == 0 || Count > 65536) {
        return;
    }

    // SMC key names are sorted. Search just the requested prefix range.
    UINT32 Low = 0, High = Count;
    while (Low < High) {
        UINT32 Mid = Low + (High - Low) / 2;
        if (!CySmcKeyAt(Mid, Key)) {
            return;
        }
        if ((UINT8)Key[0] < (UINT8)Prefix) {
            Low = Mid + 1;
        }
        else {
            High = Mid;
        }
    }
    Group->Note = NULL;
    for (UINT32 i = Low; i < Count; i++) {
        UINT8 Len;
        if (!CySmcKeyAt(i, Key)) {
            Group->Note = "incomplete: SMC key directory read failed";
            break;
        }
        if (Key[0] != Prefix) {
            break;
        }
        if (i - Low >= 128 || Group->Count == CY_MAX_READINGS) {
            Group->Note = "incomplete: sensor scan limit reached";
            break;
        }
        if (!CySmcKeyInfo(Key, &Len, Type)) {
            Group->Note = "incomplete: some SMC key metadata unreadable";
            continue;
        }
        UINT16 Divisor = SensorDivisor(Type);
        if (Len != 2 || Divisor == 0) {
            continue; // A matching prefix alone does not identify a sensor.
        }
        CY_READING *Reading = &Group->Readings[Group->Count++];
        CopyMem(Reading->Key, Key, sizeof(Reading->Key));
        Reading->Readable = CySmcRead(Key, Bytes, 2);
        if (Reading->Readable) {
            UINT16 Raw = SwapBytes16(ReadUnaligned16((CONST UINT16 *)(CONST VOID *)Bytes));
            Reading->MilliUnits = Divisor == 1 ? (INT32)Raw : (INT32)(INT16)Raw * 1000 / Divisor;
        }
    }
    if (Group->Count == 0 && Group->Note == NULL) {
        Group->Note = "no supported sensors";
    }
}

VOID CyScanElectrical(IN OUT CY_REPORT *R)
{
    UINT8 Bytes[4];
    UINT32 Count = 0;
    if (!R->IsApple) {
        return;
    }
    if (CySmcRead("#KEY", Bytes, sizeof(Bytes))) {
        Count = SwapBytes32(ReadUnaligned32((CONST UINT32 *)(CONST VOID *)Bytes));
    }
    ScanGroup(&R->Voltages, Count, 'V');
    ScanGroup(&R->Currents, Count, 'I');
    ScanGroup(&R->Power, Count, 'P');
}
