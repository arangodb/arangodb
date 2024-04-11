// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2016, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */


#include "unicode/utypes.h"
#include "unicode/ucsdet.h"
#include "unicode/ucnv.h"
#include "unicode/unistr.h"
#include "unicode/putil.h"
#include "unicode/uniset.h"

#include "intltest.h"
#include "csdetest.h"

#include "xmlparser.h"

#include <memory>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_DETECT
#include <stdio.h>
#endif


#define CH_SPACE 0x0020
#define CH_SLASH 0x002F

#define TEST_ASSERT(x) UPRV_BLOCK_MACRO_BEGIN { \
    if (!(x)) { \
        errln("Failure in file %s, line %d", __FILE__, __LINE__); \
    } \
} UPRV_BLOCK_MACRO_END

#define TEST_ASSERT_SUCCESS(errcode) UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(errcode)) { \
        errcheckln(errcode, "Failure in file %s, line %d, status = \"%s\"", __FILE__, __LINE__, u_errorName(errcode)); \
        return; \
    } \
} UPRV_BLOCK_MACRO_END


//---------------------------------------------------------------------------
//
//  Test class boilerplate
//
//---------------------------------------------------------------------------
CharsetDetectionTest::CharsetDetectionTest()
{
}


CharsetDetectionTest::~CharsetDetectionTest()
{
}



void CharsetDetectionTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite CharsetDetectionTest: ");
    switch (index) {
       case 0: name = "ConstructionTest";
            if (exec) ConstructionTest();
            break;

       case 1: name = "UTF8Test";
            if (exec) UTF8Test();
            break;

       case 2: name = "UTF16Test";
            if (exec) UTF16Test();
            break;

       case 3: name = "C1BytesTest";
            if (exec) C1BytesTest();
            break;

       case 4: name = "InputFilterTest";
            if (exec) InputFilterTest();
            break;

       case 5: name = "DetectionTest";
            if (exec) DetectionTest();
            break;
#if !UCONFIG_NO_LEGACY_CONVERSION
       case 6: name = "IBM424Test";
            if (exec) IBM424Test();
            break;

       case 7: name = "IBM420Test";
            if (exec) IBM420Test();
            break;
#else
       case 6:
       case 7: name = "skip"; break;
#endif
       case 8: name = "Ticket6394Test";
            if (exec) Ticket6394Test();
            break;

       case 9: name = "Ticket6954Test";
            if (exec) Ticket6954Test();
            break;

       case 10: name = "Ticket21823Test";
            if (exec) Ticket21823Test();
            break;

        default: name = "";
            break; //needed to end loop
    }
}

static UnicodeString *split(const UnicodeString &src, char16_t ch, int32_t &splits)
{
    int32_t offset = -1;

    splits = 1;
    while((offset = src.indexOf(ch, offset + 1)) >= 0) {
        splits += 1;
    }

    UnicodeString *result = new UnicodeString[splits];

    int32_t start = 0;
    int32_t split = 0;
    int32_t end;

    while((end = src.indexOf(ch, start)) >= 0) {
        src.extractBetween(start, end, result[split++]);
        start = end + 1;
    }

    src.extractBetween(start, src.length(), result[split]);

    return result;
}

static char *extractBytes(const UnicodeString &source, const char *codepage, int32_t &length)
{
    int32_t sLength = source.length();
    char *bytes = nullptr;

    length = source.extract(0, sLength, nullptr, codepage);

    if (length > 0) {
        bytes = new char[length + 1];
        source.extract(0, sLength, bytes, codepage);
    }
    
    return bytes;
}

void CharsetDetectionTest::checkEncoding(const UnicodeString &testString, const UnicodeString &encoding, const UnicodeString &id)
{
    int32_t splits = 0;
    int32_t testLength = testString.length();
    std::unique_ptr<UnicodeString []> eSplit(split(encoding, CH_SLASH, splits));
    UErrorCode status = U_ZERO_ERROR;
    int32_t cpLength = eSplit[0].length();
    char codepage[64];

    u_UCharsToChars(eSplit[0].getBuffer(), codepage, cpLength);
    codepage[cpLength] = '\0';

    LocalUCharsetDetectorPointer csd(ucsdet_open(&status));

    int32_t byteLength = 0;
    std::unique_ptr<char []> bytes(extractBytes(testString, codepage, byteLength));

    if (! bytes) {
#if !UCONFIG_NO_LEGACY_CONVERSION
        dataerrln("Can't open a " + encoding + " converter for " + id);
#endif
        return;
    }

    ucsdet_setText(csd.getAlias(), bytes.get(), byteLength, &status);

    int32_t matchCount = 0;
    const UCharsetMatch **matches = ucsdet_detectAll(csd.getAlias(), &matchCount, &status);


    UnicodeString name(ucsdet_getName(matches[0], &status));
    UnicodeString lang(ucsdet_getLanguage(matches[0], &status));
    char16_t *decoded = nullptr;
    int32_t dLength = 0;

    if (matchCount == 0) {
        errln("Encoding detection failure for " + id + ": expected " + eSplit[0] + ", got no matches");
        return;
    }

    if (name.compare(eSplit[0]) != 0) {
        errln("Encoding detection failure for " + id + ": expected " + eSplit[0] + ", got " + name);

#ifdef DEBUG_DETECT
        for (int32_t m = 0; m < matchCount; m += 1) {
            const char *name = ucsdet_getName(matches[m], &status);
            const char *lang = ucsdet_getLanguage(matches[m], &status);
            int32_t confidence = ucsdet_getConfidence(matches[m], &status);

            printf("%s (%s) %d\n", name, lang, confidence);
        }
#endif
        return;
    }

    if (splits > 1 && lang.compare(eSplit[1]) != 0) {
        errln("Language detection failure for " + id + ", " + eSplit[0] + ": expected " + eSplit[1] + ", got " + lang);
        return;
    }

    decoded = new char16_t[testLength];
    dLength = ucsdet_getUChars(matches[0], decoded, testLength, &status);

    if (testString.compare(decoded, dLength) != 0) {
        errln("Round-trip error for " + id + ", " + eSplit[0] + ": getUChars() didn't yield the original string.");

#ifdef DEBUG_DETECT
        for(int32_t i = 0; i < testLength; i += 1) {
            if(testString[i] != decoded[i]) {
                printf("Strings differ at byte %d\n", i);
                break;
            }
        }
#endif

    }

    delete[] decoded;
}

