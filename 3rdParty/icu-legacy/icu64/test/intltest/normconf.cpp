// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
************************************************************************
* Copyright (c) 1997-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include <string>
#include "unicode/bytestream.h"
#include "unicode/edits.h"
#include "unicode/uchar.h"
#include "unicode/normalizer2.h"
#include "unicode/normlzr.h"
#include "unicode/uniset.h"
#include "unicode/putil.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "normconf.h"
#include "uassert.h"
#include <stdio.h>

void NormalizerConformanceTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestConformance);
    TESTCASE_AUTO(TestConformance32);
    TESTCASE_AUTO(TestCase6);
    TESTCASE_AUTO_END;
}

#define FIELD_COUNT 5

NormalizerConformanceTest::NormalizerConformanceTest() :
        normalizer(UnicodeString(), UNORM_NFC) {
    UErrorCode errorCode = U_ZERO_ERROR;
    nfc = Normalizer2::getNFCInstance(errorCode);
    nfd = Normalizer2::getNFDInstance(errorCode);
    nfkc = Normalizer2::getNFKCInstance(errorCode);
    nfkd = Normalizer2::getNFKDInstance(errorCode);
    assertSuccess("", errorCode, true, __FILE__, __LINE__);
}

NormalizerConformanceTest::~NormalizerConformanceTest() {}

// more interesting conformance test cases, not in the unicode.org NormalizationTest.txt
static const char *moreCases[]={
    // Markus 2001aug30
    "0061 0332 0308;00E4 0332;0061 0332 0308;00E4 0332;0061 0332 0308; # Markus 0",

    // Markus 2001oct26 - test edge case for iteration: U+0f73.cc==0 but decomposition.lead.cc==129
    "0061 0301 0F73;00E1 0F71 0F72;0061 0F71 0F72 0301;00E1 0F71 0F72;0061 0F71 0F72 0301; # Markus 1"
};

void NormalizerConformanceTest::compare(const UnicodeString& s1, const UnicodeString& s2){
    UErrorCode status=U_ZERO_ERROR;
     // TODO: Re-enable this tests after UTC fixes UAX 21
    if(s1.indexOf((UChar32)0x0345)>=0)return;
    if(Normalizer::compare(s1,s2,U_FOLD_CASE_DEFAULT,status)!=0){
        errln("Normalizer::compare() failed for s1: " + prettify(s1) + " s2: " +prettify(s2));
    }
}

FileStream *
NormalizerConformanceTest::openNormalizationTestFile(const char *filename) {
    char unidataPath[2000];
    const char *folder;
    FileStream *input;
    UErrorCode errorCode;

    // look inside ICU_DATA first
    folder=pathToDataDirectory();
    if(folder!=nullptr) {
        strcpy(unidataPath, folder);
        strcat(unidataPath, "unidata" U_FILE_SEP_STRING);
        strcat(unidataPath, filename);
        input=T_FileStream_open(unidataPath, "rb");
        if(input!=nullptr) {
            return input;
        }
    }

    // find icu/source/data/unidata relative to the test data
    errorCode=U_ZERO_ERROR;
    folder=loadTestData(errorCode);
    if(U_SUCCESS(errorCode)) {
        strcpy(unidataPath, folder);
        strcat(unidataPath, U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".."
                     U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".."
                     U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "unidata" U_FILE_SEP_STRING);
        strcat(unidataPath, filename);
        input=T_FileStream_open(unidataPath, "rb");
        if(input!=nullptr) {
            return input;
        }
    }

    // look in icu/source/test/testdata/out/build
    errorCode=U_ZERO_ERROR;
    folder=loadTestData(errorCode);
    if(U_SUCCESS(errorCode)) {
        strcpy(unidataPath, folder);
        strcat(unidataPath, U_FILE_SEP_STRING);
        strcat(unidataPath, filename);
        input=T_FileStream_open(unidataPath, "rb");
        if(input!=nullptr) {
            return input;
        }
    }

    // look in icu/source/test/testdata
    errorCode=U_ZERO_ERROR;
    folder=loadTestData(errorCode);
    if(U_SUCCESS(errorCode)) {
        strcpy(unidataPath, folder);
        strcat(unidataPath, U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING);
        strcat(unidataPath, filename);
        input=T_FileStream_open(unidataPath, "rb");
        if(input!=nullptr) {
            return input;
        }
    }

    // find icu/source/data/unidata relative to U_TOPSRCDIR
#if defined(U_TOPSRCDIR)
    strcpy(unidataPath, U_TOPSRCDIR U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "unidata" U_FILE_SEP_STRING);
    strcat(unidataPath, filename);
    input=T_FileStream_open(unidataPath, "rb");
    if(input!=nullptr) {
        return input;
    }

    strcpy(unidataPath, U_TOPSRCDIR U_FILE_SEP_STRING "test" U_FILE_SEP_STRING "testdata" U_FILE_SEP_STRING);
    strcat(unidataPath, filename);
    input=T_FileStream_open(unidataPath, "rb");
    if(input!=nullptr) {
        return input;
    }
#endif

    dataerrln("Failed to open %s", filename);
    return nullptr;
}

