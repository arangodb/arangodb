// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// ucptrietest.c (modified from trie2test.c)
// created: 2017dec29 Markus W. Scherer

#include <stdbool.h>
#include <stdio.h>
#include "unicode/utypes.h"
#include "unicode/ucptrie.h"
#include "unicode/umutablecptrie.h"
#include "unicode/utf.h"
#include "unicode/utf16.h"
#include "unicode/utf8.h"
#include "uassert.h"
#include "utrie.h"
#include "cstring.h"
#include "cmemory.h"
#include "udataswp.h"
#include "cintltst.h"
#include "writesrc.h"
#include "toolutil.h"

void addUCPTrieTest(TestNode** root);

/* Values for setting possibly overlapping, out-of-order ranges of values */
typedef struct SetRange {
    UChar32 start, limit;
    uint32_t value;
} SetRange;

/*
 * Values for testing:
 * value is set from the previous boundary's limit to before
 * this boundary's limit
 *
 * There must be an entry with limit 0 and the intialValue.
 * It may be preceded by an entry with negative limit and the errorValue.
 */
typedef struct CheckRange {
    UChar32 limit;
    uint32_t value;
} CheckRange;

static int32_t
skipSpecialValues(const CheckRange checkRanges[], int32_t countCheckRanges) {
    int32_t i;
    for(i=0; i<countCheckRanges && checkRanges[i].limit<=0; ++i) {}
    return i;
}

static int32_t
getSpecialValues(const CheckRange checkRanges[], int32_t countCheckRanges,
                 uint32_t *pInitialValue, uint32_t *pErrorValue) {
    int32_t i=0;
    if(i<countCheckRanges && checkRanges[i].limit<0) {
        *pErrorValue=checkRanges[i++].value;
    } else {
        *pErrorValue=0xad;
    }
    if(i<countCheckRanges && checkRanges[i].limit==0) {
        *pInitialValue=checkRanges[i++].value;
    } else {
        *pInitialValue=0;
    }
    return i;
}

/* ucptrie_enum() callback, modifies a value */
static uint32_t U_CALLCONV
testFilter(const void *context, uint32_t value) {
    (void)context; // suppress compiler warnings about unused variable
    return value ^ 0x5555;
}

static UBool
doCheckRange(const char *name, const char *variant,
             UChar32 start, UChar32 end, uint32_t value,
             UChar32 expEnd, uint32_t expValue) {
    if (end < 0) {
        if (expEnd >= 0) {
            log_err("error: %s getRanges (%s) fails to deliver range [U+%04lx..U+%04lx].0x%lx\n",
                    name, variant, (long)start, (long)expEnd, (long)expValue);
        }
        return false;
    }
    if (expEnd < 0) {
        log_err("error: %s getRanges (%s) delivers unexpected range [U+%04lx..U+%04lx].0x%lx\n",
                name, variant, (long)start, (long)end, (long)value);
        return false;
    }
    if (end != expEnd || value != expValue) {
        log_err("error: %s getRanges (%s) delivers wrong range [U+%04lx..U+%04lx].0x%lx "
                "instead of [U+%04lx..U+%04lx].0x%lx\n",
                name, variant, (long)start, (long)end, (long)value,
                (long)start, (long)expEnd, (long)expValue);
        return false;
    }
    return true;
}

// Test iteration starting from various UTF-8/16 and trie structure boundaries.
// Also test starting partway through lead & trail surrogates for fixed-surrogate-value options,
// and partway through supplementary code points.
static UChar32 iterStarts[] = {
    0, 0x7f, 0x80, 0x7ff, 0x800, 0xfff, 0x1000,
    0xd7ff, 0xd800, 0xd888, 0xdddd, 0xdfff, 0xe000,
    0xffff, 0x10000, 0x12345, 0x10ffff, 0x110000
};

static void
testTrieGetRanges(const char *testName, const UCPTrie *trie, const UMutableCPTrie *mutableTrie,
                  UCPMapRangeOption option, uint32_t surrValue,
                  const CheckRange checkRanges[], int32_t countCheckRanges) {
    const char *const typeName = trie == NULL ? "mutableTrie" : "trie";
    const char *const optionName = option == UCPMAP_RANGE_NORMAL ? "normal" :
        option == UCPMAP_RANGE_FIXED_LEAD_SURROGATES ? "fixedLeadSurr" : "fixedAllSurr";
    char name[80];
    int32_t s;
    for (s = 0; s < UPRV_LENGTHOF(iterStarts); ++s) {
        UChar32 start = iterStarts[s];
        int32_t i, i0;
        UChar32 end, expEnd;
        uint32_t value, expValue;
        // No need to go from each iteration start to the very end.
        int32_t innerLoopCount;

        snprintf(name, sizeof(name), "%s/%s(%s) min=U+%04lx", typeName, optionName, testName, (long)start);

        // Skip over special values and low ranges.
        for (i = 0; i < countCheckRanges && checkRanges[i].limit <= start; ++i) {}
        i0 = i;
        // without value handler
        for (innerLoopCount = 0;; ++i, start = end + 1) {
            if (i < countCheckRanges) {
                expEnd = checkRanges[i].limit - 1;
                expValue = checkRanges[i].value;
            } else {
                expEnd = -1;
                expValue = value = 0x5005;
            }
            end = trie != NULL ?
                ucptrie_getRange(trie, start, option, surrValue, NULL, NULL, &value) :
                umutablecptrie_getRange(mutableTrie, start, option, surrValue, NULL, NULL, &value);
            if (!doCheckRange(name, "without value handler", start, end, value, expEnd, expValue)) {
                break;
            }
            if (s != 0 && ++innerLoopCount == 5) { break; }
        }
        // with value handler
        for (i = i0, start = iterStarts[s], innerLoopCount = 0;; ++i, start = end + 1) {
            if (i < countCheckRanges) {
                expEnd = checkRanges[i].limit - 1;
                expValue = checkRanges[i].value ^ 0x5555;
            } else {
                expEnd = -1;
                expValue = value = 0x5005;
            }
            end = trie != NULL ?
                ucptrie_getRange(trie, start, option, surrValue ^ 0x5555, testFilter, NULL, &value) :
                umutablecptrie_getRange(mutableTrie, start, option, surrValue ^ 0x5555,
                                        testFilter, NULL, &value);
            if (!doCheckRange(name, "with value handler", start, end, value, expEnd, expValue)) {
                break;
            }
            if (s != 0 && ++innerLoopCount == 5) { break; }
        }
        // without value
        for (i = i0, start = iterStarts[s], innerLoopCount = 0;; ++i, start = end + 1) {
            if (i < countCheckRanges) {
                expEnd = checkRanges[i].limit - 1;
            } else {
                expEnd = -1;
            }
            end = trie != NULL ?
                ucptrie_getRange(trie, start, option, surrValue, NULL, NULL, NULL) :
                umutablecptrie_getRange(mutableTrie, start, option, surrValue, NULL, NULL, NULL);
            if (!doCheckRange(name, "without value", start, end, 0, expEnd, 0)) {
                break;
            }
            if (s != 0 && ++innerLoopCount == 5) { break; }
        }
    }
}

