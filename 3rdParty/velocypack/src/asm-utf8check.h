//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 Daniel Lemire
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
////////////////////////////////////////////////////////////////////////////////


#ifndef VELOCYPACK_ASM_UTF8CHECK_H
#define VELOCYPACK_ASM_UTF8CHECK_H

#include <cstddef>
#include <cstdint>

namespace arangodb {
namespace velocypack {

#if ASM_OPTIMIZATIONS == 1
bool validate_utf8_fast_sse42(uint8_t const* src, std::size_t len);
#ifdef __AVX2__
bool validate_utf8_fast_avx_asciipath(const char *src, std::size_t len);
bool validate_utf8_fast_avx(uint8_t const* src, std::size_t len);
#endif // __AVX2__
#endif // ASM_OPTIMIZATIONS
  
}}
#endif
