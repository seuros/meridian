// Line-editing functions borrowed from gummiboot (cursor_left(),
// cursor_right(), & line_edit()).

/*
 * Simple UEFI boot loader which executes configured EFI images, where the
 * default entry is selected by a configured pattern (glob) or an on-screen
 * menu.
 *
 * All gummiboot code is LGPL not GPL, to stay out of politics and to give
 * the freedom of copying code from programs to possible future libraries.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * Copyright (C) 2012-2013 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2012 Harald Hoyer <harald@redhat.com>
 *
 * "Any intelligent fool can make things bigger, more complex, and more violent.
"
 *   -- Albert Einstein
 */
/*
 * Modified for rEFInd
 * Copyright (c) 2021 Roderick W. Smith
 *
 * Modifications distributed under the preceding terms.
 */
/**
** Modified for RefindPlus
** Copyright (c) 2022-2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the preceding terms.
**/

#include "global.h"
#include "screenmgt.h"
#include "lib.h"
#include "../include/refit_call_wrapper.h"

static
VOID cursor_left (
    UINTN *cursor,
    UINTN *first
) {
    if ((*cursor) > 0) {
        (*cursor)--;

        return;
    }

    if ((*first) > 0) (*first)--;
} // static VOID cursor_left()

static
VOID cursor_right (
    UINTN *cursor,
    UINTN *first,
    UINTN  x_max,
    UINTN  len
) {
    if (((*cursor) + 2) < x_max) {
        (*cursor)++;

        return;
    }

    if ((*first) + (*cursor) < len) (*first)++;
} // static VOID cursor_right()

