#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb::coroutine {

class Feature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() { return "Coroutines"; }

  template<typename Server>
  Feature(Server& server)
      : application_features::ApplicationFeature{
            server, Server::template id<Feature>(), name()} {
    // startsAfter<Bla, Server>();
  }
};

}  // namespace arangodb::coroutine
