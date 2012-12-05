/*
*******************************************************************************
* Copyright (C) 2009-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

/**
 * \file
 * \brief C API: AlphabeticIndex class
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION && !UCONFIG_NO_NORMALIZATION

#include "unicode/alphaindex.h"
#include "unicode/coll.h"
#include "unicode/normalizer2.h"
#include "unicode/strenum.h"
#include "unicode/tblcoll.h"
#include "unicode/ulocdata.h"
#include "unicode/uniset.h"
#include "unicode/uobject.h"
#include "unicode/uscript.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"

#include "cstring.h"
#include "mutex.h"
#include "uassert.h"
#include "ucln_in.h"
#include "uhash.h"
#include "uvector.h"

//#include <string>
//#include <iostream>
U_NAMESPACE_BEGIN

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(AlphabeticIndex)

// Forward Declarations
static int32_t U_CALLCONV
PreferenceComparator(const void *context, const void *left, const void *right);

static int32_t U_CALLCONV
sortCollateComparator(const void *context, const void *left, const void *right);

static int32_t U_CALLCONV
recordCompareFn(const void *context, const void *left, const void *right);

//  UVector<Bucket *> support function, delete a Bucket.
static void U_CALLCONV
alphaIndex_deleteBucket(void *obj) {
    delete static_cast<AlphabeticIndex::Bucket *>(obj);
}

//  UVector<Record *> support function, delete a Record.
static void U_CALLCONV
alphaIndex_deleteRecord(void *obj) {
    delete static_cast<AlphabeticIndex::Record *>(obj);
}



static const Normalizer2 *nfkdNormalizer;

//
//  Append the contents of a UnicodeSet to a UVector of UnicodeStrings.
//  Append everything - individual characters are handled as strings of length 1.
//  The destination vector owns the appended strings.

static void appendUnicodeSetToUVector(UVector &dest, const UnicodeSet &source, UErrorCode &status) {
    UnicodeSetIterator setIter(source);
    while (setIter.next()) {
        const UnicodeString &str = setIter.getString();
        dest.addElement(str.clone(), status);
    }
}


AlphabeticIndex::AlphabeticIndex(const Locale &locale, UErrorCode &status) {
    init(status);
    if (U_FAILURE(status)) {
        return;
    }
    locale_ = locale;
    langType_ = langTypeFromLocale(locale_);

    collator_ = Collator::createInstance(locale, status);
    if (collator_ != NULL) {
        collatorPrimaryOnly_ = collator_->clone();
    }
    if (collatorPrimaryOnly_ != NULL) {
        collatorPrimaryOnly_->setStrength(Collator::PRIMARY);
    }
    getIndexExemplars(*initialLabels_, locale, status);
    indexBuildRequired_ = TRUE;
    if ((collator_ == NULL || collatorPrimaryOnly_ == NULL) && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    firstScriptCharacters_ = firstStringsInScript(status);
}


AlphabeticIndex::~AlphabeticIndex() {
    uhash_close(alreadyIn_);
    delete bucketList_;
    delete collator_;
    delete collatorPrimaryOnly_;
    delete firstScriptCharacters_;
    delete labels_;
    delete inputRecords_;
    delete noDistinctSorting_;
    delete notAlphabetic_;
    delete initialLabels_;
}


AlphabeticIndex &AlphabeticIndex::addLabels(const UnicodeSet &additions, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    initialLabels_->addAll(additions);
    return *this;
}


AlphabeticIndex &AlphabeticIndex::addLabels(const Locale &locale, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    UnicodeSet additions;
    getIndexExemplars(additions, locale, status);
    initialLabels_->addAll(additions);
    return *this;
}


int32_t AlphabeticIndex::getBucketCount(UErrorCode &status) {
    buildIndex(status);
    if (U_FAILURE(status)) {
        return 0;
    }
    return bucketList_->size();
}


int32_t AlphabeticIndex::getRecordCount(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    return inputRecords_->size();
}


void AlphabeticIndex::buildIndex(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (!indexBuildRequired_) {
        return;
    }

    // Discard any already-built data.
    // This is important when the user builds and uses an index, then subsequently modifies it,
    // necessitating a rebuild.

    bucketList_->removeAllElements();
    labels_->removeAllElements();
    uhash_removeAll(alreadyIn_);
    noDistinctSorting_->clear();
    notAlphabetic_->clear();

    // first sort the incoming Labels, with a "best" ordering among items
    // that are the same according to the collator

    UVector preferenceSorting(status);   // Vector of UnicodeStrings; owned by the vector.
    preferenceSorting.setDeleter(uprv_deleteUObject);
    appendUnicodeSetToUVector(preferenceSorting, *initialLabels_, status);
    preferenceSorting.sortWithUComparator(PreferenceComparator, &status, status);

    // We now make a set of Labels.
    // Some of the input may, however, be redundant.
    // That is, we might have c, ch, d, where "ch" sorts just like "c", "h"
    // So we make a pass through, filtering out those cases.
    // TODO: filtering these out would seem to be at odds with the eventual goal
    //       of being able to split buckets that contain too many items.

    UnicodeSet labelSet;
    for (int32_t psIndex=0; psIndex<preferenceSorting.size(); psIndex++) {
        UnicodeString item = *static_cast<const UnicodeString *>(preferenceSorting.elementAt(psIndex));
        // TODO:  Since preferenceSorting was originally populated from the contents of a UnicodeSet,
        //        is it even possible for duplicates to show up in this check?
        if (labelSet.contains(item)) {
            UnicodeSetIterator itemAlreadyInIter(labelSet);
            while (itemAlreadyInIter.next()) {
                const UnicodeString &itemAlreadyIn = itemAlreadyInIter.getString();
                if (collatorPrimaryOnly_->compare(item, itemAlreadyIn) == 0) {
                    UnicodeSet *targets = static_cast<UnicodeSet *>(uhash_get(alreadyIn_, &itemAlreadyIn));
                    if (targets == NULL) {
                        // alreadyIn.put(itemAlreadyIn, targets = new LinkedHashSet<String>());
                        targets = new UnicodeSet();
                        uhash_put(alreadyIn_, itemAlreadyIn.clone(), targets, &status);
                    }
                    targets->add(item);
                    break;
                }
            }
        } else if (item.moveIndex32(0, 1) < item.length() &&  // Label contains more than one code point.
                   collatorPrimaryOnly_->compare(item, separated(item)) == 0) {
            noDistinctSorting_->add(item);
        } else if (!ALPHABETIC->containsSome(item)) {
            notAlphabetic_->add(item);
        } else {
            labelSet.add(item);
        }
    }

    // If we have no labels, hard-code a fallback default set of [A-Z]
    // This case can occur with locales that don't have exemplar character data, including root.
    // A no-labels situation will cause other problems; it needs to be avoided.
    if (labelSet.isEmpty()) {
        labelSet.add((UChar32)0x41, (UChar32)0x5A);
    }

    // Move the set of Labels from the set into a vector, and sort
    // according to the collator.

    appendUnicodeSetToUVector(*labels_, labelSet, status);
    labels_->sortWithUComparator(sortCollateComparator, collatorPrimaryOnly_, status);

    // if the result is still too large, cut down to maxLabelCount_ elements, by removing every nth element
    //    Implemented by copying the elements to be retained to a new UVector.

    const int32_t size = labelSet.size() - 1;
    if (size > maxLabelCount_) {
        UVector *newLabels = new UVector(status);
        newLabels->setDeleter(uprv_deleteUObject);
        int32_t count = 0;
        int32_t old = -1;
        for (int32_t srcIndex=0; srcIndex<labels_->size(); srcIndex++) {
            const UnicodeString *str = static_cast<const UnicodeString *>(labels_->elementAt(srcIndex));
            ++count;
            const int32_t bump = count * maxLabelCount_ / size;
            if (bump == old) {
                // it.remove();
            } else {
                newLabels->addElement(str->clone(), status);
                old = bump;
            }
        }
        delete labels_;
        labels_ = newLabels;
    }

    // We now know the list of labels.
    // Create a corresponding list of buckets, one per label.
    
    buildBucketList(status);    // Corresponds to Java BucketList constructor.

    // Bin the Records into the Buckets.
    bucketRecords(status);

    indexBuildRequired_ = FALSE;
    resetBucketIterator(status);
}

//
//  buildBucketList()    Corresponds to the BucketList constructor in the Java version.

void AlphabeticIndex::buildBucketList(UErrorCode &status) {
    UnicodeString labelStr = getUnderflowLabel();
    Bucket *b = new Bucket(labelStr, *EMPTY_STRING, U_ALPHAINDEX_UNDERFLOW, status);
    bucketList_->addElement(b, status);

    // Build up the list, adding underflow, additions, overflow
    // insert infix labels as needed, using \uFFFF.
    const UnicodeString *last = static_cast<UnicodeString *>(labels_->elementAt(0));
    b = new Bucket(*last, *last, U_ALPHAINDEX_NORMAL, status);
    bucketList_->addElement(b, status);

    UnicodeSet lastSet;
    UnicodeSet set;
    AlphabeticIndex::getScriptSet(lastSet, *last, status);
    lastSet.removeAll(*IGNORE_SCRIPTS);

    for (int i = 1; i < labels_->size(); ++i) {
        UnicodeString *current = static_cast<UnicodeString *>(labels_->elementAt(i));
        getScriptSet(set, *current, status);
        set.removeAll(*IGNORE_SCRIPTS);
        if (lastSet.containsNone(set)) {
            // check for adjacent
            const UnicodeString &overflowComparisonString = getOverflowComparisonString(*last, status);
            if (collatorPrimaryOnly_->compare(overflowComparisonString, *current) < 0) {
                labelStr = getInflowLabel();
                b = new Bucket(labelStr, overflowComparisonString, U_ALPHAINDEX_INFLOW, status);
                bucketList_->addElement(b, status);
                i++;
                lastSet = set;
            }
        }
        b = new Bucket(*current, *current, U_ALPHAINDEX_NORMAL, status);
        bucketList_->addElement(b, status);
        last = current;
        lastSet = set;
    }
    const UnicodeString &limitString = getOverflowComparisonString(*last, status);
    b = new Bucket(getOverflowLabel(), limitString, U_ALPHAINDEX_OVERFLOW, status);
    bucketList_->addElement(b, status);
    // final overflow bucket
}


//
//   Place all of the raw input records into the correct bucket.
//
//       Begin by sorting the input records; this lets us bin them in a single pass.
//
//       Note on storage management:  The input records are owned by the
//       inputRecords_ vector, and will (eventually) be auto-deleted by it.
//       The Bucket objects have pointers to the Record objects, but do not own them.
//
void AlphabeticIndex::bucketRecords(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    inputRecords_->sortWithUComparator(recordCompareFn, collator_, status);
    U_ASSERT(bucketList_->size() > 0);   // Should always have at least an overflow
                                         //   bucket, even if no user labels.
    int32_t bucketIndex = 0;
    Bucket *destBucket = static_cast<Bucket *>(bucketList_->elementAt(bucketIndex));
    Bucket *nextBucket = NULL;
    if (bucketIndex+1 < bucketList_->size()) {
        nextBucket = static_cast<Bucket *>(bucketList_->elementAt(bucketIndex+1));
    }
    int32_t recordIndex = 0;
    Record *r = static_cast<Record *>(inputRecords_->elementAt(recordIndex));
    while (recordIndex < inputRecords_->size()) {
        if (nextBucket == NULL ||
            collatorPrimaryOnly_->compare(r->sortingName_, nextBucket->lowerBoundary_) < 0) {
                // Record goes in current bucket.  Advance to next record,
                // stay on current bucket.
                destBucket->records_->addElement(r, status);
                ++recordIndex;
                r = static_cast<Record *>(inputRecords_->elementAt(recordIndex));
        } else {
            // Advance to the next bucket, stay on current record.
            bucketIndex++;
            destBucket = nextBucket;
            if (bucketIndex+1 < bucketList_->size()) {
                nextBucket = static_cast<Bucket *>(bucketList_->elementAt(bucketIndex+1));
            } else {
                nextBucket = NULL;
            }
            U_ASSERT(destBucket != NULL);
        }
    }

}


void AlphabeticIndex::getIndexExemplars(UnicodeSet  &dest, const Locale &locale, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    LocalULocaleDataPointer uld(ulocdata_open(locale.getName(), &status));
    UnicodeSet exemplars;
    ulocdata_getExemplarSet(uld.getAlias(), exemplars.toUSet(), 0, ULOCDATA_ES_INDEX, &status);
    if (U_SUCCESS(status)) {
        dest.addAll(exemplars);
        return;
    }
    status = U_ZERO_ERROR;  // Clear out U_MISSING_RESOURCE_ERROR

    // Locale data did not include explicit Index characters.
    // Synthesize a set of them from the locale's standard exemplar characters.

    ulocdata_getExemplarSet(uld.getAlias(), exemplars.toUSet(), 0, ULOCDATA_ES_STANDARD, &status);
    if (U_FAILURE(status)) {
        return;
    }

    // Upper-case any that aren't already so.
    //   (We only do this for synthesized index characters.)

    UnicodeSetIterator it(exemplars);
    UnicodeString upperC;
    UnicodeSet  lowersToRemove;
    UnicodeSet  uppersToAdd;
    while (it.next()) {
        const UnicodeString &exemplarC = it.getString();
        upperC = exemplarC;
        upperC.toUpper(locale);
        if (exemplarC != upperC) {
            lowersToRemove.add(exemplarC);
            uppersToAdd.add(upperC);
        }
    }
    exemplars.removeAll(lowersToRemove);
    exemplars.addAll(uppersToAdd);

    // get the exemplars, and handle special cases

    // question: should we add auxiliary exemplars?
    if (exemplars.containsSome(*CORE_LATIN)) {
        exemplars.addAll(*CORE_LATIN);
    }
    if (exemplars.containsSome(*HANGUL)) {
        // cut down to small list
        UnicodeSet BLOCK_HANGUL_SYLLABLES(UNICODE_STRING_SIMPLE("[:block=hangul_syllables:]"), status);
        exemplars.removeAll(BLOCK_HANGUL_SYLLABLES);
        exemplars.addAll(*HANGUL);
    }
    if (exemplars.containsSome(*ETHIOPIC)) {
        // cut down to small list
        // make use of the fact that Ethiopic is allocated in 8's, where
        // the base is 0 mod 8.
        UnicodeSetIterator  it(*ETHIOPIC);
        while (it.next() && !it.isString()) {
            if ((it.getCodepoint() & 0x7) != 0) {
                exemplars.remove(it.getCodepoint());
            }
        }
    }
    dest.addAll(exemplars);
}


/*
 * Return the string with interspersed CGJs. Input must have more than 2 codepoints.
 */
