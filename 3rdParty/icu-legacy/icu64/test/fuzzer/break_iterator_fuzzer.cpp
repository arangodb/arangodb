// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <memory>
#include <utility>
#include "fuzzer_utils.h"
#include "unicode/brkiter.h"
#include "unicode/utext.h"

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;
  uint8_t rnd8 = 0;
  uint16_t rnd16 = 0;

  if (size < 3) {
    return 0;
  }
  // Extract one and two bytes from fuzzer data for random selection purpose.
  rnd8 = *data;
  data++;
  rnd16 = *(reinterpret_cast<const uint16_t *>(data));
  data = data + 2;
  size = size - 3;

  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);

  UText* fuzzstr = utext_openUChars(nullptr, fuzzbuff.get(), unistr_size, &status);

  const icu::Locale& locale = GetRandomLocale(rnd16);

  std::unique_ptr<icu::BreakIterator> bi;

  switch (rnd8 % 5) {
    case 0:
      bi.reset(icu::BreakIterator::createWordInstance(locale, status));
      break;
    case 1:
      bi.reset(icu::BreakIterator::createLineInstance(locale, status));
      break;
    case 2:
      bi.reset(icu::BreakIterator::createCharacterInstance(locale, status));
      break;
    case 3:
      bi.reset(icu::BreakIterator::createSentenceInstance(locale, status));
      break;
    case 4:
      bi.reset(icu::BreakIterator::createTitleInstance(locale, status));
      break;
  }

  bi->setText(fuzzstr, status);

  if (U_FAILURE(status)) {
    utext_close(fuzzstr);
    return 0;
  }

  for (int32_t p = bi->first(); p != icu::BreakIterator::DONE; p = bi->next()) {}

  utext_close(fuzzstr);

  std::string str(reinterpret_cast<const char*>(data), size);
  icu::Locale locale2(str.c_str()); // ensure null-termination by c_str()
  switch (rnd8 % 5) {
    case 0:
      bi.reset(icu::BreakIterator::createWordInstance(locale2, status));
      break;
    case 1:
      bi.reset(icu::BreakIterator::createLineInstance(locale2, status));
      break;
    case 2:
      bi.reset(icu::BreakIterator::createCharacterInstance(locale2, status));
      break;
    case 3:
      bi.reset(icu::BreakIterator::createSentenceInstance(locale2, status));
      break;
    case 4:
      bi.reset(icu::BreakIterator::createTitleInstance(locale2, status));
      break;
  }

  return 0;
}