BOOLEAN line_edit (
    CHAR16  *line_in,
    CHAR16 **line_out,
    UINTN    x_max
) {
    CHAR16        *line;
    CHAR16        *print;
    CHAR16        *MsgStr;
    UINTN          len, i;
    UINTN          size;
    UINTN          first;
    UINTN          y_pos;
    UINTN          index;
    UINTN          x_tag;
    UINTN          cursor;
    UINTN          LumpSum;
    UINTN          DestSize;
    BOOLEAN        enter;
    BOOLEAN        exit;
    EFI_STATUS     err;
    EFI_INPUT_KEY  key;


    DrawScreenHeader (L"Line Editor");
    REFIT_CALL_3_WRAPPER(
        gST->ConOut->SetCursorPosition, gST->ConOut,
        (ConWidth - 71) / 2, ConHeight - 1
    );

    MsgStr = L"Use Cursor Keys to Edit, 'ESC' to Exit, 'Enter' to Boot with Edited Options";
    REFIT_CALL_2_WRAPPER(
        gST->ConOut->OutputString,
        gST->ConOut, MsgStr
    );

    if (line_in == NULL) {
        line_in = L"";
    }

    line = AllocateZeroPool (
        StrSize (line_in)
    );
    if (line == NULL) {
        // Early Return
        return FALSE;
    }

    DestSize = StrSize (line_in) / sizeof (CHAR16);
    StrCpyS (line, DestSize, line_in);

    print = AllocatePool (
        sizeof (CHAR16) * x_max
    );
    if (print == NULL) {
        MY_FREE_POOL(line);

        // Early Return
        return FALSE;
    }

    x_tag  = x_max - 2;
    y_pos  = 3;
    first  = 0;
    cursor = 0;
    enter  = FALSE;
    exit   = FALSE;
    len    = StrLen (line);
    size   = StrLen (line_in) + 1024; // DA-TAG: Investigate This ... Should it be '1'?

    REFIT_CALL_2_WRAPPER(
        gST->ConOut->EnableCursor,
        gST->ConOut, TRUE
    );

    while (!exit) {
        i = len - first;
        if (i >= x_tag) {
            i =  x_tag;
        }

        REFIT_CALL_3_WRAPPER(
            gBS->CopyMem, print,
            line + first, sizeof (CHAR16) * i
        );

        print[i++] = ' ';
        print[i]   = '\0';

        REFIT_CALL_3_WRAPPER(
            gST->ConOut->SetCursorPosition,
            gST->ConOut, 0, y_pos
        );
        REFIT_CALL_2_WRAPPER(
            gST->ConOut->OutputString,
            gST->ConOut, print
        );
        REFIT_CALL_3_WRAPPER(
            gST->ConOut->SetCursorPosition,
            gST->ConOut, cursor, y_pos
        );
        REFIT_CALL_3_WRAPPER(
            gBS->WaitForEvent, 1,
            &gST->ConIn->WaitForKey, &index
        );

        err = REFIT_CALL_2_WRAPPER(
            gST->ConIn->ReadKeyStroke,
            gST->ConIn, &key
        );
        if (EFI_ERROR(err)) continue;

        LumpSum = first + cursor;
        switch (key.ScanCode) {
            case SCAN_ESC: {
                exit = TRUE;

                break;
            }

            case SCAN_HOME: {
                cursor = 0;
                first  = 0;

                continue;
            }

            case SCAN_END:{
                cursor = len;
                if (cursor >= x_max) {
                    cursor =  x_tag;
                    first  =  len - cursor;
                }

                continue;
            }

            case SCAN_UP: {
                while (LumpSum && line[LumpSum] == ' ') cursor_left (&cursor, &first);
                while (LumpSum && line[LumpSum] != ' ') cursor_left (&cursor, &first);
                while (LumpSum && line[LumpSum] == ' ') cursor_left (&cursor, &first);

                if (LumpSum != len && LumpSum) {
                    cursor_right (&cursor, &first, x_max, len);
                }

                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition, gST->ConOut,
                    cursor, y_pos
                );

                continue;
            }

            case SCAN_DOWN: {
                while (line[LumpSum] && line[LumpSum] == ' ') cursor_right (&cursor, &first, x_max, len);
                while (line[LumpSum] && line[LumpSum] != ' ') cursor_right (&cursor, &first, x_max, len);
                while (line[LumpSum] && line[LumpSum] == ' ') cursor_right (&cursor, &first, x_max, len);

                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition,
                    gST->ConOut, cursor, y_pos
                );

                continue;
            }

            case SCAN_RIGHT: {
                if (LumpSum == len) continue;

                cursor_right (&cursor, &first, x_max, len);
                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition,
                    gST->ConOut, cursor, y_pos
                );

                continue;
            }

            case SCAN_LEFT: {
                cursor_left (&cursor, &first);
                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition,
                    gST->ConOut, cursor, y_pos
                );

                continue;
            }

            case SCAN_DELETE:{
                if (len == 0)       continue;
                if (len == LumpSum) continue;

                for (i = LumpSum; i < len; i++) {
                    line[i] = line[i + 1];
                }

                line[len - 1] = ' ';
                len--;

                continue;
            }
        } // switch

        LumpSum = first + cursor;
        switch (key.UnicodeChar) {
            case CHAR_LINEFEED:
            case CHAR_CARRIAGE_RETURN: {
                *line_out = line;
                line      = NULL;
                enter     = TRUE;
                exit      = TRUE;

                break;
            }

            case CHAR_BACKSPACE: {
                if (len == 0)                  continue;
                if (first == 0 && cursor == 0) continue;

                for (i = LumpSum - 1; i < len; i++) {
                    line[i] = line[i + 1];
                }
                len--;

                if (cursor > 0)               cursor--;
                if (cursor > 0 || first == 0) continue;

                /* Show full line if it fits */
                if (len < x_tag) {
                    cursor = first;
                    first = 0;

                    continue;
                }

                /* Jump left to see what we delete */
                if (first > 10) {
                    first -= 10;
                    cursor = 10;
                }
                else {
                    cursor = first;
                    first = 0;
                }

                continue;
            }

            case '\t':
            case ' ' ... '~':
            case 0x80 ... 0xffff: {
                if (size == (len + 1)) continue;

                for (i = len; i > LumpSum; i--) {
                    line[i] = line[i - 1];
                }

                line[LumpSum] = key.UnicodeChar;
                len++;
                line[len] = '\0';

                if ((cursor + 2) < x_max) {
                    cursor++;
                }
                else {
                    if (LumpSum < len) {
                        first++;
                    }
                }

                continue;
            }
        } // switch
    } // while
    REFIT_CALL_2_WRAPPER(
        gST->ConOut->EnableCursor,
        gST->ConOut, FALSE
    );

    MY_FREE_POOL(print);
    MY_FREE_POOL(line);

    return enter;
} // BOOLEAN line_edit()
