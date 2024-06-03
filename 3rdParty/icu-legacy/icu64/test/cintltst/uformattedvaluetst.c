// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uformattedvalue.h"
#include "unicode/unum.h"
#include "unicode/ustring.h"
#include "cformtst.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"

static void TestBasic(void);
static void TestSetters(void);

static void AssertAllPartsEqual(
    const char* messagePrefix,
    const UConstrainedFieldPosition* ucfpos,
    int32_t matching,
    UFieldCategory category,
    int32_t field,
    int32_t start,
    int32_t limit,
    int64_t context);

void addUFormattedValueTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/uformattedvalue/" #x)

void addUFormattedValueTest(TestNode** root) {
    TESTCASE(TestBasic);
    TESTCASE(TestSetters);
}


static void TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    UConstrainedFieldPosition* ucfpos = ucfpos_open(&status);
    assertSuccess("opening ucfpos", &status);
    assertTrue("ucfpos should not be null", ucfpos != NULL);

    AssertAllPartsEqual(
        "basic",
        ucfpos,
        7,
        UFIELD_CATEGORY_UNDEFINED,
        0,
        0,
        0,
        0LL);

    ucfpos_close(ucfpos);
}

void TestSetters() {
    UErrorCode status = U_ZERO_ERROR;
    UConstrainedFieldPosition* ucfpos = ucfpos_open(&status);
    assertSuccess("opening ucfpos", &status);
    assertTrue("ucfpos should not be null", ucfpos != NULL);

    ucfpos_constrainCategory(ucfpos, UFIELD_CATEGORY_DATE, &status);
    assertSuccess("setters 0", &status);
    AssertAllPartsEqual(
        "setters 0",
        ucfpos,
        4,
        UFIELD_CATEGORY_DATE,
        0,
        0,
        0,
        0LL);

    ucfpos_constrainField(ucfpos, UFIELD_CATEGORY_NUMBER, UNUM_COMPACT_FIELD, &status);
    assertSuccess("setters 1", &status);
    AssertAllPartsEqual(
        "setters 1",
        ucfpos,
        2,
        UFIELD_CATEGORY_NUMBER,
        UNUM_COMPACT_FIELD,
        0,
        0,
        0LL);

    ucfpos_setInt64IterationContext(ucfpos, 42424242424242LL, &status);
    assertSuccess("setters 2", &status);
    AssertAllPartsEqual(
        "setters 2",
        ucfpos,
        2,
        UFIELD_CATEGORY_NUMBER,
        UNUM_COMPACT_FIELD,
        0,
        0,
        42424242424242LL);

    ucfpos_setState(ucfpos, UFIELD_CATEGORY_NUMBER, UNUM_COMPACT_FIELD, 5, 10, &status);
    assertSuccess("setters 3", &status);
    AssertAllPartsEqual(
        "setters 3",
        ucfpos,
        2,
        UFIELD_CATEGORY_NUMBER,
        UNUM_COMPACT_FIELD,
        5,
        10,
        42424242424242LL);

    ucfpos_reset(ucfpos, &status);
    assertSuccess("setters 4", &status);
    AssertAllPartsEqual(
        "setters 4",
        ucfpos,
        7,
        UFIELD_CATEGORY_UNDEFINED,
        0,
        0,
        0,
        0LL);

    ucfpos_close(ucfpos);
}

/** For matching, turn on these bits:
 *
 * 1 = UNUM_INTEGER_FIELD
 * 2 = UNUM_COMPACT_FIELD
 * 4 = UDAT_AM_PM_FIELD
 */
