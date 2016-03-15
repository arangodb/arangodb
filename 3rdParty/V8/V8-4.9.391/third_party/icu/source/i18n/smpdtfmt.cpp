/*
*******************************************************************************
* Copyright (C) 1997-2014, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File SMPDTFMT.CPP
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/31/97    aliu        Modified extensively to work with 50 locales.
*   04/01/97    aliu        Added support for centuries.
*   07/09/97    helena      Made ParsePosition into a class.
*   07/21/98    stephen     Added initializeDefaultCentury.
*                             Removed getZoneIndex (added in DateFormatSymbols)
*                             Removed subParseLong
*                             Removed chk
*  02/22/99     stephen     Removed character literals for EBCDIC safety
*   10/14/99    aliu        Updated 2-digit year parsing so that only "00" thru
*                           "99" are recognized. {j28 4182066}
*   11/15/99    weiv        Added support for week of year/day of week format
********************************************************************************
*/

#define ZID_KEY_MAX 128

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#include "unicode/smpdtfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/ures.h"
#include "unicode/msgfmt.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/timezone.h"
#include "unicode/decimfmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/uchar.h"
#include "unicode/uniset.h"
#include "unicode/ustring.h"
#include "unicode/basictz.h"
#include "unicode/simpletz.h"
#include "unicode/rbtz.h"
#include "unicode/tzfmt.h"
#include "unicode/utf16.h"
#include "unicode/vtzone.h"
#include "unicode/udisplaycontext.h"
#include "unicode/brkiter.h"
#include "olsontz.h"
#include "patternprops.h"
#include "fphdlimp.h"
#include "gregoimp.h"
#include "hebrwcal.h"
#include "cstring.h"
#include "uassert.h"
#include "cmemory.h"
#include "umutex.h"
#include <float.h>
#include "smpdtfst.h"

#if defined( U_DEBUG_CALSVC ) || defined (U_DEBUG_CAL)
#include <stdio.h>
#endif

// *****************************************************************************
// class SimpleDateFormat
// *****************************************************************************

U_NAMESPACE_BEGIN

static const UChar PATTERN_CHAR_BASE = 0x40;

/**
 * Last-resort string to use for "GMT" when constructing time zone strings.
 */
// For time zones that have no names, use strings GMT+minutes and
// GMT-minutes. For instance, in France the time zone is GMT+60.
// Also accepted are GMT+H:MM or GMT-H:MM.
// Currently not being used
//static const UChar gGmt[]      = {0x0047, 0x004D, 0x0054, 0x0000};         // "GMT"
//static const UChar gGmtPlus[]  = {0x0047, 0x004D, 0x0054, 0x002B, 0x0000}; // "GMT+"
//static const UChar gGmtMinus[] = {0x0047, 0x004D, 0x0054, 0x002D, 0x0000}; // "GMT-"
//static const UChar gDefGmtPat[]       = {0x0047, 0x004D, 0x0054, 0x007B, 0x0030, 0x007D, 0x0000}; /* GMT{0} */
//static const UChar gDefGmtNegHmsPat[] = {0x002D, 0x0048, 0x0048, 0x003A, 0x006D, 0x006D, 0x003A, 0x0073, 0x0073, 0x0000}; /* -HH:mm:ss */
//static const UChar gDefGmtNegHmPat[]  = {0x002D, 0x0048, 0x0048, 0x003A, 0x006D, 0x006D, 0x0000}; /* -HH:mm */
//static const UChar gDefGmtPosHmsPat[] = {0x002B, 0x0048, 0x0048, 0x003A, 0x006D, 0x006D, 0x003A, 0x0073, 0x0073, 0x0000}; /* +HH:mm:ss */
//static const UChar gDefGmtPosHmPat[]  = {0x002B, 0x0048, 0x0048, 0x003A, 0x006D, 0x006D, 0x0000}; /* +HH:mm */
//static const UChar gUt[]       = {0x0055, 0x0054, 0x0000};  // "UT"
//static const UChar gUtc[]      = {0x0055, 0x0054, 0x0043, 0x0000};  // "UT"

typedef enum GmtPatSize {
    kGmtLen = 3,
    kGmtPatLen = 6,
    kNegHmsLen = 9,
    kNegHmLen = 6,
    kPosHmsLen = 9,
    kPosHmLen = 6,
    kUtLen = 2,
    kUtcLen = 3
} GmtPatSize;

// Stuff needed for numbering system overrides

typedef enum OvrStrType {
    kOvrStrDate = 0,
    kOvrStrTime = 1,
    kOvrStrBoth = 2
} OvrStrType;

static const UDateFormatField kDateFields[] = {
    UDAT_YEAR_FIELD,
    UDAT_MONTH_FIELD,
    UDAT_DATE_FIELD,
    UDAT_DAY_OF_YEAR_FIELD,
    UDAT_DAY_OF_WEEK_IN_MONTH_FIELD,
    UDAT_WEEK_OF_YEAR_FIELD,
    UDAT_WEEK_OF_MONTH_FIELD,
    UDAT_YEAR_WOY_FIELD,
    UDAT_EXTENDED_YEAR_FIELD,
    UDAT_JULIAN_DAY_FIELD,
    UDAT_STANDALONE_DAY_FIELD,
    UDAT_STANDALONE_MONTH_FIELD,
    UDAT_QUARTER_FIELD,
    UDAT_STANDALONE_QUARTER_FIELD,
    UDAT_YEAR_NAME_FIELD,
    UDAT_RELATED_YEAR_FIELD };
static const int8_t kDateFieldsCount = 16;

static const UDateFormatField kTimeFields[] = {
    UDAT_HOUR_OF_DAY1_FIELD,
    UDAT_HOUR_OF_DAY0_FIELD,
    UDAT_MINUTE_FIELD,
    UDAT_SECOND_FIELD,
    UDAT_FRACTIONAL_SECOND_FIELD,
    UDAT_HOUR1_FIELD,
    UDAT_HOUR0_FIELD,
    UDAT_MILLISECONDS_IN_DAY_FIELD,
    UDAT_TIMEZONE_RFC_FIELD,
    UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD };
static const int8_t kTimeFieldsCount = 10;


// This is a pattern-of-last-resort used when we can't load a usable pattern out
// of a resource.
static const UChar gDefaultPattern[] =
{
    0x79, 0x79, 0x79, 0x79, 0x4D, 0x4D, 0x64, 0x64, 0x20, 0x68, 0x68, 0x3A, 0x6D, 0x6D, 0x20, 0x61, 0
};  /* "yyyyMMdd hh:mm a" */

// This prefix is designed to NEVER MATCH real text, in order to
// suppress the parsing of negative numbers.  Adjust as needed (if
// this becomes valid Unicode).
static const UChar SUPPRESS_NEGATIVE_PREFIX[] = {0xAB00, 0};

/**
 * These are the tags we expect to see in normal resource bundle files associated
 * with a locale.
 */
static const char gDateTimePatternsTag[]="DateTimePatterns";

//static const UChar gEtcUTC[] = {0x45, 0x74, 0x63, 0x2F, 0x55, 0x54, 0x43, 0x00}; // "Etc/UTC"
static const UChar QUOTE = 0x27; // Single quote

/*
 * The field range check bias for each UDateFormatField.
 * The bias is added to the minimum and maximum values
 * before they are compared to the parsed number.
 * For example, the calendar stores zero-based month numbers
 * but the parsed month numbers start at 1, so the bias is 1.
 *
 * A value of -1 means that the value is not checked.
 */
static const int32_t gFieldRangeBias[] = {
    -1,  // 'G' - UDAT_ERA_FIELD
    -1,  // 'y' - UDAT_YEAR_FIELD
     1,  // 'M' - UDAT_MONTH_FIELD
     0,  // 'd' - UDAT_DATE_FIELD
    -1,  // 'k' - UDAT_HOUR_OF_DAY1_FIELD
    -1,  // 'H' - UDAT_HOUR_OF_DAY0_FIELD
     0,  // 'm' - UDAT_MINUTE_FIELD
     0,  // 's' - UDAT_SEOND_FIELD
    -1,  // 'S' - UDAT_FRACTIONAL_SECOND_FIELD (0-999?)
    -1,  // 'E' - UDAT_DAY_OF_WEEK_FIELD (1-7?)
    -1,  // 'D' - UDAT_DAY_OF_YEAR_FIELD (1 - 366?)
    -1,  // 'F' - UDAT_DAY_OF_WEEK_IN_MONTH_FIELD (1-5?)
    -1,  // 'w' - UDAT_WEEK_OF_YEAR_FIELD (1-52?)
    -1,  // 'W' - UDAT_WEEK_OF_MONTH_FIELD (1-5?)
    -1,  // 'a' - UDAT_AM_PM_FIELD
    -1,  // 'h' - UDAT_HOUR1_FIELD
    -1,  // 'K' - UDAT_HOUR0_FIELD
    -1,  // 'z' - UDAT_TIMEZONE_FIELD
    -1,  // 'Y' - UDAT_YEAR_WOY_FIELD
    -1,  // 'e' - UDAT_DOW_LOCAL_FIELD
    -1,  // 'u' - UDAT_EXTENDED_YEAR_FIELD
    -1,  // 'g' - UDAT_JULIAN_DAY_FIELD
    -1,  // 'A' - UDAT_MILLISECONDS_IN_DAY_FIELD
    -1,  // 'Z' - UDAT_TIMEZONE_RFC_FIELD
    -1,  // 'v' - UDAT_TIMEZONE_GENERIC_FIELD
     0,  // 'c' - UDAT_STANDALONE_DAY_FIELD
     1,  // 'L' - UDAT_STANDALONE_MONTH_FIELD
    -1,  // 'Q' - UDAT_QUARTER_FIELD (1-4?)
    -1,  // 'q' - UDAT_STANDALONE_QUARTER_FIELD
    -1,  // 'V' - UDAT_TIMEZONE_SPECIAL_FIELD
    -1,  // 'U' - UDAT_YEAR_NAME_FIELD
    -1,  // 'O' - UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD
    -1,  // 'X' - UDAT_TIMEZONE_ISO_FIELD
    -1,  // 'x' - UDAT_TIMEZONE_ISO_LOCAL_FIELD
    -1,  // 'r' - UDAT_RELATED_YEAR_FIELD
};

// When calendar uses hebr numbering (i.e. he@calendar=hebrew),
// offset the years within the current millenium down to 1-999
static const int32_t HEBREW_CAL_CUR_MILLENIUM_START_YEAR = 5000;
static const int32_t HEBREW_CAL_CUR_MILLENIUM_END_YEAR = 6000;

static UMutex LOCK = U_MUTEX_INITIALIZER;

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SimpleDateFormat)

//----------------------------------------------------------------------

SimpleDateFormat::~SimpleDateFormat()
{
    delete fSymbols;
    if (fNumberFormatters) {
        uprv_free(fNumberFormatters);
    }
    if (fTimeZoneFormat) {
        delete fTimeZoneFormat;
    }

    while (fOverrideList) {
        NSOverride *cur = fOverrideList;
        fOverrideList = cur->next;
        delete cur->nf;
        uprv_free(cur);
    }

#if !UCONFIG_NO_BREAK_ITERATION
    delete fCapitalizationBrkIter;
#endif
}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(UErrorCode& status)
  :   fLocale(Locale::getDefault()),
      fSymbols(NULL),
      fTimeZoneFormat(NULL),
      fNumberFormatters(NULL),
      fOverrideList(NULL),
      fCapitalizationBrkIter(NULL)
{
    initializeBooleanAttributes();
    construct(kShort, (EStyle) (kShort + kDateOffset), fLocale, status);
    initializeDefaultCentury();
}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const UnicodeString& pattern,
                                   UErrorCode &status)
:   fPattern(pattern),
    fLocale(Locale::getDefault()),
    fSymbols(NULL),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{
    fDateOverride.setToBogus();
    fTimeOverride.setToBogus();
    initializeBooleanAttributes();
    initializeSymbols(fLocale, initializeCalendar(NULL,fLocale,status), status);
    initialize(fLocale, status);
    initializeDefaultCentury();

}
//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const UnicodeString& pattern,
                                   const UnicodeString& override,
                                   UErrorCode &status)
:   fPattern(pattern),
    fLocale(Locale::getDefault()),
    fSymbols(NULL),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{
    fDateOverride.setTo(override);
    fTimeOverride.setToBogus();
    initializeBooleanAttributes();
    initializeSymbols(fLocale, initializeCalendar(NULL,fLocale,status), status);
    initialize(fLocale, status);
    initializeDefaultCentury();

    processOverrideString(fLocale,override,kOvrStrBoth,status);

}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const UnicodeString& pattern,
                                   const Locale& locale,
                                   UErrorCode& status)
:   fPattern(pattern),
    fLocale(locale),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{

    fDateOverride.setToBogus();
    fTimeOverride.setToBogus();
    initializeBooleanAttributes();

    initializeSymbols(fLocale, initializeCalendar(NULL,fLocale,status), status);
    initialize(fLocale, status);
    initializeDefaultCentury();
}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const UnicodeString& pattern,
                                   const UnicodeString& override,
                                   const Locale& locale,
                                   UErrorCode& status)
:   fPattern(pattern),
    fLocale(locale),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{

    fDateOverride.setTo(override);
    fTimeOverride.setToBogus();
    initializeBooleanAttributes();

    initializeSymbols(fLocale, initializeCalendar(NULL,fLocale,status), status);
    initialize(fLocale, status);
    initializeDefaultCentury();

    processOverrideString(locale,override,kOvrStrBoth,status);

}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const UnicodeString& pattern,
                                   DateFormatSymbols* symbolsToAdopt,
                                   UErrorCode& status)
:   fPattern(pattern),
    fLocale(Locale::getDefault()),
    fSymbols(symbolsToAdopt),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{

    fDateOverride.setToBogus();
    fTimeOverride.setToBogus();
    initializeBooleanAttributes();

    initializeCalendar(NULL,fLocale,status);
    initialize(fLocale, status);
    initializeDefaultCentury();
}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const UnicodeString& pattern,
                                   const DateFormatSymbols& symbols,
                                   UErrorCode& status)
:   fPattern(pattern),
    fLocale(Locale::getDefault()),
    fSymbols(new DateFormatSymbols(symbols)),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{

    fDateOverride.setToBogus();
    fTimeOverride.setToBogus();
    initializeBooleanAttributes();

    initializeCalendar(NULL, fLocale, status);
    initialize(fLocale, status);
    initializeDefaultCentury();
}

//----------------------------------------------------------------------

// Not for public consumption; used by DateFormat
SimpleDateFormat::SimpleDateFormat(EStyle timeStyle,
                                   EStyle dateStyle,
                                   const Locale& locale,
                                   UErrorCode& status)
:   fLocale(locale),
    fSymbols(NULL),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{
    initializeBooleanAttributes();
    construct(timeStyle, dateStyle, fLocale, status);
    if(U_SUCCESS(status)) {
      initializeDefaultCentury();
    }
}

//----------------------------------------------------------------------

/**
 * Not for public consumption; used by DateFormat.  This constructor
 * never fails.  If the resource data is not available, it uses the
 * the last resort symbols.
 */
SimpleDateFormat::SimpleDateFormat(const Locale& locale,
                                   UErrorCode& status)
:   fPattern(gDefaultPattern),
    fLocale(locale),
    fSymbols(NULL),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{
    if (U_FAILURE(status)) return;
    initializeBooleanAttributes();
    initializeSymbols(fLocale, initializeCalendar(NULL, fLocale, status),status);
    if (U_FAILURE(status))
    {
        status = U_ZERO_ERROR;
        delete fSymbols;
        // This constructor doesn't fail; it uses last resort data
        fSymbols = new DateFormatSymbols(status);
        /* test for NULL */
        if (fSymbols == 0) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }

    fDateOverride.setToBogus();
    fTimeOverride.setToBogus();

    initialize(fLocale, status);
    if(U_SUCCESS(status)) {
      initializeDefaultCentury();
    }
}

//----------------------------------------------------------------------

SimpleDateFormat::SimpleDateFormat(const SimpleDateFormat& other)
:   DateFormat(other),
    fLocale(other.fLocale),
    fSymbols(NULL),
    fTimeZoneFormat(NULL),
    fNumberFormatters(NULL),
    fOverrideList(NULL),
    fCapitalizationBrkIter(NULL)
{
    initializeBooleanAttributes();
    *this = other;
}

//----------------------------------------------------------------------

