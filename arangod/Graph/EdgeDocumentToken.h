////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace graph {

/// @brief Pure virtual abstract class to uniquely identify an edge
struct EdgeDocumentToken {
  EdgeDocumentToken() noexcept
      : _data(DataSourceId::none(), LocalDocumentId::none()) {}

  EdgeDocumentToken(EdgeDocumentToken&& edtkn) noexcept : _data(edtkn._data) {}

  EdgeDocumentToken(EdgeDocumentToken const& edtkn) noexcept
      : _data(edtkn._data) {}

  EdgeDocumentToken(DataSourceId const cid,
                    LocalDocumentId const localDocumentId) noexcept
      : _data(cid, localDocumentId) {}

  EdgeDocumentToken& operator=(EdgeDocumentToken const& edtkn) {
    _data = edtkn._data;
    return *this;
  }

  EdgeDocumentToken& operator=(EdgeDocumentToken&& edtkn) {
    _data = edtkn._data;
    return *this;
  }

  DataSourceId cid() const { return _data.cid; }

  LocalDocumentId localDocumentId() const { return _data.localDocumentId; }

  bool equals(EdgeDocumentToken const& other) const {
    return _data.cid == other.cid() &&
           _data.localDocumentId == other.localDocumentId();
  }

  int compare(EdgeDocumentToken const& other) const {
    if (_data.cid < other.cid()) {
      return -1;
    }
    if (_data.cid > other.cid()) {
      return 1;
    }
    if (_data.localDocumentId < other.localDocumentId()) {
      return -1;
    }
    if (_data.localDocumentId > other.localDocumentId()) {
      return 1;
    }
    return 0;
  }

  bool isValid() const {
    return _data.cid != DataSourceId::none() &&
           _data.localDocumentId != LocalDocumentId::none();
  }

  size_t hash() const {
    return std::hash<LocalDocumentId>{}(_data.localDocumentId) ^
           (_data.cid.id() << 1);
  }

  std::string toString() const {
    return std::to_string(_data.cid.id()) + ":" +
           std::to_string(_data.localDocumentId.id());
  }

 private:
  /// Identifying information for an edge documents valid on one server
  /// only used on a dbserver or single server
  struct LocalDocument {
    DataSourceId cid;
    LocalDocumentId localDocumentId;
    ~LocalDocument() = default;
  };

  /// fixed size union, works for both single server and
  /// cluster case
  using TokenData = EdgeDocumentToken::LocalDocument;
  TokenData _data;
};
}  // namespace graph
}  // namespace arangodb

namespace std {
template<>
struct hash<arangodb::graph::EdgeDocumentToken> {
  size_t operator()(
      arangodb::graph::EdgeDocumentToken const& value) const noexcept {
    return value.hash();
  }
};

template<>
struct equal_to<arangodb::graph::EdgeDocumentToken> {
  bool operator()(
      arangodb::graph::EdgeDocumentToken const& lhs,
      arangodb::graph::EdgeDocumentToken const& rhs) const noexcept {
    return lhs.equals(rhs);
  }
};
}  // namespace std
