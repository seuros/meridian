/**
 * BootMaster/badram_fix.h
 *
 * Copyright (c) 2025 Dayo Akanji (sf.net/u/dakanji/profile)
 * Released under the MIT License
**/

#ifndef __REFINDPLUS_BADRAM_H_
#define __REFINDPLUS_BADRAM_H_

EFI_STATUS ManageBadRam (
    INTN                        OurBadRamFixType,
    BOOLEAN                     OurBadRamFixWide
);

#endif
