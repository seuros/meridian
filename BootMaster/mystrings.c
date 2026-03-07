/*
 * BootMaster/mystrings.c
 * String-manipulation functions
 *
 * Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), or (at your option) any later version.
 */
/**
** Modified for RefindPlus
** Copyright (c) 2020-2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the preceding terms.
**/

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"
#include "../include/refit_call_wrapper.h"

#define MAX_WORD_LEN    (64)

BOOLEAN NestedStrStr = FALSE;


/*
 * Routine Description:
 *
 *  Confirms FirstString is shorter than or equal to SecondString.
 *
 * Arguments:
 *
 *  String1  - Null-terminated string to check length of.
 *  String2  - Null-terminated string to check against.
 *
 * Returns:
 *  False if String1 is longer than String2 or
 *  True if String1 is shorter than or equal to String2.
 */
static
BOOLEAN IsValidStrComp (
    IN CHAR16  *String1,
    IN CHAR16  *String2
) {
    UINTN  Len1;
    UINTN  Len2;

    if (String1 == NULL ||
        String2 == NULL
    ) {
        return FALSE;
    }

    Len1 = StrLen (String1);
    Len2 = StrLen (String2);

    if (Len1 > Len2) {
        // String1 is longer than String2
        return FALSE;
    }

    // String1 is shorter than or equal to String2
    return TRUE;
} // static BOOLEAN IsValidStrComp()

/*
 * Routine Description:
 *
 *  Starts each word in string with uppercase character.
 *
 * Arguments:
 *
 *  InputString  - Null-terminated string to process.
 *  SpecialCases - Change output for 'rEFIt-linux' files.
 *
 * Returns:
 *  Converted string
 */
CHAR16 * CapitalisedCase (
    IN CHAR16  *InputString,
    IN BOOLEAN  SpecialCases
) {
    UINTN      WordLen, i;
    UINTN      IndexIn;
    UINTN      IndexOut;
    CHAR16     StringChar;
    CHAR16    *ResultString;
    CHAR16     WordBuffer[MAX_WORD_LEN];
    BOOLEAN    HandleCase;


    if (InputString == NULL) {
        return NULL;
    }

    if (!SpecialCases) {
        // DA_TAG: Standard buffer size
        ResultString = AllocatePool (
            StrSize (InputString)
        );
    }
    else {
        // DA_TAG: Increased buffer size
        ResultString = AllocatePool (
            StrSize (InputString) * 2
        );
    }
    if (ResultString == NULL) {
        return NULL;
    }

    IndexIn  = 0;
    IndexOut = 0;

    while (1) {
        StringChar = InputString[IndexIn];
        if (StringChar == L'\0') {
            break;
        }

        // Handle word characters
        if ((
                (StringChar < L'a') ||
                (StringChar > L'z')
            ) && (
                (StringChar < L'A') ||
                (StringChar > L'Z')
            )
        ) {
            if (StringChar != L'-') {
                // Whitespace/Other: Copy as-is
                ResultString[IndexOut] = StringChar;
            }
            else {
                if (InputString[IndexIn + 1] == L'\0') {
                    ResultString[IndexOut] = StringChar;
                }
                else {
                    if ((
                            (InputString[IndexIn + 1] < L'a') ||
                            (InputString[IndexIn + 1] > L'z')
                        ) && (
                            (InputString[IndexIn + 1] < L'A') ||
                            (InputString[IndexIn + 1] > L'Z')
                        )
                    ) {
                        // Whitespace/Other: Copy as-is
                        ResultString[IndexOut] = StringChar;
                    }
                    else {
                        if (IndexIn == 0) {
                            // Whitespace/Other: Copy as-is
                            ResultString[IndexOut] = StringChar;
                        }
                        else {
                            if ((
                                    (InputString[IndexIn - 1] < L'a') ||
                                    (InputString[IndexIn - 1] > L'z')
                                ) && (
                                    (InputString[IndexIn - 1] < L'A') ||
                                    (InputString[IndexIn - 1] > L'Z')
                                )
                            ) {
                                // Whitespace/Other: Copy as-is
                                ResultString[IndexOut] = StringChar;
                            }
                            else {
                                // Hyphen: Replace with space
                                ResultString[IndexOut] = L' ';
                            }
                        }
                    }
                }
            }

            IndexIn  += 1;
            IndexOut += 1;

            continue;
        } // if StringChar

        // Start collecting a word
        WordLen = 0;
        while (
            (
                (StringChar >= L'a') &&
                (StringChar <= L'z')
            ) || (
                (StringChar >= L'A') &&
                (StringChar <= L'Z')
            )
        ) {
            if (WordLen < (MAX_WORD_LEN - 1)) {
                WordBuffer[WordLen] = StringChar;

                WordLen += 1;
            }
            IndexIn += 1;

            StringChar = InputString[IndexIn];
        } // while
        if (WordLen == 0) continue;

        WordBuffer[WordLen] = L'\0';

        if (!SpecialCases) {
            HandleCase = TRUE;
        }
        else {
            HandleCase = FALSE;

            // Handle "to" and "with" replacements
            if (MyStriCmp (
                    WordBuffer,
                    L"to"
                )
            ) {
                // Replace "to" with "into"
                ResultString[IndexOut++] = L'i';
                ResultString[IndexOut++] = L'n';
                ResultString[IndexOut++] = L't';
                ResultString[IndexOut++] = L'o';
            }
            else if (
                MyStriCmp (
                    WordBuffer,
                    L"with"
                )
            ) {
                // Force "with" to lowercase
                ResultString[IndexOut++] = L'w';
                ResultString[IndexOut++] = L'i';
                ResultString[IndexOut++] = L't';
                ResultString[IndexOut++] = L'h';
            }
            else if (
                MyStriCmp (
                    WordBuffer,
                    L"into"
                )
            ) {
                // Force "into" to lowercase
                ResultString[IndexOut++] = L'i';
                ResultString[IndexOut++] = L'n';
                ResultString[IndexOut++] = L't';
                ResultString[IndexOut++] = L'o';
            }
            else if (
                MyStriCmp (
                    WordBuffer,
                    L"standard"
                ) || MyStriCmp (
                    WordBuffer,
                    L"normal"
                )
            ) {
                // Replace "standard" with "Default"
                ResultString[IndexOut++] = L'D';
                ResultString[IndexOut++] = L'e';
                ResultString[IndexOut++] = L'f';
                ResultString[IndexOut++] = L'a';
                ResultString[IndexOut++] = L'u';
                ResultString[IndexOut++] = L'l';
                ResultString[IndexOut++] = L't';
            }
            else {
                HandleCase = TRUE;
            }
        } // if !SpecialCases
        if (!HandleCase) continue;

        // Capitalise first letter
        if (WordBuffer[0] >= L'a' &&
            WordBuffer[0] <= L'z'
        ) {
            ResultString[IndexOut++] = WordBuffer[0] - L'a' + L'A';
        }
        else {
            ResultString[IndexOut++] = WordBuffer[0];
        }

        // Keep rest as original
        for (i = 1; i < WordLen; i++) {
            ResultString[IndexOut++] = WordBuffer[i];
        } // for
    } // while {Infinite}

    // Null-terminate
    ResultString[IndexOut] = L'\0';

    if (SpecialCases) {
        ReplaceSubstring (
            &ResultString,
            L"Single User", L"SingleUser"
        );
    }

    return ResultString;
} // CHAR16 * CapitalisedCase()

