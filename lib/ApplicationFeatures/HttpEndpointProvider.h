////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class HttpEndpointProvider : public application_features::ApplicationFeature {
 public:
  virtual ~HttpEndpointProvider() = default;
  virtual std::vector<std::string> httpEndpoints() = 0;

 protected:
  // cppcheck-suppress virtualCallInConstructor
  template<typename Server, typename Impl>
  // cppcheck-suppress virtualCallInConstructor
  HttpEndpointProvider(Server& server, const Impl&)
  // cppcheck-suppress virtualCallInConstructor
      : ApplicationFeature(server, Server::template id<HttpEndpointProvider>(),
                           Impl::name()) {}

  HttpEndpointProvider(application_features::ApplicationServer& server,
                       size_t registration, std::string_view name)
      : application_features::ApplicationFeature(server, registration, name){};
};
}  // namespace arangodb
