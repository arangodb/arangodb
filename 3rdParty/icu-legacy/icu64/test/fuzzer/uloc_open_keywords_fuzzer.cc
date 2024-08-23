// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <string>

#include "locale_util.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input = MakeZeroTerminatedInput(data, size);

  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = uloc_openKeywords(input.c_str(), &status);

  // Have to clean up. Call works even for nullptr enumeration.
  uenum_close(enumeration);

  return 0;
}