const char *CharsetDetectionTest::getPath(char buffer[2048], const char *filename) {
    UErrorCode status = U_ZERO_ERROR;
    const char *testDataDirectory = IntlTest::getSourceTestData(status);

    if (U_FAILURE(status)) {
        errln("ERROR: getPath() failed - %s", u_errorName(status));
        return nullptr;
    }

    strcpy(buffer, testDataDirectory);
    strcat(buffer, filename);
    return buffer;
}

void CharsetDetectionTest::ConstructionTest()
{
    IcuTestErrorCode status(*this, "ConstructionTest");
    LocalUCharsetDetectorPointer csd(ucsdet_open(status));
    LocalUEnumerationPointer e(ucsdet_getAllDetectableCharsets(csd.getAlias(), status));
    int32_t count = uenum_count(e.getAlias(), status);

#ifdef DEBUG_DETECT
    printf("There are %d recognizers.\n", count);
#endif

    for(int32_t i = 0; i < count; i += 1) {
        int32_t length;
        const char *name = uenum_next(e.getAlias(), &length, status);

        if(name == nullptr || length <= 0) {
            errln("ucsdet_getAllDetectableCharsets() returned a null or empty name!");
        }

#ifdef DEBUG_DETECT
        printf("%s\n", name);
#endif
    }

    const char* defDisabled[] = {
        "IBM420_rtl", "IBM420_ltr",
        "IBM424_rtl", "IBM424_ltr",
        nullptr
    };

    LocalUEnumerationPointer eActive(ucsdet_getDetectableCharsets(csd.getAlias(), status));
    const char *activeName = nullptr;

    while ((activeName = uenum_next(eActive.getAlias(), nullptr, status))) {
        // the charset must be included in all list
        UBool found = false;

        const char *name = nullptr;
        uenum_reset(e.getAlias(), status);
        while ((name = uenum_next(e.getAlias(), nullptr, status))) {
            if (strcmp(activeName, name) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            errln(UnicodeString(activeName) + " is not included in the all charset list.");
        }

        // some charsets are disabled by default
        found = false;
        for (int32_t i = 0; defDisabled[i] != nullptr; i++) {
            if (strcmp(activeName, defDisabled[i]) == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            errln(UnicodeString(activeName) + " should not be included in the default charset list.");
        }
    }
}

void CharsetDetectionTest::UTF8Test()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString ss = "This is a string with some non-ascii characters that will "
                       "be converted to UTF-8, then shoved through the detection process.  "
                       "\\u0391\\u0392\\u0393\\u0394\\u0395"
                       "Sure would be nice if our source could contain Unicode directly!";
    UnicodeString s = ss.unescape();
    int32_t byteLength = 0, sLength = s.length();
    char *bytes = extractBytes(s, "UTF-8", byteLength);
    UCharsetDetector *csd = ucsdet_open(&status);
    const UCharsetMatch *match;
    char16_t *detected = new char16_t[sLength];

    ucsdet_setText(csd, bytes, byteLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errln("Detection failure for UTF-8: got no matches.");
        goto bail;
    }

    ucsdet_getUChars(match, detected, sLength, &status);

    if (s.compare(detected, sLength) != 0) {
        errln("Round-trip test failed!");
    }

    ucsdet_setDeclaredEncoding(csd, "UTF-8", 5, &status); /* for coverage */

bail:
    delete[] detected;
    delete[] bytes;
    ucsdet_close(csd);
}

void CharsetDetectionTest::UTF16Test()
{
    UErrorCode status = U_ZERO_ERROR;
    /* Notice the BOM on the start of this string */
    char16_t chars[] = {
        0xFEFF, 0x0623, 0x0648, 0x0631, 0x0648, 0x0628, 0x0627, 0x002C,
        0x0020, 0x0628, 0x0631, 0x0645, 0x062c, 0x064a, 0x0627, 0x062a,
        0x0020, 0x0627, 0x0644, 0x062d, 0x0627, 0x0633, 0x0648, 0x0628,
        0x0020, 0x002b, 0x0020, 0x0627, 0x0646, 0x062a, 0x0631, 0x0646,
        0x064a, 0x062a, 0x0000};
    UnicodeString s(chars);
    int32_t beLength = 0, leLength = 0;
    std::unique_ptr<char []>beBytes(extractBytes(s, "UTF-16BE", beLength));
    std::unique_ptr<char []>leBytes(extractBytes(s, "UTF-16LE", leLength));
    LocalUCharsetDetectorPointer csd(ucsdet_open(&status));
    const UCharsetMatch *match;
    const char *name;
    int32_t conf;

    ucsdet_setText(csd.getAlias(), beBytes.get(), beLength, &status);
    match = ucsdet_detect(csd.getAlias(), &status);

    if (match == nullptr) {
        errln("Encoding detection failure for UTF-16BE: got no matches.");
    } else {

        name  = ucsdet_getName(match, &status);
        conf  = ucsdet_getConfidence(match, &status);

        if (strcmp(name, "UTF-16BE") != 0) {
            errln("Encoding detection failure for UTF-16BE: got %s", name);
        } else if (conf != 100) {
            errln("Did not get 100%% confidence for UTF-16BE: got %d", conf);
        }
    }

    ucsdet_setText(csd.getAlias(), leBytes.get(), leLength, &status);
    match = ucsdet_detect(csd.getAlias(), &status);

    if (match == nullptr) {
        errln("Encoding detection failure for UTF-16LE: got no matches.");
        return;
    }

    name  = ucsdet_getName(match, &status);
    conf = ucsdet_getConfidence(match, &status);

    if (strcmp(name, "UTF-16LE") != 0) {
        errln("Encoding detection failure for UTF-16LE: got %s", name);
        return;
    }

    if (conf != 100) {
        errln("Did not get 100%% confidence for UTF-16LE: got %d", conf);
    }
}

void CharsetDetectionTest::InputFilterTest()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString s(u"<a> <lot> <of> <English> <inside> <the> <markup> Un très petit peu de Français. <to> <confuse> <the> <detector>");
    int32_t byteLength = 0;
    char *bytes = extractBytes(s, "ISO-8859-1", byteLength);
    UCharsetDetector *csd = ucsdet_open(&status);
    const UCharsetMatch *match;
    const char *lang, *name;

    ucsdet_enableInputFilter(csd, true);

    if (!ucsdet_isInputFilterEnabled(csd)) {
        errln("ucsdet_enableInputFilter(csd, true) did not enable input filter!");
    }


    ucsdet_setText(csd, bytes, byteLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errln("Turning on the input filter resulted in no matches.");
        goto turn_off;
    }

    name = ucsdet_getName(match, &status);

    if (name == nullptr || strcmp(name, "ISO-8859-1") != 0) {
        errln("Turning on the input filter resulted in %s rather than ISO-8859-1.", name);
    } else {
        lang = ucsdet_getLanguage(match, &status);

        if (lang == nullptr || strcmp(lang, "fr") != 0) {
            errln("Input filter did not strip markup!");
        }
    }

turn_off:
    ucsdet_enableInputFilter(csd, false);
    ucsdet_setText(csd, bytes, byteLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errln("Turning off the input filter resulted in no matches.");
        goto bail;
    }

    name = ucsdet_getName(match, &status);

    if (name == nullptr || strcmp(name, "ISO-8859-1") != 0) {
        errln("Turning off the input filter resulted in %s rather than ISO-8859-1.", name);
    } else {
        lang = ucsdet_getLanguage(match, &status);

        if (lang == nullptr || strcmp(lang, "en") != 0) {
            errln("Unfiltered input did not detect as English!");
        }
    }

bail:
    delete[] bytes;
    ucsdet_close(csd);
}

