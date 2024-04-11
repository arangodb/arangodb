/*
***********************************************************************
* © 2016 and later: Unicode, Inc. and others.
* License & terms of use: http://www.unicode.org/copyright.html#License
***********************************************************************
***********************************************************************
* Copyright (c) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
***********************************************************************
*/

#include <string.h>
#include "unicode/localpointer.h"
#include "unicode/uperf.h"
#include "unicode/ucol.h"
#include "unicode/coll.h"
#include "unicode/uiter.h"
#include "unicode/ustring.h"
#include "unicode/sortkey.h"
#include "uarrsort.h"
#include "uoptions.h"
#include "ustr_imp.h"

#define COMPACT_ARRAY(CompactArrays, UNIT) \
struct CompactArrays{\
    CompactArrays(const CompactArrays & );\
    CompactArrays & operator=(const CompactArrays & );\
    int32_t   count;/*total number of the strings*/ \
    int32_t * index;/*relative offset in data*/ \
    UNIT    * data; /*the real space to hold strings*/ \
    \
    ~CompactArrays(){free(index);free(data);} \
    CompactArrays() : count(0), index(NULL), data(NULL) { \
        index = (int32_t *) realloc(index, sizeof(int32_t)); \
        index[0] = 0; \
    } \
    void append_one(int32_t theLen){ /*include terminal NULL*/ \
        count++; \
        index = (int32_t *) realloc(index, sizeof(int32_t) * (count + 1)); \
        index[count] = index[count - 1] + theLen; \
        data = (UNIT *) realloc(data, sizeof(UNIT) * index[count]); \
    } \
    UNIT * last(){return data + index[count - 1];} \
    const UNIT * dataOf(int32_t i) const {return data + index[i];} \
    int32_t lengthOf(int i) const {return index[i+1] - index[i] - 1; } /*exclude terminating NULL*/  \
};

COMPACT_ARRAY(CA_uchar, UChar)
COMPACT_ARRAY(CA_char, char)

#define MAX_TEST_STRINGS_FOR_PERMUTING 1000

// C API test cases

//
// Test case taking a single test data array, calling ucol_strcoll by permuting the test data
//
class Strcoll : public UPerfFunction
{
public:
    Strcoll(const UCollator* coll, const CA_uchar* source, UBool useLen);
    ~Strcoll();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    const CA_uchar *source;
    UBool useLen;
    int32_t maxTestStrings;
};

Strcoll::Strcoll(const UCollator* coll, const CA_uchar* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
    maxTestStrings = source->count > MAX_TEST_STRINGS_FOR_PERMUTING ? MAX_TEST_STRINGS_FOR_PERMUTING : source->count;
}

Strcoll::~Strcoll()
{
}

void Strcoll::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    // call strcoll for permutation
    int32_t divisor = source->count / maxTestStrings;
    int32_t srcLen, tgtLen;
    int32_t cmp = 0;
    for (int32_t i = 0, numTestStringsI = 0; i < source->count && numTestStringsI < maxTestStrings; i++) {
        if (i % divisor) continue;
        numTestStringsI++;
        srcLen = useLen ? source->lengthOf(i) : -1;
        for (int32_t j = 0, numTestStringsJ = 0; j < source->count && numTestStringsJ < maxTestStrings; j++) {
            if (j % divisor) continue;
            numTestStringsJ++;
            tgtLen = useLen ? source->lengthOf(j) : -1;
            cmp += ucol_strcoll(coll, source->dataOf(i), srcLen, source->dataOf(j), tgtLen);
        }
    }
    // At the end, cmp must be 0
    if (cmp != 0) {
        *status = U_INTERNAL_PROGRAM_ERROR;
    }
}

long Strcoll::getOperationsPerIteration()
{
    return maxTestStrings * maxTestStrings;
}

//
// Test case taking two test data arrays, calling ucol_strcoll for strings at a same index
//
class Strcoll_2 : public UPerfFunction
{
public:
    Strcoll_2(const UCollator* coll, const CA_uchar* source, const CA_uchar* target, UBool useLen);
    ~Strcoll_2();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    const CA_uchar *source;
    const CA_uchar *target;
    UBool useLen;
};

Strcoll_2::Strcoll_2(const UCollator* coll, const CA_uchar* source, const CA_uchar* target, UBool useLen)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen)
{
}

Strcoll_2::~Strcoll_2()
{
}

void Strcoll_2::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    // call strcoll for two strings at the same index
    if (source->count < target->count) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    } else {
        for (int32_t i = 0; i < source->count; i++) {
            int32_t srcLen = useLen ? source->lengthOf(i) : -1;
            int32_t tgtLen = useLen ? target->lengthOf(i) : -1;
            ucol_strcoll(coll, source->dataOf(i), srcLen, target->dataOf(i), tgtLen);
        }
    }
}

long Strcoll_2::getOperationsPerIteration()
{
    return source->count;
}


//
// Test case taking a single test data array, calling ucol_strcollUTF8 by permuting the test data
//
class StrcollUTF8 : public UPerfFunction
{
public:
    StrcollUTF8(const UCollator* coll, const CA_char* source, UBool useLen);
    ~StrcollUTF8();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    const CA_char *source;
    UBool useLen;
    int32_t maxTestStrings;
};

StrcollUTF8::StrcollUTF8(const UCollator* coll, const CA_char* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
    maxTestStrings = source->count > MAX_TEST_STRINGS_FOR_PERMUTING ? MAX_TEST_STRINGS_FOR_PERMUTING : source->count;
}

StrcollUTF8::~StrcollUTF8()
{
}