/*
 * Routine Description:
 *
 *  Return the substring after a supplied delimiter.
 *
 * Arguments:
 *
 *  Delimiter  - Null-terminated string to search for as delimiter.
 *  String     - Null-terminated string to search for a Substring.
 *
 * Returns:
 *  The address of the matching substring after the delimiter
 *  or the original string if the delimiter was not found.
 */
CHAR16 * GetSubStrAfter (
    IN CHAR16 *InputDelimiter,
    IN CHAR16 *String
) {
    CHAR16 *Substring;
    CHAR16 *Delimiter;


    if (String == NULL) {
        return NULL;
    }

    // Handle Deprecated 'INITIAL_STRING_DELIM' = L" @@ "
    if (!MyStriCmp (InputDelimiter, DEFAULT_STRING_DELIM)) {
        Delimiter = InputDelimiter;
    }
    else if (MyStrStr (String, INITIAL_STRING_DELIM)) {
        Delimiter = INITIAL_STRING_DELIM;
    }
    else {
        Delimiter = DEFAULT_STRING_DELIM;
    }

    Substring = MyStrStr (String, Delimiter);
    if (Substring == NULL) {
        // Return original string ... Delimiter not found
        return String;
    }

    // Move past delimiter
    Substring += StrLen (Delimiter);
    if (*Substring == L'\0') {
        // Return original string ... Delimiter is at end
        return String;
    }

    return Substring;
} // CHAR16 * GetSubStrAfter()

/*
 * Routine Description:
 *
 *  Return the substring before a supplied delimiter.
 *  The calling function must free any memory allocated.
 *
 * Arguments:
 *
 *  Delimiter  - Null-terminated string to search for as delimiter.
 *  String     - Null-terminated string to search for a Substring.
 *
 * Returns:
 *  The address of the matching substring before the delimiter
 *  or the original string if the delimiter was not found.
 */
CHAR16 * GetSubStrBefore (
    IN CHAR16 *InputDelimiter,
    IN CHAR16 *String
) {
    UINTN   Length;
    CHAR16 *Result;
    CHAR16 *Substring;
    CHAR16 *Delimiter;


    if (String == NULL) {
        return NULL;
    }

    // Handle Deprecated 'INITIAL_STRING_DELIM' = L" @@ "
    if (!MyStriCmp (InputDelimiter, DEFAULT_STRING_DELIM)) {
        Delimiter = InputDelimiter;
    }
    else if (MyStrStr (String, INITIAL_STRING_DELIM)) {
        Delimiter = INITIAL_STRING_DELIM;
    }
    else {
        Delimiter = DEFAULT_STRING_DELIM;
    }

    Substring = MyStrStr (String, Delimiter);
    if (Substring == NULL) {
        // Return original string ... Delimiter not found
        return String;
    }

    if (MyStriCmp (Substring, String)) {
        // Return original string ... Delimiter is at start
        return String;
    }

    Length = StrLen (String) - StrLen (Substring);
    Result = AllocateZeroPool ((Length + 1) * sizeof (CHAR16));
    if (Result == NULL) {
        // Return original string ... Memory exhausted
        return String;
    }

    REFIT_CALL_3_WRAPPER(
        gBS->CopyMem, Result,
        String, sizeof (CHAR16) * Length
    );
    Result[Length] = L'\0'; // Null-terminate result

    return Result;
} // CHAR16 * GetSubStrBefore()

// DestSize is size in CHAR16s (including null terminator)
EFI_STATUS SafeStrCat (
    OUT       CHAR16 *Dest,
    IN        UINTN   DestSize,
    IN  CONST CHAR16 *Src
) {
    EFI_STATUS    Status;
    UINTN              i;
    BOOLEAN    FoundNull;

    if (Dest     == NULL ||
        Src      == NULL ||
        DestSize == 0
    ) {
        return EFI_INVALID_PARAMETER;
    }

    // Check that destination is null-terminated
    FoundNull = FALSE;
    for (i = 0; i < DestSize; i++) {
        if (Dest[i] == L'\0') {
            FoundNull = TRUE;

            break;
        }
    }

    if (!FoundNull) {
        // Force null-termination
        Dest[DestSize - 1] = L'\0';
    }

    Status = StrnCatS (
        Dest, DestSize,
        Src, StrLen (Src)
    );
    return Status;
} // EFI_STATUS SafeStrCat()

// Performs a case-insensitive string comparison.
// This function is needed because some StriCmp()
// implementations are atually case-sensitive.
// Returns TRUE if strings are identical or FALSE.
BOOLEAN MyStriCmp (
    IN CHAR16 *String1,
    IN CHAR16 *String2
) {
    CHAR16 c1;
    CHAR16 c2;


    if (String1 == NULL ||
        String2 == NULL
    ) {
        return FALSE;
    }

    while (*String1 && *String2) {
        c1 = *String1;
        c2 = *String2;

        if (c1 >= L'A' && c1 <= L'Z') c1 += 32;
        if (c2 >= L'A' && c2 <= L'Z') c2 += 32;
        if (c1 != c2) return FALSE;

        String1++;
        String2++;
    } // while

    return (*String1 == *String2);
} // BOOLEAN MyStriCmp()

// Checks whether String2 starts with String1
// Returns TRUE on match or FALSE.
BOOLEAN MyStrBegins (
    IN CHAR16 *String1,
    IN CHAR16 *String2
) {
    UINTN        i;
    UINTN     Len1;
    BOOLEAN IsGood;


    // String1 cannot be longer than String2.
    // In addition, neither can be NULL.
    IsGood = IsValidStrComp (String1, String2);
    if (!IsGood) {
        return FALSE;
    }

    Len1 = StrLen (String1);

    // Compare from the start of each string
    // 'IsGood' is curently 'TRUE'
    for (i = 0; i < Len1; i++) {
        if ((CHAR16)(String1[i] | 0x20) !=
            (CHAR16)(String2[i] | 0x20)
        ) {
            // Exit ... Mismatch found
            IsGood = FALSE;

            break;
        }
    }

    return IsGood;
} // BOOLEAN MyStrBegins()

