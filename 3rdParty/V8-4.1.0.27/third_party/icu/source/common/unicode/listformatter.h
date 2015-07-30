/*
*******************************************************************************
*
*   Copyright (C) 2012-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformatter.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 20120426
*   created by: Umesh P. Nair
*/

#ifndef __LISTFORMATTER_H__
#define __LISTFORMATTER_H__

#include "unicode/utypes.h"

#ifndef U_HIDE_DRAFT_API

#include "unicode/unistr.h"
#include "unicode/locid.h"

U_NAMESPACE_BEGIN

/** @internal */
class Hashtable;

#ifndef U_HIDE_INTERNAL_API
/** @internal */
struct ListFormatData : public UMemory {
    UnicodeString twoPattern;
    UnicodeString startPattern;
    UnicodeString middlePattern;
    UnicodeString endPattern;

  ListFormatData(const UnicodeString& two, const UnicodeString& start, const UnicodeString& middle, const UnicodeString& end) :
      twoPattern(two), startPattern(start), middlePattern(middle), endPattern(end) {}
};
#endif  /* U_HIDE_INTERNAL_API */


/**
 * \file
 * \brief C++ API: API for formatting a list.
 */


/**
 * An immutable class for formatting a list, using data from CLDR (or supplied
 * separately).
 *
 * Example: Input data ["Alice", "Bob", "Charlie", "Delta"] will be formatted
 * as "Alice, Bob, Charlie and Delta" in English.
 *
 * The ListFormatter class is not intended for public subclassing.
 * @draft ICU 50
 */
class U_COMMON_API ListFormatter : public UObject{

  public:

    /**
     * Copy constructor.
     * @draft ICU 52
     */
    ListFormatter(const ListFormatter&);

    /**
     * Assignment operator.
     * @draft ICU 52
     */
    ListFormatter& operator=(const ListFormatter& other);

    /**
     * Creates a ListFormatter appropriate for the default locale.
     *
     * @param errorCode ICU error code, set if no data available for default locale.
     * @return Pointer to a ListFormatter object for the default locale,
     *     created from internal data derived from CLDR data.
     * @draft ICU 50
     */
    static ListFormatter* createInstance(UErrorCode& errorCode);

    /**
     * Creates a ListFormatter appropriate for a locale.
     *
     * @param locale The locale.
     * @param errorCode ICU error code, set if no data available for the given locale.
     * @return A ListFormatter object created from internal data derived from
     *     CLDR data.
     * @draft ICU 50
     */
    static ListFormatter* createInstance(const Locale& locale, UErrorCode& errorCode);

#ifndef U_HIDE_INTERNAL_API
    /**
     * Creates a ListFormatter appropriate for a locale and style.
     *
     * @param locale The locale.
     * @param style the style, either "standard", "duration", or "duration-short"
     * @param errorCode ICU error code, set if no data available for the given locale.
     * @return A ListFormatter object created from internal data derived from
     *     CLDR data.
     * @internal
     */
    static ListFormatter* createInstance(const Locale& locale, const char* style, UErrorCode& errorCode);
#endif  /* U_HIDE_INTERNAL_API */

    /**
     * Destructor.
     *
     * @draft ICU 50
     */
    virtual ~ListFormatter();


    /**
     * Formats a list of strings.
     *
     * @param items An array of strings to be combined and formatted.
     * @param n_items Length of the array items.
     * @param appendTo The string to which the result should be appended to.
     * @param errorCode ICU error code, set if there is an error.
     * @return Formatted string combining the elements of items, appended to appendTo.
     * @draft ICU 50
     */
    UnicodeString& format(const UnicodeString items[], int32_t n_items,
        UnicodeString& appendTo, UErrorCode& errorCode) const;

#ifndef U_HIDE_INTERNAL_API
    /**
     * @internal constructor made public for testing.
     */
    ListFormatter(const ListFormatData* listFormatterData);
#endif  /* U_HIDE_INTERNAL_API */

  private:
    static void initializeHash(UErrorCode& errorCode);
    static const ListFormatData* getListFormatData(const Locale& locale, const char *style, UErrorCode& errorCode);

    ListFormatter();
    void addNewString(const UnicodeString& pattern, UnicodeString& originalString,
                      const UnicodeString& newString, UErrorCode& errorCode) const;

    const ListFormatData* data;
};

U_NAMESPACE_END

#endif /* U_HIDE_DRAFT_API */
#endif