void StrcollUTF8::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    // call strcollUTF8 for permutation
    int32_t divisor = source->count / maxTestStrings;
    int32_t srcLen, tgtLen;
    int32_t cmp = 0;
    for (int32_t i = 0, numTestStringsI = 0; U_SUCCESS(*status) && i < source->count && numTestStringsI < maxTestStrings; i++) {
        if (i % divisor) continue;
        numTestStringsI++;
        srcLen = useLen ? source->lengthOf(i) : -1;
        for (int32_t j = 0, numTestStringsJ = 0; U_SUCCESS(*status) && j < source->count && numTestStringsJ < maxTestStrings; j++) {
            if (j % divisor) continue;
            numTestStringsJ++;
            tgtLen = useLen ? source->lengthOf(j) : -1;
            cmp += ucol_strcollUTF8(coll, source->dataOf(i), srcLen, source->dataOf(j), tgtLen, status);
        }
    }
    // At the end, cmp must be 0
    if (cmp != 0) {
        *status = U_INTERNAL_PROGRAM_ERROR;
    }
}

long StrcollUTF8::getOperationsPerIteration()
{
    return maxTestStrings * maxTestStrings;
}

//
// Test case taking two test data arrays, calling ucol_strcoll for strings at a same index
//
class StrcollUTF8_2 : public UPerfFunction
{
public:
    StrcollUTF8_2(const UCollator* coll, const CA_char* source, const CA_char* target, UBool useLen);
    ~StrcollUTF8_2();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    const CA_char *source;
    const CA_char *target;
    UBool useLen;
};

StrcollUTF8_2::StrcollUTF8_2(const UCollator* coll, const CA_char* source, const CA_char* target, UBool useLen)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen)
{
}

StrcollUTF8_2::~StrcollUTF8_2()
{
}

void StrcollUTF8_2::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    // call strcoll for two strings at the same index
    if (source->count < target->count) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    } else {
        for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
            int32_t srcLen = useLen ? source->lengthOf(i) : -1;
            int32_t tgtLen = useLen ? target->lengthOf(i) : -1;
            ucol_strcollUTF8(coll, source->dataOf(i), srcLen, target->dataOf(i), tgtLen, status);
        }
    }
}

long StrcollUTF8_2::getOperationsPerIteration()
{
    return source->count;
}

//
// Test case taking a single test data array, calling ucol_getSortKey for each
//
class GetSortKey : public UPerfFunction
{
public:
    GetSortKey(const UCollator* coll, const CA_uchar* source, UBool useLen);
    ~GetSortKey();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    const CA_uchar *source;
    UBool useLen;
};

GetSortKey::GetSortKey(const UCollator* coll, const CA_uchar* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
}

GetSortKey::~GetSortKey()
{
}

#define KEY_BUF_SIZE 512

void GetSortKey::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    uint8_t key[KEY_BUF_SIZE];
    int32_t len;

    if (useLen) {
        for (int32_t i = 0; i < source->count; i++) {
            len = ucol_getSortKey(coll, source->dataOf(i), source->lengthOf(i), key, KEY_BUF_SIZE);
        }
    } else {
        for (int32_t i = 0; i < source->count; i++) {
            len = ucol_getSortKey(coll, source->dataOf(i), -1, key, KEY_BUF_SIZE);
        }
    }
}

long GetSortKey::getOperationsPerIteration()
{
    return source->count;
}

//
// Test case taking a single test data array in UTF-16, calling ucol_nextSortKeyPart for each for the
// given buffer size
//
class NextSortKeyPart : public UPerfFunction
{
public:
    NextSortKeyPart(const UCollator* coll, const CA_uchar* source, int32_t bufSize, int32_t maxIteration = -1);
    ~NextSortKeyPart();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();
    virtual long getEventsPerIteration();

private:
    const UCollator *coll;
    const CA_uchar *source;
    int32_t bufSize;
    int32_t maxIteration;
    long events;
};

// Note: maxIteration = -1 -> repeat until the end of collation key
NextSortKeyPart::NextSortKeyPart(const UCollator* coll, const CA_uchar* source, int32_t bufSize, int32_t maxIteration /* = -1 */)
    :   coll(coll),
        source(source),
        bufSize(bufSize),
        maxIteration(maxIteration),
        events(0)
{
}

NextSortKeyPart::~NextSortKeyPart()
{
}

void NextSortKeyPart::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    uint8_t *part = (uint8_t *)malloc(bufSize);
    uint32_t state[2];
    UCharIterator iter;

    events = 0;
    for (int i = 0; i < source->count && U_SUCCESS(*status); i++) {
        uiter_setString(&iter, source->dataOf(i), source->lengthOf(i));
        state[0] = 0;
        state[1] = 0;
        int32_t partLen = bufSize;
        for (int32_t n = 0; U_SUCCESS(*status) && partLen == bufSize && (maxIteration < 0 || n < maxIteration); n++) {
            partLen = ucol_nextSortKeyPart(coll, &iter, state, part, bufSize, status);
            events++;
        }
    }
    free(part);
}

long NextSortKeyPart::getOperationsPerIteration()
{
    return source->count;
}

long NextSortKeyPart::getEventsPerIteration()
{
    return events;
}

//
// Test case taking a single test data array in UTF-8, calling ucol_nextSortKeyPart for each for the
// given buffer size
//
class NextSortKeyPartUTF8 : public UPerfFunction
{
public:
    NextSortKeyPartUTF8(const UCollator* coll, const CA_char* source, int32_t bufSize, int32_t maxIteration = -1);
    ~NextSortKeyPartUTF8();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();
    virtual long getEventsPerIteration();

private:
    const UCollator *coll;
    const CA_char *source;
    int32_t bufSize;
    int32_t maxIteration;
    long events;
};

// Note: maxIteration = -1 -> repeat until the end of collation key
NextSortKeyPartUTF8::NextSortKeyPartUTF8(const UCollator* coll, const CA_char* source, int32_t bufSize, int32_t maxIteration /* = -1 */)
    :   coll(coll),
        source(source),
        bufSize(bufSize),
        maxIteration(maxIteration),
        events(0)
{
}

NextSortKeyPartUTF8::~NextSortKeyPartUTF8()
{
}

