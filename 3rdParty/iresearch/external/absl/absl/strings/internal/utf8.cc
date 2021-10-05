// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// UTF8 utilities, implemented to reduce dependencies.

#include "absl/strings/internal/utf8.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace strings_internal {

size_t EncodeUTF8Char(char *buffer, char32_t utf8_char) {
  if (utf8_char <= 0x7F) {
    *buffer = static_cast<char>(utf8_char);
    return 1;
  } else if (utf8_char <= 0x7FF) {
    buffer[1] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[0] = 0xC0 | utf8_char;
    return 2;
  } else if (utf8_char <= 0xFFFF) {
    buffer[2] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[1] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[0] = 0xE0 | utf8_char;
    return 3;
  } else {
    buffer[3] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[2] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[1] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[0] = 0xF0 | utf8_char;
    return 4;
  }
}

}  // namespace strings_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl
