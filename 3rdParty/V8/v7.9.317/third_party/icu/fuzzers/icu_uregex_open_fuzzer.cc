// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/icu/source/i18n/unicode/regex.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UParseError pe = { 0 };
  UErrorCode status = U_ZERO_ERROR;
  URegularExpression* re = uregex_open(reinterpret_cast<const UChar*>(data),
                                       static_cast<int>(size) / sizeof(UChar),
                                       0, &pe, &status);
  if (re)
    uregex_close(re);

  return 0;
}