/**
 * Test the conformance of Normalizer to
 * http://www.unicode.org/Public/UNIDATA/NormalizationTest.txt
 */
void NormalizerConformanceTest::TestConformance() {
    TestConformance(openNormalizationTestFile("NormalizationTest.txt"), 0);
}

void NormalizerConformanceTest::TestConformance32() {
    TestConformance(openNormalizationTestFile("NormalizationTest-3.2.0.txt"), UNORM_UNICODE_3_2);
}

void NormalizerConformanceTest::TestConformance(FileStream *input, int32_t options) {
    enum { BUF_SIZE = 1024 };
    char lineBuf[BUF_SIZE];
    UnicodeString fields[FIELD_COUNT];
    UErrorCode status = U_ZERO_ERROR;
    int32_t passCount = 0;
    int32_t failCount = 0;
    UChar32 c;

    if(input==nullptr) {
        return;
    }

    // UnicodeSet for all code points that are not mentioned in NormalizationTest.txt
    UnicodeSet other(0, 0x10ffff);

    int32_t count, countMoreCases = UPRV_LENGTHOF(moreCases);
    for (count = 1;;++count) {
        if (!T_FileStream_eof(input)) {
            T_FileStream_readLine(input, lineBuf, (int32_t)sizeof(lineBuf));
        } else {
            // once NormalizationTest.txt is finished, use moreCases[]
            if(count > countMoreCases) {
                count = 0;
            } else if(count == countMoreCases) {
                // all done
                break;
            }
            uprv_strcpy(lineBuf, moreCases[count]);
        }
        if (lineBuf[0] == 0 || lineBuf[0] == '\n' || lineBuf[0] == '\r') continue;

        // Expect 5 columns of this format:
        // 1E0C;1E0C;0044 0323;1E0C;0044 0323; # <comments>

        // Parse out the comment.
        if (lineBuf[0] == '#') continue;

        // Read separator lines starting with '@'
        if (lineBuf[0] == '@') {
            logln(lineBuf);
            continue;
        }

        // Parse out the fields
        if (!hexsplit(lineBuf, ';', fields, FIELD_COUNT)) {
            errln((UnicodeString)"Unable to parse line " + count);
            break; // Syntax error
        }

        // Remove a single code point from the "other" UnicodeSet
        if(fields[0].length()==fields[0].moveIndex32(0, 1)) {
            c=fields[0].char32At(0);
            if(0xac20<=c && c<=0xd73f && quick) {
                // not an exhaustive test run: skip most Hangul syllables
                if(c==0xac20) {
                    other.remove(0xac20, 0xd73f);
                }
                continue;
            }
            other.remove(c);
        }

        if (checkConformance(fields, lineBuf, options, status)) {
            ++passCount;
        } else {
            ++failCount;
            if(status == U_FILE_ACCESS_ERROR) {
              dataerrln("Something is wrong with the normalizer, skipping the rest of the test.");
              break;
            }
        }
        if ((count % 1000) == 0) {
            logln("Line %d", count);
        }
    }

    T_FileStream_close(input);

    /*
     * Test that all characters that are not mentioned
     * as single code points in column 1
     * do not change under any normalization.
     */

    // remove U+ffff because that is the end-of-iteration sentinel value
    other.remove(0xffff);

    for(c=0; c<=0x10ffff; quick ? c+=113 : ++c) {
        if(0x30000<=c && c<0xe0000) {
            c=0xe0000;
        }
        if(!other.contains(c)) {
            continue;
        }

        fields[0]=fields[1]=fields[2]=fields[3]=fields[4].setTo(c);
        snprintf(lineBuf, sizeof(lineBuf), "not mentioned code point U+%04lx", (long)c);

        if (checkConformance(fields, lineBuf, options, status)) {
            ++passCount;
        } else {
            ++failCount;
            if(status == U_FILE_ACCESS_ERROR) {
              dataerrln("Something is wrong with the normalizer, skipping the rest of the test.: %s", u_errorName(status));
              break;
            }
        }
        if ((c % 0x1000) == 0) {
            logln("Code point U+%04lx", c);
        }
    }

    if (failCount != 0) {
        dataerrln((UnicodeString)"Total: " + failCount + " lines/code points failed, " +
              passCount + " lines/code points passed");
    } else {
        logln((UnicodeString)"Total: " + passCount + " lines/code points passed");
    }
}

