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

#ifndef ARANGOD_AQL_PROJECTIONS_H
#define ARANGOD_AQL_PROJECTIONS_H 1

#include "Aql/AttributeNamePath.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/DataSourceId.h"

#include <cstdint>
#include <vector>

namespace arangodb {
namespace transaction {
class Methods;
}
namespace velocypack {
class Builder;
class Slice;
}

namespace aql {

class Projections {
 public:

  /// @brief projection for a single top-level attribute or a nested attribute
  struct Projection {
   /// @brief the attribute path
   AttributeNamePath path;
   /// @brief attribute position in a covering index entry. only valid if _supportsCoveringIndex is true
   uint32_t coveringIndexPosition;
   /// @brief attribute type
   AttributeNamePath::Type type;
  };
  
  Projections();

  explicit Projections(std::vector<arangodb::aql::AttributeNamePath> paths);

  Projections(Projections&&) = default;
  Projections& operator=(Projections&&) = default;
  Projections(Projections const&) = default;
  Projections& operator=(Projections const&) = default;

  /// @brief determine the support by the covering indexes
  void determineIndexSupport(DataSourceId const& id, 
                             std::vector<transaction::Methods::IndexHandle> const& indexes);

  /// @brief whether or not the projections are backed by a covering index
  bool supportsCoveringIndex() const noexcept { return _supportsCoveringIndex; }

  /// @brief whether or not there are any projections
  bool empty() const noexcept { return _projections.empty(); }
  
  /// @brief checks if we have a single attribute projection on the attribute
  bool isSingle(std::string const& attribute) const noexcept;
 
  /// @brief extract projections from a full document
  void toVelocyPackFromDocument(arangodb::velocypack::Builder& b, arangodb::velocypack::Slice slice,
                                transaction::Methods const* trxPtr) const;
  
  /// @brief extract projections from a covering index
  void toVelocyPackFromIndex(arangodb::velocypack::Builder& b, arangodb::velocypack::Slice slice,
                             transaction::Methods const* trxPtr) const;
  
  /// @brief serialize the projections to velocypack
  void toVelocyPack(arangodb::velocypack::Builder& b) const;

  /// @brief build projections from velocypack
  static Projections fromVelocyPack(arangodb::velocypack::Slice slice);

 private:
  std::vector<Projection> _projections;
  DataSourceId _datasourceId;
  bool _supportsCoveringIndex;
};

} // namespace aql
} // namespace arangodb

#endif
