// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/unistr.h"
#include "unicode/sortkey.h"
#include "encoll.h"
#include "cmemory.h"

CollationEnglishTest::CollationEnglishTest()
: myCollation(nullptr)
{
    UErrorCode status = U_ZERO_ERROR;
    myCollation = Collator::createInstance(Locale::getEnglish(), status);
}

CollationEnglishTest::~CollationEnglishTest()
{
    delete myCollation;
}

static const char16_t testSourceCases[][CollationEnglishTest::MAX_TOKEN_LEN] = {
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x002D /* '-' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x0020 /* ' ' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x002D /* '-' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0048 /* 'H' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x006C /* 'l' */, (char16_t)0x006C /* 'l' */, (char16_t)0x006F /* 'o' */, 0},
        {(char16_t)0x0041 /* 'A' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0043 /* 'C' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x002D /* '-' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x002D /* '-' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00EA, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, 0x00E9, 0},
        {0x00C4, (char16_t)0x0042 /* 'B' */, 0x0308, (char16_t)0x0043 /* 'C' */, 0x0308, 0},
        {(char16_t)0x0061 /* 'a' */, 0x0308, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x0072 /* 'r' */, 0},
        {(char16_t)0x0072 /* 'r' */, (char16_t)0x006F /* 'o' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x0073 /* 's' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0041 /* 'A' */, 0},
        {(char16_t)0x0041 /* 'A' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, 0},
        {(char16_t)0x0074 /* 't' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006F /* 'o' */, (char16_t)0x006D /* 'm' */, (char16_t)0x0070 /* 'p' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x0070 /* 'p' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x006E /* 'n' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0023 /* '#' */, (char16_t)0x0062 /* 'b' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0023 /* '#' */, (char16_t)0x0062 /* 'b' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0041 /* 'A' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {0x00E6, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0048 /* 'H' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, 0x0308, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0074 /* 't' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0069 /* 'i' */, 0x0302, (char16_t)0x0073 /* 's' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00EA, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, 0x00E6, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, 0x00E6, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, 0x00E9, 0}                                            // 49
};

static const char16_t testTargetCases[][CollationEnglishTest::MAX_TOKEN_LEN] = {
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x002D /* '-' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, 0},
        {(char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x006C /* 'l' */, (char16_t)0x006C /* 'l' */, (char16_t)0x006F /* 'o' */, 0},
        {(char16_t)0x0041 /* 'A' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0043 /* 'C' */, 0},
        {(char16_t)0x0041 /* 'A' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0043 /* 'C' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0073 /* 's' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0073 /* 's' */, 0},
        {(char16_t)0x0062 /* 'b' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x006B /* 'k' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, 0x00E9, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x0072 /* 'r' */, 0},
        {0x00C4, (char16_t)0x0042 /* 'B' */, 0x0308, (char16_t)0x0043 /* 'C' */, 0x0308, 0},
        {(char16_t)0x0041 /* 'A' */, 0x0308, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */, 0},
        {(char16_t)0x0072 /* 'r' */, (char16_t)0x006F /* 'o' */, 0x0302, (char16_t)0x006C /* 'l' */, (char16_t)0x0065 /* 'e' */, 0},
        {(char16_t)0x0041 /* 'A' */, 0x00E1, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0041 /* 'A' */, 0x00E1, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0054 /* 'T' */, (char16_t)0x0043 /* 'C' */, (char16_t)0x006F /* 'o' */, (char16_t)0x006D /* 'm' */, (char16_t)0x0070 /* 'p' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0072 /* 'r' */, (char16_t)0x0065 /* 'e' */, (char16_t)0x0050 /* 'P' */, (char16_t)0x006C /* 'l' */, (char16_t)0x0061 /* 'a' */, (char16_t)0x0069 /* 'i' */, (char16_t)0x006E /* 'n' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0023 /* '#' */, (char16_t)0x0042 /* 'B' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0026 /* '&' */, (char16_t)0x0062 /* 'b' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0023 /* '#' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {0x00C4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {0x00C4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {0x00C4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, (char16_t)0x0064 /* 'd' */, (char16_t)0x0061 /* 'a' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0023 /* '#' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x003D /* '=' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0064 /* 'd' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0043 /* 'C' */, (char16_t)0x0048 /* 'H' */, (char16_t)0x0063 /* 'c' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0074 /* 't' */, (char16_t)0x0068 /* 'h' */, 0x00EE, (char16_t)0x0073 /* 's' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00E9, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, 0x00E9, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0043 /* 'C' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0062 /* 'b' */, (char16_t)0x0064 /* 'd' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, 0x00C6, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0064 /* 'd' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, 0x00C6, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0061 /* 'a' */, (char16_t)0x0042 /* 'B' */, (char16_t)0x0064 /* 'd' */, 0},
        {0x00E4, (char16_t)0x0062 /* 'b' */, (char16_t)0x0063 /* 'c' */, 0},
        {(char16_t)0x0070 /* 'p' */, 0x00EA, (char16_t)0x0063 /* 'c' */, (char16_t)0x0068 /* 'h' */, (char16_t)0x0065 /* 'e' */, 0}                                           // 49
};

