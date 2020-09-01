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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_HASHES_H
#define ARANGODB_BASICS_HASHES_H 1

#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "Basics/Common.h"

/// @brief computes a FNV hash for blocks
uint64_t TRI_FnvHashBlock(uint64_t, void const*, size_t);

/// @brief computes a FNV hash for memory blobs
uint64_t TRI_FnvHashPointer(void const*, size_t);

/// @brief computes a FNV hash for strings
uint64_t TRI_FnvHashString(char const*);

/// @brief computes a FNV hash for POD types
template <typename T>
std::enable_if_t<std::is_pod_v<T>, uint64_t> TRI_FnvHashPod(T input) {
  return TRI_FnvHashPointer(&input, sizeof(T));
}

/// @brief computes a initial FNV for blocks
static constexpr uint64_t TRI_FnvHashBlockInitial() {
  return 0xcbf29ce484222325ULL;
}

/// @brief initial CRC32 value
static constexpr uint32_t TRI_InitialCrc32() { return (0xffffffff); }

/// @brief final CRC32 value
static constexpr uint32_t TRI_FinalCrc32(uint32_t value) {
  return (value ^ 0xffffffff);
}

/// @brief CRC32 value of data block
extern "C" {

#if ENABLE_ASM_CRC32 == 1
uint32_t TRI_BlockCrc32_SSE42(uint32_t, char const* data, size_t length);
#endif

uint32_t TRI_BlockCrc32_C(uint32_t hash, char const* data, size_t length);
extern uint32_t (*TRI_BlockCrc32)(uint32_t hash, char const* data, size_t length);
}

/// @brief computes a CRC32 for memory blobs
/// the polynomial used is 0x1EDC6F41.
uint32_t TRI_Crc32HashPointer(void const*, size_t);

/// @brief computes a CRC32 for strings
/// the polynomial used is 0x1EDC6F41.
uint32_t TRI_Crc32HashString(char const*);

#endif
