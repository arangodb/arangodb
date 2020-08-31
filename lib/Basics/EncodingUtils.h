////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#ifndef ARANGOD_BASICS_ENCODING_UTILS_H
#define ARANGOD_BASICS_ENCODING_UTILS_H 1

#include <velocypack/Buffer.h>

namespace arangodb {
namespace encoding {

bool gzipUncompress(uint8_t* compressed, size_t compressedLength,
                    arangodb::velocypack::Buffer<uint8_t>& uncompressed);

bool gzipDeflate(uint8_t* compressed, size_t compressedLength,
                 arangodb::velocypack::Buffer<uint8_t>& uncompressed);

}  // namespace encoding
}  // namespace arangodb

#endif
