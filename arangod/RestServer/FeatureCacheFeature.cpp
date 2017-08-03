////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "FeatureCacheFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;

FeatureCacheFeature* FeatureCacheFeature::Instance = nullptr;

FeatureCacheFeature::FeatureCacheFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "FeatureCache"),
      _authenticationFeature(nullptr),
      _databaseFeature(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);

  // reset it so it can be used in multiple tests
  Instance = nullptr;
}

FeatureCacheFeature::~FeatureCacheFeature() {
  // reset it so it can be used in multiple tests
  Instance = nullptr;
}

void FeatureCacheFeature::prepare() {
  _authenticationFeature = application_features::ApplicationServer::getFeature<AuthenticationFeature>(
          "Authentication");
  _databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");
 
  TRI_ASSERT(Instance == nullptr);
  Instance = this;
}

void FeatureCacheFeature::unprepare() {
  _authenticationFeature = nullptr;
  _databaseFeature = nullptr;
  // do not reset instance
}
