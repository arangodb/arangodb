// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include "fuzzer_utils.h"
#include "unicode/regex.h"

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UParseError pe = { 0 };
  UErrorCode status = U_ZERO_ERROR;

  URegularExpression* re = uregex_open(reinterpret_cast<const char16_t*>(data),
                                       static_cast<int>(size) / sizeof(char16_t),
                                       0, &pe, &status);

  if (re)
    uregex_close(re);
 
  return 0;
}
