////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_MMFILES_WAL_RECOVERY_FEATURE_H
#define ARANGOD_MMFILES_WAL_RECOVERY_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

namespace options {

class ProgramOptions;

}

class MMFilesWalRecoveryFeature final : public application_features::ApplicationFeature {
  MMFilesWalRecoveryFeature(MMFilesWalRecoveryFeature const&) = delete;
  MMFilesWalRecoveryFeature& operator=(MMFilesWalRecoveryFeature const&) = delete;

 public:
  explicit MMFilesWalRecoveryFeature(
    application_features::ApplicationServer& server
  );
  ~MMFilesWalRecoveryFeature() {}

  void start() override final;
};

}

#endif