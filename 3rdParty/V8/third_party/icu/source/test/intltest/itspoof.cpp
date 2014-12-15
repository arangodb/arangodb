/*
**********************************************************************
* Copyright (C) 2011-2013, International Business Machines Corporation 
* and others.  All Rights Reserved.
**********************************************************************
*/
/**
 * IntlTestSpoof tests for USpoofDetector
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_REGULAR_EXPRESSIONS && !UCONFIG_NO_NORMALIZATION && !UCONFIG_NO_FILE_IO

#include "itspoof.h"

#include "unicode/normlzr.h"
#include "unicode/regex.h"
#include "unicode/unistr.h"
#include "unicode/uscript.h"
#include "unicode/uspoof.h"

#include "cstring.h"
#include "identifier_info.h"
#include "scriptset.h"
#include "uhash.h"

#include <stdlib.h>
#include <stdio.h>

#define TEST_ASSERT_SUCCESS(status) {if (U_FAILURE(status)) { \
    errcheckln(status, "Failure at file %s, line %d, error = %s", __FILE__, __LINE__, u_errorName(status));}}

#define TEST_ASSERT(expr) {if ((expr)==FALSE) { \
    errln("Test Failure at file %s, line %d: \"%s\" is false.", __FILE__, __LINE__, #expr);};}

#define TEST_ASSERT_MSG(expr, msg) {if ((expr)==FALSE) { \
    dataerrln("Test Failure at file %s, line %d, %s: \"%s\" is false.", __FILE__, __LINE__, msg, #expr);};}

#define TEST_ASSERT_EQ(a, b) { if ((a) != (b)) { \
    errln("Test Failure at file %s, line %d: \"%s\" (%d) != \"%s\" (%d)", \
             __FILE__, __LINE__, #a, (a), #b, (b)); }}

#define TEST_ASSERT_NE(a, b) { if ((a) == (b)) { \
    errln("Test Failure at file %s, line %d: \"%s\" (%d) == \"%s\" (%d)", \
             __FILE__, __LINE__, #a, (a), #b, (b)); }}

#define LENGTHOF(array) ((int32_t)(sizeof(array)/sizeof((array)[0])))

/*
 *   TEST_SETUP and TEST_TEARDOWN
 *         macros to handle the boilerplate around setting up test case.
 *         Put arbitrary test code between SETUP and TEARDOWN.
 *         "sc" is the ready-to-go  SpoofChecker for use in the tests.
 */
#define TEST_SETUP {  \
    UErrorCode status = U_ZERO_ERROR; \
    USpoofChecker *sc;     \
    sc = uspoof_open(&status);  \
    TEST_ASSERT_SUCCESS(status);   \
    if (U_SUCCESS(status)){

#define TEST_TEARDOWN  \
    }  \
    TEST_ASSERT_SUCCESS(status);  \
    uspoof_close(sc);  \
}




void IntlTestSpoof::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite spoof: ");
    switch (index) {
        case 0:
            name = "TestSpoofAPI"; 
            if (exec) {
                testSpoofAPI();
            }
            break;
        case 1:
            name = "TestSkeleton"; 
            if (exec) {
                testSkeleton();
            }
            break;
        case 2:
            name = "TestAreConfusable";
            if (exec) {
                testAreConfusable();
            }
            break;
        case 3:
            name = "TestInvisible";
            if (exec) {
                testInvisible();
            }
            break;
        case 4:
            name = "testConfData";
            if (exec) {
                testConfData();
            }
            break;
        case 5:
            name = "testBug8654";
            if (exec) {
                testBug8654();
            }
            break;
        case 6:
            name = "testIdentifierInfo";
            if (exec) {
                testIdentifierInfo();
            }
            break;
        case 7:
            name = "testScriptSet";
            if (exec) {
                testScriptSet();
            }
            break;
        case 8:
            name = "testRestrictionLevel";
            if (exec) {
                testRestrictionLevel();
            }
            break;
       case 9:
            name = "testMixedNumbers";
            if (exec) {
                testMixedNumbers();
            }
            break;


        default: name=""; break;
    }
}

