/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
*
* File REGION.CPP
*
* Modification History:*
*   Date        Name        Description
* 01/15/13      Emmons      Original Port from ICU4J
********************************************************************************
*/

/**
 * \file
 * \brief C++ API: Region classes (territory containment)
 */

#include "unicode/region.h"
#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "unicode/ures.h"
#include "unicode/decimfmt.h"
#include "ucln_in.h"
#include "cstring.h"
#include "mutex.h"
#include "uhash.h"
#include "umutex.h"
#include "uresimp.h"
#include "region_impl.h"

#if !UCONFIG_NO_FORMATTING


U_CDECL_BEGIN

static void U_CALLCONV
deleteRegion(void *obj) {
    delete (icu::Region *)obj;
}

/**
 * Cleanup callback func
 */
static UBool U_CALLCONV region_cleanup(void)
{
    icu::Region::cleanupRegionData();

    return TRUE;
}

U_CDECL_END

U_NAMESPACE_BEGIN

static UInitOnce gRegionDataInitOnce = U_INITONCE_INITIALIZER;
static UVector* availableRegions[URGN_LIMIT];

static UHashtable *regionAliases;
static UHashtable *regionIDMap;
static UHashtable *numericCodeMap;

static const UChar UNKNOWN_REGION_ID [] = { 0x5A, 0x5A, 0 };  /* "ZZ" */
static const UChar OUTLYING_OCEANIA_REGION_ID [] = { 0x51, 0x4F, 0 };  /* "QO" */
static const UChar WORLD_ID [] = { 0x30, 0x30, 0x31, 0 };  /* "001" */

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RegionNameEnumeration)

/*
 * Initializes the region data from the ICU resource bundles.  The region data
 * contains the basic relationships such as which regions are known, what the numeric
 * codes are, any known aliases, and the territory containment data.
 *
 * If the region data has already loaded, then this method simply returns without doing
 * anything meaningful.
 */
