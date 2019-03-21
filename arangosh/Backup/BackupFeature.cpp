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

#include "Basics/StringUtils.h"
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
  using arangodb::options::DiscreteValuesParameter;
  using arangodb::options::DoubleParameter;
  using arangodb::options::StringParameter;
  using arangodb::options::UInt32Parameter;
  using arangodb::options::UInt64Parameter;
  using arangodb::options::VectorParameter;

  std::unordered_set<std::string> operations = {"list",   "download", "upload",
                                                "create", "restore",  "delete"};
  options->addOption("--operation",
                     "operation to perform (may be specified as positional "
                     "argument without '--operation')",
                     new DiscreteValuesParameter<StringParameter>(&_options.operation, operations));

  options->addOption("--force",
                     "whether to attempt to continue in face of errors (may "
                     "result in inconsistent backup state)",
                     new BooleanParameter(&_options.force));

  options->addOption("--include-search", "whether to include ArangoSearch data",
                     new BooleanParameter(&_options.includeSearch));

  options->addOption("--label",
                     "an additional label to add to the backup directory",
                     new StringParameter(&_options.label));

  options->addOption("name", "an unique name used to identify a backup",
                     new StringParameter(&_options.name));

  options->addOption("--max-wait-time",
                     "maximum time to wait (in seconds) to start operation",
                     new DoubleParameter(&_options.maxWaitTime));

  options->addOption("--timestamp",
                     "a unique timestamp used to identify a backup",
                     new StringParameter(&_options.timestamp));

  options->addSection(
      "remote", "Options detailing a remote connection to use for operations");

  options->addOption("--remote.credentials",
                     "the credentials used for the remote endpoint (see manual "
                     "for more details)",
                     new StringParameter(&_options.credentials));

  options->addOption("--remote.endpoint",
                     "the remote endpoint (see manual for more details)",
                     new StringParameter(&_options.endpoint));
}

void BackupFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    _options.operation = positionals[0];
  } else if (1 < positionals.size()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expected at most one operation, got '"
        << basics::StringUtils::join(positionals, ",") << "'";
    FATAL_ERROR_EXIT();
  }
}

void BackupFeature::start() { _exitCode = EXIT_SUCCESS; }

}  // namespace arangodb
