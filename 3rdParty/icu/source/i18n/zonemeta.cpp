/*
*******************************************************************************
* Copyright (C) 2007-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "zonemeta.h"

#include "unicode/timezone.h"
#include "unicode/ustring.h"
#include "unicode/putil.h"
#include "unicode/simpletz.h"

#include "umutex.h"
#include "uvector.h"
#include "cmemory.h"
#include "gregoimp.h"
#include "cstring.h"
#include "ucln_in.h"
#include "uassert.h"
#include "uresimp.h"
#include "uhash.h"
#include "olsontz.h"

static UMTX gZoneMetaLock = NULL;

// CLDR Canonical ID mapping table
static UHashtable *gCanonicalIDCache = NULL;
static UBool gCanonicalIDCacheInitialized = FALSE;

// Metazone mapping table
static UHashtable *gOlsonToMeta = NULL;
static UBool gOlsonToMetaInitialized = FALSE;

// Available metazone IDs vector and table
static icu::UVector *gMetaZoneIDs = NULL;
static UHashtable *gMetaZoneIDTable = NULL;
static UBool gMetaZoneIDsInitialized = FALSE;

// Country info vectors
static icu::UVector *gSingleZoneCountries = NULL;
static icu::UVector *gMultiZonesCountries = NULL;
static UBool gCountryInfoVectorsInitialized = FALSE;

U_CDECL_BEGIN

/**
 * Cleanup callback func
 */
static UBool U_CALLCONV zoneMeta_cleanup(void)
{
    umtx_destroy(&gZoneMetaLock);

    if (gCanonicalIDCache != NULL) {
        uhash_close(gCanonicalIDCache);
        gCanonicalIDCache = NULL;
    }
    gCanonicalIDCacheInitialized = FALSE;

    if (gOlsonToMeta != NULL) {
        uhash_close(gOlsonToMeta);
        gOlsonToMeta = NULL;
    }
    gOlsonToMetaInitialized = FALSE;

    if (gMetaZoneIDTable != NULL) {
        uhash_close(gMetaZoneIDTable);
    }
    // delete after closing gMetaZoneIDTable, because it holds
    // value objects held by the hashtable
    delete gMetaZoneIDs;
    gMetaZoneIDsInitialized = FALSE;

    delete gSingleZoneCountries;
    delete gMultiZonesCountries;
    gCountryInfoVectorsInitialized = FALSE;

    return TRUE;
}

/**
 * Deleter for UChar* string
 */
static void U_CALLCONV
deleteUCharString(void *obj) {
    UChar *entry = (UChar*)obj;
    uprv_free(entry);
}

/**
 * Deleter for UVector
 */
static void U_CALLCONV
deleteUVector(void *obj) {
   delete (icu::UVector*) obj;
}

/**
 * Deleter for OlsonToMetaMappingEntry
 */
static void U_CALLCONV
deleteOlsonToMetaMappingEntry(void *obj) {
    icu::OlsonToMetaMappingEntry *entry = (icu::OlsonToMetaMappingEntry*)obj;
    uprv_free(entry);
}

U_CDECL_END

U_NAMESPACE_BEGIN

#define ZID_KEY_MAX 128

static const char gMetaZones[]          = "metaZones";
static const char gMetazoneInfo[]       = "metazoneInfo";
static const char gMapTimezonesTag[]    = "mapTimezones";

static const char gTimeZoneTypes[]      = "timezoneTypes";
static const char gTypeAliasTag[]       = "typeAlias";
static const char gTypeMapTag[]         = "typeMap";
static const char gTimezoneTag[]        = "timezone";

static const char gWorldTag[]           = "001";

static const UChar gWorld[] = {0x30, 0x30, 0x31, 0x00}; // "001"

static const UChar gDefaultFrom[] = {0x31, 0x39, 0x37, 0x30, 0x2D, 0x30, 0x31, 0x2D, 0x30, 0x31,
                                     0x20, 0x30, 0x30, 0x3A, 0x30, 0x30, 0x00}; // "1970-01-01 00:00"
static const UChar gDefaultTo[]   = {0x39, 0x39, 0x39, 0x39, 0x2D, 0x31, 0x32, 0x2D, 0x33, 0x31,
                                     0x20, 0x32, 0x33, 0x3A, 0x35, 0x39, 0x00}; // "9999-12-31 23:59"

