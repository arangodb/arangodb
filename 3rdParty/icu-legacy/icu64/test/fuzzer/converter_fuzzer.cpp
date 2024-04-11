// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#include "fuzzer_utils.h"
#include "unicode/unistr.h"
#include "unicode/ucnv.h"

IcuEnvironment* env = new IcuEnvironment();

template <typename T>
using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;
  uint16_t rnd = 0;

  if (size < 2) {
    return 0;
  }

  rnd = *(reinterpret_cast<const uint16_t *>(data));
  data = data + 2;
  size = size - 2;

  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);
  
  icu::UnicodeString fuzzstr(false, fuzzbuff.get(), unistr_size);

  const char* converter_name =
      ucnv_getAvailableName(rnd % ucnv_countAvailable());
  deleted_unique_ptr<UConverter> converter(ucnv_open(converter_name, &status),
                                           &ucnv_close);
  if (U_FAILURE(status)) {
    return 0;
  }

  static const size_t dest_buffer_size = 1024 * 1204;
  static const std::unique_ptr<char[]> dest_buffer(new char[dest_buffer_size]);

  fuzzstr.extract(dest_buffer.get(), dest_buffer_size, converter.get(), status);

  return 0;
}
