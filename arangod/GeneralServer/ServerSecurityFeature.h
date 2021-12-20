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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class ServerSecurityFeature final : public application_features::ApplicationFeature {
 public:
  explicit ServerSecurityFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  bool isRestApiHardened() const;
  bool isFoxxApiDisabled() const;
  bool isFoxxStoreDisabled() const;
  bool canAccessHardenedApi() const;
  bool foxxAllowInstallFromRemote() const;

 private:
  bool _enableFoxxApi;
  bool _enableFoxxStore;
  bool _hardenedRestApi;
  bool _foxxAllowInstallFromRemote;
};

}  // namespace arangodb

