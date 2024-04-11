// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/ustring.h"
#include "unicode/uchar.h"
#include "unicode/ucpmap.h"
#include "unicode/uniset.h"
#include "unicode/putil.h"
#include "unicode/uscript.h"
#include "unicode/uset.h"
#include "charstr.h"
#include "cstring.h"
#include "hash.h"
#include "patternprops.h"
#include "ppucd.h"
#include "normalizer2impl.h"
#include "testutil.h"
#include "uparse.h"
#include "ucdtest.h"

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
    "InCB",
    "NFKC_CF",
    "NFKC_SCF"
};

UnicodeTest::UnicodeTest()
{
    UErrorCode errorCode=U_ZERO_ERROR;
    unknownPropertyNames=new U_NAMESPACE_QUALIFIER Hashtable(errorCode);
    if(U_FAILURE(errorCode)) {
        delete unknownPropertyNames;
        unknownPropertyNames=nullptr;
    }
    // Ignore some property names altogether.
    for(int32_t i=0; i<UPRV_LENGTHOF(ignorePropNames); ++i) {
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
    TESTCASE_AUTO(TestEmojiProperties);
    TESTCASE_AUTO(TestEmojiPropertiesOfStrings);
    TESTCASE_AUTO(TestIndicPositionalCategory);
    TESTCASE_AUTO(TestIndicSyllabicCategory);
    TESTCASE_AUTO(TestVerticalOrientation);
    TESTCASE_AUTO(TestDefaultScriptExtensions);
    TESTCASE_AUTO(TestInvalidCodePointFolding);
#if !UCONFIG_NO_NORMALIZATION
    TESTCASE_AUTO(TestBinaryCharacterProperties);
    TESTCASE_AUTO(TestIntCharacterProperties);
#endif
    TESTCASE_AUTO(TestPropertyNames);
    TESTCASE_AUTO(TestIDSUnaryOperator);
    TESTCASE_AUTO(TestIDCompatMath);
    TESTCASE_AUTO(TestBinaryPropertyUsingPpucd);
    TESTCASE_AUTO(TestIDStatus);
    TESTCASE_AUTO(TestIDType);
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
        if(t!=nullptr) {
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

static int32_t numErrors[UPRV_LENGTHOF(derivedPropsIndex)]={ 0 };

enum { MAX_ERRORS=50 };

U_CFUNC void U_CALLCONV
derivedPropsLineFn(void *context,
                   char *fields[][2], int32_t /* fieldCount */,
                   UErrorCode *pErrorCode)
{
    UnicodeTest *me=static_cast<UnicodeTest*>(context);
    uint32_t start, end;
    int32_t i;

    u_parseCodePointRange(fields[0][0], &start, &end, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        me->errln("UnicodeTest: syntax error in DerivedCoreProperties.txt or DerivedNormalizationProps.txt field 0 at %s\n", fields[0][0]);
        return;
    }

    /* parse derived binary property name, ignore unknown names */
    i=getTokenIndex(derivedPropsNames, UPRV_LENGTHOF(derivedPropsNames), fields[1][0]);
    if(i<0) {
        UnicodeString propName(fields[1][0], (int32_t)(fields[1][1]-fields[1][0]));
        propName.trim();
        if(me->unknownPropertyNames->find(propName)==nullptr) {
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
    if(UPRV_LENGTHOF(derivedProps)<UPRV_LENGTHOF(derivedPropsNames)) {
        errln("error: UnicodeTest::derivedProps[] too short, need at least %d UnicodeSets\n",
              UPRV_LENGTHOF(derivedPropsNames));
        return;
    }
    if(UPRV_LENGTHOF(derivedPropsIndex)!=UPRV_LENGTHOF(derivedPropsNames)) {
        errln("error in ucdtest.cpp: UPRV_LENGTHOF(derivedPropsIndex)!=UPRV_LENGTHOF(derivedPropsNames)\n");
        return;
    }

    char path[500];
    if(getUnidataPath(path) == nullptr) {
        errln("unable to find path to source/data/unidata/");
        return;
    }
    char *basename=strchr(path, 0);
    strcpy(basename, "DerivedCoreProperties.txt");

    char *fields[2][2];
    UErrorCode errorCode=U_ZERO_ERROR;
    u_parseDelimitedFile(path, ';', fields, 2, derivedPropsLineFn, this, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("error parsing DerivedCoreProperties.txt: %s\n", u_errorName(errorCode));
        return;
    }

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

    // test all true properties
    for(i=0; i<UPRV_LENGTHOF(derivedPropsNames); ++i) {
        rangeCount=derivedProps[i].getRangeCount();
        for(range=0; range<rangeCount && numErrors[i]<MAX_ERRORS; ++range) {
            start=derivedProps[i].getRangeStart(range);
            end=derivedProps[i].getRangeEnd(range);
            for(; start<=end; ++start) {
                if(!u_hasBinaryProperty(start, derivedPropsIndex[i])) {
                    dataerrln("UnicodeTest error: u_hasBinaryProperty(U+%04lx, %s)==false is wrong", start, derivedPropsNames[i]);
                    if(++numErrors[i]>=MAX_ERRORS) {
                      dataerrln("Too many errors, moving to the next test");
                      break;
                    }
                }
            }
        }
    }

    // invert all properties
    for(i=0; i<UPRV_LENGTHOF(derivedPropsNames); ++i) {
        derivedProps[i].complement();
    }

    // test all false properties
    for(i=0; i<UPRV_LENGTHOF(derivedPropsNames); ++i) {
        rangeCount=derivedProps[i].getRangeCount();
        for(range=0; range<rangeCount && numErrors[i]<MAX_ERRORS; ++range) {
            start=derivedProps[i].getRangeStart(range);
            end=derivedProps[i].getRangeEnd(range);
            for(; start<=end; ++start) {
                if(u_hasBinaryProperty(start, derivedPropsIndex[i])) {
                    errln("UnicodeTest error: u_hasBinaryProperty(U+%04lx, %s)==true is wrong\n", start, derivedPropsNames[i]);
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
    for(i=0; i<UPRV_LENGTHOF(falseValues); ++i) {
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
    for(i=0; i<UPRV_LENGTHOF(trueValues); ++i) {
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
        for(char16_t start=0xa0; start<0x2000; ++start) {
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
        //             true);
    } else {
        errln("NFC.getCanonStartSet() returned false");
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
                 "PatternProps.isSyntax()", "[:Pattern_Syntax:]", true);
    compareUSets(syn_pp, syn_list,
                 "PatternProps.isSyntax()", "[Pattern_Syntax ranges]", true);
    compareUSets(ws_pp, ws_prop,
                 "PatternProps.isWhiteSpace()", "[:Pattern_White_Space:]", true);
    compareUSets(ws_pp, ws_list,
                 "PatternProps.isWhiteSpace()", "[Pattern_White_Space ranges]", true);
    compareUSets(syn_ws_pp, syn_ws_prop,
                 "PatternProps.isSyntaxOrWhiteSpace()",
                 "[[:Pattern_Syntax:][:Pattern_White_Space:]]", true);
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
    case USCRIPT_HAN_WITH_BOPOMOFO:
    case USCRIPT_SIMPLIFIED_HAN:
    case USCRIPT_TRADITIONAL_HAN:
        return USCRIPT_HAN;
    case USCRIPT_JAPANESE:
        return USCRIPT_HIRAGANA;
    case USCRIPT_JAMO:
    case USCRIPT_KOREAN:
        return USCRIPT_HANGUL;
    case USCRIPT_SYMBOLS_EMOJI:
        return USCRIPT_SYMBOLS;
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
        // .../intltest$ make && ./intltest utility/UnicodeTest/TestScriptMetadata -v | grep -C 6 FAIL
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

void UnicodeTest::TestEmojiProperties() {
    assertFalse("space is not Emoji", u_hasBinaryProperty(0x20, UCHAR_EMOJI));
    assertTrue("shooting star is Emoji", u_hasBinaryProperty(0x1F320, UCHAR_EMOJI));
    IcuTestErrorCode errorCode(*this, "TestEmojiProperties()");
    UnicodeSet emoji("[:Emoji:]", errorCode);
    assertTrue("lots of Emoji", emoji.size() > 700);

    assertTrue("shooting star is Emoji_Presentation",
               u_hasBinaryProperty(0x1F320, UCHAR_EMOJI_PRESENTATION));
    assertTrue("Fitzpatrick 6 is Emoji_Modifier",
               u_hasBinaryProperty(0x1F3FF, UCHAR_EMOJI_MODIFIER));
    assertTrue("happy person is Emoji_Modifier_Base",
               u_hasBinaryProperty(0x1F64B, UCHAR_EMOJI_MODIFIER_BASE));
    assertTrue("asterisk is Emoji_Component",
               u_hasBinaryProperty(0x2A, UCHAR_EMOJI_COMPONENT));
    assertTrue("copyright is Extended_Pictographic",
               u_hasBinaryProperty(0xA9, UCHAR_EXTENDED_PICTOGRAPHIC));
}

namespace {

UBool hbp(const char16_t *s, int32_t length, UProperty which) {
    return u_stringHasBinaryProperty(s, length, which);
}

UBool hbp(const char16_t *s, UProperty which) {
    return u_stringHasBinaryProperty(s, -1, which);
}

}  // namespace

void UnicodeTest::TestEmojiPropertiesOfStrings() {
    // Property of code points, for coverage
    assertFalse("null is not Ideographic", hbp(nullptr, 1, UCHAR_IDEOGRAPHIC));
    assertFalse("null/0 is not Ideographic", hbp(nullptr, -1, UCHAR_IDEOGRAPHIC));
    assertFalse("empty string is not Ideographic", hbp(u"", 0, UCHAR_IDEOGRAPHIC));
    assertFalse("empty string/0 is not Ideographic", hbp(u"", -1, UCHAR_IDEOGRAPHIC));
    assertFalse("L is not Ideographic", hbp(u"L", 1, UCHAR_IDEOGRAPHIC));
    assertFalse("L/0 is not Ideographic", hbp(u"L", -1, UCHAR_IDEOGRAPHIC));
    assertTrue("U+4E02 is Ideographic", hbp(u"丂", 1, UCHAR_IDEOGRAPHIC));
    assertTrue("U+4E02/0 is Ideographic", hbp(u"丂", -1, UCHAR_IDEOGRAPHIC));
    assertFalse("2*U+4E02 is not Ideographic", hbp(u"丂丂", 2, UCHAR_IDEOGRAPHIC));
    assertFalse("2*U+4E02/0 is not Ideographic", hbp(u"丂丂", -1, UCHAR_IDEOGRAPHIC));
    assertFalse("bicycle is not Ideographic", hbp(u"🚲", 2, UCHAR_IDEOGRAPHIC));
    assertFalse("bicycle/0 is not Ideographic", hbp(u"🚲", -1, UCHAR_IDEOGRAPHIC));
    assertTrue("U+23456 is Ideographic", hbp(u"\U00023456", 2, UCHAR_IDEOGRAPHIC));
    assertTrue("U+23456/0 is Ideographic", hbp(u"\U00023456", -1, UCHAR_IDEOGRAPHIC));

    // Property of (code points and) strings
    assertFalse("null is not Basic_Emoji", hbp(nullptr, 1, UCHAR_BASIC_EMOJI));
    assertFalse("null/0 is not Basic_Emoji", hbp(nullptr, -1, UCHAR_BASIC_EMOJI));
    assertFalse("empty string is not Basic_Emoji", hbp(u"", 0, UCHAR_BASIC_EMOJI));
    assertFalse("empty string/0 is not Basic_Emoji", hbp(u"", -1, UCHAR_BASIC_EMOJI));
    assertFalse("L is not Basic_Emoji", hbp(u"L", 1, UCHAR_BASIC_EMOJI));
    assertFalse("L/0 is not Basic_Emoji", hbp(u"L", -1, UCHAR_BASIC_EMOJI));
    assertFalse("U+4E02 is not Basic_Emoji", hbp(u"丂", 1, UCHAR_BASIC_EMOJI));
    assertFalse("U+4E02/0 is not Basic_Emoji", hbp(u"丂", -1, UCHAR_BASIC_EMOJI));
    assertTrue("bicycle is Basic_Emoji", hbp(u"🚲", 2, UCHAR_BASIC_EMOJI));
    assertTrue("bicycle/0 is Basic_Emoji", hbp(u"🚲", -1, UCHAR_BASIC_EMOJI));
    assertFalse("2*bicycle is Basic_Emoji", hbp(u"🚲🚲", 4, UCHAR_BASIC_EMOJI));
    assertFalse("2*bicycle/0 is Basic_Emoji", hbp(u"🚲🚲", -1, UCHAR_BASIC_EMOJI));
    assertFalse("U+23456 is not Basic_Emoji", hbp(u"\U00023456", 2, UCHAR_BASIC_EMOJI));
    assertFalse("U+23456/0 is not Basic_Emoji", hbp(u"\U00023456", -1, UCHAR_BASIC_EMOJI));

    assertFalse("stopwatch is not Basic_Emoji", hbp(u"⏱", 1, UCHAR_BASIC_EMOJI));
    assertFalse("stopwatch/0 is not Basic_Emoji", hbp(u"⏱", -1, UCHAR_BASIC_EMOJI));
    assertTrue("stopwatch+emoji is Basic_Emoji", hbp(u"⏱\uFE0F", 2, UCHAR_BASIC_EMOJI));
    assertTrue("stopwatch+emoji/0 is Basic_Emoji", hbp(u"⏱\uFE0F", -1, UCHAR_BASIC_EMOJI));

    assertFalse("chipmunk is not Basic_Emoji", hbp(u"🐿", UCHAR_BASIC_EMOJI));
    assertTrue("chipmunk+emoji is Basic_Emoji", hbp(u"🐿\uFE0F", UCHAR_BASIC_EMOJI));
    assertFalse("chipmunk+2*emoji is not Basic_Emoji", hbp(u"🐿\uFE0F\uFE0F", UCHAR_BASIC_EMOJI));

    // Properties of strings (only)
    assertFalse("4+emoji is not Emoji_Keycap_Sequence",
                hbp(u"4\uFE0F", UCHAR_EMOJI_KEYCAP_SEQUENCE));
    assertTrue("4+emoji+keycap is Emoji_Keycap_Sequence",
               hbp(u"4\uFE0F\u20E3", UCHAR_EMOJI_KEYCAP_SEQUENCE));

    assertFalse("[B] is not RGI_Emoji_Flag_Sequence",
                hbp(u"\U0001F1E7", UCHAR_RGI_EMOJI_FLAG_SEQUENCE));
    assertTrue("[BE] is RGI_Emoji_Flag_Sequence",
               hbp(u"🇧🇪", UCHAR_RGI_EMOJI_FLAG_SEQUENCE));

    assertFalse("[flag] is not RGI_Emoji_Tag_Sequence",
                hbp(u"\U0001F3F4", UCHAR_RGI_EMOJI_TAG_SEQUENCE));
    assertTrue("[Scotland] is RGI_Emoji_Tag_Sequence",
               hbp(u"🏴󠁧󠁢󠁳󠁣󠁴󠁿", UCHAR_RGI_EMOJI_TAG_SEQUENCE));

    assertFalse("bicyclist is not RGI_Emoji_Modifier_Sequence",
                hbp(u"🚴", UCHAR_RGI_EMOJI_MODIFIER_SEQUENCE));
    assertTrue("bicyclist+medium is RGI_Emoji_Modifier_Sequence",
               hbp(u"🚴\U0001F3FD", UCHAR_RGI_EMOJI_MODIFIER_SEQUENCE));

    assertFalse("woman+dark+ZWJ is not RGI_Emoji_ZWJ_Sequence",
                hbp(u"👩\U0001F3FF\u200D", UCHAR_RGI_EMOJI_ZWJ_SEQUENCE));
    assertTrue("woman pilot: dark skin tone is RGI_Emoji_ZWJ_Sequence",
               hbp(u"👩\U0001F3FF\u200D✈\uFE0F", UCHAR_RGI_EMOJI_ZWJ_SEQUENCE));

    // RGI_Emoji = all of the above
    assertFalse("stopwatch is not RGI_Emoji", hbp(u"⏱", UCHAR_RGI_EMOJI));
    assertTrue("stopwatch+emoji is RGI_Emoji", hbp(u"⏱\uFE0F", UCHAR_RGI_EMOJI));

    assertFalse("chipmunk is not RGI_Emoji", hbp(u"🐿", UCHAR_RGI_EMOJI));
    assertTrue("chipmunk+emoji is RGI_Emoji", hbp(u"🐿\uFE0F", UCHAR_RGI_EMOJI));

    assertFalse("4+emoji is not RGI_Emoji", hbp(u"4\uFE0F", UCHAR_RGI_EMOJI));
    assertTrue("4+emoji+keycap is RGI_Emoji", hbp(u"4\uFE0F\u20E3", UCHAR_RGI_EMOJI));

    assertFalse("[B] is not RGI_Emoji", hbp(u"\U0001F1E7", UCHAR_RGI_EMOJI));
    assertTrue("[BE] is RGI_Emoji", hbp(u"🇧🇪", UCHAR_RGI_EMOJI));

    assertTrue("[flag] is RGI_Emoji", hbp(u"\U0001F3F4", UCHAR_RGI_EMOJI));
    assertTrue("[Scotland] is RGI_Emoji", hbp(u"🏴󠁧󠁢󠁳󠁣󠁴󠁿", UCHAR_RGI_EMOJI));

    assertTrue("bicyclist is RGI_Emoji", hbp(u"🚴", UCHAR_RGI_EMOJI));
    assertTrue("bicyclist+medium is RGI_Emoji", hbp(u"🚴\U0001F3FD", UCHAR_RGI_EMOJI));

    assertFalse("woman+dark+ZWJ is not RGI_Emoji", hbp(u"👩\U0001F3FF\u200D", UCHAR_RGI_EMOJI));
    assertTrue("woman pilot: dark skin tone is RGI_Emoji",
               hbp(u"👩\U0001F3FF\u200D✈\uFE0F", UCHAR_RGI_EMOJI));

    // UnicodeSet with properties of strings
    IcuTestErrorCode errorCode(*this, "TestEmojiPropertiesOfStrings()");
    UnicodeSet basic("[:Basic_Emoji:]", errorCode);
    UnicodeSet keycaps("[:Emoji_Keycap_Sequence:]", errorCode);
    UnicodeSet modified("[:RGI_Emoji_Modifier_Sequence:]", errorCode);
    UnicodeSet flags("[:RGI_Emoji_Flag_Sequence:]", errorCode);
    UnicodeSet tags("[:RGI_Emoji_Tag_Sequence:]", errorCode);
    UnicodeSet combos("[:RGI_Emoji_ZWJ_Sequence:]", errorCode);
    UnicodeSet rgi("[:RGI_Emoji:]", errorCode);
    if (errorCode.errDataIfFailureAndReset("UnicodeSets")) {
        return;
    }

    // union of all sets except for "rgi" -- should be the same as "rgi"
    UnicodeSet all(basic);
    all.addAll(keycaps).addAll(modified).addAll(flags).addAll(tags).addAll(combos);

    UnicodeSet basicOnlyCp(basic);
    basicOnlyCp.removeAllStrings();

    UnicodeSet rgiOnlyCp(rgi);
    rgiOnlyCp.removeAllStrings();

    assertTrue("lots of Basic_Emoji", basic.size() > 1000);
    assertEquals("12 Emoji_Keycap_Sequence", 12, keycaps.size());
    assertTrue("lots of RGI_Emoji_Modifier_Sequence", modified.size() > 600);
    assertTrue("lots of RGI_Emoji_Flag_Sequence", flags.size() > 250);
    assertTrue("some RGI_Emoji_Tag_Sequence", tags.size() >= 3);
    assertTrue("lots of RGI_Emoji_ZWJ_Sequence", combos.size() > 1300);
    assertTrue("lots of RGI_Emoji", rgi.size() > 3000);

    assertTrue("lots of Basic_Emoji code points", basicOnlyCp.size() > 1000);
    assertTrue("Basic_Emoji.hasStrings()", basic.hasStrings());
    assertEquals("no Emoji_Keycap_Sequence code points", 0, keycaps.getRangeCount());
    assertEquals("lots of RGI_Emoji_Modifier_Sequence", 0, modified.getRangeCount());
    assertEquals("lots of RGI_Emoji_Flag_Sequence", 0, flags.getRangeCount());
    assertEquals("some RGI_Emoji_Tag_Sequence", 0, tags.getRangeCount());
    assertEquals("lots of RGI_Emoji_ZWJ_Sequence", 0, combos.getRangeCount());

    assertTrue("lots of RGI_Emoji code points", rgiOnlyCp.size() > 1000);
    assertTrue("RGI_Emoji.hasStrings()", rgi.hasStrings());
    assertEquals("RGI_Emoji/only-cp.size() == Basic_Emoji/only-cp.size()",
                 rgiOnlyCp.size(), basicOnlyCp.size());
    assertTrue("RGI_Emoji/only-cp == Basic_Emoji/only-cp", rgiOnlyCp == basicOnlyCp);
    assertEquals("RGI_Emoji.size() == union.size()", rgi.size(), all.size());
    assertTrue("RGI_Emoji == union", rgi == all);

    assertTrue("Basic_Emoji.contains(stopwatch+emoji)", basic.contains(u"⏱\uFE0F"));
    assertTrue("Basic_Emoji.contains(chipmunk+emoji)", basic.contains(u"🐿\uFE0F"));
    assertTrue("Emoji_Keycap_Sequence.contains(4+emoji+keycap)",
               keycaps.contains(u"4\uFE0F\u20E3"));
    assertTrue("RGI_Emoji_Flag_Sequence.contains([BE])", flags.contains(u"🇧🇪"));
    assertTrue("RGI_Emoji_Tag_Sequence.contains([Scotland])", tags.contains(u"🏴󠁧󠁢󠁳󠁣󠁴󠁿"));
    assertTrue("RGI_Emoji_Modifier_Sequence.contains(bicyclist+medium)",
               modified.contains(u"🚴\U0001F3FD"));
    assertTrue("RGI_Emoji_ZWJ_Sequence.contains(woman pilot: dark skin tone)",
               combos.contains(u"👩\U0001F3FF\u200D✈\uFE0F"));
    assertTrue("RGI_Emoji.contains(stopwatch+emoji)", rgi.contains(u"⏱\uFE0F"));
    assertTrue("RGI_Emoji.contains(chipmunk+emoji)", rgi.contains(u"🐿\uFE0F"));
    assertTrue("RGI_Emoji.contains(4+emoji+keycap)", rgi.contains(u"4\uFE0F\u20E3"));
    assertTrue("RGI_Emoji.contains([BE] is RGI_Emoji)", rgi.contains(u"🇧🇪"));
    assertTrue("RGI_Emoji.contains([flag])", rgi.contains(u"\U0001F3F4"));
    assertTrue("RGI_Emoji.contains([Scotland])", rgi.contains(u"🏴󠁧󠁢󠁳󠁣󠁴󠁿"));
    assertTrue("RGI_Emoji.contains(bicyclist)", rgi.contains(u"🚴"));
    assertTrue("RGI_Emoji.contains(bicyclist+medium)", rgi.contains(u"🚴\U0001F3FD"));
    assertTrue("RGI_Emoji.contains(woman pilot: dark skin tone)", rgi.contains(u"👩\U0001F3FF\u200D✈\uFE0F"));
}

void UnicodeTest::TestIndicPositionalCategory() {
    IcuTestErrorCode errorCode(*this, "TestIndicPositionalCategory()");
    UnicodeSet na(u"[:InPC=NA:]", errorCode);
    assertTrue("mostly NA", 1000000 <= na.size() && na.size() <= UCHAR_MAX_VALUE - 500);
    UnicodeSet vol(u"[:InPC=Visual_Order_Left:]", errorCode);
    assertTrue("some Visual_Order_Left", 19 <= vol.size() && vol.size() <= 100);
    assertEquals("U+08FF: NA", U_INPC_NA,
                 u_getIntPropertyValue(0x08FF, UCHAR_INDIC_POSITIONAL_CATEGORY));
    assertEquals("U+0900: Top", U_INPC_TOP,
                 u_getIntPropertyValue(0x0900, UCHAR_INDIC_POSITIONAL_CATEGORY));
    assertEquals("U+10A06: Overstruck", U_INPC_OVERSTRUCK,
                 u_getIntPropertyValue(0x10A06, UCHAR_INDIC_POSITIONAL_CATEGORY));
}

void UnicodeTest::TestIndicSyllabicCategory() {
    IcuTestErrorCode errorCode(*this, "TestIndicSyllabicCategory()");
    UnicodeSet other(u"[:InSC=Other:]", errorCode);
    assertTrue("mostly Other", 1000000 <= other.size() && other.size() <= UCHAR_MAX_VALUE - 500);
    UnicodeSet ava(u"[:InSC=Avagraha:]", errorCode);
    assertTrue("some Avagraha", 16 <= ava.size() && ava.size() <= 100);
    assertEquals("U+08FF: Other", U_INSC_OTHER,
                 u_getIntPropertyValue(0x08FF, UCHAR_INDIC_SYLLABIC_CATEGORY));
    assertEquals("U+0900: Bindu", U_INSC_BINDU,
                 u_getIntPropertyValue(0x0900, UCHAR_INDIC_SYLLABIC_CATEGORY));
    assertEquals("U+11065: Brahmi_Joining_Number", U_INSC_BRAHMI_JOINING_NUMBER,
                 u_getIntPropertyValue(0x11065, UCHAR_INDIC_SYLLABIC_CATEGORY));
}

void UnicodeTest::TestVerticalOrientation() {
    IcuTestErrorCode errorCode(*this, "TestVerticalOrientation()");
    UnicodeSet r(u"[:vo=R:]", errorCode);
    assertTrue("mostly R", 0xc0000 <= r.size() && r.size() <= 0xd0000);
    UnicodeSet u(u"[:vo=U:]", errorCode);
    assertTrue("much U", 0x40000 <= u.size() && u.size() <= 0x50000);
    UnicodeSet tu(u"[:vo=Tu:]", errorCode);
    assertTrue("some Tu", 147 <= tu.size() && tu.size() <= 300);
    assertEquals("U+0E01: Rotated", U_VO_ROTATED,
                 u_getIntPropertyValue(0x0E01, UCHAR_VERTICAL_ORIENTATION));
    assertEquals("U+3008: Transformed_Rotated", U_VO_TRANSFORMED_ROTATED,
                 u_getIntPropertyValue(0x3008, UCHAR_VERTICAL_ORIENTATION));
    assertEquals("U+33333: Upright", U_VO_UPRIGHT,
                 u_getIntPropertyValue(0x33333, UCHAR_VERTICAL_ORIENTATION));
}

void UnicodeTest::TestDefaultScriptExtensions() {
    // Block 3000..303F CJK Symbols and Punctuation defaults to scx=Bopo Hang Hani Hira Kana Yiii
    // but some of its characters revert to scx=<script> which is usually Common.
    IcuTestErrorCode errorCode(*this, "TestDefaultScriptExtensions()");
    UScriptCode scx[20];
    scx[0] = USCRIPT_INVALID_CODE;
    assertEquals("U+3000 num scx", 1,  // IDEOGRAPHIC SPACE
                 uscript_getScriptExtensions(0x3000, scx, UPRV_LENGTHOF(scx), errorCode));
    assertEquals("U+3000 num scx[0]", USCRIPT_COMMON, scx[0]);
    scx[0] = USCRIPT_INVALID_CODE;
    assertEquals("U+3012 num scx", 1,  // POSTAL MARK
                 uscript_getScriptExtensions(0x3012, scx, UPRV_LENGTHOF(scx), errorCode));
    assertEquals("U+3012 num scx[0]", USCRIPT_COMMON, scx[0]);
}

void UnicodeTest::TestInvalidCodePointFolding() {
    // Test behavior when an invalid code point is passed to u_foldCase
    static const UChar32 invalidCodePoints[] = {
            0xD800, // lead surrogate
            0xDFFF, // trail surrogate
            0xFDD0, // noncharacter
            0xFFFF, // noncharacter
            0x110000, // out of range
            -1 // negative
    };
    for (int32_t i=0; i<UPRV_LENGTHOF(invalidCodePoints); ++i) {
        UChar32 cp = invalidCodePoints[i];
        assertEquals("Invalid code points should be echoed back",
                cp, u_foldCase(cp, U_FOLD_CASE_DEFAULT));
        assertEquals("Invalid code points should be echoed back",
                cp, u_foldCase(cp, U_FOLD_CASE_EXCLUDE_SPECIAL_I));
    }
}

void UnicodeTest::TestBinaryCharacterProperties() {
#if !UCONFIG_NO_NORMALIZATION
    IcuTestErrorCode errorCode(*this, "TestBinaryCharacterProperties()");
    // Spot-check getBinaryPropertySet() vs. hasBinaryProperty().
    for (int32_t prop = 0; prop < UCHAR_BINARY_LIMIT; ++prop) {
        const USet *uset = u_getBinaryPropertySet((UProperty)prop, errorCode);
        if (errorCode.errIfFailureAndReset("u_getBinaryPropertySet(%d)", (int)prop)) {
            continue;
        }
        const UnicodeSet &set = *UnicodeSet::fromUSet(uset);
        int32_t count = set.getRangeCount();
        if (count == 0) {
            assertFalse(UnicodeString("!hasBinaryProperty(U+0020, ") + prop + u")",
                u_hasBinaryProperty(0x20, (UProperty)prop));
            assertFalse(UnicodeString("!hasBinaryProperty(U+0061, ") + prop + u")",
                u_hasBinaryProperty(0x61, (UProperty)prop));
            assertFalse(UnicodeString("!hasBinaryProperty(U+4E00, ") + prop + u")",
                u_hasBinaryProperty(0x4e00, (UProperty)prop));
        } else {
            UChar32 c = set.getRangeStart(0);
            if (c > 0) {
                assertFalse(
                    UnicodeString("!hasBinaryProperty(") + TestUtility::hex(c - 1) +
                        u", " + prop + u")",
                    u_hasBinaryProperty(c - 1, (UProperty)prop));
            }
            assertTrue(
                UnicodeString("hasBinaryProperty(") + TestUtility::hex(c) +
                    u", " + prop + u")",
                u_hasBinaryProperty(c, (UProperty)prop));
            c = set.getRangeEnd(count - 1);
            assertTrue(
                UnicodeString("hasBinaryProperty(") + TestUtility::hex(c) +
                    u", " + prop + u")",
                u_hasBinaryProperty(c, (UProperty)prop));
            if (c < 0x10ffff) {
                assertFalse(
                    UnicodeString("!hasBinaryProperty(") + TestUtility::hex(c + 1) +
                        u", " + prop + u")",
                    u_hasBinaryProperty(c + 1, (UProperty)prop));
            }
        }
    }
#endif
}

void UnicodeTest::TestIntCharacterProperties() {
#if !UCONFIG_NO_NORMALIZATION
    IcuTestErrorCode errorCode(*this, "TestIntCharacterProperties()");
    // Spot-check getIntPropertyMap() vs. getIntPropertyValue().
    for (int32_t prop = UCHAR_INT_START; prop < UCHAR_INT_LIMIT; ++prop) {
        const UCPMap *map = u_getIntPropertyMap((UProperty)prop, errorCode);
        if (errorCode.errIfFailureAndReset("u_getIntPropertyMap(%d)", (int)prop)) {
            continue;
        }
        uint32_t value;
        UChar32 end = ucpmap_getRange(map, 0, UCPMAP_RANGE_NORMAL, 0, nullptr, nullptr, &value);
        assertTrue("int property first range", end >= 0);
        UChar32 c = end / 2;
        assertEquals(UnicodeString("int property first range value at ") + TestUtility::hex(c),
            u_getIntPropertyValue(c, (UProperty)prop), value);
        end = ucpmap_getRange(map, 0x5000, UCPMAP_RANGE_NORMAL, 0, nullptr, nullptr, &value);
        assertTrue("int property later range", end >= 0);
        assertEquals(UnicodeString("int property later range value at ") + TestUtility::hex(end),
            u_getIntPropertyValue(end, (UProperty)prop), value);
        // ucpmap_get() API coverage
        // TODO: move to cucdtst.c
        assertEquals(
            "int property upcmap_get(U+0061)",
            u_getIntPropertyValue(0x61, (UProperty)prop), ucpmap_get(map, 0x61));
    }
#endif
}

namespace {

const char *getPropName(UProperty property, int32_t nameChoice) UPRV_NO_SANITIZE_UNDEFINED {
    const char *name = u_getPropertyName(property, (UPropertyNameChoice)nameChoice);
    return name != nullptr ? name : "null";
}

const char *getValueName(UProperty property, int32_t value, int32_t nameChoice)
        UPRV_NO_SANITIZE_UNDEFINED {
    const char *name = u_getPropertyValueName(property, value, (UPropertyNameChoice)nameChoice);
    return name != nullptr ? name : "null";
}

}  // namespace

void UnicodeTest::TestPropertyNames() {
    IcuTestErrorCode errorCode(*this, "TestPropertyNames()");
    // Test names of certain properties & values.
    // The UPropertyNameChoice is really an integer with only a couple of named constants.
    UProperty prop = UCHAR_WHITE_SPACE;
    constexpr int32_t SHORT = U_SHORT_PROPERTY_NAME;
    constexpr int32_t LONG = U_LONG_PROPERTY_NAME;
    assertEquals("White_Space: index -1", "null", getPropName(prop, -1));
    assertEquals("White_Space: short", "WSpace", getPropName(prop, SHORT));
    assertEquals("White_Space: long", "White_Space", getPropName(prop, LONG));
    assertEquals("White_Space: index 2", "space", getPropName(prop, 2));
    assertEquals("White_Space: index 3", "null", getPropName(prop, 3));

    prop = UCHAR_SIMPLE_CASE_FOLDING;
    assertEquals("Simple_Case_Folding: index -1", "null", getPropName(prop, -1));
    assertEquals("Simple_Case_Folding: short", "scf", getPropName(prop, SHORT));
    assertEquals("Simple_Case_Folding: long", "Simple_Case_Folding", getPropName(prop, LONG));
    assertEquals("Simple_Case_Folding: index 2", "sfc", getPropName(prop, 2));
    assertEquals("Simple_Case_Folding: index 3", "null", getPropName(prop, 3));

    prop = UCHAR_CASED;
    assertEquals("Cased=Y: index -1", "null", getValueName(prop, 1, -1));
    assertEquals("Cased=Y: short", "Y", getValueName(prop, 1, SHORT));
    assertEquals("Cased=Y: long", "Yes", getValueName(prop, 1, LONG));
    assertEquals("Cased=Y: index 2", "T", getValueName(prop, 1, 2));
    assertEquals("Cased=Y: index 3", "True", getValueName(prop, 1, 3));
    assertEquals("Cased=Y: index 4", "null", getValueName(prop, 1, 4));

    prop = UCHAR_DECOMPOSITION_TYPE;
    int32_t value = U_DT_NOBREAK;
    assertEquals("dt=Nb: index -1", "null", getValueName(prop, value, -1));
    assertEquals("dt=Nb: short", "Nb", getValueName(prop, value, SHORT));
    assertEquals("dt=Nb: long", "Nobreak", getValueName(prop, value, LONG));
    assertEquals("dt=Nb: index 2", "nb", getValueName(prop, value, 2));
    assertEquals("dt=Nb: index 3", "null", getValueName(prop, value, 3));

    // Canonical_Combining_Class:
    // The UCD inserts the numeric values in the second filed of its PropertyValueAliases.txt lines.
    // In ICU, we don't treat these as names,
    // they are just the numeric values returned by u_getCombiningClass().
    // We return the real short and long names for the usual choice constants.
    prop = UCHAR_CANONICAL_COMBINING_CLASS;
    assertEquals("ccc=230: index -1", "null", getValueName(prop, 230, -1));
    assertEquals("ccc=230: short", "A", getValueName(prop, 230, SHORT));
    assertEquals("ccc=230: long", "Above", getValueName(prop, 230, LONG));
    assertEquals("ccc=230: index 2", "null", getValueName(prop, 230, 2));

    prop = UCHAR_GENERAL_CATEGORY;
    value = U_DECIMAL_DIGIT_NUMBER;
    assertEquals("gc=Nd: index -1", "null", getValueName(prop, value, -1));
    assertEquals("gc=Nd: short", "Nd", getValueName(prop, value, SHORT));
    assertEquals("gc=Nd: long", "Decimal_Number", getValueName(prop, value, LONG));
    assertEquals("gc=Nd: index 2", "digit", getValueName(prop, value, 2));
    assertEquals("gc=Nd: index 3", "null", getValueName(prop, value, 3));

    prop = UCHAR_GENERAL_CATEGORY_MASK;
    value = U_GC_P_MASK;
    assertEquals("gc=P mask: index -1", "null", getValueName(prop, value, -1));
    assertEquals("gc=P mask: short", "P", getValueName(prop, value, SHORT));
    assertEquals("gc=P mask: long", "Punctuation", getValueName(prop, value, LONG));
    assertEquals("gc=P mask: index 2", "punct", getValueName(prop, value, 2));
    assertEquals("gc=P mask: index 3", "null", getValueName(prop, value, 3));
}

void UnicodeTest::TestIDSUnaryOperator() {
    IcuTestErrorCode errorCode(*this, "TestIDSUnaryOperator()");
    // New in Unicode 15.1 for just two characters.
    assertFalse("U+2FFC IDSU", u_hasBinaryProperty(0x2ffc, UCHAR_IDS_UNARY_OPERATOR));
    assertFalse("U+2FFD IDSU", u_hasBinaryProperty(0x2ffd, UCHAR_IDS_UNARY_OPERATOR));
    assertTrue("U+2FFE IDSU", u_hasBinaryProperty(0x2ffe, UCHAR_IDS_UNARY_OPERATOR));
    assertTrue("U+2FFF IDSU", u_hasBinaryProperty(0x2fff, UCHAR_IDS_UNARY_OPERATOR));
    assertFalse("U+3000 IDSU", u_hasBinaryProperty(0x3000, UCHAR_IDS_UNARY_OPERATOR));
    assertFalse("U+3001 IDSU", u_hasBinaryProperty(0x3001, UCHAR_IDS_UNARY_OPERATOR));

    // Property name works and gets the correct set.
    UnicodeSet idsu(u"[:IDS_Unary_Operator:]", errorCode);
    assertEquals("IDSU set number of characters", 2, idsu.size());
    assertFalse("idsu.contains(U+2FFD)", idsu.contains(0x2ffd));
    assertTrue("idsu.contains(U+2FFE)", idsu.contains(0x2ffe));
    assertTrue("idsu.contains(U+2FFF)", idsu.contains(0x2fff));
    assertFalse("idsu.contains(U+3000)", idsu.contains(0x3000));
}

namespace {

bool isMathStart(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_ID_COMPAT_MATH_START);
}

bool isMathContinue(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_ID_COMPAT_MATH_CONTINUE);
}

}  // namespace

void UnicodeTest::TestIDCompatMath() {
    IcuTestErrorCode errorCode(*this, "TestIDCompatMath()");
    assertFalse("U+00B1 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0xb1));
    assertTrue("U+00B2 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0xb2));
    assertTrue("U+00B3 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0xb3));
    assertFalse("U+00B4 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0xb4));
    assertFalse("U+207F UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x207f));
    assertTrue("U+2080 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x2080));
    assertTrue("U+208E UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x208e));
    assertFalse("U+208F UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x208f));
    assertFalse("U+2201 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x2201));
    assertTrue("U+2202 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x2202));
    assertTrue("U+1D6C1 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x1D6C1));
    assertTrue("U+1D7C3 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x1D7C3));
    assertFalse("U+1D7C4 UCHAR_ID_COMPAT_MATH_CONTINUE", isMathContinue(0x1D7C4));

    assertFalse("U+00B2 UCHAR_ID_COMPAT_MATH_START", isMathStart(0xb2));
    assertFalse("U+2080 UCHAR_ID_COMPAT_MATH_START", isMathStart(0x2080));
    assertFalse("U+2201 UCHAR_ID_COMPAT_MATH_START", isMathStart(0x2201));
    assertTrue("U+2202 UCHAR_ID_COMPAT_MATH_START", isMathStart(0x2202));
    assertTrue("U+1D6C1 UCHAR_ID_COMPAT_MATH_START", isMathStart(0x1D6C1));
    assertTrue("U+1D7C3 UCHAR_ID_COMPAT_MATH_START", isMathStart(0x1D7C3));
    assertFalse("U+1D7C4 UCHAR_ID_COMPAT_MATH_START", isMathStart(0x1D7C4));

    // Property names work and get the correct sets.
    UnicodeSet idcmStart(u"[:ID_Compat_Math_Start:]", errorCode);
    UnicodeSet idcmContinue(u"[:ID_Compat_Math_Continue:]", errorCode);
    assertEquals("ID_Compat_Math_Start set number of characters", 13, idcmStart.size());
    assertEquals("ID_Compat_Math_Continue set number of characters", 43, idcmContinue.size());
    assertTrue("ID_Compat_Math_Start is a subset of ID_Compat_Math_Continue",
               idcmContinue.containsAll(idcmStart));
    assertFalse("idcmContinue.contains(U+207F)", idcmContinue.contains(0x207f));
    assertTrue("idcmContinue.contains(U+2080)", idcmContinue.contains(0x2080));
    assertTrue("idcmContinue.contains(U+208E)", idcmContinue.contains(0x208e));
    assertFalse("idcmContinue.contains(U+208F)", idcmContinue.contains(0x208f));
    assertFalse("idcmStart.contains(U+2201)", idcmStart.contains(0x2201));
    assertTrue("idcmStart.contains(U+2202)", idcmStart.contains(0x2202));
    assertTrue("idcmStart.contains(U+1D7C3)", idcmStart.contains(0x1D7C3));
    assertFalse("idcmStart.contains(U+1D7C4)", idcmStart.contains(0x1D7C4));
}

U_NAMESPACE_BEGIN

class BuiltInPropertyNames : public PropertyNames {
public:
    ~BuiltInPropertyNames() override {}

    int32_t getPropertyEnum(const char *name) const override {
        return u_getPropertyEnum(name);
    }

    int32_t getPropertyValueEnum(int32_t property, const char *name) const override {
        return u_getPropertyValueEnum((UProperty) property, name);
    }
};

U_NAMESPACE_END

void UnicodeTest::TestBinaryPropertyUsingPpucd() {
    IcuTestErrorCode errorCode(*this, "TestBinaryPropertyUsingPpucd()");

    // Initialize PPUCD parsing object using file in repo and using
    // property names present in built-in data in ICU
    char buffer[500];
    // get path to `source/data/unidata/` including trailing `/`
    char *unidataPath = getUnidataPath(buffer);
    if(unidataPath == nullptr) {
        errln("exiting early because unable to open ppucd.txt from ICU source tree");
        return;
    }
    CharString ppucdPath(unidataPath, errorCode);
    ppucdPath.appendPathPart("ppucd.txt", errorCode);    
    PreparsedUCD ppucd(ppucdPath.data(), errorCode);
    if(errorCode.isFailure()) {
        errln("unable to open %s - %s\n",
            ppucdPath.data(), errorCode.errorName());
        return;
    }
    BuiltInPropertyNames builtInPropNames;
    ppucd.setPropertyNames(&builtInPropNames);

    // Define which binary properties we want to compare
    constexpr UProperty propsUnderTest[] = {
        UCHAR_IDS_UNARY_OPERATOR,
        UCHAR_ID_COMPAT_MATH_START,
        UCHAR_ID_COMPAT_MATH_CONTINUE,
    };

    // Allocate & initialize UnicodeSets per binary property from PPUCD data
    UnicodeSet ppucdPropSets[std::size(propsUnderTest)];

    // Iterate through PPUCD file, accumulating each line's data into each UnicodeSet per property
    PreparsedUCD::LineType lineType;
    UnicodeSet newValues;
    while((lineType=ppucd.readLine(errorCode))!=PreparsedUCD::NO_LINE && errorCode.isSuccess()) {
        if(ppucd.lineHasPropertyValues()) {
            const UniProps *lineProps=ppucd.getProps(newValues, errorCode);

            for(uint32_t i = 0; i < std::size(propsUnderTest); i++) {
                UProperty prop = propsUnderTest[i];
                if (!newValues.contains(prop)) {
                    continue;
                }
                if (lineProps->binProps[prop]) {
                    ppucdPropSets[i].add(lineProps->start, lineProps->end);
                } else {
                    ppucdPropSets[i].remove(lineProps->start, lineProps->end);
                }
            }
        }
    }

    if(errorCode.isFailure()) {
        errln("exiting early due to parsing error");
        return;
    }

    // Assert that the PPUCD data and the ICU data are equivalent for all properties
    for(uint32_t i = 0; i < std::size(propsUnderTest); i++) {
        UnicodeSet icuPropSet;
        UProperty prop = propsUnderTest[i];
        icuPropSet.applyIntPropertyValue(prop, 1, errorCode);
        std::string msg = 
            std::string()
            + "ICU & PPUCD versions of property "
            + u_getPropertyName(prop, U_LONG_PROPERTY_NAME);
        assertTrue(msg.c_str(), ppucdPropSets[i] == icuPropSet);
    }
}

namespace {

int32_t getIDStatus(UChar32 c) {
    return u_getIntPropertyValue(c, UCHAR_IDENTIFIER_STATUS);
}

}  // namespace

void UnicodeTest::TestIDStatus() {
    IcuTestErrorCode errorCode(*this, "TestIDStatus()");
    assertEquals("ID_Status(slash)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0x2F));
    assertEquals("ID_Status(digit 0)=Allowed", U_ID_STATUS_ALLOWED, getIDStatus(0x30));
    assertEquals("ID_Status(colon)=Allowed", U_ID_STATUS_ALLOWED, getIDStatus(0x3A));
    assertEquals("ID_Status(semicolon)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0x3B));
    assertEquals("ID_Status(Greek small alpha)=Allowed", U_ID_STATUS_ALLOWED, getIDStatus(0x03B1));
    assertEquals("ID_Status(Greek small archaic koppa)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0x03D9));
    assertEquals("ID_Status(Hangul syllable)=Allowed", U_ID_STATUS_ALLOWED, getIDStatus(0xAC00));
    assertEquals("ID_Status(surrogate)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0xD800));
    assertEquals("ID_Status(Arabic tail fragment)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0xFE73));
    assertEquals("ID_Status(Hentaigana ko-3)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0x1B03A));
    assertEquals("ID_Status(Katakana small ko)=Allowed", U_ID_STATUS_ALLOWED, getIDStatus(0x1B155));
    assertEquals("ID_Status(U+2EE5D)=Allowed", U_ID_STATUS_ALLOWED, getIDStatus(0x2EE5D));
    assertEquals("ID_Status(U+10FFFF)=Restricted", U_ID_STATUS_RESTRICTED, getIDStatus(0x10FFFF));

    // Property names work and get the correct sets.
    UnicodeSet idStatus(u"[:Identifier_Status=Allowed:]", errorCode);
    // Unicode 15.1: 112778 Allowed characters; normally grows over time
    assertTrue("Allowed number of characters", idStatus.size() >= 112778);
    assertFalse("Allowed.contains(slash)", idStatus.contains(0x2F));
    assertTrue("Allowed.contains(digit 0)", idStatus.contains(0x30));
    assertTrue("Allowed.contains(colon)", idStatus.contains(0x3A));
    assertFalse("Allowed.contains(semicolon)", idStatus.contains(0x3B));
    assertTrue("Allowed.contains(Greek small alpha)", idStatus.contains(0x03B1));
    assertFalse("Allowed.contains(Greek small archaic koppa)", idStatus.contains(0x03D9));
    assertTrue("Allowed.contains(Hangul syllable)", idStatus.contains(0xAC00));
    assertFalse("Allowed.contains(surrogate)", idStatus.contains(0xD800));
    assertFalse("Allowed.contains(Arabic tail fragment)", idStatus.contains(0xFE73));
    assertFalse("Allowed.contains(Hentaigana ko-3)", idStatus.contains(0x1B03A));
    assertTrue("Allowed.contains(Katakana small ko)", idStatus.contains(0x1B155));
    assertTrue("Allowed.contains(U+2EE5D)", idStatus.contains(0x2EE5D));
    assertFalse("Allowed.contains(U+10FFFF)", idStatus.contains(0x10FFFF));
}

namespace {

UnicodeString getIDTypes(UChar32 c) {
    UErrorCode errorCode = U_ZERO_ERROR;
    UIdentifierType types[10];
    int32_t length = u_getIDTypes(c, types, UPRV_LENGTHOF(types), &errorCode);
    if (U_FAILURE(errorCode)) {
        return UnicodeString(u_errorName(errorCode), -1, US_INV);
    }
    // The order of values is undefined, but for simplicity we assume the order
    // that the current implementation yields. Otherwise we would have to sort the values.
    uint32_t typeBits = 0;
    UnicodeString result;
    for (int32_t i = 0; i < length; ++i) {
        if (i != 0) {
            result.append(u' ');
        }
        auto t = types[i];
        typeBits |= 1UL << t;
        const char *s = u_getPropertyValueName(UCHAR_IDENTIFIER_TYPE, t, U_LONG_PROPERTY_NAME);
        if (s != nullptr) {
            result.append(UnicodeString(s, -1, US_INV));
        } else {
            result.append(u"???");
        }
    }
    // Check that u_hasIDType() agrees.
    // Includes undefined behavior with t > largest enum constant.
    for (int32_t i = 0; i < 16; ++i) {
        UIdentifierType t = (UIdentifierType)i;
        bool expected = (typeBits & (1UL << i)) != 0;
        bool actual = u_hasIDType(c, t);
        if (actual != expected) {
            result.append(u" != u_hasIDType() ");
            result = result + i;
            break;
        }
    }
    return result;
}

}  // namespace

void UnicodeTest::TestIDType() {
    IcuTestErrorCode errorCode(*this, "TestIDType()");
    // Note: Types other than Recommended and Inclusion may well change over time.
    assertEquals("ID_Type(slash)", u"Not_XID", getIDTypes(0x2F));
    assertEquals("ID_Type(digit 0)", u"Recommended", getIDTypes(0x30));
    assertEquals("ID_Type(colon)", u"Inclusion", getIDTypes(0x3A));
    assertEquals("ID_Type(semicolon)", u"Not_XID", getIDTypes(0x3B));
    assertEquals("ID_Type(Greek small alpha)", u"Recommended", getIDTypes(0x03B1));
    assertEquals("ID_Type(Greek small archaic koppa)", u"Obsolete", getIDTypes(0x03D9));
    assertEquals("ID_Type(Hangul syllable)", u"Recommended", getIDTypes(0xAC00));
    assertEquals("ID_Type(surrogate)", u"Not_Character", getIDTypes(0xD800));
    assertEquals("ID_Type(Arabic tail fragment)", u"Technical", getIDTypes(0xFE73));
    assertEquals("ID_Type(Linear B syllable)", u"Exclusion", getIDTypes(0x10000));
    assertEquals("ID_Type(Hentaigana ko-3)", u"Obsolete", getIDTypes(0x1B03A));
    assertEquals("ID_Type(Katakana small ko)", u"Recommended", getIDTypes(0x1B155));
    assertEquals("ID_Type(U+2EE5D)", u"Recommended", getIDTypes(0x2EE5D));
    assertEquals("ID_Type(U+10FFFF)", u"Not_Character", getIDTypes(0x10FFFF));

    assertEquals("ID_Type(CYRILLIC THOUSANDS SIGN)", u"Not_XID Obsolete", getIDTypes(0x0482));
    assertEquals("ID_Type(SYRIAC FEMININE DOT)", u"Technical Limited_Use", getIDTypes(0x0740));
    assertEquals("ID_Type(NKO LETTER JONA JA)", u"Obsolete Limited_Use", getIDTypes(0x07E8));
    assertEquals("ID_Type(SYRIAC END OF PARAGRAPH)", u"Not_XID Limited_Use", getIDTypes(0x0700));
    assertEquals("ID_Type(LATIN SMALL LETTER EZH)=", u"Technical Uncommon_Use", getIDTypes(0x0292));
    assertEquals("ID_Type(MUSICAL SYMBOL KIEVAN C CLEF)", u"Not_XID Technical Uncommon_Use", getIDTypes(0x1D1DE));
    assertEquals("ID_Type(MRO LETTER TA)", u"Exclusion Uncommon_Use", getIDTypes(0x16A40));
    assertEquals("ID_Type(GREEK MUSICAL LEIMMA)", u"Not_XID Obsolete", getIDTypes(0x1D245));

    // error handling
    UIdentifierType types[2];
    UErrorCode failure = U_ZERO_ERROR;
    u_getIDTypes(0, types, -1, &failure);
    assertEquals("u_getIDTypes(capacity<0)", U_ILLEGAL_ARGUMENT_ERROR, failure);

    failure = U_ZERO_ERROR;
    u_getIDTypes(0, nullptr, 1, &failure);
    assertEquals("u_getIDTypes(nullptr)", U_ILLEGAL_ARGUMENT_ERROR, failure);

    failure = U_ZERO_ERROR;
    int32_t length = u_getIDTypes(0x30, types, 0, &failure);
    assertEquals("u_getIDTypes(digit 0, capacity 0) overflow", U_BUFFER_OVERFLOW_ERROR, failure);
    assertEquals("u_getIDTypes(digit 0, capacity 0) length", 1, length);

    failure = U_ZERO_ERROR;
    length = u_getIDTypes(0x1D1DE, types, 0, &failure);
    assertEquals("u_getIDTypes(Kievan C clef, capacity 2) overflow", U_BUFFER_OVERFLOW_ERROR, failure);
    assertEquals("u_getIDTypes(Kievan C clef, capacity 2) length", 3, length);

    // Property names work and get the correct sets.
    UnicodeSet rec(u"[:Identifier_Type=Recommended:]", errorCode);
    UnicodeSet incl(u"[:Identifier_Type=Inclusion:]", errorCode);
    UnicodeSet limited(u"[:Identifier_Type=Limited_Use:]", errorCode);
    UnicodeSet uncommon(u"[:Identifier_Type=Uncommon_Use:]", errorCode);
    UnicodeSet notChar(u"[:Identifier_Type=Not_Character:]", errorCode);
    // Unicode 15.1 set sizes; normally grows over time except Not_Character shrinks
    assertTrue("Recommended number of characters", rec.size() >= 112761);
    assertTrue("Inclusion number of characters", incl.size() >= 17);
    assertTrue("Limited_Use number of characters", limited.size() >= 5268);
    assertTrue("Uncommon_Use number of characters", uncommon.size() >= 398);
    assertTrue("Not_Character number of characters",
               800000 <= notChar.size() && notChar.size() <= 964293);
    assertFalse("Recommended.contains(slash)", rec.contains(0x2F));
    assertTrue("Recommended.contains(digit 0)", rec.contains(0x30));
    assertTrue("Inclusion.contains(colon)", incl.contains(0x3A));
    assertTrue("Recommended.contains(U+2EE5D)", rec.contains(0x2EE5D));
    assertTrue("Limited_Use.contains(SYRIAC FEMININE DOT)", limited.contains(0x0740));
    assertTrue("Limited_Use.contains(NKO LETTER JONA JA)", limited.contains(0x7E8));
    assertTrue("Not_Character.contains(surrogate)", notChar.contains(0xd800));
    assertTrue("Not_Character.contains(U+10FFFF)", notChar.contains(0x10FFFF));
    assertTrue("Uncommon_Use.contains(LATIN SMALL LETTER EZH)", uncommon.contains(0x0292));
    assertTrue("Uncommon_Use.contains(MUSICAL SYMBOL KIEVAN C CLEF)", uncommon.contains(0x1D1DE));

    // More mutually exclusive types, including some otherwise combinable ones.
    UnicodeSet dep(u"[:Identifier_Type=Deprecated:]", errorCode);
    UnicodeSet di(u"[:Identifier_Type=Default_Ignorable:]", errorCode);
    UnicodeSet notNFKC(u"[:Identifier_Type=Not_NFKC:]", errorCode);
    UnicodeSet excl(u"[:Identifier_Type=Exclusion:]", errorCode);
    UnicodeSet allExclusive;
    allExclusive.addAll(rec).addAll(incl).addAll(limited).addAll(excl).
        addAll(notNFKC).addAll(di).addAll(dep).addAll(notChar);
    assertEquals("num chars in mutually exclusive types",
                rec.size() + incl.size() + limited.size() + excl.size() +
                    notNFKC.size() + di.size() + dep.size() + notChar.size(),
                allExclusive.size());
}