// Checks whether String2 ends with String1
// Returns TRUE on match or FALSE.
BOOLEAN MyStrEnds (
    IN CHAR16 *String1,
    IN CHAR16 *String2
) {
    UINTN        i;
    UINTN     Len1;
    UINTN     Len2;
    BOOLEAN IsGood;


    // String1 cannot be longer than String2.
    // In addition, neither can be NULL.
    IsGood = IsValidStrComp (String1, String2);
    if (!IsGood) {
        return FALSE;
    }

    Len1 = StrLen (String1);
    Len2 = StrLen (String2);

    // Compare from the end of each string
    // 'IsGood' is curently 'TRUE'
    for (i = 0; i < Len1; i++) {
        if ((String1[Len1 - 1 - i] | 0x20) !=
            (String2[Len2 - 1 - i] | 0x20)
        ) {
            // Exit ... Mismatch found
            IsGood = FALSE;

            break;
        }
    }

    return IsGood;
} // BOOLEAN MyStrEnds()

/*
 * Routine Description:
 *
 *  Find a substring (Case Sensitive).
 *
 * Arguments:
 *
 *  String      - Null-terminated string to search.
 *  StrCharSet  - Null-terminated string to search for.
 *
 * Returns:
 *  Address of first occurrence of the matching substring or NULL.
 */
CHAR16 * MyStrStr (
    IN CHAR16  *String,
    IN CHAR16  *StrCharSet
) {
    CHAR16 *Src;
    CHAR16 *Sub;


    if (!NestedStrStr) LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- Find '%s' in '%s'", __func__,
        (StrCharSet != NULL) ? StrCharSet : L"NULL",
        (String     != NULL) ? String     : L"NULL"
    );
    if (String == NULL || StrCharSet == NULL) {
        BREAD_CRUMB(L"%a:  return 'NULL'", __func__);
        LOG_DECREMENT();
        if (!NestedStrStr) LOG_SEP(L"X");
        return NULL;
    }

    //BREAD_CRUMB(L"%a:  2", __func__);
    Src = String;
    Sub = StrCharSet;

    //BREAD_CRUMB(L"%a:  3 - WHILE LOOP:- START", __func__);
    while ((*String != L'\0') && (*StrCharSet != L'\0')) {
        if (*String++ == *StrCharSet) {
            StrCharSet++;
        }
        else {
            String     = ++Src;
            StrCharSet = Sub;
        }
    } // while
    //BREAD_CRUMB(L"%a:  4 - WHILE LOOP:- END", __func__);

    if (*StrCharSet == L'\0') {
        BREAD_CRUMB(L"%a:  4a - END:- return CHAR16 *Src (Substring Found)", __func__);
        LOG_DECREMENT();
        if (!NestedStrStr) LOG_SEP(L"X");
        return Src;
    }

    BREAD_CRUMB(L"%a:  5 - END:- return NULL (Substring *NOT* Found)", __func__);
    LOG_DECREMENT();
    if (!NestedStrStr) LOG_SEP(L"X");

    return NULL;
} // CHAR16 * MyStrStr()

/*
 * Routine Description:
 *
 *  As 'MyStrStr' but case insensitive and returns a BOOLEAN.
 *
 * Arguments:
 *
 *  BigStr    - Null-terminated string to search.
 *  SmallStr  - Null-terminated string to search for.
 *
 * Returns:
 *  TRUE if successful or FALSE.
 */
BOOLEAN IsStriStr (
    IN CHAR16 *BigStr,
    IN CHAR16 *SmallStr
) {
    UINTN   BigStart;
    UINTN   BigIndex;
    UINTN   SmallIndex;
    BOOLEAN Terminate;
    BOOLEAN Found;


    if (SmallStr == NULL || BigStr == NULL) {
        return FALSE;
    }

    Found = Terminate = FALSE;
    BigIndex = SmallIndex = BigStart = 0;
    while (!Terminate) {
        if (BigStr[BigIndex] == '\0') {
            Terminate = TRUE;
        }

        if (SmallStr[SmallIndex] == '\0') {
            Found     = TRUE;
            Terminate = TRUE;
        }

        if ((SmallStr[SmallIndex] | 0x20) == (BigStr[BigIndex] | 0x20)) {
            SmallIndex++;
            BigIndex++;
        }
        else {
            SmallIndex = 0;
            BigStart++;
            BigIndex = BigStart;
        }
    } // while

    return Found;
} // BOOLEAN IsStriStr()

/*
 * Routine Description:
 *
 *  As 'MyStrStr' but case insensitive and returns a BOOLEAN.
 *  For debugging ... Duplicates 'IsStriStr'
 *  Remove later
 *
 * Arguments:
 *
 *  RawString      - Null-terminated string to search.
 *  RawStrCharSet  - Null-terminated string to search for.
 *
 * Returns:
 *  TRUE if successful or FALSE.
 */
