// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 *   Copyright (C) 1996-2016, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 ******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/unistr.h"
#include "unicode/usearch.h"

#include "cmemory.h"
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/coleitr.h"
#include "unicode/ucoleitr.h"

#include "unicode/regex.h"        // TODO: make conditional on regexp being built.

#include "unicode/testlog.h"
#include "unicode/uniset.h"
#include "unicode/uset.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "hash.h"
#include "normalizer2impl.h"
#include "uhash.h"
#include "usrchimp.h"
#include "uassert.h"

#include "colldata.h"

#define NEW_ARRAY(type, count) (type *) uprv_malloc((size_t)(count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *) (array))
#define ARRAY_COPY(dst, src, count) uprv_memcpy((void *) (dst), (void *) (src), (size_t)(count) * sizeof (src)[0])

CEList::CEList(UCollator *coll, const UnicodeString &string, UErrorCode &status)
    : ces(nullptr), listMax(CELIST_BUFFER_SIZE), listSize(0)
{
    UCollationElements *elems = ucol_openElements(coll, string.getBuffer(), string.length(), &status);
    UCollationStrength strength = ucol_getStrength(coll);
    UBool toShift = ucol_getAttribute(coll, UCOL_ALTERNATE_HANDLING, &status) ==  UCOL_SHIFTED;
    uint32_t variableTop = ucol_getVariableTop(coll, &status);
    uint32_t strengthMask = 0;
    int32_t order;

    if (U_FAILURE(status)) {
        return;
    }

    // **** only set flag if string has Han(gul) ****
    // ucol_forceHanImplicit(elems, &status); -- removed for ticket #10476

    switch (strength)
    {
    default:
        strengthMask |= UCOL_TERTIARYORDERMASK;
        U_FALLTHROUGH;
    case UCOL_SECONDARY:
        strengthMask |= UCOL_SECONDARYORDERMASK;
        U_FALLTHROUGH;
    case UCOL_PRIMARY:
        strengthMask |= UCOL_PRIMARYORDERMASK;
    }

    ces = ceBuffer;

    while ((order = ucol_next(elems, &status)) != UCOL_NULLORDER) {
        UBool cont = isContinuation(order);

        order &= strengthMask;

        if (toShift && variableTop > (uint32_t)order && (order & UCOL_PRIMARYORDERMASK) != 0) {
            if (strength >= UCOL_QUATERNARY) {
                order &= UCOL_PRIMARYORDERMASK;
            } else {
                order = UCOL_IGNORABLE;
            }
        }

        if (order == UCOL_IGNORABLE) {
            continue;
        }

        if (cont) {
            order |= UCOL_CONTINUATION_MARKER;
        }

        add(order, status);
    }

    ucol_closeElements(elems);
}

CEList::~CEList()
{
    if (ces != ceBuffer) {
        DELETE_ARRAY(ces);
    }
}

void CEList::add(uint32_t ce, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return;
    }

    if (listSize >= listMax) {
        int32_t newMax = listMax + CELIST_BUFFER_SIZE;
        uint32_t *newCEs = NEW_ARRAY(uint32_t, newMax);

        if (newCEs == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }

        uprv_memcpy(newCEs, ces, listSize * sizeof(uint32_t));

        if (ces != ceBuffer) {
            DELETE_ARRAY(ces);
        }

        ces = newCEs;
        listMax = newMax;
    }

    ces[listSize++] = ce;
}

uint32_t CEList::get(int32_t index) const
{
    if (index >= 0 && index < listSize) {
        return ces[index];
    }

    return (uint32_t)UCOL_NULLORDER;
}

uint32_t &CEList::operator[](int32_t index) const
{
    return ces[index];
}

UBool CEList::matchesAt(int32_t offset, const CEList *other) const
{
    if (other == nullptr || listSize - offset < other->size()) {
        return false;
    }

    for (int32_t i = offset, j = 0; j < other->size(); i += 1, j += 1) {
        if (ces[i] != (*other)[j]) {
            return false;
        }
    }

    return true;
}

int32_t CEList::size() const
{
    return listSize;
}

