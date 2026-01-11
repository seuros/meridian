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


// ====================================================================
// Helper Function: Safe UTF-8 Character Decoding
// ====================================================================

/**
 * Reads a single Unicode codepoint (c) from a UTF-8 byte stream,
 * advancing the pointer (p) by 1 to 4 bytes.
 * NB: This function assumes well-formed UTF-8 input for simplicity
 * and maintains the original code's behavior regarding bounds checking.
 *
 * @param p A pointer to the current position in the fsw_u8 stream (pointer-to-pointer).
 * @return The decoded Unicode codepoint (fsw_u32).
**/
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

// ====================================================================
// Comparison Functions (Using Helper)
// ====================================================================

static
int fsw_streq_ISO88591_UTF8 (
    void *s1data,
    void *s2data,
    int   len
) {
    int     i;
    fsw_u8 *p1 = (fsw_u8 *) s1data;
    fsw_u8 *p2 = (fsw_u8 *) s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = *p1++;                  // Read 1-byte ISO88591
        c2 = decode_utf8_char (&p2); // Decode UTF-8 from p2
        if (c1 != c2) return 0;
    }
    return 1;
}

static
int fsw_streq_ISO88591_UTF16 (
    void *s1data,
    void *s2data,
    int   len
) {
    int     i;
    fsw_u8  *p1 = (fsw_u8  *) s1data;
    fsw_u16 *p2 = (fsw_u16 *) s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = *p1++;
        c2 = *p2++;
        if (c1 != c2) return 0;
    }
    return 1;
}

static
int fsw_streq_ISO88591_UTF16_SWAPPED (
    void *s1data,
    void *s2data,
    int   len
) {
    int     i;
    fsw_u8  *p1 = (fsw_u8  *) s1data;
    fsw_u16 *p2 = (fsw_u16 *) s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = *p1++;
        c2 = *p2++; c2 = FSW_SWAPVALUE_U16(c2);
        if (c1 != c2) return 0;
    }
    return 1;
}

static
int fsw_streq_UTF8_UTF16 (
    void *s1data,
    void *s2data,
    int   len
) {
    int     i;
    fsw_u8  *p1 = (fsw_u8  *) s1data;
    fsw_u16 *p2 = (fsw_u16 *) s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = decode_utf8_char (&p1); // Decode UTF-8 from p1
        c2 = *p2++;
        if (c1 != c2) return 0;
    }
    return 1;
}

static
int fsw_streq_UTF8_UTF16_SWAPPED (
    void *s1data,
    void *s2data,
    int   len
) {
    int     i;
    fsw_u8  *p1 = (fsw_u8  *) s1data;
    fsw_u16 *p2 = (fsw_u16 *) s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = decode_utf8_char (&p1); // Decode UTF-8 from p1
        c2 = *p2++; c2 = FSW_SWAPVALUE_U16(c2);
        if (c1 != c2) return 0;
    }
    return 1;
}

static
int fsw_streq_UTF16_UTF16_SWAPPED (
    void *s1data,
    void *s2data,
    int    len
) {
    int     i;
    fsw_u16 *p1 = (fsw_u16 *) s1data;
    fsw_u16 *p2 = (fsw_u16 *) s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = *p1++;
        c2 = *p2++; c2 = FSW_SWAPVALUE_U16(c2);
        if (c1 != c2) return 0;
    }
    return 1;
}


// ====================================================================
// Coercion Functions (Using Helper)
// ====================================================================

