/*
*******************************************************************************
* Copyright (C) 2009-2013, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION && !UCONFIG_NO_NORMALIZATION

#include "unicode/alphaindex.h"
#include "unicode/coleitr.h"
#include "unicode/coll.h"
#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "unicode/tblcoll.h"
#include "unicode/ulocdata.h"
#include "unicode/uniset.h"
#include "unicode/uobject.h"
#include "unicode/usetiter.h"
#include "unicode/utf16.h"

#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "uvector.h"

//#include <string>
//#include <iostream>

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

namespace {

/**
 * Prefix string for Chinese index buckets.
 * See http://unicode.org/repos/cldr/trunk/specs/ldml/tr35-collation.html#Collation_Indexes
 */
const UChar BASE[1] = { 0xFDD0 };
const int32_t BASE_LENGTH = 1;

UBool isOneLabelBetterThanOther(const Normalizer2 &nfkdNormalizer,
                                const UnicodeString &one, const UnicodeString &other);

}  // namespace

static int32_t U_CALLCONV
collatorComparator(const void *context, const void *left, const void *right);

static int32_t U_CALLCONV
recordCompareFn(const void *context, const void *left, const void *right);

//  UVector<Record *> support function, delete a Record.
static void U_CALLCONV
alphaIndex_deleteRecord(void *obj) {
    delete static_cast<AlphabeticIndex::Record *>(obj);
}

namespace {

UnicodeString *ownedString(const UnicodeString &s, LocalPointer<UnicodeString> &owned,
                           UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return NULL; }
    if (owned.isValid()) {
        return owned.orphan();
    }
    UnicodeString *p = new UnicodeString(s);
    if (p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return p;
}

inline UnicodeString *getString(const UVector &list, int32_t i) {
    return static_cast<UnicodeString *>(list[i]);
}

inline AlphabeticIndex::Bucket *getBucket(const UVector &list, int32_t i) {
    return static_cast<AlphabeticIndex::Bucket *>(list[i]);
}

inline AlphabeticIndex::Record *getRecord(const UVector &list, int32_t i) {
    return static_cast<AlphabeticIndex::Record *>(list[i]);
}

/**
 * Like Java Collections.binarySearch(List, String, Comparator).
 *
 * @return the index>=0 where the item was found,
 *         or the index<0 for inserting the string at ~index in sorted order
 */
int32_t binarySearch(const UVector &list, const UnicodeString &s, const Collator &coll) {
    if (list.size() == 0) { return ~0; }
    int32_t start = 0;
    int32_t limit = list.size();
    for (;;) {
        int32_t i = (start + limit) / 2;
        const UnicodeString *si = static_cast<UnicodeString *>(list.elementAt(i));
        UErrorCode errorCode = U_ZERO_ERROR;
        UCollationResult cmp = coll.compare(s, *si, errorCode);
        if (cmp == UCOL_EQUAL) {
            return i;
        } else if (cmp < 0) {
            if (i == start) {
                return ~start;  // insert s before *si
            }
            limit = i;
        } else {
            if (i == start) {
                return ~(start + 1);  // insert s after *si
            }
            start = i;
        }
    }
}

}  // namespace

// The BucketList is not in the anonymous namespace because only Clang
// seems to support its use in other classes from there.
// However, we also don't need U_I18N_API because it is not used from outside the i18n library.
class BucketList : public UObject {
public:
    BucketList(UVector *bucketList, UVector *publicBucketList)
            : bucketList_(bucketList), immutableVisibleList_(publicBucketList) {
        int32_t displayIndex = 0;
        for (int32_t i = 0; i < publicBucketList->size(); ++i) {
            getBucket(*publicBucketList, i)->displayIndex_ = displayIndex++;
        }
    }

    // The virtual destructor must not be inline.
    // See ticket #8454 for details.
    virtual ~BucketList();

    int32_t getBucketCount() const {
        return immutableVisibleList_->size();
    }

    int32_t getBucketIndex(const UnicodeString &name, const Collator &collatorPrimaryOnly,
                           UErrorCode &errorCode) {
        // binary search
        int32_t start = 0;
        int32_t limit = bucketList_->size();
        while ((start + 1) < limit) {
            int32_t i = (start + limit) / 2;
            const AlphabeticIndex::Bucket *bucket = getBucket(*bucketList_, i);
            UCollationResult nameVsBucket =
                collatorPrimaryOnly.compare(name, bucket->lowerBoundary_, errorCode);
            if (nameVsBucket < 0) {
                limit = i;
            } else {
                start = i;
            }
        }
        const AlphabeticIndex::Bucket *bucket = getBucket(*bucketList_, start);
        if (bucket->displayBucket_ != NULL) {
            bucket = bucket->displayBucket_;
        }
        return bucket->displayIndex_;
    }

    /** All of the buckets, visible and invisible. */
    UVector *bucketList_;
    /** Just the visible buckets. */
    UVector *immutableVisibleList_;
};

BucketList::~BucketList() {
    delete bucketList_;
    if (immutableVisibleList_ != bucketList_) {
        delete immutableVisibleList_;
    }
}

