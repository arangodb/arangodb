// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   05/23/00    aliu        Creation.
**********************************************************************
*/

#include <algorithm>
#include <vector>
#include "unicode/utypes.h"
#include "unicode/edits.h"
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "testutil.h"
#include "intltest.h"

static const UChar HEX[] = u"0123456789ABCDEF";

UnicodeString &TestUtility::appendHex(UnicodeString &buf, UChar32 ch) {
    if (ch >= 0x10000) {
        if (ch >= 0x100000) {
            buf.append(HEX[0xF&(ch>>20)]);
        }
        buf.append(HEX[0xF&(ch>>16)]);
    }
    buf.append(HEX[0xF&(ch>>12)]);
    buf.append(HEX[0xF&(ch>>8)]);
    buf.append(HEX[0xF&(ch>>4)]);
    buf.append(HEX[0xF&ch]);
    return buf;
}

UnicodeString TestUtility::hex(UChar32 ch) {
    UnicodeString buf;
    appendHex(buf, ch);
    return buf;
}

UnicodeString TestUtility::hex(const UnicodeString& s) {
    return hex(s, u',');
}

UnicodeString TestUtility::hex(const UnicodeString& s, UChar sep) {
    UnicodeString result;
    if (s.isEmpty()) return result;
    UChar32 c;
    for (int32_t i = 0; i < s.length(); i += U16_LENGTH(c)) {
        c = s.char32At(i);
        if (i > 0) {
            result.append(sep);
        }
        appendHex(result, c);
    }
    return result;
}

UnicodeString TestUtility::hex(const uint8_t* bytes, int32_t len) {
    UnicodeString buf;
    for (int32_t i = 0; i < len; ++i) {
        buf.append(HEX[0x0F & (bytes[i] >> 4)]);
        buf.append(HEX[0x0F & bytes[i]]);
    }
    return buf;
}

namespace {

UnicodeString printOneEdit(const Edits::Iterator &ei) {
    if (ei.hasChange()) {
        return UnicodeString() + ei.oldLength() + u"->" + ei.newLength();
    } else {
        return UnicodeString() + ei.oldLength() + u"=" + ei.newLength();
    }
}

/**
 * Maps indexes according to the expected edits.
 * A destination index can occur multiple times when there are source deletions.
 * Map according to the last occurrence, normally in a non-empty destination span.
 * Simplest is to search from the back.
 */
int32_t srcIndexFromDest(const EditChange expected[], int32_t expLength,
                         int32_t srcLength, int32_t destLength, int32_t index) {
    int32_t srcIndex = srcLength;
    int32_t destIndex = destLength;
    int32_t i = expLength;
    while (index < destIndex && i > 0) {
        --i;
        int32_t prevSrcIndex = srcIndex - expected[i].oldLength;
        int32_t prevDestIndex = destIndex - expected[i].newLength;
        if (index == prevDestIndex) {
            return prevSrcIndex;
        } else if (index > prevDestIndex) {
            if (expected[i].change) {
                // In a change span, map to its end.
                return srcIndex;
            } else {
                // In an unchanged span, offset within it.
                return prevSrcIndex + (index - prevDestIndex);
            }
        }
        srcIndex = prevSrcIndex;
        destIndex = prevDestIndex;
    }
    // index is outside the string.
    return srcIndex;
}

int32_t destIndexFromSrc(const EditChange expected[], int32_t expLength,
                         int32_t srcLength, int32_t destLength, int32_t index) {
    int32_t srcIndex = srcLength;
    int32_t destIndex = destLength;
    int32_t i = expLength;
    while (index < srcIndex && i > 0) {
        --i;
        int32_t prevSrcIndex = srcIndex - expected[i].oldLength;
        int32_t prevDestIndex = destIndex - expected[i].newLength;
        if (index == prevSrcIndex) {
            return prevDestIndex;
        } else if (index > prevSrcIndex) {
            if (expected[i].change) {
                // In a change span, map to its end.
                return destIndex;
            } else {
                // In an unchanged span, offset within it.
                return prevDestIndex + (index - prevSrcIndex);
            }
        }
        srcIndex = prevSrcIndex;
        destIndex = prevDestIndex;
    }
    // index is outside the string.
    return destIndex;
}

}  // namespace