static const Collator::EComparisonResult results[] = {
        Collator::LESS, 
        Collator::LESS, /*Collator::GREATER,*/
        Collator::LESS,
        Collator::GREATER,
        Collator::GREATER,
        Collator::EQUAL,
        Collator::LESS,
        Collator::LESS,
        Collator::LESS,
        Collator::LESS, /*Collator::GREATER,*/                                                          /* 10 */
        Collator::GREATER,
        Collator::LESS,
        Collator::EQUAL,
        Collator::LESS,
        Collator::GREATER,
        Collator::GREATER,
        Collator::GREATER,
        Collator::LESS,
        Collator::LESS,
        Collator::LESS,                                                             /* 20 */
        Collator::LESS,
        Collator::LESS,
        Collator::LESS,
        Collator::GREATER,
        Collator::GREATER,
        Collator::GREATER,
        /* Test Tertiary  > 26 */
        Collator::LESS,
        Collator::LESS,
        Collator::GREATER,
        Collator::LESS,                                                             /* 30 */
        Collator::GREATER,
        Collator::EQUAL,
        Collator::GREATER,
        Collator::LESS,
        Collator::LESS,
        Collator::LESS,
        /* test identical > 36 */
        Collator::EQUAL,
        Collator::EQUAL,
        /* test primary > 38 */
        Collator::EQUAL,
        Collator::EQUAL,                                                            /* 40 */
        Collator::LESS,
        Collator::EQUAL,
        Collator::EQUAL,
        /* test secondary > 43 */
        Collator::LESS,
        Collator::LESS,
        Collator::EQUAL,
        Collator::LESS,
        Collator::LESS, 
        Collator::LESS                                                                  // 49
};

static const char16_t testBugs[][CollationEnglishTest::MAX_TOKEN_LEN] = {
    {0x61, 0},
    {0x41, 0},
    {0x65, 0},
    {0x45, 0},
    {0x00e9, 0},
    {0x00e8, 0},
    {0x00ea, 0},
    {0x00eb, 0},
    {0x65, 0x61, 0},
    {0x78, 0}
};

// 0x0300 is grave, 0x0301 is acute
// the order of elements in this array must be different than the order in CollationFrenchTest
static const char16_t testAcute[][CollationEnglishTest::MAX_TOKEN_LEN] = {
    {0x65, 0x65, 0},
    {0x65, 0x65, 0x0301, 0},
    {0x65, 0x65, 0x0301, 0x0300, 0},
    {0x65, 0x65, 0x0300, 0},
    {0x65, 0x65, 0x0300, 0x0301, 0},
    {0x65, 0x0301, 0x65, 0},
    {0x65, 0x0301, 0x65, 0x0301, 0},
    {0x65, 0x0301, 0x65, 0x0301, 0x0300, 0},
    {0x65, 0x0301, 0x65, 0x0300, 0},
    {0x65, 0x0301, 0x65, 0x0300, 0x0301, 0},
    {0x65, 0x0301, 0x0300, 0x65, 0},
    {0x65, 0x0301, 0x0300, 0x65, 0x0301, 0},
    {0x65, 0x0301, 0x0300, 0x65, 0x0301, 0x0300, 0},
    {0x65, 0x0301, 0x0300, 0x65, 0x0300, 0},
    {0x65, 0x0301, 0x0300, 0x65, 0x0300, 0x0301, 0},
    {0x65, 0x0300, 0x65, 0},
    {0x65, 0x0300, 0x65, 0x0301, 0},
    {0x65, 0x0300, 0x65, 0x0301, 0x0300, 0},
    {0x65, 0x0300, 0x65, 0x0300, 0},
    {0x65, 0x0300, 0x65, 0x0300, 0x0301, 0},
    {0x65, 0x0300, 0x0301, 0x65, 0},
    {0x65, 0x0300, 0x0301, 0x65, 0x0301, 0},
    {0x65, 0x0300, 0x0301, 0x65, 0x0301, 0x0300, 0},
    {0x65, 0x0300, 0x0301, 0x65, 0x0300, 0},
    {0x65, 0x0300, 0x0301, 0x65, 0x0300, 0x0301, 0}
};

