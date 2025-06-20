//
// Created by koichi on 6/16/25.
//

#pragma once

#include "RestHandler/RestCursorHandler.h"
#include "Transaction/StandaloneContext.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}
namespace rest {

class RestSchemaHandler : public RestCursorHandler {
public:
  RestSchemaHandler(ArangodServer& server,
                    GeneralRequest* request,
                    GeneralResponse* response,
                    arangodb::aql::QueryRegistry* queryRegistry);

  RestStatus execute() override;
  RestStatus handleQueryResult() override;

private:
  static const std::string queryString;
  static const std::string graphQueryString;

  futures::Future<RestStatus> lookupCollectionSchema(
    std::string const& collection, uint64_t sampleNum);

  RestStatus lookupSchema(uint64_t sampleNum);

  const velocypack::Slice lookupGraph(std::shared_ptr<transaction::StandaloneContext>);

  uint64_t validateSampleNum();
};

}  // namespace rest
}  // namespace arangodb