void CharsetDetectionTest::C1BytesTest()
{
#if !UCONFIG_NO_LEGACY_CONVERSION
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString sISO = "This is a small sample of some English text. Just enough to be sure that it detects correctly.";
    UnicodeString ssWindows("This is another small sample of some English text. Just enough to be sure that it detects correctly. It also includes some \\u201CC1\\u201D bytes.", -1, US_INV);
    UnicodeString sWindows  = ssWindows.unescape();
    int32_t lISO = 0, lWindows = 0;
    char *bISO = extractBytes(sISO, "ISO-8859-1", lISO);
    char *bWindows = extractBytes(sWindows, "windows-1252", lWindows);
    UCharsetDetector *csd = ucsdet_open(&status);
    const UCharsetMatch *match;
    const char *name;

    ucsdet_setText(csd, bWindows, lWindows, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errcheckln(status, "English test with C1 bytes got no matches. - %s", u_errorName(status));
        goto bail;
    }

    name  = ucsdet_getName(match, &status);

    if (strcmp(name, "windows-1252") != 0) {
        errln("English text with C1 bytes does not detect as windows-1252, but as %s", name);
    }

    ucsdet_setText(csd, bISO, lISO, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errln("English text without C1 bytes got no matches.");
        goto bail;
    }

    name  = ucsdet_getName(match, &status);

    if (strcmp(name, "ISO-8859-1") != 0) {
        errln("English text without C1 bytes does not detect as ISO-8859-1, but as %s", name);
    }

bail:
    delete[] bWindows;
    delete[] bISO;

    ucsdet_close(csd);
#endif
}

