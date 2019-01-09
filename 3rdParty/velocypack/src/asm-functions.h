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

extern size_t (*JSONStringCopy)(uint8_t*, uint8_t const*, size_t);

// Now a version which also stops at high bit set bytes:
extern size_t (*JSONStringCopyCheckUtf8)(uint8_t*, uint8_t const*, size_t);

// White space skipping:
extern size_t (*JSONSkipWhiteSpace)(uint8_t const*, size_t);

// check string for invalid utf-8 sequences
extern bool (*ValidateUtf8String)(uint8_t const*, size_t);

namespace arangodb {
namespace velocypack {

void enableNativeStringFunctions();
void enableBuiltinStringFunctions();

}
}

#endif
