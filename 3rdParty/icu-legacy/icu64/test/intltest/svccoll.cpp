// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2003-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "svccoll.h"
#include "unicode/coll.h"
#include "unicode/strenum.h"
#include "cmemory.h"
#include "hash.h"
#include "uassert.h"

#include "cstring.h" // internal api used to compare locale strings

void CollationServiceTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par */)
{
    if (exec) logln("TestSuite CollationServiceTest: ");
    switch (index) {
        TESTCASE(0, TestRegister);
        TESTCASE(1, TestRegisterFactory);
        TESTCASE(2, TestSeparateTree);
    default: name = ""; break;
    }
}

void CollationServiceTest::TestRegister()
{
#if !UCONFIG_NO_SERVICE
    // register a singleton
    const Locale& FR = Locale::getFrance();
    const Locale& US = Locale::getUS();
    const Locale US_FOO("en", "US", "FOO");

    UErrorCode status = U_ZERO_ERROR;

    Collator* frcol = Collator::createInstance(FR, status);
    Collator* uscol = Collator::createInstance(US, status);
    if(U_FAILURE(status)) {
        errcheckln(status, "Failed to create collators with %s", u_errorName(status));
        delete frcol;
        delete uscol;
        return;
    }

    { // try override en_US collator
        Collator *clone = frcol->clone();
        URegistryKey key = Collator::registerInstance(frcol, US, status);
        // frcol has been adopted. We must not use it any more, nor rely on its attributes.
        frcol = nullptr;

        Collator* ncol = Collator::createInstance(US_FOO, status);
        if (*clone != *ncol) {
            errln("register of french collator for en_US failed on request for en_US_FOO");
        }
        delete clone;

        // The requested locale may be the same as the valid locale,
        // or may not be supported at all. See ticket #10477.
        Locale loc = ncol->getLocale(ULOC_REQUESTED_LOCALE, status);
        if (U_SUCCESS(status) && loc != US_FOO && loc != US) {
            errln(UnicodeString("requested locale for en_US_FOO is not en_US_FOO nor en_US but ") + loc.getName());
        }
        status = U_ZERO_ERROR;
        loc = ncol->getLocale(ULOC_VALID_LOCALE, status);
        if (loc != US) {
            errln(UnicodeString("valid locale for en_US_FOO is not en_US but ") + loc.getName());
        }
        loc = ncol->getLocale(ULOC_ACTUAL_LOCALE, status);
        if (loc != US) {
            errln(UnicodeString("actual locale for en_US_FOO is not en_US but ") + loc.getName());
        }
        delete ncol; ncol = nullptr;

        if (!Collator::unregister(key, status)) {
            errln("failed to unregister french collator");
        }

        ncol = Collator::createInstance(US, status);
        if (*uscol != *ncol) {
            errln("collator after unregister does not match original");
        }
        delete ncol; ncol = nullptr;
    }

    // recreate frcol
    frcol = Collator::createInstance(FR, status);

    LocalUCollatorPointer frFR(ucol_open("fr_FR", &status));

    { // try create collator for new locale
        Locale fu_FU_FOO("fu", "FU", "FOO");
        Locale fu_FU("fu", "FU", "");

        Collator* fucol = Collator::createInstance(fu_FU, status);
        Collator *clone = frcol->clone();
        URegistryKey key = Collator::registerInstance(frcol, fu_FU, status);
        frcol = nullptr;  // frcol has been adopted.
        Collator* ncol = Collator::createInstance(fu_FU_FOO, status);
        if (*clone != *ncol) {
            errln("register of fr collator for fu_FU failed");
        }
        delete clone;

        UnicodeString locName = fu_FU.getName();
        StringEnumeration* localeEnum = Collator::getAvailableLocales();
        UBool found = false;
        const UnicodeString* locStr, *ls2;
        for (locStr = localeEnum->snext(status);
        !found && locStr != nullptr;
        locStr = localeEnum->snext(status)) {
            //
            if (locName == *locStr) {
                found = true;
            }
        }

        StringEnumeration *le2 = nullptr;
        localeEnum->reset(status);
        int32_t i, count;
        count = localeEnum->count(status);
        for(i = 0; i < count; ++i) {
            if(i == count / 2) {
                le2 = localeEnum->clone();
                if(le2 == nullptr || count != le2->count(status)) {
                    errln("ServiceEnumeration.clone() failed");
                    break;
                }
            }
            if(i >= count / 2) {
                locStr = localeEnum->snext(status);
                ls2 = le2->snext(status);
                if(*locStr != *ls2) {
                    errln("ServiceEnumeration.clone() failed for item %d", i);
                }
            } else {
                localeEnum->snext(status);
            }
        }

        delete localeEnum;
        delete le2;

        if (!found) {
            errln("new locale fu_FU not reported as supported locale");
        }

        UnicodeString displayName;
        Collator::getDisplayName(fu_FU, displayName);
        /* The locale display pattern for the locale ja, ko, and zh are different. */
        const char16_t zh_fuFU_Array[] = { 0x0066, 0x0075, 0xff08, 0x0046, 0x0055, 0xff09, 0 };
        const UnicodeString zh_fuFU(zh_fuFU_Array);
        const Locale& defaultLocale = Locale::getDefault();
        if (displayName != "fu (FU)" &&
           ((defaultLocale == Locale::getKorean() && defaultLocale == Locale::getJapanese()) && displayName == "fu(FU)") &&
           ((defaultLocale == Locale::getChinese()) && displayName != zh_fuFU)) {
            errln(UnicodeString("found ") + displayName + " for fu_FU");
        }

        Collator::getDisplayName(fu_FU, fu_FU, displayName);
        if (displayName != "fu (FU)" &&
           ((defaultLocale == Locale::getKorean() && defaultLocale == Locale::getJapanese()) && displayName == "fu(FU)") &&
           ((defaultLocale == Locale::getChinese()) && displayName != zh_fuFU)) {
            errln(UnicodeString("found ") + displayName + " for fu_FU");
        }

        // test ucol_open
        LocalUCollatorPointer fufu(ucol_open("fu_FU_FOO", &status));
        if (fufu.isNull()) {
            errln("could not open fu_FU_FOO with ucol_open");
        } else {
            if (*Collator::fromUCollator(fufu.getAlias()) !=
                    *Collator::fromUCollator(frFR.getAlias())) {
                errln("collator fufu != collator frFR");
            }
        }

        if (!Collator::unregister(key, status)) {
            errln("failed to unregister french collator");
        }
        // !!! note frcoll invalid again, but we're no longer using it

        // other collators should still work ok
        Locale nloc = ncol->getLocale(ULOC_VALID_LOCALE, status);
        if (nloc != fu_FU) {
            errln(UnicodeString("asked for nloc valid locale after close and got") + nloc.getName());
        }
        delete ncol; ncol = nullptr;

        if (fufu.isValid()) {
            const char* nlocstr = ucol_getLocaleByType(fufu.getAlias(), ULOC_VALID_LOCALE, &status);
            if (uprv_strcmp(nlocstr, "fu_FU") != 0) {
                errln(UnicodeString("asked for uloc valid locale after close and got ") + nlocstr);
            }
        }

        ncol = Collator::createInstance(fu_FU, status);
        if (*fucol != *ncol) {
            errln("collator after unregister does not match original fu_FU");
        }
        delete uscol; uscol = nullptr;
        delete ncol; ncol = nullptr;
        delete fucol; fucol = nullptr;
    }
#endif
}