static const UChar32 CGJ = (UChar)0x034F;
UnicodeString AlphabeticIndex::separated(const UnicodeString &item) {
    UnicodeString result;
    if (item.length() == 0) {
        return result;
    }
    int32_t i = 0;
    for (;;) {
        UChar32  cp = item.char32At(i);
        result.append(cp);
        i = item.moveIndex32(i, 1);
        if (i >= item.length()) {
            break;
        }
        result.append(CGJ);
    }
    return result;
}


UBool AlphabeticIndex::operator==(const AlphabeticIndex& /* other */) const {
    return FALSE;
}


UBool AlphabeticIndex::operator!=(const AlphabeticIndex& /* other */) const {
    return FALSE;
}


const RuleBasedCollator &AlphabeticIndex::getCollator() const {
    // There are no known non-RuleBasedCollator collators, and none ever expected.
    // But, in case that changes, better a null pointer than a wrong type.
    return *dynamic_cast<RuleBasedCollator *>(collator_);
}


const UnicodeString &AlphabeticIndex::getInflowLabel() const {
    return inflowLabel_;
}

const UnicodeString &AlphabeticIndex::getOverflowLabel() const {
    return overflowLabel_;
}


const UnicodeString &AlphabeticIndex::getUnderflowLabel() const {
    return underflowLabel_;
}