static const UChar gCustomTzPrefix[]    = {0x47, 0x4D, 0x54, 0};    // "GMT"

#define ASCII_DIGIT(c) (((c)>=0x30 && (c)<=0x39) ? (c)-0x30 : -1)

/*
 * Convert a date string used by metazone mappings to UDate.
 * The format used by CLDR metazone mapping is "yyyy-MM-dd HH:mm".
 */
static UDate
parseDate (const UChar *text, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    int32_t len = u_strlen(text);
    if (len != 16 && len != 10) {
        // It must be yyyy-MM-dd HH:mm (length 16) or yyyy-MM-dd (length 10)
        status = U_INVALID_FORMAT_ERROR;
        return 0;
    }

    int32_t year = 0, month = 0, day = 0, hour = 0, min = 0, n;
    int32_t idx;

    // "yyyy" (0 - 3)
    for (idx = 0; idx <= 3 && U_SUCCESS(status); idx++) {
        n = ASCII_DIGIT((int32_t)text[idx]);
        if (n >= 0) {
            year = 10*year + n;
        } else {
            status = U_INVALID_FORMAT_ERROR;
        }
    }
    // "MM" (5 - 6)
    for (idx = 5; idx <= 6 && U_SUCCESS(status); idx++) {
        n = ASCII_DIGIT((int32_t)text[idx]);
        if (n >= 0) {
            month = 10*month + n;
        } else {
            status = U_INVALID_FORMAT_ERROR;
        }
    }
    // "dd" (8 - 9)
    for (idx = 8; idx <= 9 && U_SUCCESS(status); idx++) {
        n = ASCII_DIGIT((int32_t)text[idx]);
        if (n >= 0) {
            day = 10*day + n;
        } else {
            status = U_INVALID_FORMAT_ERROR;
        }
    }
    if (len == 16) {
        // "HH" (11 - 12)
        for (idx = 11; idx <= 12 && U_SUCCESS(status); idx++) {
            n = ASCII_DIGIT((int32_t)text[idx]);
            if (n >= 0) {
                hour = 10*hour + n;
            } else {
                status = U_INVALID_FORMAT_ERROR;
            }
        }
        // "mm" (14 - 15)
        for (idx = 14; idx <= 15 && U_SUCCESS(status); idx++) {
            n = ASCII_DIGIT((int32_t)text[idx]);
            if (n >= 0) {
                min = 10*min + n;
            } else {
                status = U_INVALID_FORMAT_ERROR;
            }
        }
    }

    if (U_SUCCESS(status)) {
        UDate date = Grego::fieldsToDay(year, month - 1, day) * U_MILLIS_PER_DAY
            + hour * U_MILLIS_PER_HOUR + min * U_MILLIS_PER_MINUTE;
        return date;
    }
    return 0;
}