// ------------------

#if !UCONFIG_NO_SERVICE
struct CollatorInfo {
  Locale locale;
  Collator* collator;
  Hashtable* displayNames; // locale name -> string

  CollatorInfo(const Locale& locale, Collator* collatorToAdopt, Hashtable* displayNamesToAdopt);
  ~CollatorInfo();
  UnicodeString& getDisplayName(const Locale& displayLocale, UnicodeString& name) const;
};

CollatorInfo::CollatorInfo(const Locale& _locale, Collator* _collator, Hashtable* _displayNames)
  : locale(_locale)
  , collator(_collator)
  , displayNames(_displayNames)
{
  collator->setLocales(locale, locale, locale);
}

CollatorInfo::~CollatorInfo() {
  delete collator;
  delete displayNames;
}

UnicodeString&
CollatorInfo::getDisplayName(const Locale& displayLocale, UnicodeString& name) const {
  if (displayNames) {
    UnicodeString* val = static_cast<UnicodeString*>(displayNames->get(displayLocale.getName()));
    if (val) {
      name = *val;
      return name;
    }
  }

  return locale.getDisplayName(displayLocale, name);
}

// ---------------

class TestFactory : public CollatorFactory {
  CollatorInfo** info;
  int32_t count;
  UnicodeString* ids;