AlphabeticIndex::ImmutableIndex::~ImmutableIndex() {
    delete buckets_;
    delete collatorPrimaryOnly_;
}

int32_t
AlphabeticIndex::ImmutableIndex::getBucketCount() const {
    return buckets_->getBucketCount();
}

int32_t
AlphabeticIndex::ImmutableIndex::getBucketIndex(
        const UnicodeString &name, UErrorCode &errorCode) const {
    return buckets_->getBucketIndex(name, *collatorPrimaryOnly_, errorCode);
}

const AlphabeticIndex::Bucket *
AlphabeticIndex::ImmutableIndex::getBucket(int32_t index) const {
    if (0 <= index && index < buckets_->getBucketCount()) {
        return icu::getBucket(*buckets_->immutableVisibleList_, index);
    } else {
        return NULL;
    }
}

AlphabeticIndex::AlphabeticIndex(const Locale &locale, UErrorCode &status)
        : inputList_(NULL),
          labelsIterIndex_(-1), itemsIterIndex_(0), currentBucket_(NULL),
          maxLabelCount_(99),
          initialLabels_(NULL), firstCharsInScripts_(NULL),
          collator_(NULL), collatorPrimaryOnly_(NULL),
          buckets_(NULL) {
    init(&locale, status);
}


AlphabeticIndex::AlphabeticIndex(RuleBasedCollator *collator, UErrorCode &status)
        : inputList_(NULL),
          labelsIterIndex_(-1), itemsIterIndex_(0), currentBucket_(NULL),
          maxLabelCount_(99),
          initialLabels_(NULL), firstCharsInScripts_(NULL),
          collator_(collator), collatorPrimaryOnly_(NULL),
          buckets_(NULL) {
    init(NULL, status);
}



AlphabeticIndex::~AlphabeticIndex() {
    delete collator_;
    delete collatorPrimaryOnly_;
    delete firstCharsInScripts_;
    delete buckets_;
    delete inputList_;
    delete initialLabels_;
}


AlphabeticIndex &AlphabeticIndex::addLabels(const UnicodeSet &additions, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    initialLabels_->addAll(additions);
    clearBuckets();
    return *this;
}


AlphabeticIndex &AlphabeticIndex::addLabels(const Locale &locale, UErrorCode &status) {
    addIndexExemplars(locale, status);
    clearBuckets();
    return *this;
}