static void
testTrieGetters(const char *testName, const UCPTrie *trie,
                UCPTrieType type, UCPTrieValueWidth valueWidth,
                const CheckRange checkRanges[], int32_t countCheckRanges) {
    uint32_t initialValue, errorValue;
    uint32_t value, value2;
    UChar32 start, limit;
    int32_t i, countSpecials;
    int32_t countErrors=0;

    const char *const typeName = "trie";

    countSpecials=getSpecialValues(checkRanges, countCheckRanges, &initialValue, &errorValue);

    start=0;
    for(i=countSpecials; i<countCheckRanges; ++i) {
        limit=checkRanges[i].limit;
        value=checkRanges[i].value;

        while(start<limit) {
            if (start <= 0x7f) {
                if (valueWidth == UCPTRIE_VALUE_BITS_16) {
                    value2 = UCPTRIE_ASCII_GET(trie, UCPTRIE_16, start);
                } else if (valueWidth == UCPTRIE_VALUE_BITS_32) {
                    value2 = UCPTRIE_ASCII_GET(trie, UCPTRIE_32, start);
                } else {
                    value2 = UCPTRIE_ASCII_GET(trie, UCPTRIE_8, start);
                }
                if (value != value2) {
                    log_err("error: %s(%s).fromASCII(U+%04lx)==0x%lx instead of 0x%lx\n",
                            typeName, testName, (long)start, (long)value2, (long)value);
                    ++countErrors;
                }
            }
            if (type == UCPTRIE_TYPE_FAST) {
                if(start<=0xffff) {
                    if(valueWidth==UCPTRIE_VALUE_BITS_16) {
                        value2=UCPTRIE_FAST_BMP_GET(trie, UCPTRIE_16, start);
                    } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
                        value2=UCPTRIE_FAST_BMP_GET(trie, UCPTRIE_32, start);
                    } else {
                        value2=UCPTRIE_FAST_BMP_GET(trie, UCPTRIE_8, start);
                    }
                    if(value!=value2) {
                        log_err("error: %s(%s).fromBMP(U+%04lx)==0x%lx instead of 0x%lx\n",
                                typeName, testName, (long)start, (long)value2, (long)value);
                        ++countErrors;
                    }
                } else {
                    if(valueWidth==UCPTRIE_VALUE_BITS_16) {
                        value2 = UCPTRIE_FAST_SUPP_GET(trie, UCPTRIE_16, start);
                    } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
                        value2 = UCPTRIE_FAST_SUPP_GET(trie, UCPTRIE_32, start);
                    } else {
                        value2 = UCPTRIE_FAST_SUPP_GET(trie, UCPTRIE_8, start);
                    }
                    if(value!=value2) {
                        log_err("error: %s(%s).fromSupp(U+%04lx)==0x%lx instead of 0x%lx\n",
                                typeName, testName, (long)start, (long)value2, (long)value);
                        ++countErrors;
                    }
                }
                if(valueWidth==UCPTRIE_VALUE_BITS_16) {
                    value2 = UCPTRIE_FAST_GET(trie, UCPTRIE_16, start);
                } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
                    value2 = UCPTRIE_FAST_GET(trie, UCPTRIE_32, start);
                } else {
                    value2 = UCPTRIE_FAST_GET(trie, UCPTRIE_8, start);
                }
            } else {
                if(valueWidth==UCPTRIE_VALUE_BITS_16) {
                    value2 = UCPTRIE_SMALL_GET(trie, UCPTRIE_16, start);
                } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
                    value2 = UCPTRIE_SMALL_GET(trie, UCPTRIE_32, start);
                } else {
                    value2 = UCPTRIE_SMALL_GET(trie, UCPTRIE_8, start);
                }
            }
            if(value!=value2) {
                log_err("error: %s(%s).get(U+%04lx)==0x%lx instead of 0x%lx\n",
                        typeName, testName, (long)start, (long)value2, (long)value);
                ++countErrors;
            }
            value2=ucptrie_get(trie, start);
            if(value!=value2) {
                log_err("error: %s(%s).get(U+%04lx)==0x%lx instead of 0x%lx\n",
                        typeName, testName, (long)start, (long)value2, (long)value);
                ++countErrors;
            }
            ++start;
            if(countErrors>10) {
                return;
            }
        }
    }

    /* test linear ASCII range from the data array pointer (access to "internal" field) */
    start=0;
    for(i=countSpecials; i<countCheckRanges && start<=0x7f; ++i) {
        limit=checkRanges[i].limit;
        value=checkRanges[i].value;

        while(start<limit && start<=0x7f) {
            if(valueWidth==UCPTRIE_VALUE_BITS_16) {
                value2=trie->data.ptr16[start];
            } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
                value2=trie->data.ptr32[start];
            } else {
                value2=trie->data.ptr8[start];
            }
            if(value!=value2) {
                log_err("error: %s(%s).asciiData[U+%04lx]==0x%lx instead of 0x%lx\n",
                        typeName, testName, (long)start, (long)value2, (long)value);
                ++countErrors;
            }
            ++start;
            if(countErrors>10) {
                return;
            }
        }
    }

    /* test errorValue */
    if (type == UCPTRIE_TYPE_FAST) {
        if(valueWidth==UCPTRIE_VALUE_BITS_16) {
            value = UCPTRIE_FAST_GET(trie, UCPTRIE_16, -1);
            value2 = UCPTRIE_FAST_GET(trie, UCPTRIE_16, 0x110000);
        } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
            value = UCPTRIE_FAST_GET(trie, UCPTRIE_32, -1);
            value2 = UCPTRIE_FAST_GET(trie, UCPTRIE_32, 0x110000);
        } else {
            value = UCPTRIE_FAST_GET(trie, UCPTRIE_8, -1);
            value2 = UCPTRIE_FAST_GET(trie, UCPTRIE_8, 0x110000);
        }
    } else {
        if(valueWidth==UCPTRIE_VALUE_BITS_16) {
            value = UCPTRIE_SMALL_GET(trie, UCPTRIE_16, -1);
            value2 = UCPTRIE_SMALL_GET(trie, UCPTRIE_16, 0x110000);
        } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
            value = UCPTRIE_SMALL_GET(trie, UCPTRIE_32, -1);
            value2 = UCPTRIE_SMALL_GET(trie, UCPTRIE_32, 0x110000);
        } else {
            value = UCPTRIE_SMALL_GET(trie, UCPTRIE_8, -1);
            value2 = UCPTRIE_SMALL_GET(trie, UCPTRIE_8, 0x110000);
        }
    }
    if(value!=errorValue || value2!=errorValue) {
        log_err("error: %s(%s).get(out of range) != errorValue\n",
                typeName, testName);
    }
    value=ucptrie_get(trie, -1);
    value2=ucptrie_get(trie, 0x110000);
    if(value!=errorValue || value2!=errorValue) {
        log_err("error: %s(%s).get(out of range) != errorValue\n",
                typeName, testName);
    }
}

static void
testBuilderGetters(const char *testName, const UMutableCPTrie *mutableTrie,
                   const CheckRange checkRanges[], int32_t countCheckRanges) {
    uint32_t initialValue, errorValue;
    uint32_t value, value2;
    UChar32 start, limit;
    int32_t i, countSpecials;
    int32_t countErrors=0;

    const char *const typeName = "mutableTrie";

    countSpecials=getSpecialValues(checkRanges, countCheckRanges, &initialValue, &errorValue);

    start=0;
    for(i=countSpecials; i<countCheckRanges; ++i) {
        limit=checkRanges[i].limit;
        value=checkRanges[i].value;

        while(start<limit) {
            value2=umutablecptrie_get(mutableTrie, start);
            if(value!=value2) {
                log_err("error: %s(%s).get(U+%04lx)==0x%lx instead of 0x%lx\n",
                        typeName, testName, (long)start, (long)value2, (long)value);
                ++countErrors;
            }
            ++start;
            if(countErrors>10) {
                return;
            }
        }
    }

    /* test errorValue */
    value=umutablecptrie_get(mutableTrie, -1);
    value2=umutablecptrie_get(mutableTrie, 0x110000);
    if(value!=errorValue || value2!=errorValue) {
        log_err("error: %s(%s).get(out of range) != errorValue\n",
                typeName, testName);
    }
}

#define ACCIDENTAL_SURROGATE_PAIR(s, length, cp) (length > 0 && U16_IS_LEAD(s[length-1]) && U_IS_TRAIL(cp))

