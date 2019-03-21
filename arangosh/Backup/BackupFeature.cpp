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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "BackupFeature.h"

#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"

namespace {

/// @brief name of the feature to report to application server
constexpr auto FeatureName = "Backup";

}  // namespace

namespace arangodb {

BackupFeature::BackupFeature(application_features::ApplicationServer& server, int& exitCode)
    : ApplicationFeature(server, BackupFeature::featureName()), _clientManager{Logger::BACKUP}, _exitCode{exitCode} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("BasicsPhase");
};

std::string BackupFeature::featureName() { return ::FeatureName; }

void BackupFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::options::BooleanParameter;
  using arangodb::options::StringParameter;
  using arangodb::options::UInt32Parameter;
  using arangodb::options::UInt64Parameter;
  using arangodb::options::VectorParameter;
}

void BackupFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {}

void BackupFeature::start() { _exitCode = EXIT_SUCCESS; }

}  // namespace arangodb
