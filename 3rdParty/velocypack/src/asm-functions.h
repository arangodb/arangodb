////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_ASM_H
#define VELOCYPACK_ASM_H 1

#include <cstdint>
#include <cstring>

static inline size_t JSONStringCopyInline(uint8_t* dst, uint8_t const* src,
                                          size_t limit) {
  // Copy up to limit uint8_t from src to dst.
  // Stop at the first control character or backslash or double quote.
  // Report the number of bytes copied. May copy less bytes, for example
  // for alignment reasons.
  size_t count = limit;
  while (count > 0 && *src >= 32 && *src != '\\' && *src != '"') {
    *dst++ = *src++;
    count--;
  }
  return limit - count;
}

size_t JSONStringCopyC(uint8_t* dst, uint8_t const* src, size_t limit);
extern size_t (*JSONStringCopy)(uint8_t*, uint8_t const*, size_t);

// Now a version which also stops at high bit set bytes:

static inline size_t JSONStringCopyCheckUtf8Inline(uint8_t* dst,
                                                   uint8_t const* src,
                                                   size_t limit) {
  // Copy up to limit uint8_t from src to dst.
  // Stop at the first control character or backslash or double quote.
  // Also stop at byte with high bit set.
  // Report the number of bytes copied. May copy less bytes, for example
  // for alignment reasons.
  size_t count = limit;
  while (count > 0 && *src >= 32 && *src != '\\' && *src != '"' &&
         *src < 0x80) {
    *dst++ = *src++;
    count--;
  }
  return limit - count;
}

size_t JSONStringCopyCheckUtf8C(uint8_t* dst, uint8_t const* src, size_t limit);
extern size_t (*JSONStringCopyCheckUtf8)(uint8_t*, uint8_t const*, size_t);

// White space skipping:

static inline size_t JSONSkipWhiteSpaceInline(uint8_t const* ptr,
                                              size_t limit) {
  // Skip up to limit uint8_t from ptr as long as they are whitespace.
  // Advance ptr and return the number of skipped bytes.
  size_t count = limit;
  while (count > 0 &&
         (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
    ptr++;
    count--;
  }
  return limit - count;
}

size_t JSONSkipWhiteSpaceC(uint8_t const* ptr, size_t limit);
extern size_t (*JSONSkipWhiteSpace)(uint8_t const*, size_t);

#endif
