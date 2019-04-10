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

#ifndef ARANGODB_APPLICATION_FEATURES_SERVER_SECURITY_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_SERVER_SECURITY_FEATURE_H 1

#include <regex>
#include "ApplicationFeatures/ApplicationFeature.h"

namespace v8 {
class Isolate;
}

namespace arangodb {

class ServerSecurityFeature final : public application_features::ApplicationFeature {
 public:
  explicit ServerSecurityFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

  bool isDeniedHardenedApi(v8::Isolate* isolate) const;
  bool disableFoxxApi(v8::Isolate* isolate) const;
  bool disableFoxxStore(v8::Isolate* isolate) const;
  bool isInternalContext(v8::Isolate* isolate) const;

 private:
  bool _enableFoxxApi;
  bool _enableFoxxStore;
  bool _denyHardenedApi;
};

}  // namespace arangodb

#endif
