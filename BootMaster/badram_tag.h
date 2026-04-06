/**
** BootMaster/badram_tag.h
**
** Copyright (c) 2025-2026 Dayo Akanji (sf.net/u/dakanji/profile)
** Released under the MIT License
**/

#ifndef __REFINDPLUS_BADRAM_H_
#define __REFINDPLUS_BADRAM_H_

EFI_STATUS ManageBadRam (
    CHAR16                *OurList  OPTIONAL,
    INTN                   OurMode,
    BOOLEAN                OurWide
);

#endif
