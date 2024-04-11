// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstring>

#include "fuzzer_utils.h"
#include "unicode/coll.h"
#include "unicode/localpointer.h"
#include "unicode/uniset.h"

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;

  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);
  icu::UnicodeString fuzzstr(false, fuzzbuff.get(), unistr_size);

  icu::LocalPointer<icu::UnicodeSet> set(
      new icu::UnicodeSet(fuzzstr, status));

  status = U_ZERO_ERROR;

  set.adoptInstead(new icu::UnicodeSet());
  set->applyPattern (fuzzstr, status);

  set.adoptInstead(new icu::UnicodeSet());
  set->addAll(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->add(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->retainAll(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->complementAll(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->removeAll(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->retain(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->remove(fuzzstr);

  set.adoptInstead(new icu::UnicodeSet());
  set->complement(fuzzstr);

  set.adoptInstead(icu::UnicodeSet::createFrom(fuzzstr));

  set.adoptInstead(icu::UnicodeSet::createFromAll(fuzzstr));
  return 0;
}
