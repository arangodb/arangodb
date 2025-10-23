////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////
/// @brief Strategy pattern for handling vector index stored values
///
/// Vector indexes can optionally store additional document fields alongside
/// the encoded vector data. This eliminates the need for runtime if-checks by
/// using compile-time strategy selection via template parameters.
///
/// Usage:
///   - NoStoredValuesStrategy: Use when index has no stored values
///   - WithStoredValuesStrategy: Use when index has stored values
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <rocksdb/slice.h>

#include <cstdint>
#include <vector>
#include <utility>
#include <concepts>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Concept defining requirements for a vector index stored values
/// strategy
////////////////////////////////////////////////////////////////////////////////
template<typename T>
concept VectorIndexStoredValuesStrategy = requires {
  // Must have a compile-time constant indicating if stored values are present
  { T::hasStoredValues } -> std::convertible_to<bool>;
}
&&requires(rocksdb::Slice const& key, rocksdb::Slice const& value,
           size_t codeSize, std::vector<uint8_t> const& encodedValue,
           velocypack::Slice storedValues) {
  // Must be able to extract entry from RocksDB key/value
  {T::extractVectorIndexEntry(key, value, codeSize)};

  // Must be able to build RocksDB value for insertion
  {
    T::buildVectorIndexValue(encodedValue, storedValues)
    } -> std::same_as<RocksDBValue>;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Strategy for vector indexes WITHOUT stored values
///
/// When stored values are not present, the RocksDB value contains only the
/// encoded vector data (raw bytes from FAISS).
////////////////////////////////////////////////////////////////////////////////
struct NoStoredValuesStrategy {
  static constexpr bool hasStoredValues = false;

  // Extract encoded vector from raw bytes
  // Returns document ID and the encoded vector (moved)
  // Caller manages the vector lifetime and can extract pointer as needed
  static std::pair<LocalDocumentId, std::vector<uint8_t>>
  extractVectorIndexEntry(rocksdb::Slice const& key,
                          rocksdb::Slice const& value, size_t codeSize) {
    auto const docId = RocksDBKey::indexDocumentId(key);
    std::vector<uint8_t> encodedValue(
        reinterpret_cast<uint8_t const*>(value.data()),
        reinterpret_cast<uint8_t const*>(value.data()) + codeSize);
    return {docId, std::move(encodedValue)};
  }

  // Build RocksDB value containing only encoded vector data
  static RocksDBValue buildVectorIndexValue(
      std::vector<uint8_t> const& encodedValue,
      velocypack::Slice /*storedValues*/) {
    return RocksDBValue::VectorIndexValue(encodedValue.data(),
                                          encodedValue.size());
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Strategy for vector indexes WITH stored values
///
/// When stored values are present, the RocksDB value contains a serialized
/// RocksDBVectorIndexEntryValue with both encoded vector data and stored
/// values.
////////////////////////////////////////////////////////////////////////////////
struct WithStoredValuesStrategy {
  static constexpr bool hasStoredValues = true;

  // Extract full entry containing both encoded vector and stored values
  // Returns document ID and the complete entry (moved)
  // Caller can access entry.encodedValue and entry.storedValues as needed
  static std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>
  extractVectorIndexEntry(rocksdb::Slice const& key,
                          rocksdb::Slice const& value, size_t /*codeSize*/) {
    auto const docId = RocksDBKey::indexDocumentId(key);
    auto entry = RocksDBValue::vectorIndexEntryValue(value);
    return {docId, std::move(entry)};
  }

  // Build RocksDB value containing encoded vector data + stored values
  static RocksDBValue buildVectorIndexValue(
      std::vector<uint8_t> const& encodedValue,
      velocypack::Slice storedValues) {
    RocksDBVectorIndexEntryValue entry;
    entry.encodedValue = encodedValue;
    // Create a Buffer copy and construct SharedSlice from it
    velocypack::Buffer<uint8_t> buffer;
    buffer.append(storedValues.start(), storedValues.byteSize());
    entry.storedValues = velocypack::SharedSlice(std::move(buffer));
    return RocksDBValue::VectorIndexValue(entry);
  }
};

}  // namespace arangodb
