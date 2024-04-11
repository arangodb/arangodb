// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstring>

#include "fuzzer_utils.h"
#include "unicode/localpointer.h"
#include "unicode/numberformatter.h"

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;

  int16_t rnd;
  int64_t value;
  double doubleValue;
  if (size < sizeof(rnd) + sizeof(value) + sizeof(doubleValue)) return 0;
  icu::StringPiece fuzzData(reinterpret_cast<const char *>(data), size);

  std::memcpy(&rnd, fuzzData.data(), sizeof(rnd));
  icu::Locale locale = GetRandomLocale(rnd);
  fuzzData.remove_prefix(sizeof(rnd));

  std::memcpy(&value, fuzzData.data(), sizeof(value));
  fuzzData.remove_prefix(sizeof(value));

  std::memcpy(&doubleValue, fuzzData.data(), sizeof(doubleValue));
  fuzzData.remove_prefix(sizeof(doubleValue));

  size_t len = fuzzData.size() / sizeof(char16_t);
  icu::UnicodeString fuzzstr(false, reinterpret_cast<const char16_t*>(fuzzData.data()), len);

  icu::number::UnlocalizedNumberFormatter unf = icu::number::NumberFormatter::forSkeleton(
      fuzzstr, status);

  icu::number::LocalizedNumberFormatter nf = unf.locale(locale);

  status = U_ZERO_ERROR;
  nf.formatInt(value, status);

  status = U_ZERO_ERROR;
  nf.formatDouble(doubleValue, status);
  return 0;
}
