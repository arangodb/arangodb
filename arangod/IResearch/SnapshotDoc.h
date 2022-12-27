////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include <span>

#include "VocBase/Identifiers/LocalDocumentId.h"

namespace arangodb {
class LogicalColletion;
class StorageSnapshot;
}

namespace arangodb::iresearch {
// Stores LocalDocumentId, Collection pointer and corresponding
// storage engine snapshot as 2 VPACK_INLINE AqlValues
// Layout:
// Reg1: <Type Byte for AqlValue><8 bytes LocalDocumentId, 4 first bytes of snapshot ptr>
// Reg2: <Type Byte for AqlValue><8 bytes collection pointer, 4 last bytes of snapshot ptr>
constexpr size_t kDocRegBufSize = sizeof(LocalDocumentId) + sizeof(void*) / 2;
constexpr size_t kCollectionRegBufSize = sizeof(void*) + sizeof(void*) / 2;

struct SnapshotDoc {
 public:

  static SnapshotDoc decode(std::string_view documentReg, std::string_view collectionReg) noexcept {
    if (ADB_LIKELY(kDocRegBufSize <= documentReg.size() && 
                   kCollectionRegBufSize <= collectionReg.size())) {
      SnapshotDoc res;
      std::memcpy(&res._documentId, documentReg.data(), sizeof res._documentId);
      std::memcpy(&res._snapshot, documentReg.data() + sizeof res._documentId, 4);
     
      std::memcpy(&res._collection, collectionReg.data(), sizeof res._collection);
      std::memcpy(reinterpret_cast<char*>(&res._snapshot) + 4,
                  collectionReg.data() + sizeof res._collection,
                  4);
      return res;
    }
    return {};
  }

  constexpr SnapshotDoc() = default;

  SnapshotDoc(LocalDocumentId documentId, LogicalCollection const* collection,
              StorageSnapshot const* snapshot) noexcept
      : _documentId(documentId), _collection(collection), _snapshot(snapshot) {}

  auto* collection() const noexcept { return _collection; }

  auto doc() const noexcept { return _documentId; }

  auto* snapshot() const noexcept { return _snapshot; }

  bool isValid() const noexcept {
    return _collection && _snapshot && _documentId.isSet();
  }

  constexpr auto operator<=>(const SnapshotDoc&) const noexcept = default;

  constexpr bool operator==(const SnapshotDoc& rhs) const noexcept {
    return _collection == rhs._collection && _documentId == rhs._documentId &&
           _snapshot == rhs._snapshot;
  }

  void encode(std::span<char, kDocRegBufSize> documentReg,
      std::span<char, kCollectionRegBufSize> collectionReg) const noexcept {
    std::memcpy(documentReg.data(), static_cast<void const*>(&_documentId),
                sizeof(_documentId));
    std::memcpy(documentReg.data() + sizeof(_documentId),
                static_cast<void const*>(&_snapshot),
                4);
    std::memcpy(collectionReg.data(), static_cast<void const*>(&_collection), sizeof _collection);
    std::memcpy(collectionReg.data() + sizeof(_collection),
                reinterpret_cast<char const*>(&_snapshot) + 4,
                4);
  }

 private:
  LocalDocumentId _documentId;
  LogicalCollection const* _collection{nullptr};
  StorageSnapshot const* _snapshot;
};

}  // namespace arangodb::iresearch