static void
testTrieUTF16(const char *testName,
              const UCPTrie *trie, UCPTrieValueWidth valueWidth,
              const CheckRange checkRanges[], int32_t countCheckRanges) {
    UChar s[30000];
    uint32_t values[16000];

    const UChar *p, *limit;

    uint32_t errorValue = ucptrie_get(trie, -1);
    uint32_t value, expected;
    UChar32 prevCP, c, c2;
    int32_t i, length, sIndex, countValues;

    /* write a string */
    prevCP=0;
    length=countValues=0;
    for(i=skipSpecialValues(checkRanges, countCheckRanges); i<countCheckRanges; ++i) {
        value=checkRanges[i].value;
        /* write three code points */
        if(!ACCIDENTAL_SURROGATE_PAIR(s, length, prevCP)) {
            U16_APPEND_UNSAFE(s, length, prevCP);   /* start of the range */
            values[countValues++]=value;
        }
        U_ASSERT(length < UPRV_LENGTHOF(s) && countValues < UPRV_LENGTHOF(values));
        c=checkRanges[i].limit;
        prevCP=(prevCP+c)/2;                    /* middle of the range */
        if(!ACCIDENTAL_SURROGATE_PAIR(s, length, prevCP)) {
            U16_APPEND_UNSAFE(s, length, prevCP);
            values[countValues++]=value;
        }
        prevCP=c;
        --c;                                    /* end of the range */
        if(!ACCIDENTAL_SURROGATE_PAIR(s, length, c)) {
            U16_APPEND_UNSAFE(s, length, c);
            values[countValues++]=value;
        }
    }
    limit=s+length;
    if(length>UPRV_LENGTHOF(s)) {
        log_err("UTF-16 test string length %d > capacity %d\n", (int)length, (int)UPRV_LENGTHOF(s));
        return;
    }
    if(countValues>UPRV_LENGTHOF(values)) {
        log_err("UTF-16 test values length %d > capacity %d\n", (int)countValues, (int)UPRV_LENGTHOF(values));
        return;
    }

    /* try forward */
    p=s;
    i=0;
    while(p<limit) {
        sIndex=(int32_t)(p-s);
        U16_NEXT(s, sIndex, length, c2);
        c=0x33;
        if(valueWidth==UCPTRIE_VALUE_BITS_16) {
            UCPTRIE_FAST_U16_NEXT(trie, UCPTRIE_16, p, limit, c, value);
        } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
            UCPTRIE_FAST_U16_NEXT(trie, UCPTRIE_32, p, limit, c, value);
        } else {
            UCPTRIE_FAST_U16_NEXT(trie, UCPTRIE_8, p, limit, c, value);
        }
        expected = U_IS_SURROGATE(c) ? errorValue : values[i];
        if(value!=expected) {
            log_err("error: wrong value from UCPTRIE_NEXT(%s)(U+%04lx): 0x%lx instead of 0x%lx\n",
                    testName, (long)c, (long)value, (long)expected);
        }
        if(c!=c2) {
            log_err("error: wrong code point from UCPTRIE_NEXT(%s): U+%04lx != U+%04lx\n",
                    testName, (long)c, (long)c2);
            continue;
        }
        ++i;
    }

    /* try backward */
    p=limit;
    i=countValues;
    while(s<p) {
        --i;
        sIndex=(int32_t)(p-s);
        U16_PREV(s, 0, sIndex, c2);
        c=0x33;
        if(valueWidth==UCPTRIE_VALUE_BITS_16) {
            UCPTRIE_FAST_U16_PREV(trie, UCPTRIE_16, s, p, c, value);
        } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
            UCPTRIE_FAST_U16_PREV(trie, UCPTRIE_32, s, p, c, value);
        } else {
            UCPTRIE_FAST_U16_PREV(trie, UCPTRIE_8, s, p, c, value);
        }
        expected = U_IS_SURROGATE(c) ? errorValue : values[i];
        if(value!=expected) {
            log_err("error: wrong value from UCPTRIE_PREV(%s)(U+%04lx): 0x%lx instead of 0x%lx\n",
                    testName, (long)c, (long)value, (long)expected);
        }
        if(c!=c2) {
            log_err("error: wrong code point from UCPTRIE_PREV(%s): U+%04lx != U+%04lx\n",
                    testName, c, c2);
        }
    }
}

static void
testTrieUTF8(const char *testName,
             const UCPTrie *trie, UCPTrieValueWidth valueWidth,
             const CheckRange checkRanges[], int32_t countCheckRanges) {
    // Note: The byte sequence comments refer to the original UTF-8 definition.
    // Starting with ICU 60, any sequence that is not a prefix of a valid one
    // is treated as multiple single-byte errors.
    // For testing, we only rely on U8_... and UCPTrie UTF-8 macros
    // iterating consistently.
    static const uint8_t illegal[]={
        0xc0, 0x80,                         /* non-shortest U+0000 */
        0xc1, 0xbf,                         /* non-shortest U+007f */
        0xc2,                               /* truncated */
        0xe0, 0x90, 0x80,                   /* non-shortest U+0400 */
        0xe0, 0xa0,                         /* truncated */
        0xed, 0xa0, 0x80,                   /* lead surrogate U+d800 */
        0xed, 0xbf, 0xbf,                   /* trail surrogate U+dfff */
        0xf0, 0x8f, 0xbf, 0xbf,             /* non-shortest U+ffff */
        0xf0, 0x90, 0x80,                   /* truncated */
        0xf4, 0x90, 0x80, 0x80,             /* beyond-Unicode U+110000 */
        0xf8, 0x80, 0x80, 0x80,             /* truncated */
        0xf8, 0x80, 0x80, 0x80, 0x80,       /* 5-byte UTF-8 */
        0xfd, 0xbf, 0xbf, 0xbf, 0xbf,       /* truncated */
        0xfd, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, /* 6-byte UTF-8 */
        0xfe,
        0xff
    };
    uint8_t s[60000];
    uint32_t values[16000];

    const uint8_t *p, *limit;

    uint32_t initialValue, errorValue;
    uint32_t value, expectedBytes, actualBytes;
    UChar32 prevCP, c;
    int32_t i, countSpecials, length, countValues;
    int32_t prev8, i8;

    countSpecials=getSpecialValues(checkRanges, countCheckRanges, &initialValue, &errorValue);

    /* write a string */
    prevCP=0;
    length=countValues=0;
    /* first a couple of trail bytes in lead position */
    s[length++]=0x80;
    values[countValues++]=errorValue;
    s[length++]=0xbf;
    values[countValues++]=errorValue;
    prev8=i8=0;
    for(i=countSpecials; i<countCheckRanges; ++i) {
        value=checkRanges[i].value;
        /* write three legal (or surrogate) code points */
        U8_APPEND_UNSAFE(s, length, prevCP);    /* start of the range */
        if(U_IS_SURROGATE(prevCP)) {
            // A surrogate byte sequence counts as 3 single-byte errors.
            values[countValues++]=errorValue;
            values[countValues++]=errorValue;
            values[countValues++]=errorValue;
        } else {
            values[countValues++]=value;
        }
        U_ASSERT(length < UPRV_LENGTHOF(s) && countValues < UPRV_LENGTHOF(values));
        c=checkRanges[i].limit;
        prevCP=(prevCP+c)/2;                    /* middle of the range */
        U8_APPEND_UNSAFE(s, length, prevCP);
        if(U_IS_SURROGATE(prevCP)) {
            // A surrogate byte sequence counts as 3 single-byte errors.
            values[countValues++]=errorValue;
            values[countValues++]=errorValue;
            values[countValues++]=errorValue;
        } else {
            values[countValues++]=value;
        }
        prevCP=c;
        --c;                                    /* end of the range */
        U8_APPEND_UNSAFE(s, length, c);
        if(U_IS_SURROGATE(c)) {
            // A surrogate byte sequence counts as 3 single-byte errors.
            values[countValues++]=errorValue;
            values[countValues++]=errorValue;
            values[countValues++]=errorValue;
        } else {
            values[countValues++]=value;
        }
        /* write an illegal byte sequence */
        if(i8<(int32_t)sizeof(illegal)) {
            U8_FWD_1(illegal, i8, sizeof(illegal));
            while(prev8<i8) {
                s[length++]=illegal[prev8++];
            }
            values[countValues++]=errorValue;
        }
    }
    /* write the remaining illegal byte sequences */
    while(i8<(int32_t)sizeof(illegal)) {
        U8_FWD_1(illegal, i8, sizeof(illegal));
        while(prev8<i8) {
            s[length++]=illegal[prev8++];
        }
        values[countValues++]=errorValue;
    }
    limit=s+length;
    if(length>UPRV_LENGTHOF(s)) {
        log_err("UTF-8 test string length %d > capacity %d\n", (int)length, (int)UPRV_LENGTHOF(s));
        return;
    }
    if(countValues>UPRV_LENGTHOF(values)) {
        log_err("UTF-8 test values length %d > capacity %d\n", (int)countValues, (int)UPRV_LENGTHOF(values));
        return;
    }

    /* try forward */
    p=s;
    i=0;
    while(p<limit) {
        prev8=i8=(int32_t)(p-s);
        U8_NEXT(s, i8, length, c);
        if(valueWidth==UCPTRIE_VALUE_BITS_16) {
            UCPTRIE_FAST_U8_NEXT(trie, UCPTRIE_16, p, limit, value);
        } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
            UCPTRIE_FAST_U8_NEXT(trie, UCPTRIE_32, p, limit, value);
        } else {
            UCPTRIE_FAST_U8_NEXT(trie, UCPTRIE_8, p, limit, value);
        }
        expectedBytes=0;
        if(value!=values[i] || i8!=(p-s)) {
            int32_t k=prev8;
            while(k<i8) {
                expectedBytes=(expectedBytes<<8)|s[k++];
            }
        }
        if(i8==(p-s)) {
            actualBytes=expectedBytes;
        } else {
            actualBytes=0;
            int32_t k=prev8;
            while(k<(p-s)) {
                actualBytes=(actualBytes<<8)|s[k++];
            }
        }
        if(value!=values[i]) {
            log_err("error: wrong value from UCPTRIE_FAST_U8_NEXT(%s)(from %d %lx->U+%04lx) (read %d bytes): "
                    "0x%lx instead of 0x%lx (from bytes %lx)\n",
                    testName, (int)prev8, (unsigned long)actualBytes, (long)c, (int)((p-s)-prev8),
                    (long)value, (long)values[i], (unsigned long)expectedBytes);
        }
        if(i8!=(p-s)) {
            log_err("error: wrong end index from UCPTRIE_FAST_U8_NEXT(%s)(from %d %lx->U+%04lx): "
                    "%ld != %ld (bytes %lx)\n",
                    testName, (int)prev8, (unsigned long)actualBytes, (long)c,
                    (long)(p-s), (long)i8, (unsigned long)expectedBytes);
            break;
        }
        ++i;
    }

    /* try backward */
    p=limit;
    i=countValues;
    while(s<p) {
        --i;
        prev8=i8=(int32_t)(p-s);
        U8_PREV(s, 0, i8, c);
        if(valueWidth==UCPTRIE_VALUE_BITS_16) {
            UCPTRIE_FAST_U8_PREV(trie, UCPTRIE_16, s, p, value);
        } else if(valueWidth==UCPTRIE_VALUE_BITS_32) {
            UCPTRIE_FAST_U8_PREV(trie, UCPTRIE_32, s, p, value);
        } else {
            UCPTRIE_FAST_U8_PREV(trie, UCPTRIE_8, s, p, value);
        }
        expectedBytes=0;
        if(value!=values[i] || i8!=(p-s)) {
            int32_t k=i8;
            while(k<prev8) {
                expectedBytes=(expectedBytes<<8)|s[k++];
            }
        }
        if(i8==(p-s)) {
            actualBytes=expectedBytes;
        } else {
            actualBytes=0;
            int32_t k=(int32_t)(p-s);
            while(k<prev8) {
                actualBytes=(actualBytes<<8)|s[k++];
            }
        }
        if(value!=values[i]) {
            log_err("error: wrong value from UCPTRIE_FAST_U8_PREV(%s)(from %d %lx->U+%04lx) (read %d bytes): "
                    "0x%lx instead of 0x%lx (from bytes %lx)\n",
                    testName, (int)prev8, (unsigned long)actualBytes, (long)c, (int)(prev8-(p-s)),
                    (long)value, (long)values[i], (unsigned long)expectedBytes);
        }
        if(i8!=(p-s)) {
            log_err("error: wrong end index from UCPTRIE_FAST_U8_PREV(%s)(from %d %lx->U+%04lx): "
                    "%ld != %ld (bytes %lx)\n",
                    testName, (int)prev8, (unsigned long)actualBytes, (long)c,
                    (long)(p-s), (long)i8, (unsigned long)expectedBytes);
            break;
        }
    }
}