void CharsetDetectionTest::DetectionTest()
{
#if !UCONFIG_NO_REGULAR_EXPRESSIONS
    UErrorCode status = U_ZERO_ERROR;
    char path[2048];
    const char *testFilePath = getPath(path, "csdetest.xml");

    if (testFilePath == nullptr) {
        return; /* Couldn't get path: error message already output. */
    }

    UXMLParser  *parser = UXMLParser::createParser(status);
    if (U_FAILURE(status)) {
        dataerrln("FAIL: UXMLParser::createParser (%s)", u_errorName(status));
        return;
    }

    UXMLElement *root   = parser->parseFile(testFilePath, status);
    if (!assertSuccess( "parseFile",status)) return;

    UnicodeString test_case = UNICODE_STRING_SIMPLE("test-case");
    UnicodeString id_attr   = UNICODE_STRING_SIMPLE("id");
    UnicodeString enc_attr  = UNICODE_STRING_SIMPLE("encodings");

    const UXMLElement *testCase;
    int32_t tc = 0;

    while((testCase = root->nextChildElement(tc)) != nullptr) {
        if (testCase->getTagName().compare(test_case) == 0) {
            const UnicodeString *id = testCase->getAttribute(id_attr);
            const UnicodeString *encodings = testCase->getAttribute(enc_attr);
            const UnicodeString  text = testCase->getText(true);
            int32_t encodingCount;
            UnicodeString *encodingList = split(*encodings, CH_SPACE, encodingCount);

            for(int32_t e = 0; e < encodingCount; e += 1) {
                checkEncoding(text, encodingList[e], *id);
            }

            delete[] encodingList;
        }
    }

    delete root;
    delete parser;
#endif
}

