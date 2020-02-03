// Copyright 2016 The Chromium Authors. All rights reserved.

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include "third_party/icu/fuzzers/fuzzer_utils.h"
#include "third_party/icu/source/common/unicode/brkiter.h"

IcuEnvironment* env = new IcuEnvironment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString str(UnicodeStringFromUtf8(data, size));

  auto rng = CreateRng(data, size);
  const icu::Locale& locale = GetRandomLocale(&rng);

  std::unique_ptr<icu::BreakIterator> bi;

  switch (rng() % 5) {
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
  if (U_FAILURE(status))
    return 0;
  bi->setText(str);

  for (int32_t p = bi->first(); p != icu::BreakIterator::DONE; p = bi->next())
    if (U_FAILURE(status))
      return 0;

  return 0;
}