const UChar* U_EXPORT2
ZoneMeta::getCanonicalCLDRID(const UnicodeString &tzid, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }

    int32_t len = tzid.length();
    if (len > ZID_KEY_MAX) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    // Checking the cached results
    UBool initialized;
    UMTX_CHECK(&gZoneMetaLock, gCanonicalIDCacheInitialized, initialized);
    if (!initialized) {
        // Create empty hashtable
        umtx_lock(&gZoneMetaLock);
        {
            if (!gCanonicalIDCacheInitialized) {
                gCanonicalIDCache = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
                if (gCanonicalIDCache == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                }
                if (U_FAILURE(status)) {
                    gCanonicalIDCache = NULL;
                    return NULL;
                }
                // No key/value deleters - keys/values are from a resource bundle
                gCanonicalIDCacheInitialized = TRUE;
                ucln_i18n_registerCleanup(UCLN_I18N_ZONEMETA, zoneMeta_cleanup);
            }
        }
        umtx_unlock(&gZoneMetaLock);
    }

    const UChar *canonicalID = NULL;

    UErrorCode tmpStatus = U_ZERO_ERROR;
    UChar utzid[ZID_KEY_MAX + 1];
    tzid.extract(utzid, ZID_KEY_MAX + 1, tmpStatus);
    U_ASSERT(tmpStatus == U_ZERO_ERROR);    // we checked the length of tzid already

    // Check if it was already cached
    umtx_lock(&gZoneMetaLock);
    {
        canonicalID = (const UChar *)uhash_get(gCanonicalIDCache, utzid);
    }
    umtx_unlock(&gZoneMetaLock);

    if (canonicalID != NULL) {
        return canonicalID;
    }

    // If not, resolve CLDR canonical ID with resource data
    UBool isInputCanonical = FALSE;
    char id[ZID_KEY_MAX + 1];
    const UChar* idChars = tzid.getBuffer();

    u_UCharsToChars(idChars,id,len);
    id[len] = (char) 0; // Make sure it is null terminated.

    // replace '/' with ':'
    char *p = id;
    while (*p++) {
        if (*p == '/') {
            *p = ':';
        }
    }

    UResourceBundle *top = ures_openDirect(NULL, gTimeZoneTypes, &tmpStatus);
    UResourceBundle *rb = ures_getByKey(top, gTypeMapTag, NULL, &tmpStatus);
    ures_getByKey(rb, gTimezoneTag, rb, &tmpStatus);
    ures_getByKey(rb, id, rb, &tmpStatus);
    if (U_SUCCESS(tmpStatus)) {
        // type entry (canonical) found
        // the input is the canonical ID. resolve to const UChar*
        canonicalID = TimeZone::findID(tzid);
        isInputCanonical = TRUE;
    }

    if (canonicalID == NULL) {
        // If a map element not found, then look for an alias
        tmpStatus = U_ZERO_ERROR;
        ures_getByKey(top, gTypeAliasTag, rb, &tmpStatus);
        ures_getByKey(rb, gTimezoneTag, rb, &tmpStatus);
        const UChar *canonical = ures_getStringByKey(rb,id,NULL,&tmpStatus);
        if (U_SUCCESS(tmpStatus)) {
            // canonical map found
            canonicalID = canonical;
        }

        if (canonicalID == NULL) {
            // Dereference the input ID using the tz data
            const UChar *derefer = TimeZone::dereferOlsonLink(tzid);
            if (derefer == NULL) {
                status = U_ILLEGAL_ARGUMENT_ERROR;
            } else {
                len = u_strlen(derefer);
                u_UCharsToChars(derefer,id,len);
                id[len] = (char) 0; // Make sure it is null terminated.

                // replace '/' with ':'
                char *p = id;
                while (*p++) {
                    if (*p == '/') {
                        *p = ':';
                    }
                }

                // If a dereference turned something up then look for an alias.
                // rb still points to the alias table, so we don't have to go looking
                // for it.
                tmpStatus = U_ZERO_ERROR;
                canonical = ures_getStringByKey(rb,id,NULL,&tmpStatus);
                if (U_SUCCESS(tmpStatus)) {
                    // canonical map for the dereferenced ID found
                    canonicalID = canonical;
                } else {
                    canonicalID = derefer;
                    isInputCanonical = TRUE;
                }
            }
        }
    }
    ures_close(rb);
    ures_close(top);

    if (U_SUCCESS(status)) {
        U_ASSERT(canonicalID != NULL);  // canocanilD must be non-NULL here

        // Put the resolved canonical ID to the cache
        umtx_lock(&gZoneMetaLock);
        {
            const UChar* idInCache = (const UChar *)uhash_get(gCanonicalIDCache, utzid);
            if (idInCache == NULL) {
                const UChar* key = ZoneMeta::findTimeZoneID(tzid);
                U_ASSERT(key != NULL);
                if (key != NULL) {
                    idInCache = (const UChar *)uhash_put(gCanonicalIDCache, (void *)key, (void *)canonicalID, &status);
                    U_ASSERT(idInCache == NULL);
                }
            }
            if (U_SUCCESS(status) && isInputCanonical) {
                // Also put canonical ID itself into the cache if not exist
                const UChar *canonicalInCache = (const UChar*)uhash_get(gCanonicalIDCache, canonicalID);
                if (canonicalInCache == NULL) {
                    canonicalInCache = (const UChar *)uhash_put(gCanonicalIDCache, (void *)canonicalID, (void *)canonicalID, &status);
                    U_ASSERT(canonicalInCache == NULL);
                }
            }
        }
        umtx_unlock(&gZoneMetaLock);
    }

    return canonicalID;
}

UnicodeString& U_EXPORT2
ZoneMeta::getCanonicalCLDRID(const UnicodeString &tzid, UnicodeString &systemID, UErrorCode& status) {
    const UChar *canonicalID = getCanonicalCLDRID(tzid, status);
    if (U_FAILURE(status) || canonicalID == NULL) {
        systemID.setToBogus();
        return systemID;
    }
    systemID.setTo(TRUE, canonicalID, -1);
    return systemID;
}