namespace {

UBool isNormalizedUTF8(const Normalizer2 *norm2, const UnicodeString &s, UErrorCode &errorCode) {
    if (norm2 == nullptr) {
        return true;
    }
    std::string s8;
    return norm2->isNormalizedUTF8(s.toUTF8String(s8), errorCode);
}

}  // namespace

/**
 * Verify the conformance of the given line of the Unicode
 * normalization (UTR 15) test suite file.  For each line,
 * there are five columns, corresponding to field[0]..field[4].
 *
 * The following invariants must be true for all conformant implementations
 *  c2 == NFC(c1) == NFC(c2) == NFC(c3)
 *  c3 == NFD(c1) == NFD(c2) == NFD(c3)
 *  c4 == NFKC(c1) == NFKC(c2) == NFKC(c3) == NFKC(c4) == NFKC(c5)
 *  c5 == NFKD(c1) == NFKD(c2) == NFKD(c3) == NFKD(c4) == NFKD(c5)
 *
 * @param field the 5 columns
 * @param line the source line from the test suite file
 * @return true if the test passes
 */
UBool NormalizerConformanceTest::checkConformance(const UnicodeString* field,
                                                  const char *line,
                                                  int32_t options,
                                                  UErrorCode &status) {
    UBool pass = true, result;
    UnicodeString out, fcd;
    int32_t fieldNum;

    for (int32_t i=0; i<FIELD_COUNT; ++i) {
        fieldNum = i+1;
        if (i<3) {
            pass &= checkNorm(UNORM_NFC, options, nfc, field[i], field[1], fieldNum);
            pass &= checkNorm(UNORM_NFD, options, nfd, field[i], field[2], fieldNum);
        }
        pass &= checkNorm(UNORM_NFKC, options, nfkc, field[i], field[3], fieldNum);
        pass &= checkNorm(UNORM_NFKD, options, nfkd, field[i], field[4], fieldNum);
    }
    compare(field[1],field[2]);
    compare(field[0],field[1]);
    // test quick checks
    if(UNORM_NO == Normalizer::quickCheck(field[1], UNORM_NFC, options, status)) {
        errln("Normalizer error: quickCheck(NFC(s), UNORM_NFC) is UNORM_NO");
        pass = false;
    }
    if(UNORM_NO == Normalizer::quickCheck(field[2], UNORM_NFD, options, status)) {
        errln("Normalizer error: quickCheck(NFD(s), UNORM_NFD) is UNORM_NO");
        pass = false;
    }
    if(UNORM_NO == Normalizer::quickCheck(field[3], UNORM_NFKC, options, status)) {
        errln("Normalizer error: quickCheck(NFKC(s), UNORM_NFKC) is UNORM_NO");
        pass = false;
    }
    if(UNORM_NO == Normalizer::quickCheck(field[4], UNORM_NFKD, options, status)) {
        errln("Normalizer error: quickCheck(NFKD(s), UNORM_NFKD) is UNORM_NO");
        pass = false;
    }

    // branch on options==0 for better code coverage
    if(options==0) {
        result = Normalizer::isNormalized(field[1], UNORM_NFC, status);
    } else {
        result = Normalizer::isNormalized(field[1], UNORM_NFC, options, status);
    }
    if(!result) {
        dataerrln("Normalizer error: isNormalized(NFC(s), UNORM_NFC) is false");
        pass = false;
    }
    if(options==0 && !isNormalizedUTF8(nfc, field[1], status)) {
        dataerrln("Normalizer error: nfc.isNormalizedUTF8(NFC(s)) is false");
        pass = false;
    }
    if(field[0]!=field[1]) {
        if(Normalizer::isNormalized(field[0], UNORM_NFC, options, status)) {
            errln("Normalizer error: isNormalized(s, UNORM_NFC) is true");
            pass = false;
        }
        if(isNormalizedUTF8(nfc, field[0], status)) {
            errln("Normalizer error: nfc.isNormalizedUTF8(s) is true");
            pass = false;
        }
    }
    if(options==0 && !isNormalizedUTF8(nfd, field[2], status)) {
        dataerrln("Normalizer error: nfd.isNormalizedUTF8(NFD(s)) is false");
        pass = false;
    }
    if(!Normalizer::isNormalized(field[3], UNORM_NFKC, options, status)) {
        dataerrln("Normalizer error: isNormalized(NFKC(s), UNORM_NFKC) is false");
        pass = false;
    } else {
        if(options==0 && !isNormalizedUTF8(nfkc, field[3], status)) {
            dataerrln("Normalizer error: nfkc.isNormalizedUTF8(NFKC(s)) is false");
            pass = false;
        }
        if(field[0]!=field[3]) {
            if(Normalizer::isNormalized(field[0], UNORM_NFKC, options, status)) {
                errln("Normalizer error: isNormalized(s, UNORM_NFKC) is true");
                pass = false;
            }
            if(options==0 && isNormalizedUTF8(nfkc, field[0], status)) {
                errln("Normalizer error: nfkc.isNormalizedUTF8(s) is true");
                pass = false;
            }
        }
    }
    if(options==0 && !isNormalizedUTF8(nfkd, field[4], status)) {
        dataerrln("Normalizer error: nfkd.isNormalizedUTF8(NFKD(s)) is false");
        pass = false;
    }

    // test FCD quick check and "makeFCD"
    Normalizer::normalize(field[0], UNORM_FCD, options, fcd, status);
    if(UNORM_NO == Normalizer::quickCheck(fcd, UNORM_FCD, options, status)) {
        errln("Normalizer error: quickCheck(FCD(s), UNORM_FCD) is UNORM_NO");
        pass = false;
    }
    if(UNORM_NO == Normalizer::quickCheck(field[2], UNORM_FCD, options, status)) {
        errln("Normalizer error: quickCheck(NFD(s), UNORM_FCD) is UNORM_NO");
        pass = false;
    }
    if(UNORM_NO == Normalizer::quickCheck(field[4], UNORM_FCD, options, status)) {
        errln("Normalizer error: quickCheck(NFKD(s), UNORM_FCD) is UNORM_NO");
        pass = false;
    }

    Normalizer::normalize(fcd, UNORM_NFD, options, out, status);
    if(out != field[2]) {
        dataerrln("Normalizer error: NFD(FCD(s))!=NFD(s)");
        pass = false;
    }

    if (U_FAILURE(status)) {
        dataerrln("Normalizer::normalize returned error status: %s", u_errorName(status));
        pass = false;
    }

    if(field[0]!=field[2]) {
        // two strings that are canonically equivalent must test
        // equal under a canonical caseless match
        // see UAX #21 Case Mappings and Jitterbug 2021 and
        // Unicode Technical Committee meeting consensus 92-C31
        int32_t rc;

        status=U_ZERO_ERROR;
        rc=Normalizer::compare(field[0], field[2], (options<<UNORM_COMPARE_NORM_OPTIONS_SHIFT)|U_COMPARE_IGNORE_CASE, status);
        if(U_FAILURE(status)) {
            dataerrln("Normalizer::compare(case-insensitive) sets %s", u_errorName(status));
            pass=false;
        } else if(rc!=0) {
            errln("Normalizer::compare(original, NFD, case-insensitive) returned %d instead of 0 for equal", rc);
            pass=false;
        }
    }

    if (!pass) {
        dataerrln("FAIL: %s", line);
    }
    return pass;
}

