/*
*******************************************************************************
*
*   Copyright (C) 2013, International Business Machines
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
#include "mutex.h"
#include "hash.h"
#include "cstring.h"
#include "ulocimp.h"
#include "charstr.h"
#include "ucln_cmn.h"
#include "uresimp.h"

U_NAMESPACE_BEGIN

static Hashtable* listPatternHash = NULL;
static UMutex listFormatterMutex = U_MUTEX_INITIALIZER;
static UChar FIRST_PARAMETER[] = { 0x7b, 0x30, 0x7d };  // "{0}"
static UChar SECOND_PARAMETER[] = { 0x7b, 0x31, 0x7d };  // "{0}"
static const char *STANDARD_STYLE = "standard";

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_listformatter_cleanup() {
    delete listPatternHash;
    listPatternHash = NULL;
    return TRUE;
}

static void U_CALLCONV
uprv_deleteListFormatData(void *obj) {
    delete static_cast<ListFormatData *>(obj);
}

U_CDECL_END

static ListFormatData* loadListFormatData(const Locale& locale, const char* style, UErrorCode& errorCode);
static void getStringByKey(const UResourceBundle* rb, const char* key, UnicodeString& result, UErrorCode& errorCode);

ListFormatter::ListFormatter(const ListFormatter& other) : data(other.data) {
}

ListFormatter& ListFormatter::operator=(const ListFormatter& other) {
    data = other.data;
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

    listPatternHash->setValueDeleter(uprv_deleteListFormatData);
    ucln_common_registerCleanup(UCLN_COMMON_LIST_FORMATTER, uprv_listformatter_cleanup);

}

const ListFormatData* ListFormatter::getListFormatData(
        const Locale& locale, const char *style, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    CharString keyBuffer(locale.getName(), errorCode);
    keyBuffer.append(':', errorCode).append(style, errorCode);
    UnicodeString key(keyBuffer.data(), -1, US_INV);
    ListFormatData* result = NULL;
    {
        Mutex m(&listFormatterMutex);
        if (listPatternHash == NULL) {
            initializeHash(errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
        result = static_cast<ListFormatData*>(listPatternHash->get(key));
    }
    if (result != NULL) {
        return result;
    }
    result = loadListFormatData(locale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }

    {
        Mutex m(&listFormatterMutex);
        ListFormatData* temp = static_cast<ListFormatData*>(listPatternHash->get(key));
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

static ListFormatData* loadListFormatData(
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
    ListFormatData* result = new ListFormatData(two, start, middle, end);
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
    const ListFormatData* listFormatData = getListFormatData(tempLocale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    ListFormatter* p = new ListFormatter(listFormatData);
    if (p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return p;
}


ListFormatter::ListFormatter(const ListFormatData* listFormatterData) : data(listFormatterData) {
}

ListFormatter::~ListFormatter() {}

UnicodeString& ListFormatter::format(const UnicodeString items[], int32_t nItems,
                      UnicodeString& appendTo, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return appendTo;
    }
    if (data == NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return appendTo;
    }

    if (nItems > 0) {
        UnicodeString newString = items[0];
        if (nItems == 2) {
            addNewString(data->twoPattern, newString, items[1], errorCode);
        } else if (nItems > 2) {
            addNewString(data->startPattern, newString, items[1], errorCode);
            int32_t i;
            for (i = 2; i < nItems - 1; ++i) {
                addNewString(data->middlePattern, newString, items[i], errorCode);
            }
            addNewString(data->endPattern, newString, items[nItems - 1], errorCode);
        }
        if (U_SUCCESS(errorCode)) {
            appendTo += newString;
        }
    }
    return appendTo;
}

/**
 * Joins originalString and nextString using the pattern pat and puts the result in
 * originalString.
 */
void ListFormatter::addNewString(const UnicodeString& pat, UnicodeString& originalString,
                                 const UnicodeString& nextString, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return;
    }

    int32_t p0Offset = pat.indexOf(FIRST_PARAMETER, 3, 0);
    if (p0Offset < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int32_t p1Offset = pat.indexOf(SECOND_PARAMETER, 3, 0);
    if (p1Offset < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t i, j;

    const UnicodeString* firstString;
    const UnicodeString* secondString;
    if (p0Offset < p1Offset) {
        i = p0Offset;
        j = p1Offset;
        firstString = &originalString;
        secondString = &nextString;
    } else {
        i = p1Offset;
        j = p0Offset;
        firstString = &nextString;
        secondString = &originalString;
    }

    UnicodeString result = UnicodeString(pat, 0, i) + *firstString;
    result += UnicodeString(pat, i+3, j-i-3);
    result += *secondString;
    result += UnicodeString(pat, j+3);
    originalString = result;
}

U_NAMESPACE_END
