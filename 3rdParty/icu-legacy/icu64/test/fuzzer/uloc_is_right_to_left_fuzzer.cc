// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <string>

#include "locale_util.h"
#include "unicode/uloc.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string input = MakeZeroTerminatedInput(data, size);

  uloc_isRightToLeft(input.c_str());

  return 0;
}