static const char *const kModeStrings[UNORM_MODE_COUNT] = {
    "?", "none", "D", "KD", "C", "KC", "FCD"
};

static const char *const kMessages[UNORM_MODE_COUNT] = {
    "?!=?", "?!=?", "c3!=D(c%d)", "c5!=KC(c%d)", "c2!=C(c%d)", "c4!=KC(c%d)", "FCD"
};

UBool NormalizerConformanceTest::checkNorm(UNormalizationMode mode, int32_t options,
                                           const Normalizer2 *norm2,
                                           const UnicodeString &s, const UnicodeString &exp,
                                           int32_t field) {
    const char *modeString = kModeStrings[mode];
    char msg[20];
    snprintf(msg, sizeof(msg), kMessages[mode], field);
    UnicodeString out;
    UErrorCode errorCode = U_ZERO_ERROR;
    Normalizer::normalize(s, mode, options, out, errorCode);
    if (U_FAILURE(errorCode)) {
        dataerrln("Error running normalize UNORM_NF%s: %s", modeString, u_errorName(errorCode));
        return false;
    }
    if (!assertEqual(modeString, "", s, out, exp, msg)) {
        return false;
    }

    iterativeNorm(s, mode, options, out, +1);
    if (!assertEqual(modeString, "(+1)", s, out, exp, msg)) {
        return false;
    }

    iterativeNorm(s, mode, options, out, -1);
    if (!assertEqual(modeString, "(-1)", s, out, exp, msg)) {
        return false;
    }

    if (norm2 == nullptr || options != 0) {
        return true;
    }

    std::string s8;
    s.toUTF8String(s8);
    std::string exp8;
    exp.toUTF8String(exp8);
    std::string out8;
    Edits edits;
    Edits *editsPtr = mode != UNORM_FCD ? &edits : nullptr;
    StringByteSink<std::string> sink(&out8, static_cast<int32_t>(exp8.length()));
    norm2->normalizeUTF8(0, s8, sink, editsPtr, errorCode);
    if (U_FAILURE(errorCode)) {
        errln("Normalizer2.%s.normalizeUTF8(%s) failed: %s",
              modeString, s8.c_str(), u_errorName(errorCode));
        return false;
    }
    if (out8 != exp8) {
        errln("Normalizer2.%s.normalizeUTF8(%s)=%s != %s",
              modeString, s8.c_str(), out8.c_str(), exp8.c_str());
        return false;
    }
    if (editsPtr == nullptr) {
        return true;
    }

    // Do the Edits cover the entire input & output?
    UBool pass = true;
    pass &= assertEquals("edits.hasChanges()", (UBool)(s8 != out8), edits.hasChanges());
    pass &= assertEquals("edits.lengthDelta()",
                         (int32_t)(out8.length() - s8.length()), edits.lengthDelta());
    Edits::Iterator iter = edits.getCoarseIterator();
    while (iter.next(errorCode)) {}
    pass &= assertEquals("edits source length", static_cast<int32_t>(s8.length()), iter.sourceIndex());
    pass &= assertEquals("edits destination length", static_cast<int32_t>(out8.length()), iter.destinationIndex());
    return pass;
}