static void
testTrie(const char *testName, const UCPTrie *trie,
         UCPTrieType type, UCPTrieValueWidth valueWidth,
         const CheckRange checkRanges[], int32_t countCheckRanges) {
    testTrieGetters(testName, trie, type, valueWidth, checkRanges, countCheckRanges);
    testTrieGetRanges(testName, trie, NULL, UCPMAP_RANGE_NORMAL, 0, checkRanges, countCheckRanges);
    if (type == UCPTRIE_TYPE_FAST) {
        testTrieUTF16(testName, trie, valueWidth, checkRanges, countCheckRanges);
        testTrieUTF8(testName, trie, valueWidth, checkRanges, countCheckRanges);
    }
}

static void
testBuilder(const char *testName, const UMutableCPTrie *mutableTrie,
            const CheckRange checkRanges[], int32_t countCheckRanges) {
    testBuilderGetters(testName, mutableTrie, checkRanges, countCheckRanges);
    testTrieGetRanges(testName, NULL, mutableTrie, UCPMAP_RANGE_NORMAL, 0, checkRanges, countCheckRanges);
}

static void
trieTestGolden(const char *testName,
             const UCPTrie* trie,
             const CheckRange checkRanges[],
             int32_t countCheckRanges) {
    log_verbose("golden testing Trie '%s'\n", testName);

    UErrorCode status = U_ZERO_ERROR;
    const char *testdatapath = loadSourceTestData(&status);
    char goldendatapath[512];
    // note: snprintf always writes a NUL terminator.
    snprintf(goldendatapath, sizeof(goldendatapath), "%scodepointtrie%s%s.toml",
        testdatapath, U_FILE_SEP_STRING, testName);

    // Write the data into a tmpfile (memstream is not portable)
    FILE* stream = tmpfile();
    usrc_writeCopyrightHeader(stream, "#", 2021);
    usrc_writeFileNameGeneratedBy(stream, "#", testName, "ucptrietest.c");
    fputs("[code_point_trie.struct]\n", stream);
    fprintf(stream, "name = \"%s\"\n", testName);
    usrc_writeUCPTrie(stream, testName, trie, UPRV_TARGET_SYNTAX_TOML);
    fputs("\n[code_point_trie.testdata]\n", stream);
    fputs("# Array of (limit, value) pairs\n", stream);
    usrc_writeArray(stream, "checkRanges = [\n  ",
        // Note: CheckRange is a tuple of two 32-bit words
        checkRanges, 32, countCheckRanges*2,
        "  ", "\n]\n");

    // Convert the stream into a memory buffer
    long fsize = ftell(stream);
    void* memoryBuffer = malloc(fsize + 1);
    if (memoryBuffer == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        log_err_status(status, "Could not allocate buffer: %s", goldendatapath);
        goto cleanup;
    }
    fseek(stream, 0, SEEK_SET);
    fread(memoryBuffer, 1, fsize, stream);

    int32_t testResult = uprv_compareGoldenFiles(
        memoryBuffer, fsize,
        goldendatapath,
        getTestOption(WRITE_GOLDEN_DATA_OPTION));

    if (testResult >= 0) {
        log_err(
            "Golden files for '%s' differ at index %d; "
            "run cintltst with -G to write new goldens",
            testName, testResult);
    }

cleanup:
    fclose(stream);
    free(memoryBuffer);
}

static uint32_t storage[120000];
static uint32_t swapped[120000];

