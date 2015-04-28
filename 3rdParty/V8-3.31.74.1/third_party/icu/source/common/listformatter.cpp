/*
*******************************************************************************
*
*   Copyright (C) 2013-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformatter.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#include "unicode/listformatter.h"
#include "simplepatternformatter.h"
#include "mutex.h"
#include "hash.h"
#include "cstring.h"
#include "ulocimp.h"
#include "charstr.h"
#include "ucln_cmn.h"
#include "uresimp.h"

U_NAMESPACE_BEGIN

struct ListFormatInternal : public UMemory {
    SimplePatternFormatter twoPattern;
    SimplePatternFormatter startPattern;
    SimplePatternFormatter middlePattern;
    SimplePatternFormatter endPattern;

ListFormatInternal(
        const UnicodeString& two,
        const UnicodeString& start,
        const UnicodeString& middle,
        const UnicodeString& end) :
        twoPattern(two),
        startPattern(start),
        middlePattern(middle),
        endPattern(end) {}

ListFormatInternal(const ListFormatData &data) :
        twoPattern(data.twoPattern),
        startPattern(data.startPattern),
        middlePattern(data.middlePattern),
        endPattern(data.endPattern) { }

ListFormatInternal(const ListFormatInternal &other) :
    twoPattern(other.twoPattern),
    startPattern(other.startPattern),
    middlePattern(other.middlePattern),
    endPattern(other.endPattern) { }
};



static Hashtable* listPatternHash = NULL;
static UMutex listFormatterMutex = U_MUTEX_INITIALIZER;
static const char *STANDARD_STYLE = "standard";

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_listformatter_cleanup() {
    delete listPatternHash;
    listPatternHash = NULL;
    return TRUE;
}

static void U_CALLCONV
uprv_deleteListFormatInternal(void *obj) {
    delete static_cast<ListFormatInternal *>(obj);
}

U_CDECL_END

static ListFormatInternal* loadListFormatInternal(
        const Locale& locale,
        const char* style,
        UErrorCode& errorCode);

static void getStringByKey(
        const UResourceBundle* rb,
        const char* key,
        UnicodeString& result,
        UErrorCode& errorCode);

ListFormatter::ListFormatter(const ListFormatter& other) :
        owned(other.owned), data(other.data) {
    if (other.owned != NULL) {
        owned = new ListFormatInternal(*other.owned);
        data = owned;
    }
}

ListFormatter& ListFormatter::operator=(const ListFormatter& other) {
    if (this == &other) {
        return *this;
    }
    delete owned;
    if (other.owned) {
        owned = new ListFormatInternal(*other.owned);
        data = owned;
    } else {
        owned = NULL;
        data = other.data;
    }
    return *this;
}

void ListFormatter::initializeHash(UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }

    listPatternHash = new Hashtable();
    if (listPatternHash == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    listPatternHash->setValueDeleter(uprv_deleteListFormatInternal);
    ucln_common_registerCleanup(UCLN_COMMON_LIST_FORMATTER, uprv_listformatter_cleanup);

}

const ListFormatInternal* ListFormatter::getListFormatInternal(
        const Locale& locale, const char *style, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    CharString keyBuffer(locale.getName(), errorCode);
    keyBuffer.append(':', errorCode).append(style, errorCode);
    UnicodeString key(keyBuffer.data(), -1, US_INV);
    ListFormatInternal* result = NULL;
    {
        Mutex m(&listFormatterMutex);
        if (listPatternHash == NULL) {
            initializeHash(errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
        result = static_cast<ListFormatInternal*>(listPatternHash->get(key));
    }
    if (result != NULL) {
        return result;
    }
    result = loadListFormatInternal(locale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }

    {
        Mutex m(&listFormatterMutex);
        ListFormatInternal* temp = static_cast<ListFormatInternal*>(listPatternHash->get(key));
        if (temp != NULL) {
            delete result;
            result = temp;
        } else {
            listPatternHash->put(key, result, errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
    }
    return result;
}

static ListFormatInternal* loadListFormatInternal(
        const Locale& locale, const char * style, UErrorCode& errorCode) {
    UResourceBundle* rb = ures_open(NULL, locale.getName(), &errorCode);
    if (U_FAILURE(errorCode)) {
        ures_close(rb);
        return NULL;
    }
    rb = ures_getByKeyWithFallback(rb, "listPattern", rb, &errorCode);
    rb = ures_getByKeyWithFallback(rb, style, rb, &errorCode);

    // TODO(Travis Keep): This is a hack until fallbacks can be added for
    // listPattern/duration and listPattern/duration-narrow in CLDR.
    if (errorCode == U_MISSING_RESOURCE_ERROR) {
        errorCode = U_ZERO_ERROR;
        rb = ures_getByKeyWithFallback(rb, "standard", rb, &errorCode);
    }
    if (U_FAILURE(errorCode)) {
        ures_close(rb);
        return NULL;
    }
    UnicodeString two, start, middle, end;
    getStringByKey(rb, "2", two, errorCode);
    getStringByKey(rb, "start", start, errorCode);
    getStringByKey(rb, "middle", middle, errorCode);
    getStringByKey(rb, "end", end, errorCode);
    ures_close(rb);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    ListFormatInternal* result = new ListFormatInternal(two, start, middle, end);
    if (result == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return result;
}

static void getStringByKey(const UResourceBundle* rb, const char* key, UnicodeString& result, UErrorCode& errorCode) {
    int32_t len;
    const UChar* ustr = ures_getStringByKeyWithFallback(rb, key, &len, &errorCode);
    if (U_FAILURE(errorCode)) {
      return;
    }
    result.setTo(ustr, len);
}

ListFormatter* ListFormatter::createInstance(UErrorCode& errorCode) {
    Locale locale;  // The default locale.
    return createInstance(locale, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, UErrorCode& errorCode) {
    return createInstance(locale, STANDARD_STYLE, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, const char *style, UErrorCode& errorCode) {
    Locale tempLocale = locale;
    const ListFormatInternal* listFormatInternal = getListFormatInternal(tempLocale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    ListFormatter* p = new ListFormatter(listFormatInternal);
    if (p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return p;
}

ListFormatter::ListFormatter(const ListFormatData& listFormatData) {
    owned = new ListFormatInternal(listFormatData);
    data = owned;
}

ListFormatter::ListFormatter(const ListFormatInternal* listFormatterInternal) : owned(NULL), data(listFormatterInternal) {
}

ListFormatter::~ListFormatter() {
    delete owned;
}

/**
 * Joins first and second using the pattern pat.
 * On entry offset is an offset into first or -1 if offset unspecified.
 * On exit offset is offset of second in result if recordOffset was set
 * Otherwise if it was >=0 it is set to point into result where it used
 * to point into first.
 */
