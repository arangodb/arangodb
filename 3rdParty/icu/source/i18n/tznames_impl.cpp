/*
*******************************************************************************
* Copyright (C) 2011-2012, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File TZNAMES_IMPL.CPP
*
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/ustring.h"
#include "unicode/timezone.h"

#include "tznames_impl.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "zonemeta.h"
#include "ucln_in.h"
#include "uvector.h"
#include "olsontz.h"


U_NAMESPACE_BEGIN

#define ZID_KEY_MAX  128
#define MZ_PREFIX_LEN 5

static const char gZoneStrings[]        = "zoneStrings";
static const char gMZPrefix[]           = "meta:";

static const char* KEYS[]               = {"lg", "ls", "ld", "sg", "ss", "sd"};
static const int32_t KEYS_SIZE = (sizeof KEYS / sizeof KEYS[0]);

static const char gEcTag[]              = "ec";

static const char EMPTY[]               = "<empty>";   // place holder for empty ZNames/TZNames

static const UTimeZoneNameType ALL_NAME_TYPES[] = {
    UTZNM_LONG_GENERIC, UTZNM_LONG_STANDARD, UTZNM_LONG_DAYLIGHT,
    UTZNM_SHORT_GENERIC, UTZNM_SHORT_STANDARD, UTZNM_SHORT_DAYLIGHT,
    UTZNM_UNKNOWN // unknown as the last one
};

#define DEFAULT_CHARACTERNODE_CAPACITY 1

// ---------------------------------------------------
// CharacterNode class implementation
// ---------------------------------------------------
void CharacterNode::clear() {
    uprv_memset(this, 0, sizeof(*this));
}

void CharacterNode::deleteValues(UObjectDeleter *valueDeleter) {
    if (fValues == NULL) {
        // Do nothing.
    } else if (!fHasValuesVector) {
        if (valueDeleter) {
            valueDeleter(fValues);
        }
    } else {
        delete (UVector *)fValues;
    }
}

void
CharacterNode::addValue(void *value, UObjectDeleter *valueDeleter, UErrorCode &status) {
    if (U_FAILURE(status)) {
        if (valueDeleter) {
            valueDeleter(value);
        }
        return;
    }
    if (fValues == NULL) {
        fValues = value;
    } else {
        // At least one value already.
        if (!fHasValuesVector) {
            // There is only one value so far, and not in a vector yet.
            // Create a vector and add the old value.
            UVector *values = new UVector(valueDeleter, NULL, DEFAULT_CHARACTERNODE_CAPACITY, status);
            if (U_FAILURE(status)) {
                if (valueDeleter) {
                    valueDeleter(value);
                }
                return;
            }
            values->addElement(fValues, status);
            fValues = values;
            fHasValuesVector = TRUE;
        }
        // Add the new value.
        ((UVector *)fValues)->addElement(value, status);
    }
}

// ---------------------------------------------------
// TextTrieMapSearchResultHandler class implementation
// ---------------------------------------------------
TextTrieMapSearchResultHandler::~TextTrieMapSearchResultHandler(){
}

// ---------------------------------------------------
// TextTrieMap class implementation
// ---------------------------------------------------
TextTrieMap::TextTrieMap(UBool ignoreCase, UObjectDeleter *valueDeleter)
: fIgnoreCase(ignoreCase), fNodes(NULL), fNodesCapacity(0), fNodesCount(0), 
  fLazyContents(NULL), fIsEmpty(TRUE), fValueDeleter(valueDeleter) {
}

TextTrieMap::~TextTrieMap() {
    int32_t index;
    for (index = 0; index < fNodesCount; ++index) {
        fNodes[index].deleteValues(fValueDeleter);
    }
    uprv_free(fNodes);
    if (fLazyContents != NULL) {
        for (int32_t i=0; i<fLazyContents->size(); i+=2) {
            if (fValueDeleter) {
                fValueDeleter(fLazyContents->elementAt(i+1));
            }
        } 
        delete fLazyContents;
    }
}

int32_t TextTrieMap::isEmpty() const {
    // Use a separate field for fIsEmpty because it will remain unchanged once the
    //   Trie is built, while fNodes and fLazyContents change with the lazy init
    //   of the nodes structure.  Trying to test the changing fields has
    //   thread safety complications.
    return fIsEmpty;
}


//  We defer actually building the TextTrieMap node structure until the first time a
//     search is performed.  put() simply saves the parameters in case we do
//     eventually need to build it.
//     
void
TextTrieMap::put(const UnicodeString &key, void *value, ZNStringPool &sp, UErrorCode &status) {
    const UChar *s = sp.get(key, status);
    put(s, value, status);
}

// This method is for designed for a persistent key, such as string key stored in
// resource bundle.
void
TextTrieMap::put(const UChar *key, void *value, UErrorCode &status) {
    fIsEmpty = FALSE;
    if (fLazyContents == NULL) {
        fLazyContents = new UVector(status);
        if (fLazyContents == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(fLazyContents != NULL);
    UChar *s = const_cast<UChar *>(key);
    fLazyContents->addElement(s, status);
    fLazyContents->addElement(value, status);
}

void
TextTrieMap::putImpl(const UnicodeString &key, void *value, UErrorCode &status) {
    if (fNodes == NULL) {
        fNodesCapacity = 512;
        fNodes = (CharacterNode *)uprv_malloc(fNodesCapacity * sizeof(CharacterNode));
        fNodes[0].clear();  // Init root node.
        fNodesCount = 1;
    }

    UnicodeString foldedKey;
    const UChar *keyBuffer;
    int32_t keyLength;
    if (fIgnoreCase) {
        // Ok to use fastCopyFrom() because we discard the copy when we return.
        foldedKey.fastCopyFrom(key).foldCase();
        keyBuffer = foldedKey.getBuffer();
        keyLength = foldedKey.length();
    } else {
        keyBuffer = key.getBuffer();
        keyLength = key.length();
    }

    CharacterNode *node = fNodes;
    int32_t index;
    for (index = 0; index < keyLength; ++index) {
        node = addChildNode(node, keyBuffer[index], status);
    }
    node->addValue(value, fValueDeleter, status);
}

UBool
TextTrieMap::growNodes() {
    if (fNodesCapacity == 0xffff) {
        return FALSE;  // We use 16-bit node indexes.
    }
    int32_t newCapacity = fNodesCapacity + 1000;
    if (newCapacity > 0xffff) {
        newCapacity = 0xffff;
    }
    CharacterNode *newNodes = (CharacterNode *)uprv_malloc(newCapacity * sizeof(CharacterNode));
    if (newNodes == NULL) {
        return FALSE;
    }
    uprv_memcpy(newNodes, fNodes, fNodesCount * sizeof(CharacterNode));
    uprv_free(fNodes);
    fNodes = newNodes;
    fNodesCapacity = newCapacity;
    return TRUE;
}

CharacterNode*
TextTrieMap::addChildNode(CharacterNode *parent, UChar c, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    // Linear search of the sorted list of children.
    uint16_t prevIndex = 0;
    uint16_t nodeIndex = parent->fFirstChild;
    while (nodeIndex > 0) {
        CharacterNode *current = fNodes + nodeIndex;
        UChar childCharacter = current->fCharacter;
        if (childCharacter == c) {
            return current;
        } else if (childCharacter > c) {
            break;
        }
        prevIndex = nodeIndex;
        nodeIndex = current->fNextSibling;
    }

    // Ensure capacity. Grow fNodes[] if needed.
    if (fNodesCount == fNodesCapacity) {
        int32_t parentIndex = (int32_t)(parent - fNodes);
        if (!growNodes()) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        parent = fNodes + parentIndex;
    }

    // Insert a new child node with c in sorted order.
    CharacterNode *node = fNodes + fNodesCount;
    node->clear();
    node->fCharacter = c;
    node->fNextSibling = nodeIndex;
    if (prevIndex == 0) {
        parent->fFirstChild = (uint16_t)fNodesCount;
    } else {
        fNodes[prevIndex].fNextSibling = (uint16_t)fNodesCount;
    }
    ++fNodesCount;
    return node;
}

CharacterNode*
TextTrieMap::getChildNode(CharacterNode *parent, UChar c) const {
    // Linear search of the sorted list of children.
    uint16_t nodeIndex = parent->fFirstChild;
    while (nodeIndex > 0) {
        CharacterNode *current = fNodes + nodeIndex;
        UChar childCharacter = current->fCharacter;
        if (childCharacter == c) {
            return current;
        } else if (childCharacter > c) {
            break;
        }
        nodeIndex = current->fNextSibling;
    }
    return NULL;
}

// Mutex for protecting the lazy creation of the Trie node structure on the first call to search().
static UMTX TextTrieMutex;

// buildTrie() - The Trie node structure is needed.  Create it from the data that was
//               saved at the time the ZoneStringFormatter was created.  The Trie is only
//               needed for parsing operations, which are less common than formatting,
//               and the Trie is big, which is why its creation is deferred until first use.
void TextTrieMap::buildTrie(UErrorCode &status) {
    umtx_lock(&TextTrieMutex);
    if (fLazyContents != NULL) {
        for (int32_t i=0; i<fLazyContents->size(); i+=2) {
            const UChar *key = (UChar *)fLazyContents->elementAt(i);
            void  *val = fLazyContents->elementAt(i+1);
            UnicodeString keyString(TRUE, key, -1);  // Aliasing UnicodeString constructor.
            putImpl(keyString, val, status);
        }
        delete fLazyContents;
        fLazyContents = NULL; 
    }
    umtx_unlock(&TextTrieMutex);
}

void
TextTrieMap::search(const UnicodeString &text, int32_t start,
                  TextTrieMapSearchResultHandler *handler, UErrorCode &status) const {
    UBool trieNeedsInitialization = FALSE;
    UMTX_CHECK(&TextTrieMutex, fLazyContents != NULL, trieNeedsInitialization);
    if (trieNeedsInitialization) {
        TextTrieMap *nonConstThis = const_cast<TextTrieMap *>(this);
        nonConstThis->buildTrie(status);
    }
    if (fNodes == NULL) {
        return;
    }
    search(fNodes, text, start, start, handler, status);
}

void
TextTrieMap::search(CharacterNode *node, const UnicodeString &text, int32_t start,
                  int32_t index, TextTrieMapSearchResultHandler *handler, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return;
    }
    if (node->hasValues()) {
        if (!handler->handleMatch(index - start, node, status)) {
            return;
        }
        if (U_FAILURE(status)) {
            return;
        }
    }
    UChar32 c = text.char32At(index);
    if (fIgnoreCase) {
        // size of character may grow after fold operation
        UnicodeString tmp(c);
        tmp.foldCase();
        int32_t tmpidx = 0;
        while (tmpidx < tmp.length()) {
            c = tmp.char32At(tmpidx);
            node = getChildNode(node, c);
            if (node == NULL) {
                break;
            }
            tmpidx = tmp.moveIndex32(tmpidx, 1);
        }
    } else {
        node = getChildNode(node, c);
    }
    if (node != NULL) {
        search(node, text, start, index+1, handler, status);
    }
}

// ---------------------------------------------------
// ZNStringPool class implementation
// ---------------------------------------------------
static const int32_t POOL_CHUNK_SIZE = 2000;
struct ZNStringPoolChunk: public UMemory {
    ZNStringPoolChunk    *fNext;                       // Ptr to next pool chunk
    int32_t               fLimit;                       // Index to start of unused area at end of fStrings
    UChar                 fStrings[POOL_CHUNK_SIZE];    //  Strings array
    ZNStringPoolChunk();
};

ZNStringPoolChunk::ZNStringPoolChunk() {
    fNext = NULL;
    fLimit = 0;
}

ZNStringPool::ZNStringPool(UErrorCode &status) {
    fChunks = NULL;
    fHash   = NULL;
    if (U_FAILURE(status)) {
        return;
    }
    fChunks = new ZNStringPoolChunk;
    if (fChunks == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    fHash   = uhash_open(uhash_hashUChars      /* keyHash */, 
                         uhash_compareUChars   /* keyComp */, 
                         uhash_compareUChars   /* valueComp */, 
                         &status);
    if (U_FAILURE(status)) {
        return;
    }
}

