////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"

#include <cstddef>
#include <cstdint>

namespace arangodb::encoding {

template<typename T>
[[nodiscard]] ErrorCode gzipUncompress(uint8_t const* compressed,
                                       size_t compressedLength,
                                       T& uncompressed);

template<typename T>
[[nodiscard]] ErrorCode zlibInflate(uint8_t const* compressed,
                                    size_t compressedLength, T& uncompressed);

template<typename T>
[[nodiscard]] ErrorCode lz4Uncompress(uint8_t const* compressed,
                                      size_t compressedLength, T& uncompressed);

template<typename T>
[[nodiscard]] ErrorCode gzipCompress(uint8_t const* uncompressed,
                                     size_t uncompressedLength, T& compressed);

template<typename T>
[[nodiscard]] ErrorCode zlibDeflate(uint8_t const* uncompressed,
                                    size_t uncompressedLength, T& compressed);

template<typename T>
[[nodiscard]] ErrorCode lz4Compress(uint8_t const* uncompressed,
                                    size_t uncompressedLength, T& compressed);

}  // namespace arangodb::encoding