const UChar* U_EXPORT2
ZoneMeta::getCanonicalCLDRID(const TimeZone& tz) {
    if (dynamic_cast<const OlsonTimeZone *>(&tz) != NULL) {
        // short cut for OlsonTimeZone
        const OlsonTimeZone *otz = (const OlsonTimeZone*)&tz;
        return otz->getCanonicalID();
    }
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString tzID;
    return getCanonicalCLDRID(tz.getID(tzID), status);
}



UnicodeString& U_EXPORT2
ZoneMeta::getCanonicalCountry(const UnicodeString &tzid, UnicodeString &canonicalCountry) {
    const UChar *region = TimeZone::getRegion(tzid);
    if (region != NULL && u_strcmp(gWorld, region) != 0) {
        canonicalCountry.setTo(region, -1);
    } else {
        canonicalCountry.setToBogus();
    }
    return canonicalCountry;
}

UnicodeString& U_EXPORT2
ZoneMeta::getSingleCountry(const UnicodeString &tzid, UnicodeString &country) {
    // Get canonical country for the zone
    const UChar *region = TimeZone::getRegion(tzid);
    if (region == NULL || u_strcmp(gWorld, region) == 0) {
        // special case - unknown or "001"
        country.setToBogus();
        return country;
    }

    // Checking the cached results
    UErrorCode status = U_ZERO_ERROR;
    UBool initialized;
    UMTX_CHECK(&gZoneMetaLock, gCountryInfoVectorsInitialized, initialized);
    if (!initialized) {
        // Create empty vectors
        umtx_lock(&gZoneMetaLock);
        {
            if (!gCountryInfoVectorsInitialized) {
                // No deleters for these UVectors, it's a reference to a resource bundle string.
                gSingleZoneCountries = new UVector(NULL, uhash_compareUChars, status);
                if (gSingleZoneCountries == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                }
                gMultiZonesCountries = new UVector(NULL, uhash_compareUChars, status);
                if (gMultiZonesCountries == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                }

                if (U_SUCCESS(status)) {
                    gCountryInfoVectorsInitialized = TRUE;
                } else {
                    delete gSingleZoneCountries;
                    delete gMultiZonesCountries;
                }
            }
        }
        umtx_unlock(&gZoneMetaLock);

        if (U_FAILURE(status)) {
            country.setToBogus();
            return country;
        }
        U_ASSERT(gSingleZoneCountries != NULL);
        U_ASSERT(gMultiZonesCountries != NULL);
    }

    // Check if it was already cached
    UBool cached = FALSE;
    UBool multiZones = FALSE;
    umtx_lock(&gZoneMetaLock);
    {
        multiZones = cached = gMultiZonesCountries->contains((void*)region);
        if (!multiZones) {
            cached = gSingleZoneCountries->contains((void*)region);
        }
    }
    umtx_unlock(&gZoneMetaLock);

    if (!cached) {
        // We need to go through all zones associated with the region.
        // This is relatively heavy operation.

        U_ASSERT(u_strlen(region) == 2);

        char buf[] = {0, 0, 0};
        u_UCharsToChars(region, buf, 2);

        StringEnumeration *ids = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL_LOCATION, buf, NULL, status);
        int32_t idsLen = ids->count(status);
        if (U_SUCCESS(status) && idsLen > 1) {
            // multiple canonical zones are available for the region
            multiZones = TRUE;
        }
        if (U_FAILURE(status)) {
            // no single country by default for any error cases
            multiZones = TRUE;
        }
        delete ids;

        // Cache the result
        umtx_lock(&gZoneMetaLock);
        {
            UErrorCode ec = U_ZERO_ERROR;
            if (multiZones) {
                if (!gMultiZonesCountries->contains((void*)region)) {
                    gMultiZonesCountries->addElement((void*)region, ec);
                }
            } else {
                if (!gSingleZoneCountries->contains((void*)region)) {
                    gSingleZoneCountries->addElement((void*)region, ec);
                }
            }
        }
        umtx_unlock(&gZoneMetaLock);
    }

    if (multiZones) {
        country.setToBogus();
    } else {
        country.setTo(region, -1);
    }
    return country;
}