ZNStringPool::~ZNStringPool() {
    if (fHash != NULL) {
        uhash_close(fHash);
        fHash = NULL;
    }

    while (fChunks != NULL) {
        ZNStringPoolChunk *nextChunk = fChunks->fNext;
        delete fChunks;
        fChunks = nextChunk;
    }
}

static const UChar EmptyString = 0;

const UChar *ZNStringPool::get(const UChar *s, UErrorCode &status) {
    const UChar *pooledString;
    if (U_FAILURE(status)) {
        return &EmptyString;
    }

    pooledString = static_cast<UChar *>(uhash_get(fHash, s));
    if (pooledString != NULL) {
        return pooledString;
    }

    int32_t length = u_strlen(s);
    int32_t remainingLength = POOL_CHUNK_SIZE - fChunks->fLimit;
    if (remainingLength <= length) {
        U_ASSERT(length < POOL_CHUNK_SIZE);
        if (length >= POOL_CHUNK_SIZE) {
            status = U_INTERNAL_PROGRAM_ERROR;
            return &EmptyString;
        }
        ZNStringPoolChunk *oldChunk = fChunks;
        fChunks = new ZNStringPoolChunk;
        if (fChunks == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return &EmptyString;
        }
        fChunks->fNext = oldChunk;
    }
    
    UChar *destString = &fChunks->fStrings[fChunks->fLimit];
    u_strcpy(destString, s);
    fChunks->fLimit += (length + 1);
    uhash_put(fHash, destString, destString, &status);
    return destString;
}        