AlphabeticIndex &AlphabeticIndex::setInflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    inflowLabel_ = label;
    indexBuildRequired_ = TRUE;
    return *this;
}


AlphabeticIndex &AlphabeticIndex::setOverflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    overflowLabel_ = label;
    indexBuildRequired_ = TRUE;
    return *this;
}


AlphabeticIndex &AlphabeticIndex::setUnderflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    underflowLabel_ = label;
    indexBuildRequired_ = TRUE;
    return *this;
}


int32_t AlphabeticIndex::getMaxLabelCount() const {
    return maxLabelCount_;
}


AlphabeticIndex &AlphabeticIndex::setMaxLabelCount(int32_t maxLabelCount, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (maxLabelCount <= 0) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    maxLabelCount_ = maxLabelCount;
    if (maxLabelCount < bucketList_->size()) {
        indexBuildRequired_ = TRUE;
    }
    return *this;
}


const UnicodeString &AlphabeticIndex::getOverflowComparisonString(const UnicodeString &lowerLimit, UErrorCode &/*status*/) {
    for (int32_t i=0; i<firstScriptCharacters_->size(); i++) {
        const UnicodeString *s =
                static_cast<const UnicodeString *>(firstScriptCharacters_->elementAt(i));
        if (collator_->compare(*s, lowerLimit) > 0) {
            return *s;
        }
    }
    return *EMPTY_STRING;
}

UnicodeSet *AlphabeticIndex::getScriptSet(UnicodeSet &dest, const UnicodeString &codePoint, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return &dest;
    }
    UChar32 cp = codePoint.char32At(0);
    UScriptCode scriptCode = uscript_getScript(cp, &status);
    dest.applyIntPropertyValue(UCHAR_SCRIPT, scriptCode, status);
    return &dest;
}

//
//  init() - Common code for constructors.
//

