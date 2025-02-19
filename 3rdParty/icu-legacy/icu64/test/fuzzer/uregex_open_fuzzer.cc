// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <stddef.h>
#include <stdint.h>

#include "fuzzer_utils.h"
#include "unicode/regex.h"

IcuEnvironment* env = new IcuEnvironment();

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