SimpleDateFormat& SimpleDateFormat::operator=(const SimpleDateFormat& other)
{
    if (this == &other) {
        return *this;
    }
    DateFormat::operator=(other);

    delete fSymbols;
    fSymbols = NULL;

    if (other.fSymbols)
        fSymbols = new DateFormatSymbols(*other.fSymbols);

    fDefaultCenturyStart         = other.fDefaultCenturyStart;
    fDefaultCenturyStartYear     = other.fDefaultCenturyStartYear;
    fHaveDefaultCentury          = other.fHaveDefaultCentury;

    fPattern = other.fPattern;

    // TimeZoneFormat in ICU4C only depends on a locale for now
    if (fLocale != other.fLocale) {
        delete fTimeZoneFormat;
        fTimeZoneFormat = NULL; // forces lazy instantiation with the other locale
        fLocale = other.fLocale;
    }

#if !UCONFIG_NO_BREAK_ITERATION
    if (other.fCapitalizationBrkIter != NULL) {
        fCapitalizationBrkIter = (other.fCapitalizationBrkIter)->clone();
    }
#endif

    return *this;
}

//----------------------------------------------------------------------

Format*
SimpleDateFormat::clone() const
{
    return new SimpleDateFormat(*this);
}

//----------------------------------------------------------------------

UBool
SimpleDateFormat::operator==(const Format& other) const
{
    if (DateFormat::operator==(other)) {
        // The DateFormat::operator== check for fCapitalizationContext equality above
        //   is sufficient to check equality of all derived context-related data.
        // DateFormat::operator== guarantees following cast is safe
        SimpleDateFormat* that = (SimpleDateFormat*)&other;
        return (fPattern             == that->fPattern &&
                fSymbols             != NULL && // Check for pathological object
                that->fSymbols       != NULL && // Check for pathological object
                *fSymbols            == *that->fSymbols &&
                fHaveDefaultCentury  == that->fHaveDefaultCentury &&
                fDefaultCenturyStart == that->fDefaultCenturyStart);
    }
    return FALSE;
}

//----------------------------------------------------------------------