void NextSortKeyPartUTF8::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    uint8_t *part = (uint8_t *)malloc(bufSize);
    uint32_t state[2];
    UCharIterator iter;

    events = 0;
    for (int i = 0; i < source->count && U_SUCCESS(*status); i++) {
        uiter_setUTF8(&iter, source->dataOf(i), source->lengthOf(i));
        state[0] = 0;
        state[1] = 0;
        int32_t partLen = bufSize;
        for (int32_t n = 0; U_SUCCESS(*status) && partLen == bufSize && (maxIteration < 0 || n < maxIteration); n++) {
            partLen = ucol_nextSortKeyPart(coll, &iter, state, part, bufSize, status);
            events++;
        }
    }
    free(part);
}

long NextSortKeyPartUTF8::getOperationsPerIteration()
{
    return source->count;
}

long NextSortKeyPartUTF8::getEventsPerIteration()
{
    return events;
}

// CPP API test cases

//
// Test case taking a single test data array, calling Collator::compare by permuting the test data
//
class CppCompare : public UPerfFunction
{
public:
    CppCompare(const Collator* coll, const CA_uchar* source, UBool useLen);
    ~CppCompare();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    const CA_uchar *source;
    UBool useLen;
    int32_t maxTestStrings;
};

CppCompare::CppCompare(const Collator* coll, const CA_uchar* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
    maxTestStrings = source->count > MAX_TEST_STRINGS_FOR_PERMUTING ? MAX_TEST_STRINGS_FOR_PERMUTING : source->count;
}

CppCompare::~CppCompare()
{
}

void CppCompare::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    // call compare for permutation of test data
    int32_t divisor = source->count / maxTestStrings;
    int32_t srcLen, tgtLen;
    int32_t cmp = 0;
    for (int32_t i = 0, numTestStringsI = 0; i < source->count && numTestStringsI < maxTestStrings; i++) {
        if (i % divisor) continue;
        numTestStringsI++;
        srcLen = useLen ? source->lengthOf(i) : -1;
        for (int32_t j = 0, numTestStringsJ = 0; j < source->count && numTestStringsJ < maxTestStrings; j++) {
            if (j % divisor) continue;
            numTestStringsJ++;
            tgtLen = useLen ? source->lengthOf(j) : -1;
            cmp += coll->compare(source->dataOf(i), srcLen, source->dataOf(j), tgtLen);
        }
    }
    // At the end, cmp must be 0
    if (cmp != 0) {
        *status = U_INTERNAL_PROGRAM_ERROR;
    }
}

long CppCompare::getOperationsPerIteration()
{
    return maxTestStrings * maxTestStrings;
}

//
// Test case taking two test data arrays, calling Collator::compare for strings at a same index
//
class CppCompare_2 : public UPerfFunction
{
public:
    CppCompare_2(const Collator* coll, const CA_uchar* source, const CA_uchar* target, UBool useLen);
    ~CppCompare_2();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    const CA_uchar *source;
    const CA_uchar *target;
    UBool useLen;
};

CppCompare_2::CppCompare_2(const Collator* coll, const CA_uchar* source, const CA_uchar* target, UBool useLen)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen)
{
}

CppCompare_2::~CppCompare_2()
{
}

void CppCompare_2::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    // call strcoll for two strings at the same index
    if (source->count < target->count) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    } else {
        for (int32_t i = 0; i < source->count; i++) {
            int32_t srcLen = useLen ? source->lengthOf(i) : -1;
            int32_t tgtLen = useLen ? target->lengthOf(i) : -1;
            coll->compare(source->dataOf(i), srcLen, target->dataOf(i), tgtLen);
        }
    }
}

long CppCompare_2::getOperationsPerIteration()
{
    return source->count;
}


//
// Test case taking a single test data array, calling Collator::compareUTF8 by permuting the test data
//
class CppCompareUTF8 : public UPerfFunction
{
public:
    CppCompareUTF8(const Collator* coll, const CA_char* source, UBool useLen);
    ~CppCompareUTF8();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    const CA_char *source;
    UBool useLen;
    int32_t maxTestStrings;
};

CppCompareUTF8::CppCompareUTF8(const Collator* coll, const CA_char* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
    maxTestStrings = source->count > MAX_TEST_STRINGS_FOR_PERMUTING ? MAX_TEST_STRINGS_FOR_PERMUTING : source->count;
}

CppCompareUTF8::~CppCompareUTF8()
{
}

void CppCompareUTF8::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    // call compareUTF8 for all permutations
    int32_t divisor = source->count / maxTestStrings;
    StringPiece src, tgt;
    int32_t cmp = 0;
    for (int32_t i = 0, numTestStringsI = 0; U_SUCCESS(*status) && i < source->count && numTestStringsI < maxTestStrings; i++) {
        if (i % divisor) continue;
        numTestStringsI++;

        if (useLen) {
            src.set(source->dataOf(i), source->lengthOf(i));
        } else {
            src.set(source->dataOf(i));
        }
        for (int32_t j = 0, numTestStringsJ = 0; U_SUCCESS(*status) && j < source->count && numTestStringsJ < maxTestStrings; j++) {
            if (j % divisor) continue;
            numTestStringsJ++;

            if (useLen) {
                tgt.set(source->dataOf(i), source->lengthOf(i));
            } else {
                tgt.set(source->dataOf(i));
            }
            cmp += coll->compareUTF8(src, tgt, *status);
        }
    }
    // At the end, cmp must be 0
    if (cmp != 0) {
        *status = U_INTERNAL_PROGRAM_ERROR;
    }
}

long CppCompareUTF8::getOperationsPerIteration()
{
    return maxTestStrings * maxTestStrings;
}


//
// Test case taking two test data arrays, calling Collator::compareUTF8 for strings at a same index
//
class CppCompareUTF8_2 : public UPerfFunction
{
public:
    CppCompareUTF8_2(const Collator* coll, const CA_char* source, const CA_char* target, UBool useLen);
    ~CppCompareUTF8_2();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    const CA_char *source;
    const CA_char *target;
    UBool useLen;
};

CppCompareUTF8_2::CppCompareUTF8_2(const Collator* coll, const CA_char* source, const CA_char* target, UBool useLen)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen)
{
}

CppCompareUTF8_2::~CppCompareUTF8_2()
{
}