UnicodeString& U_EXPORT2
ZoneMeta::getMetazoneID(const UnicodeString &tzid, UDate date, UnicodeString &result) {
    UBool isSet = FALSE;
    const UVector *mappings = getMetazoneMappings(tzid);
    if (mappings != NULL) {
        for (int32_t i = 0; i < mappings->size(); i++) {
            OlsonToMetaMappingEntry *mzm = (OlsonToMetaMappingEntry*)mappings->elementAt(i);
            if (mzm->from <= date && mzm->to > date) {
                result.setTo(mzm->mzid, -1);
                isSet = TRUE;
                break;
            }
        }
    }
    if (!isSet) {
        result.setToBogus();
    }
    return result;
}

const UVector* U_EXPORT2
ZoneMeta::getMetazoneMappings(const UnicodeString &tzid) {
    UErrorCode status = U_ZERO_ERROR;
    UChar tzidUChars[ZID_KEY_MAX + 1];
    tzid.extract(tzidUChars, ZID_KEY_MAX + 1, status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
        return NULL;
    }

    UBool initialized;
    UMTX_CHECK(&gZoneMetaLock, gOlsonToMetaInitialized, initialized);
    if (!initialized) {
        UHashtable *tmpOlsonToMeta = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
        if (U_FAILURE(status)) {
            return NULL;
        }
        uhash_setKeyDeleter(tmpOlsonToMeta, deleteUCharString);
        uhash_setValueDeleter(tmpOlsonToMeta, deleteUVector);

        umtx_lock(&gZoneMetaLock);
        {
            if (!gOlsonToMetaInitialized) {
                gOlsonToMeta = tmpOlsonToMeta;
                tmpOlsonToMeta = NULL;
                gOlsonToMetaInitialized = TRUE;
            }
        }
        umtx_unlock(&gZoneMetaLock);

        // OK to call the following multiple times with the same function
        ucln_i18n_registerCleanup(UCLN_I18N_ZONEMETA, zoneMeta_cleanup);
        if (tmpOlsonToMeta != NULL) {
            uhash_close(tmpOlsonToMeta);
        }
    }

    // get the mapping from cache
    const UVector *result = NULL;

    umtx_lock(&gZoneMetaLock);
    {
        result = (UVector*) uhash_get(gOlsonToMeta, tzidUChars);
    }
    umtx_unlock(&gZoneMetaLock);

    if (result != NULL) {
        return result;
    }

    // miss the cache - create new one
    UVector *tmpResult = createMetazoneMappings(tzid);
    if (tmpResult == NULL) {
        // not available
        return NULL;
    }

    // put the new one into the cache
    umtx_lock(&gZoneMetaLock);
    {
        // make sure it's already created
        result = (UVector*) uhash_get(gOlsonToMeta, tzidUChars);
        if (result == NULL) {
            // add the one just created
            int32_t tzidLen = tzid.length() + 1;
            UChar *key = (UChar*)uprv_malloc(tzidLen * sizeof(UChar));
            if (key == NULL) {
                // memory allocation error..  just return NULL
                result = NULL;
                delete tmpResult;
            } else {
                tzid.extract(key, tzidLen, status);
                uhash_put(gOlsonToMeta, key, tmpResult, &status);
                if (U_FAILURE(status)) {
                    // delete the mapping
                    result = NULL;
                    delete tmpResult;
                } else {
                    result = tmpResult;
                }
            }
        } else {
            // another thread already put the one
            delete tmpResult;
        }
    }
    umtx_unlock(&gZoneMetaLock);

    return result;
}

