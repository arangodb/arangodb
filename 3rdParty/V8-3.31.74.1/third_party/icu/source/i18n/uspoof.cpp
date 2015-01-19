/*
***************************************************************************
* Copyright (C) 2008-2013, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*   file name:  uspoof.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2008Feb13
*   created by: Andy Heninger
*
*   Unicode Spoof Detection
*/
#include "unicode/utypes.h"
#include "unicode/normalizer2.h"
#include "unicode/uspoof.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "cstring.h"
#include "identifier_info.h"
#include "mutex.h"
#include "scriptset.h"
#include "uassert.h"
#include "ucln_in.h"
#include "uspoof_impl.h"
#include "umutex.h"


#if !UCONFIG_NO_NORMALIZATION

U_NAMESPACE_USE


//
// Static Objects used by the spoof impl, their thread safe initialization and their cleanup.
//
static UnicodeSet *gInclusionSet = NULL;
static UnicodeSet *gRecommendedSet = NULL;
static const Normalizer2 *gNfdNormalizer = NULL;
static UMutex gInitMutex = U_MUTEX_INITIALIZER;

static UBool U_CALLCONV
uspoof_cleanup(void) {
    delete gInclusionSet;
    gInclusionSet = NULL;
    delete gRecommendedSet;
    gRecommendedSet = NULL;
    gNfdNormalizer = NULL;
    return TRUE;
}

static void initializeStatics() {
    Mutex m(&gInitMutex);
    UErrorCode status = U_ZERO_ERROR;
    if (gInclusionSet == NULL) {
        gInclusionSet = new UnicodeSet(UNICODE_STRING_SIMPLE("[\
            \\-.\\u00B7\\u05F3\\u05F4\\u0F0B\\u200C\\u200D\\u2019]"), status);
        gRecommendedSet = new UnicodeSet(UNICODE_STRING_SIMPLE("[\
            [0-z\\u00C0-\\u017E\\u01A0\\u01A1\\u01AF\\u01B0\\u01CD-\
            \\u01DC\\u01DE-\\u01E3\\u01E6-\\u01F5\\u01F8-\\u021B\\u021E\
            \\u021F\\u0226-\\u0233\\u02BB\\u02BC\\u02EC\\u0300-\\u0304\
            \\u0306-\\u030C\\u030F-\\u0311\\u0313\\u0314\\u031B\\u0323-\
            \\u0328\\u032D\\u032E\\u0330\\u0331\\u0335\\u0338\\u0339\
            \\u0342-\\u0345\\u037B-\\u03CE\\u03FC-\\u045F\\u048A-\\u0525\
            \\u0531-\\u0586\\u05D0-\\u05F2\\u0621-\\u063F\\u0641-\\u0655\
            \\u0660-\\u0669\\u0670-\\u068D\\u068F-\\u06D5\\u06E5\\u06E6\
            \\u06EE-\\u06FF\\u0750-\\u07B1\\u0901-\\u0939\\u093C-\\u094D\
            \\u0950\\u0960-\\u0972\\u0979-\\u0A4D\\u0A5C-\\u0A74\\u0A81-\
            \\u0B43\\u0B47-\\u0B61\\u0B66-\\u0C56\\u0C60\\u0C61\\u0C66-\
            \\u0CD6\\u0CE0-\\u0CEF\\u0D02-\\u0D28\\u0D2A-\\u0D39\\u0D3D-\
            \\u0D43\\u0D46-\\u0D4D\\u0D57-\\u0D61\\u0D66-\\u0D8E\\u0D91-\
            \\u0DA5\\u0DA7-\\u0DDE\\u0DF2\\u0E01-\\u0ED9\\u0F00\\u0F20-\
            \\u0F8B\\u0F90-\\u109D\\u10D0-\\u10F0\\u10F7-\\u10FA\\u1200-\
            \\u135A\\u135F\\u1380-\\u138F\\u1401-\\u167F\\u1780-\\u17A2\
            \\u17A5-\\u17A7\\u17A9-\\u17B3\\u17B6-\\u17CA\\u17D2\\u17D7-\
            \\u17DC\\u17E0-\\u17E9\\u1810-\\u18A8\\u18AA-\\u18F5\\u1E00-\
            \\u1E99\\u1F00-\\u1FFC\\u2D30-\\u2D65\\u2D80-\\u2DDE\\u3005-\
            \\u3007\\u3041-\\u31B7\\u3400-\\u9FCB\\uA000-\\uA48C\\uA67F\
            \\uA717-\\uA71F\\uA788\\uAA60-\\uAA7B\\uAC00-\\uD7A3\\uFA0E-\
            \\uFA29\\U00020000-\
            \\U0002B734]-[[:Cn:][:nfkcqc=n:][:XIDC=n:]]]"), status);
        gNfdNormalizer = Normalizer2::getNFDInstance(status);
    }
    ucln_i18n_registerCleanup(UCLN_I18N_SPOOF, uspoof_cleanup);

    return;
}