void SimpleDateFormat::construct(EStyle timeStyle,
                                 EStyle dateStyle,
                                 const Locale& locale,
                                 UErrorCode& status)
{
    // called by several constructors to load pattern data from the resources
    if (U_FAILURE(status)) return;

    // We will need the calendar to know what type of symbols to load.
    initializeCalendar(NULL, locale, status);
    if (U_FAILURE(status)) return;

    CalendarData calData(locale, fCalendar?fCalendar->getType():NULL, status);
    UResourceBundle *dateTimePatterns = calData.getByKey(gDateTimePatternsTag, status);
    UResourceBundle *currentBundle;

    if (U_FAILURE(status)) return;

    if (ures_getSize(dateTimePatterns) <= kDateTime)
    {
        status = U_INVALID_FORMAT_ERROR;
        return;
    }

    setLocaleIDs(ures_getLocaleByType(dateTimePatterns, ULOC_VALID_LOCALE, &status),
                 ures_getLocaleByType(dateTimePatterns, ULOC_ACTUAL_LOCALE, &status));

    // create a symbols object from the locale
    initializeSymbols(locale,fCalendar, status);
    if (U_FAILURE(status)) return;
    /* test for NULL */
    if (fSymbols == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    const UChar *resStr,*ovrStr;
    int32_t resStrLen,ovrStrLen = 0;
    fDateOverride.setToBogus();
    fTimeOverride.setToBogus();

    // if the pattern should include both date and time information, use the date/time
    // pattern string as a guide to tell use how to glue together the appropriate date
    // and time pattern strings.  The actual gluing-together is handled by a convenience
    // method on MessageFormat.
    if ((timeStyle != kNone) && (dateStyle != kNone))
    {
        Formattable timeDateArray[2];

        // use Formattable::adoptString() so that we can use fastCopyFrom()
        // instead of Formattable::setString()'s unaware, safe, deep string clone
        // see Jitterbug 2296

        currentBundle = ures_getByIndex(dateTimePatterns, (int32_t)timeStyle, NULL, &status);
        if (U_FAILURE(status)) {
           status = U_INVALID_FORMAT_ERROR;
           return;
        }
        switch (ures_getType(currentBundle)) {
            case URES_STRING: {
               resStr = ures_getString(currentBundle, &resStrLen, &status);
               break;
            }
            case URES_ARRAY: {
               resStr = ures_getStringByIndex(currentBundle, 0, &resStrLen, &status);
               ovrStr = ures_getStringByIndex(currentBundle, 1, &ovrStrLen, &status);
               fTimeOverride.setTo(TRUE, ovrStr, ovrStrLen);
               break;
            }
            default: {
               status = U_INVALID_FORMAT_ERROR;
               ures_close(currentBundle);
               return;
            }
        }
        ures_close(currentBundle);

        UnicodeString *tempus1 = new UnicodeString(TRUE, resStr, resStrLen);
        // NULL pointer check
        if (tempus1 == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        timeDateArray[0].adoptString(tempus1);

        currentBundle = ures_getByIndex(dateTimePatterns, (int32_t)dateStyle, NULL, &status);
        if (U_FAILURE(status)) {
           status = U_INVALID_FORMAT_ERROR;
           return;
        }
        switch (ures_getType(currentBundle)) {
            case URES_STRING: {
               resStr = ures_getString(currentBundle, &resStrLen, &status);
               break;
            }
            case URES_ARRAY: {
               resStr = ures_getStringByIndex(currentBundle, 0, &resStrLen, &status);
               ovrStr = ures_getStringByIndex(currentBundle, 1, &ovrStrLen, &status);
               fDateOverride.setTo(TRUE, ovrStr, ovrStrLen);
               break;
            }
            default: {
               status = U_INVALID_FORMAT_ERROR;
               ures_close(currentBundle);
               return;
            }
        }
        ures_close(currentBundle);

        UnicodeString *tempus2 = new UnicodeString(TRUE, resStr, resStrLen);
        // Null pointer check
        if (tempus2 == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        timeDateArray[1].adoptString(tempus2);

        int32_t glueIndex = kDateTime;
        int32_t patternsSize = ures_getSize(dateTimePatterns);
        if (patternsSize >= (kDateTimeOffset + kShort + 1)) {
            // Get proper date time format
            glueIndex = (int32_t)(kDateTimeOffset + (dateStyle - kDateOffset));
        }

        resStr = ures_getStringByIndex(dateTimePatterns, glueIndex, &resStrLen, &status);
        MessageFormat::format(UnicodeString(TRUE, resStr, resStrLen), timeDateArray, 2, fPattern, status);
    }
    // if the pattern includes just time data or just date date, load the appropriate
    // pattern string from the resources
    // setTo() - see DateFormatSymbols::assignArray comments
    else if (timeStyle != kNone) {
        currentBundle = ures_getByIndex(dateTimePatterns, (int32_t)timeStyle, NULL, &status);
        if (U_FAILURE(status)) {
           status = U_INVALID_FORMAT_ERROR;
           return;
        }
        switch (ures_getType(currentBundle)) {
            case URES_STRING: {
               resStr = ures_getString(currentBundle, &resStrLen, &status);
               break;
            }
            case URES_ARRAY: {
               resStr = ures_getStringByIndex(currentBundle, 0, &resStrLen, &status);
               ovrStr = ures_getStringByIndex(currentBundle, 1, &ovrStrLen, &status);
               fDateOverride.setTo(TRUE, ovrStr, ovrStrLen);
               break;
            }
            default: {
               status = U_INVALID_FORMAT_ERROR;
                ures_close(currentBundle);
               return;
            }
        }
        fPattern.setTo(TRUE, resStr, resStrLen);
        ures_close(currentBundle);
    }
    else if (dateStyle != kNone) {
        currentBundle = ures_getByIndex(dateTimePatterns, (int32_t)dateStyle, NULL, &status);
        if (U_FAILURE(status)) {
           status = U_INVALID_FORMAT_ERROR;
           return;
        }
        switch (ures_getType(currentBundle)) {
            case URES_STRING: {
               resStr = ures_getString(currentBundle, &resStrLen, &status);
               break;
            }
            case URES_ARRAY: {
               resStr = ures_getStringByIndex(currentBundle, 0, &resStrLen, &status);
               ovrStr = ures_getStringByIndex(currentBundle, 1, &ovrStrLen, &status);
               fDateOverride.setTo(TRUE, ovrStr, ovrStrLen);
               break;
            }
            default: {
               status = U_INVALID_FORMAT_ERROR;
               ures_close(currentBundle);
               return;
            }
        }
        fPattern.setTo(TRUE, resStr, resStrLen);
        ures_close(currentBundle);
    }

    // and if it includes _neither_, that's an error
    else
        status = U_INVALID_FORMAT_ERROR;

    // finally, finish initializing by creating a Calendar and a NumberFormat
    initialize(locale, status);
}

//----------------------------------------------------------------------

Calendar*
SimpleDateFormat::initializeCalendar(TimeZone* adoptZone, const Locale& locale, UErrorCode& status)
{
    if(!U_FAILURE(status)) {
        fCalendar = Calendar::createInstance(adoptZone?adoptZone:TimeZone::createDefault(), locale, status);
    }
    if (U_SUCCESS(status) && fCalendar == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return fCalendar;
}

void
SimpleDateFormat::initializeSymbols(const Locale& locale, Calendar* calendar, UErrorCode& status)
{
  if(U_FAILURE(status)) {
    fSymbols = NULL;
  } else {
    // pass in calendar type - use NULL (default) if no calendar set (or err).
    fSymbols = new DateFormatSymbols(locale, calendar?calendar->getType() :NULL , status);
    // Null pointer check
    if (fSymbols == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
  }
}

void
SimpleDateFormat::initialize(const Locale& locale,
                             UErrorCode& status)
{
    if (U_FAILURE(status)) return;

    // We don't need to check that the row count is >= 1, since all 2d arrays have at
    // least one row
    fNumberFormat = NumberFormat::createInstance(locale, status);
    if (fNumberFormat != NULL && U_SUCCESS(status))
    {
        // no matter what the locale's default number format looked like, we want
        // to modify it so that it doesn't use thousands separators, doesn't always
        // show the decimal point, and recognizes integers only when parsing

        fNumberFormat->setGroupingUsed(FALSE);
        DecimalFormat* decfmt = dynamic_cast<DecimalFormat*>(fNumberFormat);
        if (decfmt != NULL) {
            decfmt->setDecimalSeparatorAlwaysShown(FALSE);
        }
        fNumberFormat->setParseIntegerOnly(TRUE);
        fNumberFormat->setMinimumFractionDigits(0); // To prevent "Jan 1.00, 1997.00"

        //fNumberFormat->setLenient(TRUE); // Java uses a custom DateNumberFormat to format/parse

        initNumberFormatters(locale,status);

    }
    else if (U_SUCCESS(status))
    {
        status = U_MISSING_RESOURCE_ERROR;
    }
}

/* Initialize the fields we use to disambiguate ambiguous years. Separate
 * so we can call it from readObject().
 */
void SimpleDateFormat::initializeDefaultCentury()
{
  if(fCalendar) {
    fHaveDefaultCentury = fCalendar->haveDefaultCentury();
    if(fHaveDefaultCentury) {
      fDefaultCenturyStart = fCalendar->defaultCenturyStart();
      fDefaultCenturyStartYear = fCalendar->defaultCenturyStartYear();
    } else {
      fDefaultCenturyStart = DBL_MIN;
      fDefaultCenturyStartYear = -1;
    }
  }
}

/*
 * Initialize the boolean attributes. Separate so we can call it from all constructors.
 */
void SimpleDateFormat::initializeBooleanAttributes()
{
    UErrorCode status = U_ZERO_ERROR;

    setBooleanAttribute(UDAT_PARSE_ALLOW_WHITESPACE, true, status);
    setBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, true, status);
    setBooleanAttribute(UDAT_PARSE_PARTIAL_MATCH, true, status);
    setBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, true, status);
}

/* Define one-century window into which to disambiguate dates using
 * two-digit years. Make public in JDK 1.2.
 */
void SimpleDateFormat::parseAmbiguousDatesAsAfter(UDate startDate, UErrorCode& status)
{
    if(U_FAILURE(status)) {
        return;
    }
    if(!fCalendar) {
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return;
    }

    fCalendar->setTime(startDate, status);
    if(U_SUCCESS(status)) {
        fHaveDefaultCentury = TRUE;
        fDefaultCenturyStart = startDate;
        fDefaultCenturyStartYear = fCalendar->get(UCAL_YEAR, status);
    }
}

//----------------------------------------------------------------------

UnicodeString&
SimpleDateFormat::format(Calendar& cal, UnicodeString& appendTo, FieldPosition& pos) const
{
  UErrorCode status = U_ZERO_ERROR;
  FieldPositionOnlyHandler handler(pos);
  return _format(cal, appendTo, handler, status);
}

//----------------------------------------------------------------------

UnicodeString&
SimpleDateFormat::format(Calendar& cal, UnicodeString& appendTo,
                         FieldPositionIterator* posIter, UErrorCode& status) const
{
  FieldPositionIteratorHandler handler(posIter, status);
  return _format(cal, appendTo, handler, status);
}

//----------------------------------------------------------------------

UnicodeString&
SimpleDateFormat::_format(Calendar& cal, UnicodeString& appendTo,
                            FieldPositionHandler& handler, UErrorCode& status) const
{
    if ( U_FAILURE(status) ) {
       return appendTo; 
    }
    Calendar* workCal = &cal;
    Calendar* calClone = NULL;
    if (&cal != fCalendar && uprv_strcmp(cal.getType(), fCalendar->getType()) != 0) {
        // Different calendar type
        // We use the time and time zone from the input calendar, but
        // do not use the input calendar for field calculation.
        calClone = fCalendar->clone();
        if (calClone != NULL) {
            UDate t = cal.getTime(status);
            calClone->setTime(t, status);
            calClone->setTimeZone(cal.getTimeZone());
            workCal = calClone;
        } else {
            status = U_MEMORY_ALLOCATION_ERROR;
            return appendTo;
        }
    }

    UBool inQuote = FALSE;
    UChar prevCh = 0;
    int32_t count = 0;
    int32_t fieldNum = 0;
    UDisplayContext capitalizationContext = getContext(UDISPCTX_TYPE_CAPITALIZATION, status);

    // loop through the pattern string character by character
    for (int32_t i = 0; i < fPattern.length() && U_SUCCESS(status); ++i) {
        UChar ch = fPattern[i];

        // Use subFormat() to format a repeated pattern character
        // when a different pattern or non-pattern character is seen
        if (ch != prevCh && count > 0) {
            subFormat(appendTo, prevCh, count, capitalizationContext, fieldNum++, handler, *workCal, status);
            count = 0;
        }
        if (ch == QUOTE) {
            // Consecutive single quotes are a single quote literal,
            // either outside of quotes or between quotes
            if ((i+1) < fPattern.length() && fPattern[i+1] == QUOTE) {
                appendTo += (UChar)QUOTE;
                ++i;
            } else {
                inQuote = ! inQuote;
            }
        }
        else if ( ! inQuote && ((ch >= 0x0061 /*'a'*/ && ch <= 0x007A /*'z'*/)
                    || (ch >= 0x0041 /*'A'*/ && ch <= 0x005A /*'Z'*/))) {
            // ch is a date-time pattern character to be interpreted
            // by subFormat(); count the number of times it is repeated
            prevCh = ch;
            ++count;
        }
        else {
            // Append quoted characters and unquoted non-pattern characters
            appendTo += ch;
        }
    }

    // Format the last item in the pattern, if any
    if (count > 0) {
        subFormat(appendTo, prevCh, count, capitalizationContext, fieldNum++, handler, *workCal, status);
    }

    if (calClone != NULL) {
        delete calClone;
    }

    return appendTo;
}

//----------------------------------------------------------------------

/* Map calendar field into calendar field level.
 * the larger the level, the smaller the field unit.
 * For example, UCAL_ERA level is 0, UCAL_YEAR level is 10,
 * UCAL_MONTH level is 20.
 * NOTE: if new fields adds in, the table needs to update.
 */
const int32_t
SimpleDateFormat::fgCalendarFieldToLevel[] =
{
    /*GyM*/ 0, 10, 20,
    /*wW*/ 20, 30,
    /*dDEF*/ 30, 20, 30, 30,
    /*ahHm*/ 40, 50, 50, 60,
    /*sS*/ 70, 80,
    /*z?Y*/ 0, 0, 10,
    /*eug*/ 30, 10, 0,
    /*A?.*/ 40, 0, 0
};


/* Map calendar field LETTER into calendar field level.
 * the larger the level, the smaller the field unit.
 * NOTE: if new fields adds in, the table needs to update.
 */
const int32_t
SimpleDateFormat::fgPatternCharToLevel[] = {
    //       A   B   C   D   E   F   G   H   I   J   K   L   M   N   O
        -1, 40, -1, -1, 20, 30, 30,  0, 50, -1, -1, 50, 20, 20, -1,  0,
    //   P   Q   R   S   T   U   V   W   X   Y   Z
        -1, 20, -1, 80, -1, 10,  0, 30,  0, 10,  0, -1, -1, -1, -1, -1,
    //       a   b   c   d   e   f   g   h   i   j   k   l   m   n   o
        -1, 40, -1, 30, 30, 30, -1,  0, 50, -1, -1, 50, -1, 60, -1, -1,
    //   p   q   r   s   t   u   v   w   x   y   z
        -1, 20, 10, 70, -1, 10,  0, 20,  0, 10,  0, -1, -1, -1, -1, -1
};


// Map index into pattern character string to Calendar field number.
const UCalendarDateFields
SimpleDateFormat::fgPatternIndexToCalendarField[] =
{
    /*GyM*/ UCAL_ERA, UCAL_YEAR, UCAL_MONTH,
    /*dkH*/ UCAL_DATE, UCAL_HOUR_OF_DAY, UCAL_HOUR_OF_DAY,
    /*msS*/ UCAL_MINUTE, UCAL_SECOND, UCAL_MILLISECOND,
    /*EDF*/ UCAL_DAY_OF_WEEK, UCAL_DAY_OF_YEAR, UCAL_DAY_OF_WEEK_IN_MONTH,
    /*wWa*/ UCAL_WEEK_OF_YEAR, UCAL_WEEK_OF_MONTH, UCAL_AM_PM,
    /*hKz*/ UCAL_HOUR, UCAL_HOUR, UCAL_ZONE_OFFSET,
    /*Yeu*/ UCAL_YEAR_WOY, UCAL_DOW_LOCAL, UCAL_EXTENDED_YEAR,
    /*gAZ*/ UCAL_JULIAN_DAY, UCAL_MILLISECONDS_IN_DAY, UCAL_ZONE_OFFSET,
    /*v*/   UCAL_ZONE_OFFSET,
    /*c*/   UCAL_DOW_LOCAL,
    /*L*/   UCAL_MONTH,
    /*Q*/   UCAL_MONTH,
    /*q*/   UCAL_MONTH,
    /*V*/   UCAL_ZONE_OFFSET,
    /*U*/   UCAL_YEAR,
    /*O*/   UCAL_ZONE_OFFSET,
    /*Xx*/  UCAL_ZONE_OFFSET, UCAL_ZONE_OFFSET,
    /*r*/   UCAL_EXTENDED_YEAR,
};

// Map index into pattern character string to DateFormat field number
const UDateFormatField
SimpleDateFormat::fgPatternIndexToDateFormatField[] = {
    /*GyM*/ UDAT_ERA_FIELD, UDAT_YEAR_FIELD, UDAT_MONTH_FIELD,
    /*dkH*/ UDAT_DATE_FIELD, UDAT_HOUR_OF_DAY1_FIELD, UDAT_HOUR_OF_DAY0_FIELD,
    /*msS*/ UDAT_MINUTE_FIELD, UDAT_SECOND_FIELD, UDAT_FRACTIONAL_SECOND_FIELD,
    /*EDF*/ UDAT_DAY_OF_WEEK_FIELD, UDAT_DAY_OF_YEAR_FIELD, UDAT_DAY_OF_WEEK_IN_MONTH_FIELD,
    /*wWa*/ UDAT_WEEK_OF_YEAR_FIELD, UDAT_WEEK_OF_MONTH_FIELD, UDAT_AM_PM_FIELD,
    /*hKz*/ UDAT_HOUR1_FIELD, UDAT_HOUR0_FIELD, UDAT_TIMEZONE_FIELD,
    /*Yeu*/ UDAT_YEAR_WOY_FIELD, UDAT_DOW_LOCAL_FIELD, UDAT_EXTENDED_YEAR_FIELD,
    /*gAZ*/ UDAT_JULIAN_DAY_FIELD, UDAT_MILLISECONDS_IN_DAY_FIELD, UDAT_TIMEZONE_RFC_FIELD,
    /*v*/   UDAT_TIMEZONE_GENERIC_FIELD,
    /*c*/   UDAT_STANDALONE_DAY_FIELD,
    /*L*/   UDAT_STANDALONE_MONTH_FIELD,
    /*Q*/   UDAT_QUARTER_FIELD,
    /*q*/   UDAT_STANDALONE_QUARTER_FIELD,
    /*V*/   UDAT_TIMEZONE_SPECIAL_FIELD,
    /*U*/   UDAT_YEAR_NAME_FIELD,
    /*O*/   UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD,
    /*Xx*/  UDAT_TIMEZONE_ISO_FIELD, UDAT_TIMEZONE_ISO_LOCAL_FIELD,
    /*r*/   UDAT_RELATED_YEAR_FIELD,
};

//----------------------------------------------------------------------

/**
 * Append symbols[value] to dst.  Make sure the array index is not out
 * of bounds.
 */
static inline void
_appendSymbol(UnicodeString& dst,
              int32_t value,
              const UnicodeString* symbols,
              int32_t symbolsCount) {
    U_ASSERT(0 <= value && value < symbolsCount);
    if (0 <= value && value < symbolsCount) {
        dst += symbols[value];
    }
}

static inline void
_appendSymbolWithMonthPattern(UnicodeString& dst, int32_t value, const UnicodeString* symbols, int32_t symbolsCount,
              const UnicodeString* monthPattern, UErrorCode& status) {
    U_ASSERT(0 <= value && value < symbolsCount);
    if (0 <= value && value < symbolsCount) {
        if (monthPattern == NULL) {
            dst += symbols[value];
        } else {
            Formattable monthName((const UnicodeString&)(symbols[value]));
            MessageFormat::format(*monthPattern, &monthName, 1, dst, status);
        }
    }
}

//----------------------------------------------------------------------
void
SimpleDateFormat::initNumberFormatters(const Locale &locale,UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if ( fDateOverride.isBogus() && fTimeOverride.isBogus() ) {
        return;
    }
    umtx_lock(&LOCK);
    if (fNumberFormatters == NULL) {
        fNumberFormatters = (NumberFormat**)uprv_malloc(UDAT_FIELD_COUNT * sizeof(NumberFormat*));
        if (fNumberFormatters) {
            for (int32_t i = 0; i < UDAT_FIELD_COUNT; i++) {
                fNumberFormatters[i] = fNumberFormat;
            }
        } else {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    umtx_unlock(&LOCK);

    if (U_FAILURE(status)) {
        return;
    }

    processOverrideString(locale,fDateOverride,kOvrStrDate,status);
    processOverrideString(locale,fTimeOverride,kOvrStrTime,status);
}

void
SimpleDateFormat::processOverrideString(const Locale &locale, const UnicodeString &str, int8_t type, UErrorCode &status) {
    if (str.isBogus() || U_FAILURE(status)) {
        return;
    }

    U_ASSERT(fNumberFormatters != NULL);

    int32_t start = 0;
    int32_t len;
    UnicodeString nsName;
    UnicodeString ovrField;
    UBool moreToProcess = TRUE;

    while (moreToProcess) {
        int32_t delimiterPosition = str.indexOf((UChar)ULOC_KEYWORD_ITEM_SEPARATOR_UNICODE,start);
        if (delimiterPosition == -1) {
            moreToProcess = FALSE;
            len = str.length() - start;
        } else {
            len = delimiterPosition - start;
        }
        UnicodeString currentString(str,start,len);
        int32_t equalSignPosition = currentString.indexOf((UChar)ULOC_KEYWORD_ASSIGN_UNICODE,0);
        if (equalSignPosition == -1) { // Simple override string such as "hebrew"
            nsName.setTo(currentString);
            ovrField.setToBogus();
        } else { // Field specific override string such as "y=hebrew"
            nsName.setTo(currentString,equalSignPosition+1);
            ovrField.setTo(currentString,0,1); // We just need the first character.
        }

        int32_t nsNameHash = nsName.hashCode();
        // See if the numbering system is in the override list, if not, then add it.
        NSOverride *cur = fOverrideList;
        NumberFormat *nf = NULL;
        UBool found = FALSE;
        while ( cur && !found ) {
            if ( cur->hash == nsNameHash ) {
                nf = cur->nf;
                found = TRUE;
            }
            cur = cur->next;
        }

        if (!found) {
           cur = (NSOverride *)uprv_malloc(sizeof(NSOverride));
           if (cur) {
               char kw[ULOC_KEYWORD_AND_VALUES_CAPACITY];
               uprv_strcpy(kw,"numbers=");
               nsName.extract(0,len,kw+8,ULOC_KEYWORD_AND_VALUES_CAPACITY-8,US_INV);

               Locale ovrLoc(locale.getLanguage(),locale.getCountry(),locale.getVariant(),kw);
               nf = NumberFormat::createInstance(ovrLoc,status);

               // no matter what the locale's default number format looked like, we want
               // to modify it so that it doesn't use thousands separators, doesn't always
               // show the decimal point, and recognizes integers only when parsing

               if (U_SUCCESS(status)) {
                   nf->setGroupingUsed(FALSE);
                   DecimalFormat* decfmt = dynamic_cast<DecimalFormat*>(nf);
                   if (decfmt != NULL) {
                       decfmt->setDecimalSeparatorAlwaysShown(FALSE);
                   }
                   nf->setParseIntegerOnly(TRUE);
                   nf->setMinimumFractionDigits(0); // To prevent "Jan 1.00, 1997.00"

                   cur->nf = nf;
                   cur->hash = nsNameHash;
                   cur->next = fOverrideList;
                   fOverrideList = cur;
               }
               else {
                   // clean up before returning
                   if (cur != NULL) {
                       uprv_free(cur);
                   }
                  return;
               }

           } else {
               status = U_MEMORY_ALLOCATION_ERROR;
               return;
           }
        }

        // Now that we have an appropriate number formatter, fill in the appropriate spaces in the
        // number formatters table.
        if (ovrField.isBogus()) {
            switch (type) {
                case kOvrStrDate:
                case kOvrStrBoth: {
                    for ( int8_t i=0 ; i<kDateFieldsCount; i++ ) {
                        fNumberFormatters[kDateFields[i]] = nf;
                    }
                    if (type==kOvrStrDate) {
                        break;
                    }
                }
                case kOvrStrTime : {
                    for ( int8_t i=0 ; i<kTimeFieldsCount; i++ ) {
                        fNumberFormatters[kTimeFields[i]] = nf;
                    }
                    break;
                }
            }
        } else {
           // if the pattern character is unrecognized, signal an error and bail out
           UDateFormatField patternCharIndex =
              DateFormatSymbols::getPatternCharIndex(ovrField.charAt(0));
           if (patternCharIndex == UDAT_FIELD_COUNT) {
               status = U_INVALID_FORMAT_ERROR;
               return;
           }

           // Set the number formatter in the table
           fNumberFormatters[patternCharIndex] = nf;
        }

        start = delimiterPosition + 1;
    }
}

//---------------------------------------------------------------------
void
SimpleDateFormat::subFormat(UnicodeString &appendTo,
                            UChar ch,
                            int32_t count,
                            UDisplayContext capitalizationContext,
                            int32_t fieldNum,
                            FieldPositionHandler& handler,
                            Calendar& cal,
                            UErrorCode& status) const
{
    if (U_FAILURE(status)) {
        return;
    }

    // this function gets called by format() to produce the appropriate substitution
    // text for an individual pattern symbol (e.g., "HH" or "yyyy")

    UDateFormatField patternCharIndex = DateFormatSymbols::getPatternCharIndex(ch);
    const int32_t maxIntCount = 10;
    int32_t beginOffset = appendTo.length();
    NumberFormat *currentNumberFormat;
    DateFormatSymbols::ECapitalizationContextUsageType capContextUsageType = DateFormatSymbols::kCapContextUsageOther;

    UBool isHebrewCalendar = (uprv_strcmp(cal.getType(),"hebrew") == 0);
    UBool isChineseCalendar = (uprv_strcmp(cal.getType(),"chinese") == 0 || uprv_strcmp(cal.getType(),"dangi") == 0);

    // if the pattern character is unrecognized, signal an error and dump out
    if (patternCharIndex == UDAT_FIELD_COUNT)
    {
        if (ch != 0x6C) { // pattern char 'l' (SMALL LETTER L) just gets ignored
            status = U_INVALID_FORMAT_ERROR;
        }
        return;
    }

    UCalendarDateFields field = fgPatternIndexToCalendarField[patternCharIndex];
    int32_t value = (patternCharIndex != UDAT_RELATED_YEAR_FIELD)? cal.get(field, status): cal.getRelatedYear(status);
    if (U_FAILURE(status)) {
        return;
    }

    currentNumberFormat = getNumberFormatByIndex(patternCharIndex);
    UnicodeString hebr("hebr", 4, US_INV);
    
    switch (patternCharIndex) {

    // for any "G" symbol, write out the appropriate era string
    // "GGGG" is wide era name, "GGGGG" is narrow era name, anything else is abbreviated name
    case UDAT_ERA_FIELD:
        if (isChineseCalendar) {
            zeroPaddingNumber(currentNumberFormat,appendTo, value, 1, 9); // as in ICU4J
        } else {
            if (count == 5) {
                _appendSymbol(appendTo, value, fSymbols->fNarrowEras, fSymbols->fNarrowErasCount);
                capContextUsageType = DateFormatSymbols::kCapContextUsageEraNarrow;
            } else if (count == 4) {
                _appendSymbol(appendTo, value, fSymbols->fEraNames, fSymbols->fEraNamesCount);
                capContextUsageType = DateFormatSymbols::kCapContextUsageEraWide;
            } else {
                _appendSymbol(appendTo, value, fSymbols->fEras, fSymbols->fErasCount);
                capContextUsageType = DateFormatSymbols::kCapContextUsageEraAbbrev;
            }
        }
        break;

     case UDAT_YEAR_NAME_FIELD:
        if (fSymbols->fShortYearNames != NULL && value <= fSymbols->fShortYearNamesCount) {
            // the Calendar YEAR field runs 1 through 60 for cyclic years
            _appendSymbol(appendTo, value - 1, fSymbols->fShortYearNames, fSymbols->fShortYearNamesCount);
            break;
        }
        // else fall through to numeric year handling, do not break here

   // OLD: for "yyyy", write out the whole year; for "yy", write out the last 2 digits
    // NEW: UTS#35:
//Year         y     yy     yyy     yyyy     yyyyy
//AD 1         1     01     001     0001     00001
//AD 12       12     12     012     0012     00012
//AD 123     123     23     123     0123     00123
//AD 1234   1234     34    1234     1234     01234
//AD 12345 12345     45   12345    12345     12345
    case UDAT_YEAR_FIELD:
    case UDAT_YEAR_WOY_FIELD:
        if (fDateOverride.compare(hebr)==0 && value>HEBREW_CAL_CUR_MILLENIUM_START_YEAR && value<HEBREW_CAL_CUR_MILLENIUM_END_YEAR) {
            value-=HEBREW_CAL_CUR_MILLENIUM_START_YEAR;
        }
        if(count == 2)
            zeroPaddingNumber(currentNumberFormat, appendTo, value, 2, 2);
        else
            zeroPaddingNumber(currentNumberFormat, appendTo, value, count, maxIntCount);
        break;

    // for "MMMM"/"LLLL", write out the whole month name, for "MMM"/"LLL", write out the month
    // abbreviation, for "M"/"L" or "MM"/"LL", write out the month as a number with the
    // appropriate number of digits
    // for "MMMMM"/"LLLLL", use the narrow form
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
        if ( isHebrewCalendar ) {
           HebrewCalendar *hc = (HebrewCalendar*)&cal;
           if (hc->isLeapYear(hc->get(UCAL_YEAR,status)) && value == 6 && count >= 3 )
               value = 13; // Show alternate form for Adar II in leap years in Hebrew calendar.
           if (!hc->isLeapYear(hc->get(UCAL_YEAR,status)) && value >= 6 && count < 3 )
               value--; // Adjust the month number down 1 in Hebrew non-leap years, i.e. Adar is 6, not 7.
        }
        {
            int32_t isLeapMonth = (fSymbols->fLeapMonthPatterns != NULL && fSymbols->fLeapMonthPatternsCount >= DateFormatSymbols::kMonthPatternsCount)?
                        cal.get(UCAL_IS_LEAP_MONTH, status): 0;
            // should consolidate the next section by using arrays of pointers & counts for the right symbols...
            if (count == 5) {
                if (patternCharIndex == UDAT_MONTH_FIELD) {
                    _appendSymbolWithMonthPattern(appendTo, value, fSymbols->fNarrowMonths, fSymbols->fNarrowMonthsCount,
                            (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternFormatNarrow]): NULL, status);
                } else {
                    _appendSymbolWithMonthPattern(appendTo, value, fSymbols->fStandaloneNarrowMonths, fSymbols->fStandaloneNarrowMonthsCount,
                            (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternStandaloneNarrow]): NULL, status);
                }
                capContextUsageType = DateFormatSymbols::kCapContextUsageMonthNarrow;
            } else if (count == 4) {
                if (patternCharIndex == UDAT_MONTH_FIELD) {
                    _appendSymbolWithMonthPattern(appendTo, value, fSymbols->fMonths, fSymbols->fMonthsCount,
                            (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternFormatWide]): NULL, status);
                    capContextUsageType = DateFormatSymbols::kCapContextUsageMonthFormat;
                } else {
                    _appendSymbolWithMonthPattern(appendTo, value, fSymbols->fStandaloneMonths, fSymbols->fStandaloneMonthsCount,
                            (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternStandaloneWide]): NULL, status);
                    capContextUsageType = DateFormatSymbols::kCapContextUsageMonthStandalone;
                }
            } else if (count == 3) {
                if (patternCharIndex == UDAT_MONTH_FIELD) {
                    _appendSymbolWithMonthPattern(appendTo, value, fSymbols->fShortMonths, fSymbols->fShortMonthsCount,
                            (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternFormatAbbrev]): NULL, status);
                    capContextUsageType = DateFormatSymbols::kCapContextUsageMonthFormat;
                } else {
                    _appendSymbolWithMonthPattern(appendTo, value, fSymbols->fStandaloneShortMonths, fSymbols->fStandaloneShortMonthsCount,
                            (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternStandaloneAbbrev]): NULL, status);
                    capContextUsageType = DateFormatSymbols::kCapContextUsageMonthStandalone;
                }
            } else {
                UnicodeString monthNumber;
                zeroPaddingNumber(currentNumberFormat,monthNumber, value + 1, count, maxIntCount);
                _appendSymbolWithMonthPattern(appendTo, 0, &monthNumber, 1,
                        (isLeapMonth!=0)? &(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternNumeric]): NULL, status);
            }
        }
        break;

    // for "k" and "kk", write out the hour, adjusting midnight to appear as "24"
    case UDAT_HOUR_OF_DAY1_FIELD:
        if (value == 0)
            zeroPaddingNumber(currentNumberFormat,appendTo, cal.getMaximum(UCAL_HOUR_OF_DAY) + 1, count, maxIntCount);
        else
            zeroPaddingNumber(currentNumberFormat,appendTo, value, count, maxIntCount);
        break;

    case UDAT_FRACTIONAL_SECOND_FIELD:
        // Fractional seconds left-justify
        {
            currentNumberFormat->setMinimumIntegerDigits((count > 3) ? 3 : count);
            currentNumberFormat->setMaximumIntegerDigits(maxIntCount);
            if (count == 1) {
                value /= 100;
            } else if (count == 2) {
                value /= 10;
            }
            FieldPosition p(0);
            currentNumberFormat->format(value, appendTo, p);
            if (count > 3) {
                currentNumberFormat->setMinimumIntegerDigits(count - 3);
                currentNumberFormat->format((int32_t)0, appendTo, p);
            }
        }
        break;

    // for "ee" or "e", use local numeric day-of-the-week
    // for "EEEEEE" or "eeeeee", write out the short day-of-the-week name
    // for "EEEEE" or "eeeee", write out the narrow day-of-the-week name
    // for "EEEE" or "eeee", write out the wide day-of-the-week name
    // for "EEE" or "EE" or "E" or "eee", write out the abbreviated day-of-the-week name
    case UDAT_DOW_LOCAL_FIELD:
        if ( count < 3 ) {
            zeroPaddingNumber(currentNumberFormat,appendTo, value, count, maxIntCount);
            break;
        }
        // fall through to EEEEE-EEE handling, but for that we don't want local day-of-week,
        // we want standard day-of-week, so first fix value to work for EEEEE-EEE.
        value = cal.get(UCAL_DAY_OF_WEEK, status);
        if (U_FAILURE(status)) {
            return;
        }
        // fall through, do not break here
    case UDAT_DAY_OF_WEEK_FIELD:
        if (count == 5) {
            _appendSymbol(appendTo, value, fSymbols->fNarrowWeekdays,
                          fSymbols->fNarrowWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayNarrow;
        } else if (count == 4) {
            _appendSymbol(appendTo, value, fSymbols->fWeekdays,
                          fSymbols->fWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayFormat;
        } else if (count == 6) {
            _appendSymbol(appendTo, value, fSymbols->fShorterWeekdays,
                          fSymbols->fShorterWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayFormat;
        } else {
            _appendSymbol(appendTo, value, fSymbols->fShortWeekdays,
                          fSymbols->fShortWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayFormat;
        }
        break;

    // for "ccc", write out the abbreviated day-of-the-week name
    // for "cccc", write out the wide day-of-the-week name
    // for "ccccc", use the narrow day-of-the-week name
    // for "ccccc", use the short day-of-the-week name
    case UDAT_STANDALONE_DAY_FIELD:
        if ( count < 3 ) {
            zeroPaddingNumber(currentNumberFormat,appendTo, value, 1, maxIntCount);
            break;
        }
        // fall through to alpha DOW handling, but for that we don't want local day-of-week,
        // we want standard day-of-week, so first fix value.
        value = cal.get(UCAL_DAY_OF_WEEK, status);
        if (U_FAILURE(status)) {
            return;
        }
        if (count == 5) {
            _appendSymbol(appendTo, value, fSymbols->fStandaloneNarrowWeekdays,
                          fSymbols->fStandaloneNarrowWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayNarrow;
        } else if (count == 4) {
            _appendSymbol(appendTo, value, fSymbols->fStandaloneWeekdays,
                          fSymbols->fStandaloneWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayStandalone;
        } else if (count == 6) {
            _appendSymbol(appendTo, value, fSymbols->fStandaloneShorterWeekdays,
                          fSymbols->fStandaloneShorterWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayStandalone;
        } else { // count == 3
            _appendSymbol(appendTo, value, fSymbols->fStandaloneShortWeekdays,
                          fSymbols->fStandaloneShortWeekdaysCount);
            capContextUsageType = DateFormatSymbols::kCapContextUsageDayStandalone;
        }
        break;

    // for and "a" symbol, write out the whole AM/PM string
    case UDAT_AM_PM_FIELD:
        _appendSymbol(appendTo, value, fSymbols->fAmPms,
                      fSymbols->fAmPmsCount);
        break;

    // for "h" and "hh", write out the hour, adjusting noon and midnight to show up
    // as "12"
    case UDAT_HOUR1_FIELD:
        if (value == 0)
            zeroPaddingNumber(currentNumberFormat,appendTo, cal.getLeastMaximum(UCAL_HOUR) + 1, count, maxIntCount);
        else
            zeroPaddingNumber(currentNumberFormat,appendTo, value, count, maxIntCount);
        break;

    case UDAT_TIMEZONE_FIELD: // 'z'
    case UDAT_TIMEZONE_RFC_FIELD: // 'Z'
    case UDAT_TIMEZONE_GENERIC_FIELD: // 'v'
    case UDAT_TIMEZONE_SPECIAL_FIELD: // 'V'
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD: // 'O'
    case UDAT_TIMEZONE_ISO_FIELD: // 'X'
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD: // 'x'
        {
            UnicodeString zoneString;
            const TimeZone& tz = cal.getTimeZone();
            UDate date = cal.getTime(status);
            if (U_SUCCESS(status)) {
                if (patternCharIndex == UDAT_TIMEZONE_FIELD) {
                    if (count < 4) {
                        // "z", "zz", "zzz"
                        tzFormat()->format(UTZFMT_STYLE_SPECIFIC_SHORT, tz, date, zoneString);
                        capContextUsageType = DateFormatSymbols::kCapContextUsageMetazoneShort;
                    } else {
                        // "zzzz" or longer
                        tzFormat()->format(UTZFMT_STYLE_SPECIFIC_LONG, tz, date, zoneString);
                        capContextUsageType = DateFormatSymbols::kCapContextUsageMetazoneLong;
                    }
                }
                else if (patternCharIndex == UDAT_TIMEZONE_RFC_FIELD) {
                    if (count < 4) {
                        // "Z"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL, tz, date, zoneString);
                    } else if (count == 5) {
                        // "ZZZZZ"
                        tzFormat()->format(UTZFMT_STYLE_ISO_EXTENDED_FULL, tz, date, zoneString);
                    } else {
                        // "ZZ", "ZZZ", "ZZZZ"
                        tzFormat()->format(UTZFMT_STYLE_LOCALIZED_GMT, tz, date, zoneString);
                    }
                }
                else if (patternCharIndex == UDAT_TIMEZONE_GENERIC_FIELD) {
                    if (count == 1) {
                        // "v"
                        tzFormat()->format(UTZFMT_STYLE_GENERIC_SHORT, tz, date, zoneString);
                        capContextUsageType = DateFormatSymbols::kCapContextUsageMetazoneShort;
                    } else if (count == 4) {
                        // "vvvv"
                        tzFormat()->format(UTZFMT_STYLE_GENERIC_LONG, tz, date, zoneString);
                        capContextUsageType = DateFormatSymbols::kCapContextUsageMetazoneLong;
                    }
                }
                else if (patternCharIndex == UDAT_TIMEZONE_SPECIAL_FIELD) {
                    if (count == 1) {
                        // "V"
                        tzFormat()->format(UTZFMT_STYLE_ZONE_ID_SHORT, tz, date, zoneString);
                    } else if (count == 2) {
                        // "VV"
                        tzFormat()->format(UTZFMT_STYLE_ZONE_ID, tz, date, zoneString);
                    } else if (count == 3) {
                        // "VVV"
                        tzFormat()->format(UTZFMT_STYLE_EXEMPLAR_LOCATION, tz, date, zoneString);
                    } else if (count == 4) {
                        // "VVVV"
                        tzFormat()->format(UTZFMT_STYLE_GENERIC_LOCATION, tz, date, zoneString);
                        capContextUsageType = DateFormatSymbols::kCapContextUsageZoneLong;
                    }
                }
                else if (patternCharIndex == UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD) {
                    if (count == 1) {
                        // "O"
                        tzFormat()->format(UTZFMT_STYLE_LOCALIZED_GMT_SHORT, tz, date, zoneString);
                    } else if (count == 4) {
                        // "OOOO"
                        tzFormat()->format(UTZFMT_STYLE_LOCALIZED_GMT, tz, date, zoneString);
                    }
                }
                else if (patternCharIndex == UDAT_TIMEZONE_ISO_FIELD) {
                    if (count == 1) {
                        // "X"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_SHORT, tz, date, zoneString);
                    } else if (count == 2) {
                        // "XX"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_FIXED, tz, date, zoneString);
                    } else if (count == 3) {
                        // "XXX"
                        tzFormat()->format(UTZFMT_STYLE_ISO_EXTENDED_FIXED, tz, date, zoneString);
                    } else if (count == 4) {
                        // "XXXX"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_FULL, tz, date, zoneString);
                    } else if (count == 5) {
                        // "XXXXX"
                        tzFormat()->format(UTZFMT_STYLE_ISO_EXTENDED_FULL, tz, date, zoneString);
                    }
                }
                else if (patternCharIndex == UDAT_TIMEZONE_ISO_LOCAL_FIELD) {
                    if (count == 1) {
                        // "x"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_LOCAL_SHORT, tz, date, zoneString);
                    } else if (count == 2) {
                        // "xx"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_LOCAL_FIXED, tz, date, zoneString);
                    } else if (count == 3) {
                        // "xxx"
                        tzFormat()->format(UTZFMT_STYLE_ISO_EXTENDED_LOCAL_FIXED, tz, date, zoneString);
                    } else if (count == 4) {
                        // "xxxx"
                        tzFormat()->format(UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL, tz, date, zoneString);
                    } else if (count == 5) {
                        // "xxxxx"
                        tzFormat()->format(UTZFMT_STYLE_ISO_EXTENDED_LOCAL_FULL, tz, date, zoneString);
                    }
                }
                else {
                    U_ASSERT(FALSE);
                }
            }
            appendTo += zoneString;
        }
        break;

    case UDAT_QUARTER_FIELD:
        if (count >= 4)
            _appendSymbol(appendTo, value/3, fSymbols->fQuarters,
                          fSymbols->fQuartersCount);
        else if (count == 3)
            _appendSymbol(appendTo, value/3, fSymbols->fShortQuarters,
                          fSymbols->fShortQuartersCount);
        else
            zeroPaddingNumber(currentNumberFormat,appendTo, (value/3) + 1, count, maxIntCount);
        break;

    case UDAT_STANDALONE_QUARTER_FIELD:
        if (count >= 4)
            _appendSymbol(appendTo, value/3, fSymbols->fStandaloneQuarters,
                          fSymbols->fStandaloneQuartersCount);
        else if (count == 3)
            _appendSymbol(appendTo, value/3, fSymbols->fStandaloneShortQuarters,
                          fSymbols->fStandaloneShortQuartersCount);
        else
            zeroPaddingNumber(currentNumberFormat,appendTo, (value/3) + 1, count, maxIntCount);
        break;


    // all of the other pattern symbols can be formatted as simple numbers with
    // appropriate zero padding
    default:
        zeroPaddingNumber(currentNumberFormat,appendTo, value, count, maxIntCount);
        break;
    }
#if !UCONFIG_NO_BREAK_ITERATION
    // if first field, check to see whether we need to and are able to titlecase it
    if (fieldNum == 0 && u_islower(appendTo.char32At(beginOffset)) && fCapitalizationBrkIter != NULL) {
        UBool titlecase = FALSE;
        switch (capitalizationContext) {
            case UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE:
                titlecase = TRUE;
                break;
            case UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU:
                titlecase = fSymbols->fCapitalization[capContextUsageType][0];
                break;
            case UDISPCTX_CAPITALIZATION_FOR_STANDALONE:
                titlecase = fSymbols->fCapitalization[capContextUsageType][1];
                break;
            default:
                // titlecase = FALSE;
                break;
        }
        if (titlecase) {
            UnicodeString firstField(appendTo, beginOffset);
            firstField.toTitle(fCapitalizationBrkIter, fLocale, U_TITLECASE_NO_LOWERCASE | U_TITLECASE_NO_BREAK_ADJUSTMENT);
            appendTo.replaceBetween(beginOffset, appendTo.length(), firstField);
        }
    }
#endif

    handler.addAttribute(fgPatternIndexToDateFormatField[patternCharIndex], beginOffset, appendTo.length());
}

//----------------------------------------------------------------------

void SimpleDateFormat::adoptNumberFormat(NumberFormat *formatToAdopt) {
    formatToAdopt->setParseIntegerOnly(TRUE);
    if (fNumberFormat && fNumberFormat != formatToAdopt){
        delete fNumberFormat;
    }
    fNumberFormat = formatToAdopt;

    if (fNumberFormatters) {
        for (int32_t i = 0; i < UDAT_FIELD_COUNT; i++) {
            if (fNumberFormatters[i] == formatToAdopt) {
                fNumberFormatters[i] = NULL;
            }
        }
        uprv_free(fNumberFormatters);
        fNumberFormatters = NULL;
    }
    
    while (fOverrideList) {
        NSOverride *cur = fOverrideList;
        fOverrideList = cur->next;
        if (cur->nf != formatToAdopt) { // only delete those not duplicate
            delete cur->nf;
            uprv_free(cur);
        } else {
            cur->nf = NULL;
            uprv_free(cur);
        }
    }
}

void SimpleDateFormat::adoptNumberFormat(const UnicodeString& fields, NumberFormat *formatToAdopt, UErrorCode &status){
    // if it has not been initialized yet, initialize
    if (fNumberFormatters == NULL) {
        fNumberFormatters = (NumberFormat**)uprv_malloc(UDAT_FIELD_COUNT * sizeof(NumberFormat*));
        if (fNumberFormatters) {
            for (int32_t i = 0; i < UDAT_FIELD_COUNT; i++) {
                fNumberFormatters[i] = fNumberFormat;
            }
        } else {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
    
    // See if the numbering format is in the override list, if not, then add it.
    NSOverride *cur = fOverrideList;
    UBool found = FALSE;
    while (cur && !found) {
        if ( cur->nf == formatToAdopt ) {
            found = TRUE;
        }
        cur = cur->next;
    }

    if (!found) {
        cur = (NSOverride *)uprv_malloc(sizeof(NSOverride));
        if (cur) {
            // no matter what the locale's default number format looked like, we want
            // to modify it so that it doesn't use thousands separators, doesn't always
            // show the decimal point, and recognizes integers only when parsing
            formatToAdopt->setGroupingUsed(FALSE);
            DecimalFormat* decfmt = dynamic_cast<DecimalFormat*>(formatToAdopt);
            if (decfmt != NULL) {
                decfmt->setDecimalSeparatorAlwaysShown(FALSE);
            }
            formatToAdopt->setParseIntegerOnly(TRUE);
            formatToAdopt->setMinimumFractionDigits(0); // To prevent "Jan 1.00, 1997.00"

            cur->nf = formatToAdopt;
            cur->hash = -1; // set duplicate here (before we set it with NumberSystem Hash, here we cannot get nor use it)
            cur->next = fOverrideList;
            fOverrideList = cur;
        } else {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
    
    for (int i=0; i<fields.length(); i++) {
        UChar field = fields.charAt(i);
        // if the pattern character is unrecognized, signal an error and bail out
        UDateFormatField patternCharIndex = DateFormatSymbols::getPatternCharIndex(field);
        if (patternCharIndex == UDAT_FIELD_COUNT) {
            status = U_INVALID_FORMAT_ERROR;
            return;
        }

        // Set the number formatter in the table
        fNumberFormatters[patternCharIndex] = formatToAdopt;
    }
}

const NumberFormat *
SimpleDateFormat::getNumberFormatForField(UChar field) const {
    UDateFormatField index = DateFormatSymbols::getPatternCharIndex(field);
    return getNumberFormatByIndex(index);
}

NumberFormat *
SimpleDateFormat::getNumberFormatByIndex(UDateFormatField index) const {
    if (fNumberFormatters != NULL) {
        return fNumberFormatters[index];
    } else {
        return fNumberFormat;
    }
}

//----------------------------------------------------------------------
void
SimpleDateFormat::zeroPaddingNumber(NumberFormat *currentNumberFormat,UnicodeString &appendTo,
                                    int32_t value, int32_t minDigits, int32_t maxDigits) const
{
    if (currentNumberFormat!=NULL) {
        FieldPosition pos(0);

        currentNumberFormat->setMinimumIntegerDigits(minDigits);
        currentNumberFormat->setMaximumIntegerDigits(maxDigits);
        currentNumberFormat->format(value, appendTo, pos);  // 3rd arg is there to speed up processing
    }
}

//----------------------------------------------------------------------

/**
 * Return true if the given format character, occuring count
 * times, represents a numeric field.
 */
UBool SimpleDateFormat::isNumeric(UChar formatChar, int32_t count) {
    return DateFormatSymbols::isNumericPatternChar(formatChar, count);
}

UBool
SimpleDateFormat::isAtNumericField(const UnicodeString &pattern, int32_t patternOffset) {
    if (patternOffset >= pattern.length()) {
        // not at any field
        return FALSE;
    }
    UChar ch = pattern.charAt(patternOffset);
    UDateFormatField f = DateFormatSymbols::getPatternCharIndex(ch);
    if (f == UDAT_FIELD_COUNT) {
        // not at any field
        return FALSE;
    }
    int32_t i = patternOffset;
    while (pattern.charAt(++i) == ch) {}
    return DateFormatSymbols::isNumericField(f, i - patternOffset);
}

UBool
SimpleDateFormat::isAfterNonNumericField(const UnicodeString &pattern, int32_t patternOffset) {
    if (patternOffset <= 0) {
        // not after any field
        return FALSE;
    }
    UChar ch = pattern.charAt(--patternOffset);
    UDateFormatField f = DateFormatSymbols::getPatternCharIndex(ch);
    if (f == UDAT_FIELD_COUNT) {
        // not after any field
        return FALSE;
    }
    int32_t i = patternOffset;
    while (pattern.charAt(--i) == ch) {}
    return !DateFormatSymbols::isNumericField(f, patternOffset - i);
}

void
SimpleDateFormat::parse(const UnicodeString& text, Calendar& cal, ParsePosition& parsePos) const
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t pos = parsePos.getIndex();
    if(parsePos.getIndex() < 0) {
        parsePos.setErrorIndex(0);
        return;
    }
    int32_t start = pos;


    UBool ambiguousYear[] = { FALSE };
    int32_t saveHebrewMonth = -1;
    int32_t count = 0;
    UTimeZoneFormatTimeType tzTimeType = UTZFMT_TIME_TYPE_UNKNOWN;

    // For parsing abutting numeric fields. 'abutPat' is the
    // offset into 'pattern' of the first of 2 or more abutting
    // numeric fields.  'abutStart' is the offset into 'text'
    // where parsing the fields begins. 'abutPass' starts off as 0
    // and increments each time we try to parse the fields.
    int32_t abutPat = -1; // If >=0, we are in a run of abutting numeric fields
    int32_t abutStart = 0;
    int32_t abutPass = 0;
    UBool inQuote = FALSE;

    MessageFormat * numericLeapMonthFormatter = NULL;

    Calendar* calClone = NULL;
    Calendar *workCal = &cal;
    if (&cal != fCalendar && uprv_strcmp(cal.getType(), fCalendar->getType()) != 0) {
        // Different calendar type
        // We use the time/zone from the input calendar, but
        // do not use the input calendar for field calculation.
        calClone = fCalendar->clone();
        if (calClone != NULL) {
            calClone->setTime(cal.getTime(status),status);
            if (U_FAILURE(status)) {
                goto ExitParse;
            }
            calClone->setTimeZone(cal.getTimeZone());
            workCal = calClone;
        } else {
            status = U_MEMORY_ALLOCATION_ERROR;
            goto ExitParse;
        }
    }
    
    if (fSymbols->fLeapMonthPatterns != NULL && fSymbols->fLeapMonthPatternsCount >= DateFormatSymbols::kMonthPatternsCount) {
        numericLeapMonthFormatter = new MessageFormat(fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternNumeric], fLocale, status);
        if (numericLeapMonthFormatter == NULL) {
             status = U_MEMORY_ALLOCATION_ERROR;
             goto ExitParse;
        } else if (U_FAILURE(status)) {
             goto ExitParse; // this will delete numericLeapMonthFormatter
        }
    }

    for (int32_t i=0; i<fPattern.length(); ++i) {
        UChar ch = fPattern.charAt(i);

        // Handle alphabetic field characters.
        if (!inQuote && ((ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A))) { // [A-Za-z]
            int32_t fieldPat = i;

            // Count the length of this field specifier
            count = 1;
            while ((i+1)<fPattern.length() &&
                   fPattern.charAt(i+1) == ch) {
                ++count;
                ++i;
            }

            if (isNumeric(ch, count)) {
                if (abutPat < 0) {
                    // Determine if there is an abutting numeric field.
                    // Record the start of a set of abutting numeric fields.
                    if (isAtNumericField(fPattern, i + 1)) {
                        abutPat = fieldPat;
                        abutStart = pos;
                        abutPass = 0;
                    }
                }
            } else {
                abutPat = -1; // End of any abutting fields
            }

            // Handle fields within a run of abutting numeric fields.  Take
            // the pattern "HHmmss" as an example. We will try to parse
            // 2/2/2 characters of the input text, then if that fails,
            // 1/2/2.  We only adjust the width of the leftmost field; the
            // others remain fixed.  This allows "123456" => 12:34:56, but
            // "12345" => 1:23:45.  Likewise, for the pattern "yyyyMMdd" we
            // try 4/2/2, 3/2/2, 2/2/2, and finally 1/2/2.
            if (abutPat >= 0) {
                // If we are at the start of a run of abutting fields, then
                // shorten this field in each pass.  If we can't shorten
                // this field any more, then the parse of this set of
                // abutting numeric fields has failed.
                if (fieldPat == abutPat) {
                    count -= abutPass++;
                    if (count == 0) {
                        status = U_PARSE_ERROR;
                        goto ExitParse;
                    }
                }

                pos = subParse(text, pos, ch, count,
                               TRUE, FALSE, ambiguousYear, saveHebrewMonth, *workCal, i, numericLeapMonthFormatter, &tzTimeType);

                // If the parse fails anywhere in the run, back up to the
                // start of the run and retry.
                if (pos < 0) {
                    i = abutPat - 1;
                    pos = abutStart;
                    continue;
                }
            }

            // Handle non-numeric fields and non-abutting numeric
            // fields.
            else if (ch != 0x6C) { // pattern char 'l' (SMALL LETTER L) just gets ignored
                int32_t s = subParse(text, pos, ch, count,
                               FALSE, TRUE, ambiguousYear, saveHebrewMonth, *workCal, i, numericLeapMonthFormatter, &tzTimeType);

                if (s == -pos-1) {
                    // era not present, in special cases allow this to continue
                    // from the position where the era was expected
                    s = pos;

                    if (i+1 < fPattern.length()) {
                        // move to next pattern character
                        UChar ch = fPattern.charAt(i+1);

                        // check for whitespace
                        if (PatternProps::isWhiteSpace(ch)) {
                            i++;
                            // Advance over run in pattern
                            while ((i+1)<fPattern.length() &&
                                   PatternProps::isWhiteSpace(fPattern.charAt(i+1))) {
                                ++i;
                            }
                        }
                    }
                }
                else if (s <= 0) {
                    status = U_PARSE_ERROR;
                    goto ExitParse;
                }
                pos = s;
            }
        }

        // Handle literal pattern characters.  These are any
        // quoted characters and non-alphabetic unquoted
        // characters.
        else {

            abutPat = -1; // End of any abutting fields
            
            if (! matchLiterals(fPattern, i, text, pos, getBooleanAttribute(UDAT_PARSE_ALLOW_WHITESPACE, status), getBooleanAttribute(UDAT_PARSE_PARTIAL_MATCH, status), isLenient())) {
                status = U_PARSE_ERROR;
                goto ExitParse;
            }
        }
    }

    // Special hack for trailing "." after non-numeric field.
    if (text.charAt(pos) == 0x2e && getBooleanAttribute(UDAT_PARSE_ALLOW_WHITESPACE, status)) {
        // only do if the last field is not numeric
        if (isAfterNonNumericField(fPattern, fPattern.length())) {
            pos++; // skip the extra "."
        }
    }

    // At this point the fields of Calendar have been set.  Calendar
    // will fill in default values for missing fields when the time
    // is computed.

    parsePos.setIndex(pos);

    // This part is a problem:  When we call parsedDate.after, we compute the time.
    // Take the date April 3 2004 at 2:30 am.  When this is first set up, the year
    // will be wrong if we're parsing a 2-digit year pattern.  It will be 1904.
    // April 3 1904 is a Sunday (unlike 2004) so it is the DST onset day.  2:30 am
    // is therefore an "impossible" time, since the time goes from 1:59 to 3:00 am
    // on that day.  It is therefore parsed out to fields as 3:30 am.  Then we
    // add 100 years, and get April 3 2004 at 3:30 am.  Note that April 3 2004 is
    // a Saturday, so it can have a 2:30 am -- and it should. [LIU]
    /*
        UDate parsedDate = calendar.getTime();
        if( ambiguousYear[0] && !parsedDate.after(fDefaultCenturyStart) ) {
            calendar.add(Calendar.YEAR, 100);
            parsedDate = calendar.getTime();
        }
    */
    // Because of the above condition, save off the fields in case we need to readjust.
    // The procedure we use here is not particularly efficient, but there is no other
    // way to do this given the API restrictions present in Calendar.  We minimize
    // inefficiency by only performing this computation when it might apply, that is,
    // when the two-digit year is equal to the start year, and thus might fall at the
    // front or the back of the default century.  This only works because we adjust
    // the year correctly to start with in other cases -- see subParse().
    if (ambiguousYear[0] || tzTimeType != UTZFMT_TIME_TYPE_UNKNOWN) // If this is true then the two-digit year == the default start year
    {
        // We need a copy of the fields, and we need to avoid triggering a call to
        // complete(), which will recalculate the fields.  Since we can't access
        // the fields[] array in Calendar, we clone the entire object.  This will
        // stop working if Calendar.clone() is ever rewritten to call complete().
        Calendar *copy;
        if (ambiguousYear[0]) {
            copy = cal.clone();
            // Check for failed cloning.
            if (copy == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                goto ExitParse;
            }
            UDate parsedDate = copy->getTime(status);
            // {sfb} check internalGetDefaultCenturyStart
            if (fHaveDefaultCentury && (parsedDate < fDefaultCenturyStart)) {
                // We can't use add here because that does a complete() first.
                cal.set(UCAL_YEAR, fDefaultCenturyStartYear + 100);
            }
            delete copy;
        }

        if (tzTimeType != UTZFMT_TIME_TYPE_UNKNOWN) {
            copy = cal.clone();
            // Check for failed cloning.
            if (copy == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                goto ExitParse;
            }
            const TimeZone & tz = cal.getTimeZone();
            BasicTimeZone *btz = NULL;

            if (dynamic_cast<const OlsonTimeZone *>(&tz) != NULL
                || dynamic_cast<const SimpleTimeZone *>(&tz) != NULL
                || dynamic_cast<const RuleBasedTimeZone *>(&tz) != NULL
                || dynamic_cast<const VTimeZone *>(&tz) != NULL) {
                btz = (BasicTimeZone*)&tz;
            }

            // Get local millis
            copy->set(UCAL_ZONE_OFFSET, 0);
            copy->set(UCAL_DST_OFFSET, 0);
            UDate localMillis = copy->getTime(status);

            // Make sure parsed time zone type (Standard or Daylight)
            // matches the rule used by the parsed time zone.
            int32_t raw, dst;
            if (btz != NULL) {
                if (tzTimeType == UTZFMT_TIME_TYPE_STANDARD) {
                    btz->getOffsetFromLocal(localMillis,
                        BasicTimeZone::kStandard, BasicTimeZone::kStandard, raw, dst, status);
                } else {
                    btz->getOffsetFromLocal(localMillis,
                        BasicTimeZone::kDaylight, BasicTimeZone::kDaylight, raw, dst, status);
                }
            } else {
                // No good way to resolve ambiguous time at transition,
                // but following code work in most case.
                tz.getOffset(localMillis, TRUE, raw, dst, status);
            }

            // Now, compare the results with parsed type, either standard or daylight saving time
            int32_t resolvedSavings = dst;
            if (tzTimeType == UTZFMT_TIME_TYPE_STANDARD) {
                if (dst != 0) {
                    // Override DST_OFFSET = 0 in the result calendar
                    resolvedSavings = 0;
                }
            } else { // tztype == TZTYPE_DST
                if (dst == 0) {
                    if (btz != NULL) {
                        UDate time = localMillis + raw;
                        // We use the nearest daylight saving time rule.
                        TimeZoneTransition beforeTrs, afterTrs;
                        UDate beforeT = time, afterT = time;
                        int32_t beforeSav = 0, afterSav = 0;
                        UBool beforeTrsAvail, afterTrsAvail;

                        // Search for DST rule before or on the time
                        while (TRUE) {
                            beforeTrsAvail = btz->getPreviousTransition(beforeT, TRUE, beforeTrs);
                            if (!beforeTrsAvail) {
                                break;
                            }
                            beforeT = beforeTrs.getTime() - 1;
                            beforeSav = beforeTrs.getFrom()->getDSTSavings();
                            if (beforeSav != 0) {
                                break;
                            }
                        }

                        // Search for DST rule after the time
                        while (TRUE) {
                            afterTrsAvail = btz->getNextTransition(afterT, FALSE, afterTrs);
                            if (!afterTrsAvail) {
                                break;
                            }
                            afterT = afterTrs.getTime();
                            afterSav = afterTrs.getTo()->getDSTSavings();
                            if (afterSav != 0) {
                                break;
                            }
                        }

                        if (beforeTrsAvail && afterTrsAvail) {
                            if (time - beforeT > afterT - time) {
                                resolvedSavings = afterSav;
                            } else {
                                resolvedSavings = beforeSav;
                            }
                        } else if (beforeTrsAvail && beforeSav != 0) {
                            resolvedSavings = beforeSav;
                        } else if (afterTrsAvail && afterSav != 0) {
                            resolvedSavings = afterSav;
                        } else {
                            resolvedSavings = btz->getDSTSavings();
                        }
                    } else {
                        resolvedSavings = tz.getDSTSavings();
                    }
                    if (resolvedSavings == 0) {
                        // final fallback
                        resolvedSavings = U_MILLIS_PER_HOUR;
                    }
                }
            }
            cal.set(UCAL_ZONE_OFFSET, raw);
            cal.set(UCAL_DST_OFFSET, resolvedSavings);
            delete copy;
        }
    }
ExitParse:
    // Set the parsed result if local calendar is used
    // instead of the input calendar
    if (U_SUCCESS(status) && workCal != &cal) {
        cal.setTimeZone(workCal->getTimeZone());
        cal.setTime(workCal->getTime(status), status);
    }

    if (numericLeapMonthFormatter != NULL) {
        delete numericLeapMonthFormatter;
    }
    if (calClone != NULL) {
        delete calClone;
    }

    // If any Calendar calls failed, we pretend that we
    // couldn't parse the string, when in reality this isn't quite accurate--
    // we did parse it; the Calendar calls just failed.
    if (U_FAILURE(status)) {
        parsePos.setErrorIndex(pos);
        parsePos.setIndex(start);
    }
}

//----------------------------------------------------------------------

static UBool
newBestMatchWithOptionalDot(const UnicodeString &lcaseText,
                            const UnicodeString &data,
                            UnicodeString &bestMatchName,
                            int32_t &bestMatchLength);

int32_t SimpleDateFormat::matchQuarterString(const UnicodeString& text,
                              int32_t start,
                              UCalendarDateFields field,
                              const UnicodeString* data,
                              int32_t dataCount,
                              Calendar& cal) const
{
    int32_t i = 0;
    int32_t count = dataCount;

    // There may be multiple strings in the data[] array which begin with
    // the same prefix (e.g., Cerven and Cervenec (June and July) in Czech).
    // We keep track of the longest match, and return that.  Note that this
    // unfortunately requires us to test all array elements.
    int32_t bestMatchLength = 0, bestMatch = -1;
    UnicodeString bestMatchName;

    // {sfb} kludge to support case-insensitive comparison
    // {markus 2002oct11} do not just use caseCompareBetween because we do not know
    // the length of the match after case folding
    // {alan 20040607} don't case change the whole string, since the length
    // can change
    // TODO we need a case-insensitive startsWith function
    UnicodeString lcaseText;
    text.extract(start, INT32_MAX, lcaseText);
    lcaseText.foldCase();

    for (; i < count; ++i)
    {
        // Always compare if we have no match yet; otherwise only compare
        // against potentially better matches (longer strings).

        if (newBestMatchWithOptionalDot(lcaseText, data[i], bestMatchName, bestMatchLength)) {
            bestMatch = i;
        }
    }
    if (bestMatch >= 0)
    {
        cal.set(field, bestMatch * 3);

        // Once we have a match, we have to determine the length of the
        // original source string.  This will usually be == the length of
        // the case folded string, but it may differ (e.g. sharp s).

        // Most of the time, the length will be the same as the length
        // of the string from the locale data.  Sometimes it will be
        // different, in which case we will have to figure it out by
        // adding a character at a time, until we have a match.  We do
        // this all in one loop, where we try 'len' first (at index
        // i==0).
        int32_t len = bestMatchName.length(); // 99+% of the time
        int32_t n = text.length() - start;
        for (i=0; i<=n; ++i) {
            int32_t j=i;
            if (i == 0) {
                j = len;
            } else if (i == len) {
                continue; // already tried this when i was 0
            }
            text.extract(start, j, lcaseText);
            lcaseText.foldCase();
            if (bestMatchName == lcaseText) {
                return start + j;
            }
        }
    }

    return -start;
}

//----------------------------------------------------------------------
UBool SimpleDateFormat::matchLiterals(const UnicodeString &pattern,
                                      int32_t &patternOffset,
                                      const UnicodeString &text,
                                      int32_t &textOffset,
                                      UBool whitespaceLenient,
                                      UBool partialMatchLenient,
                                      UBool oldLeniency)
{
    UBool inQuote = FALSE;
    UnicodeString literal;    
    int32_t i = patternOffset;
	
    // scan pattern looking for contiguous literal characters
    for ( ; i < pattern.length(); i += 1) {
        UChar ch = pattern.charAt(i);
        
        if (!inQuote && ((ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A))) { // unquoted [A-Za-z]
            break;
        }
        
        if (ch == QUOTE) {
            // Match a quote literal ('') inside OR outside of quotes
            if ((i + 1) < pattern.length() && pattern.charAt(i + 1) == QUOTE) {
                i += 1;
            } else {
                inQuote = !inQuote;
                continue;
            }
        }
        
        literal += ch;
    }
    
    // at this point, literal contains the literal text
    // and i is the index of the next non-literal pattern character.
    int32_t p;
    int32_t t = textOffset;
    
    if (whitespaceLenient) {
        // trim leading, trailing whitespace from
        // the literal text
        literal.trim();
        
        // ignore any leading whitespace in the text
        while (t < text.length() && u_isWhitespace(text.charAt(t))) {
            t += 1;
        }
    }
        
    for (p = 0; p < literal.length() && t < text.length();) {
        UBool needWhitespace = FALSE;
        
        while (p < literal.length() && PatternProps::isWhiteSpace(literal.charAt(p))) {
            needWhitespace = TRUE;
            p += 1;
        }
        
        if (needWhitespace) {
            int32_t tStart = t;
            
            while (t < text.length()) {
                UChar tch = text.charAt(t);
                
                if (!u_isUWhiteSpace(tch) && !PatternProps::isWhiteSpace(tch)) {
                    break;
                }
                
                t += 1;
            }
            
            // TODO: should we require internal spaces
            // in lenient mode? (There won't be any
            // leading or trailing spaces)
            if (!whitespaceLenient && t == tStart) {
                // didn't find matching whitespace:
                // an error in strict mode
                return FALSE;
            }
            
            // In strict mode, this run of whitespace
            // may have been at the end.
            if (p >= literal.length()) {
                break;
            }
        }
        if (t >= text.length() || literal.charAt(p) != text.charAt(t)) {
            // Ran out of text, or found a non-matching character:
            // OK in lenient mode, an error in strict mode.
            if (whitespaceLenient) {
                if (t == textOffset && text.charAt(t) == 0x2e &&
                        isAfterNonNumericField(pattern, patternOffset)) {
                    // Lenient mode and the literal input text begins with a "." and
                    // we are after a non-numeric field: We skip the "."
                    ++t;
                    continue;  // Do not update p.
                }
                // if it is actual whitespace and we're whitespace lenient it's OK                
                
                UChar wsc = text.charAt(t);
                if(PatternProps::isWhiteSpace(wsc)) {
                    // Lenient mode and it's just whitespace we skip it
                    ++t;
                    continue;  // Do not update p.
                }
            } 
            // hack around oldleniency being a bit of a catch-all bucket and we're just adding support specifically for paritial matches
            if(partialMatchLenient && oldLeniency) {                             
                break;
            }
            
            return FALSE;
        }
        ++p;
        ++t;
    }
    
    // At this point if we're in strict mode we have a complete match.
    // If we're in lenient mode we may have a partial match, or no
    // match at all.
    if (p <= 0) {
        // no match. Pretend it matched a run of whitespace
        // and ignorables in the text.
        const  UnicodeSet *ignorables = NULL;
        UDateFormatField patternCharIndex = DateFormatSymbols::getPatternCharIndex(pattern.charAt(i));
        if (patternCharIndex != UDAT_FIELD_COUNT) {
            ignorables = SimpleDateFormatStaticSets::getIgnorables(patternCharIndex);
        }
        
        for (t = textOffset; t < text.length(); t += 1) {
            UChar ch = text.charAt(t);
            
            if (ignorables == NULL || !ignorables->contains(ch)) {
                break;
            }
        }
    }
    
    // if we get here, we've got a complete match.
    patternOffset = i - 1;
    textOffset = t;
    
    return TRUE;
}

//----------------------------------------------------------------------

int32_t SimpleDateFormat::matchString(const UnicodeString& text,
                              int32_t start,
                              UCalendarDateFields field,
                              const UnicodeString* data,
                              int32_t dataCount,
                              const UnicodeString* monthPattern,
                              Calendar& cal) const
{
    int32_t i = 0;
    int32_t count = dataCount;

    if (field == UCAL_DAY_OF_WEEK) i = 1;

    // There may be multiple strings in the data[] array which begin with
    // the same prefix (e.g., Cerven and Cervenec (June and July) in Czech).
    // We keep track of the longest match, and return that.  Note that this
    // unfortunately requires us to test all array elements.
    int32_t bestMatchLength = 0, bestMatch = -1;
    UnicodeString bestMatchName;
    int32_t isLeapMonth = 0;

    // {sfb} kludge to support case-insensitive comparison
    // {markus 2002oct11} do not just use caseCompareBetween because we do not know
    // the length of the match after case folding
    // {alan 20040607} don't case change the whole string, since the length
    // can change
    // TODO we need a case-insensitive startsWith function
    UnicodeString lcaseText;
    text.extract(start, INT32_MAX, lcaseText);
    lcaseText.foldCase();

    for (; i < count; ++i)
    {
        // Always compare if we have no match yet; otherwise only compare
        // against potentially better matches (longer strings).

        if (newBestMatchWithOptionalDot(lcaseText, data[i], bestMatchName, bestMatchLength)) {
            bestMatch = i;
            isLeapMonth = 0;
        }

        if (monthPattern != NULL) {
            UErrorCode status = U_ZERO_ERROR;
            UnicodeString leapMonthName;
            Formattable monthName((const UnicodeString&)(data[i]));
            MessageFormat::format(*monthPattern, &monthName, 1, leapMonthName, status);
            if (U_SUCCESS(status)) {
                if (newBestMatchWithOptionalDot(lcaseText, leapMonthName, bestMatchName, bestMatchLength)) {
                    bestMatch = i;
                    isLeapMonth = 1;
                }
            }
        }
    }
    if (bestMatch >= 0)
    {
        // Adjustment for Hebrew Calendar month Adar II
        if (!strcmp(cal.getType(),"hebrew") && field==UCAL_MONTH && bestMatch==13) {
            cal.set(field,6);
        }
        else {
            if (field == UCAL_YEAR) {
                bestMatch++; // only get here for cyclic year names, which match 1-based years 1-60
            }
            cal.set(field, bestMatch);
        }
        if (monthPattern != NULL) {
            cal.set(UCAL_IS_LEAP_MONTH, isLeapMonth);
        }

        // Once we have a match, we have to determine the length of the
        // original source string.  This will usually be == the length of
        // the case folded string, but it may differ (e.g. sharp s).

        // Most of the time, the length will be the same as the length
        // of the string from the locale data.  Sometimes it will be
        // different, in which case we will have to figure it out by
        // adding a character at a time, until we have a match.  We do
        // this all in one loop, where we try 'len' first (at index
        // i==0).
        int32_t len = bestMatchName.length(); // 99+% of the time
        int32_t n = text.length() - start;
        for (i=0; i<=n; ++i) {
            int32_t j=i;
            if (i == 0) {
                j = len;
            } else if (i == len) {
                continue; // already tried this when i was 0
            }
            text.extract(start, j, lcaseText);
            lcaseText.foldCase();
            if (bestMatchName == lcaseText) {
                return start + j;
            }
        }
    }

    return -start;
}

static UBool
newBestMatchWithOptionalDot(const UnicodeString &lcaseText,
                            const UnicodeString &data,
                            UnicodeString &bestMatchName,
                            int32_t &bestMatchLength) {
    UnicodeString lcase;
    lcase.fastCopyFrom(data).foldCase();
    int32_t length = lcase.length();
    if (length <= bestMatchLength) {
        // data cannot provide a better match.
        return FALSE;
    }

    if (lcaseText.compareBetween(0, length, lcase, 0, length) == 0) {
        // normal match
        bestMatchName = lcase;
        bestMatchLength = length;
        return TRUE;
    }
    if (lcase.charAt(--length) == 0x2e) {
        if (lcaseText.compareBetween(0, length, lcase, 0, length) == 0) {
            // The input text matches the data except for data's trailing dot.
            bestMatchName = lcase;
            bestMatchName.truncate(length);
            bestMatchLength = length;
            return TRUE;
        }
    }
    return FALSE;
}

//----------------------------------------------------------------------

void
SimpleDateFormat::set2DigitYearStart(UDate d, UErrorCode& status)
{
    parseAmbiguousDatesAsAfter(d, status);
}

/**
 * Private member function that converts the parsed date strings into
 * timeFields. Returns -start (for ParsePosition) if failed.
 */
int32_t SimpleDateFormat::subParse(const UnicodeString& text, int32_t& start, UChar ch, int32_t count,
                           UBool obeyCount, UBool allowNegative, UBool ambiguousYear[], int32_t& saveHebrewMonth, Calendar& cal,
                           int32_t patLoc, MessageFormat * numericLeapMonthFormatter, UTimeZoneFormatTimeType *tzTimeType) const
{
    Formattable number;
    int32_t value = 0;
    int32_t i;
    int32_t ps = 0;
    UErrorCode status = U_ZERO_ERROR;
    ParsePosition pos(0);
    UDateFormatField patternCharIndex = DateFormatSymbols::getPatternCharIndex(ch);
    NumberFormat *currentNumberFormat;
    UnicodeString temp;
    UBool gotNumber = FALSE;

#if defined (U_DEBUG_CAL)
    //fprintf(stderr, "%s:%d - [%c]  st=%d \n", __FILE__, __LINE__, (char) ch, start);
#endif

    if (patternCharIndex == UDAT_FIELD_COUNT) {
        return -start;
    }

    currentNumberFormat = getNumberFormatByIndex(patternCharIndex);
    UCalendarDateFields field = fgPatternIndexToCalendarField[patternCharIndex];
    UnicodeString hebr("hebr", 4, US_INV);

    if (numericLeapMonthFormatter != NULL) {
        numericLeapMonthFormatter->setFormats((const Format **)&currentNumberFormat, 1);
    }
    UBool isChineseCalendar = (uprv_strcmp(cal.getType(),"chinese") == 0 || uprv_strcmp(cal.getType(),"dangi") == 0);

    // If there are any spaces here, skip over them.  If we hit the end
    // of the string, then fail.
    for (;;) {
        if (start >= text.length()) {
            return -start;
        }
        UChar32 c = text.char32At(start);
        if (!u_isUWhiteSpace(c) /*||*/ && !PatternProps::isWhiteSpace(c)) {
            break;
        }
        start += U16_LENGTH(c);
    }
    pos.setIndex(start);

    // We handle a few special cases here where we need to parse
    // a number value.  We handle further, more generic cases below.  We need
    // to handle some of them here because some fields require extra processing on
    // the parsed value.
    if (patternCharIndex == UDAT_HOUR_OF_DAY1_FIELD ||                       // k
        patternCharIndex == UDAT_HOUR_OF_DAY0_FIELD ||                       // H
        patternCharIndex == UDAT_HOUR1_FIELD ||                              // h
        patternCharIndex == UDAT_HOUR0_FIELD ||                              // K
        (patternCharIndex == UDAT_DOW_LOCAL_FIELD && count <= 2) ||          // e
        (patternCharIndex == UDAT_STANDALONE_DAY_FIELD && count <= 2) ||     // c
        (patternCharIndex == UDAT_MONTH_FIELD && count <= 2) ||              // M
        (patternCharIndex == UDAT_STANDALONE_MONTH_FIELD && count <= 2) ||   // L
        (patternCharIndex == UDAT_QUARTER_FIELD && count <= 2) ||            // Q
        (patternCharIndex == UDAT_STANDALONE_QUARTER_FIELD && count <= 2) || // q
        patternCharIndex == UDAT_YEAR_FIELD ||                               // y
        patternCharIndex == UDAT_YEAR_WOY_FIELD ||                           // Y
        patternCharIndex == UDAT_YEAR_NAME_FIELD ||                          // U (falls back to numeric)
        (patternCharIndex == UDAT_ERA_FIELD && isChineseCalendar) ||         // G
        patternCharIndex == UDAT_FRACTIONAL_SECOND_FIELD)                    // S
    {
        int32_t parseStart = pos.getIndex();
        // It would be good to unify this with the obeyCount logic below,
        // but that's going to be difficult.
        const UnicodeString* src;

        UBool parsedNumericLeapMonth = FALSE;
        if (numericLeapMonthFormatter != NULL && (patternCharIndex == UDAT_MONTH_FIELD || patternCharIndex == UDAT_STANDALONE_MONTH_FIELD)) {
            int32_t argCount;
            Formattable * args = numericLeapMonthFormatter->parse(text, pos, argCount);
            if (args != NULL && argCount == 1 && pos.getIndex() > parseStart && args[0].isNumeric()) {
                parsedNumericLeapMonth = TRUE;
                number.setLong(args[0].getLong());
                cal.set(UCAL_IS_LEAP_MONTH, 1);
                delete[] args;
            } else {
                pos.setIndex(parseStart);
                cal.set(UCAL_IS_LEAP_MONTH, 0);
            }
        }

        if (!parsedNumericLeapMonth) {
            if (obeyCount) {
                if ((start+count) > text.length()) {
                    return -start;
                }

                text.extractBetween(0, start + count, temp);
                src = &temp;
            } else {
                src = &text;
            }

            parseInt(*src, number, pos, allowNegative,currentNumberFormat);
        }

        int32_t txtLoc = pos.getIndex();

        if (txtLoc > parseStart) {
            value = number.getLong();
            gotNumber = TRUE;
            
            // suffix processing
            if (value < 0 ) {
                txtLoc = checkIntSuffix(text, txtLoc, patLoc+1, TRUE);
                if (txtLoc != pos.getIndex()) {
                    value *= -1;
                }
            }
            else {
                txtLoc = checkIntSuffix(text, txtLoc, patLoc+1, FALSE);
            }

            if (!getBooleanAttribute(UDAT_PARSE_ALLOW_WHITESPACE, status)) {
                // Check the range of the value
                int32_t bias = gFieldRangeBias[patternCharIndex];
                if (bias >= 0 && (value > cal.getMaximum(field) + bias || value < cal.getMinimum(field) + bias)) {
                    return -start;
                }
            }

            pos.setIndex(txtLoc);
        }
    }
    
    // Make sure that we got a number if
    // we want one, and didn't get one
    // if we don't want one.
    switch (patternCharIndex) {
        case UDAT_HOUR_OF_DAY1_FIELD:
        case UDAT_HOUR_OF_DAY0_FIELD:
        case UDAT_HOUR1_FIELD:
        case UDAT_HOUR0_FIELD:
            // special range check for hours:
            if (value < 0 || value > 24) {
                return -start;
            }
            
            // fall through to gotNumber check
            
        case UDAT_YEAR_FIELD:
        case UDAT_YEAR_WOY_FIELD:
        case UDAT_FRACTIONAL_SECOND_FIELD:
            // these must be a number
            if (! gotNumber) {
                return -start;
            }
            
            break;
            
        default:
            // we check the rest of the fields below.
            break;
    }

    switch (patternCharIndex) {
    case UDAT_ERA_FIELD:
        if (isChineseCalendar) {
            if (!gotNumber) {
                return -start;
            }
            cal.set(UCAL_ERA, value);
            return pos.getIndex();
        }
        if (count == 5) {
            ps = matchString(text, start, UCAL_ERA, fSymbols->fNarrowEras, fSymbols->fNarrowErasCount, NULL, cal);
        } else if (count == 4) {
            ps = matchString(text, start, UCAL_ERA, fSymbols->fEraNames, fSymbols->fEraNamesCount, NULL, cal);
        } else {
            ps = matchString(text, start, UCAL_ERA, fSymbols->fEras, fSymbols->fErasCount, NULL, cal);
        }

        // check return position, if it equals -start, then matchString error
        // special case the return code so we don't necessarily fail out until we
        // verify no year information also
        if (ps == -start)
            ps--;

        return ps;

    case UDAT_YEAR_FIELD:
        // If there are 3 or more YEAR pattern characters, this indicates
        // that the year value is to be treated literally, without any
        // two-digit year adjustments (e.g., from "01" to 2001).  Otherwise
        // we made adjustments to place the 2-digit year in the proper
        // century, for parsed strings from "00" to "99".  Any other string
        // is treated literally:  "2250", "-1", "1", "002".
        if (fDateOverride.compare(hebr)==0 && value < 1000) {
            value += HEBREW_CAL_CUR_MILLENIUM_START_YEAR;
        } else if ((pos.getIndex() - start) == 2 && !isChineseCalendar
            && u_isdigit(text.charAt(start))
            && u_isdigit(text.charAt(start+1)))
        {
            // only adjust year for patterns less than 3.
            if(count < 3) {
                // Assume for example that the defaultCenturyStart is 6/18/1903.
                // This means that two-digit years will be forced into the range
                // 6/18/1903 to 6/17/2003.  As a result, years 00, 01, and 02
                // correspond to 2000, 2001, and 2002.  Years 04, 05, etc. correspond
                // to 1904, 1905, etc.  If the year is 03, then it is 2003 if the
                // other fields specify a date before 6/18, or 1903 if they specify a
                // date afterwards.  As a result, 03 is an ambiguous year.  All other
                // two-digit years are unambiguous.
                if(fHaveDefaultCentury) { // check if this formatter even has a pivot year
                    int32_t ambiguousTwoDigitYear = fDefaultCenturyStartYear % 100;
                    ambiguousYear[0] = (value == ambiguousTwoDigitYear);
                    value += (fDefaultCenturyStartYear/100)*100 +
                            (value < ambiguousTwoDigitYear ? 100 : 0);
                }
            }
        }
        cal.set(UCAL_YEAR, value);

        // Delayed checking for adjustment of Hebrew month numbers in non-leap years.
        if (saveHebrewMonth >= 0) {
            HebrewCalendar *hc = (HebrewCalendar*)&cal;
            if (!hc->isLeapYear(value) && saveHebrewMonth >= 6) {
               cal.set(UCAL_MONTH,saveHebrewMonth);
            } else {
               cal.set(UCAL_MONTH,saveHebrewMonth-1);
            }
            saveHebrewMonth = -1;
        }
        return pos.getIndex();

    case UDAT_YEAR_WOY_FIELD:
        // Comment is the same as for UDAT_Year_FIELDs - look above
        if (fDateOverride.compare(hebr)==0 && value < 1000) {
            value += HEBREW_CAL_CUR_MILLENIUM_START_YEAR;
        } else if ((pos.getIndex() - start) == 2
            && u_isdigit(text.charAt(start))
            && u_isdigit(text.charAt(start+1))
            && fHaveDefaultCentury )
        {
            int32_t ambiguousTwoDigitYear = fDefaultCenturyStartYear % 100;
            ambiguousYear[0] = (value == ambiguousTwoDigitYear);
            value += (fDefaultCenturyStartYear/100)*100 +
                (value < ambiguousTwoDigitYear ? 100 : 0);
        }
        cal.set(UCAL_YEAR_WOY, value);
        return pos.getIndex();

    case UDAT_YEAR_NAME_FIELD:
        if (fSymbols->fShortYearNames != NULL) {
            int32_t newStart = matchString(text, start, UCAL_YEAR, fSymbols->fShortYearNames, fSymbols->fShortYearNamesCount, NULL, cal);
            if (newStart > 0) {
                return newStart;
            }
        }
        if (gotNumber && (getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC,status) || value > fSymbols->fShortYearNamesCount)) {
            cal.set(UCAL_YEAR, value);
            return pos.getIndex();
        }
        return -start;

    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
        if (gotNumber) // i.e., M or MM.
        {
            // When parsing month numbers from the Hebrew Calendar, we might need to adjust the month depending on whether
            // or not it was a leap year.  We may or may not yet know what year it is, so might have to delay checking until
            // the year is parsed.
            if (!strcmp(cal.getType(),"hebrew")) {
                HebrewCalendar *hc = (HebrewCalendar*)&cal;
                if (cal.isSet(UCAL_YEAR)) {
                   UErrorCode status = U_ZERO_ERROR;
                   if (!hc->isLeapYear(hc->get(UCAL_YEAR,status)) && value >= 6) {
                       cal.set(UCAL_MONTH, value);
                   } else {
                       cal.set(UCAL_MONTH, value - 1);
                   }
                } else {
                    saveHebrewMonth = value;
                }
            } else {
                // Don't want to parse the month if it is a string
                // while pattern uses numeric style: M/MM, L/LL
                // [We computed 'value' above.]
                cal.set(UCAL_MONTH, value - 1);
            }
            return pos.getIndex();
        } else {
            // count >= 3 // i.e., MMM/MMMM, LLL/LLLL
            // Want to be able to parse both short and long forms.
            // Try count == 4 first:
            UnicodeString * wideMonthPat = NULL;
            UnicodeString * shortMonthPat = NULL;
            if (fSymbols->fLeapMonthPatterns != NULL && fSymbols->fLeapMonthPatternsCount >= DateFormatSymbols::kMonthPatternsCount) {
                if (patternCharIndex==UDAT_MONTH_FIELD) {
                    wideMonthPat = &fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternFormatWide];
                    shortMonthPat = &fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternFormatAbbrev];
                } else {
                    wideMonthPat = &fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternStandaloneWide];
                    shortMonthPat = &fSymbols->fLeapMonthPatterns[DateFormatSymbols::kLeapMonthPatternStandaloneAbbrev];
                }
            }
            int32_t newStart = 0;
            if (patternCharIndex==UDAT_MONTH_FIELD) {
                if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 4) {
                    newStart = matchString(text, start, UCAL_MONTH, fSymbols->fMonths, fSymbols->fMonthsCount, wideMonthPat, cal); // try MMMM
                    if (newStart > 0) {
                        return newStart;
                    }
                }
                if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 3) {
                    newStart = matchString(text, start, UCAL_MONTH, fSymbols->fShortMonths, fSymbols->fShortMonthsCount, shortMonthPat, cal); // try MMM
                }
            } else {
                if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 4) {
                    newStart = matchString(text, start, UCAL_MONTH, fSymbols->fStandaloneMonths, fSymbols->fStandaloneMonthsCount, wideMonthPat, cal); // try LLLL
                    if (newStart > 0) {
                        return newStart;
                    }
                }
                if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 3) {
                    newStart = matchString(text, start, UCAL_MONTH, fSymbols->fStandaloneShortMonths, fSymbols->fStandaloneShortMonthsCount, shortMonthPat, cal); // try LLL
                }
            }
            if (newStart > 0 || !getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, status))  // currently we do not try to parse MMMMM/LLLLL: #8860
                return newStart;
            // else we allowing parsing as number, below
        }
        break;

    case UDAT_HOUR_OF_DAY1_FIELD:
        // [We computed 'value' above.]
        if (value == cal.getMaximum(UCAL_HOUR_OF_DAY) + 1)
            value = 0;
            
        // fall through to set field
            
    case UDAT_HOUR_OF_DAY0_FIELD:
        cal.set(UCAL_HOUR_OF_DAY, value);
        return pos.getIndex();

    case UDAT_FRACTIONAL_SECOND_FIELD:
        // Fractional seconds left-justify
        i = pos.getIndex() - start;
        if (i < 3) {
            while (i < 3) {
                value *= 10;
                i++;
            }
        } else {
            int32_t a = 1;
            while (i > 3) {
                a *= 10;
                i--;
            }
            value /= a;
        }
        cal.set(UCAL_MILLISECOND, value);
        return pos.getIndex();

    case UDAT_DOW_LOCAL_FIELD:
        if (gotNumber) // i.e., e or ee
        {
            // [We computed 'value' above.]
            cal.set(UCAL_DOW_LOCAL, value);
            return pos.getIndex();
        }
        // else for eee-eeeee fall through to handling of EEE-EEEEE
        // fall through, do not break here
    case UDAT_DAY_OF_WEEK_FIELD:
        {
            // Want to be able to parse both short and long forms.
            // Try count == 4 (EEEE) wide first:
            int32_t newStart = 0;
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 4) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                          fSymbols->fWeekdays, fSymbols->fWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            // EEEE wide failed, now try EEE abbreviated
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 3) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                       fSymbols->fShortWeekdays, fSymbols->fShortWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            // EEE abbreviated failed, now try EEEEEE short
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 6) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                       fSymbols->fShorterWeekdays, fSymbols->fShorterWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            // EEEEEE short failed, now try EEEEE narrow
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 5) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                       fSymbols->fNarrowWeekdays, fSymbols->fNarrowWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            if (!getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, status) || patternCharIndex == UDAT_DAY_OF_WEEK_FIELD)
                return newStart;
            // else we allowing parsing as number, below
        }
        break;

    case UDAT_STANDALONE_DAY_FIELD:
        {
            if (gotNumber) // c or cc
            {
                // [We computed 'value' above.]
                cal.set(UCAL_DOW_LOCAL, value);
                return pos.getIndex();
            }
            // Want to be able to parse both short and long forms.
            // Try count == 4 (cccc) first:
            int32_t newStart = 0;
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 4) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                      fSymbols->fStandaloneWeekdays, fSymbols->fStandaloneWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 3) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                          fSymbols->fStandaloneShortWeekdays, fSymbols->fStandaloneShortWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 6) {
                if ((newStart = matchString(text, start, UCAL_DAY_OF_WEEK,
                                          fSymbols->fStandaloneShorterWeekdays, fSymbols->fStandaloneShorterWeekdaysCount, NULL, cal)) > 0)
                    return newStart;
            }
            if (!getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, status))
                return newStart;
            // else we allowing parsing as number, below
        }
        break;

    case UDAT_AM_PM_FIELD:
        return matchString(text, start, UCAL_AM_PM, fSymbols->fAmPms, fSymbols->fAmPmsCount, NULL, cal);

    case UDAT_HOUR1_FIELD:
        // [We computed 'value' above.]
        if (value == cal.getLeastMaximum(UCAL_HOUR)+1)
            value = 0;
            
        // fall through to set field
            
    case UDAT_HOUR0_FIELD:
        cal.set(UCAL_HOUR, value);
        return pos.getIndex();

    case UDAT_QUARTER_FIELD:
        if (gotNumber) // i.e., Q or QQ.
        {
            // Don't want to parse the month if it is a string
            // while pattern uses numeric style: Q or QQ.
            // [We computed 'value' above.]
            cal.set(UCAL_MONTH, (value - 1) * 3);
            return pos.getIndex();
        } else {
            // count >= 3 // i.e., QQQ or QQQQ
            // Want to be able to parse both short and long forms.
            // Try count == 4 first:
            int32_t newStart = 0;

            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 4) {
                if ((newStart = matchQuarterString(text, start, UCAL_MONTH,
                                      fSymbols->fQuarters, fSymbols->fQuartersCount, cal)) > 0)
                    return newStart;
            }
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 3) {
                if ((newStart = matchQuarterString(text, start, UCAL_MONTH,
                                          fSymbols->fShortQuarters, fSymbols->fShortQuartersCount, cal)) > 0)
                    return newStart;
            }
            if (!getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, status))
                return newStart;
            // else we allowing parsing as number, below
            if(!getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status))
                return -start;
        }
        break;

    case UDAT_STANDALONE_QUARTER_FIELD:
        if (gotNumber) // i.e., q or qq.
        {
            // Don't want to parse the month if it is a string
            // while pattern uses numeric style: q or q.
            // [We computed 'value' above.]
            cal.set(UCAL_MONTH, (value - 1) * 3);
            return pos.getIndex();
        } else {
            // count >= 3 // i.e., qqq or qqqq
            // Want to be able to parse both short and long forms.
            // Try count == 4 first:
            int32_t newStart = 0;

            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 4) {
                if ((newStart = matchQuarterString(text, start, UCAL_MONTH,
                                      fSymbols->fStandaloneQuarters, fSymbols->fStandaloneQuartersCount, cal)) > 0)
                    return newStart;
            }
            if(getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status) || count == 3) {
                if ((newStart = matchQuarterString(text, start, UCAL_MONTH,
                                          fSymbols->fStandaloneShortQuarters, fSymbols->fStandaloneShortQuartersCount, cal)) > 0)
                    return newStart;
            }
            if (!getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, status))
                return newStart;
            // else we allowing parsing as number, below
            if(!getBooleanAttribute(UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH, status))
                return -start;
        }
        break;

    case UDAT_TIMEZONE_FIELD: // 'z'
        {
            UTimeZoneFormatStyle style = (count < 4) ? UTZFMT_STYLE_SPECIFIC_SHORT : UTZFMT_STYLE_SPECIFIC_LONG;
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
        }
        break;
    case UDAT_TIMEZONE_RFC_FIELD: // 'Z'
        {
            UTimeZoneFormatStyle style = (count < 4) ?
                UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL : ((count == 5) ? UTZFMT_STYLE_ISO_EXTENDED_FULL: UTZFMT_STYLE_LOCALIZED_GMT);
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
            return -start;
        }
    case UDAT_TIMEZONE_GENERIC_FIELD: // 'v'
        {
            UTimeZoneFormatStyle style = (count < 4) ? UTZFMT_STYLE_GENERIC_SHORT : UTZFMT_STYLE_GENERIC_LONG;
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
            return -start;
        }
    case UDAT_TIMEZONE_SPECIAL_FIELD: // 'V'
        {
            UTimeZoneFormatStyle style;
            switch (count) {
            case 1:
                style = UTZFMT_STYLE_ZONE_ID_SHORT;
                break;
            case 2:
                style = UTZFMT_STYLE_ZONE_ID;
                break;
            case 3:
                style = UTZFMT_STYLE_EXEMPLAR_LOCATION;
                break;
            default:
                style = UTZFMT_STYLE_GENERIC_LOCATION;
                break;
            }
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
            return -start;
        }
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD: // 'O'
        {
            UTimeZoneFormatStyle style = (count < 4) ? UTZFMT_STYLE_LOCALIZED_GMT_SHORT : UTZFMT_STYLE_LOCALIZED_GMT;
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
            return -start;
        }
    case UDAT_TIMEZONE_ISO_FIELD: // 'X'
        {
            UTimeZoneFormatStyle style;
            switch (count) {
            case 1:
                style = UTZFMT_STYLE_ISO_BASIC_SHORT;
                break;
            case 2:
                style = UTZFMT_STYLE_ISO_BASIC_FIXED;
                break;
            case 3:
                style = UTZFMT_STYLE_ISO_EXTENDED_FIXED;
                break;
            case 4:
                style = UTZFMT_STYLE_ISO_BASIC_FULL;
                break;
            default:
                style = UTZFMT_STYLE_ISO_EXTENDED_FULL;
                break;
            }
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
            return -start;
        }
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD: // 'x'
        {
            UTimeZoneFormatStyle style;
            switch (count) {
            case 1:
                style = UTZFMT_STYLE_ISO_BASIC_LOCAL_SHORT;
                break;
            case 2:
                style = UTZFMT_STYLE_ISO_BASIC_LOCAL_FIXED;
                break;
            case 3:
                style = UTZFMT_STYLE_ISO_EXTENDED_LOCAL_FIXED;
                break;
            case 4:
                style = UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL;
                break;
            default:
                style = UTZFMT_STYLE_ISO_EXTENDED_LOCAL_FULL;
                break;
            }
            TimeZone *tz  = tzFormat()->parse(style, text, pos, tzTimeType);
            if (tz != NULL) {
                cal.adoptTimeZone(tz);
                return pos.getIndex();
            }
            return -start;
        }

    default:
        // Handle "generic" fields
        // this is now handled below, outside the switch block
        break;
    }
    // Handle "generic" fields:
    // switch default case now handled here (outside switch block) to allow
    // parsing of some string fields as digits for lenient case

    int32_t parseStart = pos.getIndex();
    const UnicodeString* src;
    if (obeyCount) {
        if ((start+count) > text.length()) {
            return -start;
        }
        text.extractBetween(0, start + count, temp);
        src = &temp;
    } else {
        src = &text;
    }
    parseInt(*src, number, pos, allowNegative,currentNumberFormat);
    if (pos.getIndex() != parseStart) {
        int32_t value = number.getLong();

        // Don't need suffix processing here (as in number processing at the beginning of the function);
        // the new fields being handled as numeric values (month, weekdays, quarters) should not have suffixes.

        if (!getBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, status)) {
            // Check the range of the value
            int32_t bias = gFieldRangeBias[patternCharIndex];
            if (bias >= 0 && (value > cal.getMaximum(field) + bias || value < cal.getMinimum(field) + bias)) {
                return -start;
            }
        }

        // For the following, need to repeat some of the "if (gotNumber)" code above:
        // UDAT_[STANDALONE_]MONTH_FIELD, UDAT_DOW_LOCAL_FIELD, UDAT_STANDALONE_DAY_FIELD,
        // UDAT_[STANDALONE_]QUARTER_FIELD
        switch (patternCharIndex) {
        case UDAT_MONTH_FIELD:
            // See notes under UDAT_MONTH_FIELD case above
            if (!strcmp(cal.getType(),"hebrew")) {
                HebrewCalendar *hc = (HebrewCalendar*)&cal;
                if (cal.isSet(UCAL_YEAR)) {
                   UErrorCode status = U_ZERO_ERROR;
                   if (!hc->isLeapYear(hc->get(UCAL_YEAR,status)) && value >= 6) {
                       cal.set(UCAL_MONTH, value);
                   } else {
                       cal.set(UCAL_MONTH, value - 1);
                   }
                } else {
                    saveHebrewMonth = value;
                }
            } else {
                cal.set(UCAL_MONTH, value - 1);
            }
            break;
        case UDAT_STANDALONE_MONTH_FIELD:
            cal.set(UCAL_MONTH, value - 1);
            break;
        case UDAT_DOW_LOCAL_FIELD:
        case UDAT_STANDALONE_DAY_FIELD:
            cal.set(UCAL_DOW_LOCAL, value);
            break;
        case UDAT_QUARTER_FIELD:
        case UDAT_STANDALONE_QUARTER_FIELD:
             cal.set(UCAL_MONTH, (value - 1) * 3);
             break;
        case UDAT_RELATED_YEAR_FIELD:
            cal.setRelatedYear(value);
            break;
        default:
            cal.set(field, value);
            break;
        }
        return pos.getIndex();
    }
    return -start;
}