static void
testTrieSerialize(const char *testName, UMutableCPTrie *mutableTrie,
                  UCPTrieType type, UCPTrieValueWidth valueWidth, UBool withSwap,
                  const CheckRange checkRanges[], int32_t countCheckRanges) {
    UCPTrie *trie;
    int32_t length1, length2, length3;
    UErrorCode errorCode;

    /* clone the trie so that the caller can reuse the original */
    errorCode=U_ZERO_ERROR;
    mutableTrie = umutablecptrie_clone(mutableTrie, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_clone(%s) failed - %s\n",
                testName, u_errorName(errorCode));
        return;
    }

    /*
     * This is not a loop, but simply a block that we can exit with "break"
     * when something goes wrong.
     */
    do {
        errorCode=U_ZERO_ERROR;
        trie = umutablecptrie_buildImmutable(mutableTrie, type, valueWidth, &errorCode);
        if (U_FAILURE(errorCode)) {
            log_err("error: umutablecptrie_buildImmutable(%s) failed: %s\n",
                    testName, u_errorName(errorCode));
            break;
        }
        errorCode=U_ZERO_ERROR;
        length1=ucptrie_toBinary(trie, NULL, 0, &errorCode);
        if(errorCode!=U_BUFFER_OVERFLOW_ERROR) {
            log_err("error: ucptrie_toBinary(%s) preflighting set %s != U_BUFFER_OVERFLOW_ERROR\n",
                    testName, u_errorName(errorCode));
            break;
        }
        errorCode=U_ZERO_ERROR;
        length2=ucptrie_toBinary(trie, storage, sizeof(storage), &errorCode);
        if(errorCode==U_BUFFER_OVERFLOW_ERROR) {
            log_err("error: ucptrie_toBinary(%s) needs more memory\n", testName);
            break;
        }
        if(U_FAILURE(errorCode)) {
            log_err("error: ucptrie_toBinary(%s) failed: %s\n", testName, u_errorName(errorCode));
            break;
        }
        if(length1!=length2) {
            log_err("error: trie serialization (%s) lengths different: "
                    "preflight vs. serialize\n", testName);
            break;
        }

        testTrie(testName, trie, type, valueWidth, checkRanges, countCheckRanges);

        // Compare the tries with golden data, also used for ICU4X
        // Don't print out cloning tests (redundant data)
        // Don't print out stress tests (file size too large)
        // Omit some of the short-all-same tests (~25KB apiece)
        if (!withSwap &&
                uprv_strncmp("many-", testName, 5) != 0 &&
                uprv_strncmp("much-", testName, 5) != 0 &&
                uprv_strncmp("short-all-same.16", testName, 17) != 0 &&
                uprv_strncmp("short-all-same.32", testName, 17) != 0) {
            trieTestGolden(testName, trie, checkRanges, countCheckRanges);
        }

        ucptrie_close(trie);
        trie=NULL;

        if(withSwap) {
            int32_t swappedLength;

            UDataSwapper *ds;

            /* swap to opposite-endian */
            uprv_memset(swapped, 0x55, length2);
            ds=udata_openSwapper(U_IS_BIG_ENDIAN, U_CHARSET_FAMILY,
                                 !U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, &errorCode);
            swappedLength=ucptrie_swap(ds, storage, -1, NULL, &errorCode);
            if(U_FAILURE(errorCode) || swappedLength!=length2) {
                log_err("error: ucptrie_swap(%s to OE preflighting) failed (%s) "
                        "or before/after lengths different\n",
                        testName, u_errorName(errorCode));
                udata_closeSwapper(ds);
                break;
            }
            swappedLength=ucptrie_swap(ds, storage, length2, swapped, &errorCode);
            udata_closeSwapper(ds);
            if(U_FAILURE(errorCode) || swappedLength!=length2) {
                log_err("error: ucptrie_swap(%s to OE) failed (%s) or before/after lengths different\n",
                        testName, u_errorName(errorCode));
                break;
            }

            /* swap back to platform-endian */
            uprv_memset(storage, 0xaa, length2);
            ds=udata_openSwapper(!U_IS_BIG_ENDIAN, U_CHARSET_FAMILY,
                                 U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, &errorCode);
            swappedLength=ucptrie_swap(ds, swapped, -1, NULL, &errorCode);
            if(U_FAILURE(errorCode) || swappedLength!=length2) {
                log_err("error: ucptrie_swap(%s to PE preflighting) failed (%s) "
                        "or before/after lengths different\n",
                        testName, u_errorName(errorCode));
                udata_closeSwapper(ds);
                break;
            }
            swappedLength=ucptrie_swap(ds, swapped, length2, storage, &errorCode);
            udata_closeSwapper(ds);
            if(U_FAILURE(errorCode) || swappedLength!=length2) {
                log_err("error: ucptrie_swap(%s to PE) failed (%s) or before/after lengths different\n",
                        testName, u_errorName(errorCode));
                break;
            }
        }

        trie = ucptrie_openFromBinary(type, valueWidth, storage, length2, &length3, &errorCode);
        if(U_FAILURE(errorCode)) {
            log_err("error: ucptrie_openFromBinary(%s) failed, %s\n", testName, u_errorName(errorCode));
            break;
        }
        if(type != ucptrie_getType(trie)) {
            log_err("error: trie serialization (%s) did not preserve trie type\n", testName);
            break;
        }
        if(valueWidth != ucptrie_getValueWidth(trie)) {
            log_err("error: trie serialization (%s) did not preserve data value width\n", testName);
            break;
        }
        if(length2!=length3) {
            log_err("error: trie serialization (%s) lengths different: "
                    "serialize vs. unserialize\n", testName);
            break;
        }
        /* overwrite the storage that is not supposed to be needed */
        uprv_memset((char *)storage+length3, 0xfa, (int32_t)(sizeof(storage)-length3));

        {
            errorCode=U_ZERO_ERROR;
            UCPTrie *any = ucptrie_openFromBinary(UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY,
                                                  storage, length3, NULL, &errorCode);
            if (U_SUCCESS(errorCode)) {
                if (type != ucptrie_getType(any)) {
                    log_err("error: ucptrie_openFromBinary("
                            "UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY).getType() wrong\n");
                }
                if (valueWidth != ucptrie_getValueWidth(any)) {
                    log_err("error: ucptrie_openFromBinary("
                            "UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY).getValueWidth() wrong\n");
                }
                ucptrie_close(any);
            } else {
                log_err("error: ucptrie_openFromBinary("
                        "UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY) failed - %s\n",
                        u_errorName(errorCode));
            }
        }

        errorCode=U_ZERO_ERROR;
        testTrie(testName, trie, type, valueWidth, checkRanges, countCheckRanges);
        {
            /* make a mutable trie from an immutable one */
            uint32_t value, value2;
            UMutableCPTrie *mutable2 = umutablecptrie_fromUCPTrie(trie, &errorCode);
            if(U_FAILURE(errorCode)) {
                log_err("error: umutablecptrie_fromUCPTrie(unserialized %s) failed - %s\n",
                        testName, u_errorName(errorCode));
                break;
            }

            value=umutablecptrie_get(mutable2, 0xa1);
            umutablecptrie_set(mutable2, 0xa1, 789, &errorCode);
            value2=umutablecptrie_get(mutable2, 0xa1);
            umutablecptrie_set(mutable2, 0xa1, value, &errorCode);
            if(U_FAILURE(errorCode) || value2!=789) {
                log_err("error: modifying a mutableTrie-from-UCPTrie (%s) failed - %s\n",
                        testName, u_errorName(errorCode));
            }
            testBuilder(testName, mutable2, checkRanges, countCheckRanges);
            umutablecptrie_close(mutable2);
        }
    } while(0);

    umutablecptrie_close(mutableTrie);
    ucptrie_close(trie);
}

static UMutableCPTrie *
testTrieSerializeAllValueWidth(const char *testName,
                               UMutableCPTrie *mutableTrie, UBool withClone,
                               const CheckRange checkRanges[], int32_t countCheckRanges) {
    char name[40];
    uint32_t oredValues = 0;
    int32_t i;
    for (i = 0; i < countCheckRanges; ++i) {
        oredValues |= checkRanges[i].value;
    }

    testBuilder(testName, mutableTrie, checkRanges, countCheckRanges);

    if (oredValues <= 0xffff) {
        uprv_strcpy(name, testName);
        uprv_strcat(name, ".16");
        testTrieSerialize(name, mutableTrie,
                          UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16, withClone,
                          checkRanges, countCheckRanges);
    }

    uprv_strcpy(name, testName);
    uprv_strcat(name, ".32");
    testTrieSerialize(name, mutableTrie,
                      UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_32, withClone,
                      checkRanges, countCheckRanges);

    if (oredValues <= 0xff) {
        uprv_strcpy(name, testName);
        uprv_strcat(name, ".8");
        testTrieSerialize(name, mutableTrie,
                          UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_8, withClone,
                          checkRanges, countCheckRanges);
    }

    if (oredValues <= 0xffff) {
        uprv_strcpy(name, testName);
        uprv_strcat(name, ".small16");
        testTrieSerialize(name, mutableTrie,
                          UCPTRIE_TYPE_SMALL, UCPTRIE_VALUE_BITS_16, withClone,
                          checkRanges, countCheckRanges);
    }

    return mutableTrie;
}