void IntlTestSpoof::testSpoofAPI() {

    TEST_SETUP
        UnicodeString s("xyz");  // Many latin ranges are whole-script confusable with other scripts.
                                 // If this test starts failing, consult confusablesWholeScript.txt
        int32_t position = 666;
        int32_t checkResults = uspoof_checkUnicodeString(sc, s, &position, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
        TEST_ASSERT_EQ(0, position);
    TEST_TEARDOWN;
    
    TEST_SETUP
        UnicodeString s1("cxs");
        UnicodeString s2 = UnicodeString("\\u0441\\u0445\\u0455").unescape();  // Cyrillic "cxs"
        int32_t checkResults = uspoof_areConfusableUnicodeString(sc, s1, s2, &status);
        TEST_ASSERT_EQ(USPOOF_MIXED_SCRIPT_CONFUSABLE | USPOOF_WHOLE_SCRIPT_CONFUSABLE, checkResults);

    TEST_TEARDOWN;

    TEST_SETUP
        UnicodeString s("I1l0O");
        UnicodeString dest;
        UnicodeString &retStr = uspoof_getSkeletonUnicodeString(sc, USPOOF_ANY_CASE, s, dest, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(UnicodeString("lllOO") == dest);
        TEST_ASSERT(&dest == &retStr);
    TEST_TEARDOWN;
}


#define CHECK_SKELETON(type, input, expected) { \
    checkSkeleton(sc, type, input, expected, __LINE__); \
    }


// testSkeleton.   Spot check a number of confusable skeleton substitutions from the 
//                 Unicode data file confusables.txt
//                 Test cases chosen for substitutions of various lengths, and 
//                 membership in different mapping tables.
void IntlTestSpoof::testSkeleton() {
    const uint32_t ML = 0;
    const uint32_t SL = USPOOF_SINGLE_SCRIPT_CONFUSABLE;
    const uint32_t MA = USPOOF_ANY_CASE;
    const uint32_t SA = USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE;

    TEST_SETUP
        // A long "identifier" that will overflow implementation stack buffers, forcing heap allocations.
        CHECK_SKELETON(SL, " A 1ong \\u02b9identifier' that will overflow implementation stack buffers, forcing heap allocations."
                           " A 1ong 'identifier' that will overflow implementation stack buffers, forcing heap allocations."
                           " A 1ong 'identifier' that will overflow implementation stack buffers, forcing heap allocations."
                           " A 1ong 'identifier' that will overflow implementation stack buffers, forcing heap allocations.",

               " A long 'identifier' that vvill overflovv irnplernentation stack buffers, forcing heap allocations."
               " A long 'identifier' that vvill overflovv irnplernentation stack buffers, forcing heap allocations."
               " A long 'identifier' that vvill overflovv irnplernentation stack buffers, forcing heap allocations."
               " A long 'identifier' that vvill overflovv irnplernentation stack buffers, forcing heap allocations.")

        CHECK_SKELETON(SL, "nochange", "nochange");
        CHECK_SKELETON(MA, "love", "love"); 
        CHECK_SKELETON(MA, "1ove", "love");   // Digit 1 to letter l
        CHECK_SKELETON(ML, "OOPS", "OOPS");
        CHECK_SKELETON(ML, "00PS", "00PS");   // Digit 0 unchanged in lower case mode.
        CHECK_SKELETON(MA, "OOPS", "OOPS");
        CHECK_SKELETON(MA, "00PS", "OOPS");   // Digit 0 to letter O in any case mode only
        CHECK_SKELETON(SL, "\\u059c", "\\u0301");
        CHECK_SKELETON(SL, "\\u2A74", "\\u003A\\u003A\\u003D");
        CHECK_SKELETON(SL, "\\u247E", "\\u0028\\u006C\\u006C\\u0029");  // "(ll)"
        CHECK_SKELETON(SL, "\\uFDFB", "\\u062C\\u0644\\u0020\\u062C\\u0644\\u0627\\u0644\\u0647");

        // This mapping exists in the ML and MA tables, does not exist in SL, SA
        //0C83 ;	0C03 ;	
        CHECK_SKELETON(SL, "\\u0C83", "\\u0C83");
        CHECK_SKELETON(SA, "\\u0C83", "\\u0C83");
        CHECK_SKELETON(ML, "\\u0C83", "\\u0983");
        CHECK_SKELETON(MA, "\\u0C83", "\\u0983");
        
        // 0391 ; 0041 ;
        // This mapping exists only in the MA table.
        CHECK_SKELETON(MA, "\\u0391", "A");
        CHECK_SKELETON(SA, "\\u0391", "\\u0391");
        CHECK_SKELETON(ML, "\\u0391", "\\u0391");
        CHECK_SKELETON(SL, "\\u0391", "\\u0391");

        // 13CF ;  0062 ; 
        // This mapping exists in the ML and MA tables
        CHECK_SKELETON(ML, "\\u13CF", "b");
        CHECK_SKELETON(MA, "\\u13CF", "b");
        CHECK_SKELETON(SL, "\\u13CF", "\\u13CF");
        CHECK_SKELETON(SA, "\\u13CF", "\\u13CF");

        // 0022 ;  0027 0027 ; 
        // all tables.
        CHECK_SKELETON(SL, "\\u0022", "\\u0027\\u0027");
        CHECK_SKELETON(SA, "\\u0022", "\\u0027\\u0027");
        CHECK_SKELETON(ML, "\\u0022", "\\u0027\\u0027");
        CHECK_SKELETON(MA, "\\u0022", "\\u0027\\u0027");

        // 017F ;  0066 ;
        // This mapping exists in the SA and MA tables
        CHECK_SKELETON(MA, "\\u017F", "f");
        CHECK_SKELETON(SA, "\\u017F", "f");

    TEST_TEARDOWN;
}


//
//  Run a single confusable skeleton transformation test case.
//
void IntlTestSpoof::checkSkeleton(const USpoofChecker *sc, uint32_t type, 
                                  const char *input, const char *expected, int32_t lineNum) {
    UnicodeString uInput = UnicodeString(input).unescape();
    UnicodeString uExpected = UnicodeString(expected).unescape();
    
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString actual;
    uspoof_getSkeletonUnicodeString(sc, type, uInput, actual, &status);
    if (U_FAILURE(status)) {
        errln("File %s, Line %d, Test case from line %d, status is %s", __FILE__, __LINE__, lineNum,
              u_errorName(status));
        return;
    }
    if (uExpected != actual) {
        errln("File %s, Line %d, Test case from line %d, Actual and Expected skeletons differ.",
               __FILE__, __LINE__, lineNum);
        errln(UnicodeString(" Actual   Skeleton: \"") + actual + UnicodeString("\"\n") +
              UnicodeString(" Expected Skeleton: \"") + uExpected + UnicodeString("\""));
    }
}

void IntlTestSpoof::testAreConfusable() {
    TEST_SETUP
        UnicodeString s1("A long string that will overflow stack buffers.  A long string that will overflow stack buffers. "
                         "A long string that will overflow stack buffers.  A long string that will overflow stack buffers. ");
        UnicodeString s2("A long string that wi11 overflow stack buffers.  A long string that will overflow stack buffers. "
                         "A long string that wi11 overflow stack buffers.  A long string that will overflow stack buffers. ");
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT_CONFUSABLE, uspoof_areConfusableUnicodeString(sc, s1, s2, &status));
        TEST_ASSERT_SUCCESS(status);

    TEST_TEARDOWN;
}

