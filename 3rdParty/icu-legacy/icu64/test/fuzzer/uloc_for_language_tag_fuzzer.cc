// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <string>

#include "locale_util.h"
#include "unicode/uloc.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Full locale id.
  char locale_id[ULOC_FULLNAME_CAPACITY];
  int32_t locale_id_capacity = ULOC_FULLNAME_CAPACITY;

  const std::string input = MakeZeroTerminatedInput(data, size);

  UErrorCode status = U_ZERO_ERROR;
  uloc_forLanguageTag(input.c_str(), locale_id, locale_id_capacity, nullptr,
                      &status);

  return 0;
}
