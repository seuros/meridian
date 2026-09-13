// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

//
// Weak mem* intrinsics, shared by every Meridian image.
//
// clang lowers struct copies and ZeroMem/CopyMem clears to libc calls even
// under -ffreestanding, and nothing in a UEFI image defines them. Each EFI
// binary is its own link unit -- Meridian, the SNP discoverer and each fsw
// filesystem driver link separately -- so this cannot be defined once and
// shared at link time; it is compiled into each image via MeridianIntrinsicsLib.
//
// Weak, so an image that already carries a strong definition wins: LuaLib's
// libc shim (Library/LuaLib/shim/mlua_libc.c) provides a strong set as part of
// a wider shim, and that one takes precedence wherever Lua is linked in.
//
// Mapped onto BaseMemoryLib rather than open-coded byte loops so each image
// gets the arch-optimised routines it already links.
//

#include <Base.h>
#include <Library/BaseMemoryLib.h>

__attribute__((weak)) void *memset(void *dest, int ch, __SIZE_TYPE__ count)
{
    SetMem(dest, (UINTN)count, (UINT8)ch);
    return dest;
}

__attribute__((weak)) void *memcpy(void *dest, const void *src, __SIZE_TYPE__ count)
{
    CopyMem(dest, (VOID *)src, (UINTN)count);
    return dest;
}

__attribute__((weak)) void *memmove(void *dest, const void *src, __SIZE_TYPE__ count)
{
    CopyMem(dest, (VOID *)src, (UINTN)count);
    return dest;
}

__attribute__((weak)) int memcmp(const void *a, const void *b, __SIZE_TYPE__ count)
{
    return (int)CompareMem(a, b, (UINTN)count);
}