void CharsetDetectionTest::IBM424Test()
{
#if !UCONFIG_ONLY_HTML_CONVERSION
    UErrorCode status = U_ZERO_ERROR;
    
    static const char16_t chars[] = {
            0x05D4, 0x05E4, 0x05E8, 0x05E7, 0x05DC, 0x05D9, 0x05D8, 0x0020, 0x05D4, 0x05E6, 0x05D1, 0x05D0, 0x05D9, 0x0020, 0x05D4, 0x05E8,
            0x05D0, 0x05E9, 0x05D9, 0x002C, 0x0020, 0x05EA, 0x05EA, 0x0020, 0x05D0, 0x05DC, 0x05D5, 0x05E3, 0x0020, 0x05D0, 0x05D1, 0x05D9,
            0x05D7, 0x05D9, 0x0020, 0x05DE, 0x05E0, 0x05D3, 0x05DC, 0x05D1, 0x05DC, 0x05D9, 0x05D8, 0x002C, 0x0020, 0x05D4, 0x05D5, 0x05E8,
            0x05D4, 0x0020, 0x05E2, 0x05DC, 0x0020, 0x05E4, 0x05EA, 0x05D9, 0x05D7, 0x05EA, 0x0020, 0x05D7, 0x05E7, 0x05D9, 0x05E8, 0x05EA,
            0x0020, 0x05DE, 0x05E6, 0x0022, 0x05D7, 0x0020, 0x05D1, 0x05E2, 0x05E7, 0x05D1, 0x05D5, 0x05EA, 0x0020, 0x05E2, 0x05D3, 0x05D5,
            0x05D9, 0x05D5, 0x05EA, 0x0020, 0x05D7, 0x05D9, 0x05D9, 0x05DC, 0x05D9, 0x0020, 0x05E6, 0x05D4, 0x0022, 0x05DC, 0x0020, 0x05DE,
            0x05DE, 0x05D1, 0x05E6, 0x05E2, 0x0020, 0x05E2, 0x05D5, 0x05E4, 0x05E8, 0x05EA, 0x0020, 0x05D9, 0x05E6, 0x05D5, 0x05E7, 0x05D4,
            0x0020, 0x05D1, 0x002B, 0x0020, 0x05E8, 0x05E6, 0x05D5, 0x05E2, 0x05EA, 0x0020, 0x05E2, 0x05D6, 0x05D4, 0x002E, 0x0020, 0x05DC,
            0x05D3, 0x05D1, 0x05E8, 0x05D9, 0x0020, 0x05D4, 0x05E4, 0x05E6, 0x0022, 0x05E8, 0x002C, 0x0020, 0x05DE, 0x05D4, 0x05E2, 0x05D3,
            0x05D5, 0x05D9, 0x05D5, 0x05EA, 0x0020, 0x05E2, 0x05D5, 0x05DC, 0x05D4, 0x0020, 0x05EA, 0x05DE, 0x05D5, 0x05E0, 0x05D4, 0x0020,
            0x05E9, 0x05DC, 0x0020, 0x0022, 0x05D4, 0x05EA, 0x05E0, 0x05D4, 0x05D2, 0x05D5, 0x05EA, 0x0020, 0x05E4, 0x05E1, 0x05D5, 0x05DC,
            0x05D4, 0x0020, 0x05DC, 0x05DB, 0x05D0, 0x05D5, 0x05E8, 0x05D4, 0x0020, 0x05E9, 0x05DC, 0x0020, 0x05D7, 0x05D9, 0x05D9, 0x05DC,
            0x05D9, 0x05DD, 0x0020, 0x05D1, 0x05DE, 0x05D4, 0x05DC, 0x05DA, 0x0020, 0x05DE, 0x05D1, 0x05E6, 0x05E2, 0x0020, 0x05E2, 0x05D5,
            0x05E4, 0x05E8, 0x05EA, 0x0020, 0x05D9, 0x05E6, 0x05D5, 0x05E7, 0x05D4, 0x0022, 0x002E, 0x0020, 0x05DE, 0x05E0, 0x05D3, 0x05DC,
            0x05D1, 0x05DC, 0x05D9, 0x05D8, 0x0020, 0x05E7, 0x05D9, 0x05D1, 0x05DC, 0x0020, 0x05D0, 0x05EA, 0x0020, 0x05D4, 0x05D7, 0x05DC,
            0x05D8, 0x05EA, 0x05D5, 0x0020, 0x05DC, 0x05D0, 0x05D7, 0x05E8, 0x0020, 0x05E9, 0x05E2, 0x05D9, 0x05D9, 0x05DF, 0x0020, 0x05D1,
            0x05EA, 0x05DE, 0x05DC, 0x05D9, 0x05DC, 0x0020, 0x05D4, 0x05E2, 0x05D3, 0x05D5, 0x05D9, 0x05D5, 0x05EA, 0x0000
    };
    
    static const char16_t chars_reverse[] = {
            0x05EA, 0x05D5, 0x05D9, 0x05D5, 0x05D3, 0x05E2, 0x05D4, 0x0020, 0x05DC, 0x05D9, 0x05DC, 0x05DE, 0x05EA,
            0x05D1, 0x0020, 0x05DF, 0x05D9, 0x05D9, 0x05E2, 0x05E9, 0x0020, 0x05E8, 0x05D7, 0x05D0, 0x05DC, 0x0020, 0x05D5, 0x05EA, 0x05D8,
            0x05DC, 0x05D7, 0x05D4, 0x0020, 0x05EA, 0x05D0, 0x0020, 0x05DC, 0x05D1, 0x05D9, 0x05E7, 0x0020, 0x05D8, 0x05D9, 0x05DC, 0x05D1,
            0x05DC, 0x05D3, 0x05E0, 0x05DE, 0x0020, 0x002E, 0x0022, 0x05D4, 0x05E7, 0x05D5, 0x05E6, 0x05D9, 0x0020, 0x05EA, 0x05E8, 0x05E4,
            0x05D5, 0x05E2, 0x0020, 0x05E2, 0x05E6, 0x05D1, 0x05DE, 0x0020, 0x05DA, 0x05DC, 0x05D4, 0x05DE, 0x05D1, 0x0020, 0x05DD, 0x05D9,
            0x05DC, 0x05D9, 0x05D9, 0x05D7, 0x0020, 0x05DC, 0x05E9, 0x0020, 0x05D4, 0x05E8, 0x05D5, 0x05D0, 0x05DB, 0x05DC, 0x0020, 0x05D4,
            0x05DC, 0x05D5, 0x05E1, 0x05E4, 0x0020, 0x05EA, 0x05D5, 0x05D2, 0x05D4, 0x05E0, 0x05EA, 0x05D4, 0x0022, 0x0020, 0x05DC, 0x05E9,
            0x0020, 0x05D4, 0x05E0, 0x05D5, 0x05DE, 0x05EA, 0x0020, 0x05D4, 0x05DC, 0x05D5, 0x05E2, 0x0020, 0x05EA, 0x05D5, 0x05D9, 0x05D5,
            0x05D3, 0x05E2, 0x05D4, 0x05DE, 0x0020, 0x002C, 0x05E8, 0x0022, 0x05E6, 0x05E4, 0x05D4, 0x0020, 0x05D9, 0x05E8, 0x05D1, 0x05D3,
            0x05DC, 0x0020, 0x002E, 0x05D4, 0x05D6, 0x05E2, 0x0020, 0x05EA, 0x05E2, 0x05D5, 0x05E6, 0x05E8, 0x0020, 0x002B, 0x05D1, 0x0020,
            0x05D4, 0x05E7, 0x05D5, 0x05E6, 0x05D9, 0x0020, 0x05EA, 0x05E8, 0x05E4, 0x05D5, 0x05E2, 0x0020, 0x05E2, 0x05E6, 0x05D1, 0x05DE,
            0x05DE, 0x0020, 0x05DC, 0x0022, 0x05D4, 0x05E6, 0x0020, 0x05D9, 0x05DC, 0x05D9, 0x05D9, 0x05D7, 0x0020, 0x05EA, 0x05D5, 0x05D9,
            0x05D5, 0x05D3, 0x05E2, 0x0020, 0x05EA, 0x05D5, 0x05D1, 0x05E7, 0x05E2, 0x05D1, 0x0020, 0x05D7, 0x0022, 0x05E6, 0x05DE, 0x0020,
            0x05EA, 0x05E8, 0x05D9, 0x05E7, 0x05D7, 0x0020, 0x05EA, 0x05D7, 0x05D9, 0x05EA, 0x05E4, 0x0020, 0x05DC, 0x05E2, 0x0020, 0x05D4,
            0x05E8, 0x05D5, 0x05D4, 0x0020, 0x002C, 0x05D8, 0x05D9, 0x05DC, 0x05D1, 0x05DC, 0x05D3, 0x05E0, 0x05DE, 0x0020, 0x05D9, 0x05D7,
            0x05D9, 0x05D1, 0x05D0, 0x0020, 0x05E3, 0x05D5, 0x05DC, 0x05D0, 0x0020, 0x05EA, 0x05EA, 0x0020, 0x002C, 0x05D9, 0x05E9, 0x05D0,
            0x05E8, 0x05D4, 0x0020, 0x05D9, 0x05D0, 0x05D1, 0x05E6, 0x05D4, 0x0020, 0x05D8, 0x05D9, 0x05DC, 0x05E7, 0x05E8, 0x05E4, 0x05D4,
            0x0000
    };

    int32_t bLength = 0, brLength = 0;
    
    UnicodeString s1(chars);
    UnicodeString s2(chars_reverse);
    
    char *bytes = extractBytes(s1, "IBM424", bLength);
    char *bytes_r = extractBytes(s2, "IBM424", brLength);
    
    UCharsetDetector *csd = ucsdet_open(&status);
	ucsdet_setDetectableCharset(csd, "IBM424_rtl", true, &status);
	ucsdet_setDetectableCharset(csd, "IBM424_ltr", true, &status);
	ucsdet_setDetectableCharset(csd, "IBM420_rtl", true, &status);
	ucsdet_setDetectableCharset(csd, "IBM420_ltr", true, &status);
    if (U_FAILURE(status)) {
        errln("Error opening charset detector. - %s", u_errorName(status));
    }
    const UCharsetMatch *match;
    const char *name;

    ucsdet_setText(csd, bytes, bLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errcheckln(status, "Encoding detection failure for IBM424_rtl: got no matches. - %s", u_errorName(status));
        goto bail;
    }

    name  = ucsdet_getName(match, &status);
    if (strcmp(name, "IBM424_rtl") != 0) {
        errln("Encoding detection failure for IBM424_rtl: got %s", name);
    }
    
    ucsdet_setText(csd, bytes_r, brLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errln("Encoding detection failure for IBM424_ltr: got no matches.");
        goto bail;
    }

    name  = ucsdet_getName(match, &status);
    if (strcmp(name, "IBM424_ltr") != 0) {
        errln("Encoding detection failure for IBM424_ltr: got %s", name);
    }

bail:
    delete[] bytes;
    delete[] bytes_r;
    ucsdet_close(csd);
#endif
}