void CppCompareUTF8_2::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    // call strcoll for two strings at the same index
    StringPiece src, tgt;
    if (source->count < target->count) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    } else {
        for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
            if (useLen) {
                src.set(source->dataOf(i), source->lengthOf(i));
                tgt.set(target->dataOf(i), target->lengthOf(i));
            } else {
                src.set(source->dataOf(i));
                tgt.set(target->dataOf(i));
            }
            coll->compareUTF8(src, tgt, *status);
        }
    }
}

long CppCompareUTF8_2::getOperationsPerIteration()
{
    return source->count;
}


//
// Test case taking a single test data array, calling Collator::getCollationKey for each
//
class CppGetCollationKey : public UPerfFunction
{
public:
    CppGetCollationKey(const Collator* coll, const CA_uchar* source, UBool useLen);
    ~CppGetCollationKey();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    const CA_uchar *source;
    UBool useLen;
};

CppGetCollationKey::CppGetCollationKey(const Collator* coll, const CA_uchar* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
}

CppGetCollationKey::~CppGetCollationKey()
{
}

void CppGetCollationKey::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    CollationKey key;
    for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
        coll->getCollationKey(source->dataOf(i), source->lengthOf(i), key, *status);
    }
}

long CppGetCollationKey::getOperationsPerIteration() {
    return source->count;
}

namespace {

struct CollatorAndCounter {
    CollatorAndCounter(const Collator& coll) : coll(coll), ucoll(NULL), counter(0) {}
    CollatorAndCounter(const Collator& coll, const UCollator *ucoll)
            : coll(coll), ucoll(ucoll), counter(0) {}
    const Collator& coll;
    const UCollator *ucoll;
    int32_t counter;
};

int32_t U_CALLCONV
UniStrCollatorComparator(const void* context, const void* left, const void* right) {
    CollatorAndCounter& cc = *(CollatorAndCounter*)context;
    const UnicodeString& leftString = **(const UnicodeString**)left;
    const UnicodeString& rightString = **(const UnicodeString**)right;
    UErrorCode errorCode = U_ZERO_ERROR;
    ++cc.counter;
    return cc.coll.compare(leftString, rightString, errorCode);
}

}  // namespace

class CollPerfFunction : public UPerfFunction {
public:
    CollPerfFunction(const Collator& coll, const UCollator *ucoll)
            : coll(coll), ucoll(ucoll), ops(0) {}
    virtual ~CollPerfFunction();
    /** Calls call() to set the ops field, and returns that. */
    virtual long getOperationsPerIteration();

protected:
    const Collator& coll;
    const UCollator *ucoll;
    int32_t ops;
};

CollPerfFunction::~CollPerfFunction() {}

long CollPerfFunction::getOperationsPerIteration() {
    UErrorCode errorCode = U_ZERO_ERROR;
    call(&errorCode);
    return U_SUCCESS(errorCode) ? ops : 0;
}

class UniStrCollPerfFunction : public CollPerfFunction {
public:
    UniStrCollPerfFunction(const Collator& coll, const UCollator *ucoll, const CA_uchar* data16)
            : CollPerfFunction(coll, ucoll), d16(data16),
              source(new UnicodeString*[d16->count]) {
        for (int32_t i = 0; i < d16->count; ++i) {
            source[i] = new UnicodeString(TRUE, d16->dataOf(i), d16->lengthOf(i));
        }
    }
    virtual ~UniStrCollPerfFunction();

protected:
    const CA_uchar* d16;
    UnicodeString** source;
};

UniStrCollPerfFunction::~UniStrCollPerfFunction() {
    for (int32_t i = 0; i < d16->count; ++i) {
        delete source[i];
    }
    delete[] source;
}

//
// Test case sorting an array of UnicodeString pointers.
//
class UniStrSort : public UniStrCollPerfFunction {
public:
    UniStrSort(const Collator& coll, const UCollator *ucoll, const CA_uchar* data16)
            : UniStrCollPerfFunction(coll, ucoll, data16),
              dest(new UnicodeString*[d16->count]) {}
    virtual ~UniStrSort();
    virtual void call(UErrorCode* status);

private:
    UnicodeString** dest;  // aliases only
};

UniStrSort::~UniStrSort() {
    delete[] dest;
}

void UniStrSort::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    CollatorAndCounter cc(coll);
    int32_t count = d16->count;
    memcpy(dest, source, count * sizeof(UnicodeString *));
    uprv_sortArray(dest, count, (int32_t)sizeof(UnicodeString *),
                   UniStrCollatorComparator, &cc, TRUE, status);
    ops = cc.counter;
}

namespace {

int32_t U_CALLCONV
StringPieceCollatorComparator(const void* context, const void* left, const void* right) {
    CollatorAndCounter& cc = *(CollatorAndCounter*)context;
    const StringPiece& leftString = *(const StringPiece*)left;
    const StringPiece& rightString = *(const StringPiece*)right;
    UErrorCode errorCode = U_ZERO_ERROR;
    ++cc.counter;
    return cc.coll.compareUTF8(leftString, rightString, errorCode);
}

int32_t U_CALLCONV
StringPieceUCollatorComparator(const void* context, const void* left, const void* right) {
    CollatorAndCounter& cc = *(CollatorAndCounter*)context;
    const StringPiece& leftString = *(const StringPiece*)left;
    const StringPiece& rightString = *(const StringPiece*)right;
    UErrorCode errorCode = U_ZERO_ERROR;
    ++cc.counter;
    return ucol_strcollUTF8(cc.ucoll,
                            leftString.data(), leftString.length(),
                            rightString.data(), rightString.length(), &errorCode);
}

}  // namespace