UVector*
ZoneMeta::createMetazoneMappings(const UnicodeString &tzid) {
    UVector *mzMappings = NULL;
    UErrorCode status = U_ZERO_ERROR;

    UnicodeString canonicalID;
    UResourceBundle *rb = ures_openDirect(NULL, gMetaZones, &status);
    ures_getByKey(rb, gMetazoneInfo, rb, &status);
    getCanonicalCLDRID(tzid, canonicalID, status);

    if (U_SUCCESS(status)) {
        char tzKey[ZID_KEY_MAX + 1];
        int32_t tzKeyLen = canonicalID.extract(0, canonicalID.length(), tzKey, sizeof(tzKey), US_INV);
        tzKey[tzKeyLen] = 0;

        // tzid keys are using ':' as separators
        char *p = tzKey;
        while (*p) {
            if (*p == '/') {
                *p = ':';
            }
            p++;
        }

        ures_getByKey(rb, tzKey, rb, &status);

        if (U_SUCCESS(status)) {
            UResourceBundle *mz = NULL;
            while (ures_hasNext(rb)) {
                mz = ures_getNextResource(rb, mz, &status);

                const UChar *mz_name = ures_getStringByIndex(mz, 0, NULL, &status);
                const UChar *mz_from = gDefaultFrom;
                const UChar *mz_to = gDefaultTo;

                if (ures_getSize(mz) == 3) {
                    mz_from = ures_getStringByIndex(mz, 1, NULL, &status);
                    mz_to   = ures_getStringByIndex(mz, 2, NULL, &status);
                }

                if(U_FAILURE(status)){
                    status = U_ZERO_ERROR;
                    continue;
                }
                // We do not want to use SimpleDateformat to parse boundary dates,
                // because this code could be triggered by the initialization code
                // used by SimpleDateFormat.
                UDate from = parseDate(mz_from, status);
                UDate to = parseDate(mz_to, status);
                if (U_FAILURE(status)) {
                    status = U_ZERO_ERROR;
                    continue;
                }

                OlsonToMetaMappingEntry *entry = (OlsonToMetaMappingEntry*)uprv_malloc(sizeof(OlsonToMetaMappingEntry));
                if (entry == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                entry->mzid = mz_name;
                entry->from = from;
                entry->to = to;

                if (mzMappings == NULL) {
                    mzMappings = new UVector(deleteOlsonToMetaMappingEntry, NULL, status);
                    if (U_FAILURE(status)) {
                        delete mzMappings;
                        deleteOlsonToMetaMappingEntry(entry);
                        uprv_free(entry);
                        break;
                    }
                }

                mzMappings->addElement(entry, status);
                if (U_FAILURE(status)) {
                    break;
                }
            }
            ures_close(mz);
            if (U_FAILURE(status)) {
                if (mzMappings != NULL) {
                    delete mzMappings;
                    mzMappings = NULL;
                }
            }
        }
    }
    ures_close(rb);
    return mzMappings;
}

UnicodeString& U_EXPORT2
ZoneMeta::getZoneIdByMetazone(const UnicodeString &mzid, const UnicodeString &region, UnicodeString &result) {
    UErrorCode status = U_ZERO_ERROR;
    const UChar *tzid = NULL;
    int32_t tzidLen = 0;
    char keyBuf[ZID_KEY_MAX + 1];
    int32_t keyLen = 0;

    if (mzid.length() > ZID_KEY_MAX) {
        result.setToBogus();
        return result;
    }

    keyLen = mzid.extract(0, mzid.length(), keyBuf, ZID_KEY_MAX + 1, US_INV);
    keyBuf[keyLen] = 0;

    UResourceBundle *rb = ures_openDirect(NULL, gMetaZones, &status);
    ures_getByKey(rb, gMapTimezonesTag, rb, &status);
    ures_getByKey(rb, keyBuf, rb, &status);

    if (U_SUCCESS(status)) {
        // check region mapping
        if (region.length() == 2 || region.length() == 3) {
            keyLen = region.extract(0, region.length(), keyBuf, ZID_KEY_MAX + 1, US_INV);
            keyBuf[keyLen] = 0;
            tzid = ures_getStringByKey(rb, keyBuf, &tzidLen, &status);
            if (status == U_MISSING_RESOURCE_ERROR) {
                status = U_ZERO_ERROR;
            }
        }
        if (U_SUCCESS(status) && tzid == NULL) {
            // try "001"
            tzid = ures_getStringByKey(rb, gWorldTag, &tzidLen, &status);
        }
    }
    ures_close(rb);

    if (tzid == NULL) {
        result.setToBogus();
    } else {
        result.setTo(tzid, tzidLen);
    }

    return result;
}

void
ZoneMeta::initAvailableMetaZoneIDs () {
    UBool initialized;
    UMTX_CHECK(&gZoneMetaLock, gMetaZoneIDsInitialized, initialized);
    if (!initialized) {
        umtx_lock(&gZoneMetaLock);
        {
            if (!gMetaZoneIDsInitialized) {
                UErrorCode status = U_ZERO_ERROR;
                UHashtable *metaZoneIDTable = uhash_open(uhash_hashUnicodeString, uhash_compareUnicodeString, NULL, &status);
                uhash_setKeyDeleter(metaZoneIDTable, uprv_deleteUObject);
                // No valueDeleter, because the vector maintain the value objects
                UVector *metaZoneIDs = NULL;
                if (U_SUCCESS(status)) {
                    metaZoneIDs = new UVector(NULL, uhash_compareUChars, status);
                    if (metaZoneIDs == NULL) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                    }
                } else {
                    uhash_close(metaZoneIDTable);
                }
                if (U_SUCCESS(status)) {
                    U_ASSERT(metaZoneIDs != NULL);
                    metaZoneIDs->setDeleter(uprv_free);

                    UResourceBundle *rb = ures_openDirect(NULL, gMetaZones, &status);
                    UResourceBundle *bundle = ures_getByKey(rb, gMapTimezonesTag, NULL, &status);
                    UResourceBundle res;
                    ures_initStackObject(&res);
                    while (U_SUCCESS(status) && ures_hasNext(bundle)) {
                        ures_getNextResource(bundle, &res, &status);
                        if (U_FAILURE(status)) {
                            break;
                        }
                        const char *mzID = ures_getKey(&res);
                        int32_t len = uprv_strlen(mzID);
                        UChar *uMzID = (UChar*)uprv_malloc(sizeof(UChar) * (len + 1));
                        if (uMzID == NULL) {
                            status = U_MEMORY_ALLOCATION_ERROR;
                            break;
                        }
                        u_charsToUChars(mzID, uMzID, len);
                        uMzID[len] = 0;
                        UnicodeString *usMzID = new UnicodeString(uMzID);
                        if (uhash_get(metaZoneIDTable, usMzID) == NULL) {
                            metaZoneIDs->addElement((void *)uMzID, status);
                            uhash_put(metaZoneIDTable, (void *)usMzID, (void *)uMzID, &status);
                        } else {
                            uprv_free(uMzID);
                            delete usMzID;
                        }
                    }
                    if (U_SUCCESS(status)) {
                        gMetaZoneIDs = metaZoneIDs;
                        gMetaZoneIDTable = metaZoneIDTable;
                        gMetaZoneIDsInitialized = TRUE;
                    } else {
                        uhash_close(metaZoneIDTable);
                        delete metaZoneIDs;
                    }
                    ures_close(&res);
                    ures_close(bundle);
                    ures_close(rb);
                }
            }
        }
        umtx_unlock(&gZoneMetaLock);
    }
}