void AlphabeticIndex::init(UErrorCode &status) {
    // Initialize statics if needed.
    AlphabeticIndex::staticInit(status);

    // Put the object into a known state so that the destructor will function.

    alreadyIn_             = NULL;
    bucketList_            = NULL;
    collator_              = NULL;
    collatorPrimaryOnly_   = NULL;
    currentBucket_         = NULL;
    firstScriptCharacters_ = NULL;
    initialLabels_         = NULL;
    indexBuildRequired_    = TRUE;
    inputRecords_          = NULL;
    itemsIterIndex_        = 0;
    labels_                = NULL;
    labelsIterIndex_       = 0;
    maxLabelCount_         = 99;
    noDistinctSorting_     = NULL;
    notAlphabetic_         = NULL;
    recordCounter_         = 0;

    if (U_FAILURE(status)) {
        return;
    }
    alreadyIn_             = uhash_open(uhash_hashUnicodeString,    // Key Hash,
                                        uhash_compareUnicodeString, // key Comparator,
                                        NULL,                       // value Comparator
                                        &status);
    uhash_setKeyDeleter(alreadyIn_, uprv_deleteUObject);
    uhash_setValueDeleter(alreadyIn_, uprv_deleteUObject);

    bucketList_            = new UVector(status);
    bucketList_->setDeleter(alphaIndex_deleteBucket);
    labels_                = new UVector(status);
    labels_->setDeleter(uprv_deleteUObject);
    labels_->setComparer(uhash_compareUnicodeString);
    inputRecords_          = new UVector(status);
    inputRecords_->setDeleter(alphaIndex_deleteRecord);

    noDistinctSorting_     = new UnicodeSet();
    notAlphabetic_         = new UnicodeSet();
    initialLabels_         = new UnicodeSet();

    inflowLabel_.remove();
    inflowLabel_.append((UChar)0x2026);    // Ellipsis
    overflowLabel_ = inflowLabel_;
    underflowLabel_ = inflowLabel_;

    // TODO:  check for memory allocation failures.
}


static  UBool  indexCharactersAreInitialized = FALSE;

//  Index Characters Clean up function.  Delete statically allocated constant stuff.
U_CDECL_BEGIN
static UBool U_CALLCONV indexCharacters_cleanup(void) {
    AlphabeticIndex::staticCleanup();
    return TRUE;
}
U_CDECL_END

void AlphabeticIndex::staticCleanup() {
    delete ALPHABETIC;
    ALPHABETIC = NULL;
    delete HANGUL;
    HANGUL = NULL;
    delete ETHIOPIC;
    ETHIOPIC = NULL;
    delete CORE_LATIN;
    CORE_LATIN = NULL;
    delete IGNORE_SCRIPTS;
    IGNORE_SCRIPTS = NULL;
    delete TO_TRY;
    TO_TRY = NULL;
    delete UNIHAN;
    UNIHAN = NULL;
    delete EMPTY_STRING;
    EMPTY_STRING = NULL;
    nfkdNormalizer = NULL;  // ref to a singleton.  Do not delete.
    indexCharactersAreInitialized = FALSE;
}


UnicodeSet *AlphabeticIndex::ALPHABETIC;
UnicodeSet *AlphabeticIndex::HANGUL;
UnicodeSet *AlphabeticIndex::ETHIOPIC;
UnicodeSet *AlphabeticIndex::CORE_LATIN;
UnicodeSet *AlphabeticIndex::IGNORE_SCRIPTS;
UnicodeSet *AlphabeticIndex::TO_TRY;
UnicodeSet *AlphabeticIndex::UNIHAN;
const UnicodeString *AlphabeticIndex::EMPTY_STRING;

//
//  staticInit()    One-time initialization of constants.
//                  Thread safe.  Called from constructors.
//                  Mutex overhead is not a concern.  AlphabeticIndex constructors are
//                  sufficiently heavy that the cost of the mutex check is not significant.

void AlphabeticIndex::staticInit(UErrorCode &status) {
    static UMTX IndexCharsInitMutex;

    Mutex mutex(&IndexCharsInitMutex);
    if (indexCharactersAreInitialized || U_FAILURE(status)) {
        return;
    }
    UBool finishedInit = FALSE;

    {
        UnicodeString alphaString = UNICODE_STRING_SIMPLE("[[:alphabetic:]-[:mark:]]");
        ALPHABETIC = new UnicodeSet(alphaString, status);
        if (ALPHABETIC == NULL) {
            goto err;
        }

        HANGUL = new UnicodeSet();
        HANGUL->add(0xAC00).add(0xB098).add(0xB2E4).add(0xB77C).add(0xB9C8).add(0xBC14).add(0xC0AC).
                add(0xC544).add(0xC790).add(0xCC28).add(0xCE74).add(0xD0C0).add(0xD30C).add(0xD558);
        if (HANGUL== NULL) {
            goto err;
        }


        UnicodeString EthiopicStr = UNICODE_STRING_SIMPLE("[[:Block=Ethiopic:]&[:Script=Ethiopic:]]");
        ETHIOPIC = new UnicodeSet(EthiopicStr, status);
        if (ETHIOPIC == NULL) {
            goto err;
        }

        CORE_LATIN = new UnicodeSet((UChar32)0x61, (UChar32)0x7a);  // ('a', 'z');
        if (CORE_LATIN == NULL) {
            goto err;
        }

        UnicodeString IgnoreStr= UNICODE_STRING_SIMPLE(
                "[[:sc=Common:][:sc=inherited:][:script=Unknown:][:script=braille:]]");
        IGNORE_SCRIPTS = new UnicodeSet(IgnoreStr, status);
        IGNORE_SCRIPTS->freeze();
        if (IGNORE_SCRIPTS == NULL) {
            goto err;
        }

        UnicodeString nfcqcStr = UNICODE_STRING_SIMPLE("[:^nfcqc=no:]");
        TO_TRY = new UnicodeSet(nfcqcStr, status);
        if (TO_TRY == NULL) {
            goto err;
        }

        UnicodeString unihanStr = UNICODE_STRING_SIMPLE("[:script=Hani:]");
        UNIHAN = new UnicodeSet(unihanStr, status);
        if (UNIHAN == NULL) {
            goto err;
        }

        EMPTY_STRING = new UnicodeString();

        nfkdNormalizer = Normalizer2::getNFKDInstance(status);
        if (nfkdNormalizer == NULL) {
            goto err;
        }
    }
    finishedInit = TRUE;

  err:
    if (!finishedInit && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        indexCharacters_cleanup();
        return;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_INDEX_CHARACTERS, indexCharacters_cleanup);
    indexCharactersAreInitialized = TRUE;
}


