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

#ifndef ARANGOD_GRAPH_EDGEDOCUMENTTOKEN_H
#define ARANGOD_GRAPH_EDGEDOCUMENTTOKEN_H 1

#include "Basics/Common.h"
#include "Basics/StringRef.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "VocBase/voc-types.h"

namespace arangodb {

namespace graph {

/// @brief Pure virtual abstract class to uniquely identify an edge
///        Has specific implementations for Cluster adn SingleServer

struct EdgeDocumentToken {
 public:
  EdgeDocumentToken();

  virtual ~EdgeDocumentToken();

  virtual bool equals(EdgeDocumentToken const* other) const = 0;

};

/// @brief SingleServer identifier for edge documents
///        Contains information about collection as
///        well as the document token.

struct SingleServerEdgeDocumentToken : public EdgeDocumentToken {

 private:
  TRI_voc_cid_t const _cid;
  DocumentIdentifierToken const _token;

 public:

  SingleServerEdgeDocumentToken();

  SingleServerEdgeDocumentToken(TRI_voc_cid_t const cid,
                                DocumentIdentifierToken const token);

  ~SingleServerEdgeDocumentToken();

  TRI_voc_cid_t cid() const;

  DocumentIdentifierToken token() const;

  bool equals(EdgeDocumentToken const* other) const override;
};

/// @brief Cluster identifier for Documents
///        Contains full _id string as StringRef

struct ClusterEdgeDocumentToken : public EdgeDocumentToken {

 private:
  StringRef const _id;

 public:

  ClusterEdgeDocumentToken();

  explicit ClusterEdgeDocumentToken(StringRef const id);

  ~ClusterEdgeDocumentToken();

  StringRef id() const;

  bool equals(EdgeDocumentToken const* other) const override;
};

}
}

/*
inline bool operator==(arangodb::graph::EdgeDocumentToken const& lhs,
                         arangodb::graph::EdgeDocumentToken const& rhs) {
  return rhs.token() == lhs.token() && rhs.cid() == lhs.cid();
}

inline bool operator!=(arangodb::graph::EdgeDocumentToken const& lhs,
                       arangodb::graph::EdgeDocumentToken const& rhs) {
  return !(lhs == rhs);
}

namespace std {

template <>
struct hash<arangodb::graph::EdgeDocumentToken> {
  size_t operator()(arangodb::graph::EdgeDocumentToken const& value) const
      noexcept {
    return static_cast<size_t>(value.token()._data ^ value.cid());
  }
};

template <>
struct equal_to<arangodb::graph::EdgeDocumentToken> {
  bool operator()(arangodb::graph::EdgeDocumentToken const& lhs,
                  arangodb::graph::EdgeDocumentToken const& rhs) const
      noexcept {
    return lhs.token() == rhs.token() && lhs.cid() == rhs.cid();
  }
};

}  // namespace std

*/
#endif