StringList::StringList(UErrorCode &status)
    : strings(nullptr), listMax(STRING_LIST_BUFFER_SIZE), listSize(0)
{
    if (U_FAILURE(status)) {
        return;
    }

    strings = new UnicodeString [listMax];

    if (strings == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

StringList::~StringList()
{
    delete[] strings;
}

void StringList::add(const UnicodeString *string, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return;
    }
    if (listSize >= listMax) {
        int32_t newMax = listMax + STRING_LIST_BUFFER_SIZE;
        UnicodeString *newStrings = new UnicodeString[newMax];
        if (newStrings == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        for (int32_t i=0; i<listSize; ++i) {
            newStrings[i] = strings[i];
        }
        delete[] strings;
        strings = newStrings;
        listMax = newMax;
    }

    // The ctor initialized all the strings in
    // the array to empty strings, so this
    // is the same as copying the source string.
    strings[listSize++].append(*string);
}

void StringList::add(const char16_t *chars, int32_t count, UErrorCode &status)
{
    const UnicodeString string(chars, count);

    add(&string, status);
}

const UnicodeString *StringList::get(int32_t index) const
{
    if (index >= 0 && index < listSize) {
        return &strings[index];
    }

    return nullptr;
}

int32_t StringList::size() const
{
    return listSize;
}


U_CDECL_BEGIN
static void U_CALLCONV
deleteStringList(void *obj)
{
    StringList *strings = static_cast<StringList *>(obj);

    delete strings;
}
U_CDECL_END

class CEToStringsMap
{
public:
    CEToStringsMap(UErrorCode &status);
    ~CEToStringsMap();

    void put(uint32_t ce, UnicodeString *string, UErrorCode &status);
    StringList *getStringList(uint32_t ce) const;

private:
    void putStringList(uint32_t ce, StringList *stringList, UErrorCode &status);
    UHashtable *map;
};

CEToStringsMap::CEToStringsMap(UErrorCode &status)
    : map(nullptr)
{
    if (U_FAILURE(status)) {
        return;
    }

    map = uhash_open(uhash_hashLong, uhash_compareLong,
                     uhash_compareCaselessUnicodeString,
                     &status);

    if (U_FAILURE(status)) {
        return;
    }

    uhash_setValueDeleter(map, deleteStringList);
}

CEToStringsMap::~CEToStringsMap()
{
    uhash_close(map);
}

void CEToStringsMap::put(uint32_t ce, UnicodeString *string, UErrorCode &status)
{
    StringList *strings = getStringList(ce);

    if (strings == nullptr) {
        strings = new StringList(status);

        if (strings == nullptr || U_FAILURE(status)) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }

        putStringList(ce, strings, status);
    }

    strings->add(string, status);
}

StringList *CEToStringsMap::getStringList(uint32_t ce) const
{
    return static_cast<StringList *>(uhash_iget(map, ce));
}

void CEToStringsMap::putStringList(uint32_t ce, StringList *stringList, UErrorCode &status)
{
    uhash_iput(map, ce, (void *) stringList, &status);
}

#define CLONE_COLLATOR

CollData::CollData(UCollator *collator, UErrorCode &status)
    : coll(nullptr), ceToCharsStartingWith(nullptr)
{
    // [:c:] == [[:cn:][:cc:][:co:][:cf:][:cs:]]
    // i.e. other, control, private use, format, surrogate
    U_STRING_DECL(test_pattern, "[[:assigned:]-[:c:]]", 20);
    U_STRING_INIT(test_pattern, "[[:assigned:]-[:c:]]", 20);
    USet *charsToTest = uset_openPattern(test_pattern, 20, &status);

    // Han ext. A, Han, Jamo, Hangul, Han Ext. B
    // i.e. all the characters we handle implicitly
    U_STRING_DECL(remove_pattern, "[[\\u3400-\\u9FFF][\\u1100-\\u11F9][\\uAC00-\\uD7AF][\\U00020000-\\U0002A6DF]]", 70);
    U_STRING_INIT(remove_pattern, "[[\\u3400-\\u9FFF][\\u1100-\\u11F9][\\uAC00-\\uD7AF][\\U00020000-\\U0002A6DF]]", 70);
    USet *charsToRemove = uset_openPattern(remove_pattern, 70, &status);

    if (U_FAILURE(status)) {
        return;
    }

    USet *expansions   = uset_openEmpty();
    USet *contractions = uset_openEmpty();
    int32_t itemCount;

    ceToCharsStartingWith = new CEToStringsMap(status);

    if (U_FAILURE(status)) {
        goto bail;
    }

#ifdef CLONE_COLLATOR
    coll = ucol_safeClone(collator, nullptr, nullptr, &status);

    if (U_FAILURE(status)) {
        goto bail;
    }
#else
    coll = collator;
#endif

    ucol_getContractionsAndExpansions(coll, contractions, expansions, false, &status);

    uset_addAll(charsToTest, contractions);
    uset_addAll(charsToTest, expansions);
    uset_removeAll(charsToTest, charsToRemove);

    itemCount = uset_getItemCount(charsToTest);
    for(int32_t item = 0; item < itemCount; item += 1) {
        UChar32 start = 0, end = 0;
        char16_t buffer[16];
        int32_t len = uset_getItem(charsToTest, item, &start, &end,
                                   buffer, 16, &status);

        if (len == 0) {
            for (UChar32 ch = start; ch <= end; ch += 1) {
                UnicodeString *st = new UnicodeString(ch);

                if (st == nullptr) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }

                CEList *ceList = new CEList(coll, *st, status);

                ceToCharsStartingWith->put(ceList->get(0), st, status);

                delete ceList;
                delete st;
            }
        } else if (len > 0) {
            UnicodeString *st = new UnicodeString(buffer, len);

            if (st == nullptr) {
                status = U_MEMORY_ALLOCATION_ERROR;
                break;
            }

            CEList *ceList = new CEList(coll, *st, status);

            ceToCharsStartingWith->put(ceList->get(0), st, status);

            delete ceList;
            delete st;
        } else {
            // shouldn't happen...
        }

        if (U_FAILURE(status)) {
             break;
        }
    }

bail:
    uset_close(contractions);
    uset_close(expansions);
    uset_close(charsToRemove);
    uset_close(charsToTest);

    if (U_FAILURE(status)) {
        return;
    }

    UnicodeSet hanRanges(UNICODE_STRING_SIMPLE("[:Unified_Ideograph:]"), status);
    if (U_FAILURE(status)) {
        return;
    }
    UnicodeSetIterator hanIter(hanRanges);
    UnicodeString hanString;
    while(hanIter.nextRange()) {
        hanString.append(hanIter.getCodepoint());
        hanString.append(hanIter.getCodepointEnd());
    }
    // TODO: Why U+11FF? The old code had an outdated UCOL_LAST_T_JAMO=0x11F9,
    // but as of Unicode 6.3 the 11xx block is filled,
    // and there are also more Jamo T at U+D7CB..U+D7FB.
    // Maybe use [:HST=T:] and look for the end of the last range?
    // Maybe use script boundary mappings instead of this code??
    char16_t  jamoRanges[] = {Hangul::JAMO_L_BASE, Hangul::JAMO_V_BASE, Hangul::JAMO_T_BASE + 1, 0x11FF};
     UnicodeString jamoString(false, jamoRanges, UPRV_LENGTHOF(jamoRanges));
     CEList hanList(coll, hanString, status);
     CEList jamoList(coll, jamoString, status);
     int32_t j = 0;

     if (U_FAILURE(status)) {
         return;
     }

     for (int32_t c = 0; c < jamoList.size(); c += 1) {
         uint32_t jce = jamoList[c];

         if (! isContinuation(jce)) {
             jamoLimits[j++] = jce;
         }
     }

     jamoLimits[3] += (1 << UCOL_PRIMARYORDERSHIFT);

     minHan = 0xFFFFFFFF;
     maxHan = 0;

     for(int32_t h = 0; h < hanList.size(); h += 2) {
         uint32_t han = (uint32_t) hanList[h];

         if (han < minHan) {
             minHan = han;
         }

         if (han > maxHan) {
             maxHan = han;
         }
     }

     maxHan += (1 << UCOL_PRIMARYORDERSHIFT);
}