/**
 * Parse an integer using fNumberFormat.  This method is semantically
 * const, but actually may modify fNumberFormat.
 */
void SimpleDateFormat::parseInt(const UnicodeString& text,
                                Formattable& number,
                                ParsePosition& pos,
                                UBool allowNegative,
                                NumberFormat *fmt) const {
    parseInt(text, number, -1, pos, allowNegative,fmt);
}

/**
 * Parse an integer using fNumberFormat up to maxDigits.
 */
void SimpleDateFormat::parseInt(const UnicodeString& text,
                                Formattable& number,
                                int32_t maxDigits,
                                ParsePosition& pos,
                                UBool allowNegative,
                                NumberFormat *fmt) const {
    UnicodeString oldPrefix;
    DecimalFormat* df = NULL;
    if (!allowNegative && (df = dynamic_cast<DecimalFormat*>(fmt)) != NULL) {
        df->getNegativePrefix(oldPrefix);
        df->setNegativePrefix(UnicodeString(TRUE, SUPPRESS_NEGATIVE_PREFIX, -1));
    }
    int32_t oldPos = pos.getIndex();
    fmt->parse(text, number, pos);
    if (df != NULL) {
        df->setNegativePrefix(oldPrefix);
    }

    if (maxDigits > 0) {
        // adjust the result to fit into
        // the maxDigits and move the position back
        int32_t nDigits = pos.getIndex() - oldPos;
        if (nDigits > maxDigits) {
            int32_t val = number.getLong();
            nDigits -= maxDigits;
            while (nDigits > 0) {
                val /= 10;
                nDigits--;
            }
            pos.setIndex(oldPos + maxDigits);
            number.setLong(val);
        }
    }
}

