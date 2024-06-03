// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File UNIFIEDCACHETEST.CPP
*
********************************************************************************
*/
#include "cstring.h"
#include "intltest.h"
#include "unifiedcache.h"
#include "unicode/datefmt.h"

class UCTItem : public SharedObject {
  public:
    char *value;
    UCTItem(const char *x) : value(NULL) { 
        value = uprv_strdup(x);
    }
    virtual ~UCTItem() {
        uprv_free(value);
    }
};

class UCTItem2 : public SharedObject {
};

U_NAMESPACE_BEGIN

template<> U_EXPORT
const UCTItem *LocaleCacheKey<UCTItem>::createObject(
        const void *context, UErrorCode &status) const {
    const UnifiedCache *cacheContext = (const UnifiedCache *) context;
    if (uprv_strcmp(fLoc.getName(), "zh") == 0) {
        status = U_MISSING_RESOURCE_ERROR;
        return NULL;
    }
    if (uprv_strcmp(fLoc.getLanguage(), fLoc.getName()) != 0) {
        const UCTItem *item = NULL;
        if (cacheContext == NULL) {
            UnifiedCache::getByLocale(fLoc.getLanguage(), item, status);
        } else {
            cacheContext->get(LocaleCacheKey<UCTItem>(fLoc.getLanguage()), item, status);
        }
        if (U_FAILURE(status)) {
            return NULL;
        }
        return item;
    }
    UCTItem *result = new UCTItem(fLoc.getName());
    result->addRef();
    return result;
}

template<> U_EXPORT
const UCTItem2 *LocaleCacheKey<UCTItem2>::createObject(
        const void * /*unused*/, UErrorCode & /*status*/) const {
    return NULL;
}

U_NAMESPACE_END


class UnifiedCacheTest : public IntlTest {
public:
    UnifiedCacheTest() {
    }
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestEvictionPolicy();
    void TestBounded();
    void TestBasic();
    void TestError();
    void TestHashEquals();
    void TestEvictionUnderStress();
};

void UnifiedCacheTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestEvictionPolicy);
  TESTCASE_AUTO(TestBounded);
  TESTCASE_AUTO(TestBasic);
  TESTCASE_AUTO(TestError);
  TESTCASE_AUTO(TestHashEquals);
  TESTCASE_AUTO(TestEvictionUnderStress);
  TESTCASE_AUTO_END;
}

void UnifiedCacheTest::TestEvictionUnderStress() {
#if !UCONFIG_NO_FORMATTING
    int32_t localeCount;
    const Locale *locales = DateFormat::getAvailableLocales(localeCount);
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = UnifiedCache::getInstance(status);
    int64_t evictedCountBefore = cache->autoEvictedCount();
    for (int32_t i = 0; i < localeCount; ++i) {
        LocalPointer<DateFormat> ptr(DateFormat::createInstanceForSkeleton("yMd", locales[i], status));
    }
    int64_t evictedCountAfter = cache->autoEvictedCount();
    if (evictedCountBefore == evictedCountAfter) {
        dataerrln("%s:%d Items should have been evicted from cache",
               __FILE__, __LINE__);
    }
#endif /* #if !UCONFIG_NO_FORMATTING */
}

void UnifiedCacheTest::TestEvictionPolicy() {
    UErrorCode status = U_ZERO_ERROR;

    // We have to call this first or else calling the UnifiedCache
    // ctor will fail. This is by design to deter clients from using the 
    // cache API incorrectly by creating their own cache instances.
    UnifiedCache::getInstance(status);

    // We create our own local UnifiedCache instance to ensure we have
    // complete control over it. Real clients should never ever create
    // their own cache!
    UnifiedCache cache(status);
    assertSuccess("", status);

    // Don't allow unused entries to exeed more than 100% of in use entries.
    cache.setEvictionPolicy(0, 100, status);

    static const char *locales[] = {
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
            "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"};

    const UCTItem *usedReferences[] = {NULL, NULL, NULL, NULL, NULL};
    const UCTItem *unusedReference = NULL;

    // Add 5 in-use entries
    for (int32_t i = 0; i < UPRV_LENGTHOF(usedReferences); i++) {
        cache.get(
                LocaleCacheKey<UCTItem>(locales[i]),
                &cache,
                usedReferences[i],
                status);
    }

    // Add 10 not in use entries.
    for (int32_t i = 0; i < 10; ++i) {
        cache.get(
                LocaleCacheKey<UCTItem>(
                        locales[i + UPRV_LENGTHOF(usedReferences)]),
                &cache,
                unusedReference,
                status);
    }
    unusedReference->removeRef();

    // unused count not to exeed in use count
    assertEquals("T1", UPRV_LENGTHOF(usedReferences), cache.unusedCount());
    assertEquals("T2", 2*UPRV_LENGTHOF(usedReferences), cache.keyCount());

    // Free up those used entries.
    for (int32_t i = 0; i < UPRV_LENGTHOF(usedReferences); i++) {
        usedReferences[i]->removeRef();
    }

    // This should free up all cache items
    assertEquals("T3", 0, cache.keyCount());

    assertSuccess("T4", status);
}