//
//  Comparison function for UVector<UnicodeString *> sorting with a collator.
//
static int32_t U_CALLCONV
sortCollateComparator(const void *context, const void *left, const void *right) {
    const UElement *leftElement = static_cast<const UElement *>(left);
    const UElement *rightElement = static_cast<const UElement *>(right);
    const UnicodeString *leftString  = static_cast<const UnicodeString *>(leftElement->pointer);
    const UnicodeString *rightString = static_cast<const UnicodeString *>(rightElement->pointer);
    const Collator *col = static_cast<const Collator *>(context);

    if (leftString == rightString) {
        // Catches case where both are NULL
        return 0;
    }
    if (leftString == NULL) {
        return 1;
    };
    if (rightString == NULL) {
        return -1;
    }
    Collator::EComparisonResult r = col->compare(*leftString, *rightString);
    return (int32_t) r;
}

//
//  Comparison function for UVector<Record *> sorting with a collator.
//
static int32_t U_CALLCONV
recordCompareFn(const void *context, const void *left, const void *right) {
    const UElement *leftElement = static_cast<const UElement *>(left);
    const UElement *rightElement = static_cast<const UElement *>(right);
    const AlphabeticIndex::Record *leftRec  = static_cast<const AlphabeticIndex::Record *>(leftElement->pointer);
    const AlphabeticIndex::Record *rightRec = static_cast<const AlphabeticIndex::Record *>(rightElement->pointer);
    const Collator *col = static_cast<const Collator *>(context);

    Collator::EComparisonResult r = col->compare(leftRec->sortingName_, rightRec->sortingName_);
    if (r == Collator::EQUAL) {
        if (leftRec->serialNumber_ < rightRec->serialNumber_) {
            r = Collator::LESS;
        } else if (leftRec->serialNumber_ > rightRec->serialNumber_) {
            r = Collator::GREATER;
        }
    }
    return (int32_t) r;
}


#if 0
//
//  First characters in scripts.
//  Create a UVector whose contents are pointers to UnicodeStrings for the First Characters in each script.
//  The vector is sorted according to this index's collation.
//
//  This code is too slow to use, so for now hard code the data.
//    Hard coded implementation is follows.
//
UVector *AlphabeticIndex::firstStringsInScript(Collator *ruleBasedCollator, UErrorCode &status) {

    if (U_FAILURE(status)) {
        return NULL;
    }

    UnicodeString results[USCRIPT_CODE_LIMIT];
    UnicodeString LOWER_A = UNICODE_STRING_SIMPLE("a");

    UnicodeSetIterator siter(*TO_TRY);
    while (siter.next()) {
        const UnicodeString &current = siter.getString();
        Collator::EComparisonResult r = ruleBasedCollator->compare(current, LOWER_A);
        if (r < 0) {  // TODO fix; we only want "real" script characters, not
                      // symbols.
            continue;
        }

        int script = uscript_getScript(current.char32At(0), &status);
        if (results[script].length() == 0) {
            results[script] = current;
        }
        else if (ruleBasedCollator->compare(current, results[script]) < 0) {
            results[script] = current;
        }
    }

    UnicodeSet extras;
    UnicodeSet expansions;
    RuleBasedCollator *rbc = dynamic_cast<RuleBasedCollator *>(ruleBasedCollator);
    const UCollator *uRuleBasedCollator = rbc->getUCollator();
    ucol_getContractionsAndExpansions(uRuleBasedCollator, extras.toUSet(), expansions.toUSet(), true, &status);
    extras.addAll(expansions).removeAll(*TO_TRY);
    if (extras.size() != 0) {
        const Normalizer2 *normalizer = Normalizer2::getNFKCInstance(status);
        UnicodeSetIterator extrasIter(extras);
        while (extrasIter.next()) {
            const UnicodeString &current = extrasIter.next();
            if (!TO_TRY->containsAll(current))
                continue;
            if (!normalizer->isNormalized(current, status) ||
                ruleBasedCollator->compare(current, LOWER_A) < 0) {
                continue;
            }
            int script = uscript_getScript(current.char32At(0), &status);
            if (results[script].length() == 0) {
                results[script] = current;
            } else if (ruleBasedCollator->compare(current, results[script]) < 0) {
                results[script] = current;
            }
        }
    }

    UVector *dest = new UVector(status);
    dest->setDeleter(uprv_deleteUObject);
    for (uint32_t i = 0; i < sizeof(results) / sizeof(results[0]); ++i) {
        if (results[i].length() > 0) {
            dest->addElement(results[i].clone(), status);
        }
    }
    dest->sortWithUComparator(sortCollateComparator, ruleBasedCollator, status);
    return dest;
}
#endif


//
//  First characters in scripts.
//  Create a UVector whose contents are pointers to UnicodeStrings for the First Characters in each script.
//  The vector is sorted according to this index's collation.
//
//  It takes too much time to compute this from character properties, so hard code it for now.
//  Character constants copied from corresponding declaration in ICU4J.
//  See main/classes/collate/src/com/ibm/icu/text/AlphabeticIndex.java