U_CAPI USpoofChecker * U_EXPORT2
uspoof_open(UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    initializeStatics();
    SpoofImpl *si = new SpoofImpl(SpoofData::getDefault(*status), *status);
    if (U_FAILURE(*status)) {
        delete si;
        si = NULL;
    }
    return reinterpret_cast<USpoofChecker *>(si);
}


U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSerialized(const void *data, int32_t length, int32_t *pActualLength,
                          UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    initializeStatics();
    SpoofData *sd = new SpoofData(data, length, *status);
    SpoofImpl *si = new SpoofImpl(sd, *status);
    if (U_FAILURE(*status)) {
        delete sd;
        delete si;
        return NULL;
    }
    if (sd == NULL || si == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        delete sd;
        delete si;
        return NULL;
    }
        
    if (pActualLength != NULL) {
        *pActualLength = sd->fRawData->fLength;
    }
    return reinterpret_cast<USpoofChecker *>(si);
}


U_CAPI USpoofChecker * U_EXPORT2
uspoof_clone(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *src = SpoofImpl::validateThis(sc, *status);
    if (src == NULL) {
        return NULL;
    }
    SpoofImpl *result = new SpoofImpl(*src, *status);   // copy constructor
    if (U_FAILURE(*status)) {
        delete result;
        result = NULL;
    }
    return reinterpret_cast<USpoofChecker *>(result);
}


U_CAPI void U_EXPORT2
uspoof_close(USpoofChecker *sc) {
    UErrorCode status = U_ZERO_ERROR;
    SpoofImpl *This = SpoofImpl::validateThis(sc, status);
    delete This;
}


U_CAPI void U_EXPORT2
uspoof_setChecks(USpoofChecker *sc, int32_t checks, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return;
    }

    // Verify that the requested checks are all ones (bits) that 
    //   are acceptable, known values.
    if (checks & ~USPOOF_ALL_CHECKS) {
        *status = U_ILLEGAL_ARGUMENT_ERROR; 
        return;
    }

    This->fChecks = checks;
}


U_CAPI int32_t U_EXPORT2
uspoof_getChecks(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return 0;
    }
    return This->fChecks;
}

U_CAPI void U_EXPORT2
uspoof_setRestrictionLevel(USpoofChecker *sc, URestrictionLevel restrictionLevel) {
    UErrorCode status = U_ZERO_ERROR;
    SpoofImpl *This = SpoofImpl::validateThis(sc, status);
    if (This != NULL) {
        This->fRestrictionLevel = restrictionLevel;
    }
}

U_CAPI URestrictionLevel U_EXPORT2
uspoof_getRestrictionLevel(const USpoofChecker *sc) {
    UErrorCode status = U_ZERO_ERROR;
    const SpoofImpl *This = SpoofImpl::validateThis(sc, status);
    if (This == NULL) {
        return USPOOF_UNRESTRICTIVE;
    }
    return This->fRestrictionLevel;
}

U_CAPI void U_EXPORT2
uspoof_setAllowedLocales(USpoofChecker *sc, const char *localesList, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return;
    }
    This->setAllowedLocales(localesList, *status);
}

U_CAPI const char * U_EXPORT2
uspoof_getAllowedLocales(USpoofChecker *sc, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return NULL;
    }
    return This->getAllowedLocales(*status);
}


U_CAPI const USet * U_EXPORT2
uspoof_getAllowedChars(const USpoofChecker *sc, UErrorCode *status) {
    const UnicodeSet *result = uspoof_getAllowedUnicodeSet(sc, status);
    return result->toUSet();
}

