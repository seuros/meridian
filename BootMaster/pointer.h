/*
 * BootMaster/pointer.h
 * pointer device functions header file
 *
 * Copyright (c) 2018 CJ Vaughter
 * All rights reserved.
 *
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
** Copyright (c) 2020 - 2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the preceding terms.
**/

#ifndef __REFINDPLUS_POINTERDEVICE_H_
#define __REFINDPLUS_POINTERDEVICE_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#else
#include "../include/tiano_includes.h"
#endif

#ifndef _EFI_POINT_H
#include "../EfiLib/AbsolutePointer.h"
#endif

typedef struct PointerStateStruct {
    UINTN         X;
    UINTN         Y;
    BOOLEAN   Press;
    BOOLEAN Holding;
} POINTER_STATE;


#ifndef INT32_MIN
#define INT32_MIN    ((INT32) 0x80000000)         // -2,147,483,648
#endif

#ifndef INT32_MAX
#define INT32_MAX    ((INT32) 0x7FFFFFFF)         //  2,147,483,647
#endif

#ifndef UINTN_MIN
#define UINTN_MIN    ((UINTN) 0)                  //  Always 0
#endif

#ifndef UINTN_MAX
#define UINTN_MAX    ((UINTN)~0)                  //  32-bit == 4,294,967,295 (32-bit)
#endif                                            //  64-bit == 18,446,744,073,709,551,615


VOID pdInitialize (VOID);
VOID pdDraw (VOID);
VOID pdClear (
    BOOLEAN VetStatus
);

UINTN pdCount (VOID);

BOOLEAN pdAvailable (VOID);

EFI_EVENT pdWaitEvent(IN UINTN Index);

EFI_STATUS pdUpdateState (VOID);

POINTER_STATE pdGetState (VOID);

#endif