void IntlTestSpoof::testInvisible() {
    TEST_SETUP
        UnicodeString  s = UnicodeString("abcd\\u0301ef").unescape();
        int32_t position = -42;
        TEST_ASSERT_EQ(0, uspoof_checkUnicodeString(sc, s, &position, &status));
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(0 == position);

        UnicodeString  s2 = UnicodeString("abcd\\u0301\\u0302\\u0301ef").unescape();
        TEST_ASSERT_EQ(USPOOF_INVISIBLE, uspoof_checkUnicodeString(sc, s2, &position, &status));
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, position);

        // Two acute accents, one from the composed a with acute accent, \u00e1,
        // and one separate.
        position = -42;
        UnicodeString  s3 = UnicodeString("abcd\\u00e1\\u0301xyz").unescape();
        TEST_ASSERT_EQ(USPOOF_INVISIBLE, uspoof_checkUnicodeString(sc, s3, &position, &status));
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, position);
    TEST_TEARDOWN;
}

void IntlTestSpoof::testBug8654() {
    TEST_SETUP
        UnicodeString s = UnicodeString("B\\u00c1\\u0301").unescape();
        int32_t position = -42;
        TEST_ASSERT_EQ(USPOOF_INVISIBLE, uspoof_checkUnicodeString(sc, s, &position, &status) & USPOOF_INVISIBLE );
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, position);
    TEST_TEARDOWN;
}

