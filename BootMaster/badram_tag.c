/**
** BootMaster/badram_tag.c
**
** Copyright (c) 2025-2026 Dayo Akanji (sf.net/u/dakanji/profile)
** Released under the MIT License
**/

#include "lib.h"
#include "global.h"
#include "mystrings.h"
#include "badram_tag.h"
#include "../include/refit_call_wrapper.h"


// Increment after BadRam scan changes or to force
// cache refresh on next boot on RefindPlus update.
#define TAG_VER               L"v01"

// Flags for the BadRAM cache to indicate whether
// issues arose during automated scanning or not.
#define TAG_ACE               L"ACE"
#define TAG_ERR               L"ERR"
#define TAG_XXX               L"XXX"

// Maximum pages in single bad RAM regions.
// Larger regions are likely misconfigured.
#define MAX_BADRAM_PAGES_NORM 0x040000   // ~1.0GB (  262,144 Pages @ 4k Each)
#define MAX_BADRAM_PAGES_FAST 0x100000   // ~4.0GB (1,048,576 Pages @ 4k Each)

// Maximum valid page-aligned physical address (64-bit ceiling minus one page).
// Addresses at/below this address are typically safe to pass to AllocatePages.
#define MAX_PHYSICAL_ADDRESS  0xfffffffffffff000ULL

// Maximum number of physical regions.
// Required to control runaway setups.
#define MAX_BADRAM_REGIONS    20

// Maximum number of memory pages set.
// Required to control runaway setups.
#define MAX_MEMORY_PAGES      (MAX_PHYSICAL_ADDRESS / EFI_PAGE_SIZE)


#if REFIT_DEBUG > 0
static CHAR16  *MemoryErr  = NULL;
static BOOLEAN  SizeBreach = FALSE;
#endif

static UINTN    RangeCount = 0;
static UINTN    MaxEntries = MAX_BADRAM_REGIONS;
static CHAR16  *BadRamInfo = NULL;
static BOOLEAN  GotSuccess = FALSE;
static BOOLEAN  MemDrained = FALSE;
static BOOLEAN  ManualMode = FALSE;

static
VOID LogEvalOut (
    EFI_STATUS Status
) {
    OUR_MSG_L00((
        OUR_MSG_STR(
            "Potential BadRAM Range: Evaluation ... Completed With Status:- '%r'\n"
        ), Status
    ));
} // static VOID LogEvalOut()

static
BOOLEAN SetGotSuccess (
    EFI_STATUS Status
) {
    BOOLEAN Outcome;


    Outcome = (
        !GotSuccess && Status == EFI_SUCCESS
    );
    if (Outcome) GotSuccess = TRUE;

    return Outcome;
} // static BOOLEAN SetGotSuccess()

static
BOOLEAN VetState (
    EFI_STATUS XState
) {
    return (
        !EFI_ERROR(XState)      ||
        XState == EFI_NOT_READY ||
        XState == EFI_ACCESS_DENIED
    );
} // static BOOLEAN VetState()

static
BOOLEAN SyncState (
    EFI_STATUS XState,
    EFI_STATUS Status
) {
    if (XState == EFI_SUCCESS &&
        Status == EFI_ACCESS_DENIED
    ) {
        return FALSE;
    }

    if (Status == EFI_NOT_READY) {
        if (XState == EFI_SUCCESS ||
            XState == EFI_ACCESS_DENIED
        ) {
            return FALSE;
        }
    }

    return (
        XState == EFI_ACCESS_DENIED ||
        XState == EFI_NOT_READY || (
            EFI_ERROR(Status) &&
            VetState (XState)
        )
    );
} // static BOOLEAN SyncState()

static
EFI_STATUS MarkPageUnusable (
    EFI_PHYSICAL_ADDRESS PageAddress
) {
    return REFIT_CALL_4_WRAPPER(
        gBS->AllocatePages, AllocateAddress,
        EfiUnusableMemory, 1, &PageAddress
    );
} // static EFI_STATUS MarkPageUnusable()

static
EFI_STATUS MarkPageReserved (
    EFI_PHYSICAL_ADDRESS PageAddress
) {
    return REFIT_CALL_4_WRAPPER(
        gBS->AllocatePages, AllocateAddress,
        EfiReservedMemoryType, 1, &PageAddress
    );
} // static EFI_STATUS MarkPageReserved()

static
EFI_STATUS GetMemMapBuffer (
    EFI_MEMORY_DESCRIPTOR **MapTheMemory,
    UINTN                  *MemoryMapSize,
    UINTN                  *DescriptorSize
) {
    EFI_STATUS              Status;
    UINT32                  DescriptorVersion;
    UINTN                   MapKey;
    INTN                    Calc;


    // Get Memory Map Size.
    *MemoryMapSize = 0;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->GetMemoryMap, MemoryMapSize,
        NULL, &MapKey,
        DescriptorSize, &DescriptorVersion
    );
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return EFI_LOAD_ERROR;
    }

    // 'INTN' Below is Deliberate.
    //  Mitigates wraparound risk.
    for (INTN i = 0; i < 12; i++) {
        // Slack to Handle Potential Map Growth.
        // Can result from the AllocatePool call.
        // Skip adding slack on the first attempt.
        Calc = 2 + ((i - 1) * 2);
        *MemoryMapSize += (
            *DescriptorSize * (UINTN) Calc
        );

        // Allocate Memory Map Size.
        Status = REFIT_CALL_3_WRAPPER(
            gBS->AllocatePool, EfiBootServicesData,
            *MemoryMapSize, (VOID **) MapTheMemory
        );
        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            MemoryErr = L"In 'GetMemMapBuffer' Func:- 'Memory Allocation Failure'";
            #endif

            MemDrained = TRUE;
            MY_FREE_POOL(*MapTheMemory);

            // Normalise to OOR Status.
            return EFI_OUT_OF_RESOURCES;
        }

        // Get Memory Map Data.
        Status = REFIT_CALL_5_WRAPPER(
            gBS->GetMemoryMap, MemoryMapSize,
            *MapTheMemory, &MapKey,
            DescriptorSize, &DescriptorVersion
        );
        if (!EFI_ERROR(Status) || Status != EFI_BUFFER_TOO_SMALL) break;

        MY_FREE_POOL(*MapTheMemory);
    } // for INTN i

    if (EFI_ERROR(Status)) {
        MY_FREE_POOL(*MapTheMemory);
        Status = EFI_DEVICE_ERROR;
    }

    return Status;
} // static EFI_STATUS GetMemMapBuffer()