static UChar HACK_FIRST_CHARS_IN_SCRIPTS[] =  { 0x61, 0, 0x03B1, 0,
            0x2C81, 0, 0x0430, 0, 0x2C30, 0, 0x10D0, 0, 0x0561, 0, 0x05D0, 0, 0xD802, 0xDD00, 0, 0x0800, 0, 0x0621, 0, 0x0710, 0,
            0x0780, 0, 0x07CA, 0, 0x2D30, 0, 0x1200, 0, 0x0950, 0, 0x0985, 0, 0x0A74, 0, 0x0AD0, 0, 0x0B05, 0, 0x0BD0, 0,
            0x0C05, 0, 0x0C85, 0, 0x0D05, 0, 0x0D85, 0,
            0xAAF2, 0,  // Meetei Mayek
            0xA800, 0, 0xA882, 0, 0xD804, 0xDC83, 0,
            U16_LEAD(0x111C4), U16_TRAIL(0x111C4), 0,  // Sharada
            U16_LEAD(0x11680), U16_TRAIL(0x11680), 0,  // Takri
            0x1B83, 0,
            0xD802, 0xDE00, 0, 0x0E01, 0,
            0x0EDE, 0,  // Lao
            0xAA80, 0, 0x0F40, 0, 0x1C00, 0, 0xA840, 0, 0x1900, 0, 0x1700, 0, 0x1720, 0,
            0x1740, 0, 0x1760, 0, 0x1A00, 0, 0xA930, 0, 0xA90A, 0, 0x1000, 0,
            U16_LEAD(0x11103), U16_TRAIL(0x11103), 0,  // Chakma
            0x1780, 0, 0x1950, 0, 0x1980, 0, 0x1A20, 0,
            0xAA00, 0, 0x1B05, 0, 0xA984, 0, 0x1880, 0, 0x1C5A, 0, 0x13A0, 0, 0x1401, 0, 0x1681, 0, 0x16A0, 0, 0xD803, 0xDC00, 0,
            0xA500, 0, 0xA6A0, 0, 0x1100, 0, 0x3041, 0, 0x30A1, 0, 0x3105, 0, 0xA000, 0, 0xA4F8, 0,
            U16_LEAD(0x16F00), U16_TRAIL(0x16F00), 0,  // Miao
            0xD800, 0xDE80, 0,
            0xD800, 0xDEA0, 0, 0xD802, 0xDD20, 0, 0xD800, 0xDF00, 0, 0xD800, 0xDF30, 0, 0xD801, 0xDC28, 0, 0xD801, 0xDC50, 0,
            0xD801, 0xDC80, 0,
            U16_LEAD(0x110D0), U16_TRAIL(0x110D0), 0,  // Sora Sompeng
            0xD800, 0xDC00, 0, 0xD802, 0xDC00, 0, 0xD802, 0xDE60, 0, 0xD802, 0xDF00, 0, 0xD802, 0xDC40, 0,
            0xD802, 0xDF40, 0, 0xD802, 0xDF60, 0, 0xD800, 0xDF80, 0, 0xD800, 0xDFA0, 0, 0xD808, 0xDC00, 0, 0xD80C, 0xDC00, 0,
            U16_LEAD(0x109A0), U16_TRAIL(0x109A0), 0,  // Meroitic Cursive
            U16_LEAD(0x10980), U16_TRAIL(0x10980), 0,  // Meroitic Hieroglyphs
            0x4E00, 0 };

UVector *AlphabeticIndex::firstStringsInScript(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    UVector *dest = new UVector(status);
    if (dest == NULL) {
        if (U_SUCCESS(status)) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        return NULL;
    }
    dest->setDeleter(uprv_deleteUObject);
    const UChar *src  = HACK_FIRST_CHARS_IN_SCRIPTS;
    const UChar *limit = src + sizeof(HACK_FIRST_CHARS_IN_SCRIPTS) / sizeof(HACK_FIRST_CHARS_IN_SCRIPTS[0]);
    do {
        if (U_FAILURE(status)) {
            return dest;
        }
        UnicodeString *str = new UnicodeString(src, -1);
        if (str == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        } else {
            dest->addElement(str, status);
            src += str->length() + 1;
        }
    } while (src < limit);
    dest->sortWithUComparator(sortCollateComparator, collator_, status);
    return dest;
}


AlphabeticIndex::ELangType AlphabeticIndex::langTypeFromLocale(const Locale &loc) {
    const char *lang = loc.getLanguage();
    if (uprv_strcmp(lang, "zh") != 0) {
        return kNormal;
    }
    const char *script = loc.getScript();
    if (uprv_strcmp(script, "Hant") == 0) {
        return kTraditional;
    }
    const char *country = loc.getCountry();
    if (uprv_strcmp(country, "TW") == 0) {
        return kTraditional;
    }
    return kSimplified;
}


//
// Pinyin Hacks.  Direct port from Java.
//

static const UChar32  probeCharInLong = 0x28EAD;


static const UChar PINYIN_LOWER_BOUNDS_SHORT[] = {      // "\u0101bcd\u0113fghjkl\u1E3F\u0144\u014Dpqrstwxyz"
            0x0101, 0x62, 0x63, 0x64, 0x0113, 0x66, 0x67, 0x68, 0x6A, 0x6B, /*l*/0x6C, 0x1E3F, 0x0144, 0x014D,
            /*p*/0x70, 0x71, 0x72, 0x73, 0x74, /*w*/0x77, 0x78, 0x79, 0x7A};


// Pinyin lookup tables copied, pasted (and reformatted) from the ICU4J code.

AlphabeticIndex::PinyinLookup AlphabeticIndex::HACK_PINYIN_LOOKUP_SHORT = {
        {(UChar)0,      (UChar)0, (UChar)0}, // A 
        {(UChar)0x516B, (UChar)0, (UChar)0}, // B 
        {(UChar)0x5693, (UChar)0, (UChar)0}, // C 
        {(UChar)0x5491, (UChar)0, (UChar)0}, // D 
        {(UChar)0x59B8, (UChar)0, (UChar)0}, // E 
        {(UChar)0x53D1, (UChar)0, (UChar)0}, // F 
        {(UChar)0x65EE, (UChar)0, (UChar)0}, // G 
        {(UChar)0x54C8, (UChar)0, (UChar)0}, // H 
        {(UChar)0x4E0C, (UChar)0, (UChar)0}, // J 
        {(UChar)0x5494, (UChar)0, (UChar)0}, // K 
        {(UChar)0x5783, (UChar)0, (UChar)0}, // L 
        {(UChar)0x5452, (UChar)0, (UChar)0}, // M 
        {(UChar)0x5514, (UChar)0, (UChar)0}, // N 
        {(UChar)0x5594, (UChar)0, (UChar)0}, // O 
        {(UChar)0x5991, (UChar)0, (UChar)0}, // P 
        {(UChar)0x4E03, (UChar)0, (UChar)0}, // Q 
        {(UChar)0x513F, (UChar)0, (UChar)0}, // R 
        {(UChar)0x4EE8, (UChar)0, (UChar)0}, // S 
        {(UChar)0x4ED6, (UChar)0, (UChar)0}, // T 
        {(UChar)0x7A75, (UChar)0, (UChar)0}, // W 
        {(UChar)0x5915, (UChar)0, (UChar)0}, // X 
        {(UChar)0x4E2B, (UChar)0, (UChar)0}, // Y 
        {(UChar)0x5E00, (UChar)0, (UChar)0}, // Z 
        {(UChar)0xFFFF, (UChar)0, (UChar)0}, // mark end of array 
    };

static const UChar PINYIN_LOWER_BOUNDS_LONG[] = {   // "\u0101bcd\u0113fghjkl\u1E3F\u0144\u014Dpqrstwxyz";
            0x0101, 0x62, 0x63, 0x64, 0x0113, 0x66, 0x67, 0x68, 0x6A, 0x6B, /*l*/0x6C, 0x1E3F, 0x0144, 0x014D,
            /*p*/0x70, 0x71, 0x72, 0x73, 0x74, /*w*/0x77, 0x78, 0x79, 0x7A};

