// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <array>
#include <cstring>

#include "fuzzer_utils.h"
#include "unicode/coll.h"
#include "unicode/localpointer.h"
#include "unicode/locid.h"

IcuEnvironment* env = new IcuEnvironment();

static const std::array<icu::Collator::ECollationStrength, 5> kStrength = {
    icu::Collator::PRIMARY,
    icu::Collator::SECONDARY,
    icu::Collator::TERTIARY,
    icu::Collator::QUATERNARY,
    icu::Collator::IDENTICAL
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;

  uint16_t rnd16;

  if (size < 2 + sizeof(rnd16))
    return 0;

  std::memcpy(&rnd16, data, sizeof(rnd16));
  size -= sizeof(rnd16);
  data += sizeof(rnd16);
  icu::Collator::ECollationStrength strength = kStrength[rnd16 % kStrength.size()];
  const icu::Locale& locale = GetRandomLocale(rnd16 / kStrength.size());

  // Limit the comparison size to 4096 to avoid unnecessary timeout
  if (size > 4096) {
      size = 4096;
  }
  std::unique_ptr<char16_t[]> compbuff1(new char16_t[size/4]);
  std::memcpy(compbuff1.get(), data, (size/4)*2);
  std::unique_ptr<char16_t[]> compbuff2(new char16_t[size/4]);
  std::memcpy(compbuff2.get(), data + size/2, (size/4)*2);


  icu::LocalPointer<icu::Collator> fuzzCollator(
      icu::Collator::createInstance(locale, status), status);
  if (U_SUCCESS(status)) {

    fuzzCollator->setStrength(strength);

    fuzzCollator->compare(compbuff1.get(), size/4,
                          compbuff2.get(), size/4);
  }
  status = U_ZERO_ERROR;

  std::string str(reinterpret_cast<const char*>(data), size);
  fuzzCollator.adoptInstead(
      icu::Collator::createInstance(icu::Locale(str.c_str()), status));
  return 0;
}
