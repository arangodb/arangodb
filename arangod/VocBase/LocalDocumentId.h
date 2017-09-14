////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_LOCAL_DOCUMENT_ID_H
#define ARANGOD_VOCBASE_LOCAL_DOCUMENT_ID_H 1

#include "Basics/Common.h"
#include "VocBase/ticks.h"

namespace arangodb {
/// @brief a LocalDocumentId is an identifier for storing and retrieving documents
/// using a uint64_t value.
class LocalDocumentId {
 private:
  explicit LocalDocumentId(uint64_t id) : _id(id) {}

 public:
  /// @brief return the document id
  inline uint64_t id() const noexcept { return _id; }
  
  /// @brief check if two LocalDocumentIds are equal
  inline bool operator==(LocalDocumentId const& other) const {
    return _id == other._id;
  }
  
  /// @brief check if two LocalDocumentIds are equal (less)
  inline bool operator<(LocalDocumentId const& other) const {
    return _id < other._id;
  }

  /// @brief create a new document id
  static LocalDocumentId create() { return LocalDocumentId(TRI_HybridLogicalClock()); }
  static LocalDocumentId create(uint64_t id) { return LocalDocumentId(id); }

 private:
  uint64_t const _id;
};

// LocalDocumentId should not be bigger than a uint64_t
static_assert(sizeof(LocalDocumentId) == sizeof(uint64_t), "invalid size of LocalDocumentId");
}

#endif
