////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "UpgradeOptionsProvider.h"

#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/RestartAction.h"

namespace arangodb {

using namespace arangodb::options;

namespace {
int upgradeRestart() {
  unsetenv(StaticStrings::UpgradeEnvName.c_str());
  return 0;
}
}  // namespace

void UpgradeOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    UpgradeFeatureOptions& opts) {
  options->addOldOption("upgrade", "database.auto-upgrade");

  options
      ->addOption("--database.auto-upgrade",
                  "Perform a database upgrade if necessary.",
                  new BooleanParameter(&opts.upgrade))
      .setLongDescription(R"(If you specify this option, then the server
performs a database upgrade instead of starting normally.

A database upgrade first compares the version number stored in the `VERSION`
file in the database directory with the current server version.

If the version number found in the database directory is higher than that of the
server, the server considers this is an unintentional downgrade and warns about
this. Using the server in these conditions is neither recommended nor supported.

If the version number found in the database directory is lower than that of the
server, the server checks whether there are any upgrade tasks to perform.
It then executes all required upgrade tasks and prints the status. If one of the
upgrade tasks fails, the server exits with an error. Re-starting the server with
the upgrade option again triggers the upgrade check and execution until the
problem is fixed.

Whether or not you specify this option, the server always perform a version
check on startup. If you running the server with a non-matching version number
in the `VERSION` file, the server refuses to start.)");

  options->addOption(
      "--database.upgrade-check", "Skip the database upgrade if set to false.",
      new BooleanParameter(&opts.upgradeCheck),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--database.auto-upgrade-full-compaction",
                  "Perform a full RocksDB compaction after database upgrade.",
                  new BooleanParameter(&opts.upgradeFullCompaction))
      .setLongDescription(R"(If this option is specified together with
`--database.auto-upgrade`, the server performs a full RocksDB compaction
after the database upgrade has completed successfully but before shutting down.

This performs a complete compaction of all column families with both
changeLevel and compactBottomMostLevel options enabled, which can help
optimize the database files after an upgrade.

The server will exit with an error code if the compaction fails.)");
}

void UpgradeOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> opts,
    UpgradeFeatureOptions& options) {
  // The following environment variable is another way to run a database
  // upgrade. If the environment variable is set, the system does a database
  // upgrade and then restarts itself without the environment variable.
  // This is used in hotbackup if a restore to a backup happens which is from
  // an older database version. The restore process sets the environment
  // variable at runtime and then does a restore. After the restart (with
  // the old data) the database upgrade is run and another restart is
  // happening afterwards with the environment variable being cleared.
  char* upgrade = getenv(StaticStrings::UpgradeEnvName.c_str());
  if (upgrade != nullptr) {
    options.upgrade = true;
    restartAction = new std::function<int()>();
    *restartAction = upgradeRestart;
    LOG_TOPIC("fdeae", INFO, Logger::STARTUP)
        << "Detected environment variable " << StaticStrings::UpgradeEnvName
        << " with value " << upgrade
        << " will perform database auto-upgrade and immediately restart.";
  }
  if (options.upgrade && !options.upgradeCheck) {
    LOG_TOPIC("47698", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both '--database.auto-upgrade true' and "
           "'--database.upgrade-check false'";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_INVALID_OPTION_VALUE);
  }
  if (options.upgradeFullCompaction && !options.upgrade) {
    LOG_TOPIC("47699", FATAL, arangodb::Logger::ENGINES)
        << "cannot specify '--database.auto-upgrade-full-compaction true' "
           "without '--database.auto-upgrade true'";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_INVALID_OPTION_VALUE);
  }
}

}  // namespace arangodb
