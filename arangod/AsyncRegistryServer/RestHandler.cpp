#include "RestHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb::async_registry {

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

auto RestHandler::execute() -> RestStatus {
  VPackBuilder builder;
  builder.add(VPackValue("Hello coroutines"));
  generateResult(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}

}  // namespace arangodb::async_registry