  const CollatorInfo* getInfo(const Locale& loc) const {
    for (CollatorInfo** p = info; *p; ++p) {
      if (loc == (**p).locale) {
        return *p;
      }
    }
    return nullptr;
  }

public:
  TestFactory(CollatorInfo** _info)
    : info(_info)
    , count(0)
    , ids(nullptr)
  {
    CollatorInfo** p;
    for (p = info; *p; ++p) {}
    count = (int32_t)(p - info);
  }

  ~TestFactory() {
    for (CollatorInfo** p = info; *p; ++p) {
      delete *p;
    }
    delete[] info;
    delete[] ids;
  }

  virtual Collator* createCollator(const Locale& loc) override {
    const CollatorInfo* ci = getInfo(loc);
    if (ci) {
      return ci->collator->clone();
    }
    return nullptr;
  }

  virtual UnicodeString& getDisplayName(const Locale& objectLocale,
                                        const Locale& displayLocale,
                                        UnicodeString& result) override
  {
    const CollatorInfo* ci = getInfo(objectLocale);
    if (ci) {
      ci->getDisplayName(displayLocale, result);
    } else {
      result.setToBogus();
    }
    return result;
  }

  const UnicodeString* getSupportedIDs(int32_t& _count, UErrorCode& status) override {
    if (U_SUCCESS(status)) {
      if (!ids) {
        ids = new UnicodeString[count];
        if (!ids) {
          status = U_MEMORY_ALLOCATION_ERROR;
          _count = 0;
          return nullptr;
        }

        for (int i = 0; i < count; ++i) {
          ids[i] = info[i]->locale.getName();
        }
      }

      _count = count;
      return ids;
    }
    return nullptr;
  }

  virtual inline UClassID getDynamicClassID() const override {
    return (UClassID)&gClassID;
  }

  static UClassID getStaticClassID() {
    return (UClassID)&gClassID;
  }

private:
  static char gClassID;
};

char TestFactory::gClassID = 0;
#endif