//
//  ZNStringPool::adopt()    Put a string into the hash, but do not copy the string data
//                           into the pool's storage.  Used for strings from resource bundles,
//                           which will perisist for the life of the zone string formatter, and
//                           therefore can be used directly without copying.
const UChar *ZNStringPool::adopt(const UChar * s, UErrorCode &status) {
    const UChar *pooledString;
    if (U_FAILURE(status)) {
        return &EmptyString;
    }
    if (s != NULL) {
        pooledString = static_cast<UChar *>(uhash_get(fHash, s));
        if (pooledString == NULL) {
            UChar *ncs = const_cast<UChar *>(s);
            uhash_put(fHash, ncs, ncs, &status);
        }
    }
    return s;
}

    
const UChar *ZNStringPool::get(const UnicodeString &s, UErrorCode &status) {
    UnicodeString &nonConstStr = const_cast<UnicodeString &>(s);
    return this->get(nonConstStr.getTerminatedBuffer(), status);
}

/*
 * freeze().   Close the hash table that maps to the pooled strings.
 *             After freezing, the pool can not be searched or added to,
 *             but all existing references to pooled strings remain valid.
 *
 *             The main purpose is to recover the storage used for the hash.
 */
void ZNStringPool::freeze() {
    uhash_close(fHash);
    fHash = NULL;
}


