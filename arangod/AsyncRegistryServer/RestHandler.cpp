#include "RestHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/Registry/registry_variable.h"
#include "Inspection/VPack.h"
#include "Rest/CommonDefines.h"

using namespace arangodb::async_registry;

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

auto RestHandler::execute() -> RestStatus {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  VPackBuilder builder;
  builder.openArray();
  coroutine_registry.for_promise([&](PromiseInList* promise) {
    velocypack::serialize(builder, *promise);
  });
  builder.close();

  generateResult(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}