static void AssertAllPartsEqual(
        const char* messagePrefix,
        const UConstrainedFieldPosition* ucfpos,
        int32_t matching,
        UFieldCategory category,
        int32_t field,
        int32_t start,
        int32_t limit,
        int64_t context) {

    UErrorCode status = U_ZERO_ERROR;

    char message[256];
    uprv_strncpy(message, messagePrefix, 256);
    int32_t prefixEnd = (int32_t)uprv_strlen(messagePrefix);
    message[prefixEnd++] = ':';
    message[prefixEnd++] = ' ';
    U_ASSERT(prefixEnd < 256);

#define AAPE_MSG(suffix) (uprv_strncpy(message+prefixEnd, suffix, 256-prefixEnd)-prefixEnd)

    UFieldCategory _category = ucfpos_getCategory(ucfpos, &status);
    assertSuccess(AAPE_MSG("_"), &status);
    assertIntEquals(AAPE_MSG("category"), category, _category);

    int32_t _field = ucfpos_getField(ucfpos, &status);
    assertSuccess(AAPE_MSG("field"), &status);
    assertIntEquals(AAPE_MSG("field"), field, _field);

    int32_t _start, _limit;
    ucfpos_getIndexes(ucfpos, &_start, &_limit, &status);
    assertSuccess(AAPE_MSG("indexes"), &status);
    assertIntEquals(AAPE_MSG("start"), start, _start);
    assertIntEquals(AAPE_MSG("limit"), limit, _limit);

    int64_t _context = ucfpos_getInt64IterationContext(ucfpos, &status);
    assertSuccess(AAPE_MSG("context"), &status);
    assertIntEquals(AAPE_MSG("context"), context, _context);

    UBool _matchesInteger = ucfpos_matchesField(ucfpos, UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, &status);
    assertSuccess(AAPE_MSG("integer field"), &status);
    assertTrue(AAPE_MSG("integer field"),
        ((matching & 1) != 0) ? _matchesInteger : !_matchesInteger);

    UBool _matchesCompact = ucfpos_matchesField(ucfpos, UFIELD_CATEGORY_NUMBER, UNUM_COMPACT_FIELD, &status);
    assertSuccess(AAPE_MSG("compact field"), &status);
    assertTrue(AAPE_MSG("compact field"),
        ((matching & 2) != 0) ? _matchesCompact : !_matchesCompact);

    UBool _matchesDate = ucfpos_matchesField(ucfpos, UFIELD_CATEGORY_DATE, UDAT_AM_PM_FIELD, &status);
    assertSuccess(AAPE_MSG("date field"), &status);
    assertTrue(AAPE_MSG("date field"),
        ((matching & 4) != 0) ? _matchesDate : !_matchesDate);
}


static void checkFormattedValueString(
        const char* message,
        const UFormattedValue* fv,
        const UChar* expectedString,
        UErrorCode* ec) {
    int32_t length;
    const UChar* actualString = ufmtval_getString(fv, &length, ec);
    if (U_FAILURE(*ec)) {
        assertIntEquals(message, 0, length);
        return;
    }
    assertSuccess(message, ec);
    // The string is guaranteed to be NUL-terminated.
    int32_t actualLength = u_strlen(actualString);
    assertIntEquals(message, actualLength, length);
    assertUEquals(message, expectedString, actualString);
}

// Declared in cformtst.h
void checkFormattedValue(
        const char* message,
        const UFormattedValue* fv,
        const UChar* expectedString,
        UFieldCategory expectedCategory,
        const UFieldPosition* expectedFieldPositions,
        int32_t expectedFieldPositionsLength) {
    UErrorCode ec = U_ZERO_ERROR;
    checkFormattedValueString(message, fv, expectedString, &ec);
    if (U_FAILURE(ec)) { return; }

    // Basic loop over the fields (more rigorous testing in C++)
    UConstrainedFieldPosition* ucfpos = ucfpos_open(&ec);
    int32_t i = 0;
    while (ufmtval_nextPosition(fv, ucfpos, &ec)) {
        assertIntEquals("category",
            expectedCategory, ucfpos_getCategory(ucfpos, &ec));
        assertIntEquals("field",
            expectedFieldPositions[i].field, ucfpos_getField(ucfpos, &ec));
        int32_t start, limit;
        ucfpos_getIndexes(ucfpos, &start, &limit, &ec);
        assertIntEquals("start",
            expectedFieldPositions[i].beginIndex, start);
        assertIntEquals("limit",
            expectedFieldPositions[i].endIndex, limit);
        i++;
    }
    assertTrue("After loop", !ufmtval_nextPosition(fv, ucfpos, &ec));
    assertSuccess("After loop", &ec);
    ucfpos_close(ucfpos);
}

void checkMixedFormattedValue(
        const char* message,
        const UFormattedValue* fv,
        const UChar* expectedString,
        const UFieldPositionWithCategory* expectedFieldPositions,
        int32_t length) {
    UErrorCode ec = U_ZERO_ERROR;
    checkFormattedValueString(message, fv, expectedString, &ec);
    if (U_FAILURE(ec)) { return; }

    // Basic loop over the fields (more rigorous testing in C++)
    UConstrainedFieldPosition* ucfpos = ucfpos_open(&ec);
    int32_t i = 0;
    while (ufmtval_nextPosition(fv, ucfpos, &ec)) {
        assertIntEquals("category",
            expectedFieldPositions[i].category, ucfpos_getCategory(ucfpos, &ec));
        assertIntEquals("field",
            expectedFieldPositions[i].field, ucfpos_getField(ucfpos, &ec));
        int32_t start, limit;
        ucfpos_getIndexes(ucfpos, &start, &limit, &ec);
        assertIntEquals("start",
            expectedFieldPositions[i].beginIndex, start);
        assertIntEquals("limit",
            expectedFieldPositions[i].endIndex, limit);
        i++;
    }
    assertTrue("After loop", !ufmtval_nextPosition(fv, ucfpos, &ec));
    assertSuccess("After loop", &ec);
    ucfpos_close(ucfpos);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
