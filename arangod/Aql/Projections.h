////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include <unordered_set>
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

/// @brief helper class to manage projections and extract attribute data from
/// documents and index entries
class Projections {
 public:
  /// @brief projection for a single top-level attribute or a nested attribute
  struct Projection {
   /// @brief the attribute path
   AttributeNamePath path;
   /// @brief attribute position in a covering index entry. only valid if _supportsCoveringIndex is true
   uint16_t coveringIndexPosition;
   /// @brief attribute length in a covering index entry. this can be shorter than the projection
   uint16_t coveringIndexCutoff;
   /// @brief attribute type
   AttributeNamePath::Type type;
  };
  
  Projections();

  /// @brief create projections from the vector of attributes passed.
  /// attributes will be sorted and made unique inside
  explicit Projections(std::vector<arangodb::aql::AttributeNamePath> paths);
  explicit Projections(std::unordered_set<arangodb::aql::AttributeNamePath> const& paths);

  Projections(Projections&&) = default;
  Projections& operator=(Projections&&) = default;
  Projections(Projections const&) = default;
  Projections& operator=(Projections const&) = default;

  /// @brief determine if there is covering support by indexes passed
  void determineIndexSupport(DataSourceId const& id, 
                             std::vector<transaction::Methods::IndexHandle> const& indexes);

  /// @brief whether or not the projections are backed by a covering index
  bool supportsCoveringIndex() const noexcept { return _supportsCoveringIndex; }

  /// @brief whether or not there are any projections
  bool empty() const noexcept { return _projections.empty(); }
  
  /// @brief number of projections
  size_t size() const noexcept { return _projections.size(); }
  
  /// @brief checks if we have a single attribute projection on the attribute
  bool isSingle(std::string const& attribute) const noexcept;
  
  /// @brief get projection at position
  Projection const& operator[](size_t index) const;
  
  /// @brief get projection at position
  Projection& operator[](size_t index);
 
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
  /// @brief shared init function
  void init();

  /// @brief clean up projections, so that there are no 2 projections with a
  /// shared prefix
  void removeSharedPrefixes();

 private:
  /// @brief all our projections (sorted, unique)
  std::vector<Projection> _projections;

  /// @brief collection data source id (in case _id is queries and we need to resolve
  /// the collection name)
  DataSourceId _datasourceId;

  /// @brief whether or not the projections are backed by a covering index
  bool _supportsCoveringIndex;
};

} // namespace aql
} // namespace arangodb

#endif
