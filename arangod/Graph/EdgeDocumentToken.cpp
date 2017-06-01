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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "EdgeDocumentToken.h"

using namespace arangodb;
using namespace arangodb::graph;

EdgeDocumentToken::EdgeDocumentToken() {}
EdgeDocumentToken::~EdgeDocumentToken() {}

/// @brief SingleServer Token for edge documents

SingleServerEdgeDocumentToken::SingleServerEdgeDocumentToken()
    : EdgeDocumentToken(), _cid(0), _token() {}

SingleServerEdgeDocumentToken::SingleServerEdgeDocumentToken(
    TRI_voc_cid_t const cid, DocumentIdentifierToken const token)
    : EdgeDocumentToken(), _cid(cid), _token(token) {}

SingleServerEdgeDocumentToken::~SingleServerEdgeDocumentToken() {}

TRI_voc_cid_t SingleServerEdgeDocumentToken::cid() const {
  TRI_ASSERT(_cid != 0);
  return _cid;
}

DocumentIdentifierToken SingleServerEdgeDocumentToken::token() const {
  TRI_ASSERT(_token._data != 0);
  return _token;
}

bool SingleServerEdgeDocumentToken::equals(EdgeDocumentToken const* other) const {
  auto o = static_cast<SingleServerEdgeDocumentToken const*>(other); 
  // This cast is save because on DBServer and SingleServer we can only create
  // this type of EdgeToken and not on Coordinator
  return _cid == o->_cid && _token == o->_token;
}

ClusterEdgeDocumentToken::ClusterEdgeDocumentToken() : EdgeDocumentToken() {}
ClusterEdgeDocumentToken::ClusterEdgeDocumentToken(StringRef const id) : EdgeDocumentToken() , _id(id) {}

ClusterEdgeDocumentToken::~ClusterEdgeDocumentToken() {}

StringRef ClusterEdgeDocumentToken::id() const {
  TRI_ASSERT(!_id.empty());
  return _id;
}

bool ClusterEdgeDocumentToken::equals(EdgeDocumentToken const* other) const {
    auto o = static_cast<ClusterEdgeDocumentToken const*>(other); 
    // This cast is save because on Coordinator we can only create
    // this type of EdgeToken and not on SingleServer and DBServer
    return id() == o->id();
  }

