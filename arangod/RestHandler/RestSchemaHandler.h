////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/GraphManager.h"
#include "RestHandler/RestCursorHandler.h"
#include "Utils/CollectionNameResolver.h"

#include <set>

namespace arangodb {
namespace aql {
class QueryRegistry;
}
namespace rest {

class RestSchemaHandler : public RestCursorHandler {
 public:
  RestSchemaHandler(ArangodServer& server, GeneralRequest* request,
                    GeneralResponse* response,
                    aql::QueryRegistry* queryRegistry);

  RestStatus execute() override;
  RestStatus handleQueryResult() override;

 private:
  static constexpr uint64_t defaultSampleNum = 100;
  static constexpr uint64_t defaultExampleNum = 1;
  static const std::string _queryStr;
  graph::GraphManager _graphManager;
  CollectionNameResolver _nameResolver;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called by /_api/schema to show graphs, views and collections
  //////////////////////////////////////////////////////////////////////////////
  Result lookupSchema(uint64_t sampleNum, uint64_t exampleNum);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called by /_api/schema/collection/<collection-name> to show the
  /// collection and its index and schemas
  //////////////////////////////////////////////////////////////////////////////
  Result lookupSchemaCollection(std::string const& colName, uint64_t sampleNum,
                                uint64_t exampleNum);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called by /_api/schema/graph/<graph-name> to show the graph and
  /// connected collections
  //////////////////////////////////////////////////////////////////////////////
  Result lookupSchemaGraph(std::string const& graphName, uint64_t sampleNum,
                           uint64_t exampleNum);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called by /_api/schema/view/<view-name> to show the view and
  /// linked collections
  //////////////////////////////////////////////////////////////////////////////
  Result lookupSchemaView(std::string const& viewName, uint64_t sampleNum,
                          uint64_t exampleNum);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to build VPack for one collection's indexes,
  /// schemas, and examples
  //////////////////////////////////////////////////////////////////////////////
  Result getCollection(std::string const& colName, uint64_t sampleNum,
                       uint64_t exampleNum, velocypack::Builder& colBuilder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to iterate given collection set to build VPack by
  /// calling getCollection() method
  //////////////////////////////////////////////////////////////////////////////
  Result getAllCollections(std::set<std::string> const& colSet,
                           uint64_t sampleNum, uint64_t exampleNum,
                           velocypack::Builder& colsBuilder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to build VPack for a graph obj and record its
  /// connected collections to set
  //////////////////////////////////////////////////////////////////////////////
  Result getGraphAndCollections(graph::Graph const& graph,
                                velocypack::Builder& graphBuilder,
                                std::set<std::string>& colSet);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to iterate all graphs to build VPack and record
  /// their connected collections by calling getGraphAndCollections() method
  //////////////////////////////////////////////////////////////////////////////
  Result getAllGraphsAndCollections(velocypack::Builder& graphBuilder,
                                    std::set<std::string>& colSet);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to build VPack for a view and record its linked
  /// collections
  //////////////////////////////////////////////////////////////////////////////
  Result getViewAndCollections(std::string const& viewName,
                               velocypack::Builder& colBuilder,
                               std::set<std::string>& colSet);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to iterate all views to build VPack and record
  /// their linked collections by calling getViewAndCollections() method
  //////////////////////////////////////////////////////////////////////////////
  Result getAllViewsAndCollections(velocypack::Builder& viewsBuilder,
                                   std::set<std::string>& colSet);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to fetch all built indexes for a collection and
  /// appends them to VPack
  //////////////////////////////////////////////////////////////////////////////
  Result getIndexes(LogicalCollection const& col, velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper method to parse, validate and default numeric query params
  //////////////////////////////////////////////////////////////////////////////
  ResultT<uint64_t> validateParameter(const std::string& param,
                                      uint64_t defaultValue,
                                      bool allowZero = false);
};

}  // namespace rest
}  // namespace arangodb