static
BOOLEAN IsValidType (
    EFI_MEMORY_TYPE Type
) {
    return (
        Type == EfiConventionalMemory ||
        Type == EfiRuntimeServicesCode ||
        Type == EfiRuntimeServicesData ||
        Type == EfiBootServicesCode   ||
        Type == EfiBootServicesData  ||
        Type == EfiLoaderCode       ||
        Type == EfiLoaderData
    );
} // static BOOLEAN IsValidType()

static
BOOLEAN IsPageUsed (
    EFI_PHYSICAL_ADDRESS   PageAddress,
    EFI_MEMORY_DESCRIPTOR *PageMemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize
) {
    BOOLEAN                IsInUse;
    EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;


    IsInUse = FALSE;
    for (MemoryMapEntry = PageMemoryMap;
        (UINT8 *) MemoryMapEntry < (
            (UINT8 *) PageMemoryMap + MemoryMapSize
        );
        MemoryMapEntry = (EFI_MEMORY_DESCRIPTOR *) (
            (UINT8 *) MemoryMapEntry + DescriptorSize
        )
    ) {
        if (PageAddress >= MemoryMapEntry->PhysicalStart &&
            PageAddress <  MemoryMapEntry->PhysicalStart + (
                MemoryMapEntry->NumberOfPages * EFI_PAGE_SIZE
            )
        ) {
            IsInUse = (
                MemoryMapEntry->Type != EfiConventionalMemory
            );

            break;
        }
    } // for

    return IsInUse;
} // static BOOLEAN IsPageUsed()

static
BOOLEAN ProbePage (
    EFI_PHYSICAL_ADDRESS  TheAddress
) {
    BOOLEAN               Fix;
    UINT64               *Ptr;
    UINT64                Data1;
    UINT64                Data2;
    UINTN                 Pages, i;


    Fix   = FALSE;
    Ptr   = (UINT64 *) TheAddress;
    Pages = EFI_PAGE_SIZE / sizeof (UINT64);
    Data1 = 0x5555555555555555ULL; // 0101... bit pattern.
    Data2 = 0xAAAAAAAAAAAAAAAAULL; // 1010... bit pattern (Data1 Toggle).

    // Only probe zeroed memory locations to avoid data corruption.
    // Assumes zeroed regions are safe to test-write at this stage.
    for (i = 0; i < Pages; i++) {
        if (Ptr[i] != 0) {
            // Consider Page "Good".
            // In use and not empty.
            return TRUE;
        }

        // Empty Slot ... Test.
        Ptr[i] = Data1;
        if (Ptr[i] != Data1) {
            Fix = TRUE;
        }
        else {
            Ptr[i] = Data2;
            if (Ptr[i] != Data2) {
                Fix = TRUE;
            }
        }

        // Zero Slot Out.
        // Original Data.
        Ptr[i] = 0;

        if (Fix) break;
    }

    return !Fix; // Deliberate
} // static BOOLEAN ProbePage()

static
EFI_MEMORY_TYPE GetAddressMemType (
    EFI_PHYSICAL_ADDRESS   PageAddress,
    EFI_MEMORY_DESCRIPTOR *AddrMemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize
) {
    UINTN                  SizeTotalPages;
    EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;


    for (MemoryMapEntry = AddrMemoryMap;
        (UINT8 *) MemoryMapEntry < (
            (UINT8 *) AddrMemoryMap + MemoryMapSize
        );
        MemoryMapEntry = (EFI_MEMORY_DESCRIPTOR *) (
            (UINT8 *) MemoryMapEntry + DescriptorSize
        )
    ) {
        SizeTotalPages   = MemoryMapEntry->NumberOfPages  * EFI_PAGE_SIZE;
        if (PageAddress >= MemoryMapEntry->PhysicalStart &&
            PageAddress < (MemoryMapEntry->PhysicalStart  + SizeTotalPages)
        ) {
            return MemoryMapEntry->Type;
        }
    } // for

    // Return non-qualifying type if unknown type.
    return EfiReservedMemoryType;
} // static EFI_MEMORY_TYPE GetAddressMemType()

static
EFI_STATUS AddBadRange (
    IN OUT CHAR16               **BadRamData,
    IN     EFI_PHYSICAL_ADDRESS   PageAddress,
    IN     EFI_PHYSICAL_ADDRESS   RangeStart
) {
    CHAR16                       *BadRamTemp;


    if (*BadRamData == NULL) {
        BadRamTemp = PoolPrint (
            L"0x%lx:0x%lx",
            RangeStart, PageAddress
        );
    }
    else {
        BadRamTemp = PoolPrint (
            L"%s,0x%lx:0x%lx",
            *BadRamData, RangeStart, PageAddress
        );
    }

    if (BadRamTemp == NULL) {
        #if REFIT_DEBUG > 0
        MemoryErr = L"In 'AddBadRange' Func:- 'Memory Allocation Failure'";
        #endif

        MemDrained = TRUE;
        return EFI_OUT_OF_RESOURCES;
    }

    // Only Change if Allocated
    MY_FREE_POOL(*BadRamData);
    *BadRamData = BadRamTemp;

    return EFI_SUCCESS;
} // static EFI_STATUS AddBadRange()

