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
                    aql::QueryRegistry* queryRegistry);

  RestStatus execute() override;
  RestStatus handleQueryResult() override;

private:
  static const std::string queryString;

  RestStatus lookupSchema(uint64_t, uint64_t);
  RestStatus lookupSchemaCollection(std::string const&, uint64_t, uint64_t);
  RestStatus lookupSchemaGraph();
  RestStatus lookupSchemaView();

  const velocypack::Slice lookupGraph(std::shared_ptr<transaction::StandaloneContext>);

  const velocypack::Slice lookupCollection(std::string const&, uint64_t, uint64_t);

  const velocypack::Slice getCount(std::string const&);
  const velocypack::Slice getExamples(std::string const&, uint64_t);
  uint64_t validateParameter(const std::string& param);
};

}  // namespace rest
}  // namespace arangodb