static
fsw_status_t fsw_strcoerce_UTF8_ISO88591 (
    void *srcdata,
    int srclen,
    struct fsw_string *dest
) {
    fsw_status_t   status;
    int            i;
    fsw_u8       *sp = (fsw_u8 *) srcdata;
    fsw_u8       *dp;
    fsw_u32        c;

    dest->type = FSW_STRING_TYPE_ISO88591;
    dest->len  = srclen;
    dest->size = srclen * sizeof (fsw_u8);

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    dp = (fsw_u8 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = decode_utf8_char (&sp); // Decode UTF-8 from sp
        *dp++ = (fsw_u8) c;
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_UTF16_ISO88591 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             i;
    fsw_u16       *sp = (fsw_u16 *) srcdata;
    fsw_u8        *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_ISO88591;
    dest->len  = srclen;
    dest->size = srclen * sizeof (fsw_u8);

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    dp = (fsw_u8 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++;
        *dp++ = (fsw_u8) c;
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_UTF16_SWAPPED_ISO88591 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             i;
    fsw_u16       *sp = (fsw_u16 *) srcdata;
    fsw_u8        *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_ISO88591;
    dest->len  = srclen;
    dest->size = srclen * sizeof (fsw_u8);

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    dp = (fsw_u8 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++; c = FSW_SWAPVALUE_U16(c);
        *dp++ = (fsw_u8) c;
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_ISO88591_UTF16 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             i;
    fsw_u8        *sp = (fsw_u8 *) srcdata;
    fsw_u16       *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_UTF16;
    dest->len  = srclen;
    dest->size = srclen * sizeof (fsw_u16);

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    dp = (fsw_u16 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++;
        *dp++ = (fsw_u16) c;
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_UTF8_UTF16 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             i;
    fsw_u8        *sp = (fsw_u8 *) srcdata;
    fsw_u16       *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_UTF16;
    dest->len  = srclen;
    dest->size = srclen * sizeof (fsw_u16);

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    dp = (fsw_u16 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = decode_utf8_char (&sp); // Decode UTF-8 from sp
        *dp++ = (fsw_u16) c;
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_UTF16_SWAPPED_UTF16 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             i;
    fsw_u16       *sp = (fsw_u16 *) srcdata;
    fsw_u16       *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_UTF16;
    dest->len  = srclen;
    dest->size = srclen * sizeof (fsw_u16);

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    dp = (fsw_u16 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++; c = FSW_SWAPVALUE_U16(c);
        *dp++ = (fsw_u16) c;
    }

    return FSW_SUCCESS;
}

//
// UTF-8 Destination Coercion Functions
//

static
fsw_status_t fsw_strcoerce_ISO88591_UTF8 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t   status;
    int            destsize;
    int            i;
    fsw_u8       *sp = (fsw_u8 *) srcdata;
    fsw_u8       *dp;
    fsw_u32        c;

    // Sizing Pass
    destsize = 0;
    fsw_u8 *sp_sizing = (fsw_u8 *) srcdata; // Dedicated pointer for sizing
    for (i = 0; i < srclen; i++) {
        c = *sp_sizing++; // Read 1 byte

        if (0);
        else if (c < 0x000080) destsize++;
        else if (c < 0x000800) destsize += 2;
        else if (c < 0x010000) destsize += 3;
        else                   destsize += 4;
    }

    dest->type = FSW_STRING_TYPE_UTF8;
    dest->len  = srclen;
    dest->size = destsize;

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    // Coercion Pass
    dp = (fsw_u8 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++; // Read 1 byte

        if (c < 0x000080) {
            *dp++ = (fsw_u8) c;
        }
        else if (c < 0x000800) {
            *dp++ = (fsw_u8)(0xc0 | ((c >> 6) & 0x1f));
            *dp++ = (fsw_u8)(0x80 | ( c       & 0x3f));
        }
        else if (c < 0x010000) {
            *dp++ = (fsw_u8)(0xe0 | ((c >> 12) & 0x0f));
            *dp++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ( c        & 0x3f));
        }
        else {
            *dp++ = (fsw_u8)(0xf0 | ((c >> 18) & 0x07));
            *dp++ = (fsw_u8)(0x80 | ((c >> 12) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ( c        & 0x3f));
        }
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_UTF16_UTF8 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             destsize;
    int             i;
    fsw_u16       *sp = (fsw_u16 *) srcdata;
    fsw_u8        *dp;
    fsw_u32         c;

    // Sizing Pass
    fsw_u16 *sp_sizing = (fsw_u16 *) srcdata; // Dedicated pointer for sizing
    destsize = 0;
    for (i = 0; i < srclen; i++) {
        c = *sp_sizing++; // Read 2 bytes

        if (0);
        else if (c < 0x000080) destsize++;
        else if (c < 0x000800) destsize += 2;
        else if (c < 0x010000) destsize += 3;
        else                   destsize += 4;
    }

    dest->type = FSW_STRING_TYPE_UTF8;
    dest->len  = srclen;
    dest->size = destsize;

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    // Coercion Pass
    dp = (fsw_u8 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++;

        if (c < 0x000080) {
            *dp++ = (fsw_u8) c;
        }
        else if (c < 0x000800) {
            *dp++ = (fsw_u8)(0xc0 | ((c >> 6) & 0x1f));
            *dp++ = (fsw_u8)(0x80 | ( c       & 0x3f));
        }
        else if (c < 0x010000) {
            *dp++ = (fsw_u8)(0xe0 | ((c >> 12) & 0x0f));
            *dp++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ( c        & 0x3f));
        }
        else {
            *dp++ = (fsw_u8)(0xf0 | ((c >> 18) & 0x07));
            *dp++ = (fsw_u8)(0x80 | ((c >> 12) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ( c        & 0x3f));
        }
    }
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_strcoerce_UTF16_SWAPPED_UTF8 (
    void              *srcdata,
    int                srclen,
    struct fsw_string *dest
) {
    fsw_status_t    status;
    int             destsize;
    int             i;
    fsw_u16       *sp = (fsw_u16 *) srcdata;
    fsw_u8        *dp;
    fsw_u32         c;

    // Sizing Pass
    fsw_u16 *sp_sizing = (fsw_u16 *) srcdata; // Dedicated pointer for sizing
    destsize = 0;
    for (i = 0; i < srclen; i++) {
        c = *sp_sizing++;
        c = FSW_SWAPVALUE_U16(c);

        if (0);
        else if (c < 0x000080) destsize++;
        else if (c < 0x000800) destsize += 2;
        else if (c < 0x010000) destsize += 3;
        else                   destsize += 4;
    }

    dest->type = FSW_STRING_TYPE_UTF8;
    dest->len  = srclen;
    dest->size = destsize;

    status = FSW_DO_ALLOC(dest->size, &dest->data);
    if (status) return status;

    // Coercion Pass
    dp = (fsw_u8 *) dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++; c = FSW_SWAPVALUE_U16(c);

        if (c < 0x000080) {
            *dp++ = (fsw_u8) c;
        }
        else if (c < 0x000800) {
            *dp++ = (fsw_u8)(0xc0 | ((c >> 6) & 0x1f));
            *dp++ = (fsw_u8)(0x80 | ( c       & 0x3f));
        }
        else if (c < 0x010000) {
            *dp++ = (fsw_u8)(0xe0 | ((c >> 12) & 0x0f));
            *dp++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ( c        & 0x3f));
        }
        else {
            *dp++ = (fsw_u8)(0xf0 | ((c >> 18) & 0x07));
            *dp++ = (fsw_u8)(0x80 | ((c >> 12) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ((c >> 6 ) & 0x3f));
            *dp++ = (fsw_u8)(0x80 | ( c        & 0x3f));
        }
    }
    return FSW_SUCCESS;
}