void UnifiedCacheTest::TestBounded() {
    UErrorCode status = U_ZERO_ERROR;

    // We have to call this first or else calling the UnifiedCache
    // ctor will fail. This is by design to deter clients from using the
    // cache API incorrectly by creating their own cache instances.
    UnifiedCache::getInstance(status);

    // We create our own local UnifiedCache instance to ensure we have
    // complete control over it. Real clients should never ever create
    // their own cache!
    UnifiedCache cache(status);
    assertSuccess("T0", status);

    // Maximum unused count is 3.
    cache.setEvictionPolicy(3, 0, status);

    // Our cache will hold up to 3 unused key-value pairs
    // We test the following invariants:
    // 1. unusedCount <= 3
    // 2. cache->get(X) always returns the same reference as long as caller
    //   already holds references to that same object. 

    // We first add 5 key-value pairs with two distinct values, "en" and "fr"
    // keeping all those references.

    const UCTItem *en = NULL;
    const UCTItem *enGb = NULL;
    const UCTItem *enUs = NULL;
    const UCTItem *fr = NULL;
    const UCTItem *frFr = NULL;
    cache.get(LocaleCacheKey<UCTItem>("en_US"), &cache, enUs, status);
    cache.get(LocaleCacheKey<UCTItem>("en"), &cache, en, status);
    assertEquals("T1", 1, cache.unusedCount());
    cache.get(LocaleCacheKey<UCTItem>("en_GB"), &cache, enGb, status);
    cache.get(LocaleCacheKey<UCTItem>("fr_FR"), &cache, frFr, status);
    cache.get(LocaleCacheKey<UCTItem>("fr"), &cache, fr, status);

    // Client holds two unique references, "en" and "fr" the other three
    // entries are eligible for eviction. 
    assertEquals("T2", 3, cache.unusedCount());
    assertEquals("T3", 5, cache.keyCount());

    // Exercise cache more but don't hold the references except for
    // the last one. At the end of this, we will hold references to one
    // additional distinct value, so we will have references to 3 distinct
    // values.
    const UCTItem *throwAway = NULL;
    cache.get(LocaleCacheKey<UCTItem>("zn_AA"), &cache, throwAway, status);
    cache.get(LocaleCacheKey<UCTItem>("sr_AA"), &cache, throwAway, status);
    cache.get(LocaleCacheKey<UCTItem>("de_AU"), &cache, throwAway, status);

    const UCTItem *deAu(throwAway);
    deAu->addRef();

    // Client holds three unique references, "en", "fr", "de" although we
    // could have a total of 8 entries in the cache maxUnusedCount == 3
    // so we have only 6 entries.
    assertEquals("T4", 3, cache.unusedCount());
    assertEquals("T5", 6, cache.keyCount());

    // For all the references we have, cache must continue to return
    // those same references (#2)

    cache.get(LocaleCacheKey<UCTItem>("en"), &cache, throwAway, status);
    if (throwAway != en) {
        errln("T6: Expected en to resolve to the same object.");
    }
    cache.get(LocaleCacheKey<UCTItem>("en_US"), &cache, throwAway, status);
    if (throwAway != enUs) {
        errln("T7: Expected enUs to resolve to the same object.");
    }
    cache.get(LocaleCacheKey<UCTItem>("en_GB"), &cache, throwAway, status);
    if (throwAway != enGb) {
        errln("T8: Expected enGb to resolve to the same object.");
    }
    cache.get(LocaleCacheKey<UCTItem>("fr_FR"), &cache, throwAway, status);
    if (throwAway != frFr) {
        errln("T9: Expected frFr to resolve to the same object.");
    }
    cache.get(LocaleCacheKey<UCTItem>("fr_FR"), &cache, throwAway, status);
    cache.get(LocaleCacheKey<UCTItem>("fr"), &cache, throwAway, status);
    if (throwAway != fr) {
        errln("T10: Expected fr to resolve to the same object.");
    }
    cache.get(LocaleCacheKey<UCTItem>("de_AU"), &cache, throwAway, status);
    if (throwAway != deAu) {
        errln("T11: Expected deAu to resolve to the same object.");
    }

    assertEquals("T12", 3, cache.unusedCount());
    assertEquals("T13", 6, cache.keyCount());

    // Now we hold a references to two more distinct values. Cache size 
    // should grow to 8.
    const UCTItem *es = NULL;
    const UCTItem *ru = NULL;
    cache.get(LocaleCacheKey<UCTItem>("es"), &cache, es, status);
    cache.get(LocaleCacheKey<UCTItem>("ru"), &cache, ru, status);
    assertEquals("T14", 3, cache.unusedCount());
    assertEquals("T15", 8, cache.keyCount());

    // Now release all the references we hold except for
    // es, ru, and en
    SharedObject::clearPtr(enGb);
    SharedObject::clearPtr(enUs);
    SharedObject::clearPtr(fr);
    SharedObject::clearPtr(frFr);
    SharedObject::clearPtr(deAu);
    SharedObject::clearPtr(es);
    SharedObject::clearPtr(ru);
    SharedObject::clearPtr(en);
    SharedObject::clearPtr(throwAway);

    // Size of cache should magically drop to 3.
    assertEquals("T16", 3, cache.unusedCount());
    assertEquals("T17", 3, cache.keyCount());

    // Be sure nothing happens setting the eviction policy in the middle of
    // a run.
    cache.setEvictionPolicy(3, 0, status);
    assertSuccess("T18", status);
    
}

void UnifiedCacheTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = UnifiedCache::getInstance(status);
    assertSuccess("", status);
    cache->flush();
    int32_t baseCount = cache->keyCount();
    const UCTItem *en = NULL;
    const UCTItem *enGb = NULL;
    const UCTItem *enGb2 = NULL;
    const UCTItem *enUs = NULL;
    const UCTItem *fr = NULL;
    const UCTItem *frFr = NULL;
    cache->get(LocaleCacheKey<UCTItem>("en"), en, status);
    cache->get(LocaleCacheKey<UCTItem>("en_US"), enUs, status);
    cache->get(LocaleCacheKey<UCTItem>("en_GB"), enGb, status);
    cache->get(LocaleCacheKey<UCTItem>("fr_FR"), frFr, status);
    cache->get(LocaleCacheKey<UCTItem>("fr"), fr, status);
    cache->get(LocaleCacheKey<UCTItem>("en_GB"), enGb2, status); 
    SharedObject::clearPtr(enGb2);
    if (enGb != enUs) {
        errln("Expected en_GB and en_US to resolve to same object.");
    } 
    if (fr != frFr) {
        errln("Expected fr and fr_FR to resolve to same object.");
    } 
    if (enGb == fr) {
        errln("Expected en_GB and fr to return different objects.");
    }
    assertSuccess("T1", status);
    // en_US, en_GB, en share one object; fr_FR and fr don't share.
    // 5 keys in all.
    assertEquals("T2", baseCount + 5, cache->keyCount());
    SharedObject::clearPtr(enGb);
    cache->flush();

    // Only 2 unique values in the cache. flushing trims cache down
    // to this minimum size.
    assertEquals("T3", baseCount + 2, cache->keyCount());
    SharedObject::clearPtr(enUs);
    SharedObject::clearPtr(en);
    cache->flush();
    // With en_GB and en_US and en cleared there are no more hard references to
    // the "en" object, so it gets flushed and the keys that refer to it
    // get removed from the cache. Now we have just one unique value, fr, in
    // the cache
    assertEquals("T4", baseCount + 1, cache->keyCount());
    SharedObject::clearPtr(fr);
    cache->flush();
    assertEquals("T5", baseCount + 1, cache->keyCount());
    SharedObject::clearPtr(frFr);
    cache->flush();
    assertEquals("T6", baseCount + 0, cache->keyCount());
    assertSuccess("T7", status);
}

void UnifiedCacheTest::TestError() {
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = UnifiedCache::getInstance(status);
    assertSuccess("", status);
    cache->flush();
    int32_t baseCount = cache->keyCount();
    const UCTItem *zh = NULL;
    const UCTItem *zhTw = NULL;
    const UCTItem *zhHk = NULL;

    status = U_ZERO_ERROR;
    cache->get(LocaleCacheKey<UCTItem>("zh"), zh, status);
    if (status != U_MISSING_RESOURCE_ERROR) {
        errln("Expected U_MISSING_RESOURCE_ERROR");
    }
    status = U_ZERO_ERROR;
    cache->get(LocaleCacheKey<UCTItem>("zh_TW"), zhTw, status);
    if (status != U_MISSING_RESOURCE_ERROR) {
        errln("Expected U_MISSING_RESOURCE_ERROR");
    }
    status = U_ZERO_ERROR;
    cache->get(LocaleCacheKey<UCTItem>("zh_HK"), zhHk, status);
    if (status != U_MISSING_RESOURCE_ERROR) {
        errln("Expected U_MISSING_RESOURCE_ERROR");
    }
    // 3 keys in cache zh, zhTW, zhHk all pointing to error placeholders
    assertEquals("", baseCount + 3, cache->keyCount());
    cache->flush();
    // error placeholders have no hard references so they always get flushed. 
    assertEquals("", baseCount + 0, cache->keyCount());
}

void UnifiedCacheTest::TestHashEquals() {
    LocaleCacheKey<UCTItem> key1("en_US");
    LocaleCacheKey<UCTItem> key2("en_US");
    LocaleCacheKey<UCTItem> diffKey1("en_UT");
    LocaleCacheKey<UCTItem2> diffKey2("en_US");
    assertTrue("", key1.hashCode() == key2.hashCode());
    assertTrue("", key1.hashCode() != diffKey1.hashCode());
    assertTrue("", key1.hashCode() != diffKey2.hashCode());
    assertTrue("", diffKey1.hashCode() != diffKey2.hashCode());
    assertTrue("", key1 == key2);
    assertTrue("", key1 != diffKey1);
    assertTrue("", key1 != diffKey2);
    assertTrue("", diffKey1 != diffKey2);
}

extern IntlTest *createUnifiedCacheTest() {
    return new UnifiedCacheTest();
}