static
EFI_STATUS AutoScanRam (
    EFI_MEMORY_DESCRIPTOR *AutoMemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize,
    INTN                   FixAutoScanMode
) {
    #if REFIT_DEBUG > 0
    BOOLEAN                CheckMute = FALSE;
    #endif

    EFI_STATUS             Status;
    EFI_STATUS             XState;
    EFI_STATUS             TmpStatus;
    UINTN                  ListSize, i;
    UINTN                  AbortCache;
    UINTN                  SeenRanges;
    UINTN                  TempNumber;
    CHAR16                *FlagCache;
    CHAR16                *FlagError;
    BOOLEAN                GoodProbe;
    BOOLEAN                CloseThis;
    BOOLEAN                PageState;
    BOOLEAN                GotStatus;
    BOOLEAN                EnterSeek;
    BOOLEAN                InBadRange;
    EFI_PHYSICAL_ADDRESS   RangeStart;
    EFI_PHYSICAL_ADDRESS   PageAddress;
    EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;


    if (FixAutoScanMode != 8 && FixAutoScanMode != 9) {
        return EFI_NOT_READY;
    }

    PageAddress =             0;
    RangeStart  =             0;
    SeenRanges  =             0;
    AbortCache  =             0;
    InBadRange  =         FALSE;
    GotStatus   =         FALSE;
    EnterSeek   =         FALSE;
    Status      = EFI_NOT_READY;
    XState      = EFI_NOT_READY;

    for (MemoryMapEntry = AutoMemoryMap;
        (UINT8 *) MemoryMapEntry < (
            (UINT8 *) AutoMemoryMap + MemoryMapSize
        );
        MemoryMapEntry = (EFI_MEMORY_DESCRIPTOR *) (
            (UINT8 *) MemoryMapEntry + DescriptorSize
        )
    ) { // Outer Loop
        if (MemoryMapEntry->Type != EfiConventionalMemory) {
            // Only consider EfiConventionalMemory.
            // Basically avoid probing other types.
            continue; // Outer Loop
        }

        for (i = 0; i < MemoryMapEntry->NumberOfPages; i++) { // Inner Loop 1
            Status = EFI_NOT_READY;
            PageAddress = MemoryMapEntry->PhysicalStart + (
                i * EFI_PAGE_SIZE
            );
            if (PageAddress == 0) {
                // Firmware Restricted on x86.
                // Contains the Real Mode IVT.
                // Deliberately for all Archs.
                continue; // Inner Loop 1
            }

            if (PageAddress > MAX_PHYSICAL_ADDRESS) {
                // Beyond Limits ... Skip.
                Status = EFI_BAD_BUFFER_SIZE;
                if (VetState (XState)) {
                    XState = Status;
                }

                break; // Inner Loop 1
            }

            CloseThis = FALSE;
            GoodProbe = ProbePage (
                PageAddress
            );
            if (!GoodProbe) {
                if (!InBadRange) {
                    EnterSeek = TRUE;
                    OUR_MSG_L00((
                        OUR_MSG_STR(
                            "\n\nPotential BadRAM Range: Evaluating...\n"
                        )
                    ));

                    // New Bad Range ... Verify.
                    if (SeenRanges == MaxEntries) {
                        // Region[1 + MaxEntries] ... Exit.
                        Status = EFI_UNSUPPORTED;
                        if (VetState (XState)) {
                            XState = EFI_UNSUPPORTED;
                        }

                        break; // Inner Loop 1
                    }

                    // New Bad Range ... Start.
                    InBadRange = TRUE;
                    RangeStart = PageAddress;
                }

                TempNumber = (
                    PageAddress - RangeStart
                ) + EFI_PAGE_SIZE;
                TempNumber /= EFI_PAGE_SIZE;
                if (TempNumber > MAX_BADRAM_PAGES_NORM) {
                    // Oversized ... Skip.
                    Status = EFI_BAD_BUFFER_SIZE;
                    break; // Inner Loop 1
                }

                // Handle Page Marking.
                PageState = IsPageUsed (
                    PageAddress, AutoMemoryMap,
                    MemoryMapSize, DescriptorSize
                );

                if (!PageState) {
                    Status = MarkPageUnusable (
                        PageAddress
                    );
                    if (FixAutoScanMode == 8) {
                        SetGotSuccess (Status);
                    }
                }
                else if (FixAutoScanMode == 9) {
                    Status = MarkPageReserved (
                        PageAddress
                    );
                }
                else { // FixAutoScanMode == 8
                    Status = (
                        GotSuccess
                    ) ? EFI_SUCCESS : EFI_ACCESS_DENIED;
                }

                if (TempNumber == MAX_BADRAM_PAGES_NORM) {
                    // At Limit ... Tag.
                    CloseThis = TRUE;
                }
            } // if !GoodProbe

            if (InBadRange && (GoodProbe || CloseThis)) {
                do { // Inner Loop 1A
                    if (!GotStatus      &&
                        FixAutoScanMode == 8
                    ) {
                        break; // Inner Loop 1A
                    }

                    // Bad Range Close ... Reset.
                    InBadRange = FALSE;
                    SeenRanges++;

                    if (GoodProbe) {
                        // Remove Good Page.
                        PageAddress -= EFI_PAGE_SIZE;
                    }

                    OUR_MSG_L00((
                        OUR_MSG_STR(
                            "Potential BadRAM Range:   - Caching Self Derived BadRAM Range %02d"
                        ), SeenRanges
                    ));

                    // Add Range to Cache.
                    // Even if 'bad' size.
                    TmpStatus = AddBadRange (
                        &BadRamInfo,
                        PageAddress,
                        RangeStart
                    );

                    OUR_MSG_L00((
                        OUR_MSG_STR(
                            " ... Completed With Status:- '%r'\n"
                        ), TmpStatus
                    ));

                    if (EFI_ERROR(TmpStatus)) {
                        // Means Out of Resources.
                        Status = TmpStatus;
                    }

                    // Bad Range Close ... Verify.
                    TempNumber = (
                        PageAddress - RangeStart
                    ) + EFI_PAGE_SIZE;
                    TempNumber /= EFI_PAGE_SIZE;
                    if (TempNumber > MAX_BADRAM_PAGES_NORM) {
                        // Oversized ... Flag.
                        // Unlikely to Happen.
                        Status = EFI_BAD_BUFFER_SIZE;
                    }
                } while (0); // This 'loop' only runs once - Inner Loop 1A

                if (EnterSeek) {
                    EnterSeek = FALSE;
                    LogEvalOut (Status);
                }
            } // if InBadRange etc

            if (!GotStatus      &&
                FixAutoScanMode == 8 &&
                Status != EFI_NOT_READY &&
                Status != EFI_ACCESS_DENIED
            ) {
                GotStatus = TRUE;
            }

            if (SyncState (XState, Status)) {
                XState = Status;
            }
        } // for i - Inner Loop 1

        if (Status == EFI_UNSUPPORTED) {
            // Beyond Limits ... Exit.
            LogEvalOut (Status);
            break; // Outer Loop
        }

        if (InBadRange) {
            OUR_MSG_L00((
                OUR_MSG_STR(
                    "Potential BadRAM Range:   * Range 'Descriptor-End' Handler ... START\n"
                )
            ));

            // Range 'Descriptor-End' ... Handle.
            InBadRange = FALSE;
            SeenRanges++;

            if (PageAddress > MAX_PHYSICAL_ADDRESS) {
                // Beyond Limits ... Skip.
                Status = EFI_BAD_BUFFER_SIZE;
                break; // Outer Loop
            }

            do { // Inner Loop 1B
                if (!GotStatus      &&
                    FixAutoScanMode == 8
                ) {
                    SeenRanges--;
                    break; // Inner Loop 1B
                }

                OUR_MSG_L00((
                    OUR_MSG_STR(
                        "Potential BadRAM Range:   - Caching Self Derived BadRAM Range %02d"
                    ), SeenRanges
                ));

                // Add Range to Cache.
                // Even if 'bad' size.
                TmpStatus = AddBadRange (
                    &BadRamInfo,
                    PageAddress,
                    RangeStart
                );

                OUR_MSG_L00((
                    OUR_MSG_STR(
                        " ... Completed With Status:- '%r'\n"
                    ), TmpStatus
                ));

                if (EFI_ERROR(TmpStatus)) {
                    // Means Out of Resources.
                    Status = TmpStatus;
                }

                // Close Range 'Descriptor-End' ... Verify.
                TempNumber = (
                    PageAddress - RangeStart
                ) + EFI_PAGE_SIZE;
                TempNumber /= EFI_PAGE_SIZE;
                if (TempNumber > MAX_BADRAM_PAGES_NORM) {
                    // Oversized ... Flag.
                    // Unlikely to Happen.
                    Status = EFI_BAD_BUFFER_SIZE;
                }
            } while (0); // This 'loop' only runs once - Inner Loop 1B

            if (SyncState (XState, Status)) {
                XState = Status;
            }

            OUR_MSG_L00((
                OUR_MSG_STR(
                    "Potential BadRAM Range:   * Range 'Descriptor-End' Handler ... CLOSE\n"
                )
            ));

            if (EnterSeek) {
                EnterSeek = FALSE;
                LogEvalOut (Status);
            }
        } // if InBadRange
    } // for MemoryMapEntry = AutoMemoryMap - Outer Loop

    // Attempt to Cache Whatever Was Derived.
    // This includes even when error was met.
    FlagError = EFI_ERROR(
        XState
    ) ? TAG_ERR : TAG_ACE;

    if (BadRamInfo == NULL) {
        BadRamInfo = PoolPrint (
            L"%s-%s-%02d|%s",
            TAG_VER, FlagError,
            FixAutoScanMode, TAG_XXX
        );
        if (BadRamInfo == NULL) {
            AbortCache = 1;
        }
    }
    else {
        FlagCache = PoolPrint (
            L"%s-%s-%02d|%s",
            TAG_VER, FlagError,
            FixAutoScanMode, BadRamInfo
        );

        MY_FREE_POOL(BadRamInfo);

        if (FlagCache == NULL) {
            AbortCache = 2;
        }
        else {
            BadRamInfo = FlagCache;
        }
    }

    if (AbortCache > 0) {
        #if REFIT_DEBUG > 0
        if (AbortCache == 1) {
            MemoryErr = L"In 'AutoScanRam' Func:- 'PoolPrint Failure 01'";
        }
        else {
            MemoryErr = L"In 'AutoScanRam' Func:- 'PoolPrint Failure 02'";
        }
        #endif

        MemDrained = TRUE;
        return EFI_OUT_OF_RESOURCES;
    }

    // Cache BadRamInfo.
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    ListSize = StrSize (
        BadRamInfo
    );
    EfivarSetRaw (
        &RefindPlusGuid, L"BadRamTag",
        BadRamInfo, ListSize, TRUE
    );
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    return XState;
} // static EFI_STATUS AutoScanRam()

