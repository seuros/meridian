/* Misc string encoding functions */

/**
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/
/**
** Refactored for RefindPlus
** Copyright (c) 2025-2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the MIT License.
**/


// Internal Routing Constants
#define FSW_ENC_88591_NORM   0
#define FSW_ENC_UTF08_NORM   8
#define FSW_ENC_UTF16_NORM  16
#define FSW_ENC_UTF16_SWAP 161



// ====================================================================
// Internal Helpers
// ====================================================================

/* Reads a Single Unicode Codepoint from a UTF-8 Byte Stream */
static
fsw_u32 decode_utf8_char (
    fsw_u8 **p
) {
    fsw_u32 c = *(*p)++; // Correctly set for 1-byte sequences (0xxxxxxx)

    // Check for 2-byte sequence (110xxxxx)
    if ((c & 0xe0) == 0xc0) {
        c = ((c & 0x1f) << 6) | (*(*p)++ & 0x3f);
    }
    // Check for 3-byte sequence (1110xxxx)
    else if ((c & 0xf0) == 0xe0) {
        c = ((c & 0x0f) << 12) | ((*(*p)++ & 0x3f) << 6);
        c |= (*(*p)++ & 0x3f);
    }
    else {
        // Check for 4-byte sequence (11110xxx)
        if ((c & 0xf8) == 0xf0) {
            c = ((c & 0x07) << 18) | ((*(*p)++ & 0x3f) << 12);
            c |= ((*(*p)++ & 0x3f) << 6);
            c |= ( *(*p)++ & 0x3f);
        }
    }

    return c;
}

/* Universal Fetcher: Extracts Codepoint and Advances Source Pointer */
static
fsw_u32 fsw_fetch_codepoint (
    void **p,
    int type
) {
    fsw_u32 c;


    if (type == FSW_ENC_UTF08_NORM) {
        return decode_utf8_char (
            (fsw_u8 **)p
        );
    }

    if (type == FSW_ENC_UTF16_NORM ||
        type == FSW_ENC_UTF16_SWAP
    ) {
        c = **((fsw_u16 **)p);
        (*((fsw_u16 **)p))++;

        if (type == FSW_ENC_UTF16_NORM) {
            return c;
        }

        return FSW_SWAPVALUE_U16(c);
    }

    // Default: ISO-8859-1
    c = **((fsw_u8 **)p);
    (*((fsw_u8 **)p))++;

    return c;
}

/* UTF-8 Encoder Helpers */
static
int fsw_utf8_size (
    fsw_u32 c
) {
    if (c < 0x80   ) return 1;
    if (c < 0x800  ) return 2;
    if (c < 0x10000) return 3;

    return 4;
}

static
void fsw_write_utf8_char (
    fsw_u8  **dp,
    fsw_u32   c
) {
    if (c < 0x80) {
        *(*dp)++ = (fsw_u8)c;
    }
    else if (c < 0x800) {
        *(*dp)++ = (fsw_u8)(0xc0 | ((c >> 6 ) & 0x1f));
        *(*dp)++ = (fsw_u8)(0x80 | ( c        & 0x3f));
    }
    else if (c < 0x10000) {
        *(*dp)++ = (fsw_u8)(0xe0 | ((c >> 12) & 0x0f));
        *(*dp)++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
        *(*dp)++ = (fsw_u8)(0x80 | ( c        & 0x3f));
    }
    else {
        *(*dp)++ = (fsw_u8)(0xf0 | ((c >> 18) & 0x07));
        *(*dp)++ = (fsw_u8)(0x80 | ((c >> 12) & 0x3f));
        *(*dp)++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
        *(*dp)++ = (fsw_u8)(0x80 | ( c        & 0x3f));
    }
}



// =================================================
// Unified Comparison Handler (Ex 'fsw_streq_*')
// =================================================

static
int fsw_streq_internal (
    void *s1data,
    void *s2data,
    int   len,
    int   t1,
    int   t2
) {
    void *p1 = s1data;
    void *p2 = s2data;


    for (int i = 0; i < len; i++) {
        if (fsw_fetch_codepoint (&p1, t1) !=
            fsw_fetch_codepoint (&p2, t2)
        ) {
            return 0;
        }
    }

    return 1;
}

