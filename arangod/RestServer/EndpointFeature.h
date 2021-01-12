////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_HTTP_SERVER_ENDPOINT_FEATURE_H
#define ARANGOD_HTTP_SERVER_ENDPOINT_FEATURE_H 1

#include "ApplicationFeatures/HttpEndpointProvider.h"

#include "Endpoint/EndpointList.h"

namespace arangodb {

class EndpointFeature final : public HttpEndpointProvider {
 public:
  explicit EndpointFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;

 private:
  std::vector<std::string> _endpoints;
  bool _reuseAddress;
  uint64_t _backlogSize;

 public:
  std::vector<std::string> httpEndpoints() override;
  EndpointList const& endpointList() const { return _endpointList; }

 private:
  void buildEndpointLists();

  EndpointList _endpointList;
};

}  // namespace arangodb

#endif