class StringPieceCollPerfFunction : public CollPerfFunction {
public:
    StringPieceCollPerfFunction(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : CollPerfFunction(coll, ucoll), d8(data8),
              source(new StringPiece[d8->count]) {
        for (int32_t i = 0; i < d8->count; ++i) {
            source[i].set(d8->dataOf(i), d8->lengthOf(i));
        }
    }
    virtual ~StringPieceCollPerfFunction();

protected:
    const CA_char* d8;
    StringPiece* source;
};

StringPieceCollPerfFunction::~StringPieceCollPerfFunction() {
    delete[] source;
}

class StringPieceSort : public StringPieceCollPerfFunction {
public:
    StringPieceSort(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : StringPieceCollPerfFunction(coll, ucoll, data8),
              dest(new StringPiece[d8->count]) {}
    virtual ~StringPieceSort();

protected:
    StringPiece* dest;
};

StringPieceSort::~StringPieceSort() {
    delete[] dest;
}

//
// Test case sorting an array of UTF-8 StringPiece's with Collator::compareUTF8().
//
class StringPieceSortCpp : public StringPieceSort {
public:
    StringPieceSortCpp(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : StringPieceSort(coll, ucoll, data8) {}
    virtual ~StringPieceSortCpp();
    virtual void call(UErrorCode* status);
};

StringPieceSortCpp::~StringPieceSortCpp() {}

void StringPieceSortCpp::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    CollatorAndCounter cc(coll);
    int32_t count = d8->count;
    memcpy(dest, source, count * sizeof(StringPiece));
    uprv_sortArray(dest, count, (int32_t)sizeof(StringPiece),
                   StringPieceCollatorComparator, &cc, TRUE, status);
    ops = cc.counter;
}

//
// Test case sorting an array of UTF-8 StringPiece's with ucol_strcollUTF8().
//
class StringPieceSortC : public StringPieceSort {
public:
    StringPieceSortC(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : StringPieceSort(coll, ucoll, data8) {}
    virtual ~StringPieceSortC();
    virtual void call(UErrorCode* status);
};

StringPieceSortC::~StringPieceSortC() {}

void StringPieceSortC::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    CollatorAndCounter cc(coll, ucoll);
    int32_t count = d8->count;
    memcpy(dest, source, count * sizeof(StringPiece));
    uprv_sortArray(dest, count, (int32_t)sizeof(StringPiece),
                   StringPieceUCollatorComparator, &cc, TRUE, status);
    ops = cc.counter;
}

//
// Test case performing binary searches in a sorted array of UnicodeString pointers.
//
class UniStrBinSearch : public UniStrCollPerfFunction {
public:
    UniStrBinSearch(const Collator& coll, const UCollator *ucoll, const CA_uchar* data16)
            : UniStrCollPerfFunction(coll, ucoll, data16) {}
    virtual ~UniStrBinSearch();
    virtual void call(UErrorCode* status);
};

UniStrBinSearch::~UniStrBinSearch() {}

void UniStrBinSearch::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    CollatorAndCounter cc(coll);
    int32_t count = d16->count;
    for (int32_t i = 0; i < count; ++i) {
        (void)uprv_stableBinarySearch((char *)source, count,
                                      source + i, (int32_t)sizeof(UnicodeString *),
                                      UniStrCollatorComparator, &cc);
    }
    ops = cc.counter;
}

class StringPieceBinSearch : public StringPieceCollPerfFunction {
public:
    StringPieceBinSearch(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : StringPieceCollPerfFunction(coll, ucoll, data8) {}
    virtual ~StringPieceBinSearch();
};

StringPieceBinSearch::~StringPieceBinSearch() {}

//
// Test case performing binary searches in a sorted array of UTF-8 StringPiece's
// with Collator::compareUTF8().
//
class StringPieceBinSearchCpp : public StringPieceBinSearch {
public:
    StringPieceBinSearchCpp(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : StringPieceBinSearch(coll, ucoll, data8) {}
    virtual ~StringPieceBinSearchCpp();
    virtual void call(UErrorCode* status);
};

StringPieceBinSearchCpp::~StringPieceBinSearchCpp() {}

void StringPieceBinSearchCpp::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    CollatorAndCounter cc(coll);
    int32_t count = d8->count;
    for (int32_t i = 0; i < count; ++i) {
        (void)uprv_stableBinarySearch((char *)source, count,
                                      source + i, (int32_t)sizeof(StringPiece),
                                      StringPieceCollatorComparator, &cc);
    }
    ops = cc.counter;
}

//
// Test case performing binary searches in a sorted array of UTF-8 StringPiece's
// with ucol_strcollUTF8().
//
class StringPieceBinSearchC : public StringPieceBinSearch {
public:
    StringPieceBinSearchC(const Collator& coll, const UCollator *ucoll, const CA_char* data8)
            : StringPieceBinSearch(coll, ucoll, data8) {}
    virtual ~StringPieceBinSearchC();
    virtual void call(UErrorCode* status);
};

StringPieceBinSearchC::~StringPieceBinSearchC() {}

void StringPieceBinSearchC::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    CollatorAndCounter cc(coll, ucoll);
    int32_t count = d8->count;
    for (int32_t i = 0; i < count; ++i) {
        (void)uprv_stableBinarySearch((char *)source, count,
                                      source + i, (int32_t)sizeof(StringPiece),
                                      StringPieceUCollatorComparator, &cc);
    }
    ops = cc.counter;
}


class CollPerf2Test : public UPerfTest
{
public:
    CollPerf2Test(int32_t argc, const char *argv[], UErrorCode &status);
    ~CollPerf2Test();
    virtual UPerfFunction* runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *par = NULL);

private:
    UCollator* coll;
    Collator* collObj;

    int32_t count;
    CA_uchar* data16;
    CA_char* data8;

    CA_uchar* modData16;
    CA_char* modData8;

    CA_uchar* sortedData16;
    CA_char* sortedData8;

    CA_uchar* randomData16;
    CA_char* randomData8;

    const CA_uchar* getData16(UErrorCode &status);
    const CA_char* getData8(UErrorCode &status);

    const CA_uchar* getModData16(UErrorCode &status);
    const CA_char* getModData8(UErrorCode &status);

    const CA_uchar* getSortedData16(UErrorCode &status);
    const CA_char* getSortedData8(UErrorCode &status);