BOOLEAN FindSubStr (
    IN CHAR16  *RawString,
    IN CHAR16  *RawStrCharSet
) {
    BOOLEAN  FoundStr;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- Find '%s' in '%s'", __func__,
        (RawStrCharSet != NULL) ? RawStrCharSet : L"NULL",
        (RawString     != NULL) ? RawString     : L"NULL"
    );
    if (RawString == NULL || RawStrCharSet == NULL) {
        BREAD_CRUMB(L"%a:  return 'FALSE'", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    FoundStr = IsStriStr (RawString, RawStrCharSet);

    BREAD_CRUMB(L"%a:  3 - END:- return BOOLEAN FoundStr = '%s'", __func__,
        (FoundStr) ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return FoundStr;
} // BOOLEAN FindSubStr()

/**
  Returns the first occurrence of a Null-terminated ASCII sub-string
  in a Null-terminated ASCII string.

  This function scans the contents of the ASCII string specified by String
  and returns the first occurrence of SearchString. If SearchString is not
  found in String, then NULL is returned. If the length of SearchString is zero,
  then String is returned.

  If String is NULL, then ASSERT().
  If SearchString is NULL, then ASSERT().

  If PcdMaximumAsciiStringLength is not zero, and SearchString or
  String contains more than PcdMaximumAsciiStringLength Unicode characters
  not including the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.
  @param  SearchString    A pointer to a Null-terminated ASCII string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @retval others          If there is a match return the first occurrence of SearchingString.
                          If the length of SearchString is zero,return String.

**/
CHAR8 * MyAsciiStrStr (
    IN const CHAR8 *String,
    IN const CHAR8 *SearchString
) {
    const CHAR8 *FirstMatch;
    const CHAR8 *SearchStringTmp;


    //
    // ASSERT both strings are shorter than PcdMaximumAsciiStringLength
    //
    ASSERT (AsciiStrSize (String) != 0);
    ASSERT (AsciiStrSize (SearchString) != 0);

    if (*SearchString == '\0') {
        return (CHAR8 *) String;
    }

    while (*String != '\0') {
        SearchStringTmp = SearchString;
        FirstMatch = String;

        while (*String == *SearchStringTmp && *String != '\0') {
            String++;
            SearchStringTmp++;
        }

        if (*SearchStringTmp == '\0') {
            return (CHAR8 *) FirstMatch;
        }

        if (*String == '\0') {
            return NULL;
        }

        String = FirstMatch + 1;
    } // while

    return NULL;
} // CHAR16 * MyAsciiStrStr()

// Convert input string to all-lowercase.
// DO NOT USE the standard StrLwr() function, as it is broken on some EFIs!
VOID ToLower (
    IN OUT CHAR16 *MyString
) {
    UINTN i;


    if (MyString == NULL) {
        return;
    }

    i = 0;
    while (MyString[i] != L'\0') {
        if ((MyString[i] >= L'A') && (MyString[i] <= L'Z')) {
            MyString[i] = MyString[i] - L'A' + L'a';
        }
        i++;
    } // while
} // VOID ToLower()

// Convert input string to all-uppercase.
VOID ToUpper (
    IN OUT CHAR16 *MyString
) {
    UINTN i;


    if (MyString == NULL) {
        return;
    }

    i = 0;
    while (MyString[i] != L'\0') {
        if ((MyString[i] >= L'a') && (MyString[i] <= L'z')) {
            MyString[i] = MyString[i] - L'a' + L'A';
        }
        i++;
    } // while
} // VOID ToUpper()

static
VOID MergeStringsHelper (
    IN OUT CHAR16  **First,
    IN     CHAR16   *Second,
    IN     CHAR16    AddChar,
    IN     BOOLEAN   UniqueOnly
) {
    UINTN    i;
    UINTN    Length1;
    UINTN    Length2;
    UINTN    BufSize;
    CHAR16  *TestStr;
    CHAR16  *NewString;
    BOOLEAN  SkipMerge;


    if (*First == NULL) {
        *First = StrDuplicate (Second);

        return;
    }

    Length1 = StrLen (*First);
    Length2 = (
        Second != NULL
    ) ? StrLen (Second) : 0;

    // DA-TAG: Added 2 for AddChar and null terminator
    BufSize = Length1 + Length2 + 2;
    NewString = AllocatePool (
        BufSize * sizeof (CHAR16)
    );
    if (NewString == NULL) {
        return;
    }

    if (*First != NULL && Length1 == 0) {
        MY_FREE_POOL(*First);
    }

    NewString[0] = L'\0';
    if (*First != NULL) {
        SafeStrCat (
            NewString,
            BufSize,
            *First
        );

        if (AddChar) {
            StrnCatS (
                NewString,
                BufSize,
                &AddChar, 1
            );
        }
    }

    if (Second != NULL) {
        SkipMerge = FALSE;

        if (UniqueOnly && AddChar) {
            i = 0;
            while (!SkipMerge) {
                TestStr = FindCommaDelimited (
                    NewString, i++
                );
                if (TestStr == NULL) break;

                NestedStrStr = TRUE;
                if (MyStriCmp (TestStr, Second)) {
                    SkipMerge = TRUE;
                }
                NestedStrStr = FALSE;

                MY_FREE_POOL(TestStr);
            } // while
        }

        if (!SkipMerge) {
            SafeStrCat (
                NewString,
                BufSize,
                Second
            );
        }
        else {
            if (AddChar) {
                // Remove AddChar if not merging this item
                NewString[Length1] = L'\0';
            }
        }
    }

    MY_FREE_POOL(*First);
    *First = NewString;
} // static VOID MergeStringsHelper()

// Merges two strings, creating a new one and returning a pointer to it.
// If AddChar != 0, the specified character is placed between the two original
// strings (unless the first string is NULL or empty). The original input
// string *First is de-allocated and replaced by the new merged string.
// This is similar to StrCat, but safer and more flexible because
// MergeStrings allocates memory that is the correct size for the
// new merged string, so it can take a NULL *First and it cleans
// up the old memory. It should *NOT* be used with a constant
// *First, though.
VOID MergeStrings (
    IN OUT CHAR16 **First,
    IN     CHAR16  *Second,
    IN     CHAR16   AddChar
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    MergeStringsHelper (
        First, Second,
        AddChar, FALSE
    );

    BREAD_CRUMB(L"%a:  2 - END", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID MergeStrings()

// As MergeStrings but does not repeat substrings.
VOID MergeUniqueStrings (
    IN OUT CHAR16 **First,
    IN     CHAR16  *Second,
    IN     CHAR16   AddChar
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    MergeStringsHelper (
        First, Second,
        AddChar, TRUE
    );

    BREAD_CRUMB(L"%a:  2 - END", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID MergeUniqueStrings()

static
VOID MergeWordsHelper (
    CHAR16  **MergeTo,
    CHAR16   *InString,
    CHAR16    AddChar,
    BOOLEAN   UniqueOnly
) {
    CHAR16  *Temp, *Word, *p;
    BOOLEAN  LineFinished;


    if (InString == NULL) {
        return;
    }

    Temp = Word = p = StrDuplicate (
        InString
    );
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if ((*p == L' ')  ||
                (*p == L':')  ||
                (*p == L'_')  ||
                (*p == L'-')  ||
                (*p == L'/')  ||
                (*p == L'\\') ||
                (*p == L'\0')
            ) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    if (UniqueOnly) {
                        MergeUniqueStrings (
                            MergeTo,
                            Word,
                            AddChar
                        );
                    }
                    else {
                        MergeStrings (
                            MergeTo,
                            Word,
                            AddChar
                        );
                    }
                }

                Word = p + 1;
            }

            p++;
        } // while

        MY_FREE_POOL(Temp);
    }
} // static VOID MergeWordsHelper()

// Similar to MergeStrings, but breaks the input string into word chunks
// then merges each separately. Words are defined as string fragments
// separated by ' ', ':', '_', '\', '/', or '-'.
VOID MergeWords (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    MergeWordsHelper (
        MergeTo, InString,
        AddChar, FALSE
    );
} // VOID MergeWords()

// As MergeWords, but only unique words are merged
VOID MergeUniqueWords (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    MergeWordsHelper (
        MergeTo, InString,
        AddChar, TRUE
    );
} // VOID MergeUniqueWords()

// As MergeUniqueWords, but items are separated by ','
VOID MergeUniqueItems (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    UINTN   i;
    CHAR16 *Item;


    if (InString == NULL) {
        return;
    }

    i = 0;
    while (1) {
        Item = FindCommaDelimited (
            InString, i++
        );
        if (Item == NULL) break;

        MergeUniqueStrings (
            MergeTo,
            Item,
            AddChar
        );
        MY_FREE_POOL(Item);
    } // while {Infinite}
} // VOID MergeUniqueItems()

// Replaces special characters in the input string with a space.
CHAR16 * SanitiseString (
    CHAR16  *InString
) {
    CHAR16  *Temp, *Word, *p;
    CHAR16  *OutString;
    BOOLEAN  LineFinished;


    if (InString == NULL) {
        return NULL;
    }

    OutString = NULL;
    Temp = Word = p = StrDuplicate (
        InString
    );
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if (
                (*p != L' ') &&
                (*p != L'_') &&
                (*p != L'-') &&
                !('a' <= *p && 'z' >= *p) &&
                !('A' <= *p && 'Z' >= *p) &&
                !('0' <= *p && '9' >= *p)
            ) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    MergeStrings (
                        &OutString,
                        Word, L' '
                    );
                }

                Word = p + 1;
            }

            p++;
        } // while

        MY_FREE_POOL(Temp);
    }

    if (OutString == NULL) {
        OutString = StrDuplicate (
            InString
        );
    }

    return OutString;
} // CHAR16 * SanitiseString()

