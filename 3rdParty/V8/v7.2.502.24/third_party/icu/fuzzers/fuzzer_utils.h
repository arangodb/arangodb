// Copyright 2016 The Chromium Authors. All rights reserved.

#ifndef THIRD_PARTY_ICU_FUZZERS_FUZZER_UTILS_H_
#define THIRD_PARTY_ICU_FUZZERS_FUZZER_UTILS_H_

#include <assert.h>
#include <algorithm>
#include <random>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"

struct IcuEnvironment {
  IcuEnvironment() {
    base::i18n::InitializeICU();
  }
};

// Create RNG and seed it from data.
std::mt19937_64 CreateRng(const uint8_t* data, size_t size) {
  std::mt19937_64 rng;
  std::string str = std::string(reinterpret_cast<const char*>(data), size);
  std::size_t data_hash = std::hash<std::string>()(str);
  rng.seed(data_hash);
  return rng;
}

const icu::Locale& GetRandomLocale(std::mt19937_64* rng) {
  int32_t num_locales = 0;
  const icu::Locale* locales = icu::Locale::getAvailableLocales(num_locales);
  assert(num_locales > 0);
  return locales[(*rng)() % num_locales];
}

icu::UnicodeString UnicodeStringFromUtf8(const uint8_t* data, size_t size) {
  return icu::UnicodeString::fromUTF8(
      icu::StringPiece(reinterpret_cast<const char*>(data), size));
}

icu::UnicodeString UnicodeStringFromUtf32(const uint8_t* data, size_t size) {
  std::vector<UChar32> uchars;
  uchars.resize(size * sizeof(uint8_t) / (sizeof(UChar32)));
  memcpy(uchars.data(), data, uchars.size() * sizeof(UChar32));
  for (size_t i = 0; i < uchars.size(); ++i) {
    uchars[i] = std::min(uchars[i], UCHAR_MAX_VALUE);
  }

  return icu::UnicodeString::fromUTF32(uchars.data(), uchars.size());
}

#endif  // THIRD_PARTY_ICU_FUZZERS_FUZZER_UTILS_H_