    const CA_uchar* getRandomData16(UErrorCode &status);
    const CA_char* getRandomData8(UErrorCode &status);

    static CA_uchar* sortData16(
            const CA_uchar* d16,
            UComparator *cmp, const void *context,
            UErrorCode &status);
    static CA_char* getData8FromData16(const CA_uchar* d16, UErrorCode &status);

    UPerfFunction* TestStrcoll();
    UPerfFunction* TestStrcollNull();
    UPerfFunction* TestStrcollSimilar();

    UPerfFunction* TestStrcollUTF8();
    UPerfFunction* TestStrcollUTF8Null();
    UPerfFunction* TestStrcollUTF8Similar();

    UPerfFunction* TestGetSortKey();
    UPerfFunction* TestGetSortKeyNull();

    UPerfFunction* TestNextSortKeyPart_4All();
    UPerfFunction* TestNextSortKeyPart_4x2();
    UPerfFunction* TestNextSortKeyPart_4x4();
    UPerfFunction* TestNextSortKeyPart_4x8();
    UPerfFunction* TestNextSortKeyPart_32All();
    UPerfFunction* TestNextSortKeyPart_32x2();

    UPerfFunction* TestNextSortKeyPartUTF8_4All();
    UPerfFunction* TestNextSortKeyPartUTF8_4x2();
    UPerfFunction* TestNextSortKeyPartUTF8_4x4();
    UPerfFunction* TestNextSortKeyPartUTF8_4x8();
    UPerfFunction* TestNextSortKeyPartUTF8_32All();
    UPerfFunction* TestNextSortKeyPartUTF8_32x2();

    UPerfFunction* TestCppCompare();
    UPerfFunction* TestCppCompareNull();
    UPerfFunction* TestCppCompareSimilar();

    UPerfFunction* TestCppCompareUTF8();
    UPerfFunction* TestCppCompareUTF8Null();
    UPerfFunction* TestCppCompareUTF8Similar();

    UPerfFunction* TestCppGetCollationKey();
    UPerfFunction* TestCppGetCollationKeyNull();

    UPerfFunction* TestUniStrSort();
    UPerfFunction* TestStringPieceSortCpp();
    UPerfFunction* TestStringPieceSortC();

    UPerfFunction* TestUniStrBinSearch();
    UPerfFunction* TestStringPieceBinSearchCpp();
    UPerfFunction* TestStringPieceBinSearchC();
};

CollPerf2Test::CollPerf2Test(int32_t argc, const char *argv[], UErrorCode &status) :
    UPerfTest(argc, argv, status),
    coll(NULL),
    collObj(NULL),
    count(0),
    data16(NULL),
    data8(NULL),
    modData16(NULL),
    modData8(NULL),
    sortedData16(NULL),
    sortedData8(NULL),
    randomData16(NULL),
    randomData8(NULL)
{
    if (U_FAILURE(status)) {
        return;
    }

    if (locale == NULL){
        locale = "root";
    }

    // Set up an ICU collator.
    // Starting with ICU 54 (ticket #8260), this supports standard collation locale keywords.
    coll = ucol_open(locale, &status);
    collObj = Collator::createInstance(locale, status);
}

CollPerf2Test::~CollPerf2Test()
{
    ucol_close(coll);
    delete collObj;

    delete data16;
    delete data8;
    delete modData16;
    delete modData8;
    delete sortedData16;
    delete sortedData8;
    delete randomData16;
    delete randomData8;
}

#define MAX_NUM_DATA 10000

const CA_uchar* CollPerf2Test::getData16(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (data16) return data16;

    CA_uchar* d16 = new CA_uchar();
    const UChar *line = NULL;
    int32_t len = 0;
    int32_t numData = 0;

    for (;;) {
        line = ucbuf_readline(ucharBuf, &len, &status);
        if (line == NULL || U_FAILURE(status)) break;

        // Refer to the source code of ucbuf_readline()
        // 1. 'len' includes the line terminal symbols
        // 2. The length of the line terminal symbols is only one character
        // 3. The Windows CR LF line terminal symbols will be converted to CR

        if (len == 1 || line[0] == 0x23 /* '#' */) {
            continue; // skip empty/comment line
        } else {
            d16->append_one(len);
            UChar *p = d16->last();
            u_memcpy(p, line, len - 1);  // exclude the CR
            p[len - 1] = 0;  // NUL-terminate

            numData++;
            if (numData >= MAX_NUM_DATA) break;
        }
    }

    if (U_SUCCESS(status)) {
        data16 = d16;
    } else {
        delete d16;
    }

    return data16;
}

const CA_char* CollPerf2Test::getData8(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (data8) return data8;
    return data8 = getData8FromData16(getData16(status), status);
}

const CA_uchar* CollPerf2Test::getModData16(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (modData16) return modData16;

    const CA_uchar* d16 = getData16(status);
    if (U_FAILURE(status)) return NULL;

    CA_uchar* modData16 = new CA_uchar();

    for (int32_t i = 0; i < d16->count; i++) {
        const UChar *s = d16->dataOf(i);
        int32_t len = d16->lengthOf(i) + 1; // including NULL terminator

        modData16->append_one(len);
        u_memcpy(modData16->last(), s, len);

        // replacing the last character with a different character
        UChar *lastChar = &modData16->last()[len -2];
        for (int32_t j = i + 1; j != i; j++) {
            if (j >= d16->count) {
                j = 0;
            }
            const UChar *s1 = d16->dataOf(j);
            UChar lastChar1 = s1[d16->lengthOf(j) - 1];
            if (*lastChar != lastChar1) {
                *lastChar = lastChar1;
                break;
            }
        }
    }

    return modData16;
}

const CA_char* CollPerf2Test::getModData8(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (modData8) return modData8;
    return modData8 = getData8FromData16(getModData16(status), status);
}