AlphabeticIndex::PinyinLookup AlphabeticIndex::HACK_PINYIN_LOOKUP_LONG = {
        {(UChar)0,      (UChar)0,      (UChar)0}, // A
        {(UChar)0x516B, (UChar)0,      (UChar)0}, // b 
        {(UChar)0xD863, (UChar)0xDEAD, (UChar)0}, // c 
        {(UChar)0xD844, (UChar)0xDE51, (UChar)0}, // d 
        {(UChar)0x59B8, (UChar)0,      (UChar)0}, // e 
        {(UChar)0x53D1, (UChar)0,      (UChar)0}, // f 
        {(UChar)0xD844, (UChar)0xDE45, (UChar)0}, // g 
        {(UChar)0x54C8, (UChar)0,      (UChar)0}, // h 
        {(UChar)0x4E0C, (UChar)0,      (UChar)0}, // j 
        {(UChar)0x5494, (UChar)0,      (UChar)0}, // k 
        {(UChar)0x3547, (UChar)0,      (UChar)0}, // l 
        {(UChar)0x5452, (UChar)0,      (UChar)0}, // m 
        {(UChar)0x5514, (UChar)0,      (UChar)0}, // n 
        {(UChar)0x5594, (UChar)0,      (UChar)0}, // o 
        {(UChar)0xD84F, (UChar)0xDC7A, (UChar)0}, // p 
        {(UChar)0x4E03, (UChar)0,      (UChar)0}, // q 
        {(UChar)0x513F, (UChar)0,      (UChar)0}, // r 
        {(UChar)0x4EE8, (UChar)0,      (UChar)0}, // s 
        {(UChar)0x4ED6, (UChar)0,      (UChar)0}, // t 
        {(UChar)0x7A75, (UChar)0,      (UChar)0}, // w 
        {(UChar)0x5915, (UChar)0,      (UChar)0}, // x 
        {(UChar)0x4E2B, (UChar)0,      (UChar)0}, // y 
        {(UChar)0x5E00, (UChar)0,      (UChar)0}, // z 
        {(UChar)0xFFFF, (UChar)0,      (UChar)0}, // mark end of array 
    };


//
//  Probe the collation data, and decide which Pinyin tables should be used
//
//  ICU can be built with a choice between two Chinese collations.
//  The hack Pinyin tables to use depend on which one is in use.
//  We can assume that any given copy of ICU will have only one of the collations available,
//  and that there is no way, in a given process, to create two alphabetic indexes using
//  different Chinese collations.  Which means the probe can be done once
//  and the results cached.
//
//  This whole arrangement is temporary.
//
AlphabeticIndex::PinyinLookup *AlphabeticIndex::HACK_PINYIN_LOOKUP  = NULL;
const UChar  *AlphabeticIndex::PINYIN_LOWER_BOUNDS = NULL;