// Restrict 'TheString' to no more than 'Limit' characters.
// Does this in two steps:
//   - Compresses blocks of two or more spaces down to one.
//   - Truncates 'TheString' if still longer than 'Limit'.
// Returns TRUE if changes were made or FALSE.
BOOLEAN LimitStringLength (
    CHAR16 *TheString,
    UINTN    Limit
) {
    UINTN     i;
    UINTN     DestSize;
    CHAR16   *SubString;
    CHAR16   *TempString;
    BOOLEAN   HasChanged;
    BOOLEAN   WasTruncated;


    if (TheString == NULL) {
        return FALSE;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (StrLen (TheString) < Limit) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- return BOOLEAN HasChanged = 'FALSE'", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    //BREAD_CRUMB(L"%a:  2 - WHILE LOOP:- START/ENTER", __func__);
    HasChanged = FALSE;
    // SubString will be NULL or point WITHIN TheString
    SubString = MyStrStr (TheString, L"  ");

    //BREAD_CRUMB(L"%a:  3 - WHILE LOOP:- START/ENTER", __func__);
    while (SubString != NULL) {
        i = 0;
        while (SubString[i] == L' ') {
            i++;
        }

        if (i >= StrLen (SubString)) {
            SubString[0] = '\0';
        }
        else {
            TempString = StrDuplicate (
                &SubString[i]
            );
            if (TempString == NULL) {
                // Memory Allocation Problem ... abort to avoid potential infinite loop!
                break;
            }

            DestSize = StrSize (
                &SubString[1]
            ) / sizeof (CHAR16);
            StrCpyS (
                &SubString[1],
                DestSize, TempString
            );
            MY_FREE_POOL(TempString);
        }

        HasChanged = TRUE;
        SubString = MyStrStr (TheString, L"  ");
    } // while
    //BREAD_CRUMB(L"%a:  4 - WHILE LOOP:- END/EXIT", __func__);

    // Truncate if still too long.
    WasTruncated = TruncateString (
        TheString, Limit
    );

    //BREAD_CRUMB(L"%a:  5", __func__);
    if (!HasChanged) {
        //BREAD_CRUMB(L"%a:  5a 1", __func__);
        HasChanged = WasTruncated;
    }

    BREAD_CRUMB(L"%a:  6 - END:- return BOOLEAN HasChanged = '%s'", __func__,
        (HasChanged) ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return HasChanged;
} // BOOLEAN LimitStringLength()

// Truncate 'TheString' to 'Limit' characters if longer.
// Returns TRUE if truncated or FALSE.
BOOLEAN TruncateString (
    CHAR16 *TheString,
    UINTN   Limit
) {
    BOOLEAN WasTruncated;


    if (StrLen (TheString) <= Limit) {
        WasTruncated = FALSE;
    }
    else {
        TheString[Limit] = '\0';
        WasTruncated     = TRUE;
    }

    return WasTruncated;
} // BOOLEAN TruncateString()

// Returns all the digits in the input string, including intervening
// non-digit characters. For instance, if InString is "foo-3.3.4-7.img",
// this function returns "3.3.4-7". The GlobalConfig.ExtraKernelVersionStrings
// variable specifies extra strings that may be treated as numbers. If InString
// contains no digits or ExtraKernelVersionStrings, the return value is NULL.
CHAR16 * FindNumbers (
    IN CHAR16 *InString
) {
    UINTN   i, EndOfElement, StartOfElement, CopyLength;
    CHAR16 *Found, *ExtraFound, *LookFor;


    if (InString == NULL) {
        return NULL;
    }

    StartOfElement = StrLen (InString);

    // Find extra_kernel_version_strings
    EndOfElement = i = 0;
    ExtraFound = NULL;
    while (ExtraFound == NULL) {
        LookFor = FindCommaDelimited (
            GlobalConfig.ExtraKernelVersionStrings, i++
        );
        if (LookFor == NULL) break;

        ExtraFound = MyStrStr (InString, LookFor);
        if (ExtraFound != NULL) {
            StartOfElement = ExtraFound - InString;
            EndOfElement = (StrLen (LookFor) + StartOfElement) - 1;
        }

        MY_FREE_POOL(LookFor);
    } // while

    // Find start & end of target element
    for (i = 0; InString[i] != L'\0'; i++) {
        if ((InString[i] >= L'0') && (InString[i] <= L'9')) {
            if (StartOfElement > i) {
                StartOfElement = i;
            }

            if (EndOfElement < i) {
                EndOfElement = i;
            }
        }
    } // for

    // Extract the target element
    Found = NULL;
    if (EndOfElement > 0) {
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;

            Found = StrDuplicate (
                &InString[StartOfElement]
            );
            if (Found != NULL) {
                Found[CopyLength] = 0;
            }
        }
    }

    return (Found);
} // CHAR16 *FindNumbers()

// Returns the number of characters that are in common between
// String1 and String2 before they diverge. For instance, if
// String1 is "FooBar" and String2 is "FoodiesBar", this function
// will return "3", since they both start with "Foo".
UINTN NumCharsInCommon (
    IN CHAR16 *String1,
    IN CHAR16 *String2
) {
    UINTN Count;


    if (String1 == NULL || String2 == NULL) {
        return 0;
    }

    Count = 0;
    while (
        String1[Count] != L'\0'      &&
        String2[Count] != L'\0'      &&
        String1[Count] == String2[Count]
    ) {
        Count++;
    } // while

    return Count;
} // UINTN NumCharsInCommon()

// Find the #Index element (numbered from 0) in a comma-delimited string.
// The calling function must free any memory allocated.
// Returns the found element or NULL.
//
// DA-TAG: Updated for 'ABC, 123, XYZ, 456'
//         That is, ignores leading spaces
//         'Internal' spaces not affected
//         'A B C, XYZ' = 'A B C' & 'XYZ'
CHAR16 * FindCommaDelimited (
    IN CHAR16 *InString,
    IN UINTN   Index
) {
    UINTN     CurPos;
    UINTN     StartPos;
    UINTN     InLength;
    BOOLEAN   Found;
    BOOLEAN   LeadingSpace;
    CHAR16   *FoundString;


    if (InString == NULL) {
        return NULL;
    }

    StartPos = CurPos = 0;
    InLength = StrLen (InString);

    // After while() loop, StartPos marks start of item #Index
    while (Index > 0 && CurPos < InLength) {
        if (InString[CurPos] == L',') {
            Index--;
            StartPos = CurPos + 1;
        }

        CurPos++;
    } // while

    Found        = FALSE;
    LeadingSpace =  TRUE;

    // After while() loop, CurPos is one past the end of the element
    while (!Found && CurPos < InLength) {
        if (InString[CurPos] == L',') {
            Found = TRUE;
        }
        else {
            // Move Current Position
            CurPos++;

            if (LeadingSpace) {
                if (InString[CurPos] == L' ') {
                    // Ignore Leading Space ... Move Start Position
                    ++StartPos;
                }
                else {
                    // No Leading Space
                    LeadingSpace = FALSE;
                }
            }
        }
    } // while

    FoundString = NULL;
    if (Index == 0)  {
        FoundString = StrDuplicate (
            &InString[StartPos]
        );
    }

    if (FoundString != NULL) {
        FoundString[CurPos - StartPos] = 0;
    }

    return FoundString;
} // CHAR16 * FindCommaDelimited()

// Delete an element from a list of comma separated values.
// Modifies the *List string, but not the *ToDelete string.
// Returns TRUE if the item was deleted, FALSE otherwise.
BOOLEAN DeleteItemFromCsvList (
    CHAR16  *ToDelete,
    CHAR16 **List
) {
    CHAR16  *Found;
    CHAR16  *Comma;
    CHAR16  *PartA;   // Do *NOT* Free
    CHAR16  *PartB;   // Do *NOT* Free
    CHAR16  *TmpStr;
    BOOLEAN  Retval;


    if (ToDelete == NULL || *List == NULL) {
        return FALSE;
    }

    Retval = FALSE;
    Found = MyStrStr (*List, ToDelete);
    if (Found != NULL) {
        Comma = MyStrStr (Found, L",");
        if (Comma == NULL) {
            // 'Found' is final element
            if (Found == *List) {
                // 'Found' is ONLY element
                *List[0] = L'\0';
            }
            else {
                // Delete the comma preceding 'Found'.
                Found--;
                Found[0] = L'\0';
            }
        }
        else {
            // 'Found' is NOT the final element
            TmpStr = PoolPrint (L",%s", ToDelete);
            PartA = GetSubStrBefore (
                TmpStr, *List
            );
            if (PartA == *List) {
                PartA = GetSubStrBefore (
                    ToDelete, *List
                );
                if (MyStriCmp (PartA, *List)) {
                    PartA = NULL;
                }
            }
            MY_FREE_POOL(TmpStr);

            TmpStr = PoolPrint (L"%s,", ToDelete);
            PartB = GetSubStrAfter (
                TmpStr, *List
            );
            if (PartB == *List) {
                PartB = GetSubStrAfter (
                    ToDelete, *List
                );
                if (MyStriCmp (PartB, *List)) {
                    PartB = NULL;
                }
            }
            MY_FREE_POOL(TmpStr);

            if (PartA != NULL || PartB != NULL) {
                MY_FREE_POOL(*List);

                if (PartA != NULL && PartB != NULL) {
                    *List = PoolPrint (
                        L"%s,%s",
                        PartA, PartB
                    );
                }
                else if (PartA != NULL) {
                    *List = StrDuplicate (
                        PartA
                    );
                }
                else {
                    *List = StrDuplicate (
                        PartB
                    );
                }
            }
        }

        Retval = TRUE;
    }

    return Retval;
} // BOOLEAN DeleteItemFromCsvList()

// Replaced by IsListItem.
// Kept for upstream compatibility.
BOOLEAN IsIn (
    IN CHAR16 *SmallString,
    IN CHAR16 *List
) {
    if (SmallString == NULL || List == NULL) {
        return FALSE;
    }

    return IsListItem (
        SmallString, List
    );
} // BOOLEAN IsIn()

// Replaced by IsListItemSubstringIn.
// Kept for upstream compatibility.
BOOLEAN IsInSubstring (
    IN CHAR16 *BigString,
    IN CHAR16 *List
) {
    if (BigString == NULL || List == NULL) {
        return FALSE;
    }

    return IsListItemSubstringIn (
        BigString, List
    );
} // BOOLEAN IsInSubstring()

// Returns TRUE if TestString matches a pattern in the comma-delimited List,
// FALSE otherwise.
BOOLEAN IsListMatch (
    IN CHAR16 *TestString,
    IN CHAR16 *List
) {
    UINTN     i;
    BOOLEAN   Found;
    CHAR16   *OnePattern;


    if (TestString == NULL || List == NULL) {
        return FALSE;
    }

    i     =     0;
    Found = FALSE;
    while (!Found) {
        OnePattern = FindCommaDelimited (
            List, i++
        );
        if (OnePattern == NULL) break;

        if (RefitMetaiMatch (TestString, OnePattern)) {
            Found = TRUE;
        }
        MY_FREE_POOL(OnePattern);
    } // while

   return Found;
} // BOOLEAN IsListMatch()

// Returns TRUE if SmallString is an element in the comma-delimited List,
// FALSE otherwise. Performs comparison case-insensitively.
BOOLEAN IsListItem (
    IN CHAR16 *SmallString,
    IN CHAR16 *List
) {
    UINTN     i;
    BOOLEAN   Found;
    CHAR16   *OneItem;


    if (SmallString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found) {
        OneItem = FindCommaDelimited (
            List, i++
        );
        if (OneItem == NULL) break;

        if (MyStriCmp (OneItem, SmallString)) {
            Found = TRUE;
        }

        MY_FREE_POOL(OneItem);
    } // while

   return Found;
} // BOOLEAN IsListItem()

// Returns TRUE if any element of List can be found as a substring of
// BigString or FALSE. Performs comparisons case-insensitively.
BOOLEAN IsListItemSubstringIn (
    IN CHAR16 *BigString,
    IN CHAR16 *List
) {
    BOOLEAN  Found;
    UINTN    ElementLength, i;
    CHAR16  *OneElement;


    if (BigString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found) {
        OneElement = FindCommaDelimited (
            List, i++
        );
        if (OneElement == NULL) break;

        ElementLength = StrLen (OneElement);
        if (ElementLength > 0                   &&
            ElementLength <= StrLen (BigString) &&
            IsStriStr (BigString, OneElement)
        ) {
            Found = TRUE;
        }

        if (!Found) {
            if (ElementLength <= StrLen (BigString) &&
                IsStriStr (BigString, OneElement)
            ) {
                Found = TRUE;
            }
        }
        MY_FREE_POOL(OneElement);
    } // while

    return Found;
} // BOOLEAN IsListItemSubstringIn()

// Replace *SearchString in **MainString with *ReplString -- but if *SearchString
// is preceded by "%", instead remove that character.
// Returns TRUE if replacement was done or FALSE.
BOOLEAN ReplaceSubstring (
    IN OUT CHAR16 **MainString,
    IN     CHAR16  *SearchString,
    IN     CHAR16  *ReplString
) {
    UINTN   DestSize;
    CHAR16 *EndString;
    CHAR16 *NewString;
    CHAR16 *FoundSearchString;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- Replace '%s' with '%s' in '%s'", __func__,
        (SearchString != NULL) ? SearchString : L"NULL",
        (ReplString   != NULL) ? ReplString   : L"NULL",
        (*MainString  != NULL) ? *MainString  : L"NULL"
    );
    if (*MainString == NULL || SearchString == NULL || ReplString == NULL) {
        BREAD_CRUMB(L"%a:  1a - END:- return BOOLEAN 'FALSE' ... NULL Input!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    FoundSearchString = MyStrStr (*MainString, SearchString);
    NestedStrStr      = FALSE;

    BREAD_CRUMB(L"%a:  3", __func__);
    if (FoundSearchString == NULL) {
        BREAD_CRUMB(L"%a:  3a - END:- return BOOLEAN 'FALSE' ... SearchString *NOT* Found!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    DestSize = StrLen (*MainString) + 1;
    NewString = AllocateZeroPool (DestSize * sizeof (CHAR16));
    if (NewString == NULL) {
        BREAD_CRUMB(L"%a:  4a - END:- return BOOLEAN 'FALSE' ... Out of Resources!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    EndString = &(FoundSearchString[StrLen (SearchString)]);
    FoundSearchString[0] = L'\0';

    BREAD_CRUMB(L"%a:  6", __func__);
    // "FoundSearchString > *MainString" is required to make sure:
    // "FoundSearchString" is within "*MainString" in terms of memory address
    // "FoundSearchString" is not at the start of "*MainString" for the "-1" index
    if ((FoundSearchString > *MainString) &&
        (FoundSearchString[-1] == L'%')
    ) {
        BREAD_CRUMB(L"%a:  6a 1", __func__);
        FoundSearchString[-1] = L'\0';
        ReplString = SearchString;
    }

    BREAD_CRUMB(L"%a:  7", __func__);
    StrCpyS (NewString, DestSize, *MainString);

    BREAD_CRUMB(L"%a:  8", __func__);
    MergeStrings (&NewString, ReplString, L'\0');

    BREAD_CRUMB(L"%a:  9", __func__);
    MergeStrings (&NewString, EndString, L'\0');

    BREAD_CRUMB(L"%a:  10", __func__);
    MY_FREE_POOL(*MainString);
    *MainString = NewString;

    BREAD_CRUMB(L"%a:  11 - END:- return BOOLEAN 'TRUE'", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return TRUE;
} // BOOLEAN ReplaceSubstring()

// Returns TRUE if *Input contains nothing but valid hexadecimal characters,
// FALSE otherwise.
// NB: Exclude leading "0x" from input!
BOOLEAN IsValidHex (
    CHAR16 *Input
) {
    UINTN   i;
    BOOLEAN IsHex;


    i = 0;
    IsHex = TRUE;
    while (IsHex && (Input[i] != L'\0')) {
        if (
            !(
                ((Input[i] >= L'0') && (Input[i] <= L'9')) ||
                ((Input[i] >= L'A') && (Input[i] <= L'F')) ||
                ((Input[i] >= L'a') && (Input[i] <= L'f'))
            )
        ) {
            IsHex = FALSE;
        }

        i++;
    } // while

    return IsHex;
} // BOOLEAN IsValidHex()

// Converts consecutive characters in the input string into a
// number, interpreting the string as a hexadecimal number, starting
// at the specified position and continuing for the specified number
// of characters or until the end of the string, whichever is first.
// NumChars must be between 1 and 16 (Excluding the "0x" notation).
// The "0x" notation is optional in OurStr (makes no difference).
// Invalid characters are handled without 'fouling' the result.
UINT64 StrToHex (
    CHAR16 *OurStr,
    UINTN   Pos,
    UINTN   NumChars
) {
    UINTN   InputLength;
    UINTN   NumDone;
    UINT64  retval;
    CHAR16 *Input;   // Do *NOT* Free
    CHAR16  a;


    if (OurStr == NULL) {
        return 0;
    }

    Input = GetSubStrAfter (
        L"0x", OurStr
    );
    if (NumChars == 0 ||
        NumChars > 16
    ) {
        return 0;
    }

    NumDone = 0;
    retval = 0x00;
    InputLength = StrLen (Input);
    while (Pos <= InputLength && NumDone < NumChars) {
        a = Input[Pos];

        if ((a >= '0') && (a <= '9')) {
            retval *= 0x10;
            retval += (a - '0');
            NumDone++;
        }

        if ((a >= 'a') && (a <= 'f')) {
            retval *= 0x10;
            retval += (a - 'a' + 0x0a);
            NumDone++;
        }

        if ((a >= 'A') && (a <= 'F')) {
            retval *= 0x10;
            retval += (a - 'A' + 0x0a);
            NumDone++;
        }

        Pos++;
    } // while

    return retval;
} // StrToHex()

// Returns TRUE if UnknownString can be interpreted as a GUID or FALSE.
// Note that the input string must have no extraneous spaces and must be
// conventionally formatted as a 36-character GUID, complete with dashes in
// appropriate places.
BOOLEAN IsGuid (
    CHAR16 *UnknownString
) {
    UINTN   Length, i;
    CHAR16  a;
    BOOLEAN retval;


    if (UnknownString == NULL) {
        return FALSE;
    }

    Length = StrLen (UnknownString);
    if (Length != 36) {
        return FALSE;
    }

    retval = TRUE;
    for (i = 0; i < Length; i++) {
        a = UnknownString[i];
        if (i ==  8 ||
            i == 13 ||
            i == 18 ||
            i == 23
        ) {
            if (a != L'-') {
                retval = FALSE;
                break;
            }
        }
        // DA-TAG: Investigate This
        //         Condition below can apparently never be met (coverity scan)
        //         Comment out until review
        //else if (
        //    ((a < L'a') || (a > L'f')) &&
        //    ((a < L'A') || (a > L'F')) &&
        //    ((a < L'0') && (a > L'9'))
        //) {
        //    retval = FALSE;
        //    break;
        //}
    } // for

    return retval;
} // BOOLEAN IsGuid()

// Return the GUID as a string, suitable for display to the user.
// The calling function must free any allocated memory.
CHAR16 * GuidAsString (
    EFI_GUID *GuidData
) {
    CHAR16 *TheString;


    if (GuidData == NULL) {
        // Early Return
        return NULL;
    }

    TheString = AllocatePool (sizeof (CHAR16) * 37);
    if (TheString == NULL) {
        // Early Return
        return NULL;
    }

    SPrint (
        TheString,
        sizeof (CHAR16) * 37,
        L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (UINTN) GuidData->Data1,
        (UINTN) GuidData->Data2,
        (UINTN) GuidData->Data3,
        (UINTN) GuidData->Data4[0],
        (UINTN) GuidData->Data4[1],
        (UINTN) GuidData->Data4[2],
        (UINTN) GuidData->Data4[3],
        (UINTN) GuidData->Data4[4],
        (UINTN) GuidData->Data4[5],
        (UINTN) GuidData->Data4[6],
        (UINTN) GuidData->Data4[7]
    );

    return TheString;
} // CHAR16 * GuidAsString()

EFI_GUID StringAsGuid (
    CHAR16 *InString
) {
    EFI_GUID  Guid = NULL_GUID_VALUE;


    if (!IsGuid (InString)) {
        return Guid;
    }

    Guid.Data1    = (UINT32) StrToHex (InString,  0, 8);
    Guid.Data2    = (UINT16) StrToHex (InString,  9, 4);
    Guid.Data3    = (UINT16) StrToHex (InString, 14, 4);
    Guid.Data4[0] =  (UINT8) StrToHex (InString, 19, 2);
    Guid.Data4[1] =  (UINT8) StrToHex (InString, 21, 2);
    Guid.Data4[2] =  (UINT8) StrToHex (InString, 23, 2);
    Guid.Data4[3] =  (UINT8) StrToHex (InString, 26, 2);
    Guid.Data4[4] =  (UINT8) StrToHex (InString, 28, 2);
    Guid.Data4[5] =  (UINT8) StrToHex (InString, 30, 2);
    Guid.Data4[6] =  (UINT8) StrToHex (InString, 32, 2);
    Guid.Data4[7] =  (UINT8) StrToHex (InString, 34, 2);

    return Guid;
} // EFI_GUID StringAsGuid()

// Delete the STRING_LIST pointed to by *StringList.
VOID DeleteStringList (
    STRING_LIST *StringList
) {
    STRING_LIST *Current, *Previous;


    if (StringList == NULL) {
        return;
    }

    Current = StringList;
    while (Current != NULL) {
        MY_FREE_POOL(Current->Value);
        Previous = Current;
        Current  = Current->Next;
        MY_FREE_POOL(Previous);
    }
} // VOID DeleteStringList()

/** Convert null terminated ascii string to unicode.

  @param[in]  String1  A pointer to the ascii string to convert to unicode.
  @param[in]  Length   Length or 0 to calculate the length of the ascii string to convert.

  @retval  A pointer to the converted unicode string allocated from pool.
**/
CHAR16 * MyAsciiStrCopyToUnicode (
    IN  CHAR8   *AsciiString,
    IN  UINTN    Length
) {
    CHAR16  *UnicodeString;
    CHAR16  *UnicodeStringWalker;
    UINTN    UnicodeStringSize;


    if (AsciiString == NULL) {
        return NULL;
    }

    if (Length == 0) {
        Length = AsciiStrLen (AsciiString);
    }

    UnicodeStringSize = (Length + 1) * sizeof (CHAR16);
    UnicodeString = AllocatePool (UnicodeStringSize);

    if (UnicodeString != NULL) {
        UnicodeStringWalker = UnicodeString;
        while (*AsciiString != '\0' && Length--) {
            *(UnicodeStringWalker++) = *(AsciiString++);
        } // while
        *UnicodeStringWalker = L'\0';
    }

    return UnicodeString;
} // CHAR16 * MyAsciiStrCopyToUnicode()

VOID MyUnicodeFilterString (
    IN OUT CHAR16   *String,
    IN     BOOLEAN   SingleLine
) {
    while (*String != L'\0') {
        if ((*String & 0x7FU) != *String) {
            // Remove all unicode characters.
            *String = L'_';
        }
        else if (
            SingleLine &&
            (
                *String == L'\r' ||
                *String == L'\n'
            )
        ) {
            // Stop after printing one line.
            *String = L'\0';

            break;
        }
        else {
            if (*String < 0x20 ||
                *String == 0x7F
            ) {
                // Drop all unprintable spaces but space including tabs.
                *String = L'_';
            }
        }

        ++String;
    }
} // VOID MyUnicodeFilterString()