//----------------------------------------------------------------------

void SimpleDateFormat::translatePattern(const UnicodeString& originalPattern,
                                        UnicodeString& translatedPattern,
                                        const UnicodeString& from,
                                        const UnicodeString& to,
                                        UErrorCode& status)
{
  // run through the pattern and convert any pattern symbols from the version
  // in "from" to the corresponding character ion "to".  This code takes
  // quoted strings into account (it doesn't try to translate them), and it signals
  // an error if a particular "pattern character" doesn't appear in "from".
  // Depending on the values of "from" and "to" this can convert from generic
  // to localized patterns or localized to generic.
  if (U_FAILURE(status))
    return;

  translatedPattern.remove();
  UBool inQuote = FALSE;
  for (int32_t i = 0; i < originalPattern.length(); ++i) {
    UChar c = originalPattern[i];
    if (inQuote) {
      if (c == QUOTE)
    inQuote = FALSE;
    }
    else {
      if (c == QUOTE)
    inQuote = TRUE;
      else if ((c >= 0x0061 /*'a'*/ && c <= 0x007A) /*'z'*/
           || (c >= 0x0041 /*'A'*/ && c <= 0x005A /*'Z'*/)) {
    int32_t ci = from.indexOf(c);
    if (ci == -1) {
      status = U_INVALID_FORMAT_ERROR;
      return;
    }
    c = to[ci];
      }
    }
    translatedPattern += c;
  }
  if (inQuote) {
    status = U_INVALID_FORMAT_ERROR;
    return;
  }
}

