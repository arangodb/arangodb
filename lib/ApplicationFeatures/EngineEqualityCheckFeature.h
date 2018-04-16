////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_APPLICATION_FEATURES_ENGINE_EQUALITY_CHECK_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_ENGINE_EQUALITY_CHECK_FEATURE_H

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class EngineEqualityCheckFeature final : public application_features::ApplicationFeature {
 public:
  explicit EngineEqualityCheckFeature(application_features::ApplicationServer* server);

 public:
  void start() override final;
};
}

#endif