// ---------------------------------------------------
// ZNames - names common for time zone and meta zone
// ---------------------------------------------------
class ZNames : public UMemory {
public:
    virtual ~ZNames();

    static ZNames* createInstance(UResourceBundle* rb, const char* key);
    const UChar* getName(UTimeZoneNameType type);

protected:
    ZNames(const UChar** names);
    static const UChar** loadData(UResourceBundle* rb, const char* key);

private:
    const UChar** fNames;
};

ZNames::ZNames(const UChar** names)
: fNames(names) {
}

ZNames::~ZNames() {
    if (fNames != NULL) {
        uprv_free(fNames);
    }
}

ZNames*
ZNames::createInstance(UResourceBundle* rb, const char* key) {
    const UChar** names = loadData(rb, key);
    if (names == NULL) {
        // No names data available
        return NULL; 
    }
    return new ZNames(names);
}

const UChar*
ZNames::getName(UTimeZoneNameType type) {
    if (fNames == NULL) {
        return NULL;
    }
    const UChar *name = NULL;
    switch(type) {
    case UTZNM_LONG_GENERIC:
        name = fNames[0];
        break;
    case UTZNM_LONG_STANDARD:
        name = fNames[1];
        break;
    case UTZNM_LONG_DAYLIGHT:
        name = fNames[2];
        break;
    case UTZNM_SHORT_GENERIC:
        name = fNames[3];
        break;
    case UTZNM_SHORT_STANDARD:
        name = fNames[4];
        break;
    case UTZNM_SHORT_DAYLIGHT:
        name = fNames[5];
        break;
    default:
        name = NULL;
    }
    return name;
}

const UChar**
ZNames::loadData(UResourceBundle* rb, const char* key) {
    if (rb == NULL || key == NULL || *key == 0) {
        return NULL;
    }

    UErrorCode status = U_ZERO_ERROR;
    const UChar **names = NULL;

    UResourceBundle* rbTable = NULL;
    rbTable = ures_getByKeyWithFallback(rb, key, rbTable, &status);
    if (U_SUCCESS(status)) {
        names = (const UChar **)uprv_malloc(sizeof(const UChar*) * KEYS_SIZE);
        if (names != NULL) {
            UBool isEmpty = TRUE;
            for (int32_t i = 0; i < KEYS_SIZE; i++) {
                status = U_ZERO_ERROR;
                int32_t len = 0;
                const UChar *value = ures_getStringByKeyWithFallback(rbTable, KEYS[i], &len, &status);
                if (U_FAILURE(status) || len == 0) {
                    names[i] = NULL;
                } else {
                    names[i] = value;
                    isEmpty = FALSE;
                }
            }
            if (isEmpty) {
                // No need to keep the names array
                uprv_free(names);
                names = NULL;
            }
        }
    }
    ures_close(rbTable);
    return names;
}

// ---------------------------------------------------
// TZNames - names for a time zone
// ---------------------------------------------------
class TZNames : public ZNames {
public:
    virtual ~TZNames();

    static TZNames* createInstance(UResourceBundle* rb, const char* key);
    const UChar* getLocationName(void);

private:
    TZNames(const UChar** names, const UChar* locationName);
    const UChar* fLocationName;
};

TZNames::TZNames(const UChar** names, const UChar* locationName)
: ZNames(names), fLocationName(locationName) {
}

TZNames::~TZNames() {
}

const UChar*
TZNames::getLocationName() {
    return fLocationName;
}

TZNames*
TZNames::createInstance(UResourceBundle* rb, const char* key) {
    if (rb == NULL || key == NULL || *key == 0) {
        return NULL;
    }
    TZNames* tznames = NULL;
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle* rbTable = ures_getByKeyWithFallback(rb, key, NULL, &status);
    if (U_SUCCESS(status)) {
        int32_t len = 0;
        const UChar* locationName = ures_getStringByKeyWithFallback(rbTable, gEcTag, &len, &status);
        if (U_FAILURE(status) || len == 0) {
            locationName = NULL;
        }

        const UChar** names = loadData(rb, key);

        if (locationName != NULL || names != NULL) {
            tznames = new TZNames(names, locationName);
        }
    }
    ures_close(rbTable);
    return tznames;
}