U_CAPI const UnicodeSet * U_EXPORT2
uspoof_getAllowedUnicodeSet(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return NULL;
    }
    return This->fAllowedCharsSet;
}


U_CAPI void U_EXPORT2
uspoof_setAllowedChars(USpoofChecker *sc, const USet *chars, UErrorCode *status) {
    const UnicodeSet *set = UnicodeSet::fromUSet(chars);
    uspoof_setAllowedUnicodeSet(sc, set, status);
}


U_CAPI void U_EXPORT2
uspoof_setAllowedUnicodeSet(USpoofChecker *sc, const UnicodeSet *chars, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return;
    }
    if (chars->isBogus()) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    UnicodeSet *clonedSet = static_cast<UnicodeSet *>(chars->clone());
    if (clonedSet == NULL || clonedSet->isBogus()) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    clonedSet->freeze();
    delete This->fAllowedCharsSet;
    This->fAllowedCharsSet = clonedSet;
    This->fChecks |= USPOOF_CHAR_LIMIT;
}


U_CAPI int32_t U_EXPORT2
uspoof_check(const USpoofChecker *sc,
             const UChar *id, int32_t length,
             int32_t *position,
             UErrorCode *status) {
             
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return 0;
    }
    if (length < -1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString idStr((length == -1), id, length);  // Aliasing constructor.
    int32_t result = uspoof_checkUnicodeString(sc, idStr, position, status);
    return result;
}


U_CAPI int32_t U_EXPORT2
uspoof_checkUTF8(const USpoofChecker *sc,
                 const char *id, int32_t length,
                 int32_t *position,
                 UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return 0;
    }
    UnicodeString idStr = UnicodeString::fromUTF8(StringPiece(id, length>=0 ? length : uprv_strlen(id)));
    int32_t result = uspoof_checkUnicodeString(sc, idStr, position, status);
    return result;
}