void AlphabeticIndex::initPinyinBounds(const Collator *col, UErrorCode &status) {
    {
        Mutex m;
        if (PINYIN_LOWER_BOUNDS != NULL) {
            return;
        }
    }
    UnicodeSet *colSet = col->getTailoredSet(status);
    if (U_FAILURE(status) || colSet == NULL) {
        delete colSet;
        if (U_SUCCESS(status))  {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        return;
    }
    UBool useLongTables = colSet->contains(probeCharInLong);
    delete colSet;
    {
        Mutex m;
        if (useLongTables) {
            PINYIN_LOWER_BOUNDS = PINYIN_LOWER_BOUNDS_LONG;
            HACK_PINYIN_LOOKUP  = &HACK_PINYIN_LOOKUP_LONG;
        } else {
            PINYIN_LOWER_BOUNDS = PINYIN_LOWER_BOUNDS_SHORT;
            HACK_PINYIN_LOOKUP  = &HACK_PINYIN_LOOKUP_SHORT;
        }
    }
}

// Pinyin Hack:
//    Modify a Chinese name by prepending a Latin letter.  The modified name is used
//      when putting records (names) into buckets, to put the name under a Latin index heading.

void AlphabeticIndex::hackName(UnicodeString &dest, const UnicodeString &name, const Collator *col) {

    if (langType_ != kSimplified || !UNIHAN->contains(name.char32At(0))) {
        dest = name;
        return;
    }

    UErrorCode status = U_ZERO_ERROR;
    initPinyinBounds(col, status);
    if (U_FAILURE(status)) {
        dest = name;
        return;
    }
    // TODO:  use binary search
    int index;
    for (index=0; ; index++) {
        if ((*HACK_PINYIN_LOOKUP)[index][0] == (UChar)0xffff) {
            index--;
            break;
        }
        int32_t compareResult = col->compare(name, UnicodeString(TRUE, (*HACK_PINYIN_LOOKUP)[index], -1));
        if (compareResult < 0) {
            index--;
        }
        if (compareResult <= 0) {
            break;
        }
    }
    UChar c = PINYIN_LOWER_BOUNDS[index];
    dest.setTo(c);
    dest.append(name);
    return;
}



/**
 * Comparator that returns "better" items first, where shorter NFKD is better, and otherwise NFKD binary order is
 * better, and otherwise binary order is better.
 *
 * For use with array sort or UVector.
 * @param context  A UErrorCode pointer.
 * @param left     A UElement pointer, which must refer to a UnicodeString *
 * @param right    A UElement pointer, which must refer to a UnicodeString *
 */

static int32_t U_CALLCONV
PreferenceComparator(const void *context, const void *left, const void *right) {
    const UElement *leftElement  = static_cast<const UElement *>(left);
    const UElement *rightElement = static_cast<const UElement *>(right);
    const UnicodeString *s1  = static_cast<const UnicodeString *>(leftElement->pointer);
    const UnicodeString *s2  = static_cast<const UnicodeString *>(rightElement->pointer);
    UErrorCode &status       = *(UErrorCode *)(context);   // Cast off both static and const.
    if (s1 == s2) {
        return 0;
    }

    UnicodeString n1 = nfkdNormalizer->normalize(*s1, status);
    UnicodeString n2 = nfkdNormalizer->normalize(*s2, status);
    int32_t result = n1.length() - n2.length();
    if (result != 0) {
        return result;
    }

    result = n1.compareCodePointOrder(n2);
    if (result != 0) {
        return result;
    }
    return s1->compareCodePointOrder(*s2);
}


//
//  Constructor & Destructor for AlphabeticIndex::Record
//
//     Records are internal only, instances are not directly surfaced in the public API.
//     This class is mostly struct-like, with all public fields.

AlphabeticIndex::Record::Record(AlphabeticIndex *alphaIndex, const UnicodeString &name, const void *data):
    alphaIndex_(alphaIndex), name_(name), data_(data) 
{
    UnicodeString prefixedName;
    alphaIndex->hackName(sortingName_, name_, alphaIndex->collatorPrimaryOnly_);
    serialNumber_ = ++alphaIndex->recordCounter_;
}
    
AlphabeticIndex::Record::~Record() {
}


AlphabeticIndex & AlphabeticIndex::addRecord(const UnicodeString &name, const void *data, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    Record *r = new Record(this, name, data);
    inputRecords_->addElement(r, status);
    indexBuildRequired_ = TRUE;
    //std::string ss;
    //std::string ss2;
    //std::cout << "added record: name = \"" << r->name_.toUTF8String(ss) << "\"" << 
    //             "   sortingName = \"" << r->sortingName_.toUTF8String(ss2) << "\"" << std::endl;
    return *this;
}


AlphabeticIndex &AlphabeticIndex::clearRecords(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    inputRecords_->removeAllElements();
    indexBuildRequired_ = TRUE;
    return *this;
}


int32_t AlphabeticIndex::getBucketIndex(const UnicodeString &name, UErrorCode &status) {
    buildIndex(status);
    if (U_FAILURE(status)) {
        return 0;
    }

    // For simplified Chinese prepend a prefix to the name.
    //   For non-Chinese locales or non-Chinese names, the name is not modified.

    UnicodeString prefixedName;
    hackName(prefixedName, name, collatorPrimaryOnly_);

    // TODO:  use a binary search.
    for (int32_t i = 0; i < bucketList_->size(); ++i) {
        Bucket *bucket = static_cast<Bucket *>(bucketList_->elementAt(i));
        Collator::EComparisonResult comp = collatorPrimaryOnly_->compare(prefixedName, bucket->lowerBoundary_);
        if (comp < 0) {
            return i - 1;
        }
    }
    // Loop runs until we find the bucket following the one that would hold prefixedName.
    // If the prefixedName belongs in the last bucket the loop will drop out the bottom rather
    //  than returning from the middle.

    return bucketList_->size() - 1;
}


int32_t AlphabeticIndex::getBucketIndex() const {
    return labelsIterIndex_;
}


UBool AlphabeticIndex::nextBucket(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (indexBuildRequired_ && currentBucket_ != NULL) {
        status = U_ENUM_OUT_OF_SYNC_ERROR;
        return FALSE;
    }
    buildIndex(status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    ++labelsIterIndex_;
    if (labelsIterIndex_ >= bucketList_->size()) {
        labelsIterIndex_ = bucketList_->size();
        return FALSE;
    }
    currentBucket_ = static_cast<Bucket *>(bucketList_->elementAt(labelsIterIndex_));
    resetRecordIterator();
    return TRUE;
}

const UnicodeString &AlphabeticIndex::getBucketLabel() const {
    if (currentBucket_ != NULL) {
        return currentBucket_->label_;
    } else {
        return *EMPTY_STRING;
    }
}


UAlphabeticIndexLabelType AlphabeticIndex::getBucketLabelType() const {
    if (currentBucket_ != NULL) {
        return currentBucket_->labelType_;
    } else {
        return U_ALPHAINDEX_NORMAL;
    }
}


int32_t AlphabeticIndex::getBucketRecordCount() const {
    if (currentBucket_ != NULL) {
        return currentBucket_->records_->size();
    } else {
        return 0;
    }
}


AlphabeticIndex &AlphabeticIndex::resetBucketIterator(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    buildIndex(status);
    labelsIterIndex_ = -1;
    currentBucket_ = NULL;
    return *this;
}


UBool AlphabeticIndex::nextRecord(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (currentBucket_ == NULL) {
        // We are trying to iterate over the items in a bucket, but there is no
        // current bucket from the enumeration of buckets.
        status = U_INVALID_STATE_ERROR;
        return FALSE;
    }
    if (indexBuildRequired_) {
        status = U_ENUM_OUT_OF_SYNC_ERROR;
        return FALSE;
    }
    ++itemsIterIndex_;
    if (itemsIterIndex_ >= currentBucket_->records_->size()) {
        itemsIterIndex_  = currentBucket_->records_->size();
        return FALSE;
    }
    return TRUE;
}


const UnicodeString &AlphabeticIndex::getRecordName() const {
    const UnicodeString *retStr = EMPTY_STRING;
    if (currentBucket_ != NULL &&
        itemsIterIndex_ >= 0 &&
        itemsIterIndex_ < currentBucket_->records_->size()) {
            Record *item = static_cast<Record *>(currentBucket_->records_->elementAt(itemsIterIndex_));
            retStr = &item->name_;
    }
    return *retStr;
}

const void *AlphabeticIndex::getRecordData() const {
    const void *retPtr = NULL;
    if (currentBucket_ != NULL &&
        itemsIterIndex_ >= 0 &&
        itemsIterIndex_ < currentBucket_->records_->size()) {
            Record *item = static_cast<Record *>(currentBucket_->records_->elementAt(itemsIterIndex_));
            retPtr = item->data_;
    }
    return retPtr;
}


AlphabeticIndex & AlphabeticIndex::resetRecordIterator() {
    itemsIterIndex_ = -1;
    return *this;
}



AlphabeticIndex::Bucket::Bucket(const UnicodeString &label,
                                const UnicodeString &lowerBoundary,
                                UAlphabeticIndexLabelType type,
                                UErrorCode &status):
         label_(label), lowerBoundary_(lowerBoundary), labelType_(type), records_(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    records_ = new UVector(status);
    if (records_ == NULL && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}


AlphabeticIndex::Bucket::~Bucket() {
    delete records_;
}

U_NAMESPACE_END

#endif