CollData::~CollData()
{
#ifdef CLONE_COLLATOR
   ucol_close(coll);
#endif

   delete ceToCharsStartingWith;
}

UCollator *CollData::getCollator() const
{
    return coll;
}

const StringList *CollData::getStringList(int32_t ce) const
{
    return ceToCharsStartingWith->getStringList(ce);
}

const CEList *CollData::getCEList(const UnicodeString *string) const
{
    UErrorCode status = U_ZERO_ERROR;
    const CEList *list = new CEList(coll, *string, status);

    if (U_FAILURE(status)) {
        delete list;
        list = nullptr;
    }

    return list;
}

void CollData::freeCEList(const CEList *list)
{
    delete list;
}

int32_t CollData::minLengthInChars(const CEList *ceList, int32_t offset, int32_t *history) const
{
    // find out shortest string for the longest sequence of ces.
    // this can probably be folded with the minLengthCache...

    if (history[offset] >= 0) {
        return history[offset];
    }

    uint32_t ce = ceList->get(offset);
    int32_t maxOffset = ceList->size();
    int32_t shortestLength = INT32_MAX;
    const StringList *strings = ceToCharsStartingWith->getStringList(ce);

    if (strings != nullptr) {
        int32_t stringCount = strings->size();

        for (int32_t s = 0; s < stringCount; s += 1) {
            const UnicodeString *string = strings->get(s);
            UErrorCode status = U_ZERO_ERROR;
            const CEList *ceList2 = new CEList(coll, *string, status);

            if (U_FAILURE(status)) {
                delete ceList2;
                ceList2 = nullptr;
            }

            if (ceList->matchesAt(offset, ceList2)) {
                U_ASSERT(ceList2 != nullptr);
                int32_t clength = ceList2->size();
                int32_t slength = string->length();
                int32_t roffset = offset + clength;
                int32_t rlength = 0;

                if (roffset < maxOffset) {
                    rlength = minLengthInChars(ceList, roffset, history);

                    if (rlength <= 0) {
                    // delete before continue to avoid memory leak.
                        delete ceList2;

                        // ignore any dead ends
                        continue;
                    }
                }

                if (shortestLength > slength + rlength) {
                    shortestLength = slength + rlength;
                }
            }

            delete ceList2;
        }
    }

    if (shortestLength == INT32_MAX) {
        // No matching strings at this offset. See if
        // the CE is in a range we can handle manually.
        if (ce >= minHan && ce < maxHan) {
            // all han have implicit orders which
            // generate two CEs.
            int32_t roffset = offset + 2;
            int32_t rlength = 0;

          //history[roffset++] = -1;
          //history[roffset++] = 1;

            if (roffset < maxOffset) {
                rlength = minLengthInChars(ceList, roffset, history);
            }

            if (rlength < 0) {
                return -1;
            }

            shortestLength = 1 + rlength;
            goto have_shortest;
        } else if (ce >= jamoLimits[0] && ce < jamoLimits[3]) {
            int32_t roffset = offset;
            int32_t rlength = 0;

            // **** this loop may not handle archaic Hangul correctly ****
            for (int32_t j = 0; roffset < maxOffset && j < 4; j += 1, roffset += 1) {
                uint32_t jce = ceList->get(roffset);

                // Some Jamo have 24-bit primary order; skip the
                // 2nd CE. This should always be OK because if
                // we're still in the loop all we've seen are
                // a series of Jamo in LVT order.
                if (isContinuation(jce)) {
                    continue;
                }

                if (j >= 3 || jce < jamoLimits[j] || jce >= jamoLimits[j + 1]) {
                    break;
                }
            }

            if (roffset == offset) {
                // we started with a non-L Jamo...
                // just say it comes from a single character
                roffset += 1;

                // See if the single Jamo has a 24-bit order.
                if (roffset < maxOffset && isContinuation(ceList->get(roffset))) {
                    roffset += 1;
                }
            }

            if (roffset < maxOffset) {
                rlength = minLengthInChars(ceList, roffset, history);
            }

            if (rlength < 0) {
                return -1;
            }

            shortestLength = 1 + rlength;
            goto have_shortest;
        }

        // Can't handle it manually either. Just move on.
        return -1;
    }

have_shortest:
    history[offset] = shortestLength;

    return shortestLength;
}

int32_t CollData::minLengthInChars(const CEList *ceList, int32_t offset) const
{
    int32_t clength = ceList->size();
    int32_t *history = NEW_ARRAY(int32_t, clength);

    for (int32_t i = 0; i < clength; i += 1) {
        history[i] = -1;
    }

    int32_t minLength = minLengthInChars(ceList, offset, history);

    DELETE_ARRAY(history);

    return minLength;
}

#endif // #if !UCONFIG_NO_COLLATION