namespace {

struct ArrayAndColl {
    ArrayAndColl(const CA_uchar* a, const Collator& c) : d16(a), coll(c) {}
    const CA_uchar* d16;
    const Collator& coll;
};

int32_t U_CALLCONV
U16CollatorComparator(const void* context, const void* left, const void* right) {
    const ArrayAndColl& ac = *(const ArrayAndColl*)context;
    const CA_uchar* d16 = ac.d16;
    int32_t leftIndex = *(const int32_t*)left;
    int32_t rightIndex = *(const int32_t*)right;
    UErrorCode errorCode = U_ZERO_ERROR;
    return ac.coll.compare(d16->dataOf(leftIndex), d16->lengthOf(leftIndex),
                           d16->dataOf(rightIndex), d16->lengthOf(rightIndex),
                           errorCode);
}

int32_t U_CALLCONV
U16HashComparator(const void* context, const void* left, const void* right) {
    const CA_uchar* d16 = (const CA_uchar*)context;
    int32_t leftIndex = *(const int32_t*)left;
    int32_t rightIndex = *(const int32_t*)right;
    int32_t leftHash = ustr_hashUCharsN(d16->dataOf(leftIndex), d16->lengthOf(leftIndex));
    int32_t rightHash = ustr_hashUCharsN(d16->dataOf(rightIndex), d16->lengthOf(rightIndex));
    return leftHash < rightHash ? -1 : leftHash == rightHash ? 0 : 1;
}

}  // namespace

const CA_uchar* CollPerf2Test::getSortedData16(UErrorCode &status) {
    if (U_FAILURE(status)) return NULL;
    if (sortedData16) return sortedData16;

    ArrayAndColl ac(getData16(status), *collObj);
    return sortedData16 = sortData16(ac.d16, U16CollatorComparator, &ac, status);
}

const CA_char* CollPerf2Test::getSortedData8(UErrorCode &status) {
    if (U_FAILURE(status)) return NULL;
    if (sortedData8) return sortedData8;
    return sortedData8 = getData8FromData16(getSortedData16(status), status);
}

const CA_uchar* CollPerf2Test::getRandomData16(UErrorCode &status) {
    if (U_FAILURE(status)) return NULL;
    if (randomData16) return randomData16;

    // Sort the strings by their hash codes, which should be a reasonably pseudo-random order.
    const CA_uchar* d16 = getData16(status);
    return randomData16 = sortData16(d16, U16HashComparator, d16, status);
}

const CA_char* CollPerf2Test::getRandomData8(UErrorCode &status) {
    if (U_FAILURE(status)) return NULL;
    if (randomData8) return randomData8;
    return randomData8 = getData8FromData16(getRandomData16(status), status);
}

CA_uchar* CollPerf2Test::sortData16(const CA_uchar* d16,
                                    UComparator *cmp, const void *context,
                                    UErrorCode &status) {
    if (U_FAILURE(status)) return NULL;

    LocalArray<int32_t> indexes(new int32_t[d16->count]);
    for (int32_t i = 0; i < d16->count; ++i) {
        indexes[i] = i;
    }
    uprv_sortArray(indexes.getAlias(), d16->count, 4, cmp, context, TRUE, &status);
    if (U_FAILURE(status)) return NULL;

    // Copy the strings in sorted order into a new array.
    LocalPointer<CA_uchar> newD16(new CA_uchar());
    for (int32_t i = 0; i < d16->count; i++) {
        int32_t j = indexes[i];
        const UChar* s = d16->dataOf(j);
        int32_t len = d16->lengthOf(j);
        int32_t capacity = len + 1;  // including NULL terminator
        newD16->append_one(capacity);
        u_memcpy(newD16->last(), s, capacity);
    }

    if (U_SUCCESS(status)) {
        return newD16.orphan();
    } else {
        return NULL;
    }
}

CA_char* CollPerf2Test::getData8FromData16(const CA_uchar* d16, UErrorCode &status) {
    if (U_FAILURE(status)) return NULL;

    // UTF-16 -> UTF-8 conversion
    LocalPointer<CA_char> d8(new CA_char());
    for (int32_t i = 0; i < d16->count; i++) {
        const UChar *s16 = d16->dataOf(i);
        int32_t length16 = d16->lengthOf(i);

        // get length in UTF-8
        int32_t length8;
        u_strToUTF8(NULL, 0, &length8, s16, length16, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR || status == U_ZERO_ERROR){
            status = U_ZERO_ERROR;
        } else {
            break;
        }
        int32_t capacity8 = length8 + 1;  // plus terminal NULL
        d8->append_one(capacity8);

        // convert to UTF-8
        u_strToUTF8(d8->last(), capacity8, NULL, s16, length16, &status);
        if (U_FAILURE(status)) break;
    }

    if (U_SUCCESS(status)) {
        return d8.orphan();
    } else {
        return NULL;
    }
}

UPerfFunction*
CollPerf2Test::runIndexedTest(int32_t index, UBool exec, const char *&name, char *par /*= NULL*/)
{
    (void)par;
    TESTCASE_AUTO_BEGIN;

    TESTCASE_AUTO(TestStrcoll);
    TESTCASE_AUTO(TestStrcollNull);
    TESTCASE_AUTO(TestStrcollSimilar);

    TESTCASE_AUTO(TestStrcollUTF8);
    TESTCASE_AUTO(TestStrcollUTF8Null);
    TESTCASE_AUTO(TestStrcollUTF8Similar);

    TESTCASE_AUTO(TestGetSortKey);
    TESTCASE_AUTO(TestGetSortKeyNull);

    TESTCASE_AUTO(TestNextSortKeyPart_4All);
    TESTCASE_AUTO(TestNextSortKeyPart_4x4);
    TESTCASE_AUTO(TestNextSortKeyPart_4x8);
    TESTCASE_AUTO(TestNextSortKeyPart_32All);
    TESTCASE_AUTO(TestNextSortKeyPart_32x2);

    TESTCASE_AUTO(TestNextSortKeyPartUTF8_4All);
    TESTCASE_AUTO(TestNextSortKeyPartUTF8_4x4);
    TESTCASE_AUTO(TestNextSortKeyPartUTF8_4x8);
    TESTCASE_AUTO(TestNextSortKeyPartUTF8_32All);
    TESTCASE_AUTO(TestNextSortKeyPartUTF8_32x2);

    TESTCASE_AUTO(TestCppCompare);
    TESTCASE_AUTO(TestCppCompareNull);
    TESTCASE_AUTO(TestCppCompareSimilar);

    TESTCASE_AUTO(TestCppCompareUTF8);
    TESTCASE_AUTO(TestCppCompareUTF8Null);
    TESTCASE_AUTO(TestCppCompareUTF8Similar);

    TESTCASE_AUTO(TestCppGetCollationKey);
    TESTCASE_AUTO(TestCppGetCollationKeyNull);

    TESTCASE_AUTO(TestUniStrSort);
    TESTCASE_AUTO(TestStringPieceSortCpp);
    TESTCASE_AUTO(TestStringPieceSortC);

    TESTCASE_AUTO(TestUniStrBinSearch);
    TESTCASE_AUTO(TestStringPieceBinSearchCpp);
    TESTCASE_AUTO(TestStringPieceBinSearchC);

    TESTCASE_AUTO_END;
    return NULL;
}