// ---------------------------------------------------
// The meta zone ID enumeration class
// ---------------------------------------------------
class MetaZoneIDsEnumeration : public StringEnumeration {
public:
    MetaZoneIDsEnumeration();
    MetaZoneIDsEnumeration(const UVector& mzIDs);
    MetaZoneIDsEnumeration(UVector* mzIDs);
    virtual ~MetaZoneIDsEnumeration();
    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
    virtual const UnicodeString* snext(UErrorCode& status);
    virtual void reset(UErrorCode& status);
    virtual int32_t count(UErrorCode& status) const;
private:
    int32_t fLen;
    int32_t fPos;
    const UVector* fMetaZoneIDs;
    UVector *fLocalVector;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MetaZoneIDsEnumeration)

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration() 
: fLen(0), fPos(0), fMetaZoneIDs(NULL), fLocalVector(NULL) {
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration(const UVector& mzIDs) 
: fPos(0), fMetaZoneIDs(&mzIDs), fLocalVector(NULL) {
    fLen = fMetaZoneIDs->size();
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration(UVector *mzIDs)
: fLen(0), fPos(0), fMetaZoneIDs(mzIDs), fLocalVector(mzIDs) {
    if (fMetaZoneIDs) {
        fLen = fMetaZoneIDs->size();
    }
}

const UnicodeString*
MetaZoneIDsEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && fMetaZoneIDs != NULL && fPos < fLen) {
        unistr.setTo((const UChar*)fMetaZoneIDs->elementAt(fPos++), -1);
        return &unistr;
    }
    return NULL;
}

void
MetaZoneIDsEnumeration::reset(UErrorCode& /*status*/) {
    fPos = 0;
}

int32_t
MetaZoneIDsEnumeration::count(UErrorCode& /*status*/) const {
    return fLen;
}

MetaZoneIDsEnumeration::~MetaZoneIDsEnumeration() {
    if (fLocalVector) {
        delete fLocalVector;
    }
}

U_CDECL_BEGIN
/**
 * ZNameInfo stores zone name information in the trie
 */
typedef struct ZNameInfo {
    UTimeZoneNameType   type;
    const UChar*        tzID;
    const UChar*        mzID;
} ZNameInfo;

/**
 * ZMatchInfo stores zone name match information used by find method
 */
typedef struct ZMatchInfo {
    const ZNameInfo*    znameInfo;
    int32_t             matchLength;
} ZMatchInfo;
U_CDECL_END


// ---------------------------------------------------
// ZNameSearchHandler
// ---------------------------------------------------
class ZNameSearchHandler : public TextTrieMapSearchResultHandler {
public:
    ZNameSearchHandler(uint32_t types);
    virtual ~ZNameSearchHandler();

    UBool handleMatch(int32_t matchLength, const CharacterNode *node, UErrorCode &status);
    TimeZoneNames::MatchInfoCollection* getMatches(int32_t& maxMatchLen);

private:
    uint32_t fTypes;
    int32_t fMaxMatchLen;
    TimeZoneNames::MatchInfoCollection* fResults;
};

ZNameSearchHandler::ZNameSearchHandler(uint32_t types) 
: fTypes(types), fMaxMatchLen(0), fResults(NULL) {
}

ZNameSearchHandler::~ZNameSearchHandler() {
    if (fResults != NULL) {
        delete fResults;
    }
}

UBool
ZNameSearchHandler::handleMatch(int32_t matchLength, const CharacterNode *node, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (node->hasValues()) {
        int32_t valuesCount = node->countValues();
        for (int32_t i = 0; i < valuesCount; i++) {
            ZNameInfo *nameinfo = (ZNameInfo *)node->getValue(i);
            if (nameinfo == NULL) {
                break;
            }
            if ((nameinfo->type & fTypes) != 0) {
                // matches a requested type
                if (fResults == NULL) {
                    fResults = new TimeZoneNames::MatchInfoCollection();
                    if (fResults == NULL) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                    }
                }
                if (U_SUCCESS(status)) {
                    U_ASSERT(fResults != NULL);
                    if (nameinfo->tzID) {
                        fResults->addZone(nameinfo->type, matchLength, UnicodeString(nameinfo->tzID, -1), status);
                    } else {
                        U_ASSERT(nameinfo->mzID);
                        fResults->addMetaZone(nameinfo->type, matchLength, UnicodeString(nameinfo->mzID, -1), status);
                    }
                    if (U_SUCCESS(status) && matchLength > fMaxMatchLen) {
                        fMaxMatchLen = matchLength;
                    }
                }
            }
        }
    }
    return TRUE;
}

TimeZoneNames::MatchInfoCollection*
ZNameSearchHandler::getMatches(int32_t& maxMatchLen) {
    // give the ownership to the caller
    TimeZoneNames::MatchInfoCollection* results = fResults;
    maxMatchLen = fMaxMatchLen;

    // reset
    fResults = NULL;
    fMaxMatchLen = 0;
    return results;
}

// ---------------------------------------------------
// TimeZoneNamesImpl
//
// TimeZoneNames implementation class. This is the main
// part of this module.
// ---------------------------------------------------