static UnicodeString parseHex(const UnicodeString &in) {
    // Convert a series of hex numbers in a Unicode String to a string with the
    // corresponding characters.
    // The conversion is _really_ annoying.  There must be some function to just do it.
    UnicodeString result;
    UChar32 cc = 0;
    for (int32_t i=0; i<in.length(); i++) {
        UChar c = in.charAt(i);
        if (c == 0x20) {   // Space
            if (cc > 0) {
               result.append(cc);
               cc = 0;
            }
        } else if (c>=0x30 && c<=0x39) {
            cc = (cc<<4) + (c - 0x30);
        } else if ((c>=0x41 && c<=0x46) || (c>=0x61 && c<=0x66)) {
            cc = (cc<<4) + (c & 0x0f)+9;
        }
        // else do something with bad input.
    }
    if (cc > 0) {
        result.append(cc);
    }
    return result;
}


//
// Append the hex form of a UChar32 to a UnicodeString.
// Used in formatting error messages.
// Match the formatting of numbers in confusables.txt
// Minimum of 4 digits, no leading zeroes for positions 5 and up.
//
static void appendHexUChar(UnicodeString &dest, UChar32 c) {
    UBool   doZeroes = FALSE;    
    for (int bitNum=28; bitNum>=0; bitNum-=4) {
        if (bitNum <= 12) {
            doZeroes = TRUE;
        }
        int hexDigit = (c>>bitNum) & 0x0f;
        if (hexDigit != 0 || doZeroes) {
            doZeroes = TRUE;
            dest.append((UChar)(hexDigit<=9? hexDigit + 0x30: hexDigit -10 + 0x41));
        }
    }
    dest.append((UChar)0x20);
}

U_DEFINE_LOCAL_OPEN_POINTER(LocalStdioFilePointer, FILE, fclose);