// For debugging, set -v to see matching edits up to a failure.
UBool TestUtility::checkEqualEdits(IntlTest &test, const UnicodeString &name,
                                   const Edits &e1, const Edits &e2, UErrorCode &errorCode) {
    Edits::Iterator ei1 = e1.getFineIterator();
    Edits::Iterator ei2 = e2.getFineIterator();
    UBool ok = TRUE;
    for (int32_t i = 0; ok; ++i) {
        UBool ei1HasNext = ei1.next(errorCode);
        UBool ei2HasNext = ei2.next(errorCode);
        ok &= test.assertEquals(name + u" next()[" + i + u"]" + __LINE__,
                                ei1HasNext, ei2HasNext);
        ok &= test.assertSuccess(name + u" errorCode[" + i + u"]" + __LINE__, errorCode);
        ok &= test.assertEquals(name + u" edit[" + i + u"]" + __LINE__,
                                printOneEdit(ei1), printOneEdit(ei2));
        if (!ei1HasNext || !ei2HasNext) {
            break;
        }
        test.logln();
    }
    return ok;
}

void TestUtility::checkEditsIter(
        IntlTest &test,
        const UnicodeString &name,
        Edits::Iterator ei1, Edits::Iterator ei2,  // two equal iterators
        const EditChange expected[], int32_t expLength, UBool withUnchanged,
        UErrorCode &errorCode) {
    test.assertFalse(name + u":" + __LINE__, ei2.findSourceIndex(-1, errorCode));
    test.assertFalse(name + u":" + __LINE__, ei2.findDestinationIndex(-1, errorCode));

    int32_t expSrcIndex = 0;
    int32_t expDestIndex = 0;
    int32_t expReplIndex = 0;
    for (int32_t expIndex = 0; expIndex < expLength; ++expIndex) {
        const EditChange &expect = expected[expIndex];
        UnicodeString msg = UnicodeString(name).append(u' ') + expIndex;
        if (withUnchanged || expect.change) {
            test.assertTrue(msg + u":" + __LINE__, ei1.next(errorCode));
            test.assertEquals(msg + u":" + __LINE__, expect.change, ei1.hasChange());
            test.assertEquals(msg + u":" + __LINE__, expect.oldLength, ei1.oldLength());
            test.assertEquals(msg + u":" + __LINE__, expect.newLength, ei1.newLength());
            test.assertEquals(msg + u":" + __LINE__, expSrcIndex, ei1.sourceIndex());
            test.assertEquals(msg + u":" + __LINE__, expDestIndex, ei1.destinationIndex());
            test.assertEquals(msg + u":" + __LINE__, expReplIndex, ei1.replacementIndex());
        }

        if (expect.oldLength > 0) {
            test.assertTrue(msg + u":" + __LINE__, ei2.findSourceIndex(expSrcIndex, errorCode));
            test.assertEquals(msg + u":" + __LINE__, expect.change, ei2.hasChange());
            test.assertEquals(msg + u":" + __LINE__, expect.oldLength, ei2.oldLength());
            test.assertEquals(msg + u":" + __LINE__, expect.newLength, ei2.newLength());
            test.assertEquals(msg + u":" + __LINE__, expSrcIndex, ei2.sourceIndex());
            test.assertEquals(msg + u":" + __LINE__, expDestIndex, ei2.destinationIndex());
            test.assertEquals(msg + u":" + __LINE__, expReplIndex, ei2.replacementIndex());
            if (!withUnchanged) {
                // For some iterators, move past the current range
                // so that findSourceIndex() has to look before the current index.
                ei2.next(errorCode);
                ei2.next(errorCode);
            }
        }

        if (expect.newLength > 0) {
            test.assertTrue(msg + u":" + __LINE__, ei2.findDestinationIndex(expDestIndex, errorCode));
            test.assertEquals(msg + u":" + __LINE__, expect.change, ei2.hasChange());
            test.assertEquals(msg + u":" + __LINE__, expect.oldLength, ei2.oldLength());
            test.assertEquals(msg + u":" + __LINE__, expect.newLength, ei2.newLength());
            test.assertEquals(msg + u":" + __LINE__, expSrcIndex, ei2.sourceIndex());
            test.assertEquals(msg + u":" + __LINE__, expDestIndex, ei2.destinationIndex());
            test.assertEquals(msg + u":" + __LINE__, expReplIndex, ei2.replacementIndex());
            if (!withUnchanged) {
                // For some iterators, move past the current range
                // so that findSourceIndex() has to look before the current index.
                ei2.next(errorCode);
                ei2.next(errorCode);
            }
        }

        expSrcIndex += expect.oldLength;
        expDestIndex += expect.newLength;
        if (expect.change) {
            expReplIndex += expect.newLength;
        }
    }
    UnicodeString msg = UnicodeString(name).append(u" end");
    test.assertFalse(msg + u":" + __LINE__, ei1.next(errorCode));
    test.assertFalse(msg + u":" + __LINE__, ei1.hasChange());
    test.assertEquals(msg + u":" + __LINE__, 0, ei1.oldLength());
    test.assertEquals(msg + u":" + __LINE__, 0, ei1.newLength());
    test.assertEquals(msg + u":" + __LINE__, expSrcIndex, ei1.sourceIndex());
    test.assertEquals(msg + u":" + __LINE__, expDestIndex, ei1.destinationIndex());
    test.assertEquals(msg + u":" + __LINE__, expReplIndex, ei1.replacementIndex());

    test.assertFalse(name + u":" + __LINE__, ei2.findSourceIndex(expSrcIndex, errorCode));
    test.assertFalse(name + u":" + __LINE__, ei2.findDestinationIndex(expDestIndex, errorCode));

    // Check mapping of all indexes against a simple implementation
    // that works on the expected changes.
    // Iterate once forward, once backward, to cover more runtime conditions.
    int32_t srcLength = expSrcIndex;
    int32_t destLength = expDestIndex;
    std::vector<int32_t> srcIndexes;
    std::vector<int32_t> destIndexes;
    srcIndexes.push_back(-1);
    destIndexes.push_back(-1);
    int32_t srcIndex = 0;
    int32_t destIndex = 0;
    for (int32_t i = 0; i < expLength; ++i) {
        if (expected[i].oldLength > 0) {
            srcIndexes.push_back(srcIndex);
            if (expected[i].oldLength > 1) {
                srcIndexes.push_back(srcIndex + 1);
                if (expected[i].oldLength > 2) {
                    srcIndexes.push_back(srcIndex + expected[i].oldLength - 1);
                }
            }
        }
        if (expected[i].newLength > 0) {
            destIndexes.push_back(destIndex);
            if (expected[i].newLength > 1) {
                destIndexes.push_back(destIndex + 1);
                if (expected[i].newLength > 2) {
                    destIndexes.push_back(destIndex + expected[i].newLength - 1);
                }
            }
        }
        srcIndex += expected[i].oldLength;
        destIndex += expected[i].newLength;
    }
    srcIndexes.push_back(srcLength);
    destIndexes.push_back(destLength);
    srcIndexes.push_back(srcLength + 1);
    destIndexes.push_back(destLength + 1);
    std::reverse(destIndexes.begin(), destIndexes.end());
    // Zig-zag across the indexes to stress next() <-> previous().
    static const int32_t ZIG_ZAG[] = { 0, 1, 2, 3, 2, 1 };
    for (auto i = 0; i < (int32_t)srcIndexes.size(); ++i) {
        for (int32_t ij = 0; ij < UPRV_LENGTHOF(ZIG_ZAG); ++ij) {
            int32_t j = ZIG_ZAG[ij];
            if ((i + j) < (int32_t)srcIndexes.size()) {
                int32_t si = srcIndexes[i + j];
                test.assertEquals(name + u" destIndexFromSrc(" + si + u"):" + __LINE__,
                                  destIndexFromSrc(expected, expLength, srcLength, destLength, si),
                                  ei2.destinationIndexFromSourceIndex(si, errorCode));
            }
        }
    }
    for (auto i = 0; i < (int32_t)destIndexes.size(); ++i) {
        for (int32_t ij = 0; ij < UPRV_LENGTHOF(ZIG_ZAG); ++ij) {
            int32_t j = ZIG_ZAG[ij];
            if ((i + j) < (int32_t)destIndexes.size()) {
                int32_t di = destIndexes[i + j];
                test.assertEquals(name + u" srcIndexFromDest(" + di + u"):" + __LINE__,
                                  srcIndexFromDest(expected, expLength, srcLength, destLength, di),
                                  ei2.sourceIndexFromDestinationIndex(di, errorCode));
            }
        }
    }
}
