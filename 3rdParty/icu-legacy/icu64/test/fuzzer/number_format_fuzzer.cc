// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Fuzzer for NumberFormat::parse.

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include "fuzzer_utils.h"
#include "unicode/numfmt.h"

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;
  uint16_t rnd = 0;

  if (size < 2) {
    return 0;
  }

  rnd = *(reinterpret_cast<const uint16_t *>(data));
  data = data + 2;
  size = size - 2;

  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);

  const icu::Locale& locale = GetRandomLocale(rnd);

  std::unique_ptr<icu::NumberFormat> fmt(
      icu::NumberFormat::createInstance(locale, status));
  if (U_FAILURE(status)) {
    return 0;
  }

  icu::UnicodeString fuzzstr(false, fuzzbuff.get(), unistr_size);
  icu::Formattable result;
  fmt->parse(fuzzstr, result, status);

  return 0;
}