//----------------------------------------------------------------------

UnicodeString&
SimpleDateFormat::toPattern(UnicodeString& result) const
{
    result = fPattern;
    return result;
}

//----------------------------------------------------------------------

UnicodeString&
SimpleDateFormat::toLocalizedPattern(UnicodeString& result,
                                     UErrorCode& status) const
{
    translatePattern(fPattern, result,
                     UnicodeString(DateFormatSymbols::getPatternUChars()),
                     fSymbols->fLocalPatternChars, status);
    return result;
}

//----------------------------------------------------------------------

void
SimpleDateFormat::applyPattern(const UnicodeString& pattern)
{
    fPattern = pattern;
}

//----------------------------------------------------------------------

void
SimpleDateFormat::applyLocalizedPattern(const UnicodeString& pattern,
                                        UErrorCode &status)
{
    translatePattern(pattern, fPattern,
                     fSymbols->fLocalPatternChars,
                     UnicodeString(DateFormatSymbols::getPatternUChars()), status);
}

//----------------------------------------------------------------------

const DateFormatSymbols*
SimpleDateFormat::getDateFormatSymbols() const
{
    return fSymbols;
}

//----------------------------------------------------------------------

void
SimpleDateFormat::adoptDateFormatSymbols(DateFormatSymbols* newFormatSymbols)
{
    delete fSymbols;
    fSymbols = newFormatSymbols;
}

