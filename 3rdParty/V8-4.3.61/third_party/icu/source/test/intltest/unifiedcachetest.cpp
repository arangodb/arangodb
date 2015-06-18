/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
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
        const void * /*unused*/, UErrorCode &status) const {
    if (uprv_strcmp(fLoc.getName(), "zh") == 0) {
        status = U_MISSING_RESOURCE_ERROR;
        return NULL;
    }
    if (uprv_strcmp(fLoc.getLanguage(), fLoc.getName()) != 0) {
        const UCTItem *item = NULL;
        UnifiedCache::getByLocale(fLoc.getLanguage(), item, status);
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
    void TestBasic();
    void TestError();
    void TestHashEquals();
};

void UnifiedCacheTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestBasic);
  TESTCASE_AUTO(TestError);
  TESTCASE_AUTO(TestHashEquals);
  TESTCASE_AUTO_END;
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
    assertSuccess("", status);
    // en_US, en_GB, en share one object; fr_FR and fr don't share.
    // 5 keys in all.
    assertEquals("", baseCount + 5, cache->keyCount());
    SharedObject::clearPtr(enGb);
    cache->flush();
    assertEquals("", baseCount + 5, cache->keyCount());
    SharedObject::clearPtr(enUs);
    SharedObject::clearPtr(en);
    cache->flush();
    // With en_GB and en_US and en cleared there are no more hard references to
    // the "en" object, so it gets flushed and the keys that refer to it
    // get removed from the cache.
    assertEquals("", baseCount + 2, cache->keyCount());
    SharedObject::clearPtr(fr);
    cache->flush();
    assertEquals("", baseCount + 2, cache->keyCount());
    SharedObject::clearPtr(frFr);
    cache->flush();
    assertEquals("", baseCount + 0, cache->keyCount());
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
