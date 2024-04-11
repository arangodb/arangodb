// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// localematchertest.cpp
// created: 2019jul04 Markus W. Scherer

#include <string>
#include <vector>
#include <utility>

#include "unicode/utypes.h"
#include "unicode/localematcher.h"
#include "unicode/locid.h"
#include "charstr.h"
#include "cmemory.h"
#include "intltest.h"
#include "localeprioritylist.h"
#include "ucbuf.h"

#define ARRAY_RANGE(array) (array), ((array) + UPRV_LENGTHOF(array))

namespace {

const char *locString(const Locale *loc) {
    return loc != nullptr ? loc->getName() : "(null)";
}

struct TestCase {
    int32_t lineNr = 0;

    CharString supported;
    CharString def;
    UnicodeString favor;
    UnicodeString threshold;
    CharString desired;
    CharString expMatch;
    CharString expDesired;
    CharString expCombined;

    void reset() {
        supported.clear();
        def.clear();
        favor.remove();
        threshold.remove();
    }
};

}  // namespace

class LocaleMatcherTest : public IntlTest {
public:
    LocaleMatcherTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=nullptr) override;

    void testEmpty();
    void testCopyErrorTo();
    void testBasics();
    void testSupportedDefault();
    void testUnsupportedDefault();
    void testNoDefault();
    void testDemotion();
    void testDirection();
    void testMaxDistanceAndIsMatch();
    void testMatch();
    void testResolvedLocale();
    void testDataDriven();

private:
    UBool dataDriven(const TestCase &test, IcuTestErrorCode &errorCode);
};

extern IntlTest *createLocaleMatcherTest() {
    return new LocaleMatcherTest();
}

void LocaleMatcherTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite LocaleMatcherTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testEmpty);
    TESTCASE_AUTO(testCopyErrorTo);
    TESTCASE_AUTO(testBasics);
    TESTCASE_AUTO(testSupportedDefault);
    TESTCASE_AUTO(testUnsupportedDefault);
    TESTCASE_AUTO(testNoDefault);
    TESTCASE_AUTO(testDemotion);
    TESTCASE_AUTO(testDirection);
    TESTCASE_AUTO(testMaxDistanceAndIsMatch);
    TESTCASE_AUTO(testMatch);
    TESTCASE_AUTO(testResolvedLocale);
    TESTCASE_AUTO(testDataDriven);
    TESTCASE_AUTO_END;
}

void LocaleMatcherTest::testEmpty() {
    IcuTestErrorCode errorCode(*this, "testEmpty");
    LocaleMatcher matcher = LocaleMatcher::Builder().build(errorCode);
    const Locale *best = matcher.getBestMatch(Locale::getFrench(), errorCode);
    assertEquals("getBestMatch(fr)", "(null)", locString(best));
    LocaleMatcher::Result result = matcher.getBestMatchResult("fr", errorCode);
    assertEquals("getBestMatchResult(fr).des", "(null)", locString(result.getDesiredLocale()));
    assertEquals("getBestMatchResult(fr).desIndex", -1, result.getDesiredIndex());
    assertEquals("getBestMatchResult(fr).supp",
                 "(null)", locString(result.getSupportedLocale()));
    assertEquals("getBestMatchResult(fr).suppIndex",
                 -1, result.getSupportedIndex());
}

void LocaleMatcherTest::testCopyErrorTo() {
    IcuTestErrorCode errorCode(*this, "testCopyErrorTo");
    // The builder does not set any errors except out-of-memory.
    // Test what we can.
    LocaleMatcher::Builder builder;
    UErrorCode success = U_ZERO_ERROR;
    assertFalse("no error", builder.copyErrorTo(success));
    assertTrue("still success", U_SUCCESS(success));
    UErrorCode failure = U_INVALID_FORMAT_ERROR;
    assertTrue("failure passed in", builder.copyErrorTo(failure));
    assertEquals("same failure", U_INVALID_FORMAT_ERROR, failure);
}

