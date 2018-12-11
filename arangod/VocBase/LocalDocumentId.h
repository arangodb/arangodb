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
 public:
  typedef uint64_t BaseType;

 public:
  constexpr LocalDocumentId() noexcept : _id(0) {}
  constexpr explicit LocalDocumentId(BaseType id) noexcept : _id(id) {}
  LocalDocumentId(LocalDocumentId const& other) noexcept : _id(other._id) {}
  LocalDocumentId& operator=(LocalDocumentId const& other) noexcept { _id = other._id; return *this; }
  LocalDocumentId(LocalDocumentId&& other) noexcept : _id(other._id) {}
  LocalDocumentId& operator=(LocalDocumentId&& other) noexcept { _id = other._id; return *this; }

  /// @brief whether or not the id is set
  inline bool isSet() const noexcept { return _id != 0; }
  inline bool empty() const noexcept { return !isSet(); }

  /// @brief return the document id
  inline BaseType id() const noexcept { return _id; }
  inline BaseType const* data() const noexcept { return &_id; }
 
  // same as isSet() 
  inline operator bool() const noexcept { return _id != 0; }
  
  /// @brief check if two LocalDocumentIds are equal
  inline bool operator==(LocalDocumentId const& other) const {
    return _id == other._id;
  }
  
  inline bool operator!=(LocalDocumentId const& other) const { return !(operator==(other)); }
  
  /// @brief check if two LocalDocumentIds are equal (less)
  inline bool operator<(LocalDocumentId const& other) const {
    return _id < other._id;
  }
  
  /// @brief check if two LocalDocumentIds are equal (less)
  inline bool operator>(LocalDocumentId const& other) const {
    return _id > other._id;
  }

  void clear() { _id = 0; }
  
  /// @brief create a not-set document id 
  static LocalDocumentId none() { return LocalDocumentId(0); }

  /// @brief create a new document id
  static LocalDocumentId create() { return LocalDocumentId(TRI_HybridLogicalClock()); }
  
  /// @brief create a document id from an existing id
  static LocalDocumentId create(BaseType id) { return LocalDocumentId(id); }

  /// @brief use to track an existing value in recovery to ensure no duplicates
  static void track(LocalDocumentId const& id) { TRI_HybridLogicalClock(id.id()); }

 private:
  BaseType _id;
};

// LocalDocumentId should not be bigger than the BaseType
static_assert(sizeof(LocalDocumentId) == sizeof(LocalDocumentId::BaseType), "invalid size of LocalDocumentId");
}

namespace std {
template <>
struct hash<arangodb::LocalDocumentId> {
  inline size_t operator()(arangodb::LocalDocumentId const& value) const noexcept {
    // TODO: we may need to use a better hash for this
    return value.id();
  }
};

template <>
struct equal_to<arangodb::LocalDocumentId> {
  bool operator()(arangodb::LocalDocumentId const& lhs,
                  arangodb::LocalDocumentId const& rhs) const { return lhs == rhs; }
};
}

#endif
