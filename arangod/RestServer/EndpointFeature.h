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

#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "RestServer/arangod.h"

#include "Endpoint/EndpointList.h"

namespace arangodb {

class EndpointFeature final : public HttpEndpointProvider {
 public:
  static constexpr std::string_view name() noexcept { return "Endpoint"; }

  explicit EndpointFeature(ArangodServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

  std::vector<std::string> httpEndpoints() override;
  EndpointList& endpointList() { return _endpointList; }
  EndpointList const& endpointList() const { return _endpointList; }

 private:
  void buildEndpointLists();

  std::vector<std::string> _endpoints;
  bool _reuseAddress;
  uint64_t _backlogSize;

  EndpointList _endpointList;
};

}  // namespace arangodb