U_CAPI int32_t U_EXPORT2
uspoof_areConfusable(const USpoofChecker *sc,
                     const UChar *id1, int32_t length1,
                     const UChar *id2, int32_t length2,
                     UErrorCode *status) {
    SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    if (length1 < -1 || length2 < -1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
        
    UnicodeString id1Str((length1==-1), id1, length1);  // Aliasing constructor
    UnicodeString id2Str((length2==-1), id2, length2);  // Aliasing constructor
    return uspoof_areConfusableUnicodeString(sc, id1Str, id2Str, status);
}


U_CAPI int32_t U_EXPORT2
uspoof_areConfusableUTF8(const USpoofChecker *sc,
                         const char *id1, int32_t length1,
                         const char *id2, int32_t length2,
                         UErrorCode *status) {
    SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    if (length1 < -1 || length2 < -1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString id1Str = UnicodeString::fromUTF8(StringPiece(id1, length1>=0? length1 : uprv_strlen(id1)));
    UnicodeString id2Str = UnicodeString::fromUTF8(StringPiece(id2, length2>=0? length2 : uprv_strlen(id2)));
    int32_t results = uspoof_areConfusableUnicodeString(sc, id1Str, id2Str, status);
    return results;
}
 

U_CAPI int32_t U_EXPORT2
uspoof_areConfusableUnicodeString(const USpoofChecker *sc,
                                  const icu::UnicodeString &id1,
                                  const icu::UnicodeString &id2,
                                  UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    // 
    // See section 4 of UAX 39 for the algorithm for checking whether two strings are confusable,
    //   and for definitions of the types (single, whole, mixed-script) of confusables.
    
    // We only care about a few of the check flags.  Ignore the others.
    // If no tests relavant to this function have been specified, return an error.
    // TODO:  is this really the right thing to do?  It's probably an error on the caller's part,
    //        but logically we would just return 0 (no error).
    if ((This->fChecks & (USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE | 
                          USPOOF_WHOLE_SCRIPT_CONFUSABLE)) == 0) {
        *status = U_INVALID_STATE_ERROR;
        return 0;
    }
    int32_t  flagsForSkeleton = This->fChecks & USPOOF_ANY_CASE;

    int32_t  result = 0;
    IdentifierInfo *identifierInfo = This->getIdentifierInfo(*status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    identifierInfo->setIdentifier(id1, *status);
    int32_t id1ScriptCount = identifierInfo->getScriptCount();
    identifierInfo->setIdentifier(id2, *status);
    int32_t id2ScriptCount = identifierInfo->getScriptCount();
    This->releaseIdentifierInfo(identifierInfo);
    identifierInfo = NULL;

    if (This->fChecks & USPOOF_SINGLE_SCRIPT_CONFUSABLE) {
        UnicodeString   id1Skeleton;
        UnicodeString   id2Skeleton;
        if (id1ScriptCount <= 1 && id2ScriptCount <= 1) {
            flagsForSkeleton |= USPOOF_SINGLE_SCRIPT_CONFUSABLE;
            uspoof_getSkeletonUnicodeString(sc, flagsForSkeleton, id1, id1Skeleton, status);
            uspoof_getSkeletonUnicodeString(sc, flagsForSkeleton, id2, id2Skeleton, status);
            if (id1Skeleton == id2Skeleton) {
                result |= USPOOF_SINGLE_SCRIPT_CONFUSABLE;
            }
        }
    }

    if (result & USPOOF_SINGLE_SCRIPT_CONFUSABLE) {
         // If the two inputs are single script confusable they cannot also be
         // mixed or whole script confusable, according to the UAX39 definitions.
         // So we can skip those tests.
         return result;
    }

    // Two identifiers are whole script confusable if each is of a single script 
    // and they are mixed script confusable.
    UBool possiblyWholeScriptConfusables = 
        id1ScriptCount <= 1 && id2ScriptCount <= 1 && (This->fChecks & USPOOF_WHOLE_SCRIPT_CONFUSABLE);

    //
    // Mixed Script Check
    //
    if ((This->fChecks & USPOOF_MIXED_SCRIPT_CONFUSABLE) || possiblyWholeScriptConfusables ) {
        // For getSkeleton(), resetting the USPOOF_SINGLE_SCRIPT_CONFUSABLE flag will get us
        // the mixed script table skeleton, which is what we want.
        // The Any Case / Lower Case bit in the skelton flags was set at the top of the function.
        UnicodeString id1Skeleton;
        UnicodeString id2Skeleton;
        flagsForSkeleton &= ~USPOOF_SINGLE_SCRIPT_CONFUSABLE;
        uspoof_getSkeletonUnicodeString(sc, flagsForSkeleton, id1, id1Skeleton, status);
        uspoof_getSkeletonUnicodeString(sc, flagsForSkeleton, id2, id2Skeleton, status);
        if (id1Skeleton == id2Skeleton) {
            result |= USPOOF_MIXED_SCRIPT_CONFUSABLE;
            if (possiblyWholeScriptConfusables) {
                result |= USPOOF_WHOLE_SCRIPT_CONFUSABLE;
            }
        }
    }

    return result;
}




U_CAPI int32_t U_EXPORT2
uspoof_checkUnicodeString(const USpoofChecker *sc,
                          const icu::UnicodeString &id, 
                          int32_t *position,
                          UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return 0;
    }
    int32_t result = 0;

    IdentifierInfo *identifierInfo = NULL;
    if ((This->fChecks) & (USPOOF_RESTRICTION_LEVEL | USPOOF_MIXED_NUMBERS)) {
        identifierInfo = This->getIdentifierInfo(*status);
        if (U_FAILURE(*status)) {
            goto cleanupAndReturn;
        }
        identifierInfo->setIdentifier(id, *status);
        identifierInfo->setIdentifierProfile(*This->fAllowedCharsSet);
    }


    if ((This->fChecks) & USPOOF_RESTRICTION_LEVEL) {
        URestrictionLevel idRestrictionLevel = identifierInfo->getRestrictionLevel(*status);
        if (idRestrictionLevel > This->fRestrictionLevel) {
            result |= USPOOF_RESTRICTION_LEVEL;
        }
        if (This->fChecks & USPOOF_AUX_INFO) {
            result |= idRestrictionLevel;
        }
    }

    if ((This->fChecks) & USPOOF_MIXED_NUMBERS) {
        const UnicodeSet *numerics = identifierInfo->getNumerics();
        if (numerics->size() > 1) {
            result |= USPOOF_MIXED_NUMBERS;
        }

        // TODO: ICU4J returns the UnicodeSet of the numerics found in the identifier.
        //       We have no easy way to do the same in C.
        // if (checkResult != null) {
        //     checkResult.numerics = numerics;
        // }
    }


    if (This->fChecks & (USPOOF_CHAR_LIMIT)) {
        int32_t i;
        UChar32 c;
        int32_t length = id.length();
        for (i=0; i<length ;) {
            c = id.char32At(i);
            i += U16_LENGTH(c);
            if (!This->fAllowedCharsSet->contains(c)) {
                result |= USPOOF_CHAR_LIMIT;
                break;
            }
        }
    }

    if (This->fChecks & 
        (USPOOF_WHOLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE | USPOOF_INVISIBLE)) {
        // These are the checks that need to be done on NFD input
        UnicodeString nfdText;
        gNfdNormalizer->normalize(id, nfdText, *status);
        int32_t nfdLength = nfdText.length();

        if (This->fChecks & USPOOF_INVISIBLE) {
           
            // scan for more than one occurence of the same non-spacing mark
            // in a sequence of non-spacing marks.
            int32_t     i;
            UChar32     c;
            UChar32     firstNonspacingMark = 0;
            UBool       haveMultipleMarks = FALSE;  
            UnicodeSet  marksSeenSoFar;   // Set of combining marks in a single combining sequence.
            
            for (i=0; i<nfdLength ;) {
                c = nfdText.char32At(i);
                i += U16_LENGTH(c);
                if (u_charType(c) != U_NON_SPACING_MARK) {
                    firstNonspacingMark = 0;
                    if (haveMultipleMarks) {
                        marksSeenSoFar.clear();
                        haveMultipleMarks = FALSE;
                    }
                    continue;
                }
                if (firstNonspacingMark == 0) {
                    firstNonspacingMark = c;
                    continue;
                }
                if (!haveMultipleMarks) {
                    marksSeenSoFar.add(firstNonspacingMark);
                    haveMultipleMarks = TRUE;
                }
                if (marksSeenSoFar.contains(c)) {
                    // report the error, and stop scanning.
                    // No need to find more than the first failure.
                    result |= USPOOF_INVISIBLE;
                    break;
                }
                marksSeenSoFar.add(c);
            }
        }
       
        
        if (This->fChecks & (USPOOF_WHOLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE)) {
            // The basic test is the same for both whole and mixed script confusables.
            // Compute the set of scripts that every input character has a confusable in.
            // For this computation an input character is always considered to be
            // confusable with itself in its own script.
            //
            // If the number of such scripts is two or more, and the input consisted of
            // characters all from a single script, we have a whole script confusable.
            // (The two scripts will be the original script and the one that is confusable)
            //
            // If the number of such scripts >= one, and the original input contained characters from
            // more than one script, we have a mixed script confusable.  (We can transform
            // some of the characters, and end up with a visually similar string all in
            // one script.)

            if (identifierInfo == NULL) {
                identifierInfo = This->getIdentifierInfo(*status);
                if (U_FAILURE(*status)) {
                    goto cleanupAndReturn;
                }
                identifierInfo->setIdentifier(id, *status);
            }

            int32_t scriptCount = identifierInfo->getScriptCount();
            
            ScriptSet scripts;
            This->wholeScriptCheck(nfdText, &scripts, *status);
            int32_t confusableScriptCount = scripts.countMembers();
            //printf("confusableScriptCount = %d\n", confusableScriptCount);
            
            if ((This->fChecks & USPOOF_WHOLE_SCRIPT_CONFUSABLE) &&
                confusableScriptCount >= 2 &&
                scriptCount == 1) {
                result |= USPOOF_WHOLE_SCRIPT_CONFUSABLE;
            }
        
            if ((This->fChecks & USPOOF_MIXED_SCRIPT_CONFUSABLE) &&
                confusableScriptCount >= 1 &&
                scriptCount > 1) {
                result |= USPOOF_MIXED_SCRIPT_CONFUSABLE;
            }
        }
    }

cleanupAndReturn:
    This->releaseIdentifierInfo(identifierInfo);
    if (position != NULL) {
        *position = 0;
    }
    return result;
}


U_CAPI int32_t U_EXPORT2
uspoof_getSkeleton(const USpoofChecker *sc,
                   uint32_t type,
                   const UChar *id,  int32_t length,
                   UChar *dest, int32_t destCapacity,
                   UErrorCode *status) {

    SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    if (length<-1 || destCapacity<0 || (destCapacity==0 && dest!=NULL)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UnicodeString idStr((length==-1), id, length);  // Aliasing constructor
    UnicodeString destStr;
    uspoof_getSkeletonUnicodeString(sc, type, idStr, destStr, status);
    destStr.extract(dest, destCapacity, *status);
    return destStr.length();
}



U_I18N_API UnicodeString &  U_EXPORT2
uspoof_getSkeletonUnicodeString(const USpoofChecker *sc,
                                uint32_t type,
                                const UnicodeString &id,
                                UnicodeString &dest,
                                UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return dest;
    }

   int32_t tableMask = 0;
   switch (type) {
      case 0:
        tableMask = USPOOF_ML_TABLE_FLAG;
        break;
      case USPOOF_SINGLE_SCRIPT_CONFUSABLE:
        tableMask = USPOOF_SL_TABLE_FLAG;
        break;
      case USPOOF_ANY_CASE:
        tableMask = USPOOF_MA_TABLE_FLAG;
        break;
      case USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE:
        tableMask = USPOOF_SA_TABLE_FLAG;
        break;
      default:
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return dest;
    }

    UnicodeString nfdId;
    gNfdNormalizer->normalize(id, nfdId, *status);

    // Apply the skeleton mapping to the NFD normalized input string
    // Accumulate the skeleton, possibly unnormalized, in a UnicodeString.
    int32_t inputIndex = 0;
    UnicodeString skelStr;
    int32_t normalizedLen = nfdId.length();
    for (inputIndex=0; inputIndex < normalizedLen; ) {
        UChar32 c = nfdId.char32At(inputIndex);
        inputIndex += U16_LENGTH(c);
        This->confusableLookup(c, tableMask, skelStr);
    }

    gNfdNormalizer->normalize(skelStr, dest, *status);
    return dest;
}


U_CAPI int32_t U_EXPORT2
uspoof_getSkeletonUTF8(const USpoofChecker *sc,
                       uint32_t type,
                       const char *id,  int32_t length,
                       char *dest, int32_t destCapacity,
                       UErrorCode *status) {
    SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    if (length<-1 || destCapacity<0 || (destCapacity==0 && dest!=NULL)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UnicodeString srcStr = UnicodeString::fromUTF8(StringPiece(id, length>=0 ? length : uprv_strlen(id)));
    UnicodeString destStr;
    uspoof_getSkeletonUnicodeString(sc, type, srcStr, destStr, status);
    if (U_FAILURE(*status)) {
        return 0;
    }

    int32_t lengthInUTF8 = 0;
    u_strToUTF8(dest, destCapacity, &lengthInUTF8,
                destStr.getBuffer(), destStr.length(), status);
    return lengthInUTF8;
}


U_CAPI int32_t U_EXPORT2
uspoof_serialize(USpoofChecker *sc,void *buf, int32_t capacity, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        U_ASSERT(U_FAILURE(*status));
        return 0;
    }
    int32_t dataSize = This->fSpoofData->fRawData->fLength;
    if (capacity < dataSize) {
        *status = U_BUFFER_OVERFLOW_ERROR;
        return dataSize;
    }
    uprv_memcpy(buf, This->fSpoofData->fRawData, dataSize);
    return dataSize;
}

U_CAPI const USet * U_EXPORT2
uspoof_getInclusionSet(UErrorCode *) {
    initializeStatics();
    return gInclusionSet->toUSet();
}

U_CAPI const USet * U_EXPORT2
uspoof_getRecommendedSet(UErrorCode *) {
    initializeStatics();
    return gRecommendedSet->toUSet();
}

U_I18N_API const UnicodeSet * U_EXPORT2
uspoof_getInclusionUnicodeSet(UErrorCode *) {
    initializeStatics();
    return gInclusionSet;
}

U_I18N_API const UnicodeSet * U_EXPORT2
uspoof_getRecommendedUnicodeSet(UErrorCode *) {
    initializeStatics();
    return gRecommendedSet;
}



#endif // !UCONFIG_NO_NORMALIZATION