static
EFI_STATUS HandleRange (
    EFI_PHYSICAL_ADDRESS   StartAddress,
    UINTN                  FixPageCount,
    INTN                   FixPageMode,
    BOOLEAN                FixPageWide
) {
    EFI_STATUS             Status;
    EFI_STATUS             XState;
    UINTN                  i;
    UINTN                  DescriptorSize;
    UINTN                  MemoryMapSize;
    BOOLEAN                GotUsedPages;
    BOOLEAN                ValidTarget;
    EFI_MEMORY_TYPE        MemoryType;
    EFI_PHYSICAL_ADDRESS   ScanAddress;
    EFI_PHYSICAL_ADDRESS   PageAddress;
    EFI_MEMORY_DESCRIPTOR *MapOurMemory;


    if (FixPageMode < 8 && StartAddress == 0) {
        // Firmware Restricted on x86.
        // Contains the Real Mode IVT.
        // Deliberately for all Archs.
        if (FixPageCount == 1) {
            // Only Page 0 ... Error Out.
            return EFI_INVALID_PARAMETER;
        }

        // Drop Page 0.
        FixPageCount -= 1;
        StartAddress += EFI_PAGE_SIZE;
    }

    if (FixPageMode == 1) {
        Status = REFIT_CALL_4_WRAPPER(
            gBS->AllocatePages, AllocateAddress,
            EfiRuntimeServicesData, FixPageCount, &StartAddress
        );
        if (Status == EFI_OUT_OF_RESOURCES) {
            MemDrained = TRUE;

            #if REFIT_DEBUG > 0
            MemoryErr = L"In 'HandleRange' Func:- 'AllocatePages Failure'";
            #endif
        }

        return Status;
    }

    if (FixPageMode < 8 && FixPageWide) {
        // Fast Path: Attempt to allocate the entire range as runtime data.
        // If successful, the memory is effectively removed from general use.
        // Fall back on per-page marking if the allocation optimisation fails.
        Status = REFIT_CALL_4_WRAPPER(
            gBS->AllocatePages, AllocateAddress,
            EfiRuntimeServicesData, FixPageCount, &StartAddress
        );
        if (!EFI_ERROR(Status)) {
            return Status;
        }

        // Vet Against 'Actual' Pages Limit.
        if (FixPageCount > MAX_BADRAM_PAGES_NORM) {
            #if REFIT_DEBUG > 0
            SizeBreach = TRUE;
            #endif

            return EFI_BAD_BUFFER_SIZE;
        }
    }

    MemoryMapSize = 0;
    MapOurMemory = NULL;

    Status = GetMemMapBuffer (
        &MapOurMemory,
        &MemoryMapSize,
        &DescriptorSize
    );
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (FixPageMode == 8 || FixPageMode == 9) {
        Status = AutoScanRam (
            MapOurMemory, MemoryMapSize,
            DescriptorSize, FixPageMode
        );

        MY_FREE_POOL(MapOurMemory);
        return Status;
    }

    GotUsedPages = FALSE;
    XState = EFI_NOT_READY;

    do { // Outer Loop
        // Handle These Modes First (For Speed).
        if (FixPageMode == 4 || FixPageMode == 5) {
            // Pre-scan all pages to determine whether any
            // is in use, or non-qualifying, before acting.
            for (i = 0; i < FixPageCount; i++) { // Inner Loop 1
                ScanAddress = StartAddress + (
                    i * EFI_PAGE_SIZE
                );

                GotUsedPages = IsPageUsed (
                    ScanAddress, MapOurMemory,
                    MemoryMapSize, DescriptorSize
                );
                if (GotUsedPages) break; // Inner Loop 1
            } // for i - Inner Loop 1

            if (FixPageMode == 4 && GotUsedPages) {
                if (VetState (XState) && !GotSuccess) {
                    XState = EFI_ACCESS_DENIED;
                }

                break; // Outer Loop ... Whole region handled
            }

            for (i = 0; i < FixPageCount; i++) { // Inner Loop 2
                PageAddress = StartAddress + (
                    i * EFI_PAGE_SIZE
                );

                Status = (
                    GotUsedPages
                ) ? MarkPageReserved (
                    PageAddress
                ) : MarkPageUnusable (
                    PageAddress
                );
                if (GotUsedPages     &&
                    FixPageMode == 5 &&
                    EFI_ERROR(Status)
                ) {
                    // Suppress Error.
                    Status = EFI_SUCCESS;
                }

                if (Status == EFI_SUCCESS &&
                    XState == EFI_NOT_READY
                ) {
                    XState = EFI_SUCCESS;
                    if (FixPageMode == 4) {
                        GotSuccess = TRUE;
                    }
                }

                if (VetState (XState)) {
                    if (EFI_ERROR(Status) && XState != EFI_ACCESS_DENIED) {
                        /* coverity[dead_error_line: SUPPRESS] */
                        XState = Status;
                    }
                    else {
                        if (FixPageMode == 4 &&
                            SetGotSuccess (Status)
                        ) {
                            XState = EFI_SUCCESS;
                        }
                    }
                } // if VetState (XState)
            } // for i - Inner Loop 2

            break; // Outer Loop ... Whole region handled
        } // if FixPageMode == 4 || 5

        // FixPageMode == 2 || 3 || 6 || 7
        for (i = 0; i < FixPageCount; i++) { // Inner Loop 3
            PageAddress = StartAddress + (
                i * EFI_PAGE_SIZE
            );

            if (FixPageMode == 2) {
                Status = MarkPageUnusable (
                    PageAddress
                );
            }
            else if (FixPageMode == 3) {
                Status = MarkPageReserved (
                    PageAddress
                );
            }
            else { // FixPageMode == 6 || 7
                Status = EFI_NOT_READY;
                MemoryType = GetAddressMemType (
                    PageAddress, MapOurMemory,
                    MemoryMapSize, DescriptorSize
                );

                ValidTarget = IsValidType (
                    MemoryType
                );
                if (ValidTarget) {
                    GotUsedPages = IsPageUsed (
                        PageAddress, MapOurMemory,
                        MemoryMapSize, DescriptorSize
                    );
                    if (!GotUsedPages) {
                        Status = MarkPageUnusable (
                            PageAddress
                        );
                        if (FixPageMode == 6) {
                            SetGotSuccess (Status);
                        }
                    }
                    else if (FixPageMode == 7) {
                        Status = MarkPageReserved (
                            PageAddress
                        );
                    }
                    else { // FixPageMode == 6
                        Status = (
                            GotSuccess
                        ) ? EFI_SUCCESS : EFI_ACCESS_DENIED;
                    }
                } // if ValidTarget
            } // if/else FixPageMode == 2 etc

            if (SyncState (XState, Status)) {
                XState = Status;
            }
        } // for i - Inner Loop 3
    } while (0); // This 'loop' only runs once - Outer Loop

    MY_FREE_POOL(MapOurMemory);
    return XState;
} // static EFI_STATUS HandleRange()