void CollationServiceTest::TestRegisterFactory()
{
#if !UCONFIG_NO_SERVICE
    int32_t n1, n2, n3;
    Locale fu_FU("fu", "FU", "");
    Locale fu_FU_FOO("fu", "FU", "FOO");

    UErrorCode status = U_ZERO_ERROR;

    Hashtable* fuFUNames = new Hashtable(false, status);
    if (!fuFUNames) {
        errln("memory allocation error");
        return;
    }
    fuFUNames->setValueDeleter(uprv_deleteUObject);

    fuFUNames->put(fu_FU.getName(), new UnicodeString("ze leetle bunny Fu-Fu"), status);
    fuFUNames->put(fu_FU_FOO.getName(), new UnicodeString("zee leetel bunny Foo-Foo"), status);
    fuFUNames->put(Locale::getDefault().getName(), new UnicodeString("little bunny Foo Foo"), status);

    Collator* frcol = Collator::createInstance(Locale::getFrance(), status);
    Collator* gecol = Collator::createInstance(Locale::getGermany(), status);
    Collator* jpcol = Collator::createInstance(Locale::getJapan(), status);
    if(U_FAILURE(status)) {
      errcheckln(status, "Failed to create collators with %s", u_errorName(status));
      delete frcol;
      delete gecol;
      delete jpcol;
      delete fuFUNames;
      return;
    }

    CollatorInfo** info = new CollatorInfo*[4];
    if (!info) {
        errln("memory allocation error");
        return;
    }

    info[0] = new CollatorInfo(Locale::getUS(), frcol, nullptr);
    info[1] = new CollatorInfo(Locale::getFrance(), gecol, nullptr);
    info[2] = new CollatorInfo(fu_FU, jpcol, fuFUNames);
    info[3] = nullptr;

    TestFactory* factory = new TestFactory(info);
    if (!factory) {
        errln("memory allocation error");
        return;
    }

    Collator* uscol = Collator::createInstance(Locale::getUS(), status);
    Collator* fucol = Collator::createInstance(fu_FU, status);

    {
        n1 = checkAvailable("before registerFactory");

        URegistryKey key = Collator::registerFactory(factory, status);

        n2 = checkAvailable("after registerFactory");
        assertTrue("count after > count before", n2 > n1);

        Collator* ncol = Collator::createInstance(Locale::getUS(), status);
        if (*frcol != *ncol) {
            errln("frcoll for en_US failed");
        }
        delete ncol; ncol = nullptr;

        ncol = Collator::createInstance(fu_FU_FOO, status);
        if (*jpcol != *ncol) {
            errln("jpcol for fu_FU_FOO failed");
        }

        // The requested locale may be the same as the valid locale,
        // or may not be supported at all. See ticket #10477.
        Locale loc = ncol->getLocale(ULOC_REQUESTED_LOCALE, status);
        if (U_SUCCESS(status) && loc != fu_FU_FOO && loc != fu_FU) {
            errln(UnicodeString("requested locale for fu_FU_FOO is not fu_FU_FOO nor fu_FU but ") + loc.getName());
        }
        status = U_ZERO_ERROR;
        loc = ncol->getLocale(ULOC_VALID_LOCALE, status);
        if (loc != fu_FU) {
            errln(UnicodeString("valid locale for fu_FU_FOO is not fu_FU but ") + loc.getName());
        }
        delete ncol; ncol = nullptr;

        UnicodeString locName = fu_FU.getName();
        StringEnumeration* localeEnum = Collator::getAvailableLocales();
        UBool found = false;
        const UnicodeString* locStr;
        for (locStr = localeEnum->snext(status);
            !found && locStr != nullptr;
            locStr = localeEnum->snext(status))
        {
            if (locName == *locStr) {
                found = true;
            }
        }
        delete localeEnum;

        if (!found) {
            errln("new locale fu_FU not reported as supported locale");
        }

        UnicodeString name;
        Collator::getDisplayName(fu_FU, name);
        if (name != "little bunny Foo Foo") {
            errln(UnicodeString("found ") + name + " for fu_FU");
        }

        Collator::getDisplayName(fu_FU, fu_FU_FOO, name);
        if (name != "zee leetel bunny Foo-Foo") {
            errln(UnicodeString("found ") + name + " for fu_FU in fu_FU_FOO");
        }

        if (!Collator::unregister(key, status)) {
            errln("failed to unregister factory");
        }
        // ja, fr, ge collators no longer valid

        ncol = Collator::createInstance(fu_FU, status);
        if (*fucol != *ncol) {
            errln("collator after unregister does not match original fu_FU");
        }
        delete ncol;

        n3 = checkAvailable("after unregister");
        assertTrue("count after unregister == count before register", n3 == n1);
    }

    delete fucol;
    delete uscol;
#endif
}

/**
 * Iterate through the given iterator, checking to see that all the strings
 * in the expected array are present.
 * @param expected array of strings we expect to see, or nullptr
 * @param expectedCount number of elements of expected, or 0
 */
int32_t CollationServiceTest::checkStringEnumeration(const char* msg,
                                                     StringEnumeration& iter,
                                                     const char** expected,
                                                     int32_t expectedCount) {
    UErrorCode ec = U_ZERO_ERROR;
    U_ASSERT(expectedCount >= 0 && expectedCount < 31); // [sic] 31 not 32
    int32_t i = 0, idxAfterReset = 0, n = iter.count(ec);
    assertSuccess("count", ec);
    UnicodeString buf, buffAfterReset;
    int32_t seenMask = 0;
    for (;; ++i) {
        const UnicodeString* s = iter.snext(ec);
        if (!assertSuccess("snext", ec) || s == nullptr)
            break;
        if (i != 0)
            buf.append(UNICODE_STRING_SIMPLE(", "));
        buf.append(*s);
        // check expected list
        for (int32_t j=0, bit=1; j<expectedCount; ++j, bit<<=1) {
            if ((seenMask&bit)==0) {
                UnicodeString exp(expected[j], (char*)nullptr);
                if (*s == exp) {
                    seenMask |= bit;
                    logln((UnicodeString)"Ok: \"" + exp + "\" seen");
                }
            }
        }
    }
    // can't get pesky operator+(const US&, foo) to cooperate; use toString
#if !UCONFIG_NO_FORMATTING
    logln(UnicodeString() + msg + " = [" + buf + "] (" + toString(i) + ")");
#else
    logln(UnicodeString() + msg + " = [" + buf + "] (??? NO_FORMATTING)");
#endif
    assertTrue("count verified", i==n);
    iter.reset(ec);
    for (;; ++idxAfterReset) {
        const char16_t *s = iter.unext(nullptr, ec);
        if (!assertSuccess("unext", ec) || s == nullptr)
            break;
        if (idxAfterReset != 0)
            buffAfterReset.append(UNICODE_STRING_SIMPLE(", "));
        buffAfterReset.append(s);
    }
    assertTrue("idxAfterReset verified", idxAfterReset==n);
    assertTrue("buffAfterReset verified", buffAfterReset==buf);
    // did we see all expected strings?
    if (((1<<expectedCount)-1) != seenMask) {
        for (int32_t j=0, bit=1; j<expectedCount; ++j, bit<<=1) {
            if ((seenMask&bit)==0) {
                errln((UnicodeString)"FAIL: \"" + expected[j] + "\" not seen");
            }
        }
    }
    return n;
}