//  testConfData - Check each data item from the Unicode confusables.txt file,
//                 verify that it transforms correctly in a skeleton.
//
void IntlTestSpoof::testConfData() {
    UErrorCode status = U_ZERO_ERROR;

    const char *testDataDir = IntlTest::getSourceTestData(status);
    TEST_ASSERT_SUCCESS(status);
    char buffer[2000];
    uprv_strcpy(buffer, testDataDir);
    uprv_strcat(buffer, "confusables.txt");

    LocalStdioFilePointer f(fopen(buffer, "rb"));
    if (f.isNull()) {
        errln("Skipping test spoof/testConfData.  File confusables.txt not accessible.");
        return;
    }
    fseek(f.getAlias(), 0, SEEK_END);
    int32_t  fileSize = ftell(f.getAlias());
    LocalArray<char> fileBuf(new char[fileSize]);
    fseek(f.getAlias(), 0, SEEK_SET);
    int32_t amt_read = fread(fileBuf.getAlias(), 1, fileSize, f.getAlias());
    TEST_ASSERT_EQ(amt_read, fileSize);
    TEST_ASSERT(fileSize>0);
    if (amt_read != fileSize || fileSize <=0) {
        return;
    }
    UnicodeString confusablesTxt = UnicodeString::fromUTF8(StringPiece(fileBuf.getAlias(), fileSize));

    LocalUSpoofCheckerPointer sc(uspoof_open(&status));
    TEST_ASSERT_SUCCESS(status);

    // Parse lines from the confusables.txt file.  Example Line:
    // FF44 ;	0064 ;	SL	# ( d -> d ) FULLWIDTH ....
    // Three fields.  The hex fields can contain more than one character,
    //                and each character may be more than 4 digits (for supplemntals)
    // This regular expression matches lines and splits the fields into capture groups.
    RegexMatcher parseLine("(?m)^([0-9A-F]{4}[^#;]*?);([^#;]*?);([^#]*)", confusablesTxt, 0, status);
    TEST_ASSERT_SUCCESS(status);
    while (parseLine.find()) {
        UnicodeString from = parseHex(parseLine.group(1, status));
        if (!Normalizer::isNormalized(from, UNORM_NFD, status)) {
            // The source character was not NFD.
            // Skip this case; the first step in obtaining a skeleton is to NFD the input,
            //  so the mapping in this line of confusables.txt will never be applied.
            continue;
        }

        UnicodeString rawExpected = parseHex(parseLine.group(2, status));
        UnicodeString expected;
        Normalizer::decompose(rawExpected, FALSE /*NFD*/, 0, expected, status);
        TEST_ASSERT_SUCCESS(status);

        int32_t skeletonType = 0;
        UnicodeString tableType = parseLine.group(3, status);
        TEST_ASSERT_SUCCESS(status);
        if (tableType.indexOf("SL") >= 0) {
            skeletonType = USPOOF_SINGLE_SCRIPT_CONFUSABLE;
        } else if (tableType.indexOf("SA") >= 0) {
            skeletonType = USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE;
        } else if (tableType.indexOf("ML") >= 0) {
            skeletonType = 0;
        } else if (tableType.indexOf("MA") >= 0) {
            skeletonType = USPOOF_ANY_CASE;
        }

        UnicodeString actual;
        uspoof_getSkeletonUnicodeString(sc.getAlias(), skeletonType, from, actual, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(actual == expected);
        if (actual != expected) {
            errln(parseLine.group(0, status));
            UnicodeString line = "Actual: ";
            int i = 0;
            while (i < actual.length()) {
                appendHexUChar(line, actual.char32At(i));
                i = actual.moveIndex32(i, 1);
            }
            errln(line);
        }
        if (U_FAILURE(status)) {
            break;
        }
    }
}

// testIdentifierInfo. Note that IdentifierInfo is not public ICU API at this time
void IntlTestSpoof::testIdentifierInfo() {
    UErrorCode status = U_ZERO_ERROR;
    ScriptSet bitset12; bitset12.set(USCRIPT_LATIN, status).set(USCRIPT_HANGUL, status);
    ScriptSet bitset2;  bitset2.set(USCRIPT_HANGUL, status);
    TEST_ASSERT(bitset12.contains(bitset2));
    TEST_ASSERT(bitset12.contains(bitset12));
    TEST_ASSERT(!bitset2.contains(bitset12));

    ScriptSet arabSet;  arabSet.set(USCRIPT_ARABIC, status);
    ScriptSet latinSet; latinSet.set(USCRIPT_LATIN, status);
    UElement arabEl;  arabEl.pointer = &arabSet;
    UElement latinEl; latinEl.pointer = &latinSet;
    TEST_ASSERT(uhash_compareScriptSet(arabEl, latinEl) < 0);
    TEST_ASSERT(uhash_compareScriptSet(latinEl, arabEl) > 0);

    UnicodeString scriptString;
    bitset12.displayScripts(scriptString);
    TEST_ASSERT(UNICODE_STRING_SIMPLE("Hang Latn") == scriptString);

    status = U_ZERO_ERROR;
    UHashtable *alternates = uhash_open(uhash_hashScriptSet ,uhash_compareScriptSet, NULL, &status);
    uhash_puti(alternates, &bitset12, 1, &status);
    uhash_puti(alternates, &bitset2, 1, &status);
    UnicodeString alternatesString;
    IdentifierInfo::displayAlternates(alternatesString, alternates, status);
    TEST_ASSERT(UNICODE_STRING_SIMPLE("Hang; Hang Latn") == alternatesString);
    TEST_ASSERT_SUCCESS(status);

    status = U_ZERO_ERROR;
    ScriptSet tScriptSet;
    tScriptSet.parseScripts(scriptString, status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT(bitset12 == tScriptSet);
    UnicodeString ss;
    ss.remove();
    uhash_close(alternates);

    struct Test {
        const char         *fTestString;
        URestrictionLevel   fRestrictionLevel;
        const char         *fNumerics;
        const char         *fScripts;
        const char         *fAlternates;
        const char         *fCommonAlternates;
    } tests[] = {
            {"\\u0061\\u2665",                USPOOF_UNRESTRICTIVE,      "[]", "Latn", "", ""},
            {"\\u0061\\u3006",                USPOOF_HIGHLY_RESTRICTIVE, "[]", "Latn", "Hani Hira Kana", "Hani Hira Kana"},
            {"\\u0061\\u30FC\\u3006",         USPOOF_HIGHLY_RESTRICTIVE, "[]", "Latn", "Hira Kana", "Hira Kana"},
            {"\\u0061\\u30FC\\u3006\\u30A2",  USPOOF_HIGHLY_RESTRICTIVE, "[]", "Latn Kana", "", ""},
            {"\\u30A2\\u0061\\u30FC\\u3006",  USPOOF_HIGHLY_RESTRICTIVE, "[]", "Latn Kana", "", ""},
            {"\\u0061\\u0031\\u0661",         USPOOF_UNRESTRICTIVE,      "[\\u0030\\u0660]", "Latn", "Arab Thaa", "Arab Thaa"},
            {"\\u0061\\u0031\\u0661\\u06F1",  USPOOF_UNRESTRICTIVE,      "[\\u0030\\u0660\\u06F0]", "Latn Arab", "", ""},
            {"\\u0661\\u30FC\\u3006\\u0061\\u30A2\\u0031\\u0967\\u06F1",  USPOOF_UNRESTRICTIVE, 
                  "[\\u0030\\u0660\\u06F0\\u0966]", "Latn Kana Arab", "Deva Kthi", "Deva Kthi"},
            {"\\u0061\\u30A2\\u30FC\\u3006\\u0031\\u0967\\u0661\\u06F1",  USPOOF_UNRESTRICTIVE, 
                  "[\\u0030\\u0660\\u06F0\\u0966]", "Latn Kana Arab", "Deva Kthi", "Deva Kthi"}
    };

    int testNum;
    for (testNum = 0; testNum < LENGTHOF(tests); testNum++) {
        char testNumStr[40];
        sprintf(testNumStr, "testNum = %d", testNum);
        Test &test = tests[testNum];
        status = U_ZERO_ERROR;
        UnicodeString testString(test.fTestString);  // Note: may do charset conversion.
        testString = testString.unescape();
        IdentifierInfo idInfo(status);
        TEST_ASSERT_SUCCESS(status);
        idInfo.setIdentifierProfile(*uspoof_getRecommendedUnicodeSet(&status));
        idInfo.setIdentifier(testString, status);
        TEST_ASSERT_MSG(*idInfo.getIdentifier() == testString, testNumStr);

        URestrictionLevel restrictionLevel = test.fRestrictionLevel;
        TEST_ASSERT_MSG(restrictionLevel == idInfo.getRestrictionLevel(status), testNumStr);
        
        status = U_ZERO_ERROR;
        UnicodeSet numerics(UnicodeString(test.fNumerics).unescape(), status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_MSG(numerics == *idInfo.getNumerics(), testNumStr);

        ScriptSet scripts;
        scripts.parseScripts(UnicodeString(test.fScripts), status);
        TEST_ASSERT_MSG(scripts == *idInfo.getScripts(), testNumStr);

        UnicodeString alternatesStr;
        IdentifierInfo::displayAlternates(alternatesStr, idInfo.getAlternates(), status);
        TEST_ASSERT_MSG(UnicodeString(test.fAlternates) == alternatesStr, testNumStr);

        ScriptSet commonAlternates;
        commonAlternates.parseScripts(UnicodeString(test.fCommonAlternates), status);
        TEST_ASSERT_MSG(commonAlternates == *idInfo.getCommonAmongAlternates(), testNumStr);
    }

    // Test of getScriptCount()
    //   Script and or Script Extension for chars used in the tests
    //     \\u3013  ; Bopo Hang Hani Hira Kana # So       GETA MARK
    //     \\uA838  ; Deva Gujr Guru Kthi Takr # Sc       NORTH INDIC RUPEE MARK
    //     \\u0951  ; Deva Latn                # Mn       DEVANAGARI STRESS SIGN UDATTA
    //
    //     \\u0370  ; Greek                    # L        GREEK CAPITAL LETTER HETA
    //     \\u0481  ; Cyrillic                 # L&       CYRILLIC SMALL LETTER KOPPA
    //     \\u0904  ; Devanagari               # Lo       DEVANAGARI LETTER SHORT A
    //     \\u3041  ; Hiragana                 # Lo       HIRAGANA LETTER SMALL A
    //     1234     ; Common                   #          ascii digits
    //     \\u0300  ; Inherited                # Mn       COMBINING GRAVE ACCENT
    
    struct ScriptTest {
        const char *fTestString;
        int32_t     fScriptCount;
    } scriptTests[] = {
        {"Hello", 1},
        {"Hello\\u0370", 2},
        {"1234", 0},
        {"Hello1234\\u0300", 1},   // Common and Inherited are ignored.
        {"\\u0030", 0},
        {"abc\\u0951", 1},
        {"abc\\u3013", 2},
        {"\\uA838\\u0951", 1},     // Triggers commonAmongAlternates path.
        {"\\u3013\\uA838", 2}
    };

    status = U_ZERO_ERROR;
    IdentifierInfo identifierInfo(status);
    for (testNum=0; testNum<LENGTHOF(scriptTests); testNum++) {
        ScriptTest &test = scriptTests[testNum];
        char msgBuf[100];
        sprintf(msgBuf, "testNum = %d ", testNum);
        UnicodeString testString = UnicodeString(test.fTestString).unescape();
        
        status = U_ZERO_ERROR;
        identifierInfo.setIdentifier(testString, status);
        int32_t scriptCount = identifierInfo.getScriptCount();
        TEST_ASSERT_MSG(test.fScriptCount == scriptCount, msgBuf);
    }
}

void IntlTestSpoof::testScriptSet() {
    ScriptSet s1;
    ScriptSet s2;
    UErrorCode status = U_ZERO_ERROR;

    TEST_ASSERT(s1 == s2);
    s1.set(USCRIPT_ARABIC,status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT(!(s1 == s2));
    TEST_ASSERT(s1.test(USCRIPT_ARABIC, status));
    TEST_ASSERT(s1.test(USCRIPT_GREEK, status) == FALSE);

    status = U_ZERO_ERROR;
    s1.reset(USCRIPT_ARABIC, status);
    TEST_ASSERT(s1 == s2);

    status = U_ZERO_ERROR;
    s1.setAll();
    TEST_ASSERT(s1.test(USCRIPT_COMMON, status));
    TEST_ASSERT(s1.test(USCRIPT_ETHIOPIC, status));
    TEST_ASSERT(s1.test(USCRIPT_CODE_LIMIT, status));
    s1.resetAll();
    TEST_ASSERT(!s1.test(USCRIPT_COMMON, status));
    TEST_ASSERT(!s1.test(USCRIPT_ETHIOPIC, status));
    TEST_ASSERT(!s1.test(USCRIPT_CODE_LIMIT, status));

    status = U_ZERO_ERROR;
    s1.set(USCRIPT_TAKRI, status);
    s1.set(USCRIPT_BLISSYMBOLS, status);
    s2.setAll();
    TEST_ASSERT(s2.contains(s1));
    TEST_ASSERT(!s1.contains(s2));
    TEST_ASSERT(s2.intersects(s1));
    TEST_ASSERT(s1.intersects(s2));
    s2.reset(USCRIPT_TAKRI, status);
    TEST_ASSERT(!s2.contains(s1));
    TEST_ASSERT(!s1.contains(s2));
    TEST_ASSERT(s1.intersects(s2));
    TEST_ASSERT(s2.intersects(s1));
    TEST_ASSERT_SUCCESS(status);

    status = U_ZERO_ERROR;
    s1.resetAll();
    s1.set(USCRIPT_NKO, status);
    s1.set(USCRIPT_COMMON, status);
    s2 = s1;
    TEST_ASSERT(s2 == s1);
    TEST_ASSERT_EQ(2, s2.countMembers());
    s2.intersect(s1);
    TEST_ASSERT(s2 == s1);
    s2.setAll();
    TEST_ASSERT(!(s2 == s1));
    TEST_ASSERT(s2.countMembers() >= USCRIPT_CODE_LIMIT);
    s2.intersect(s1);
    TEST_ASSERT(s2 == s1);
    
    s2.setAll();
    s2.reset(USCRIPT_COMMON, status);
    s2.intersect(s1);
    TEST_ASSERT(s2.countMembers() == 1);

    s1.resetAll();
    s1.set(USCRIPT_AFAKA, status);
    s1.set(USCRIPT_VAI, status);
    s1.set(USCRIPT_INHERITED, status);
    int32_t n = -1;
    for (int32_t i=0; i<4; i++) {
        n = s1.nextSetBit(n+1);
        switch (i) {
          case 0: TEST_ASSERT_EQ(USCRIPT_INHERITED, n); break;
          case 1: TEST_ASSERT_EQ(USCRIPT_VAI, n); break;
          case 2: TEST_ASSERT_EQ(USCRIPT_AFAKA, n); break;
          case 3: TEST_ASSERT_EQ(-1, (int32_t)n); break;
          default: TEST_ASSERT(FALSE);
        }
    }
    TEST_ASSERT_SUCCESS(status);
}


void IntlTestSpoof::testRestrictionLevel() {
    struct Test {
        const char         *fId;
        URestrictionLevel   fExpectedRestrictionLevel;
    } tests[] = {
        {"\\u0061\\u03B3\\u2665", USPOOF_UNRESTRICTIVE},
        {"a",                     USPOOF_ASCII},
        {"\\u03B3",               USPOOF_HIGHLY_RESTRICTIVE},
        {"\\u0061\\u30A2\\u30FC", USPOOF_HIGHLY_RESTRICTIVE},
        {"\\u0061\\u0904",        USPOOF_MODERATELY_RESTRICTIVE},
        {"\\u0061\\u03B3",        USPOOF_MINIMALLY_RESTRICTIVE}
    };
    char msgBuffer[100];

    URestrictionLevel restrictionLevels[] = { USPOOF_ASCII, USPOOF_HIGHLY_RESTRICTIVE, 
         USPOOF_MODERATELY_RESTRICTIVE, USPOOF_MINIMALLY_RESTRICTIVE, USPOOF_UNRESTRICTIVE};
    
    UErrorCode status = U_ZERO_ERROR;
    IdentifierInfo idInfo(status);
    TEST_ASSERT_SUCCESS(status);
    idInfo.setIdentifierProfile(*uspoof_getRecommendedUnicodeSet(&status));
    TEST_ASSERT_SUCCESS(status);
    for (int32_t testNum=0; testNum < LENGTHOF(tests); testNum++) {
        status = U_ZERO_ERROR;
        const Test &test = tests[testNum];
        UnicodeString testString = UnicodeString(test.fId).unescape();
        URestrictionLevel expectedLevel = test.fExpectedRestrictionLevel;
        idInfo.setIdentifier(testString, status);
        sprintf(msgBuffer, "testNum = %d ", testNum);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_MSG(expectedLevel == idInfo.getRestrictionLevel(status), msgBuffer);
        for (int levelIndex=0; levelIndex<LENGTHOF(restrictionLevels); levelIndex++) {
            status = U_ZERO_ERROR;
            URestrictionLevel levelSetInSpoofChecker = restrictionLevels[levelIndex];
            USpoofChecker *sc = uspoof_open(&status);
            uspoof_setChecks(sc, USPOOF_RESTRICTION_LEVEL, &status);
            uspoof_setAllowedChars(sc, uspoof_getRecommendedSet(&status), &status);
            uspoof_setRestrictionLevel(sc, levelSetInSpoofChecker);
            UBool actualValue = uspoof_checkUnicodeString(sc, testString, NULL, &status) != 0;

            // we want to fail if the text is (say) MODERATE and the testLevel is ASCII
            UBool expectedFailure = expectedLevel > levelSetInSpoofChecker ||
                                    !uspoof_getRecommendedUnicodeSet(&status)->containsAll(testString);
            sprintf(msgBuffer, "testNum = %d, levelIndex = %d", testNum, levelIndex);
            TEST_ASSERT_MSG(expectedFailure == actualValue, msgBuffer);
            TEST_ASSERT_SUCCESS(status);
            uspoof_close(sc);
        }
    }
}


void IntlTestSpoof::testMixedNumbers() {
    struct Test {
        const char *fTestString;
        const char *fExpectedSet;
    } tests[] = {
        {"1",              "[0]"},
        {"\\u0967",        "[\\u0966]"},
        {"1\\u0967",       "[0\\u0966]"},
        {"\\u0661\\u06F1", "[\\u0660\\u06F0]"}
    };
    UErrorCode status = U_ZERO_ERROR;
    IdentifierInfo idInfo(status);
    for (int32_t testNum=0; testNum < LENGTHOF(tests); testNum++) {
        char msgBuf[100];
        sprintf(msgBuf, "testNum = %d ", testNum);
        Test &test = tests[testNum];

        status = U_ZERO_ERROR;
        UnicodeString testString = UnicodeString(test.fTestString).unescape();
        UnicodeSet expectedSet(UnicodeString(test.fExpectedSet).unescape(), status);
        idInfo.setIdentifier(testString, status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_MSG(expectedSet == *idInfo.getNumerics(), msgBuf);

        status = U_ZERO_ERROR;
        USpoofChecker *sc = uspoof_open(&status);
        uspoof_setChecks(sc, USPOOF_MIXED_NUMBERS, &status); // only check this
        int32_t result = uspoof_checkUnicodeString(sc, testString, NULL, &status);
        UBool mixedNumberFailure = ((result & USPOOF_MIXED_NUMBERS) != 0);
        TEST_ASSERT_MSG((expectedSet.size() > 1) == mixedNumberFailure, msgBuf);
        uspoof_close(sc);
    }
}

#endif /* !UCONFIG_NO_REGULAR_EXPRESSIONS && !UCONFIG_NO_NORMALIZATION && !UCONFIG_NO_FILE_IO */
