//
// Created by koichi on 6/16/25.
//

#pragma once

#include "RestHandler/RestCursorHandler.h"

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
  /// Builds and registers the AQL to infer the schema for `collection`
  futures::Future<RestStatus> lookupSchema(std::string const& collection,
                                           int64_t sampleNum);
};

}  // namespace rest
}  // namespace arangodb