/**
 * Do a normalization using the iterative API in the given direction.
 * @param dir either +1 or -1
 */
void NormalizerConformanceTest::iterativeNorm(const UnicodeString& str,
                                              UNormalizationMode mode, int32_t options,
                                              UnicodeString& result,
                                              int8_t dir) {
    UErrorCode status = U_ZERO_ERROR;
    normalizer.setText(str, status);
    normalizer.setMode(mode);
    normalizer.setOption(-1, 0);        // reset all options
    normalizer.setOption(options, 1);   // set desired options
    result.truncate(0);
    if (U_FAILURE(status)) {
        return;
    }
    UChar32 ch;
    if (dir > 0) {
        for (ch = normalizer.first(); ch != Normalizer::DONE;
             ch = normalizer.next()) {
            result.append(ch);
        }
    } else {
        for (ch = normalizer.last(); ch != Normalizer::DONE;
             ch = normalizer.previous()) {
            result.insert(0, ch);
        }
    }
}

UBool NormalizerConformanceTest::assertEqual(const char *op, const char *op2,
                                             const UnicodeString& s,
                                             const UnicodeString& got,
                                             const UnicodeString& exp,
                                             const char *msg) {
    if (exp == got)
        return true;

    char *sChars, *gotChars, *expChars;
    UnicodeString sPretty(prettify(s));
    UnicodeString gotPretty(prettify(got));
    UnicodeString expPretty(prettify(exp));

    sChars = new char[sPretty.length() + 1];
    gotChars = new char[gotPretty.length() + 1];
    expChars = new char[expPretty.length() + 1];

    sPretty.extract(0, sPretty.length(), sChars, sPretty.length() + 1);
    sChars[sPretty.length()] = 0;
    gotPretty.extract(0, gotPretty.length(), gotChars, gotPretty.length() + 1);
    gotChars[gotPretty.length()] = 0;
    expPretty.extract(0, expPretty.length(), expChars, expPretty.length() + 1);
    expChars[expPretty.length()] = 0;

    errln("    %s: %s%s(%s)=%s, exp. %s", msg, op, op2, sChars, gotChars, expChars);

    delete []sChars;
    delete []gotChars;
    delete []expChars;
    return false;
}