void Region::loadRegionData(UErrorCode &status) {
    LocalPointer<DecimalFormat> df(new DecimalFormat(status));
    if (U_FAILURE(status)) {
        return;
    }
    if (df == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    df->setParseIntegerOnly(TRUE);

    regionIDMap = uhash_open(uhash_hashUnicodeString, uhash_compareUnicodeString, NULL, &status);
    if (U_FAILURE(status)) {
        return;
    }
    if (regionIDMap == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uhash_setValueDeleter(regionIDMap, deleteRegion);

    numericCodeMap = uhash_open(uhash_hashLong,uhash_compareLong,NULL,&status);

    regionAliases = uhash_open(uhash_hashUnicodeString,uhash_compareUnicodeString,NULL,&status);
    if (U_FAILURE(status)) {
        return;
    }
    if (regionAliases == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uhash_setKeyDeleter(regionAliases,uprv_deleteUObject);

    LocalUResourceBundlePointer rb(ures_openDirect(NULL,"metadata",&status));
    LocalUResourceBundlePointer regionCodes(ures_getByKey(rb.getAlias(),"regionCodes",NULL,&status));
    LocalUResourceBundlePointer territoryAlias(ures_getByKey(rb.getAlias(),"territoryAlias",NULL,&status));

    LocalUResourceBundlePointer rb2(ures_openDirect(NULL,"supplementalData",&status));
    LocalUResourceBundlePointer codeMappings(ures_getByKey(rb2.getAlias(),"codeMappings",NULL,&status));

    LocalUResourceBundlePointer territoryContainment(ures_getByKey(rb2.getAlias(),"territoryContainment",NULL,&status));
    LocalUResourceBundlePointer worldContainment(ures_getByKey(territoryContainment.getAlias(),"001",NULL,&status));
    LocalUResourceBundlePointer groupingContainment(ures_getByKey(territoryContainment.getAlias(),"grouping",NULL,&status));

    UVector *continents = new UVector(uprv_deleteUObject, uhash_compareUnicodeString, status);

    while ( ures_hasNext(worldContainment.getAlias()) ) {
        UnicodeString *continentName = new UnicodeString(ures_getNextUnicodeString(worldContainment.getAlias(),NULL,&status));
        continents->addElement(continentName,status);
    }

    UVector *groupings = new UVector(uprv_deleteUObject, uhash_compareUnicodeString, status);
    while ( ures_hasNext(groupingContainment.getAlias()) ) {
        UnicodeString *groupingName = new UnicodeString(ures_getNextUnicodeString(groupingContainment.getAlias(),NULL,&status));
        groupings->addElement(groupingName,status);
    }

    while ( ures_hasNext(regionCodes.getAlias()) ) {
        UnicodeString regionID = ures_getNextUnicodeString(regionCodes.getAlias(), NULL, &status);
        Region *r = new Region();
        r->idStr = regionID;
        r->idStr.extract(0,r->idStr.length(),r->id,sizeof(r->id),US_INV);
        r->type = URGN_TERRITORY; // Only temporary - figure out the real type later once the aliases are known.

        uhash_put(regionIDMap,(void *)&(r->idStr),(void *)r,&status);
        Formattable result;
        UErrorCode ps = U_ZERO_ERROR;
        df->parse(r->idStr,result,ps);
        if ( U_SUCCESS(ps) ) {
            r->code = result.getLong(); // Convert string to number
            uhash_iput(numericCodeMap,r->code,(void *)r,&status);
            r->type = URGN_SUBCONTINENT;
        } else {
            r->code = -1;
        }
    }


    // Process the territory aliases
    while ( ures_hasNext(territoryAlias.getAlias()) ) {
        UResourceBundle *res = ures_getNextResource(territoryAlias.getAlias(),NULL,&status);
        const char *aliasFrom = ures_getKey(res);
        UnicodeString* aliasFromStr = new UnicodeString(aliasFrom, -1, US_INV);
        UnicodeString aliasTo = ures_getUnicodeString(res,&status);
        ures_close(res);

        Region *aliasToRegion = (Region *) uhash_get(regionIDMap,&aliasTo);
        Region *aliasFromRegion = (Region *)uhash_get(regionIDMap,aliasFromStr);

        if ( aliasToRegion != NULL && aliasFromRegion == NULL ) { // This is just an alias from some string to a region
            uhash_put(regionAliases,(void *)aliasFromStr, (void *)aliasToRegion,&status);
        } else {
            if ( aliasFromRegion == NULL ) { // Deprecated region code not in the master codes list - so need to create a deprecated region for it.
                aliasFromRegion = new Region();
                aliasFromRegion->idStr.setTo(*aliasFromStr);
                aliasFromRegion->idStr.extract(0,aliasFromRegion->idStr.length(),aliasFromRegion->id,sizeof(aliasFromRegion->id),US_INV);
                uhash_put(regionIDMap,(void *)&(aliasFromRegion->idStr),(void *)aliasFromRegion,&status);
                Formattable result;
                UErrorCode ps = U_ZERO_ERROR;
                df->parse(aliasFromRegion->idStr,result,ps);
                if ( U_SUCCESS(ps) ) {
                    aliasFromRegion->code = result.getLong(); // Convert string to number
                    uhash_iput(numericCodeMap,aliasFromRegion->code,(void *)aliasFromRegion,&status);
                } else {
                    aliasFromRegion->code = -1;
                }
                aliasFromRegion->type = URGN_DEPRECATED;
            } else {
                aliasFromRegion->type = URGN_DEPRECATED;
            }
            delete aliasFromStr;

            aliasFromRegion->preferredValues = new UVector(uprv_deleteUObject, uhash_compareUnicodeString, status);
            UnicodeString currentRegion;
            currentRegion.remove();
            for (int32_t i = 0 ; i < aliasTo.length() ; i++ ) {
                if ( aliasTo.charAt(i) != 0x0020 ) {
                    currentRegion.append(aliasTo.charAt(i));
                }
                if ( aliasTo.charAt(i) == 0x0020 || i+1 == aliasTo.length() ) {
                    Region *target = (Region *)uhash_get(regionIDMap,(void *)&currentRegion);
                    if (target) {
                        UnicodeString *preferredValue = new UnicodeString(target->idStr);
                        aliasFromRegion->preferredValues->addElement((void *)preferredValue,status);
                    }
                    currentRegion.remove();
                }
            }
        }
    }

    // Process the code mappings - This will allow us to assign numeric codes to most of the territories.
    while ( ures_hasNext(codeMappings.getAlias()) ) {
        UResourceBundle *mapping = ures_getNextResource(codeMappings.getAlias(),NULL,&status);
        if ( ures_getType(mapping) == URES_ARRAY && ures_getSize(mapping) == 3) {
            UnicodeString codeMappingID = ures_getUnicodeStringByIndex(mapping,0,&status);
            UnicodeString codeMappingNumber = ures_getUnicodeStringByIndex(mapping,1,&status);
            UnicodeString codeMapping3Letter = ures_getUnicodeStringByIndex(mapping,2,&status);

            Region *r = (Region *)uhash_get(regionIDMap,(void *)&codeMappingID);
            if ( r ) {
                Formattable result;
                UErrorCode ps = U_ZERO_ERROR;
                df->parse(codeMappingNumber,result,ps);
                if ( U_SUCCESS(ps) ) {
                    r->code = result.getLong(); // Convert string to number
                    uhash_iput(numericCodeMap,r->code,(void *)r,&status);
                }
                UnicodeString *code3 = new UnicodeString(codeMapping3Letter);
                uhash_put(regionAliases,(void *)code3, (void *)r,&status);
            }
        }
        ures_close(mapping);
    }

    // Now fill in the special cases for WORLD, UNKNOWN, CONTINENTS, and GROUPINGS
    Region *r;
	UnicodeString WORLD_ID_STRING(WORLD_ID);
    r = (Region *) uhash_get(regionIDMap,(void *)&WORLD_ID_STRING);
    if ( r ) {
        r->type = URGN_WORLD;
    }

	UnicodeString UNKNOWN_REGION_ID_STRING(UNKNOWN_REGION_ID);
    r = (Region *) uhash_get(regionIDMap,(void *)&UNKNOWN_REGION_ID_STRING);
    if ( r ) {
        r->type = URGN_UNKNOWN;
    }

    for ( int32_t i = 0 ; i < continents->size() ; i++ ) {
        r = (Region *) uhash_get(regionIDMap,(void *)continents->elementAt(i));
        if ( r ) {
            r->type = URGN_CONTINENT;
        }
    }
    delete continents;

    for ( int32_t i = 0 ; i < groupings->size() ; i++ ) {
        r = (Region *) uhash_get(regionIDMap,(void *)groupings->elementAt(i));
        if ( r ) {
            r->type = URGN_GROUPING;
        }
    }
    delete groupings;

    // Special case: The region code "QO" (Outlying Oceania) is a subcontinent code added by CLDR
    // even though it looks like a territory code.  Need to handle it here.

	UnicodeString OUTLYING_OCEANIA_REGION_ID_STRING(OUTLYING_OCEANIA_REGION_ID);
    r = (Region *) uhash_get(regionIDMap,(void *)&OUTLYING_OCEANIA_REGION_ID_STRING);
    if ( r ) {
        r->type = URGN_SUBCONTINENT;
    }

    // Load territory containment info from the supplemental data.
    while ( ures_hasNext(territoryContainment.getAlias()) ) {
        UResourceBundle *mapping = ures_getNextResource(territoryContainment.getAlias(),NULL,&status);
        const char *parent = ures_getKey(mapping);
        if (uprv_strcmp(parent, "containedGroupings") == 0 || uprv_strcmp(parent, "deprecated") == 0) {
            ures_close(mapping);
            continue; // handle new pseudo-parent types added in ICU data per cldrbug 7808; for now just skip.
            // #11232 is to do something useful with these.
        }
        UnicodeString parentStr = UnicodeString(parent, -1 , US_INV);
        Region *parentRegion = (Region *) uhash_get(regionIDMap,(void *)&parentStr);

        for ( int j = 0 ; j < ures_getSize(mapping); j++ ) {
            UnicodeString child = ures_getUnicodeStringByIndex(mapping,j,&status);
            Region *childRegion = (Region *) uhash_get(regionIDMap,(void *)&child);
            if ( parentRegion != NULL && childRegion != NULL ) {

                // Add the child region to the set of regions contained by the parent
                if (parentRegion->containedRegions == NULL) {
                    parentRegion->containedRegions = new UVector(uprv_deleteUObject, uhash_compareUnicodeString, status);
                }

                UnicodeString *childStr = new UnicodeString();
                childStr->fastCopyFrom(childRegion->idStr);
                parentRegion->containedRegions->addElement((void *)childStr,status);

                // Set the parent region to be the containing region of the child.
                // Regions of type GROUPING can't be set as the parent, since another region
                // such as a SUBCONTINENT, CONTINENT, or WORLD must always be the parent.
                if ( parentRegion->type != URGN_GROUPING) {
                    childRegion->containingRegion = parentRegion;
                }
            }
        }
        ures_close(mapping);
    }

    // Create the availableRegions lists
    int32_t pos = -1;
    while ( const UHashElement* element = uhash_nextElement(regionIDMap,&pos)) {
        Region *ar = (Region *)element->value.pointer;
        if ( availableRegions[ar->type] == NULL ) {
            availableRegions[ar->type] = new UVector(uprv_deleteUObject, uhash_compareUnicodeString, status);
        }
        UnicodeString *arString = new UnicodeString(ar->idStr);
        availableRegions[ar->type]->addElement((void *)arString,status);
    }

    ucln_i18n_registerCleanup(UCLN_I18N_REGION, region_cleanup);
}

void Region::cleanupRegionData() {
    for (int32_t i = 0 ; i < URGN_LIMIT ; i++ ) {
        if ( availableRegions[i] ) {
            delete availableRegions[i];
        }
    }

    if (regionAliases) {
        uhash_close(regionAliases);
    }

    if (numericCodeMap) {
        uhash_close(numericCodeMap);
    }

    if (regionIDMap) {
        uhash_close(regionIDMap);
    }
    gRegionDataInitOnce.reset();
}

Region::Region ()
        : code(-1),
          type(URGN_UNKNOWN),
          containingRegion(NULL),
          containedRegions(NULL),
          preferredValues(NULL) {
    id[0] = 0;
}

Region::~Region () {
        if (containedRegions) {
            delete containedRegions;
        }
        if (preferredValues) {
            delete preferredValues;
        }
}

/**
 * Returns true if the two regions are equal.
 */
UBool
Region::operator==(const Region &that) const {
    return (idStr == that.idStr);
}

/**
 * Returns true if the two regions are NOT equal; that is, if operator ==() returns false.
 */
UBool
Region::operator!=(const Region &that) const {
        return (idStr != that.idStr);
}

/**
 * Returns a pointer to a Region using the given region code.  The region code can be either 2-letter ISO code,
 * 3-letter ISO code,  UNM.49 numeric code, or other valid Unicode Region Code as defined by the LDML specification.
 * The identifier will be canonicalized internally using the supplemental metadata as defined in the CLDR.
 * If the region code is NULL or not recognized, the appropriate error code will be set ( U_ILLEGAL_ARGUMENT_ERROR )
 */
const Region* U_EXPORT2
Region::getInstance(const char *region_code, UErrorCode &status) {

    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    if (U_FAILURE(status)) {
        return NULL;
    }

    if ( !region_code ) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    UnicodeString regionCodeString = UnicodeString(region_code, -1, US_INV);
    Region *r = (Region *)uhash_get(regionIDMap,(void *)&regionCodeString);

    if ( !r ) {
        r = (Region *)uhash_get(regionAliases,(void *)&regionCodeString);
    }

    if ( !r ) { // Unknown region code
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if ( r->type == URGN_DEPRECATED && r->preferredValues->size() == 1) {
        StringEnumeration *pv = r->getPreferredValues();
        pv->reset(status);
        const UnicodeString *ustr = pv->snext(status);
        r = (Region *)uhash_get(regionIDMap,(void *)ustr);
        delete pv;
    }

    return r;

}

/**
 * Returns a pointer to a Region using the given numeric region code. If the numeric region code is not recognized,
 * the appropriate error code will be set ( U_ILLEGAL_ARGUMENT_ERROR ).
 */
const Region* U_EXPORT2
Region::getInstance (int32_t code, UErrorCode &status) {

    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    if (U_FAILURE(status)) {
        return NULL;
    }

    Region *r = (Region *)uhash_iget(numericCodeMap,code);

    if ( !r ) { // Just in case there's an alias that's numeric, try to find it.
        UErrorCode fs = U_ZERO_ERROR;
        UnicodeString pat = UNICODE_STRING_SIMPLE("00#");
        DecimalFormat *df = new DecimalFormat(pat,fs);

        UnicodeString id;
        id.remove();
        df->format(code,id);
        delete df;
        r = (Region *)uhash_get(regionAliases,&id);
    }

    if ( !r ) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if ( r->type == URGN_DEPRECATED && r->preferredValues->size() == 1) {
        StringEnumeration *pv = r->getPreferredValues();
        pv->reset(status);
        const UnicodeString *ustr = pv->snext(status);
        r = (Region *)uhash_get(regionIDMap,(void *)ustr);
        delete pv;
    }

    return r;
}


/**
 * Returns an enumeration over the IDs of all known regions that match the given type.
 */
StringEnumeration* U_EXPORT2
Region::getAvailable(URegionType type) {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    return new RegionNameEnumeration(availableRegions[type],status);
}

/**
 * Returns a pointer to the region that contains this region.  Returns NULL if this region is code "001" (World)
 * or "ZZ" (Unknown region). For example, calling this method with region "IT" (Italy) returns the
 * region "039" (Southern Europe).
 */
const Region*
Region::getContainingRegion() const {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    return containingRegion;
}

/**
 * Return a pointer to the region that geographically contains this region and matches the given type,
 * moving multiple steps up the containment chain if necessary.  Returns NULL if no containing region can be found
 * that matches the given type. Note: The URegionTypes = "URGN_GROUPING", "URGN_DEPRECATED", or "URGN_UNKNOWN"
 * are not appropriate for use in this API. NULL will be returned in this case. For example, calling this method
 * with region "IT" (Italy) for type "URGN_CONTINENT" returns the region "150" ( Europe ).
 */
const Region*
Region::getContainingRegion(URegionType type) const {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    if ( containingRegion == NULL ) {
        return NULL;
    }

    if ( containingRegion->type == type ) {
        return containingRegion;
    } else {
        return containingRegion->getContainingRegion(type);
    }
}

/**
 * Return an enumeration over the IDs of all the regions that are immediate children of this region in the
 * region hierarchy. These returned regions could be either macro regions, territories, or a mixture of the two,
 * depending on the containment data as defined in CLDR.  This API may return NULL if this region doesn't have
 * any sub-regions. For example, calling this method with region "150" (Europe) returns an enumeration containing
 * the various sub regions of Europe - "039" (Southern Europe) - "151" (Eastern Europe) - "154" (Northern Europe)
 * and "155" (Western Europe).
 */
StringEnumeration*
Region::getContainedRegions() const {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    return new RegionNameEnumeration(containedRegions,status);
}

/**
 * Returns an enumeration over the IDs of all the regions that are children of this region anywhere in the region
 * hierarchy and match the given type.  This API may return an empty enumeration if this region doesn't have any
 * sub-regions that match the given type. For example, calling this method with region "150" (Europe) and type
 * "URGN_TERRITORY" returns a set containing all the territories in Europe ( "FR" (France) - "IT" (Italy) - "DE" (Germany) etc. )
 */
StringEnumeration*
Region::getContainedRegions( URegionType type ) const {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    if (U_FAILURE(status)) {
        return NULL;
    }

    UVector *result = new UVector(NULL, uhash_compareChars, status);

    StringEnumeration *cr = getContainedRegions();

    for ( int32_t i = 0 ; i < cr->count(status) ; i++ ) {
        const char *id = cr->next(NULL,status);
        const Region *r = Region::getInstance(id,status);
        if ( r->getType() == type ) {
            result->addElement((void *)&r->idStr,status);
        } else {
            StringEnumeration *children = r->getContainedRegions(type);
            for ( int32_t j = 0 ; j < children->count(status) ; j++ ) {
                const char *id2 = children->next(NULL,status);
                const Region *r2 = Region::getInstance(id2,status);
                result->addElement((void *)&r2->idStr,status);
            }
            delete children;
        }
    }
    delete cr;
    StringEnumeration* resultEnumeration = new RegionNameEnumeration(result,status);
    delete result;
    return resultEnumeration;
}

/**
 * Returns true if this region contains the supplied other region anywhere in the region hierarchy.
 */
UBool
Region::contains(const Region &other) const {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);

    if (!containedRegions) {
          return FALSE;
    }
    if (containedRegions->contains((void *)&other.idStr)) {
        return TRUE;
    } else {
        for ( int32_t i = 0 ; i < containedRegions->size() ; i++ ) {
            UnicodeString *crStr = (UnicodeString *)containedRegions->elementAt(i);
            Region *cr = (Region *) uhash_get(regionIDMap,(void *)crStr);
            if ( cr && cr->contains(other) ) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

/**
 * For deprecated regions, return an enumeration over the IDs of the regions that are the preferred replacement
 * regions for this region.  Returns NULL for a non-deprecated region.  For example, calling this method with region
 * "SU" (Soviet Union) would return a list of the regions containing "RU" (Russia), "AM" (Armenia), "AZ" (Azerbaijan), etc...
 */
StringEnumeration*
Region::getPreferredValues() const {
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gRegionDataInitOnce, &loadRegionData, status);
    if ( type == URGN_DEPRECATED ) {
        return new RegionNameEnumeration(preferredValues,status);
    } else {
        return NULL;
    }
}


/**
 * Return this region's canonical region code.
 */
const char*
Region::getRegionCode() const {
    return id;
}

int32_t
Region::getNumericCode() const {
    return code;
}

/**
 * Returns the region type of this region.
 */
URegionType
Region::getType() const {
    return type;
}

RegionNameEnumeration::RegionNameEnumeration(UVector *fNameList, UErrorCode& status) {
    pos=0;
    if (fNameList && U_SUCCESS(status)) {
        fRegionNames = new UVector(uprv_deleteUObject, uhash_compareUnicodeString, fNameList->size(),status);
        for ( int32_t i = 0 ; i < fNameList->size() ; i++ ) {
            UnicodeString* this_region_name = (UnicodeString *)fNameList->elementAt(i);
            UnicodeString* new_region_name = new UnicodeString(*this_region_name);
            fRegionNames->addElement((void *)new_region_name,status);
        }
    }
    else {
        fRegionNames = NULL;
    }
}

const UnicodeString*
RegionNameEnumeration::snext(UErrorCode& status) {
  if (U_FAILURE(status) || (fRegionNames==NULL)) {
    return NULL;
  }
  const UnicodeString* nextStr = (const UnicodeString *)fRegionNames->elementAt(pos);
  if (nextStr!=NULL) {
    pos++;
  }
  return nextStr;
}

void
RegionNameEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
RegionNameEnumeration::count(UErrorCode& /*status*/) const {
    return (fRegionNames==NULL) ? 0 : fRegionNames->size();
}

RegionNameEnumeration::~RegionNameEnumeration() {
    delete fRegionNames;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