U_CDECL_BEGIN
/**
 * Deleter for ZNames
 */
static void U_CALLCONV
deleteZNames(void *obj) {
    if (obj != EMPTY) {
        delete (ZNames *)obj;
    }
}
/**
 * Deleter for TZNames
 */
static void U_CALLCONV
deleteTZNames(void *obj) {
    if (obj != EMPTY) {
        delete (TZNames *)obj;
    }
}

/**
 * Deleter for ZNameInfo
 */
static void U_CALLCONV
deleteZNameInfo(void *obj) {
    uprv_free(obj);
}

U_CDECL_END

TimeZoneNamesImpl::TimeZoneNamesImpl(const Locale& locale, UErrorCode& status)
: fLocale(locale),
  fLock(NULL),
  fZoneStrings(NULL),
  fTZNamesMap(NULL),
  fMZNamesMap(NULL),
  fNamesTrieFullyLoaded(FALSE),
  fNamesTrie(TRUE, deleteZNameInfo) {
    initialize(locale, status);
}

void
TimeZoneNamesImpl::initialize(const Locale& locale, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    // Load zoneStrings bundle
    UErrorCode tmpsts = U_ZERO_ERROR;   // OK with fallback warning..
    fZoneStrings = ures_open(U_ICUDATA_ZONE, locale.getName(), &tmpsts);
    fZoneStrings = ures_getByKeyWithFallback(fZoneStrings, gZoneStrings, fZoneStrings, &tmpsts);
    if (U_FAILURE(tmpsts)) {
        status = tmpsts;
        cleanup();
        return;
    }

    // Initialize hashtables holding time zone/meta zone names
    fMZNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    fTZNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }

    uhash_setValueDeleter(fMZNamesMap, deleteZNames);
    uhash_setValueDeleter(fTZNamesMap, deleteTZNames);
    // no key deleters for name maps

    // preload zone strings for the default zone
    TimeZone *tz = TimeZone::createDefault();
    const UChar *tzID = ZoneMeta::getCanonicalCLDRID(*tz);
    if (tzID != NULL) {
        loadStrings(UnicodeString(tzID));
    }
    delete tz;

    return;
}

/*
 * This method updates the cache and must be called with a lock,
 * except initializer.
 */
void
TimeZoneNamesImpl::loadStrings(const UnicodeString& tzCanonicalID) {
    loadTimeZoneNames(tzCanonicalID);

    UErrorCode status = U_ZERO_ERROR;
    StringEnumeration *mzIDs = getAvailableMetaZoneIDs(tzCanonicalID, status);
    if (U_SUCCESS(status) && mzIDs != NULL) {
        const UnicodeString *mzID;
        while ((mzID = mzIDs->snext(status))) {
            if (U_FAILURE(status)) {
                break;
            }
            loadMetaZoneNames(*mzID);
        }
        delete mzIDs;
    }
}

TimeZoneNamesImpl::~TimeZoneNamesImpl() {
    cleanup();
    umtx_destroy(&fLock);
}

void
TimeZoneNamesImpl::cleanup() {
    if (fZoneStrings != NULL) {
        ures_close(fZoneStrings);
        fZoneStrings = NULL;
    }
    if (fMZNamesMap != NULL) {
        uhash_close(fMZNamesMap);
        fMZNamesMap = NULL;
    }
    if (fTZNamesMap != NULL) {
        uhash_close(fTZNamesMap);
        fTZNamesMap = NULL;
    }
}

UBool
TimeZoneNamesImpl::operator==(const TimeZoneNames& other) const {
    if (this == &other) {
        return TRUE;
    }
    // No implementation for now
    return FALSE;
}

TimeZoneNames*
TimeZoneNamesImpl::clone() const {
    UErrorCode status = U_ZERO_ERROR;
    return new TimeZoneNamesImpl(fLocale, status);
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const UVector* mzIDs = ZoneMeta::getAvailableMetazoneIDs();
    if (mzIDs == NULL) {
        return new MetaZoneIDsEnumeration();
    }
    return new MetaZoneIDsEnumeration(*mzIDs);
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const UVector* mappings = ZoneMeta::getMetazoneMappings(tzID);
    if (mappings == NULL) {
        return new MetaZoneIDsEnumeration();
    }

    MetaZoneIDsEnumeration *senum = NULL;
    UVector* mzIDs = new UVector(NULL, uhash_compareUChars, status);
    if (mzIDs == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_SUCCESS(status)) {
        U_ASSERT(mzIDs != NULL);
        for (int32_t i = 0; U_SUCCESS(status) && i < mappings->size(); i++) {

            OlsonToMetaMappingEntry *map = (OlsonToMetaMappingEntry *)mappings->elementAt(i);
            const UChar *mzID = map->mzid;
            if (!mzIDs->contains((void *)mzID)) {
                mzIDs->addElement((void *)mzID, status);
            }
        }
        if (U_SUCCESS(status)) {
            senum = new MetaZoneIDsEnumeration(mzIDs);
        } else {
            delete mzIDs;
        }
    }
    return senum;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    ZoneMeta::getMetazoneID(tzID, date, mzID);
    return mzID;
}