static const char16_t testMore[][CollationEnglishTest::MAX_TOKEN_LEN] = {
    {(char16_t)0x0061 /* 'a' */, (char16_t)0x0065 /* 'e' */, 0},
    { 0x00E6, 0},
    { 0x00C6, 0},
    {(char16_t)0x0061 /* 'a' */, (char16_t)0x0066 /* 'f' */, 0},
    {(char16_t)0x006F /* 'o' */, (char16_t)0x0065 /* 'e' */, 0},
    { 0x0153, 0},
    { 0x0152, 0},
    {(char16_t)0x006F /* 'o' */, (char16_t)0x0066 /* 'f' */, 0},
};

void CollationEnglishTest::TestTertiary(/* char* par */)
{
    int32_t i = 0;
    myCollation->setStrength(Collator::TERTIARY);
    for (i = 0; i < 38 ; i++)
    {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }

    int32_t j = 0;
    for (i = 0; i < 10; i++)
    {
        for (j = i+1; j < 10; j++)
        {
            doTest(myCollation, testBugs[i], testBugs[j], Collator::LESS);
        }
    }

    //test more interesting cases
    Collator::EComparisonResult expected;
    const int32_t testMoreSize = UPRV_LENGTHOF(testMore);
    for (i = 0; i < testMoreSize; i++)
    {
        for (j = 0; j < testMoreSize; j++)
        {
            if (i <  j)
                expected = Collator::LESS;
            else if (i == j)
                expected = Collator::EQUAL;
            else // (i >  j)
                expected = Collator::GREATER;
            doTest(myCollation, testMore[i], testMore[j], expected );
        }
    }

}

void CollationEnglishTest::TestPrimary(/* char* par */)
{
    int32_t i;
    myCollation->setStrength(Collator::PRIMARY);
    for (i = 38; i < 43 ; i++)
    {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }
}

void CollationEnglishTest::TestSecondary(/* char* par */)
{
    int32_t i;
    myCollation->setStrength(Collator::SECONDARY);
    for (i = 43; i < 49 ; i++)
    {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }

    //test acute and grave ordering (compare to french collation)
    int32_t j;
    Collator::EComparisonResult expected;
    const int32_t testAcuteSize = UPRV_LENGTHOF(testAcute);
    for (i = 0; i < testAcuteSize; i++)
    {
        for (j = 0; j < testAcuteSize; j++)
        {
            if (i <  j)
                expected = Collator::LESS;
            else if (i == j)
                expected = Collator::EQUAL;
            else // (i >  j)
                expected = Collator::GREATER;
            doTest(myCollation, testAcute[i], testAcute[j], expected );
        }
    }
}

void CollationEnglishTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite CollationEnglishTest: ");
    if(myCollation) {
      switch (index) {
          case 0: name = "TestPrimary";   if (exec)   TestPrimary(/* par */); break;
          case 1: name = "TestSecondary"; if (exec)   TestSecondary(/* par */); break;
          case 2: name = "TestTertiary";  if (exec)   TestTertiary(/* par */); break;
          default: name = ""; break;
      }
    } else {
      dataerrln("Collator couldn't be instantiated!");
      name = "";
    }
}

#endif /* #if !UCONFIG_NO_COLLATION */