//----------------------------------------------------------------------
void
SimpleDateFormat::setDateFormatSymbols(const DateFormatSymbols& newFormatSymbols)
{
    delete fSymbols;
    fSymbols = new DateFormatSymbols(newFormatSymbols);
}

//----------------------------------------------------------------------
const TimeZoneFormat*
SimpleDateFormat::getTimeZoneFormat(void) const {
    return (const TimeZoneFormat*)tzFormat();
}

//----------------------------------------------------------------------
void
SimpleDateFormat::adoptTimeZoneFormat(TimeZoneFormat* timeZoneFormatToAdopt)
{
    delete fTimeZoneFormat;
    fTimeZoneFormat = timeZoneFormatToAdopt;
}

//----------------------------------------------------------------------
void
SimpleDateFormat::setTimeZoneFormat(const TimeZoneFormat& newTimeZoneFormat)
{
    delete fTimeZoneFormat;
    fTimeZoneFormat = new TimeZoneFormat(newTimeZoneFormat);
}

//----------------------------------------------------------------------


void SimpleDateFormat::adoptCalendar(Calendar* calendarToAdopt)
{
  UErrorCode status = U_ZERO_ERROR;
  DateFormat::adoptCalendar(calendarToAdopt);
  delete fSymbols;
  fSymbols=NULL;
  initializeSymbols(fLocale, fCalendar, status);  // we need new symbols
  initializeDefaultCentury();  // we need a new century (possibly)
}


