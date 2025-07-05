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

#include "RestHandler/RestCursorHandler.h"
#include "Graph/GraphManager.h"

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
  static const std::string queryString;
  graph::GraphManager _graphManager;

  Result lookupSchema(uint64_t sampleNum, uint64_t exampleNum);
  Result lookupSchemaCollection(std::string const& colName, uint64_t sampleNum,
                                uint64_t exampleNum);
  Result lookupSchemaGraph(std::string const& graphName, uint64_t sampleNum,
                           uint64_t exampleNum);
  Result lookupSchemaView(std::string const& viewName, uint64_t sampleNum,
                          uint64_t exampleNum);

  Result getCollections(std::set<std::string> const& colSet, uint64_t sampleNum,
                        uint64_t exampleNum, velocypack::Builder& colsBuilder);
  Result getCollection(std::string const& colName, uint64_t sampleNum,
                       uint64_t exampleNum, velocypack::Builder& colBuilder);
  Result getGraphAndCollections(std::string const& graphName,
                                velocypack::Builder& graphBuilder,
                                std::set<std::string>& colSet);
  Result getAllGraphsAndCollections(velocypack::Builder& graphBuilder,
                                    std::set<std::string>& colSet);
  Result getViewAndCollections(std::string const& viewName,
                               velocypack::Builder& colBuilder,
                               std::set<std::string>& colSet);
  Result getAllViewsAndCollections(velocypack::Builder& viewsBuilder,
                                   std::set<std::string>& colSet);

  std::optional<uint64_t> validateParameter(const std::string& param,
                                            uint64_t defaultValue,
                                            bool allowZero = false);
  Result getNumOfDocumentsOrEdges(std::string const& colName,
                                  velocypack::Builder& builder,
                                  bool isDocument = true);
  Result getExamples(std::string const& colName, uint64_t exampleNum,
                     velocypack::Builder& builder);
  Result getConnectedCollections(std::string const& graphName,
                                 std::set<std::string>& colSet);
};

}  // namespace rest
}  // namespace arangodb