UnicodeString&
TimeZoneNamesImpl::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    ZoneMeta::getZoneIdByMetazone(mzID, UnicodeString(region, -1, US_INV), tzID);
    return tzID;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneDisplayName(const UnicodeString& mzID,
                                          UTimeZoneNameType type,
                                          UnicodeString& name) const {
    name.setToBogus();  // cleanup result.
    if (mzID.isEmpty()) {
        return name;
    }

    ZNames *znames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    umtx_lock(&nonConstThis->fLock);
    {
        znames = nonConstThis->loadMetaZoneNames(mzID);
    }
    umtx_unlock(&nonConstThis->fLock);

    if (znames != NULL) {
        const UChar* s = znames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    name.setToBogus();  // cleanup result.
    if (tzID.isEmpty()) {
        return name;
    }

    TZNames *tznames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    umtx_lock(&nonConstThis->fLock);
    {
        tznames = nonConstThis->loadTimeZoneNames(tzID);
    }
    umtx_unlock(&nonConstThis->fLock);

    if (tznames != NULL) {
        const UChar *s = tznames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    const UChar* locName = NULL;
    TZNames *tznames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    umtx_lock(&nonConstThis->fLock);
    {
        tznames = nonConstThis->loadTimeZoneNames(tzID);
    }
    umtx_unlock(&nonConstThis->fLock);

    if (tznames != NULL) {
        locName = tznames->getLocationName();
    }
    if (locName != NULL) {
        name.setTo(TRUE, locName, -1);
        return name;
    }

    return TimeZoneNames::getExemplarLocationName(tzID, name);
}


// Merge the MZ_PREFIX and mzId
static void mergeTimeZoneKey(const UnicodeString& mzID, char* result) {
    if (mzID.isEmpty()) {
        result[0] = '\0';
        return;
    }

    char mzIdChar[ZID_KEY_MAX + 1];
    int32_t keyLen;
    int32_t prefixLen = uprv_strlen(gMZPrefix);
    keyLen = mzID.extract(0, mzID.length(), mzIdChar, ZID_KEY_MAX + 1, US_INV);
    uprv_memcpy((void *)result, (void *)gMZPrefix, prefixLen);
    uprv_memcpy((void *)(result + prefixLen), (void *)mzIdChar, keyLen);
    result[keyLen + prefixLen] = '\0';
}

/*
 * This method updates the cache and must be called with a lock
 */
ZNames*
TimeZoneNamesImpl::loadMetaZoneNames(const UnicodeString& mzID) {
    if (mzID.length() > (ZID_KEY_MAX - MZ_PREFIX_LEN)) {
        return NULL;
    }

    ZNames *znames = NULL;

    UErrorCode status = U_ZERO_ERROR;
    UChar mzIDKey[ZID_KEY_MAX + 1];
    mzID.extract(mzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(status == U_ZERO_ERROR);   // already checked length above
    mzIDKey[mzID.length()] = 0;

    void *cacheVal = uhash_get(fMZNamesMap, mzIDKey);
    if (cacheVal == NULL) {
        char key[ZID_KEY_MAX + 1];
        mergeTimeZoneKey(mzID, key);
        znames = ZNames::createInstance(fZoneStrings, key);

        if (znames == NULL) {
            cacheVal = (void *)EMPTY;
        } else {
            cacheVal = znames;
        }
        // Use the persistent ID as the resource key, so we can
        // avoid duplications.
        const UChar* newKey = ZoneMeta::findMetaZoneID(mzID);
        if (newKey != NULL) {
            uhash_put(fMZNamesMap, (void *)newKey, cacheVal, &status);
            if (U_FAILURE(status)) {
                if (znames != NULL) {
                    delete znames;
                }
            } else if (znames != NULL) {
                // put the name info into the trie
                for (int32_t i = 0; ALL_NAME_TYPES[i] != UTZNM_UNKNOWN; i++) {
                    const UChar* name = znames->getName(ALL_NAME_TYPES[i]);
                    if (name != NULL) {
                        ZNameInfo *nameinfo = (ZNameInfo *)uprv_malloc(sizeof(ZNameInfo));
                        if (nameinfo != NULL) {
                            nameinfo->type = ALL_NAME_TYPES[i];
                            nameinfo->tzID = NULL;
                            nameinfo->mzID = newKey;
                            fNamesTrie.put(name, nameinfo, status);
                        }
                    }
                }
            }

        } else {
            // Should never happen with a valid input
            if (znames != NULL) {
                // It's not possible that we get a valid ZNames with unknown ID.
                // But just in case..
                delete znames;
                znames = NULL;
            }
        }
    } else if (cacheVal != EMPTY) {
        znames = (ZNames *)cacheVal;
    }

    return znames;
}

/*
 * This method updates the cache and must be called with a lock
 */
TZNames*
TimeZoneNamesImpl::loadTimeZoneNames(const UnicodeString& tzID) {
    if (tzID.length() > ZID_KEY_MAX) {
        return NULL;
    }

    TZNames *tznames = NULL;

    UErrorCode status = U_ZERO_ERROR;
    UChar tzIDKey[ZID_KEY_MAX + 1];
    int32_t tzIDKeyLen = tzID.extract(tzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(status == U_ZERO_ERROR);   // already checked length above
    tzIDKey[tzIDKeyLen] = 0;

    void *cacheVal = uhash_get(fTZNamesMap, tzIDKey);
    if (cacheVal == NULL) {
        char key[ZID_KEY_MAX + 1];
        UErrorCode status = U_ZERO_ERROR;
        // Replace "/" with ":".
        UnicodeString uKey(tzID);
        for (int32_t i = 0; i < uKey.length(); i++) {
            if (uKey.charAt(i) == (UChar)0x2F) {
                uKey.setCharAt(i, (UChar)0x3A);
            }
        }
        uKey.extract(0, uKey.length(), key, sizeof(key), US_INV);
        tznames = TZNames::createInstance(fZoneStrings, key);

        if (tznames == NULL) {
            cacheVal = (void *)EMPTY;
        } else {
            cacheVal = tznames;
        }
        // Use the persistent ID as the resource key, so we can
        // avoid duplications.
        const UChar* newKey = ZoneMeta::findTimeZoneID(tzID);
        if (newKey != NULL) {
            uhash_put(fTZNamesMap, (void *)newKey, cacheVal, &status);
            if (U_FAILURE(status)) {
                if (tznames != NULL) {
                    delete tznames;
                }
            } else if (tznames != NULL) {
                // put the name info into the trie
                for (int32_t i = 0; ALL_NAME_TYPES[i] != UTZNM_UNKNOWN; i++) {
                    const UChar* name = tznames->getName(ALL_NAME_TYPES[i]);
                    if (name != NULL) {
                        ZNameInfo *nameinfo = (ZNameInfo *)uprv_malloc(sizeof(ZNameInfo));
                        if (nameinfo != NULL) {
                            nameinfo->type = ALL_NAME_TYPES[i];
                            nameinfo->tzID = newKey;
                            nameinfo->mzID = NULL;
                            fNamesTrie.put(name, nameinfo, status);
                        }
                    }
                }
            }
        } else {
            // Should never happen with a valid input
            if (tznames != NULL) {
                // It's not possible that we get a valid TZNames with unknown ID.
                // But just in case..
                delete tznames;
                tznames = NULL;
            }
        }
    } else if (cacheVal != EMPTY) {
        tznames = (TZNames *)cacheVal;
    }

    return tznames;
}

TimeZoneNames::MatchInfoCollection*
TimeZoneNamesImpl::find(const UnicodeString& text, int32_t start, uint32_t types, UErrorCode& status) const {
    ZNameSearchHandler handler(types);

    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    umtx_lock(&nonConstThis->fLock);
    {
        fNamesTrie.search(text, start, (TextTrieMapSearchResultHandler *)&handler, status);
    }
    umtx_unlock(&nonConstThis->fLock);

    if (U_FAILURE(status)) {
        return NULL;
    }

    int32_t maxLen = 0;
    TimeZoneNames::MatchInfoCollection* matches = handler.getMatches(maxLen);
    if (matches != NULL && ((maxLen == (text.length() - start)) || fNamesTrieFullyLoaded)) {
        // perfect match
        return matches;
    }

    delete matches;

    // All names are not yet loaded into the trie
    umtx_lock(&nonConstThis->fLock);
    {
        if (!fNamesTrieFullyLoaded) {
            const UnicodeString *id;

            // load strings for all zones
            StringEnumeration *tzIDs = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, NULL, NULL, status);
            if (U_SUCCESS(status)) {
                while ((id = tzIDs->snext(status))) {
                    if (U_FAILURE(status)) {
                        break;
                    }
                    // loadStrings also load related metazone strings
                    nonConstThis->loadStrings(*id);
                }
            }
            if (tzIDs != NULL) {
                delete tzIDs;
            }
            if (U_SUCCESS(status)) {
                nonConstThis->fNamesTrieFullyLoaded = TRUE;
            }
        }
    }
    umtx_unlock(&nonConstThis->fLock);

    if (U_FAILURE(status)) {
        return NULL;
    }

    umtx_lock(&nonConstThis->fLock);
    {
        // now try it again
        fNamesTrie.search(text, start, (TextTrieMapSearchResultHandler *)&handler, status);
    }
    umtx_unlock(&nonConstThis->fLock);

    return handler.getMatches(maxLen);
}

U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
