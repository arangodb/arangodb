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

#ifndef ARANGOD_EDGE_COLLECTION_INFO_H
#define ARANGOD_EDGE_COLLECTION_INFO_H 1

#include "Aql/Graphs.h"
#include "Graph/Traverser.h"
#include "Transaction/Methods.h"

namespace arangodb {
namespace traverser {

////////////////////////////////////////////////////////////////////////////////
/// @brief Information required internally of the traverser.
///        Used to easily pass around collections.
///        Also offer abstraction to extract edges.
////////////////////////////////////////////////////////////////////////////////

class EdgeCollectionInfo {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying transaction
  //////////////////////////////////////////////////////////////////////////////

  transaction::Methods* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge collection name
  //////////////////////////////////////////////////////////////////////////////

  std::string _collectionName;

  /// @brief index used for forward iteration
  transaction::Methods::IndexHandle _forwardIndexId;

  /// @brief index used for backward iteration
  transaction::Methods::IndexHandle _backwardIndexId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Temporary builder for index search values
  ///        NOTE: Single search builder is NOT thread-save
  //////////////////////////////////////////////////////////////////////////////

  aql::EdgeConditionBuilderContainer _searchBuilder;

  std::string _weightAttribute;

  TRI_edge_direction_e _dir;

 public:
  EdgeCollectionInfo(transaction::Methods* trx, std::string const& collectionName,
                     TRI_edge_direction_e const direction,
                     std::string const& weightAttribute, double defaultWeight);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get edges for the given direction and start vertex.
  ////////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<arangodb::OperationCursor> getEdges(std::string const&);

  transaction::Methods* trx() const { return _trx; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Return name of the wrapped collection
  ////////////////////////////////////////////////////////////////////////////////

  std::string const& getName() const;
};

}  // namespace traverser
}  // namespace arangodb

#endif