AlphabeticIndex::ImmutableIndex *AlphabeticIndex::buildImmutableIndex(UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return NULL; }
    // In C++, the ImmutableIndex must own its copy of the BucketList,
    // even if it contains no records, for proper memory management.
    // We could clone the buckets_ if they are not NULL,
    // but that would be worth it only if this method is called multiple times,
    // or called after using the old-style bucket iterator API.
    LocalPointer<BucketList> immutableBucketList(createBucketList(errorCode));
    LocalPointer<RuleBasedCollator> coll(
        static_cast<RuleBasedCollator *>(collatorPrimaryOnly_->clone()));
    if (immutableBucketList.isNull() || coll.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    ImmutableIndex *immIndex = new ImmutableIndex(immutableBucketList.getAlias(), coll.getAlias());
    if (immIndex == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    // The ImmutableIndex adopted its parameter objects.
    immutableBucketList.orphan();
    coll.orphan();
    return immIndex;
}

int32_t AlphabeticIndex::getBucketCount(UErrorCode &status) {
    initBuckets(status);
    if (U_FAILURE(status)) {
        return 0;
    }
    return buckets_->getBucketCount();
}


int32_t AlphabeticIndex::getRecordCount(UErrorCode &status) {
    if (U_FAILURE(status) || inputList_ == NULL) {
        return 0;
    }
    return inputList_->size();
}

void AlphabeticIndex::initLabels(UVector &indexCharacters, UErrorCode &errorCode) const {
    const Normalizer2 *nfkdNormalizer = Normalizer2::getNFKDInstance(errorCode);
    if (U_FAILURE(errorCode)) { return; }

    const UnicodeString &firstScriptBoundary = *getString(*firstCharsInScripts_, 0);
    const UnicodeString &overflowBoundary =
        *getString(*firstCharsInScripts_, firstCharsInScripts_->size() - 1);

    // We make a sorted array of elements.
    // Some of the input may be redundant.
    // That is, we might have c, ch, d, where "ch" sorts just like "c", "h".
    // We filter out those cases.
    UnicodeSetIterator iter(*initialLabels_);
    while (iter.next()) {
        const UnicodeString *item = &iter.getString();
        LocalPointer<UnicodeString> ownedItem;
        UBool checkDistinct;
        int32_t itemLength = item->length();
        if (!item->hasMoreChar32Than(0, itemLength, 1)) {
            checkDistinct = FALSE;
        } else if(item->charAt(itemLength - 1) == 0x2a &&  // '*'
                item->charAt(itemLength - 2) != 0x2a) {
            // Use a label if it is marked with one trailing star,
            // even if the label string sorts the same when all contractions are suppressed.
            ownedItem.adoptInstead(new UnicodeString(*item, 0, itemLength - 1));
            item = ownedItem.getAlias();
            if (item == NULL) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            checkDistinct = FALSE;
        } else {
            checkDistinct = TRUE;
        }
        if (collatorPrimaryOnly_->compare(*item, firstScriptBoundary, errorCode) < 0) {
            // Ignore a primary-ignorable or non-alphabetic index character.
        } else if (collatorPrimaryOnly_->compare(*item, overflowBoundary, errorCode) >= 0) {
            // Ignore an index characters that will land in the overflow bucket.
        } else if (checkDistinct &&
                collatorPrimaryOnly_->compare(*item, separated(*item), errorCode) == 0) {
            // Ignore a multi-code point index character that does not sort distinctly
            // from the sequence of its separate characters.
        } else {
            int32_t insertionPoint = binarySearch(indexCharacters, *item, *collatorPrimaryOnly_);
            if (insertionPoint < 0) {
                indexCharacters.insertElementAt(
                    ownedString(*item, ownedItem, errorCode), ~insertionPoint, errorCode);
            } else {
                const UnicodeString &itemAlreadyIn = *getString(indexCharacters, insertionPoint);
                if (isOneLabelBetterThanOther(*nfkdNormalizer, *item, itemAlreadyIn)) {
                    indexCharacters.setElementAt(
                        ownedString(*item, ownedItem, errorCode), insertionPoint);
                }
            }
        }
    }
    if (U_FAILURE(errorCode)) { return; }

    // if the result is still too large, cut down to maxCount elements, by removing every nth element

    int32_t size = indexCharacters.size() - 1;
    if (size > maxLabelCount_) {
        int32_t count = 0;
        int32_t old = -1;
        for (int32_t i = 0; i < indexCharacters.size();) {
            ++count;
            int32_t bump = count * maxLabelCount_ / size;
            if (bump == old) {
                indexCharacters.removeElementAt(i);
            } else {
                old = bump;
                ++i;
            }
        }
    }
}

namespace {

const UnicodeString &fixLabel(const UnicodeString &current, UnicodeString &temp) {
    if (!current.startsWith(BASE, BASE_LENGTH)) {
        return current;
    }
    UChar rest = current.charAt(BASE_LENGTH);
    if (0x2800 < rest && rest <= 0x28FF) { // stroke count
        int32_t count = rest-0x2800;
        temp.setTo((UChar)(0x30 + count % 10));
        if (count >= 10) {
            count /= 10;
            temp.insert(0, (UChar)(0x30 + count % 10));
            if (count >= 10) {
                count /= 10;
                temp.insert(0, (UChar)(0x30 + count));
            }
        }
        return temp.append((UChar)0x5283);
    }
    return temp.setTo(current, BASE_LENGTH);
}

UBool hasMultiplePrimaryWeights(
        CollationElementIterator &cei, int32_t variableTop,
        const UnicodeString &s, UErrorCode &errorCode) {
    cei.setText(s, errorCode);
    UBool seenPrimary = FALSE;
    for (;;) {
        int32_t ce32 = cei.next(errorCode);
        if (ce32 == CollationElementIterator::NULLORDER) {
            break;
        }
        int32_t p = CollationElementIterator::primaryOrder(ce32);
        if (p > variableTop && (ce32 & 0xc0) != 0xc0) {
            // not primary ignorable, and not a continuation CE
            if (seenPrimary) {
                return TRUE;
            }
            seenPrimary = TRUE;
        }
    }
    return FALSE;
}

}  // namespace

BucketList *AlphabeticIndex::createBucketList(UErrorCode &errorCode) const {
    // Initialize indexCharacters.
    UVector indexCharacters(errorCode);
    indexCharacters.setDeleter(uprv_deleteUObject);
    initLabels(indexCharacters, errorCode);
    if (U_FAILURE(errorCode)) { return NULL; }

    // Variables for hasMultiplePrimaryWeights().
    LocalPointer<CollationElementIterator> cei(
        collatorPrimaryOnly_->createCollationElementIterator(emptyString_));
    if (cei.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    int32_t variableTop;
    if (collatorPrimaryOnly_->getAttribute(UCOL_ALTERNATE_HANDLING, errorCode) == UCOL_SHIFTED) {
        variableTop = CollationElementIterator::primaryOrder(
            (int32_t)collatorPrimaryOnly_->getVariableTop(errorCode));
    } else {
        variableTop = 0;
    }
    UBool hasInvisibleBuckets = FALSE;

    // Helper arrays for Chinese Pinyin collation.
    Bucket *asciiBuckets[26] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
    Bucket *pinyinBuckets[26] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
    UBool hasPinyin = FALSE;

    LocalPointer<UVector> bucketList(new UVector(errorCode));
    if (bucketList.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    bucketList->setDeleter(uprv_deleteUObject);

    // underflow bucket
    Bucket *bucket = new Bucket(getUnderflowLabel(), emptyString_, U_ALPHAINDEX_UNDERFLOW);
    if (bucket == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    bucketList->addElement(bucket, errorCode);
    if (U_FAILURE(errorCode)) { return NULL; }

    UnicodeString temp;

    // fix up the list, adding underflow, additions, overflow
    // Insert inflow labels as needed.
    int32_t scriptIndex = -1;
    const UnicodeString *scriptUpperBoundary = &emptyString_;
    for (int32_t i = 0; i < indexCharacters.size(); ++i) {
        UnicodeString &current = *getString(indexCharacters, i);
        if (collatorPrimaryOnly_->compare(current, *scriptUpperBoundary, errorCode) >= 0) {
            // We crossed the script boundary into a new script.
            const UnicodeString &inflowBoundary = *scriptUpperBoundary;
            UBool skippedScript = FALSE;
            for (;;) {
                scriptUpperBoundary = getString(*firstCharsInScripts_, ++scriptIndex);
                if (collatorPrimaryOnly_->compare(current, *scriptUpperBoundary, errorCode) < 0) {
                    break;
                }
                skippedScript = TRUE;
            }
            if (skippedScript && bucketList->size() > 1) {
                // We are skipping one or more scripts,
                // and we are not just getting out of the underflow label.
                bucket = new Bucket(getInflowLabel(), inflowBoundary, U_ALPHAINDEX_INFLOW);
                if (bucket == NULL) {
                    errorCode = U_MEMORY_ALLOCATION_ERROR;
                    return NULL;
                }
                bucketList->addElement(bucket, errorCode);
            }
        }
        // Add a bucket with the current label.
        bucket = new Bucket(fixLabel(current, temp), current, U_ALPHAINDEX_NORMAL);
        if (bucket == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        bucketList->addElement(bucket, errorCode);
        // Remember ASCII and Pinyin buckets for Pinyin redirects.
        UChar c;
        if (current.length() == 1 && 0x41 <= (c = current.charAt(0)) && c <= 0x5A) {  // A-Z
            asciiBuckets[c - 0x41] = bucket;
        } else if (current.length() == BASE_LENGTH + 1 && current.startsWith(BASE, BASE_LENGTH) &&
                0x41 <= (c = current.charAt(BASE_LENGTH)) && c <= 0x5A) {
            pinyinBuckets[c - 0x41] = bucket;
            hasPinyin = TRUE;
        }
        // Check for multiple primary weights.
        if (!current.startsWith(BASE, BASE_LENGTH) &&
                hasMultiplePrimaryWeights(*cei, variableTop, current, errorCode) &&
                current.charAt(current.length() - 1) != 0xFFFF /* !current.endsWith("\uffff") */) {
            // "AE-ligature" or "Sch" etc.
            for (int32_t i = bucketList->size() - 2;; --i) {
                Bucket *singleBucket = getBucket(*bucketList, i);
                if (singleBucket->labelType_ != U_ALPHAINDEX_NORMAL) {
                    // There is no single-character bucket since the last
                    // underflow or inflow label.
                    break;
                }
                if (singleBucket->displayBucket_ == NULL &&
                        !hasMultiplePrimaryWeights(
                            *cei, variableTop, singleBucket->lowerBoundary_, errorCode)) {
                    // Add an invisible bucket that redirects strings greater than the expansion
                    // to the previous single-character bucket.
                    // For example, after ... Q R S Sch we add Sch\uFFFF->S
                    // and after ... Q R S Sch Sch\uFFFF St we add St\uFFFF->S.
                    bucket = new Bucket(emptyString_,
                        UnicodeString(current).append((UChar)0xFFFF),
                        U_ALPHAINDEX_NORMAL);
                    if (bucket == NULL) {
                        errorCode = U_MEMORY_ALLOCATION_ERROR;
                        return NULL;
                    }
                    bucket->displayBucket_ = singleBucket;
                    bucketList->addElement(bucket, errorCode);
                    hasInvisibleBuckets = TRUE;
                    break;
                }
            }
        }
    }
    if (U_FAILURE(errorCode)) { return NULL; }
    if (bucketList->size() == 1) {
        // No real labels, show only the underflow label.
        BucketList *bl = new BucketList(bucketList.getAlias(), bucketList.getAlias());
        if (bl == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        bucketList.orphan();
        return bl;
    }
    // overflow bucket
    bucket = new Bucket(getOverflowLabel(), *scriptUpperBoundary, U_ALPHAINDEX_OVERFLOW);
    if (bucket == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    bucketList->addElement(bucket, errorCode); // final

    if (hasPinyin) {
        // Redirect Pinyin buckets.
        Bucket *asciiBucket = NULL;
        for (int32_t i = 0; i < 26; ++i) {
            if (asciiBuckets[i] != NULL) {
                asciiBucket = asciiBuckets[i];
            }
            if (pinyinBuckets[i] != NULL && asciiBucket != NULL) {
                pinyinBuckets[i]->displayBucket_ = asciiBucket;
                hasInvisibleBuckets = TRUE;
            }
        }
    }

    if (U_FAILURE(errorCode)) { return NULL; }
    if (!hasInvisibleBuckets) {
        BucketList *bl = new BucketList(bucketList.getAlias(), bucketList.getAlias());
        if (bl == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        bucketList.orphan();
        return bl;
    }
    // Merge inflow buckets that are visually adjacent.
    // Iterate backwards: Merge inflow into overflow rather than the other way around.
    int32_t i = bucketList->size() - 1;
    Bucket *nextBucket = getBucket(*bucketList, i);
    while (--i > 0) {
        bucket = getBucket(*bucketList, i);
        if (bucket->displayBucket_ != NULL) {
            continue;  // skip invisible buckets
        }
        if (bucket->labelType_ == U_ALPHAINDEX_INFLOW) {
            if (nextBucket->labelType_ != U_ALPHAINDEX_NORMAL) {
                bucket->displayBucket_ = nextBucket;
                continue;
            }
        }
        nextBucket = bucket;
    }

    LocalPointer<UVector> publicBucketList(new UVector(errorCode));
    if (bucketList.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    // Do not call publicBucketList->setDeleter():
    // This vector shares its objects with the bucketList.
    for (int32_t i = 0; i < bucketList->size(); ++i) {
        bucket = getBucket(*bucketList, i);
        if (bucket->displayBucket_ == NULL) {
            publicBucketList->addElement(bucket, errorCode);
        }
    }
    if (U_FAILURE(errorCode)) { return NULL; }
    BucketList *bl = new BucketList(bucketList.getAlias(), publicBucketList.getAlias());
    if (bl == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    bucketList.orphan();
    publicBucketList.orphan();
    return bl;
}

/**
 * Creates an index, and buckets and sorts the list of records into the index.
 */
void AlphabeticIndex::initBuckets(UErrorCode &errorCode) {
    if (U_FAILURE(errorCode) || buckets_ != NULL) {
        return;
    }
    buckets_ = createBucketList(errorCode);
    if (U_FAILURE(errorCode) || inputList_ == NULL || inputList_->isEmpty()) {
        return;
    }

    // Sort the records by name.
    // Stable sort preserves input order of collation duplicates.
    inputList_->sortWithUComparator(recordCompareFn, collator_, errorCode);

    // Now, we traverse all of the input, which is now sorted.
    // If the item doesn't go in the current bucket, we find the next bucket that contains it.
    // This makes the process order n*log(n), since we just sort the list and then do a linear process.
    // However, if the user adds an item at a time and then gets the buckets, this isn't efficient, so
    // we need to improve it for that case.

    Bucket *currentBucket = getBucket(*buckets_->bucketList_, 0);
    int32_t bucketIndex = 1;
    Bucket *nextBucket;
    const UnicodeString *upperBoundary;
    if (bucketIndex < buckets_->bucketList_->size()) {
        nextBucket = getBucket(*buckets_->bucketList_, bucketIndex++);
        upperBoundary = &nextBucket->lowerBoundary_;
    } else {
        nextBucket = NULL;
        upperBoundary = NULL;
    }
    for (int32_t i = 0; i < inputList_->size(); ++i) {
        Record *r = getRecord(*inputList_, i);
        // if the current bucket isn't the right one, find the one that is
        // We have a special flag for the last bucket so that we don't look any further
        while (upperBoundary != NULL &&
                collatorPrimaryOnly_->compare(r->name_, *upperBoundary, errorCode) >= 0) {
            currentBucket = nextBucket;
            // now reset the boundary that we compare against
            if (bucketIndex < buckets_->bucketList_->size()) {
                nextBucket = getBucket(*buckets_->bucketList_, bucketIndex++);
                upperBoundary = &nextBucket->lowerBoundary_;
            } else {
                upperBoundary = NULL;
            }
        }
        // now put the record into the bucket.
        Bucket *bucket = currentBucket;
        if (bucket->displayBucket_ != NULL) {
            bucket = bucket->displayBucket_;
        }
        if (bucket->records_ == NULL) {
            bucket->records_ = new UVector(errorCode);
            if (bucket->records_ == NULL) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        }
        bucket->records_->addElement(r, errorCode);
    }
}

void AlphabeticIndex::clearBuckets() {
    if (buckets_ != NULL) {
        delete buckets_;
        buckets_ = NULL;
        internalResetBucketIterator();
    }
}

void AlphabeticIndex::internalResetBucketIterator() {
    labelsIterIndex_ = -1;
    currentBucket_ = NULL;
}


void AlphabeticIndex::addIndexExemplars(const Locale &locale, UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    // Chinese index characters, which are specific to each of the several Chinese tailorings,
    // take precedence over the single locale data exemplar set per language.
    const char *language = locale.getLanguage();
    if (uprv_strcmp(language, "zh") == 0 || uprv_strcmp(language, "ja") == 0 ||
            uprv_strcmp(language, "ko") == 0) {
        // TODO: This should be done regardless of the language, but it's expensive.
        // We should add a Collator function (can be @internal)
        // to enumerate just the contractions that start with a given code point or string.
        if (addChineseIndexCharacters(status) || U_FAILURE(status)) {
            return;
        }
    }

    LocalULocaleDataPointer uld(ulocdata_open(locale.getName(), &status));
    if (U_FAILURE(status)) {
        return;
    }

    UnicodeSet exemplars;
    ulocdata_getExemplarSet(uld.getAlias(), exemplars.toUSet(), 0, ULOCDATA_ES_INDEX, &status);
    if (U_SUCCESS(status)) {
        initialLabels_->addAll(exemplars);
        return;
    }
    status = U_ZERO_ERROR;  // Clear out U_MISSING_RESOURCE_ERROR

    // The locale data did not include explicit Index characters.
    // Synthesize a set of them from the locale's standard exemplar characters.
    ulocdata_getExemplarSet(uld.getAlias(), exemplars.toUSet(), 0, ULOCDATA_ES_STANDARD, &status);
    if (U_FAILURE(status)) {
        return;
    }

    // question: should we add auxiliary exemplars?
    if (exemplars.containsSome(0x61, 0x7A) /* a-z */ || exemplars.size() == 0) {
        exemplars.add(0x61, 0x7A);
    }
    if (exemplars.containsSome(0xAC00, 0xD7A3)) {  // Hangul syllables
        // cut down to small list
        exemplars.remove(0xAC00, 0xD7A3).
            add(0xAC00).add(0xB098).add(0xB2E4).add(0xB77C).
            add(0xB9C8).add(0xBC14).add(0xC0AC).add(0xC544).
            add(0xC790).add(0xCC28).add(0xCE74).add(0xD0C0).
            add(0xD30C).add(0xD558);
    }
    if (exemplars.containsSome(0x1200, 0x137F)) {  // Ethiopic block
        // cut down to small list
        // make use of the fact that Ethiopic is allocated in 8's, where
        // the base is 0 mod 8.
        UnicodeSet ethiopic(
            UNICODE_STRING_SIMPLE("[[:Block=Ethiopic:]&[:Script=Ethiopic:]]"), status);
        UnicodeSetIterator it(ethiopic);
        while (it.next() && !it.isString()) {
            if ((it.getCodepoint() & 0x7) != 0) {
                exemplars.remove(it.getCodepoint());
            }
        }
    }

    // Upper-case any that aren't already so.
    //   (We only do this for synthesized index characters.)
    UnicodeSetIterator it(exemplars);
    UnicodeString upperC;
    while (it.next()) {
        const UnicodeString &exemplarC = it.getString();
        upperC = exemplarC;
        upperC.toUpper(locale);
        initialLabels_->add(upperC);
    }
}

UBool AlphabeticIndex::addChineseIndexCharacters(UErrorCode &errorCode) {
    UnicodeSet contractions;
    ucol_getContractionsAndExpansions(collatorPrimaryOnly_->getUCollator(),
                                      contractions.toUSet(), NULL, FALSE, &errorCode);
    if (U_FAILURE(errorCode)) { return FALSE; }
    UnicodeString firstHanBoundary;
    UBool hasPinyin = FALSE;
    UnicodeSetIterator iter(contractions);
    while (iter.next()) {
        const UnicodeString &s = iter.getString();
        if (s.startsWith(BASE, BASE_LENGTH)) {
            initialLabels_->add(s);
            if (firstHanBoundary.isEmpty() ||
                    collatorPrimaryOnly_->compare(s, firstHanBoundary, errorCode) < 0) {
                firstHanBoundary = s;
            }
            UChar c = s.charAt(s.length() - 1);
            if (0x41 <= c && c <= 0x5A) {  // A-Z
                hasPinyin = TRUE;
            }
        }
    }
    if (hasPinyin) {
        initialLabels_->add(0x41, 0x5A);  // A-Z
    }
    if (!firstHanBoundary.isEmpty()) {
        // The hardcoded list of script boundaries includes U+4E00
        // which is tailored to not be the first primary
        // in all Chinese tailorings except "unihan".
        // Replace U+4E00 with the first boundary string from the tailoring.
        // TODO: This becomes obsolete when the root collator gets
        // reliable script-first-primary mappings.
        int32_t hanIndex = binarySearch(
                *firstCharsInScripts_, UnicodeString((UChar)0x4E00), *collatorPrimaryOnly_);
        if (hanIndex >= 0) {
            UnicodeString *fh = new UnicodeString(firstHanBoundary);
            if (fh == NULL) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return FALSE;
            }
            firstCharsInScripts_->setElementAt(fh, hanIndex);
        }
        return TRUE;
    } else {
        return FALSE;
    }
}


/*
 * Return the string with interspersed CGJs. Input must have more than 2 codepoints.
 */
static const UChar CGJ = 0x034F;
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
    clearBuckets();
    return *this;
}


AlphabeticIndex &AlphabeticIndex::setOverflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    overflowLabel_ = label;
    clearBuckets();
    return *this;
}


AlphabeticIndex &AlphabeticIndex::setUnderflowLabel(const UnicodeString &label, UErrorCode &/*status*/) {
    underflowLabel_ = label;
    clearBuckets();
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
    clearBuckets();
    return *this;
}


//
//  init() - Common code for constructors.
//

void AlphabeticIndex::init(const Locale *locale, UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    if (locale == NULL && collator_ == NULL) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    initialLabels_         = new UnicodeSet();
    if (initialLabels_ == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    inflowLabel_.setTo((UChar)0x2026);    // Ellipsis
    overflowLabel_ = inflowLabel_;
    underflowLabel_ = inflowLabel_;

    if (collator_ == NULL) {
        collator_ = static_cast<RuleBasedCollator *>(Collator::createInstance(*locale, status));
        if (U_FAILURE(status)) { return; }
        if (collator_ == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
    collatorPrimaryOnly_ = static_cast<RuleBasedCollator *>(collator_->clone());
    if (collatorPrimaryOnly_ == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    collatorPrimaryOnly_->setAttribute(UCOL_STRENGTH, UCOL_PRIMARY, status);
    firstCharsInScripts_ = firstStringsInScript(status);
    if (U_FAILURE(status)) { return; }
    firstCharsInScripts_->sortWithUComparator(collatorComparator, collatorPrimaryOnly_, status);
    UnicodeString _4E00((UChar)0x4E00);
    UnicodeString _1100((UChar)0x1100);
    UnicodeString _1112((UChar)0x1112);
    if (collatorPrimaryOnly_->compare(_4E00, _1112, status) <= 0 &&
            collatorPrimaryOnly_->compare(_1100, _4E00, status) <= 0) {
        // The standard Korean tailoring sorts Hanja (Han characters)
        // as secondary differences from Hangul syllables.
        // This makes U+4E00 not useful as a Han-script boundary.
        // TODO: This becomes obsolete when the root collator gets
        // reliable script-first-primary mappings.
        int32_t hanIndex = binarySearch(
                *firstCharsInScripts_, _4E00, *collatorPrimaryOnly_);
        if (hanIndex >= 0) {
            firstCharsInScripts_->removeElementAt(hanIndex);
        }
    }
    // Guard against a degenerate collator where
    // some script boundary strings are primary ignorable.
    for (;;) {
        if (U_FAILURE(status)) { return; }
        if (firstCharsInScripts_->isEmpty()) {
            // AlphabeticIndex requires some non-ignorable script boundary strings.
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        if (collatorPrimaryOnly_->compare(
                *static_cast<UnicodeString *>(firstCharsInScripts_->elementAt(0)),
                emptyString_, status) == UCOL_EQUAL) {
            firstCharsInScripts_->removeElementAt(0);
        } else {
            break;
        }
    }

    if (locale != NULL) {
        addIndexExemplars(*locale, status);
    }
}


//
//  Comparison function for UVector<UnicodeString *> sorting with a collator.
//
static int32_t U_CALLCONV
collatorComparator(const void *context, const void *left, const void *right) {
    const UElement *leftElement = static_cast<const UElement *>(left);
    const UElement *rightElement = static_cast<const UElement *>(right);
    const UnicodeString *leftString  = static_cast<const UnicodeString *>(leftElement->pointer);
    const UnicodeString *rightString = static_cast<const UnicodeString *>(rightElement->pointer);

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
    const Collator *col = static_cast<const Collator *>(context);
    UErrorCode errorCode = U_ZERO_ERROR;
    return col->compare(*leftString, *rightString, errorCode);
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
    UErrorCode errorCode = U_ZERO_ERROR;
    return col->compare(leftRec->name_, rightRec->name_, errorCode);
}


/**
 * This list contains one character per script that has the
 * lowest primary weight for that script in the root collator.
 * This list will be copied and sorted to account for script reordering.
 *
 * <p>TODO: This is fragile. If the first character of a script is tailored
 * so that it does not map to the script's lowest primary weight any more,
 * then the buckets will be off.
 * There are hacks in the code to handle the known CJK tailorings of U+4E00.
 *
 * <p>We use "A" not "a" because the en_US_POSIX tailoring sorts A primary-before a.
 *
 * Keep this in sync with HACK_FIRST_CHARS_IN_SCRIPTS in
 * ICU4J main/classes/collate/src/com/ibm/icu/text/AlphabeticIndex.java
 */
static const UChar HACK_FIRST_CHARS_IN_SCRIPTS[] =  {
    0x41, 0, 0x03B1, 0,
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
    0x4E00, 0,
    // TODO: The overflow bucket's lowerBoundary string should be the
    // first item after the last reordering group in the collator's script order.
    // This should normally be the first Unicode code point
    // that is unassigned (U+0378 in Unicode 6.3) and untailored.
    // However, at least up to ICU 51 the Hani reordering group includes
    // unassigned code points,
    // and there is no stable string for the start of the trailing-weights range.
    // The only known string that sorts "high" is U+FFFF.
    // When ICU separates Hani vs. unassigned reordering groups, we need to fix this,
    // and fix relevant test code.
    // Ideally, FractionalUCA.txt will have a "script first primary"
    // for unassigned code points.
    0xFFFF, 0
};

UVector *AlphabeticIndex::firstStringsInScript(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    UVector *dest = new UVector(status);
    if (dest == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    dest->setDeleter(uprv_deleteUObject);
    const UChar *src  = HACK_FIRST_CHARS_IN_SCRIPTS;
    const UChar *limit = src + LENGTHOF(HACK_FIRST_CHARS_IN_SCRIPTS);
    do {
        if (U_FAILURE(status)) {
            return dest;
        }
        UnicodeString *str = new UnicodeString(src, -1);
        if (str == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return dest;
        }
        dest->addElement(str, status);
        src += str->length() + 1;
    } while (src < limit);
    return dest;
}


namespace {

/**
 * Returns true if one index character string is "better" than the other.
 * Shorter NFKD is better, and otherwise NFKD-binary-less-than is
 * better, and otherwise binary-less-than is better.
 */
UBool isOneLabelBetterThanOther(const Normalizer2 &nfkdNormalizer,
                                const UnicodeString &one, const UnicodeString &other) {
    // This is called with primary-equal strings, but never with one.equals(other).
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString n1 = nfkdNormalizer.normalize(one, status);
    UnicodeString n2 = nfkdNormalizer.normalize(other, status);
    if (U_FAILURE(status)) { return FALSE; }
    int32_t result = n1.countChar32() - n2.countChar32();
    if (result != 0) {
        return result < 0;
    }
    result = n1.compareCodePointOrder(n2);
    if (result != 0) {
        return result < 0;
    }
    return one.compareCodePointOrder(other) < 0;
}

}  // namespace

//
//  Constructor & Destructor for AlphabeticIndex::Record
//
//     Records are internal only, instances are not directly surfaced in the public API.
//     This class is mostly struct-like, with all public fields.

AlphabeticIndex::Record::Record(const UnicodeString &name, const void *data)
        : name_(name), data_(data) {}

AlphabeticIndex::Record::~Record() {
}


AlphabeticIndex & AlphabeticIndex::addRecord(const UnicodeString &name, const void *data, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (inputList_ == NULL) {
        inputList_ = new UVector(status);
        if (inputList_ == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return *this;
        }
        inputList_->setDeleter(alphaIndex_deleteRecord);
    }
    Record *r = new Record(name, data);
    if (r == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return *this;
    }
    inputList_->addElement(r, status);
    clearBuckets();
    //std::string ss;
    //std::string ss2;
    //std::cout << "added record: name = \"" << r->name_.toUTF8String(ss) << "\"" << 
    //             "   sortingName = \"" << r->sortingName_.toUTF8String(ss2) << "\"" << std::endl;
    return *this;
}


AlphabeticIndex &AlphabeticIndex::clearRecords(UErrorCode &status) {
    if (U_SUCCESS(status) && inputList_ != NULL && !inputList_->isEmpty()) {
        inputList_->removeAllElements();
        clearBuckets();
    }
    return *this;
}

int32_t AlphabeticIndex::getBucketIndex(const UnicodeString &name, UErrorCode &status) {
    initBuckets(status);
    if (U_FAILURE(status)) {
        return 0;
    }
    return buckets_->getBucketIndex(name, *collatorPrimaryOnly_, status);
}


int32_t AlphabeticIndex::getBucketIndex() const {
    return labelsIterIndex_;
}


UBool AlphabeticIndex::nextBucket(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (buckets_ == NULL && currentBucket_ != NULL) {
        status = U_ENUM_OUT_OF_SYNC_ERROR;
        return FALSE;
    }
    initBuckets(status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    ++labelsIterIndex_;
    if (labelsIterIndex_ >= buckets_->getBucketCount()) {
        labelsIterIndex_ = buckets_->getBucketCount();
        return FALSE;
    }
    currentBucket_ = getBucket(*buckets_->immutableVisibleList_, labelsIterIndex_);
    resetRecordIterator();
    return TRUE;
}

const UnicodeString &AlphabeticIndex::getBucketLabel() const {
    if (currentBucket_ != NULL) {
        return currentBucket_->label_;
    } else {
        return emptyString_;
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
    if (currentBucket_ != NULL && currentBucket_->records_ != NULL) {
        return currentBucket_->records_->size();
    } else {
        return 0;
    }
}


AlphabeticIndex &AlphabeticIndex::resetBucketIterator(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    internalResetBucketIterator();
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
    if (buckets_ == NULL) {
        status = U_ENUM_OUT_OF_SYNC_ERROR;
        return FALSE;
    }
    if (currentBucket_->records_ == NULL) {
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
    const UnicodeString *retStr = &emptyString_;
    if (currentBucket_ != NULL && currentBucket_->records_ != NULL &&
        itemsIterIndex_ >= 0 &&
        itemsIterIndex_ < currentBucket_->records_->size()) {
            Record *item = static_cast<Record *>(currentBucket_->records_->elementAt(itemsIterIndex_));
            retStr = &item->name_;
    }
    return *retStr;
}

const void *AlphabeticIndex::getRecordData() const {
    const void *retPtr = NULL;
    if (currentBucket_ != NULL && currentBucket_->records_ != NULL &&
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
                                UAlphabeticIndexLabelType type)
        : label_(label), lowerBoundary_(lowerBoundary), labelType_(type),
          displayBucket_(NULL), displayIndex_(-1),
          records_(NULL) {
}


AlphabeticIndex::Bucket::~Bucket() {
    delete records_;
}

U_NAMESPACE_END

#endif
