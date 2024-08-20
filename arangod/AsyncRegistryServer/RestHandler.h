#pragma once

#include "AsyncRegistryServer/Feature.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb::async_registry {

class RestHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  RestHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override final { return "CoroutineRestHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

  Feature& _feature;
};

}  // namespace arangodb::async_registry
