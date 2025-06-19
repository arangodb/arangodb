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
  const std::string queryString = R"(
    LET sampleNum = @sampleNum

    LET docs = (
      FOR d IN @@collection
        SORT RAND()
        LIMIT sampleNum
        RETURN d
    )

    LET total = LENGTH(docs)

    FOR d IN docs
      LET keys = ATTRIBUTES(d)
      FOR key IN keys
        FILTER key != "_rev"
        COLLECT attribute = key
        AGGREGATE
          count = COUNT(d),
          types = UNIQUE(TYPENAME(d[key]))
        RETURN {
          attribute,
          types,
          optional: count < total
        }
    )";

  futures::Future<RestStatus> lookupCollectionSchema(
    std::string const& collection, uint64_t sampleNum);

  RestStatus lookupSchema(uint64_t sampleNum);

  uint64_t validateSampleNum();
};

}  // namespace rest
}  // namespace arangodb