void LocaleMatcherTest::testBasics() {
    IcuTestErrorCode errorCode(*this, "testBasics");
    Locale locales[] = { "fr", "en_GB", "en" };
    {
        LocaleMatcher matcher = LocaleMatcher::Builder().
            setSupportedLocales(ARRAY_RANGE(locales)).build(errorCode);
        const Locale *best = matcher.getBestMatch("en_GB", errorCode);
        assertEquals("fromRange.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("fromRange.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("fromRange.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("fromRange.getBestMatch(ja_JP)", "fr", locString(best));
    }
    // Code coverage: Variations of setting supported locales.
    {
        std::vector<Locale> locales{ "fr", "en_GB", "en" };
        LocaleMatcher matcher = LocaleMatcher::Builder().
            setSupportedLocales(locales.begin(), locales.end()).build(errorCode);
        const Locale *best = matcher.getBestMatch("en_GB", errorCode);
        assertEquals("fromRange.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("fromRange.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("fromRange.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("fromRange.getBestMatch(ja_JP)", "fr", locString(best));
    }
    {
        Locale::RangeIterator<Locale *> iter(ARRAY_RANGE(locales));
        LocaleMatcher matcher = LocaleMatcher::Builder().
            setSupportedLocales(iter).build(errorCode);
        const Locale *best = matcher.getBestMatch("en_GB", errorCode);
        assertEquals("fromIter.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("fromIter.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("fromIter.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("fromIter.getBestMatch(ja_JP)", "fr", locString(best));
    }
    {
        Locale *pointers[] = { locales, locales + 1, locales + 2 };
        // Lambda with explicit reference return type to prevent copy-constructing a temporary
        // which would be destructed right away.
        LocaleMatcher matcher = LocaleMatcher::Builder().
            setSupportedLocalesViaConverter(
                ARRAY_RANGE(pointers), [](const Locale *p) -> const Locale & { return *p; }).
            build(errorCode);
        const Locale *best = matcher.getBestMatch("en_GB", errorCode);
        assertEquals("viaConverter.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("viaConverter.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("viaConverter.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("viaConverter.getBestMatch(ja_JP)", "fr", locString(best));
    }
    {
        LocaleMatcher matcher = LocaleMatcher::Builder().
            addSupportedLocale(locales[0]).
            addSupportedLocale(locales[1]).
            addSupportedLocale(locales[2]).
            build(errorCode);
        const Locale *best = matcher.getBestMatch("en_GB", errorCode);
        assertEquals("added.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("added.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("added.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("added.getBestMatch(ja_JP)", "fr", locString(best));
    }
    {
        LocaleMatcher matcher = LocaleMatcher::Builder().
            setSupportedLocalesFromListString(
                " el, fr;q=0.555555, en-GB ; q = 0.88  , el; q =0, en;q=0.88 , fr ").
            build(errorCode);
        const Locale *best = matcher.getBestMatchForListString("el, fr, fr;q=0, en-GB", errorCode);
        assertEquals("fromList.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("fromList.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("fromList.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("fromList.getBestMatch(ja_JP)", "fr", locString(best));
    }
    // more API coverage
    {
        LocalePriorityList list("fr, en-GB", errorCode);
        LocalePriorityList::Iterator iter(list.iterator());
        LocaleMatcher matcher = LocaleMatcher::Builder().
            setSupportedLocales(iter).
            addSupportedLocale(Locale::getEnglish()).
            setDefaultLocale(&Locale::getGerman()).
            build(errorCode);
        const Locale *best = matcher.getBestMatch("en_GB", errorCode);
        assertEquals("withDefault.getBestMatch(en_GB)", "en_GB", locString(best));
        best = matcher.getBestMatch("en_US", errorCode);
        assertEquals("withDefault.getBestMatch(en_US)", "en", locString(best));
        best = matcher.getBestMatch("fr_FR", errorCode);
        assertEquals("withDefault.getBestMatch(fr_FR)", "fr", locString(best));
        best = matcher.getBestMatch("ja_JP", errorCode);
        assertEquals("withDefault.getBestMatch(ja_JP)", "de", locString(best));

        Locale desired("en_GB");  // distinct object from Locale.UK
        LocaleMatcher::Result result = matcher.getBestMatchResult(desired, errorCode);
        assertTrue("withDefault: exactly desired en-GB object",
                   &desired == result.getDesiredLocale());
        assertEquals("withDefault: en-GB desired index", 0, result.getDesiredIndex());
        assertEquals("withDefault: en-GB supported",
                     "en_GB", locString(result.getSupportedLocale()));
        assertEquals("withDefault: en-GB supported index", 1, result.getSupportedIndex());

        LocalePriorityList list2("ja-JP, en-US", errorCode);
        LocalePriorityList::Iterator iter2(list2.iterator());
        result = matcher.getBestMatchResult(iter2, errorCode);
        assertEquals("withDefault: ja-JP, en-US desired index", 1, result.getDesiredIndex());
        assertEquals("withDefault: ja-JP, en-US desired",
                     "en_US", locString(result.getDesiredLocale()));

        desired = Locale("en", "US");  // distinct object from Locale.US
        result = matcher.getBestMatchResult(desired, errorCode);
        assertTrue("withDefault: exactly desired en-US object",
                   &desired == result.getDesiredLocale());
        assertEquals("withDefault: en-US desired index", 0, result.getDesiredIndex());
        assertEquals("withDefault: en-US supported", "en", locString(result.getSupportedLocale()));
        assertEquals("withDefault: en-US supported index", 2, result.getSupportedIndex());

        result = matcher.getBestMatchResult("ja_JP", errorCode);
        assertEquals("withDefault: ja-JP desired", "(null)", locString(result.getDesiredLocale()));
        assertEquals("withDefault: ja-JP desired index", -1, result.getDesiredIndex());
        assertEquals("withDefault: ja-JP supported", "de", locString(result.getSupportedLocale()));
        assertEquals("withDefault: ja-JP supported index", -1, result.getSupportedIndex());
    }
}

void LocaleMatcherTest::testSupportedDefault() {
    // The default locale is one of the supported locales.
    IcuTestErrorCode errorCode(*this, "testSupportedDefault");
    Locale locales[] = { "fr", "en_GB", "en" };
    LocaleMatcher matcher = LocaleMatcher::Builder().
        setSupportedLocales(ARRAY_RANGE(locales)).
        setDefaultLocale(&locales[1]).
        build(errorCode);
    const Locale *best = matcher.getBestMatch("en_GB", errorCode);
    assertEquals("getBestMatch(en_GB)", "en_GB", locString(best));
    best = matcher.getBestMatch("en_US", errorCode);
    assertEquals("getBestMatch(en_US)", "en", locString(best));
    best = matcher.getBestMatch("fr_FR", errorCode);
    assertEquals("getBestMatch(fr_FR)", "fr", locString(best));
    best = matcher.getBestMatch("ja_JP", errorCode);
    assertEquals("getBestMatch(ja_JP)", "en_GB", locString(best));
    LocaleMatcher::Result result = matcher.getBestMatchResult("ja_JP", errorCode);
    assertEquals("getBestMatchResult(ja_JP).supp",
                 "en_GB", locString(result.getSupportedLocale()));
    assertEquals("getBestMatchResult(ja_JP).suppIndex",
                 -1, result.getSupportedIndex());
}

void LocaleMatcherTest::testUnsupportedDefault() {
    // The default locale does not match any of the supported locales.
    IcuTestErrorCode errorCode(*this, "testUnsupportedDefault");
    Locale locales[] = { "fr", "en_GB", "en" };
    Locale def("de");
    LocaleMatcher matcher = LocaleMatcher::Builder().
        setSupportedLocales(ARRAY_RANGE(locales)).
        setDefaultLocale(&def).
        build(errorCode);
    const Locale *best = matcher.getBestMatch("en_GB", errorCode);
    assertEquals("getBestMatch(en_GB)", "en_GB", locString(best));
    best = matcher.getBestMatch("en_US", errorCode);
    assertEquals("getBestMatch(en_US)", "en", locString(best));
    best = matcher.getBestMatch("fr_FR", errorCode);
    assertEquals("getBestMatch(fr_FR)", "fr", locString(best));
    best = matcher.getBestMatch("ja_JP", errorCode);
    assertEquals("getBestMatch(ja_JP)", "de", locString(best));
    LocaleMatcher::Result result = matcher.getBestMatchResult("ja_JP", errorCode);
    assertEquals("getBestMatchResult(ja_JP).supp",
                 "de", locString(result.getSupportedLocale()));
    assertEquals("getBestMatchResult(ja_JP).suppIndex",
                 -1, result.getSupportedIndex());
}

void LocaleMatcherTest::testNoDefault() {
    // We want nullptr instead of any default locale.
    IcuTestErrorCode errorCode(*this, "testNoDefault");
    Locale locales[] = { "fr", "en_GB", "en" };
    LocaleMatcher matcher = LocaleMatcher::Builder().
        setSupportedLocales(ARRAY_RANGE(locales)).
        setNoDefaultLocale().
        build(errorCode);
    const Locale *best = matcher.getBestMatch("en_GB", errorCode);
    assertEquals("getBestMatch(en_GB)", "en_GB", locString(best));
    best = matcher.getBestMatch("en_US", errorCode);
    assertEquals("getBestMatch(en_US)", "en", locString(best));
    best = matcher.getBestMatch("fr_FR", errorCode);
    assertEquals("getBestMatch(fr_FR)", "fr", locString(best));
    best = matcher.getBestMatch("ja_JP", errorCode);
    assertEquals("getBestMatch(ja_JP)", "(null)", locString(best));
    LocaleMatcher::Result result = matcher.getBestMatchResult("ja_JP", errorCode);
    assertEquals("getBestMatchResult(ja_JP).supp",
                 "(null)", locString(result.getSupportedLocale()));
    assertEquals("getBestMatchResult(ja_JP).suppIndex",
                 -1, result.getSupportedIndex());
}

void LocaleMatcherTest::testDemotion() {
    IcuTestErrorCode errorCode(*this, "testDemotion");
    Locale supported[] = { "fr", "de-CH", "it" };
    Locale desired[] = { "fr-CH", "de-CH", "it" };
    {
        LocaleMatcher noDemotion = LocaleMatcher::Builder().
            setSupportedLocales(ARRAY_RANGE(supported)).
            setDemotionPerDesiredLocale(ULOCMATCH_DEMOTION_NONE).build(errorCode);
        Locale::RangeIterator<Locale *> desiredIter(ARRAY_RANGE(desired));
        assertEquals("no demotion",
                     "de_CH", locString(noDemotion.getBestMatch(desiredIter, errorCode)));
    }

    {
        LocaleMatcher regionDemotion = LocaleMatcher::Builder().
            setSupportedLocales(ARRAY_RANGE(supported)).
            setDemotionPerDesiredLocale(ULOCMATCH_DEMOTION_REGION).build(errorCode);
        Locale::RangeIterator<Locale *> desiredIter(ARRAY_RANGE(desired));
        assertEquals("region demotion",
                     "fr", locString(regionDemotion.getBestMatch(desiredIter, errorCode)));
    }
}

void LocaleMatcherTest::testDirection() {
    IcuTestErrorCode errorCode(*this, "testDirection");
    Locale supported[] = { "ar", "nn" };
    Locale desired[] = { "arz-EG", "nb-DK" };
    LocaleMatcher::Builder builder;
    builder.setSupportedLocales(ARRAY_RANGE(supported));
    {
        // arz is a close one-way match to ar, and the region matches.
        // (Egyptian Arabic vs. Arabic)
        // Also explicitly exercise the move copy constructor.
        LocaleMatcher built = builder.build(errorCode);
        LocaleMatcher withOneWay(std::move(built));
        Locale::RangeIterator<Locale *> desiredIter(ARRAY_RANGE(desired));
        assertEquals("with one-way", "ar",
                     locString(withOneWay.getBestMatch(desiredIter, errorCode)));
    }
    {
        // nb is a less close two-way match to nn, and the regions differ.
        // (Norwegian Bokmal vs. Nynorsk)
        // Also explicitly exercise the move assignment operator.
        LocaleMatcher onlyTwoWay = builder.build(errorCode);
        LocaleMatcher built =
            builder.setDirection(ULOCMATCH_DIRECTION_ONLY_TWO_WAY).build(errorCode);
        onlyTwoWay = std::move(built);
        Locale::RangeIterator<Locale *> desiredIter(ARRAY_RANGE(desired));
        assertEquals("only two-way", "nn",
                     locString(onlyTwoWay.getBestMatch(desiredIter, errorCode)));
    }
}

void LocaleMatcherTest::testMaxDistanceAndIsMatch() {
    IcuTestErrorCode errorCode(*this, "testMaxDistanceAndIsMatch");
    LocaleMatcher::Builder builder;
    LocaleMatcher standard = builder.build(errorCode);
    Locale germanLux("de-LU");
    Locale germanPhoenician("de-Phnx-AT");
    Locale greek("el");
    assertTrue("standard de-LU / de", standard.isMatch(germanLux, Locale::getGerman(), errorCode));
    assertFalse("standard de-Phnx-AT / de",
                standard.isMatch(germanPhoenician, Locale::getGerman(), errorCode));

    // Allow a script difference to still match.
    LocaleMatcher loose =
        builder.setMaxDistance(germanPhoenician, Locale::getGerman()).build(errorCode);
    assertTrue("loose de-LU / de", loose.isMatch(germanLux, Locale::getGerman(), errorCode));
    assertTrue("loose de-Phnx-AT / de",
               loose.isMatch(germanPhoenician, Locale::getGerman(), errorCode));
    assertFalse("loose el / de", loose.isMatch(greek, Locale::getGerman(), errorCode));

    // Allow at most a regional difference.
    LocaleMatcher regional =
        builder.setMaxDistance(Locale("de-AT"), Locale::getGerman()).build(errorCode);
    assertTrue("regional de-LU / de",
               regional.isMatch(Locale("de-LU"), Locale::getGerman(), errorCode));
    assertFalse("regional da / no", regional.isMatch(Locale("da"), Locale("no"), errorCode));
    assertFalse("regional zh-Hant / zh",
                regional.isMatch(Locale::getChinese(), Locale::getTraditionalChinese(), errorCode));
}


void LocaleMatcherTest::testMatch() {
    IcuTestErrorCode errorCode(*this, "testMatch");
    LocaleMatcher matcher = LocaleMatcher::Builder().build(errorCode);

    // Java test function testMatch_exact()
    Locale en_CA("en_CA");
    assertEquals("exact match", 1.0, matcher.internalMatch(en_CA, en_CA, errorCode));

    // testMatch_none
    Locale ar_MK("ar_MK");
    double match = matcher.internalMatch(ar_MK, en_CA, errorCode);
    assertTrue("mismatch: 0<=match<0.2", 0 <= match && match < 0.2);

    // testMatch_matchOnMaximized
    Locale und_TW("und_TW");
    Locale zh("zh");
    Locale zh_Hant("zh_Hant");
    double matchZh = matcher.internalMatch(und_TW, zh, errorCode);
    double matchZhHant = matcher.internalMatch(und_TW, zh_Hant, errorCode);
    assertTrue("und_TW should be closer to zh_Hant than to zh",
               matchZh < matchZhHant);
    Locale en_Hant_TW("en_Hant_TW");
    double matchEnHantTw = matcher.internalMatch(en_Hant_TW, zh_Hant, errorCode);
    assertTrue("zh_Hant should be closer to und_TW than to en_Hant_TW",
               matchEnHantTw < matchZhHant);
    assertTrue("zh should not match und_TW or en_Hant_TW",
               matchZh == 0.0 && matchEnHantTw == 0.0); // with changes in CLDR-1435
}

void LocaleMatcherTest::testResolvedLocale() {
    IcuTestErrorCode errorCode(*this, "testResolvedLocale");
    LocaleMatcher matcher = LocaleMatcher::Builder().
        addSupportedLocale("ar-EG").
        build(errorCode);
    Locale desired("ar-SA-u-nu-latn");
    LocaleMatcher::Result result = matcher.getBestMatchResult(desired, errorCode);
    assertEquals("best", "ar_EG", locString(result.getSupportedLocale()));
    Locale resolved = result.makeResolvedLocale(errorCode);
    assertEquals("ar-EG + ar-SA-u-nu-latn = ar-SA-u-nu-latn",
                 "ar-SA-u-nu-latn",
                 resolved.toLanguageTag<std::string>(errorCode).data());
}

namespace {

bool toInvariant(const UnicodeString &s, CharString &inv, ErrorCode &errorCode) {
    if (errorCode.isSuccess()) {
        inv.clear().appendInvariantChars(s, errorCode);
        return errorCode.isSuccess();
    }
    return false;
}

bool getSuffixAfterPrefix(const UnicodeString &s, int32_t limit,
                          const UnicodeString &prefix, UnicodeString &suffix) {
    if (prefix.length() <= limit && s.startsWith(prefix)) {
        suffix.setTo(s, prefix.length(), limit - prefix.length());
        return true;
    } else {
        return false;
    }
}

bool getInvariantSuffixAfterPrefix(const UnicodeString &s, int32_t limit,
                                   const UnicodeString &prefix, CharString &suffix,
                                   ErrorCode &errorCode) {
    UnicodeString u_suffix;
    return getSuffixAfterPrefix(s, limit, prefix, u_suffix) &&
        toInvariant(u_suffix, suffix, errorCode);
}

bool readTestCase(const UnicodeString &line, TestCase &test, IcuTestErrorCode &errorCode) {
    if (errorCode.isFailure()) { return false; }
    ++test.lineNr;
    // Start of comment, or end of line, minus trailing spaces.
    int32_t limit = line.indexOf(u'#');
    if (limit < 0) {
        limit = line.length();
        // Remove trailing CR LF.
        char16_t c;
        while (limit > 0 && ((c = line.charAt(limit - 1)) == u'\n' || c == u'\r')) {
            --limit;
        }
    }
    // Remove spaces before comment or at the end of the line.
    char16_t c;
    while (limit > 0 && ((c = line.charAt(limit - 1)) == u' ' || c == u'\t')) {
        --limit;
    }
    if (limit == 0) {  // empty line
        return false;
    }
    if (line.startsWith(u"** test: ")) {
        test.reset();
    } else if (getInvariantSuffixAfterPrefix(line, limit, u"@supported=",
                                             test.supported, errorCode)) {
    } else if (getInvariantSuffixAfterPrefix(line, limit, u"@default=",
                                             test.def, errorCode)) {
    } else if (getSuffixAfterPrefix(line, limit, u"@favor=", test.favor)) {
    } else if (getSuffixAfterPrefix(line, limit, u"@threshold=", test.threshold)) {
    } else {
        int32_t matchSep = line.indexOf(u">>");
        // >> before an inline comment, and followed by more than white space.
        if (0 <= matchSep && (matchSep + 2) < limit) {
            toInvariant(line.tempSubStringBetween(0, matchSep).trim(), test.desired, errorCode);
            test.expDesired.clear();
            test.expCombined.clear();
            int32_t start = matchSep + 2;
            int32_t expLimit = line.indexOf(u'|', start);
            if (expLimit < 0) {
                toInvariant(line.tempSubStringBetween(start, limit).trim(),
                            test.expMatch, errorCode);
            } else {
                toInvariant(line.tempSubStringBetween(start, expLimit).trim(),
                            test.expMatch, errorCode);
                start = expLimit + 1;
                expLimit = line.indexOf(u'|', start);
                if (expLimit < 0) {
                    toInvariant(line.tempSubStringBetween(start, limit).trim(),
                                test.expDesired, errorCode);
                } else {
                    toInvariant(line.tempSubStringBetween(start, expLimit).trim(),
                                test.expDesired, errorCode);
                    toInvariant(line.tempSubStringBetween(expLimit + 1, limit).trim(),
                                test.expCombined, errorCode);
                }
            }
            return errorCode.isSuccess();
        } else {
            errorCode.set(U_INVALID_FORMAT_ERROR);
        }
    }
    return false;
}

Locale *getLocaleOrNull(const CharString &s, Locale &locale) {
    if (s == "null") {
        return nullptr;
    } else {
        return &(locale = Locale(s.data()));
    }
}

}  // namespace

UBool LocaleMatcherTest::dataDriven(const TestCase &test, IcuTestErrorCode &errorCode) {
    LocaleMatcher::Builder builder;
    builder.setSupportedLocalesFromListString(test.supported.toStringPiece());
    if (!test.def.isEmpty()) {
        Locale defaultLocale(test.def.data());
        builder.setDefaultLocale(&defaultLocale);
    }
    if (!test.favor.isEmpty()) {
        ULocMatchFavorSubtag favor;
        if (test.favor == u"normal") {
            favor = ULOCMATCH_FAVOR_LANGUAGE;
        } else if (test.favor == u"script") {
            favor = ULOCMATCH_FAVOR_SCRIPT;
        } else {
            errln(UnicodeString(u"unsupported FavorSubtag value ") + test.favor);
            return false;
        }
        builder.setFavorSubtag(favor);
    }
    if (!test.threshold.isEmpty()) {
        infoln("skipping test case on line %d with non-default threshold: not exposed via API",
               (int)test.lineNr);
        return true;
        // int32_t threshold = Integer.valueOf(test.threshold);
        // builder.internalSetThresholdDistance(threshold);
    }
    LocaleMatcher matcher = builder.build(errorCode);
    if (errorCode.errIfFailureAndReset("LocaleMatcher::Builder::build()")) {
        return false;
    }

    Locale expMatchLocale("");
    Locale *expMatch = getLocaleOrNull(test.expMatch, expMatchLocale);
    if (test.expDesired.isEmpty() && test.expCombined.isEmpty()) {
        StringPiece desiredSP = test.desired.toStringPiece();
        const Locale *bestSupported = matcher.getBestMatchForListString(desiredSP, errorCode);
        if (!assertEquals("bestSupported from string",
                          locString(expMatch), locString(bestSupported))) {
            return false;
        }
        LocalePriorityList desired(test.desired.toStringPiece(), errorCode);
        LocalePriorityList::Iterator desiredIter = desired.iterator();
        if (desired.getLength() == 1) {
            const Locale &desiredLocale = desiredIter.next();
            bestSupported = matcher.getBestMatch(desiredLocale, errorCode);
            UBool ok = assertEquals("bestSupported from Locale",
                                    locString(expMatch), locString(bestSupported));

            LocaleMatcher::Result result = matcher.getBestMatchResult(desiredLocale, errorCode);
            return ok & assertEquals("result.getSupportedLocale from Locale",
                                     locString(expMatch), locString(result.getSupportedLocale()));
        } else {
            bestSupported = matcher.getBestMatch(desiredIter, errorCode);
            return assertEquals("bestSupported from Locale iterator",
                                locString(expMatch), locString(bestSupported));
        }
    } else {
        LocalePriorityList desired(test.desired.toStringPiece(), errorCode);
        LocalePriorityList::Iterator desiredIter = desired.iterator();
        LocaleMatcher::Result result = matcher.getBestMatchResult(desiredIter, errorCode);
        UBool ok = assertEquals("result.getSupportedLocale from Locales",
                                locString(expMatch), locString(result.getSupportedLocale()));
        if (!test.expDesired.isEmpty()) {
            Locale expDesiredLocale("");
            Locale *expDesired = getLocaleOrNull(test.expDesired, expDesiredLocale);
            ok &= assertEquals("result.getDesiredLocale from Locales",
                               locString(expDesired), locString(result.getDesiredLocale()));
        }
        if (!test.expCombined.isEmpty()) {
            if (test.expMatch.contains("-u-")) {
                logKnownIssue("20727",
                              UnicodeString(u"ignoring makeResolvedLocale() line ") + test.lineNr);
                return ok;
            }
            Locale expCombinedLocale("");
            Locale *expCombined = getLocaleOrNull(test.expCombined, expCombinedLocale);
            Locale combined = result.makeResolvedLocale(errorCode);
            ok &= assertEquals("combined Locale from Locales",
                               locString(expCombined), locString(&combined));
        }
        return ok;
    }
}

void LocaleMatcherTest::testDataDriven() {
    IcuTestErrorCode errorCode(*this, "testDataDriven");
    CharString path(getSourceTestData(errorCode), errorCode);
    path.appendPathPart("localeMatcherTest.txt", errorCode);
    const char *codePage = "UTF-8";
    LocalUCHARBUFPointer f(ucbuf_open(path.data(), &codePage, true, false, errorCode));
    if(errorCode.errIfFailureAndReset("ucbuf_open(localeMatcherTest.txt)")) {
        return;
    }
    int32_t lineLength;
    const char16_t *p;
    UnicodeString line;
    TestCase test;
    int32_t numPassed = 0;
    while ((p = ucbuf_readline(f.getAlias(), &lineLength, errorCode)) != nullptr &&
            errorCode.isSuccess()) {
        line.setTo(false, p, lineLength);
        if (!readTestCase(line, test, errorCode)) {
            if (errorCode.errIfFailureAndReset(
                    "test data syntax error on line %d", (int)test.lineNr)) {
                infoln(line);
            }
            continue;
        }
        UBool ok = dataDriven(test, errorCode);
        if (errorCode.errIfFailureAndReset("test error on line %d", (int)test.lineNr)) {
            infoln(line);
        } else if (!ok) {
            infoln("test failure on line %d", (int)test.lineNr);
            infoln(line);
        } else {
            ++numPassed;
        }
    }
    infoln("number of passing test cases: %d", (int)numPassed);
}
