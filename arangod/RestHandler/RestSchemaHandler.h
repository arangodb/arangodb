//
// Created by koichi on 6/16/25.
//

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
                    GeneralResponse* response, aql::QueryRegistry* queryRegistry);

  RestStatus execute() override;
  RestStatus handleQueryResult() override;

private:
  static constexpr uint64_t defaultSampleNum = 100;
  static constexpr uint64_t defaultExampleNum = 1;
  static const std::string queryString;
  graph::GraphManager _graphManager;

  RestStatus lookupSchema(uint64_t sampleNum, uint64_t exampleNum);
  RestStatus lookupSchemaCollection(
    std::string const& colName, uint64_t sampleNum, uint64_t exampleNum);
  RestStatus lookupSchemaGraph(std::string const& graphName,
    uint64_t sampleNum, uint64_t exampleNum);
  RestStatus lookupSchemaView();

  bool getCollection(std::string const& colName,
    uint64_t sampleNum, uint64_t exampleNum, velocypack::Builder& colBuilder);
  bool getGraphAndCollections(std::string const& graphName,
    velocypack::Builder& graphBuilder, std::set<std::string>& colSet);
  bool getAllGraphsAndCollections(velocypack::Builder& graphBuilder,
    std::set<std::string>& colSet);

  std::optional<uint64_t> validateParameter(const std::string& param,
    uint64_t defaultValue, bool allowZero=false);
  bool getNumOfDocumentsOrEdges(std::string const& colName,
    velocypack::Builder& builder, bool isDocument=true);
  bool getExamples(std::string const& colName,
    uint64_t exampleNum, velocypack::Builder& builder);
  bool getConnectedCollections(std::string const& graphName,
    std::set<std::string>& colSet);
};

}  // namespace rest
}  // namespace arangodb