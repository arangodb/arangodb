// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Helper method for ICU locale fuzzer.

#include "locale_util.h"

#include <string>

std::string MakeZeroTerminatedInput(const uint8_t *data, int32_t size) {
  return size == 0 ? "" : std::string(reinterpret_cast<const char *>(data), size);
}
