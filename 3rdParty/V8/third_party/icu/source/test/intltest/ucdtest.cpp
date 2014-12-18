/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/ustring.h"
#include "unicode/uchar.h"
#include "unicode/uniset.h"
#include "unicode/putil.h"
#include "unicode/uscript.h"
#include "cstring.h"
#include "hash.h"
#include "patternprops.h"
#include "normalizer2impl.h"
#include "uparse.h"
#include "ucdtest.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof(array[0]))

static const char *ignorePropNames[]={
    "FC_NFKC",
    "NFD_QC",
    "NFC_QC",
    "NFKD_QC",
    "NFKC_QC",
    "Expands_On_NFD",
    "Expands_On_NFC",
    "Expands_On_NFKD",
    "Expands_On_NFKC",
    "NFKC_CF"
};

UnicodeTest::UnicodeTest()
{
    UErrorCode errorCode=U_ZERO_ERROR;
    unknownPropertyNames=new U_NAMESPACE_QUALIFIER Hashtable(errorCode);
    if(U_FAILURE(errorCode)) {
        delete unknownPropertyNames;
        unknownPropertyNames=NULL;
    }
    // Ignore some property names altogether.
    for(int32_t i=0; i<LENGTHOF(ignorePropNames); ++i) {
        unknownPropertyNames->puti(UnicodeString(ignorePropNames[i], -1, US_INV), 1, errorCode);
    }
}

UnicodeTest::~UnicodeTest()
{
    delete unknownPropertyNames;
}

void UnicodeTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if(exec) {
        logln("TestSuite UnicodeTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestAdditionalProperties);
    TESTCASE_AUTO(TestBinaryValues);
    TESTCASE_AUTO(TestConsistency);
    TESTCASE_AUTO(TestPatternProperties);
    TESTCASE_AUTO(TestScriptMetadata);
    TESTCASE_AUTO(TestBidiPairedBracketType);
    TESTCASE_AUTO_END;
}

//====================================================
// private data used by the tests
//====================================================

// test DerivedCoreProperties.txt -------------------------------------------

// copied from genprops.c
static int32_t
getTokenIndex(const char *const tokens[], int32_t countTokens, const char *s) {
    const char *t, *z;
    int32_t i, j;

    s=u_skipWhitespace(s);
    for(i=0; i<countTokens; ++i) {
        t=tokens[i];
        if(t!=NULL) {
            for(j=0;; ++j) {
                if(t[j]!=0) {
                    if(s[j]!=t[j]) {
                        break;
                    }
                } else {
                    z=u_skipWhitespace(s+j);
                    if(*z==';' || *z==0) {
                        return i;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    return -1;
}

static const char *const
derivedPropsNames[]={
    "Math",
    "Alphabetic",
    "Lowercase",
    "Uppercase",
    "ID_Start",
    "ID_Continue",
    "XID_Start",
    "XID_Continue",
    "Default_Ignorable_Code_Point",
    "Full_Composition_Exclusion",
    "Grapheme_Extend",
    "Grapheme_Link", /* Unicode 5 moves this property here from PropList.txt */
    "Grapheme_Base",
    "Cased",
    "Case_Ignorable",
    "Changes_When_Lowercased",
    "Changes_When_Uppercased",
    "Changes_When_Titlecased",
    "Changes_When_Casefolded",
    "Changes_When_Casemapped",
    "Changes_When_NFKC_Casefolded"
};

static const UProperty
derivedPropsIndex[]={
    UCHAR_MATH,
    UCHAR_ALPHABETIC,
    UCHAR_LOWERCASE,
    UCHAR_UPPERCASE,
    UCHAR_ID_START,
    UCHAR_ID_CONTINUE,
    UCHAR_XID_START,
    UCHAR_XID_CONTINUE,
    UCHAR_DEFAULT_IGNORABLE_CODE_POINT,
    UCHAR_FULL_COMPOSITION_EXCLUSION,
    UCHAR_GRAPHEME_EXTEND,
    UCHAR_GRAPHEME_LINK,
    UCHAR_GRAPHEME_BASE,
    UCHAR_CASED,
    UCHAR_CASE_IGNORABLE,
    UCHAR_CHANGES_WHEN_LOWERCASED,
    UCHAR_CHANGES_WHEN_UPPERCASED,
    UCHAR_CHANGES_WHEN_TITLECASED,
    UCHAR_CHANGES_WHEN_CASEFOLDED,
    UCHAR_CHANGES_WHEN_CASEMAPPED,
    UCHAR_CHANGES_WHEN_NFKC_CASEFOLDED
};

static int32_t numErrors[LENGTHOF(derivedPropsIndex)]={ 0 };

enum { MAX_ERRORS=50 };

U_CFUNC void U_CALLCONV
derivedPropsLineFn(void *context,
                   char *fields[][2], int32_t /* fieldCount */,
                   UErrorCode *pErrorCode)
{
    UnicodeTest *me=(UnicodeTest *)context;
    uint32_t start, end;
    int32_t i;

    u_parseCodePointRange(fields[0][0], &start, &end, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        me->errln("UnicodeTest: syntax error in DerivedCoreProperties.txt or DerivedNormalizationProps.txt field 0 at %s\n", fields[0][0]);
        return;
    }

    /* parse derived binary property name, ignore unknown names */
    i=getTokenIndex(derivedPropsNames, LENGTHOF(derivedPropsNames), fields[1][0]);
    if(i<0) {
        UnicodeString propName(fields[1][0], (int32_t)(fields[1][1]-fields[1][0]));
        propName.trim();
        if(me->unknownPropertyNames->find(propName)==NULL) {
            UErrorCode errorCode=U_ZERO_ERROR;
            me->unknownPropertyNames->puti(propName, 1, errorCode);
            me->errln("UnicodeTest warning: unknown property name '%s' in DerivedCoreProperties.txt or DerivedNormalizationProps.txt\n", fields[1][0]);
        }
        return;
    }

    me->derivedProps[i].add(start, end);
}

void UnicodeTest::TestAdditionalProperties() {
#if !UCONFIG_NO_NORMALIZATION
    // test DerivedCoreProperties.txt and DerivedNormalizationProps.txt
    if(LENGTHOF(derivedProps)<LENGTHOF(derivedPropsNames)) {
        errln("error: UnicodeTest::derivedProps[] too short, need at least %d UnicodeSets\n",
              LENGTHOF(derivedPropsNames));
        return;
    }
    if(LENGTHOF(derivedPropsIndex)!=LENGTHOF(derivedPropsNames)) {
        errln("error in ucdtest.cpp: LENGTHOF(derivedPropsIndex)!=LENGTHOF(derivedPropsNames)\n");
        return;
    }

    char newPath[256];
    char backupPath[256];
    char *fields[2][2];
    UErrorCode errorCode=U_ZERO_ERROR;

    /* Look inside ICU_DATA first */
    strcpy(newPath, pathToDataDirectory());
    strcat(newPath, "unidata" U_FILE_SEP_STRING "DerivedCoreProperties.txt");

    // As a fallback, try to guess where the source data was located
    // at the time ICU was built, and look there.
#   ifdef U_TOPSRCDIR
        strcpy(backupPath, U_TOPSRCDIR  U_FILE_SEP_STRING "data");
#   else
        strcpy(backupPath, loadTestData(errorCode));
        strcat(backupPath, U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING "data");
#   endif
    strcat(backupPath, U_FILE_SEP_STRING);
    strcat(backupPath, "unidata" U_FILE_SEP_STRING "DerivedCoreProperties.txt");

    char *path=newPath;
    u_parseDelimitedFile(newPath, ';', fields, 2, derivedPropsLineFn, this, &errorCode);

    if(errorCode==U_FILE_ACCESS_ERROR) {
        errorCode=U_ZERO_ERROR;
        path=backupPath;
        u_parseDelimitedFile(backupPath, ';', fields, 2, derivedPropsLineFn, this, &errorCode);
    }
    if(U_FAILURE(errorCode)) {
        errln("error parsing DerivedCoreProperties.txt: %s\n", u_errorName(errorCode));
        return;
    }
    char *basename=path+strlen(path)-strlen("DerivedCoreProperties.txt");
    strcpy(basename, "DerivedNormalizationProps.txt");
    u_parseDelimitedFile(path, ';', fields, 2, derivedPropsLineFn, this, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("error parsing DerivedNormalizationProps.txt: %s\n", u_errorName(errorCode));
        return;
    }

    // now we have all derived core properties in the UnicodeSets
    // run them all through the API
    int32_t rangeCount, range;
    uint32_t i;
    UChar32 start, end;

    // test all TRUE properties
    for(i=0; i<LENGTHOF(derivedPropsNames); ++i) {
        rangeCount=derivedProps[i].getRangeCount();
        for(range=0; range<rangeCount && numErrors[i]<MAX_ERRORS; ++range) {
            start=derivedProps[i].getRangeStart(range);
            end=derivedProps[i].getRangeEnd(range);
            for(; start<=end; ++start) {
                if(!u_hasBinaryProperty(start, derivedPropsIndex[i])) {
                    dataerrln("UnicodeTest error: u_hasBinaryProperty(U+%04lx, %s)==FALSE is wrong", start, derivedPropsNames[i]);
                    if(++numErrors[i]>=MAX_ERRORS) {
                      dataerrln("Too many errors, moving to the next test");
                      break;
                    }
                }
            }
        }
    }

    // invert all properties
    for(i=0; i<LENGTHOF(derivedPropsNames); ++i) {
        derivedProps[i].complement();
    }

    // test all FALSE properties
    for(i=0; i<LENGTHOF(derivedPropsNames); ++i) {
        rangeCount=derivedProps[i].getRangeCount();
        for(range=0; range<rangeCount && numErrors[i]<MAX_ERRORS; ++range) {
            start=derivedProps[i].getRangeStart(range);
            end=derivedProps[i].getRangeEnd(range);
            for(; start<=end; ++start) {
                if(u_hasBinaryProperty(start, derivedPropsIndex[i])) {
                    errln("UnicodeTest error: u_hasBinaryProperty(U+%04lx, %s)==TRUE is wrong\n", start, derivedPropsNames[i]);
                    if(++numErrors[i]>=MAX_ERRORS) {
                      errln("Too many errors, moving to the next test");
                      break;
                    }
                }
            }
        }
    }
#endif /* !UCONFIG_NO_NORMALIZATION */
}

void UnicodeTest::TestBinaryValues() {
    /*
     * Unicode 5.1 explicitly defines binary property value aliases.
     * Verify that they are all recognized.
     */
    UErrorCode errorCode=U_ZERO_ERROR;
    UnicodeSet alpha(UNICODE_STRING_SIMPLE("[:Alphabetic:]"), errorCode);
    if(U_FAILURE(errorCode)) {
        dataerrln("UnicodeSet([:Alphabetic:]) failed - %s", u_errorName(errorCode));
        return;
    }

    static const char *const falseValues[]={ "N", "No", "F", "False" };
    static const char *const trueValues[]={ "Y", "Yes", "T", "True" };
    int32_t i;
    for(i=0; i<LENGTHOF(falseValues); ++i) {
        UnicodeString pattern=UNICODE_STRING_SIMPLE("[:Alphabetic=:]");
        pattern.insert(pattern.length()-2, UnicodeString(falseValues[i], -1, US_INV));
        errorCode=U_ZERO_ERROR;
        UnicodeSet set(pattern, errorCode);
        if(U_FAILURE(errorCode)) {
            errln("UnicodeSet([:Alphabetic=%s:]) failed - %s\n", falseValues[i], u_errorName(errorCode));
            continue;
        }
        set.complement();
        if(set!=alpha) {
            errln("UnicodeSet([:Alphabetic=%s:]).complement()!=UnicodeSet([:Alphabetic:])\n", falseValues[i]);
        }
    }
    for(i=0; i<LENGTHOF(trueValues); ++i) {
        UnicodeString pattern=UNICODE_STRING_SIMPLE("[:Alphabetic=:]");
        pattern.insert(pattern.length()-2, UnicodeString(trueValues[i], -1, US_INV));
        errorCode=U_ZERO_ERROR;
        UnicodeSet set(pattern, errorCode);
        if(U_FAILURE(errorCode)) {
            errln("UnicodeSet([:Alphabetic=%s:]) failed - %s\n", trueValues[i], u_errorName(errorCode));
            continue;
        }
        if(set!=alpha) {
            errln("UnicodeSet([:Alphabetic=%s:])!=UnicodeSet([:Alphabetic:])\n", trueValues[i]);
        }
    }
}

void UnicodeTest::TestConsistency() {
#if !UCONFIG_NO_NORMALIZATION
    /*
     * Test for an example that getCanonStartSet() delivers
     * all characters that compose from the input one,
     * even in multiple steps.
     * For example, the set for "I" (0049) should contain both
     * I-diaeresis (00CF) and I-diaeresis-acute (1E2E).
     * In general, the set for the middle such character should be a subset
     * of the set for the first.
     */
    IcuTestErrorCode errorCode(*this, "TestConsistency");
    const Normalizer2 *nfd=Normalizer2::getNFDInstance(errorCode);
    const Normalizer2Impl *nfcImpl=Normalizer2Factory::getNFCImpl(errorCode);
    if(!nfcImpl->ensureCanonIterData(errorCode) || errorCode.isFailure()) {
        dataerrln("Normalizer2::getInstance(NFD) or Normalizer2Factory::getNFCImpl() failed - %s\n",
                  errorCode.errorName());
        errorCode.reset();
        return;
    }

    UnicodeSet set1, set2;
    if (nfcImpl->getCanonStartSet(0x49, set1)) {
        /* enumerate all characters that are plausible to be latin letters */
        for(UChar start=0xa0; start<0x2000; ++start) {
            UnicodeString decomp=nfd->normalize(UnicodeString(start), errorCode);
            if(decomp.length()>1 && decomp[0]==0x49) {
                set2.add(start);
            }
        }

        if (set1!=set2) {
            errln("[canon start set of 0049] != [all c with canon decomp with 0049]");
        }
        // This was available in cucdtst.c but the test had to move to intltest
        // because the new internal normalization functions are in C++.
        //compareUSets(set1, set2,
        //             "[canon start set of 0049]", "[all c with canon decomp with 0049]",
        //             TRUE);
    } else {
        errln("NFC.getCanonStartSet() returned FALSE");
    }
#endif
}

/**
 * Test various implementations of Pattern_Syntax & Pattern_White_Space.
 */
void UnicodeTest::TestPatternProperties() {
    IcuTestErrorCode errorCode(*this, "TestPatternProperties()");
    UnicodeSet syn_pp;
    UnicodeSet syn_prop(UNICODE_STRING_SIMPLE("[:Pattern_Syntax:]"), errorCode);
    UnicodeSet syn_list(
        "[!-/\\:-@\\[-\\^`\\{-~"
        "\\u00A1-\\u00A7\\u00A9\\u00AB\\u00AC\\u00AE\\u00B0\\u00B1\\u00B6\\u00BB\\u00BF\\u00D7\\u00F7"
        "\\u2010-\\u2027\\u2030-\\u203E\\u2041-\\u2053\\u2055-\\u205E\\u2190-\\u245F\\u2500-\\u2775"
        "\\u2794-\\u2BFF\\u2E00-\\u2E7F\\u3001-\\u3003\\u3008-\\u3020\\u3030\\uFD3E\\uFD3F\\uFE45\\uFE46]", errorCode);
    UnicodeSet ws_pp;
    UnicodeSet ws_prop(UNICODE_STRING_SIMPLE("[:Pattern_White_Space:]"), errorCode);
    UnicodeSet ws_list(UNICODE_STRING_SIMPLE("[\\u0009-\\u000D\\ \\u0085\\u200E\\u200F\\u2028\\u2029]"), errorCode);
    UnicodeSet syn_ws_pp;
    UnicodeSet syn_ws_prop(syn_prop);
    syn_ws_prop.addAll(ws_prop);
    for(UChar32 c=0; c<=0xffff; ++c) {
        if(PatternProps::isSyntax(c)) {
            syn_pp.add(c);
        }
        if(PatternProps::isWhiteSpace(c)) {
            ws_pp.add(c);
        }
        if(PatternProps::isSyntaxOrWhiteSpace(c)) {
            syn_ws_pp.add(c);
        }
    }
    compareUSets(syn_pp, syn_prop,
                 "PatternProps.isSyntax()", "[:Pattern_Syntax:]", TRUE);
    compareUSets(syn_pp, syn_list,
                 "PatternProps.isSyntax()", "[Pattern_Syntax ranges]", TRUE);
    compareUSets(ws_pp, ws_prop,
                 "PatternProps.isWhiteSpace()", "[:Pattern_White_Space:]", TRUE);
    compareUSets(ws_pp, ws_list,
                 "PatternProps.isWhiteSpace()", "[Pattern_White_Space ranges]", TRUE);
    compareUSets(syn_ws_pp, syn_ws_prop,
                 "PatternProps.isSyntaxOrWhiteSpace()",
                 "[[:Pattern_Syntax:][:Pattern_White_Space:]]", TRUE);
}

// So far only minimal port of Java & cucdtst.c compareUSets().
UBool
UnicodeTest::compareUSets(const UnicodeSet &a, const UnicodeSet &b,
                          const char *a_name, const char *b_name,
                          UBool diffIsError) {
    UBool same= a==b;
    if(!same && diffIsError) {
        errln("Sets are different: %s vs. %s\n", a_name, b_name);
    }
    return same;
}

namespace {

/**
 * Maps a special script code to the most common script of its encoded characters.
 */
UScriptCode getCharScript(UScriptCode script) {
    switch(script) {
    case USCRIPT_SIMPLIFIED_HAN:
    case USCRIPT_TRADITIONAL_HAN:
        return USCRIPT_HAN;
    case USCRIPT_JAPANESE:
        return USCRIPT_HIRAGANA;
    case USCRIPT_KOREAN:
        return USCRIPT_HANGUL;
    default:
        return script;
    }
}

}  // namespace

void UnicodeTest::TestScriptMetadata() {
    IcuTestErrorCode errorCode(*this, "TestScriptMetadata()");
    UnicodeSet rtl("[[:bc=R:][:bc=AL:]-[:Cn:]-[:sc=Common:]]", errorCode);
    // So far, sample characters are uppercase.
    // Georgian is special.
    UnicodeSet cased("[[:Lu:]-[:sc=Common:]-[:sc=Geor:]]", errorCode);
    for(int32_t sci = 0; sci < USCRIPT_CODE_LIMIT; ++sci) {
        UScriptCode sc = (UScriptCode)sci;
        // Run the test with -v to see which script has failures:
        // .../intltest$ make && ./intltest utility/UnicodeTest/TestScriptMetadata -v | grep -C 3 FAIL
        logln(uscript_getShortName(sc));
        UScriptUsage usage = uscript_getUsage(sc);
        UnicodeString sample = uscript_getSampleUnicodeString(sc);
        UnicodeSet scriptSet;
        scriptSet.applyIntPropertyValue(UCHAR_SCRIPT, sc, errorCode);
        if(usage == USCRIPT_USAGE_NOT_ENCODED) {
            assertTrue("not encoded, no sample", sample.isEmpty());
            assertFalse("not encoded, not RTL", uscript_isRightToLeft(sc));
            assertFalse("not encoded, not LB letters", uscript_breaksBetweenLetters(sc));
            assertFalse("not encoded, not cased", uscript_isCased(sc));
            assertTrue("not encoded, no characters", scriptSet.isEmpty());
        } else {
            assertFalse("encoded, has a sample character", sample.isEmpty());
            UChar32 firstChar = sample.char32At(0);
            UScriptCode charScript = getCharScript(sc);
            assertEquals("script(sample(script))",
                         (int32_t)charScript, (int32_t)uscript_getScript(firstChar, errorCode));
            assertEquals("RTL vs. set", (UBool)rtl.contains(firstChar), (UBool)uscript_isRightToLeft(sc));
            assertEquals("cased vs. set", (UBool)cased.contains(firstChar), (UBool)uscript_isCased(sc));
            assertEquals("encoded, has characters", (UBool)(sc == charScript), (UBool)(!scriptSet.isEmpty()));
            if(uscript_isRightToLeft(sc)) {
                rtl.removeAll(scriptSet);
            }
            if(uscript_isCased(sc)) {
                cased.removeAll(scriptSet);
            }
        }
    }
    UnicodeString pattern;
    assertEquals("no remaining RTL characters",
                 UnicodeString("[]"), rtl.toPattern(pattern));
    assertEquals("no remaining cased characters",
                 UnicodeString("[]"), cased.toPattern(pattern));

    assertTrue("Hani breaks between letters", uscript_breaksBetweenLetters(USCRIPT_HAN));
    assertTrue("Thai breaks between letters", uscript_breaksBetweenLetters(USCRIPT_THAI));
    assertFalse("Latn does not break between letters", uscript_breaksBetweenLetters(USCRIPT_LATIN));
}

void UnicodeTest::TestBidiPairedBracketType() {
    // BidiBrackets-6.3.0.txt says:
    //
    // The set of code points listed in this file was originally derived
    // using the character properties General_Category (gc), Bidi_Class (bc),
    // Bidi_Mirrored (Bidi_M), and Bidi_Mirroring_Glyph (bmg), as follows:
    // two characters, A and B, form a pair if A has gc=Ps and B has gc=Pe,
    // both have bc=ON and Bidi_M=Y, and bmg of A is B. Bidi_Paired_Bracket
    // maps A to B and vice versa, and their Bidi_Paired_Bracket_Type
    // property values are Open and Close, respectively.
    IcuTestErrorCode errorCode(*this, "TestBidiPairedBracketType()");
    UnicodeSet bpt("[:^bpt=n:]", errorCode);
    assertTrue("bpt!=None is not empty", !bpt.isEmpty());
    // The following should always be true.
    UnicodeSet mirrored("[:Bidi_M:]", errorCode);
    UnicodeSet other_neutral("[:bc=ON:]", errorCode);
    assertTrue("bpt!=None is a subset of Bidi_M", mirrored.containsAll(bpt));
    assertTrue("bpt!=None is a subset of bc=ON", other_neutral.containsAll(bpt));
    // The following are true at least initially in Unicode 6.3.
    UnicodeSet bpt_open("[:bpt=o:]", errorCode);
    UnicodeSet bpt_close("[:bpt=c:]", errorCode);
    UnicodeSet ps("[:Ps:]", errorCode);
    UnicodeSet pe("[:Pe:]", errorCode);
    assertTrue("bpt=Open is a subset of Ps", ps.containsAll(bpt_open));
    assertTrue("bpt=Close is a subset of Pe", pe.containsAll(bpt_close));
}