/**
 * Split a string into pieces based on the given delimiter
 * character.  Then, parse the resultant fields from hex into
 * characters.  That is, "0040 0400;0C00;0899" -> new String[] {
 * "\u0040\u0400", "\u0C00", "\u0899" }.  The output is assumed to
 * be of the proper length already, and exactly output.length
 * fields are parsed.  If there are too few an exception is
 * thrown.  If there are too many the extras are ignored.
 *
 * @return false upon failure
 */
UBool NormalizerConformanceTest::hexsplit(const char *s, char delimiter,
                                          UnicodeString output[], int32_t outputLength) {
    const char *t = s;
    char *end = nullptr;
    UChar32 c;
    int32_t i;
    for (i=0; i<outputLength; ++i) {
        // skip whitespace
        while(*t == ' ' || *t == '\t') {
            ++t;
        }

        // read a sequence of code points
        output[i].remove();
        for(;;) {
            c = (UChar32)uprv_strtoul(t, &end, 16);

            if( (char *)t == end ||
                (uint32_t)c > 0x10ffff ||
                (*end != ' ' && *end != '\t' && *end != delimiter)
            ) {
                errln(UnicodeString("Bad field ", "") + (i + 1) + " in " + UnicodeString(s, ""));
                return false;
            }

            output[i].append(c);

            t = (const char *)end;

            // skip whitespace
            while(*t == ' ' || *t == '\t') {
                ++t;
            }

            if(*t == delimiter) {
                ++t;
                break;
            }
            if(*t == 0) {
                if((i + 1) == outputLength) {
                    return true;
                } else {
                    errln(UnicodeString("Missing field(s) in ", "") + s + " only " + (i + 1) + " out of " + outputLength);
                    return false;
                }
            }
        }
    }
    return true;
}

// Specific tests for debugging.  These are generally failures taken from
// the conformance file, but culled out to make debugging easier.

void NormalizerConformanceTest::TestCase6() {
    _testOneLine("0385;0385;00A8 0301;0020 0308 0301;0020 0308 0301;");
}

void NormalizerConformanceTest::_testOneLine(const char *line) {
  UErrorCode status = U_ZERO_ERROR;
    UnicodeString fields[FIELD_COUNT];
    if (!hexsplit(line, ';', fields, FIELD_COUNT)) {
        errln((UnicodeString)"Unable to parse line " + line);
    } else {
        checkConformance(fields, line, 0, status);
    }
}

#endif /* #if !UCONFIG_NO_NORMALIZATION */