/**
 * Check the integrity of the results of Collator::getAvailableLocales().
 * Return the number of items returned.
 */
#if !UCONFIG_NO_SERVICE
int32_t CollationServiceTest::checkAvailable(const char* msg) {
    StringEnumeration *iter = Collator::getAvailableLocales();
    if (!assertTrue("getAvailableLocales != nullptr", iter!=nullptr)) return -1;
    int32_t n = checkStringEnumeration(msg, *iter, nullptr, 0);
    delete iter;
    return n;
}
#endif

static const char* KW[] = {
    "collation"
};
static const int32_t KW_COUNT = UPRV_LENGTHOF(KW);

static const char* KWVAL[] = {
    "phonebook",
    "stroke",
    "emoji",
    "eor"
};
static const int32_t KWVAL_COUNT = UPRV_LENGTHOF(KWVAL);

void CollationServiceTest::TestSeparateTree() {
    UErrorCode ec = U_ZERO_ERROR;
    StringEnumeration *iter = Collator::getKeywords(ec);
    if (!assertTrue("getKeywords != nullptr", iter!=nullptr)) return;
    if (!assertSuccess("getKeywords", ec)) return;
    checkStringEnumeration("getKeywords", *iter, KW, KW_COUNT);
    delete iter;

    iter = Collator::getKeywordValues(KW[0], ec);
    if (!assertTrue("getKeywordValues != nullptr", iter!=nullptr, false, true)) return;
    if (!assertSuccess("getKeywordValues", ec)) return;
    checkStringEnumeration("getKeywordValues", *iter, KWVAL, KWVAL_COUNT);
    delete iter;

    UBool isAvailable;
    Locale equiv = Collator::getFunctionalEquivalent("collation",
                                                     Locale::createFromName("de"),
                                                     isAvailable, ec);
    assertSuccess("getFunctionalEquivalent", ec);
    assertEquals("getFunctionalEquivalent(de)", "", equiv.getName());
    assertTrue("getFunctionalEquivalent(de).isAvailable==true",
               isAvailable == true);

    equiv = Collator::getFunctionalEquivalent("collation",
                                              Locale::createFromName("de_DE"),
                                              isAvailable, ec);
    assertSuccess("getFunctionalEquivalent", ec);
    assertEquals("getFunctionalEquivalent(de_DE)", "", equiv.getName());
    assertTrue("getFunctionalEquivalent(de_DE).isAvailable==false",
               isAvailable == false);

    equiv = Collator::getFunctionalEquivalent("collation",
                                                     Locale::createFromName("sv"),
                                                     isAvailable, ec);
    assertSuccess("getFunctionalEquivalent", ec);
    assertEquals("getFunctionalEquivalent(sv)", "sv", equiv.getName());
    assertTrue("getFunctionalEquivalent(sv).isAvailable==true",
               isAvailable == true);

    equiv = Collator::getFunctionalEquivalent("collation",
                                              Locale::createFromName("sv_SE"),
                                              isAvailable, ec);
    assertSuccess("getFunctionalEquivalent", ec);
    assertEquals("getFunctionalEquivalent(sv_SE)", "sv", equiv.getName());
    assertTrue("getFunctionalEquivalent(sv_SE).isAvailable==false",
               isAvailable == false);
}

#endif