// API Interface:- Comparison
static int fsw_streq_UTF08_UTF16         (void *s1, void *s2, int l) { return fsw_streq_internal (s1, s2, l, FSW_ENC_UTF08_NORM, FSW_ENC_UTF16_NORM); }
static int fsw_streq_ISO88591_UTF16      (void *s1, void *s2, int l) { return fsw_streq_internal (s1, s2, l, FSW_ENC_88591_NORM, FSW_ENC_UTF16_NORM); }
static int fsw_streq_ISO88591_UTF08      (void *s1, void *s2, int l) { return fsw_streq_internal (s1, s2, l, FSW_ENC_88591_NORM, FSW_ENC_UTF08_NORM); }
static int fsw_streq_ISO88591_UTF16_SWAP (void *s1, void *s2, int l) { return fsw_streq_internal (s1, s2, l, FSW_ENC_88591_NORM, FSW_ENC_UTF16_SWAP); }
static int fsw_streq_UTF16_UTF16_SWAP    (void *s1, void *s2, int l) { return fsw_streq_internal (s1, s2, l, FSW_ENC_UTF16_NORM, FSW_ENC_UTF16_SWAP); }
static int fsw_streq_UTF08_UTF16_SWAP    (void *s1, void *s2, int l) { return fsw_streq_internal (s1, s2, l, FSW_ENC_UTF08_NORM, FSW_ENC_UTF16_SWAP); }



// =================================================
// Unified Coercion Handler (Ex 'fsw_strcoerce_*')
// =================================================

static
fsw_status_t fsw_strcoerce_internal (
    void              *src,
    int                slen,
    struct fsw_string *dest,
    int                stype,
    int                dtype
) {
    fsw_status_t status;
    void *sp = src;

    dest->len = slen;

    if (dtype == FSW_ENC_UTF08_NORM) {
        int dsize = 0;
        for (int i = 0; i < slen; i++) {
            dsize += fsw_utf8_size (
                fsw_fetch_codepoint (
                    &sp, stype
                )
            );
        }

        dest->type = FSW_STRING_TYPE_UTF08;
        dest->size = dsize;

        status = FSW_DO_ALLOC(dest->size, &dest->data);
        if (status) return status;

        sp = src; fsw_u8 *dp = (fsw_u8 *)dest->data;
        for (int i = 0; i < slen; i++) {
            fsw_write_utf8_char (
                &dp, fsw_fetch_codepoint (
                    &sp, stype
                )
            );
        }
    }
    else if (dtype == FSW_ENC_UTF16_NORM) {
        dest->type = FSW_STRING_TYPE_UTF16;
        dest->size = slen * sizeof (fsw_u16);

        status = FSW_DO_ALLOC(dest->size, &dest->data);
        if (status) return status;

        fsw_u16 *dp = (fsw_u16 *)dest->data;
        for (int i = 0; i < slen; i++) {
            *dp++ = (fsw_u16)fsw_fetch_codepoint (
                &sp, stype
            );
        }
    }
    else { // ISO-8859-1
        dest->type = FSW_STRING_TYPE_ISO88591;
        dest->size = slen;

        status = FSW_DO_ALLOC(dest->size, &dest->data);
        if (status) return status;

        fsw_u8 *dp = (fsw_u8 *)dest->data;
        for (int i = 0; i < slen; i++) {
            *dp++ = (fsw_u8)fsw_fetch_codepoint (
                &sp, stype
            );
        }
    }

    return FSW_SUCCESS;
}

// API Interface:- Coercion
static fsw_status_t fsw_strcoerce_ISO88591_UTF16      (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_88591_NORM, FSW_ENC_UTF16_NORM); }
static fsw_status_t fsw_strcoerce_ISO88591_UTF08      (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_88591_NORM, FSW_ENC_UTF08_NORM); }
static fsw_status_t fsw_strcoerce_UTF08_ISO88591      (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF08_NORM, FSW_ENC_88591_NORM); }
static fsw_status_t fsw_strcoerce_UTF08_UTF16         (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF08_NORM, FSW_ENC_UTF16_NORM); }
static fsw_status_t fsw_strcoerce_UTF16_UTF08         (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF16_NORM, FSW_ENC_UTF08_NORM); }
static fsw_status_t fsw_strcoerce_UTF16_ISO88591      (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF16_NORM, FSW_ENC_88591_NORM); }
static fsw_status_t fsw_strcoerce_UTF16_SWAP_UTF08    (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF16_SWAP, FSW_ENC_UTF08_NORM); }
static fsw_status_t fsw_strcoerce_UTF16_SWAP_UTF16    (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF16_SWAP, FSW_ENC_UTF16_NORM); }
static fsw_status_t fsw_strcoerce_UTF16_SWAP_ISO88591 (void *s, int l, struct fsw_string *d) { return fsw_strcoerce_internal (s, l, d, FSW_ENC_UTF16_SWAP, FSW_ENC_88591_NORM); }