void CharsetDetectionTest::IBM420Test()
{
#if !UCONFIG_ONLY_HTML_CONVERSION
    UErrorCode status = U_ZERO_ERROR;
    
    static const char16_t chars[] = {
        0x0648, 0x064F, 0x0636, 0x0639, 0x062A, 0x0020, 0x0648, 0x0646, 0x064F, 0x0641, 0x0630, 0x062A, 0x0020, 0x0628, 0x0631, 0x0627,
        0x0645, 0x062C, 0x0020, 0x062A, 0x0623, 0x0645, 0x064A, 0x0646, 0x0020, 0x0639, 0x062F, 0x064A, 0x062F, 0x0629, 0x0020, 0x0641,
        0x064A, 0x0020, 0x0645, 0x0624, 0x0633, 0x0633, 0x0629, 0x0020, 0x0627, 0x0644, 0x062A, 0x0623, 0x0645, 0x064A, 0x0646, 0x0020,
        0x0627, 0x0644, 0x0648, 0x0637, 0x0646, 0x064A, 0x002C, 0x0020, 0x0645, 0x0639, 0x0020, 0x0645, 0x0644, 0x0627, 0x0626, 0x0645,
        0x062A, 0x0647, 0x0627, 0x0020, 0x062F, 0x0627, 0x0626, 0x0645, 0x0627, 0x064B, 0x0020, 0x0644, 0x0644, 0x0627, 0x062D, 0x062A,
        0x064A, 0x0627, 0x062C, 0x0627, 0x062A, 0x0020, 0x0627, 0x0644, 0x0645, 0x062A, 0x063A, 0x064A, 0x0631, 0x0629, 0x0020, 0x0644,
        0x0644, 0x0645, 0x062C, 0x062A, 0x0645, 0x0639, 0x0020, 0x0648, 0x0644, 0x0644, 0x062F, 0x0648, 0x0644, 0x0629, 0x002E, 0x0020,
        0x062A, 0x0648, 0x0633, 0x0639, 0x062A, 0x0020, 0x0648, 0x062A, 0x0637, 0x0648, 0x0631, 0x062A, 0x0020, 0x0627, 0x0644, 0x0645,
        0x0624, 0x0633, 0x0633, 0x0629, 0x0020, 0x0628, 0x0647, 0x062F, 0x0641, 0x0020, 0x0636, 0x0645, 0x0627, 0x0646, 0x0020, 0x0634,
        0x0628, 0x0643, 0x0629, 0x0020, 0x0623, 0x0645, 0x0627, 0x0646, 0x0020, 0x0644, 0x0633, 0x0643, 0x0627, 0x0646, 0x0020, 0x062F,
        0x0648, 0x0644, 0x0629, 0x0020, 0x0627, 0x0633, 0x0631, 0x0627, 0x0626, 0x064A, 0x0644, 0x0020, 0x0628, 0x0648, 0x062C, 0x0647,
        0x0020, 0x0627, 0x0644, 0x0645, 0x062E, 0x0627, 0x0637, 0x0631, 0x0020, 0x0627, 0x0644, 0x0627, 0x0642, 0x062A, 0x0635, 0x0627,
        0x062F, 0x064A, 0x0629, 0x0020, 0x0648, 0x0627, 0x0644, 0x0627, 0x062C, 0x062A, 0x0645, 0x0627, 0x0639, 0x064A, 0x0629, 0x002E,
        0x0000
    };
    static const char16_t chars_reverse[] = {
        0x002E, 0x0629, 0x064A, 0x0639, 0x0627, 0x0645, 0x062A, 0x062C, 0x0627, 0x0644, 0x0627, 0x0648, 0x0020, 0x0629, 0x064A, 0x062F,
        0x0627, 0x0635, 0x062A, 0x0642, 0x0627, 0x0644, 0x0627, 0x0020, 0x0631, 0x0637, 0x0627, 0x062E, 0x0645, 0x0644, 0x0627, 0x0020,
        0x0647, 0x062C, 0x0648, 0x0628, 0x0020, 0x0644, 0x064A, 0x0626, 0x0627, 0x0631, 0x0633, 0x0627, 0x0020, 0x0629, 0x0644, 0x0648,
        0x062F, 0x0020, 0x0646, 0x0627, 0x0643, 0x0633, 0x0644, 0x0020, 0x0646, 0x0627, 0x0645, 0x0623, 0x0020, 0x0629, 0x0643, 0x0628,
        0x0634, 0x0020, 0x0646, 0x0627, 0x0645, 0x0636, 0x0020, 0x0641, 0x062F, 0x0647, 0x0628, 0x0020, 0x0629, 0x0633, 0x0633, 0x0624,
        0x0645, 0x0644, 0x0627, 0x0020, 0x062A, 0x0631, 0x0648, 0x0637, 0x062A, 0x0648, 0x0020, 0x062A, 0x0639, 0x0633, 0x0648, 0x062A,
        0x0020, 0x002E, 0x0629, 0x0644, 0x0648, 0x062F, 0x0644, 0x0644, 0x0648, 0x0020, 0x0639, 0x0645, 0x062A, 0x062C, 0x0645, 0x0644,
        0x0644, 0x0020, 0x0629, 0x0631, 0x064A, 0x063A, 0x062A, 0x0645, 0x0644, 0x0627, 0x0020, 0x062A, 0x0627, 0x062C, 0x0627, 0x064A,
        0x062A, 0x062D, 0x0627, 0x0644, 0x0644, 0x0020, 0x064B, 0x0627, 0x0645, 0x0626, 0x0627, 0x062F, 0x0020, 0x0627, 0x0647, 0x062A,
        0x0645, 0x0626, 0x0627, 0x0644, 0x0645, 0x0020, 0x0639, 0x0645, 0x0020, 0x002C, 0x064A, 0x0646, 0x0637, 0x0648, 0x0644, 0x0627,
        0x0020, 0x0646, 0x064A, 0x0645, 0x0623, 0x062A, 0x0644, 0x0627, 0x0020, 0x0629, 0x0633, 0x0633, 0x0624, 0x0645, 0x0020, 0x064A,
        0x0641, 0x0020, 0x0629, 0x062F, 0x064A, 0x062F, 0x0639, 0x0020, 0x0646, 0x064A, 0x0645, 0x0623, 0x062A, 0x0020, 0x062C, 0x0645,
        0x0627, 0x0631, 0x0628, 0x0020, 0x062A, 0x0630, 0x0641, 0x064F, 0x0646, 0x0648, 0x0020, 0x062A, 0x0639, 0x0636, 0x064F, 0x0648,
        0x0000,
    };
    
    int32_t bLength = 0, brLength = 0;
    
    UnicodeString s1(chars);
    UnicodeString s2(chars_reverse);
    
    char *bytes = extractBytes(s1, "IBM420", bLength);
    char *bytes_r = extractBytes(s2, "IBM420", brLength);
    
    UCharsetDetector *csd = ucsdet_open(&status);
    if (U_FAILURE(status)) {
        errln("Error opening charset detector. - %s", u_errorName(status));
    }
	ucsdet_setDetectableCharset(csd, "IBM424_rtl", true, &status);
	ucsdet_setDetectableCharset(csd, "IBM424_ltr", true, &status);
	ucsdet_setDetectableCharset(csd, "IBM420_rtl", true, &status);
	ucsdet_setDetectableCharset(csd, "IBM420_ltr", true, &status);
    const UCharsetMatch *match;
    const char *name;

    ucsdet_setText(csd, bytes, bLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errcheckln(status, "Encoding detection failure for IBM420_rtl: got no matches. - %s", u_errorName(status));
        goto bail;
    }

    name  = ucsdet_getName(match, &status);
    if (strcmp(name, "IBM420_rtl") != 0) {
        errln("Encoding detection failure for IBM420_rtl: got %s\n", name);
    }
    
    ucsdet_setText(csd, bytes_r, brLength, &status);
    match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        errln("Encoding detection failure for IBM420_ltr: got no matches.\n");
        goto bail;
    }

    name  = ucsdet_getName(match, &status);
    if (strcmp(name, "IBM420_ltr") != 0) {
        errln("Encoding detection failure for IBM420_ltr: got %s\n", name);
    }