static UMutableCPTrie *
makeTrieWithRanges(const char *testName, UBool withClone,
                   const SetRange setRanges[], int32_t countSetRanges,
                   uint32_t initialValue, uint32_t errorValue) {
    UMutableCPTrie *mutableTrie;
    uint32_t value;
    UChar32 start, limit;
    int32_t i;
    UErrorCode errorCode;

    log_verbose("testing Trie '%s'\n", testName);
    errorCode=U_ZERO_ERROR;
    mutableTrie = umutablecptrie_open(initialValue, errorValue, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_open(%s) failed: %s\n", testName, u_errorName(errorCode));
        return NULL;
    }

    /* set values from setRanges[] */
    for(i=0; i<countSetRanges; ++i) {
        if(withClone && i==countSetRanges/2) {
            /* switch to a clone in the middle of setting values */
            UMutableCPTrie *clone = umutablecptrie_clone(mutableTrie, &errorCode);
            if(U_FAILURE(errorCode)) {
                log_err("error: umutablecptrie_clone(%s) failed - %s\n",
                        testName, u_errorName(errorCode));
                errorCode=U_ZERO_ERROR;  /* continue with the original */
            } else {
                umutablecptrie_close(mutableTrie);
                mutableTrie = clone;
            }
        }
        start=setRanges[i].start;
        limit=setRanges[i].limit;
        value=setRanges[i].value;
        if ((limit - start) == 1) {
            umutablecptrie_set(mutableTrie, start, value, &errorCode);
        } else {
            umutablecptrie_setRange(mutableTrie, start, limit-1, value, &errorCode);
        }
    }

    if(U_SUCCESS(errorCode)) {
        return mutableTrie;
    } else {
        log_err("error: setting values into a mutable trie (%s) failed - %s\n",
                testName, u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return NULL;
    }
}

static void
testTrieRanges(const char *testName, UBool withClone,
               const SetRange setRanges[], int32_t countSetRanges,
               const CheckRange checkRanges[], int32_t countCheckRanges) {
    uint32_t initialValue, errorValue;
    getSpecialValues(checkRanges, countCheckRanges, &initialValue, &errorValue);
    UMutableCPTrie *mutableTrie = makeTrieWithRanges(
        testName, withClone, setRanges, countSetRanges, initialValue, errorValue);
    if (mutableTrie != NULL) {
        mutableTrie = testTrieSerializeAllValueWidth(testName, mutableTrie, withClone,
                                                     checkRanges, countCheckRanges);
        umutablecptrie_close(mutableTrie);
    }
}

/* test data ----------------------------------------------------------------*/

/* set consecutive ranges, even with value 0 */
static const SetRange
setRanges1[]={
    { 0,        0x40,     0    },
    { 0x40,     0xe7,     0x34 },
    { 0xe7,     0x3400,   0    },
    { 0x3400,   0x9fa6,   0x61 },
    { 0x9fa6,   0xda9e,   0x31 },
    { 0xdada,   0xeeee,   0xff },
    { 0xeeee,   0x11111,  1    },
    { 0x11111,  0x44444,  0x61 },
    { 0x44444,  0x60003,  0    },
    { 0xf0003,  0xf0004,  0xf  },
    { 0xf0004,  0xf0006,  0x10 },
    { 0xf0006,  0xf0007,  0x11 },
    { 0xf0007,  0xf0040,  0x12 },
    { 0xf0040,  0x110000, 0    }
};

static const CheckRange
checkRanges1[]={
    { 0,        0 },
    { 0x40,     0 },
    { 0xe7,     0x34 },
    { 0x3400,   0 },
    { 0x9fa6,   0x61 },
    { 0xda9e,   0x31 },
    { 0xdada,   0 },
    { 0xeeee,   0xff },
    { 0x11111,  1 },
    { 0x44444,  0x61 },
    { 0xf0003,  0 },
    { 0xf0004,  0xf },
    { 0xf0006,  0x10 },
    { 0xf0007,  0x11 },
    { 0xf0040,  0x12 },
    { 0x110000, 0 }
};

/* set some interesting overlapping ranges */
static const SetRange
setRanges2[]={
    { 0x21,     0x7f,     0x5555 },
    { 0x2f800,  0x2fedc,  0x7a   },
    { 0x72,     0xdd,     3      },
    { 0xdd,     0xde,     4      },
    { 0x201,    0x240,    6      },  /* 3 consecutive blocks with the same pattern but */
    { 0x241,    0x280,    6      },  /* discontiguous value ranges, testing iteration */
    { 0x281,    0x2c0,    6      },
    { 0x2f987,  0x2fa98,  5      },
    { 0x2f777,  0x2f883,  0      },
    { 0x2fedc,  0x2ffaa,  1      },
    { 0x2ffaa,  0x2ffab,  2      },
    { 0x2ffbb,  0x2ffc0,  7      }
};

static const CheckRange
checkRanges2[]={
    { 0,        0 },
    { 0x21,     0 },
    { 0x72,     0x5555 },
    { 0xdd,     3 },
    { 0xde,     4 },
    { 0x201,    0 },
    { 0x240,    6 },
    { 0x241,    0 },
    { 0x280,    6 },
    { 0x281,    0 },
    { 0x2c0,    6 },
    { 0x2f883,  0 },
    { 0x2f987,  0x7a },
    { 0x2fa98,  5 },
    { 0x2fedc,  0x7a },
    { 0x2ffaa,  1 },
    { 0x2ffab,  2 },
    { 0x2ffbb,  0 },
    { 0x2ffc0,  7 },
    { 0x110000, 0 }
};

/* use a non-zero initial value */
static const SetRange
setRanges3[]={
    { 0x31,     0xa4,     1 },
    { 0x3400,   0x6789,   2 },
    { 0x8000,   0x89ab,   9 },
    { 0x9000,   0xa000,   4 },
    { 0xabcd,   0xbcde,   3 },
    { 0x55555,  0x110000, 6 },  /* highStart<U+ffff with non-initialValue */
    { 0xcccc,   0x55555,  6 }
};

static const CheckRange
checkRanges3[]={
    { 0,        9 },  /* non-zero initialValue */
    { 0x31,     9 },
    { 0xa4,     1 },
    { 0x3400,   9 },
    { 0x6789,   2 },
    { 0x9000,   9 },
    { 0xa000,   4 },
    { 0xabcd,   9 },
    { 0xbcde,   3 },
    { 0xcccc,   9 },
    { 0x110000, 6 }
};

/* empty or single-value tries, testing highStart==0 */
static const SetRange
setRangesEmpty[]={
    { 0,        0,        0 },  /* need some values for it to compile */
};

static const CheckRange
checkRangesEmpty[]={
    { 0,        3 },
    { 0x110000, 3 }
};

static const SetRange
setRangesSingleValue[]={
    { 0,        0x110000, 5 },
};

static const CheckRange
checkRangesSingleValue[]={
    { 0,        3 },
    { 0x110000, 5 }
};

static void
TrieTestSet1(void) {
    testTrieRanges("set1", false,
        setRanges1, UPRV_LENGTHOF(setRanges1),
        checkRanges1, UPRV_LENGTHOF(checkRanges1));
}

static void
TrieTestSet2Overlap(void) {
    testTrieRanges("set2-overlap", false,
        setRanges2, UPRV_LENGTHOF(setRanges2),
        checkRanges2, UPRV_LENGTHOF(checkRanges2));
}

static void
TrieTestSet3Initial9(void) {
    testTrieRanges("set3-initial-9", false,
        setRanges3, UPRV_LENGTHOF(setRanges3),
        checkRanges3, UPRV_LENGTHOF(checkRanges3));
    testTrieRanges("set3-initial-9-clone", true,
        setRanges3, UPRV_LENGTHOF(setRanges3),
        checkRanges3, UPRV_LENGTHOF(checkRanges3));
}

static void
TrieTestSetEmpty(void) {
    testTrieRanges("set-empty", false,
        setRangesEmpty, 0,
        checkRangesEmpty, UPRV_LENGTHOF(checkRangesEmpty));
}

static void
TrieTestSetSingleValue(void) {
    testTrieRanges("set-single-value", false,
        setRangesSingleValue, UPRV_LENGTHOF(setRangesSingleValue),
        checkRangesSingleValue, UPRV_LENGTHOF(checkRangesSingleValue));
}

static void
TrieTestSet2OverlapWithClone(void) {
    testTrieRanges("set2-overlap.withClone", true,
        setRanges2, UPRV_LENGTHOF(setRanges2),
        checkRanges2, UPRV_LENGTHOF(checkRanges2));
}

/* test mutable-trie memory management -------------------------------------- */

static void
FreeBlocksTest(void) {
    static const CheckRange
    checkRanges[]={
        { 0,        1 },
        { 0x740,    1 },
        { 0x780,    2 },
        { 0x880,    3 },
        { 0x110000, 1 }
    };
    static const char *const testName="free-blocks";

    UMutableCPTrie *mutableTrie;
    int32_t i;
    UErrorCode errorCode;

    errorCode=U_ZERO_ERROR;
    mutableTrie=umutablecptrie_open(1, 0xad, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_open(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }

    /*
     * Repeatedly set overlapping same-value ranges to stress the free-data-block management.
     * If it fails, it will overflow the data array.
     */
    for(i=0; i<(0x120000>>4)/2; ++i) {  // 4=UCPTRIE_SHIFT_3
        umutablecptrie_setRange(mutableTrie, 0x740, 0x840-1, 1, &errorCode);
        umutablecptrie_setRange(mutableTrie, 0x780, 0x880-1, 1, &errorCode);
        umutablecptrie_setRange(mutableTrie, 0x740, 0x840-1, 2, &errorCode);
        umutablecptrie_setRange(mutableTrie, 0x780, 0x880-1, 3, &errorCode);
    }
    /* make blocks that will be free during compaction */
    umutablecptrie_setRange(mutableTrie, 0x1000, 0x3000-1, 2, &errorCode);
    umutablecptrie_setRange(mutableTrie, 0x2000, 0x4000-1, 3, &errorCode);
    umutablecptrie_setRange(mutableTrie, 0x1000, 0x4000-1, 1, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("error: setting lots of ranges into a mutable trie (%s) failed - %s\n",
                testName, u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }

    mutableTrie = testTrieSerializeAllValueWidth(testName, mutableTrie, false,
                                                 checkRanges, UPRV_LENGTHOF(checkRanges));
    umutablecptrie_close(mutableTrie);
}

static void
GrowDataArrayTest(void) {
    static const CheckRange
    checkRanges[]={
        { 0,        1 },
        { 0x720,    2 },
        { 0x7a0,    3 },
        { 0x8a0,    4 },
        { 0x110000, 5 }
    };
    static const char *const testName="grow-data";

    UMutableCPTrie *mutableTrie;
    int32_t i;
    UErrorCode errorCode;

    errorCode=U_ZERO_ERROR;
    mutableTrie=umutablecptrie_open(1, 0xad, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_open(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }

    /*
     * Use umutablecptrie_set() not umutablecptrie_setRange() to write non-initialValue-data.
     * Should grow/reallocate the data array to a sufficient length.
     */
    for(i=0; i<0x1000; ++i) {
        umutablecptrie_set(mutableTrie, i, 2, &errorCode);
    }
    for(i=0x720; i<0x1100; ++i) { /* some overlap */
        umutablecptrie_set(mutableTrie, i, 3, &errorCode);
    }
    for(i=0x7a0; i<0x900; ++i) {
        umutablecptrie_set(mutableTrie, i, 4, &errorCode);
    }
    for(i=0x8a0; i<0x110000; ++i) {
        umutablecptrie_set(mutableTrie, i, 5, &errorCode);
    }
    if(U_FAILURE(errorCode)) {
        log_err("error: setting lots of values into a mutable trie (%s) failed - %s\n",
                testName, u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }

    mutableTrie = testTrieSerializeAllValueWidth(testName, mutableTrie, false,
                                                 checkRanges, UPRV_LENGTHOF(checkRanges));
    umutablecptrie_close(mutableTrie);
}

static void
ManyAllSameBlocksTest(void) {
    static const char *const testName="many-all-same";

    UMutableCPTrie *mutableTrie;
    int32_t i;
    UErrorCode errorCode;
    CheckRange checkRanges[(0x110000 >> 12) + 1];

    errorCode = U_ZERO_ERROR;
    mutableTrie = umutablecptrie_open(0xff33, 0xad, &errorCode);
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_open(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }
    checkRanges[0].limit = 0;
    checkRanges[0].value = 0xff33;  // initialValue

    // Many all-same-value blocks.
    for (i = 0; i < 0x110000; i += 0x1000) {
        uint32_t value = i >> 12;
        umutablecptrie_setRange(mutableTrie, i, i + 0xfff, value, &errorCode);
        checkRanges[value + 1].limit = i + 0x1000;
        checkRanges[value + 1].value = value;
    }
    for (i = 0; i < 0x110000; i += 0x1000) {
        uint32_t expected = i >> 12;
        uint32_t v0 = umutablecptrie_get(mutableTrie, i);
        uint32_t vfff = umutablecptrie_get(mutableTrie, i + 0xfff);
        if (v0 != expected || vfff != expected) {
            log_err("error: UMutableCPTrie U+%04lx unexpected value\n", (long)i);
        }
    }

    mutableTrie = testTrieSerializeAllValueWidth(testName, mutableTrie, false,
                                                 checkRanges, UPRV_LENGTHOF(checkRanges));
    umutablecptrie_close(mutableTrie);
}

static void
MuchDataTest(void) {
    static const char *const testName="much-data";

    UMutableCPTrie *mutableTrie;
    int32_t r, c;
    UErrorCode errorCode = U_ZERO_ERROR;
    CheckRange checkRanges[(0x10000 >> 6) + (0x10240 >> 4) + 10];

    mutableTrie = umutablecptrie_open(0xff33, 0xad, &errorCode);
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_open(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }
    checkRanges[0].limit = 0;
    checkRanges[0].value = 0xff33;  // initialValue
    r = 1;

    // Add much data that does not compact well,
    // to get more than 128k data values after compaction.
    for (c = 0; c < 0x10000; c += 0x40) {
        uint32_t value = c >> 4;
        umutablecptrie_setRange(mutableTrie, c, c + 0x3f, value, &errorCode);
        checkRanges[r].limit = c + 0x40;
        checkRanges[r++].value = value;
    }
    checkRanges[r].limit = 0x20000;
    checkRanges[r++].value = 0xff33;
    for (c = 0x20000; c < 0x30230; c += 0x10) {
        uint32_t value = c >> 4;
        umutablecptrie_setRange(mutableTrie, c, c + 0xf, value, &errorCode);
        checkRanges[r].limit = c + 0x10;
        checkRanges[r++].value = value;
    }
    umutablecptrie_setRange(mutableTrie, 0x30230, 0x30233, 0x3023, &errorCode);
    checkRanges[r].limit = 0x30234;
    checkRanges[r++].value = 0x3023;
    umutablecptrie_setRange(mutableTrie, 0x30234, 0xdffff, 0x5005, &errorCode);
    checkRanges[r].limit = 0xe0000;
    checkRanges[r++].value = 0x5005;
    umutablecptrie_setRange(mutableTrie, 0xe0000, 0x10ffff, 0x9009, &errorCode);
    checkRanges[r].limit = 0x110000;
    checkRanges[r++].value = 0x9009;
    if (U_FAILURE(errorCode)) {
        log_err("error: setting lots of values into a mutable trie (%s) failed - %s\n",
                testName, u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }
    U_ASSERT(r <= UPRV_LENGTHOF(checkRanges));

    testBuilder(testName, mutableTrie, checkRanges, r);
    testTrieSerialize("much-data.16", mutableTrie,
                      UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16, false, checkRanges, r);
    umutablecptrie_close(mutableTrie);
}

static void testGetRangesFixedSurr(const char *testName, const UMutableCPTrie *mutableTrie,
                                   UCPMapRangeOption option,
                                   const CheckRange checkRanges[], int32_t countCheckRanges) {
    testTrieGetRanges(testName, NULL, mutableTrie, option, 5, checkRanges, countCheckRanges);
    UErrorCode errorCode = U_ZERO_ERROR;
    UMutableCPTrie *clone = umutablecptrie_clone(mutableTrie, &errorCode);
    UCPTrie *trie;
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_clone(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }
    trie = umutablecptrie_buildImmutable(clone, UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16, &errorCode);
    umutablecptrie_close(clone);
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_buildImmutable(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }
    testTrieGetRanges(testName, trie, NULL, option, 5, checkRanges, countCheckRanges);
    ucptrie_close(trie);
}

static void
TrieTestGetRangesFixedSurr(void) {
    static const SetRange
    setRangesFixedSurr[]={
        { 0xd000, 0xd7ff, 5 },
        { 0xd7ff, 0xe001, 3 },
        { 0xe001, 0xf900, 5 },
    };

    static const CheckRange
    checkRangesFixedLeadSurr1[]={
        { 0,      0 },
        { 0xd000, 0 },
        { 0xd7ff, 5 },
        { 0xd800, 3 },
        { 0xdc00, 5 },
        { 0xe001, 3 },
        { 0xf900, 5 },
        { 0x110000, 0 }
    };

    static const CheckRange
    checkRangesFixedAllSurr1[]={
        { 0,      0 },
        { 0xd000, 0 },
        { 0xd7ff, 5 },
        { 0xd800, 3 },
        { 0xe000, 5 },
        { 0xe001, 3 },
        { 0xf900, 5 },
        { 0x110000, 0 }
    };

    static const CheckRange
    checkRangesFixedLeadSurr3[]={
        { 0,      0 },
        { 0xd000, 0 },
        { 0xdc00, 5 },
        { 0xe001, 3 },
        { 0xf900, 5 },
        { 0x110000, 0 }
    };

    static const CheckRange
    checkRangesFixedAllSurr3[]={
        { 0,      0 },
        { 0xd000, 0 },
        { 0xe000, 5 },
        { 0xe001, 3 },
        { 0xf900, 5 },
        { 0x110000, 0 }
    };

    static const CheckRange
    checkRangesFixedSurr4[]={
        { 0,      0 },
        { 0xd000, 0 },
        { 0xf900, 5 },
        { 0x110000, 0 }
    };

    uint32_t initialValue, errorValue;
    getSpecialValues(
        checkRangesFixedLeadSurr1, UPRV_LENGTHOF(checkRangesFixedLeadSurr1),
        &initialValue, &errorValue);
    UMutableCPTrie *mutableTrie = makeTrieWithRanges(
        "fixedSurr", false, setRangesFixedSurr, UPRV_LENGTHOF(setRangesFixedSurr),
        initialValue, errorValue);
    UErrorCode errorCode = U_ZERO_ERROR;
    if (mutableTrie == NULL) {
        return;
    }
    testGetRangesFixedSurr("fixedLeadSurr1", mutableTrie, UCPMAP_RANGE_FIXED_LEAD_SURROGATES,
                           checkRangesFixedLeadSurr1, UPRV_LENGTHOF(checkRangesFixedLeadSurr1));
    testGetRangesFixedSurr("fixedAllSurr1", mutableTrie, UCPMAP_RANGE_FIXED_ALL_SURROGATES,
                           checkRangesFixedAllSurr1, UPRV_LENGTHOF(checkRangesFixedAllSurr1));
    // Setting a range in the middle of lead surrogates makes no difference.
    umutablecptrie_setRange(mutableTrie, 0xd844, 0xd899, 5, &errorCode);
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_setRange(fixedSurr2) failed: %s\n", u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }
    testGetRangesFixedSurr("fixedLeadSurr2", mutableTrie, UCPMAP_RANGE_FIXED_LEAD_SURROGATES,
                           checkRangesFixedLeadSurr1, UPRV_LENGTHOF(checkRangesFixedLeadSurr1));
    // Bridge the gap before the lead surrogates.
    umutablecptrie_set(mutableTrie, 0xd7ff, 5, &errorCode);
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_set(fixedSurr3) failed: %s\n", u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }
    testGetRangesFixedSurr("fixedLeadSurr3", mutableTrie, UCPMAP_RANGE_FIXED_LEAD_SURROGATES,
                           checkRangesFixedLeadSurr3, UPRV_LENGTHOF(checkRangesFixedLeadSurr3));
    testGetRangesFixedSurr("fixedAllSurr3", mutableTrie, UCPMAP_RANGE_FIXED_ALL_SURROGATES,
                           checkRangesFixedAllSurr3, UPRV_LENGTHOF(checkRangesFixedAllSurr3));
    // Bridge the gap after the trail surrogates.
    umutablecptrie_set(mutableTrie, 0xe000, 5, &errorCode);
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_set(fixedSurr4) failed: %s\n", u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }
    testGetRangesFixedSurr("fixedSurr4", mutableTrie, UCPMAP_RANGE_FIXED_ALL_SURROGATES,
                           checkRangesFixedSurr4, UPRV_LENGTHOF(checkRangesFixedSurr4));
    umutablecptrie_close(mutableTrie);
}

static void TestSmallNullBlockMatchesFast(void) {
    // The initial builder+getRange code had a bug:
    // When there is no null data block in the fast-index range,
    // but a fast-range data block starts with enough values to match a small data block,
    // then getRange() got confused.
    // The builder must prevent this.
    static const SetRange setRanges[] = {
        { 0, 0x880, 1 },
        // U+0880..U+088F map to initial value 0, potential match for small null data block.
        { 0x890, 0x1040, 2 },
        // U+1040..U+1050 map to 0.
        // First small null data block in a small-type trie.
        // In a fast-type trie, it is ok to match a small null data block at U+1041
        // but not at U+1040.
        { 0x1051, 0x10000, 3 },
        // No fast data block (block length 64) filled with 0 regardless of trie type.
        // Need more blocks filled with 0 than the largest range above,
        // and need a highStart above that so that it actually counts.
        { 0x20000, 0x110000, 9 }
    };

    static const CheckRange checkRanges[] = {
        { 0x0880, 1 },
        { 0x0890, 0 },
        { 0x1040, 2 },
        { 0x1051, 0 },
        { 0x10000, 3 },
        { 0x20000, 0 },
        { 0x110000, 9 }
    };

    testTrieRanges("small0-in-fast", false,
        setRanges, UPRV_LENGTHOF(setRanges),
        checkRanges, UPRV_LENGTHOF(checkRanges));
}

static void ShortAllSameBlocksTest(void) {
    static const char *const testName = "short-all-same";
    // Many all-same-value blocks but only of the small block length used in the mutable trie.
    // The builder code needs to turn a group of short ALL_SAME blocks below fastLimit
    // into a MIXED block, and reserve data array capacity for that.
    UErrorCode errorCode = U_ZERO_ERROR;
    UMutableCPTrie *mutableTrie = umutablecptrie_open(0, 0xad, &errorCode);
    CheckRange checkRanges[0x101];
    int32_t i;
    if (U_FAILURE(errorCode)) {
        log_err("error: umutablecptrie_open(%s) failed: %s\n", testName, u_errorName(errorCode));
        return;
    }
    for (i = 0; i < 0x1000; i += 0x10) {
        uint32_t value = i >> 4;
        umutablecptrie_setRange(mutableTrie, i, i + 0xf, value, &errorCode);
        checkRanges[value].limit = i + 0x10;
        checkRanges[value].value = value;
    }
    checkRanges[0x100].limit = 0x110000;
    checkRanges[0x100].value = 0;
    if (U_FAILURE(errorCode)) {
        log_err("error: setting values into a mutable trie (%s) failed - %s\n",
                testName, u_errorName(errorCode));
        umutablecptrie_close(mutableTrie);
        return;
    }

    mutableTrie = testTrieSerializeAllValueWidth(testName, mutableTrie, false,
                                                 checkRanges, UPRV_LENGTHOF(checkRanges));
    umutablecptrie_close(mutableTrie);
}

void
addUCPTrieTest(TestNode** root) {
    addTest(root, &TrieTestSet1, "tsutil/ucptrietest/TrieTestSet1");
    addTest(root, &TrieTestSet2Overlap, "tsutil/ucptrietest/TrieTestSet2Overlap");
    addTest(root, &TrieTestSet3Initial9, "tsutil/ucptrietest/TrieTestSet3Initial9");
    addTest(root, &TrieTestSetEmpty, "tsutil/ucptrietest/TrieTestSetEmpty");
    addTest(root, &TrieTestSetSingleValue, "tsutil/ucptrietest/TrieTestSetSingleValue");
    addTest(root, &TrieTestSet2OverlapWithClone, "tsutil/ucptrietest/TrieTestSet2OverlapWithClone");
    addTest(root, &FreeBlocksTest, "tsutil/ucptrietest/FreeBlocksTest");
    addTest(root, &GrowDataArrayTest, "tsutil/ucptrietest/GrowDataArrayTest");
    addTest(root, &ManyAllSameBlocksTest, "tsutil/ucptrietest/ManyAllSameBlocksTest");
    addTest(root, &MuchDataTest, "tsutil/ucptrietest/MuchDataTest");
    addTest(root, &TrieTestGetRangesFixedSurr, "tsutil/ucptrietest/TrieTestGetRangesFixedSurr");
    addTest(root, &TestSmallNullBlockMatchesFast, "tsutil/ucptrietest/TestSmallNullBlockMatchesFast");
    addTest(root, &ShortAllSameBlocksTest, "tsutil/ucptrietest/ShortAllSameBlocksTest");
}