const UVector*
ZoneMeta::getAvailableMetazoneIDs() {
    initAvailableMetaZoneIDs();
    return gMetaZoneIDs;
}

const UChar*
ZoneMeta::findMetaZoneID(const UnicodeString& mzid) {
    initAvailableMetaZoneIDs();
    return (const UChar*)uhash_get(gMetaZoneIDTable, &mzid);
}

const UChar*
ZoneMeta::findTimeZoneID(const UnicodeString& tzid) {
    return TimeZone::findID(tzid);
}


TimeZone*
ZoneMeta::createCustomTimeZone(int32_t offset) {
    UBool negative = FALSE;
    int32_t tmp = offset;
    if (offset < 0) {
        negative = TRUE;
        tmp = -offset;
    }
    int32_t hour, min, sec;

    tmp /= 1000;
    sec = tmp % 60;
    tmp /= 60;
    min = tmp % 60;
    hour = tmp / 60;

    UnicodeString zid;
    formatCustomID(hour, min, sec, negative, zid);
    return new SimpleTimeZone(offset, zid);
}

UnicodeString&
ZoneMeta::formatCustomID(uint8_t hour, uint8_t min, uint8_t sec, UBool negative, UnicodeString& id) {
    // Create normalized time zone ID - GMT[+|-]HH:mm[:ss]
    id.setTo(gCustomTzPrefix, -1);
    if (hour != 0 || min != 0) {
        if (negative) {
            id.append(0x2D);    // '-'
        } else {
            id.append(0x2B);    // '+'
        }
        // Always use US-ASCII digits
        id.append(0x30 + (hour%100)/10);
        id.append(0x30 + (hour%10));
        id.append(0x3A);    // ':'
        id.append(0x30 + (min%100)/10);
        id.append(0x30 + (min%10));
        if (sec != 0) {
            id.append(0x3A);    // ':'
            id.append(0x30 + (sec%100)/10);
            id.append(0x30 + (sec%10));
        }
    }
    return id;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