EFI_STATUS ManageBadRam (
    CHAR16                *OurList  OPTIONAL,
    INTN                   OurMode,
    BOOLEAN                OurWide
) {
    #if REFIT_DEBUG > 0
    CHAR16                *MsgErrorA =  NULL;
    CHAR16                *MsgErrorB =  NULL;
    BOOLEAN                CheckMute = FALSE;
    #endif

    EFI_STATUS             Status;
    EFI_STATUS             XState;
    INTN                   OrigMode;
    UINTN                  SizeBound;
    UINTN                  TempNumber;
    UINTN                  NumPages, i;
    CHAR16                *TheFixList;
    CHAR16                *CachedList;
    CHAR16                *VersionTag;
    CHAR16                *AddressDuo;
    CHAR16                *AddressOne;
    CHAR16                *AddressTwo;  // Do *NOT* Free (mid-buf pointer into AddressDuo)
    UINT64                 SizeBytes;
    UINT64                 SizeLimit;
    BOOLEAN                ListBreak;
    BOOLEAN                TempCheck;
    BOOLEAN                DropCache;
    EFI_PHYSICAL_ADDRESS   TopBadRAM;
    EFI_PHYSICAL_ADDRESS   EndBadRAM;


    #if REFIT_DEBUG > 0
    LOG_MSG("M A R K   D E F E C T I V E   M E M O R Y");
    LOG_MSG("\n");

    MemoryErr  =          NULL;
    #endif

    MaxEntries = MAX_BADRAM_REGIONS;
    RangeCount =                  0;
    TheFixList =               NULL;
    CachedList =               NULL;
    MemDrained =              FALSE;
    GotSuccess =              FALSE;
    OrigMode   =            OurMode;
    XState     =      EFI_NOT_READY;

    MY_FREE_POOL(BadRamInfo);

    if (OurMode < 0 || OurMode > 9) {
        // Disable and clear cache later.
        // Leverages '-1' to clear cache.
        // Input > 9 should already be 0.
        // - Set during config file read.
        // - Only here for full coverage.
        OurMode = 0;
    }

    ManualMode = (
        OrigMode != 8 && OrigMode != 9
    );

    if (OurMode == 8 || OurMode == 9) {
        MaxEntries *= 2;
        VersionTag = NULL;

        do { // Outer Loop 1
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            Status = EfivarGetRaw (
                &RefindPlusGuid, L"BadRamTag",
                (VOID **) &BadRamInfo, NULL
            );
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            if (EFI_ERROR(Status)) break; // Outer Loop 1

            VersionTag = PoolPrint (
                L"%s-%s-%02d|",
                TAG_VER, TAG_ACE, OurMode
            );
            if (VersionTag == NULL) {
                // Disable for invalid setting.
                // Cache preserved if present.
                // Skips the check for cache.
                Status     = EFI_OUT_OF_RESOURCES;
                XState     = Status;
                MemDrained = TRUE;
                OurMode    = 0;

                #if REFIT_DEBUG > 0
                MemoryErr = L"Automated Scan:- 'Memory Allocation Failure 01'";
                #endif

                break; // Outer Loop 1
            }

            if (!MyStrBegins (VersionTag, BadRamInfo)) {
                MY_FREE_POOL(VersionTag);
                VersionTag = PoolPrint (
                    L"%s-%s-%02d|",
                    TAG_VER, TAG_ERR, OurMode
                );
                if (VersionTag == NULL) {
                    // See earlier notes on equivalent.
                    Status     = EFI_OUT_OF_RESOURCES;
                    XState     = Status;
                    MemDrained = TRUE;
                    OurMode    = 0;

                    #if REFIT_DEBUG > 0
                    MemoryErr = L"Automated Scan:- 'Memory Allocation Failure 02'";
                    #endif

                    break; // Outer Loop 1
                }

                if (!MyStrBegins (VersionTag, BadRamInfo)) {
                    // Discard existing cache.
                    MY_FREE_POOL(BadRamInfo);
                    Status = EFI_NOT_FOUND;

                    break; // Outer Loop 1
                }
            }

            // RAM previously scanned and results saved to nvRAM.
            if (!IsStriStr (BadRamInfo, L":0x")) {
                // Indicates stored cache is 'v??-???-??|XXX'.
                // Bad RAM addresses NOT FOUND on initial scan.
                // Set Mode to 0 to skip execution and log this.
                XState = EFI_ALREADY_STARTED;
                OurMode = 0;

                #if REFIT_DEBUG > 0
                MsgErrorA = L"Automated Scan:- 'Bad RAM Ranges *NOT* Found'";
                #endif

                break; // Outer Loop 1
            }

            // Use duplicate for memory management.
            // See 'CachedList' free near func end.
            CachedList = StrDuplicate (BadRamInfo);
            if (CachedList == NULL) {
                // See earlier notes on equivalent.
                Status     = EFI_OUT_OF_RESOURCES;
                XState     = Status;
                MemDrained = TRUE;
                OurMode    = 0;

                #if REFIT_DEBUG > 0
                MemoryErr = L"Automated Scan:- 'Memory Allocation Failure 03'";
                #endif

                break; // Outer Loop 1
            }

            // Bad RAM Addresses found on previous scan.
            // Set to Mode 6/7 for the actual execution.
            OurMode -= 2;

            // Strip version stamp prefix before use as address list.
            // IsStriStr call confirmed the presence of 'VersionTag'.
            TheFixList = GetSubStrAfter (
                VersionTag, CachedList
            );
        } while (0); // This 'loop' only runs once - Outer Loop 1

        MY_FREE_POOL(VersionTag);
    } // if OurMode == 8 || 9

    do { // Outer Loop 2
        if (OurMode == 0) {
            if (XState == EFI_NOT_READY) {
                XState  = EFI_NOT_STARTED;
            }

            break; // Outer Loop 2
        }

        if (OurMode == 8 || OurMode == 9) {
            OUR_MSG_L00((
                OUR_MSG_STR(
                    "\nScanning Memory Map for Bad RAM ... Please Wait\n"
                )
            ));

            XState = HandleRange (
                0, 0, OurMode, OurWide
            );

            #if REFIT_DEBUG > 0
            if (EFI_ERROR(XState)) {
                if (XState == EFI_OUT_OF_RESOURCES) {
                    MsgErrorA = (
                        MemoryErr != NULL
                    ) ? MemoryErr :  L"Automated Scan:- 'Memory Allocation Failure 00'";
                }
                else {
                    switch (XState) {
                        case EFI_LOAD_ERROR:       MsgErrorA = L"Automated Scan:- 'UEFI Protocol Failure'"      ; break;
                        case EFI_UNSUPPORTED:      MsgErrorA = L"Automated Scan:- 'Bad Ram Ranges > 20 Limit'"  ; break;
                        case EFI_DEVICE_ERROR:     MsgErrorA = L"Automated Scan:- 'Apparent Memory Map Failure'"; break;
                        case EFI_BAD_BUFFER_SIZE:  MsgErrorA = L"Automated Scan:- 'Invalid Bad RAM Range Size'" ; break;
                        default:                   MsgErrorA = L"Automated Scan:- 'Unsanitised Error Status'"   ; break;
                    } // switch
                }
            }
            #endif

            OUR_MSG_L00((
                OUR_MSG_STR(
                    "\nScanning Memory Map for Bad RAM ... Completed With Status:- '%r'\n"
                ), XState
            ));

            break; // Outer Loop 2
        }

        if (TheFixList == NULL) {
            if (OrigMode == 8 || OrigMode == 9) {
                XState = EFI_NOT_FOUND;

                #if REFIT_DEBUG > 0
                MsgErrorA = L"Automated Scan:- 'Failed to Determine BadRAM Ranges'";
                #endif

                break; // Outer Loop 2
            }

            if (OurList == NULL) {
                XState = EFI_INVALID_PARAMETER;

                #if REFIT_DEBUG > 0
                MsgErrorA = L"Config Setting Missing:- 'badram_tag_list'";
                #endif

                break; // Outer Loop 2
            }

            TheFixList = StrDuplicate (OurList);
            if (TheFixList == NULL) {
                MemDrained = TRUE;
                XState = EFI_OUT_OF_RESOURCES;

                #if REFIT_DEBUG > 0
                MemoryErr = L"Basic Memory Allocation Failure 01";
                #endif

                break; // Outer Loop 2
            }
        } // if TheFixList == NULL

        RangeCount = CountListItems (
            TheFixList
        );
        ListBreak = (
            RangeCount > MaxEntries
        ) ? TRUE : FALSE;

        i = 0;
        while (1) { // Inner Loop 2A
            AddressOne = NULL;
            AddressTwo = NULL; // Avoid Dangling Pointer

            if (ListBreak && i == MaxEntries) {
                // Prioritise this.
                XState = EFI_INVALID_PARAMETER;

                #if REFIT_DEBUG > 0
                if (ManualMode) {
                    MsgErrorA = L"In 'badram_tag_list' Config:- 'Address Entries > 20 Limit'";
                }
                else {
                    MsgErrorA = L"In Cached List:- 'Address Entries > MaxEntries'";
                }

                MsgErrorB = L"Maximum allowable number of configured addresses exceeded";
                #endif

                break; // Inner Loop 2A
            }

            AddressDuo = FindCommaDelimited (
                TheFixList, i++
            );
            if (AddressDuo == NULL) break; // Inner Loop 2A

            OUR_MSG_L01((
                OUR_MSG_STR(
                    "%s BadRAM Ranges: Handling %02d of %02d"
                ),
                (ManualMode) ? L"User Defined" : L"Self Derived",
                i, RangeCount
            ));

            do { // Inner Loop 2A 1
                if (!IsStriStr (AddressDuo, L":0x")) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Invalid BadRAM Address Pair Structure'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Invalid BadRAM Address Pair Structure'";
                        }

                        MsgErrorB = L"A start/end address separator is missing";
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }

                if (!MyStrBegins (L"0x", AddressDuo)) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Invalid BadRAM Start Address Format'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Invalid BadRAM Start Address Format'";
                        }

                        MsgErrorB = L"A start address does not begin with '0x'";
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }

                AddressOne = GetSubStrBefore (
                    L":", AddressDuo
                );
                if (AddressOne == NULL) {
                    if (VetState (XState)) {
                        MemDrained = TRUE;
                        XState = EFI_OUT_OF_RESOURCES;

                        #if REFIT_DEBUG > 0
                        MemoryErr = L"Basic Memory Allocation Failure 02";
                        #endif
                    }

                    Status = EFI_OUT_OF_RESOURCES;
                    break; // Inner Loop 2A 1
                }

                if (!IsValidHex (AddressOne)) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Invalid BadRAM Start Address Entry'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Invalid BadRAM Start Address Entry'";
                        }

                        MsgErrorB = L"Supplied address(es) must be valid hex";
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }

                TopBadRAM = StrToHex (
                    AddressOne, 0, 16
                );
                if (TopBadRAM == 0) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Invalid BadRAM Start Address Entry'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Invalid BadRAM Start Address Entry'";
                        }

                        MsgErrorB = L"A start address is the x86-Firmware-Restricted 'Address Zero'";
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }

                AddressTwo = GetSubStrAfter  (
                    L":", AddressDuo
                );
                if (!MyStrBegins (L"0x", AddressTwo)) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Invalid BadRAM End Address Entry'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Invalid BadRAM End Address Entry'";
                        }

                        MsgErrorB = L"An end address does not begin with '0x'";
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }

                if (!IsValidHex (AddressTwo)) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Invalid BadRAM End Address Entry'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Invalid BadRAM End Address Entry'";
                        }

                        MsgErrorB = L"Addresses must be valid hex";
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }

                EndBadRAM = StrToHex (
                    AddressTwo, 0, 16
                );
                if (EndBadRAM < TopBadRAM) {
                    if (VetState (XState)) {
                        XState = EFI_INVALID_PARAMETER;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Bad RAM End Address Entry < Start Address Entry'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Bad RAM End Address Entry < Start Address Entry'";
                        }
                        #endif
                    }

                    Status = EFI_INVALID_PARAMETER;
                    break; // Inner Loop 2A 1
                }


                // Convert physical addresses to page numbers to derive the total number of pages.
                TopBadRAM /=  EFI_PAGE_SIZE;                 // Convert Top Address to Page Number.
                EndBadRAM /=  EFI_PAGE_SIZE;                 // Convert End Address to Page Number.
                NumPages   = (EndBadRAM - TopBadRAM) + 1;    // Determine the Total Number of Pages.
                TopBadRAM *=  EFI_PAGE_SIZE;                 // Convert Page Number Back to Address.


                /** FOR INFO ONLY
                EndBadRAM += 1;             // Change Page to Address Step 1 ... Not used.
                EndBadRAM *= EFI_PAGE_SIZE; // Change Page to Address Step 2 ... Not used.
                EndBadRAM -= 1;             // Change Page to Address Step 3 ... Not used.
                **/


                if (TopBadRAM > MAX_PHYSICAL_ADDRESS) {
                    if (VetState (XState)) {
                        XState = EFI_BAD_BUFFER_SIZE;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Range Start Entry > MAX_PHYSICAL_ADDRESS'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Found Address Range Start Entry > MAX_PHYSICAL_ADDRESS'";
                        }

                        MsgErrorB = L"A start address was defined beyond firmware architecture limits";
                        #endif
                    }

                    Status = EFI_BAD_BUFFER_SIZE;
                    break; // Inner Loop 2A 1
                }

                if (EndBadRAM > MAX_MEMORY_PAGES) {
                    if (VetState (XState)) {
                        XState = EFI_BAD_BUFFER_SIZE;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Range End Entry > MAX_MEMORY_PAGES'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Found Address Range > MAX_MEMORY_PAGES'";
                        }

                        MsgErrorB = L"An end address was defined beyond total page size limits";
                        #endif
                    }

                    Status = EFI_BAD_BUFFER_SIZE;
                    break; // Inner Loop 2A 1
                }

                TempNumber = (
                    EndBadRAM != MAX_MEMORY_PAGES
                ) ? EndBadRAM + 1 : EndBadRAM;

                SizeLimit = (TempNumber * EFI_PAGE_SIZE) - 1;

                if (SizeLimit > MAX_PHYSICAL_ADDRESS) {
                    if (VetState (XState)) {
                        XState = EFI_BAD_BUFFER_SIZE;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Range End Entry > MAX_PHYSICAL_ADDRESS'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Found Address Range End Entry > MAX_PHYSICAL_ADDRESS'";
                        }

                        MsgErrorB = L"An end address was defined beyond firmware architecture limits";
                        #endif
                    }

                    Status = EFI_BAD_BUFFER_SIZE;
                    break; // Inner Loop 2A 1
                }

                SizeBound = (
                    OrigMode > 7
                ) ? MAX_BADRAM_PAGES_NORM : (
                    !OurWide
                ) ? MAX_BADRAM_PAGES_NORM : MAX_BADRAM_PAGES_FAST;

                if (NumPages > SizeBound) {
                    if (VetState (XState)) {
                        XState = EFI_BAD_BUFFER_SIZE;

                        #if REFIT_DEBUG > 0
                        if (OrigMode == 8 || OrigMode == 9) {
                            MsgErrorA = L"In Cached List:- 'Found Address Entry > 1.0GB Limit'";
                        }
                        else if (OrigMode > 1) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Entry > 1.0GB Limit'";
                        }
                        else { // OrigMode == 1
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Entry > 4.0GB Limit'";
                        }

                        MsgErrorB = L"Review the supplied address(es)";
                        #endif
                    }

                    Status = EFI_BAD_BUFFER_SIZE;
                    break; // Inner Loop 2A 1
                }

                SizeBytes = (UINT64) NumPages * EFI_PAGE_SIZE;
                if (SizeBytes > (MAX_PHYSICAL_ADDRESS - TopBadRAM)) {
                    if (VetState (XState)) {
                        XState = EFI_BAD_BUFFER_SIZE;

                        #if REFIT_DEBUG > 0
                        if (ManualMode) {
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Range > Max Top Address'";
                        }
                        else {
                            MsgErrorA = L"In Cached List:- 'Found Address Range > Max Top Address'";
                        }

                        MsgErrorB = L"Review the supplied address(es)";
                        #endif
                    }

                    Status = EFI_BAD_BUFFER_SIZE;
                    break; // Inner Loop 2A 1
                }

                #if REFIT_DEBUG > 0
                SizeBreach = FALSE;
                #endif

                Status = HandleRange (
                    TopBadRAM, NumPages,
                    OurMode, OurWide
                );
                if (!VetState (XState)) break; // Inner Loop 2A 1

                XState = Status;

                #if REFIT_DEBUG > 0
                if (EFI_ERROR(XState)) {
                    if (XState == EFI_OUT_OF_RESOURCES) {
                        MsgErrorA = (
                            MemoryErr != NULL
                        ) ? MemoryErr :  L"Bad RAM Regions:- 'Buffer Allocation Failure'";
                    }
                    else {
                        switch (XState) {
                            case EFI_NOT_FOUND:     MsgErrorA = L"Bad RAM Regions:- 'Bad RAM Tag Failure'"        ; break;
                            case EFI_LOAD_ERROR:    MsgErrorA = L"Bad RAM Regions:- 'UEFI Protocol Failure'"      ; break;
                            case EFI_DEVICE_ERROR:  MsgErrorA = L"Bad RAM Regions:- 'Apparent Memory Map Failure'"; break;
                            case EFI_ACCESS_DENIED: MsgErrorA = L"Bad RAM Regions:- 'All Bad RAM Regions in Use'" ; break;
                            default:                MsgErrorA = L"Bad RAM Regions:- 'Unsanitised Error Status'"   ; break;
                        } // switch

                        if (SizeBreach) {
                            // Delibrately Log 1.0GB (4.0GB was just some slack).
                            MsgErrorA = L"In 'badram_tag_list' Config:- 'Found Address Range Size > 1.0GB Limit'";
                            MsgErrorB = L"Review the supplied address(es)";
                        }
                    }
                }
                #endif
            } while (0); // This 'loop' only runs once - Inner Loop 2A 1

            OUR_MSG_L01((
                OUR_MSG_STR(
                    " ... Completed With Status:- '%r'\n"
                ), Status
            ));

            MY_FREE_POOL(AddressOne);
            MY_FREE_POOL(AddressDuo);
        } // while {Infinite} - Inner Loop 2A
    } while (0); // This 'loop' only runs once - Outer Loop 2

    // Set Status to 'EFI_OUT_OF_RESOURCES' If:
    // - The status is not 'EFI_OUT_OF_RESOURCES'.
    // - Memory allocation failure met at any point.
    if (XState != EFI_OUT_OF_RESOURCES && MemDrained) {
        XState  = EFI_OUT_OF_RESOURCES;

        #if REFIT_DEBUG > 0
        MsgErrorA = (
            MemoryErr != NULL
        ) ? MemoryErr :  L"Memory Allocation Failure (Not Tracked)";
        #endif
    }

    // If initial scan recorded an error in the cache,
    // return EFI_UNSUPPORTED if current run succeeded.
    // Only if regions were actually added to the cache.
    if (OrigMode == 8 || OrigMode == 9) {
        TempCheck = MyStrEnds (
            TAG_XXX, BadRamInfo
        );
        if (!TempCheck) {
            TempCheck = IsStriStr (
                BadRamInfo, TAG_ERR
            );
            if (TempCheck && VetState (XState)) {
                XState = EFI_UNSUPPORTED;

                #if REFIT_DEBUG > 0
                MsgErrorA = L"Fix Original Scan Error Then Use 'CleanNvram' Tool to Clear Cache";
                MsgErrorB = L"Alternatively, temporarily set the 'badram_tag_mode -1' token";
                #endif
            }
        }
    }

    // Set Status to 'Success' If:
    // - The Config Mode is 4/6/8.
    // - Status is 'Access Denied'.
    // - At Least One Target Tagged.
    if (GotSuccess &&
        XState == EFI_ACCESS_DENIED &&
        (OurMode == 4 || OurMode == 6 || OurMode == 8)
    ) {
        XState = EFI_SUCCESS;

        #if REFIT_DEBUG > 0
        MsgErrorA = NULL;
        MsgErrorB = NULL;
        #endif
    }

    // Handle Logging for 'Not Ready' Status
    #if REFIT_DEBUG > 0
    if (XState == EFI_NOT_READY) {
        MsgErrorA = L"Could Not Locate Applicable Bad RAM for Configured Mode";
        MsgErrorB = L"Review addresses, try another mode, or disable BadRamTag";
    }
    #endif

    #if REFIT_DEBUG > 0
    LOG_MSG("INFO: Tag Bad RAM Regions ... %r", XState);
    if (MsgErrorA != NULL && EFI_ERROR(XState)) {
        LOG_MSG("%s      %s", OffsetNext, MsgErrorA);

        if (MsgErrorB != NULL) {
            LOG_MSG("%s      %s", OffsetNext, MsgErrorB);
        }

        LOG_MSG("%s      * Current Settings:", OffsetNext);
        LOG_MSG("%s          - badram_tag_mode:- '%d'", OffsetNext, OrigMode);
        LOG_MSG("%s          - badram_tag_wide:- ", OffsetNext);
        if (OrigMode == 1) {
            LOG_MSG("'N/A'");
        }
        else {
            LOG_MSG("'%s'", (OurWide) ? L"Active" : L"Inactive");
        }

        LOG_MSG("%s          - badram_tag_list:- ", OffsetNext);
        if (OrigMode == 8 || OrigMode == 9) {
            LOG_MSG("'N/A'");
        }
        else {
            LOG_MSG("'%s'", (OurList !=  NULL) ? OurList : L"NULL");
        }
    } // if MsgErrorA != NULL
    LOG_MSG("\n\n");
    #endif

    if (ManualMode) {
        // Conditionally Drop Cache.
        if (OrigMode == -1) {
            DropCache = TRUE;
        }
        else if (OrigMode != 0) {
            DropCache = (
                EFI_ERROR(XState)
            ) ? FALSE : TRUE;
        }
        else { // OrigMode == 0
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            Status = EfivarGetRaw (
                &RefindPlusGuid, L"BadRamTag",
                (VOID **) &BadRamInfo, NULL
            );
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            if (EFI_ERROR(Status)) {
                DropCache = FALSE;
            }
            else {
                DropCache = MyStrEnds (
                    TAG_XXX, BadRamInfo
                );
            }
        } // if/else OrigMode == -1 etc

        if (DropCache) {
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            EfivarSetRaw (
                &RefindPlusGuid, L"BadRamTag",
                NULL, 0, TRUE
            );
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif
        }
    } // if OrigMode != 8 && != 9

    MY_FREE_POOL(BadRamInfo);
    if (CachedList == NULL) {
        MY_FREE_POOL(TheFixList);
    }
    else {
        MY_FREE_POOL(CachedList);
        TheFixList = NULL; // Avoid Dangling Pointer
    }

    return XState;
} // EFI_STATUS ManageBadRam()