UPerfFunction* CollPerf2Test::TestStrcoll()
{
    UErrorCode status = U_ZERO_ERROR;
    Strcoll *testCase = new Strcoll(coll, getData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollNull()
{
    UErrorCode status = U_ZERO_ERROR;
    Strcoll *testCase = new Strcoll(coll, getData16(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollSimilar()
{
    UErrorCode status = U_ZERO_ERROR;
    Strcoll_2 *testCase = new Strcoll_2(coll, getData16(status), getModData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollUTF8()
{
    UErrorCode status = U_ZERO_ERROR;
    StrcollUTF8 *testCase = new StrcollUTF8(coll, getData8(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollUTF8Null()
{
    UErrorCode status = U_ZERO_ERROR;
    StrcollUTF8 *testCase = new StrcollUTF8(coll, getData8(status),FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollUTF8Similar()
{
    UErrorCode status = U_ZERO_ERROR;
    StrcollUTF8_2 *testCase = new StrcollUTF8_2(coll, getData8(status), getModData8(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestGetSortKey()
{
    UErrorCode status = U_ZERO_ERROR;
    GetSortKey *testCase = new GetSortKey(coll, getData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestGetSortKeyNull()
{
    UErrorCode status = U_ZERO_ERROR;
    GetSortKey *testCase = new GetSortKey(coll, getData16(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart_4All()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 4 /* bufSize */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart_4x4()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 4 /* bufSize */, 4 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart_4x8()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 4 /* bufSize */, 8 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart_32All()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 32 /* bufSize */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart_32x2()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 32 /* bufSize */, 2 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPartUTF8_4All()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPartUTF8 *testCase = new NextSortKeyPartUTF8(coll, getData8(status), 4 /* bufSize */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPartUTF8_4x4()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPartUTF8 *testCase = new NextSortKeyPartUTF8(coll, getData8(status), 4 /* bufSize */, 4 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPartUTF8_4x8()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPartUTF8 *testCase = new NextSortKeyPartUTF8(coll, getData8(status), 4 /* bufSize */, 8 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPartUTF8_32All()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPartUTF8 *testCase = new NextSortKeyPartUTF8(coll, getData8(status), 32 /* bufSize */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPartUTF8_32x2()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPartUTF8 *testCase = new NextSortKeyPartUTF8(coll, getData8(status), 32 /* bufSize */, 2 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompare()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompare *testCase = new CppCompare(collObj, getData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareNull()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompare *testCase = new CppCompare(collObj, getData16(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareSimilar()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompare_2 *testCase = new CppCompare_2(collObj, getData16(status), getModData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareUTF8()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompareUTF8 *testCase = new CppCompareUTF8(collObj, getData8(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareUTF8Null()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompareUTF8 *testCase = new CppCompareUTF8(collObj, getData8(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareUTF8Similar()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompareUTF8_2 *testCase = new CppCompareUTF8_2(collObj, getData8(status), getModData8(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppGetCollationKey()
{
    UErrorCode status = U_ZERO_ERROR;
    CppGetCollationKey *testCase = new CppGetCollationKey(collObj, getData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppGetCollationKeyNull()
{
    UErrorCode status = U_ZERO_ERROR;
    CppGetCollationKey *testCase = new CppGetCollationKey(collObj, getData16(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestUniStrSort() {
    UErrorCode status = U_ZERO_ERROR;
    UPerfFunction *testCase = new UniStrSort(*collObj, coll, getRandomData16(status));
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStringPieceSortCpp() {
    UErrorCode status = U_ZERO_ERROR;
    UPerfFunction *testCase = new StringPieceSortCpp(*collObj, coll, getRandomData8(status));
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStringPieceSortC() {
    UErrorCode status = U_ZERO_ERROR;
    UPerfFunction *testCase = new StringPieceSortC(*collObj, coll, getRandomData8(status));
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestUniStrBinSearch() {
    UErrorCode status = U_ZERO_ERROR;
    UPerfFunction *testCase = new UniStrBinSearch(*collObj, coll, getSortedData16(status));
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStringPieceBinSearchCpp() {
    UErrorCode status = U_ZERO_ERROR;
    UPerfFunction *testCase = new StringPieceBinSearchCpp(*collObj, coll, getSortedData8(status));
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStringPieceBinSearchC() {
    UErrorCode status = U_ZERO_ERROR;
    UPerfFunction *testCase = new StringPieceBinSearchC(*collObj, coll, getSortedData8(status));
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}


int main(int argc, const char *argv[])
{
    UErrorCode status = U_ZERO_ERROR;
    CollPerf2Test test(argc, argv, status);

    if (U_FAILURE(status)){
        printf("The error is %s\n", u_errorName(status));
        //TODO: print usage here
        return status;
    }

    if (test.run() == FALSE){
        fprintf(stderr, "FAILED: Tests could not be run please check the arguments.\n");
        return -1;
    }
    return 0;
}