//----------------------------------------------------------------------


// override the DateFormat implementation in order to
// lazily initialize fCapitalizationBrkIter
void
SimpleDateFormat::setContext(UDisplayContext value, UErrorCode& status)
{
    DateFormat::setContext(value, status);
#if !UCONFIG_NO_BREAK_ITERATION
    if (U_SUCCESS(status)) {
        if ( fCapitalizationBrkIter == NULL && (value==UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE ||
                value==UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU || value==UDISPCTX_CAPITALIZATION_FOR_STANDALONE) ) {
            UErrorCode status = U_ZERO_ERROR;
            fCapitalizationBrkIter = BreakIterator::createSentenceInstance(fLocale, status);
            if (U_FAILURE(status)) {
                delete fCapitalizationBrkIter;
                fCapitalizationBrkIter = NULL;
            }
        }
    }
#endif
}


//----------------------------------------------------------------------


UBool
SimpleDateFormat::isFieldUnitIgnored(UCalendarDateFields field) const {
    return isFieldUnitIgnored(fPattern, field);
}


UBool
SimpleDateFormat::isFieldUnitIgnored(const UnicodeString& pattern,
                                     UCalendarDateFields field) {
    int32_t fieldLevel = fgCalendarFieldToLevel[field];
    int32_t level;
    UChar ch;
    UBool inQuote = FALSE;
    UChar prevCh = 0;
    int32_t count = 0;

    for (int32_t i = 0; i < pattern.length(); ++i) {
        ch = pattern[i];
        if (ch != prevCh && count > 0) {
            level = fgPatternCharToLevel[prevCh - PATTERN_CHAR_BASE];
            // the larger the level, the smaller the field unit.
            if ( fieldLevel <= level ) {
                return FALSE;
            }
            count = 0;
        }
        if (ch == QUOTE) {
            if ((i+1) < pattern.length() && pattern[i+1] == QUOTE) {
                ++i;
            } else {
                inQuote = ! inQuote;
            }
        }
        else if ( ! inQuote && ((ch >= 0x0061 /*'a'*/ && ch <= 0x007A /*'z'*/)
                    || (ch >= 0x0041 /*'A'*/ && ch <= 0x005A /*'Z'*/))) {
            prevCh = ch;
            ++count;
        }
    }
    if ( count > 0 ) {
        // last item
        level = fgPatternCharToLevel[prevCh - PATTERN_CHAR_BASE];
            if ( fieldLevel <= level ) {
                return FALSE;
            }
    }
    return TRUE;
}

//----------------------------------------------------------------------

const Locale&
SimpleDateFormat::getSmpFmtLocale(void) const {
    return fLocale;
}

//----------------------------------------------------------------------

int32_t
SimpleDateFormat::checkIntSuffix(const UnicodeString& text, int32_t start,
                                 int32_t patLoc, UBool isNegative) const {
    // local variables
    UnicodeString suf;
    int32_t patternMatch;
    int32_t textPreMatch;
    int32_t textPostMatch;

    // check that we are still in range
    if ( (start > text.length()) ||
         (start < 0) ||
         (patLoc < 0) ||
         (patLoc > fPattern.length())) {
        // out of range, don't advance location in text
        return start;
    }

    // get the suffix
    DecimalFormat* decfmt = dynamic_cast<DecimalFormat*>(fNumberFormat);
    if (decfmt != NULL) {
        if (isNegative) {
            suf = decfmt->getNegativeSuffix(suf);
        }
        else {
            suf = decfmt->getPositiveSuffix(suf);
        }
    }

    // check for suffix
    if (suf.length() <= 0) {
        return start;
    }

    // check suffix will be encountered in the pattern
    patternMatch = compareSimpleAffix(suf,fPattern,patLoc);

    // check if a suffix will be encountered in the text
    textPreMatch = compareSimpleAffix(suf,text,start);

    // check if a suffix was encountered in the text
    textPostMatch = compareSimpleAffix(suf,text,start-suf.length());

    // check for suffix match
    if ((textPreMatch >= 0) && (patternMatch >= 0) && (textPreMatch == patternMatch)) {
        return start;
    }
    else if ((textPostMatch >= 0) && (patternMatch >= 0) && (textPostMatch == patternMatch)) {
        return  start - suf.length();
    }

    // should not get here
    return start;
}

//----------------------------------------------------------------------

int32_t
SimpleDateFormat::compareSimpleAffix(const UnicodeString& affix,
                   const UnicodeString& input,
                   int32_t pos) const {
    int32_t start = pos;
    for (int32_t i=0; i<affix.length(); ) {
        UChar32 c = affix.char32At(i);
        int32_t len = U16_LENGTH(c);
        if (PatternProps::isWhiteSpace(c)) {
            // We may have a pattern like: \u200F \u0020
            //        and input text like: \u200F \u0020
            // Note that U+200F and U+0020 are Pattern_White_Space but only
            // U+0020 is UWhiteSpace.  So we have to first do a direct
            // match of the run of Pattern_White_Space in the pattern,
            // then match any extra characters.
            UBool literalMatch = FALSE;
            while (pos < input.length() &&
                   input.char32At(pos) == c) {
                literalMatch = TRUE;
                i += len;
                pos += len;
                if (i == affix.length()) {
                    break;
                }
                c = affix.char32At(i);
                len = U16_LENGTH(c);
                if (!PatternProps::isWhiteSpace(c)) {
                    break;
                }
            }

            // Advance over run in pattern
            i = skipPatternWhiteSpace(affix, i);

            // Advance over run in input text
            // Must see at least one white space char in input,
            // unless we've already matched some characters literally.
            int32_t s = pos;
            pos = skipUWhiteSpace(input, pos);
            if (pos == s && !literalMatch) {
                return -1;
            }

            // If we skip UWhiteSpace in the input text, we need to skip it in the pattern.
            // Otherwise, the previous lines may have skipped over text (such as U+00A0) that
            // is also in the affix.
            i = skipUWhiteSpace(affix, i);
        } else {
            if (pos < input.length() &&
                input.char32At(pos) == c) {
                i += len;
                pos += len;
            } else {
                return -1;
            }
        }
    }
    return pos - start;
}

//----------------------------------------------------------------------

int32_t
SimpleDateFormat::skipPatternWhiteSpace(const UnicodeString& text, int32_t pos) const {
    const UChar* s = text.getBuffer();
    return (int32_t)(PatternProps::skipWhiteSpace(s + pos, text.length() - pos) - s);
}

//----------------------------------------------------------------------

int32_t
SimpleDateFormat::skipUWhiteSpace(const UnicodeString& text, int32_t pos) const {
    while (pos < text.length()) {
        UChar32 c = text.char32At(pos);
        if (!u_isUWhiteSpace(c)) {
            break;
        }
        pos += U16_LENGTH(c);
    }
    return pos;
}

//----------------------------------------------------------------------

// Lazy TimeZoneFormat instantiation, semantically const.
TimeZoneFormat *
SimpleDateFormat::tzFormat() const {
    if (fTimeZoneFormat == NULL) {
        umtx_lock(&LOCK);
        {
            if (fTimeZoneFormat == NULL) {
                UErrorCode status = U_ZERO_ERROR;
                TimeZoneFormat *tzfmt = TimeZoneFormat::createInstance(fLocale, status);
                if (U_FAILURE(status)) {
                    return NULL;
                }

                const_cast<SimpleDateFormat *>(this)->fTimeZoneFormat = tzfmt;
            }
        }
        umtx_unlock(&LOCK);
    }
    return fTimeZoneFormat;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
