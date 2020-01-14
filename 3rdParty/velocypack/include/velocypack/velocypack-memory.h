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
/// @author Lauri Keel
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_MEMORY_H
#define VELOCYPACK_MEMORY_H 1

#include <cstdint>
#include <memory>

// memory management definitions

extern "C" {

extern void* velocypack_malloc(std::size_t size);
extern void* velocypack_realloc(void* ptr, std::size_t size);
extern void velocypack_free(void* ptr);

#ifndef velocypack_malloc

#define velocypack_malloc(size) malloc(size)
#define velocypack_realloc(ptr, size) realloc(ptr, size)
#define velocypack_free(ptr) free(ptr)

#endif

}

#endif
