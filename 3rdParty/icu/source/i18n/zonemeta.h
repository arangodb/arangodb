/*
*******************************************************************************
* Copyright (C) 2007-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef ZONEMETA_H
#define ZONEMETA_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unistr.h"
#include "hash.h"

U_NAMESPACE_BEGIN

typedef struct OlsonToMetaMappingEntry {
    const UChar *mzid; // const because it's a reference to a resource bundle string.
    UDate from;
    UDate to;
} OlsonToMetaMappingEntry;

class UVector;
class TimeZone;

class U_I18N_API ZoneMeta {
public:
    /**
     * Return the canonical id for this tzid defined by CLDR, which might be the id itself.
     * If the given system tzid is not known, U_ILLEGAL_ARGUMENT_ERROR is set in the status.
     *
     * Note: this internal API supports all known system IDs and "Etc/Unknown" (which is
     * NOT a system ID).
     */
    static UnicodeString& U_EXPORT2 getCanonicalCLDRID(const UnicodeString &tzid, UnicodeString &systemID, UErrorCode& status);

    /**
     * Return the canonical id for this tzid defined by CLDR, which might be the id itself.
     * This overload method returns a persistent const UChar*, which is guranteed to persist
     * (a pointer to a resource).
     */
    static const UChar* U_EXPORT2 getCanonicalCLDRID(const UnicodeString &tzid, UErrorCode& status);

    /*
     * Conveninent method returning CLDR canonical ID for the given time zone
     */
    static const UChar* U_EXPORT2 getCanonicalCLDRID(const TimeZone& tz);

    /**
     * Return the canonical country code for this tzid.  If we have none, or if the time zone
     * is not associated with a country, return null.
     */
    static UnicodeString& U_EXPORT2 getCanonicalCountry(const UnicodeString &tzid, UnicodeString &canonicalCountry);

    /**
     * Return the country code if this is a 'single' time zone that can fallback to just
     * the country, otherwise return empty string.  (Note, one must also check the locale data
     * to see that there is a localization for the country in order to implement
     * tr#35 appendix J step 5.)
     */
    static UnicodeString& U_EXPORT2 getSingleCountry(const UnicodeString &tzid, UnicodeString &country);

    /**
     * Returns a CLDR metazone ID for the given Olson tzid and time.
     */
    static UnicodeString& U_EXPORT2 getMetazoneID(const UnicodeString &tzid, UDate date, UnicodeString &result);
    /**
     * Returns an Olson ID for the ginve metazone and region
     */
    static UnicodeString& U_EXPORT2 getZoneIdByMetazone(const UnicodeString &mzid, const UnicodeString &region, UnicodeString &result);

    static const UVector* U_EXPORT2 getMetazoneMappings(const UnicodeString &tzid);

    static const UVector* U_EXPORT2 getAvailableMetazoneIDs();

    /**
     * Returns the pointer to the persistent time zone ID string, or NULL if the given tzid is not in the
     * tz database. This method is useful when you maintain persistent zone IDs without duplication.
     */
    static const UChar* U_EXPORT2 findTimeZoneID(const UnicodeString& tzid);

    /**
     * Returns the pointer to the persistent meta zone ID string, or NULL if the given mzid is not available.
     * This method is useful when you maintain persistent meta zone IDs without duplication.
     */
    static const UChar* U_EXPORT2 findMetaZoneID(const UnicodeString& mzid);

    /**
     * Creates a custom zone for the offset
     * @param offset GMT offset in milliseconds
     * @return A custom TimeZone for the offset with normalized time zone id
     */
    static TimeZone* createCustomTimeZone(int32_t offset);

private:
    ZoneMeta(); // Prevent construction.
    static UVector* createMetazoneMappings(const UnicodeString &tzid);
    static void initAvailableMetaZoneIDs();
    static UnicodeString& formatCustomID(uint8_t hour, uint8_t min, uint8_t sec, UBool negative, UnicodeString& id);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif // ZONEMETA_H
