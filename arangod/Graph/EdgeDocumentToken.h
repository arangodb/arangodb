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
      : _cid(DataSourceId::none()), _localDocumentId(LocalDocumentId::none()) {}

  EdgeDocumentToken(EdgeDocumentToken&& edtkn) noexcept
      : _cid(edtkn._cid), _localDocumentId(edtkn._localDocumentId) {}

  EdgeDocumentToken(EdgeDocumentToken const& edtkn) noexcept
      : _cid(edtkn._cid), _localDocumentId(edtkn._localDocumentId) {}

  EdgeDocumentToken(DataSourceId const cid,
                    LocalDocumentId const localDocumentId) noexcept
      : _cid(cid), _localDocumentId(localDocumentId) {}

  EdgeDocumentToken& operator=(EdgeDocumentToken const& edtkn) {
    _cid = edtkn._cid;
    _localDocumentId = edtkn._localDocumentId;
    return *this;
  }

  EdgeDocumentToken& operator=(EdgeDocumentToken&& edtkn) {
    _cid = edtkn._cid;
    _localDocumentId = edtkn._localDocumentId;
    return *this;
  }

  DataSourceId cid() const { return _cid; }

  LocalDocumentId localDocumentId() const { return _localDocumentId; }

  bool equals(EdgeDocumentToken const& other) const {
    return _cid == other.cid() && _localDocumentId == other.localDocumentId();
  }

  int compare(EdgeDocumentToken const& other) const {
    if (_cid < other.cid()) {
      return -1;
    }
    if (_cid > other.cid()) {
      return 1;
    }
    if (_localDocumentId < other.localDocumentId()) {
      return -1;
    }
    if (_localDocumentId > other.localDocumentId()) {
      return 1;
    }
    return 0;
  }

  bool isValid() const {
    return _cid != DataSourceId::none() &&
           _localDocumentId != LocalDocumentId::none();
  }

  size_t hash() const {
    return std::hash<LocalDocumentId>{}(_localDocumentId) ^ (_cid.id() << 1);
  }

  std::string toString() const {
    return std::to_string(_cid.id()) + ":" +
           std::to_string(_localDocumentId.id());
  }

 private:
  /// Identifying information for an edge documents valid on one server
  /// only used on a dbserver or single server
  DataSourceId _cid;
  LocalDocumentId _localDocumentId;
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
