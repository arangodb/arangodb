////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_VOC_TYPES_H
#define ARANGOD_VOC_BASE_VOC_TYPES_H 1

#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Basics/Identifier.h"

/// @brief tick type (56bit)
typedef uint64_t TRI_voc_tick_t;

/// @brief enum for write operations
enum TRI_voc_document_operation_e : uint8_t {
  TRI_VOC_DOCUMENT_OPERATION_UNKNOWN = 0,
  TRI_VOC_DOCUMENT_OPERATION_INSERT,
  TRI_VOC_DOCUMENT_OPERATION_UPDATE,
  TRI_VOC_DOCUMENT_OPERATION_REPLACE,
  TRI_VOC_DOCUMENT_OPERATION_REMOVE
};

/// @brief edge direction
enum TRI_edge_direction_e {
  TRI_EDGE_ANY = 0,  // can only be used for searching
  TRI_EDGE_IN = 1,
  TRI_EDGE_OUT = 2
};

enum class ShardingPrototype : uint32_t {
  Undefined = 0,
  Users = 1,
  Graphs = 2
};

/// @brief Hash function for a vector of VPackSlice
namespace std {

template <>
struct hash<std::vector<arangodb::velocypack::Slice>> {
  size_t operator()(std::vector<arangodb::velocypack::Slice> const& x) const {
    std::hash<arangodb::velocypack::Slice> sliceHash;
    size_t res = 0xdeadbeef;
    for (auto& el : x) {
      res ^= sliceHash(el);
    }
    return res;
  }
};

}  // namespace std

/// @brief databases list structure
struct TRI_vocbase_t;

#endif