static void joinStrings(
        const SimplePatternFormatter& pat,
        const UnicodeString& first,
        const UnicodeString& second,
        UnicodeString &result,
        UBool recordOffset,
        int32_t &offset,
        UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    const UnicodeString *params[2] = {&first, &second};
    int32_t offsets[2];
    pat.format(
            params,
            UPRV_LENGTHOF(params),
            result,
            offsets,
            UPRV_LENGTHOF(offsets),
            errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    if (offsets[0] == -1 || offsets[1] == -1) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return;
    }
    if (recordOffset) {
        offset = offsets[1];
    } else if (offset >= 0) {
        offset += offsets[0];
    }
}

UnicodeString& ListFormatter::format(
        const UnicodeString items[],
        int32_t nItems,
        UnicodeString& appendTo,
        UErrorCode& errorCode) const {
    int32_t offset;
    return format(items, nItems, appendTo, -1, offset, errorCode);
}

UnicodeString& ListFormatter::format(
        const UnicodeString items[],
        int32_t nItems,
        UnicodeString& appendTo,
        int32_t index,
        int32_t &offset,
        UErrorCode& errorCode) const {
    offset = -1;
    if (U_FAILURE(errorCode)) {
        return appendTo;
    }
    if (data == NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return appendTo;
    }

    if (nItems <= 0) {
        return appendTo;
    }
    if (nItems == 1) {
        if (index == 0) {
            offset = appendTo.length();
        }
        appendTo.append(items[0]);
        return appendTo;
    }
    if (nItems == 2) {
        if (index == 0) {
            offset = 0;
        }
        joinStrings(
                data->twoPattern,
                items[0],
                items[1],
                appendTo,
                index == 1,
                offset,
                errorCode);
        return appendTo;
    }
    UnicodeString temp[2];
    if (index == 0) {
        offset = 0;
    }
    joinStrings(
            data->startPattern,
            items[0],
            items[1],
            temp[0],
            index == 1,
            offset,
            errorCode);
    int32_t i;
    int32_t pos = 0;
    int32_t npos = 0;
    UBool startsWithZeroPlaceholder =
            data->middlePattern.startsWithPlaceholder(0);
    for (i = 2; i < nItems - 1; ++i) {
         if (!startsWithZeroPlaceholder) {
             npos = (pos + 1) & 1;
             temp[npos].remove();
         }
         joinStrings(
                 data->middlePattern,
                 temp[pos],
                 items[i],
                 temp[npos],
                 index == i,
                 offset,
                 errorCode);
         pos = npos;
    }
    if (!data->endPattern.startsWithPlaceholder(0)) {
        npos = (pos + 1) & 1;
        temp[npos].remove();
    }
    joinStrings(
            data->endPattern,
            temp[pos],
            items[nItems - 1],
            temp[npos],
            index == nItems - 1,
            offset,
            errorCode);
    if (U_SUCCESS(errorCode)) {
        if (offset >= 0) {
            offset += appendTo.length();
        }
        appendTo += temp[npos];
    }
    return appendTo;
}

U_NAMESPACE_END