bail:
    delete[] bytes;
    delete[] bytes_r;
    ucsdet_close(csd);
#endif
}


void CharsetDetectionTest::Ticket6394Test() {
#if !UCONFIG_NO_CONVERSION
    const char charText[] =  "Here is some random English text that should be detected as ISO-8859-1."
                             "Ticket 6394 claims that ISO-8859-1 will appear in the array of detected "
                             "encodings more than once.  The hop through UnicodeString is for platforms " 
                             "where this char * string is be EBCDIC and needs conversion to Latin1.";
    char latin1Text[sizeof(charText)];
    UnicodeString(charText).extract(0, sizeof(charText)-2, latin1Text, sizeof(latin1Text), "ISO-8859-1");

    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector *csd = ucsdet_open(&status);
    ucsdet_setText(csd, latin1Text, -1, &status);
    if (U_FAILURE(status)) {
        errln("Fail at file %s, line %d.  status = %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    int32_t matchCount = 0;
    const UCharsetMatch **matches = ucsdet_detectAll(csd, &matchCount, &status);
    if (U_FAILURE(status)) {
        errln("Fail at file %s, line %d.  status = %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    UnicodeSet  setOfCharsetNames;    // UnicodeSets can hold strings.
    int32_t i;
    for (i=0; i<matchCount; i++) {
        UnicodeString charSetName(ucsdet_getName(matches[i], &status));
        if (U_FAILURE(status)) {
            errln("Fail at file %s, line %d.  status = %s;  i=%d", __FILE__, __LINE__, u_errorName(status), i);
            status = U_ZERO_ERROR;
        }
        if (setOfCharsetNames.contains(charSetName)) {
            errln("Fail at file %s, line %d ", __FILE__, __LINE__);
            errln(UnicodeString("   Duplicate charset name = ") + charSetName);
        }
        setOfCharsetNames.add(charSetName);
    }
    ucsdet_close(csd);
#endif
}


// Ticket 6954 - trouble with the haveC1Bytes flag that is used to distinguish between
//               similar Windows and non-Windows SBCS encodings. State was kept in the shared
//               Charset Recognizer objects, and could be overwritten.
void CharsetDetectionTest::Ticket6954Test() {
#if !UCONFIG_NO_CONVERSION && !UCONFIG_NO_LEGACY_CONVERSION && !UCONFIG_NO_FORMATTING
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString sISO = "This is a small sample of some English text. Just enough to be sure that it detects correctly.";
    UnicodeString ssWindows("This is another small sample of some English text. Just enough to be sure that it detects correctly."
                            "It also includes some \\u201CC1\\u201D bytes.", -1, US_INV);
    UnicodeString sWindows  = ssWindows.unescape();
    int32_t lISO = 0, lWindows = 0;
    std::unique_ptr<char[]> bISO(extractBytes(sISO, "ISO-8859-1", lISO));
    std::unique_ptr<char[]> bWindows(extractBytes(sWindows, "windows-1252", lWindows));

    // First do a plain vanilla detect of 1252 text

    LocalUCharsetDetectorPointer csd1(ucsdet_open(&status));
    ucsdet_setText(csd1.getAlias(), bWindows.get(), lWindows, &status);
    const UCharsetMatch *match1 = ucsdet_detect(csd1.getAlias(), &status);
    const char *name1 = ucsdet_getName(match1, &status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT(strcmp(name1, "windows-1252")==0);

    // Next, using a completely separate detector, detect some 8859-1 text

    LocalUCharsetDetectorPointer csd2(ucsdet_open(&status));
    ucsdet_setText(csd2.getAlias(), bISO.get(), lISO, &status);
    const UCharsetMatch *match2 = ucsdet_detect(csd2.getAlias(), &status);
    const char *name2 = ucsdet_getName(match2, &status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT(strcmp(name2, "ISO-8859-1")==0);

    // Recheck the 1252 results from the first detector, which should not have been
    //  altered by the use of a different detector.

    name1 = ucsdet_getName(match1, &status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT(strcmp(name1, "windows-1252")==0);
#endif
}


// Ticket 21823 - Issue with Charset Detector for ill-formed input strings. 
//                Its fix involves returning a failure based error code 
//                (U_INVALID_CHAR_FOUND) incase no charsets appear to match the input data.
void CharsetDetectionTest::Ticket21823Test() {
    UErrorCode status = U_ZERO_ERROR;
    std::string str = "\x80";
    UCharsetDetector* csd = ucsdet_open(&status);

    ucsdet_setText(csd, str.data(), str.length(), &status);
    const UCharsetMatch* match = ucsdet_detect(csd, &status);

    if (match == nullptr) {
        TEST_ASSERT(U_FAILURE(status));
    }

    ucsdet_close(csd);
}
