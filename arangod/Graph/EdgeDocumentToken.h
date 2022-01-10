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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      : _data(DataSourceId::none(), LocalDocumentId::none()),
        _type(TokenType::NONE) {
  }
#else
      : _data(DataSourceId::none(), LocalDocumentId::none()) {
  }
#endif

  EdgeDocumentToken(EdgeDocumentToken&& edtkn) noexcept : _data(edtkn._data) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _type = edtkn._type;
#endif
  }

  EdgeDocumentToken(EdgeDocumentToken const& edtkn) noexcept
      : _data(edtkn._data) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _type = edtkn._type;
#endif
  }

  EdgeDocumentToken(DataSourceId const cid,
                    LocalDocumentId const localDocumentId) noexcept
      : _data(cid, localDocumentId) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _type = EdgeDocumentToken::TokenType::LOCAL;
#endif
  }

  explicit EdgeDocumentToken(arangodb::velocypack::Slice const& edge) noexcept
      : _data(edge) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _type = EdgeDocumentToken::TokenType::COORDINATOR;
#endif
  }

  EdgeDocumentToken& operator=(EdgeDocumentToken const& edtkn) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _type = edtkn._type;
#endif
    _data = edtkn._data;
    return *this;
  }

  EdgeDocumentToken& operator=(EdgeDocumentToken&& edtkn) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _type = edtkn._type;
#endif
    _data = edtkn._data;
    return *this;
  }

  DataSourceId cid() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_type == TokenType::LOCAL);
#endif
    TRI_ASSERT(_data.document.cid.isSet());
    return _data.document.cid;
  }

  LocalDocumentId localDocumentId() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_type == TokenType::LOCAL);
#endif
    TRI_ASSERT(_data.document.localDocumentId.isSet());
    return _data.document.localDocumentId;
  }

  uint8_t const* vpack() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_type == TokenType::COORDINATOR);
#endif
    TRI_ASSERT(_data.vpack);
    return _data.vpack;
  }

  bool equalsCoordinator(EdgeDocumentToken const& other) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_type == TokenType::COORDINATOR);
#endif
    return velocypack::Slice(_data.vpack)
        .binaryEquals(velocypack::Slice(other._data.vpack));
  }

  bool equalsLocal(EdgeDocumentToken const& other) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // For local the cid and localDocumentId have illegal values on NONE
    // and can be compared with real values
    TRI_ASSERT(_type == TokenType::LOCAL || _type == TokenType::NONE);
#endif
    return _data.document.cid == other.cid() &&
           _data.document.localDocumentId == other.localDocumentId();
  }

  bool equals(EdgeDocumentToken const& other) const {
    if (ServerState::instance()->isCoordinator()) {
      return equalsCoordinator(other);
    }
    return equalsLocal(other);
  }

  bool isValid() const {
    if (ServerState::instance()->isCoordinator()) {
      return _data.vpack != nullptr;
    }
    return _data.document.cid != DataSourceId::none() &&
           _data.document.localDocumentId != LocalDocumentId::none();
  }

  size_t hash() const {
    if (ServerState::instance()->isCoordinator()) {
      auto vslice = arangodb::velocypack::Slice(vpack());
      return vslice.hash();
    }
    return std::hash<LocalDocumentId>{}(_data.document.localDocumentId) ^
           (_data.document.cid.id() << 1);
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
  union TokenData {
    EdgeDocumentToken::LocalDocument document;
    uint8_t const* vpack;

    TokenData() noexcept { vpack = nullptr; }
    TokenData(velocypack::Slice const& edge) noexcept : vpack(edge.begin()) {
      TRI_ASSERT(!velocypack::Slice(vpack).isExternal());
    }
    TokenData(DataSourceId cid, LocalDocumentId tk) noexcept {
      document.cid = cid;
      document.localDocumentId = tk;
    }
    TokenData(TokenData const& other) noexcept : document(other.document) {}
    TokenData& operator=(TokenData const& other) noexcept {
      document = other.document;
      return *this;
    }

    ~TokenData() = default;
  };

  static_assert(sizeof(TokenData::document) >= sizeof(TokenData::vpack),
                "invalid TokenData struct");

  TokenData _data;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  enum TokenType : uint8_t { NONE, LOCAL, COORDINATOR };
  TokenType _type;
#endif
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
