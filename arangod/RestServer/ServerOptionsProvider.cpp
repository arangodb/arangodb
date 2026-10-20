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

#include "ServerOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void ServerOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    ServerFeatureOptions& opts) {
  options
      ->addOption("--console",
                  "Start the server with a JavaScript emergency console.",
                  new BooleanParameter(&opts.console))
      .setLongDescription(R"(In this exclusive emergency mode, all networking
and HTTP interfaces of the server are disabled. No requests can be made to the
server in this mode, and the only way to work with the server in this mode is by
using the emergency console.

The server cannot be started in this mode if it is already running in this or
another mode.)");

  options->addSection("server", "server features");

  options->addOption(
      "--server.rest-server", "Start a REST server.",
      new BooleanParameter(&opts.restServer),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--server.validate-utf8-strings",
      "Perform UTF-8 string validation for incoming JSON and VelocyPack "
      "data.",
      new BooleanParameter(&opts.validateUtf8Strings),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption("--javascript.script", "Run the script and exit.",
                     new VectorParameter<StringParameter>(&opts.scripts));

  // add obsolete MMFiles WAL options (obsoleted in 3.7)
  options->addSection("wal", "WAL of the MMFiles engine", "", true, true);
  options->addObsoleteOption(
      "--wal.allow-oversize-entries",
      "allow entries that are bigger than '--wal.logfile-size'", false);
  options->addObsoleteOption("--wal.use-mlock",
                             "mlock WAL logfiles in memory (may require "
                             "elevated privileges or limits)",
                             false);
  options->addObsoleteOption("--wal.directory", "logfile directory", true);
  options->addObsoleteOption(
      "--wal.historic-logfiles",
      "maximum number of historic logfiles to keep after collection", true);
  options->addObsoleteOption(
      "--wal.ignore-logfile-errors",
      "ignore logfile errors. this will read recoverable data from corrupted "
      "logfiles but ignore any unrecoverable data",
      false);
  options->addObsoleteOption(
      "--wal.ignore-recovery-errors",
      "continue recovery even if re-applying operations fails", false);
  options->addObsoleteOption("--wal.flush-timeout",
                             "flush timeout (in milliseconds)", true);
  options->addObsoleteOption("--wal.logfile-size",
                             "size of each logfile (in bytes)", true);
  options->addObsoleteOption("--wal.open-logfiles",
                             "maximum number of parallel open logfiles", true);
  options->addObsoleteOption("--wal.reserve-logfiles",
                             "maximum number of reserve logfiles to maintain",
                             true);
  options->addObsoleteOption("--wal.slots", "number of logfile slots to use",
                             true);
  options->addObsoleteOption(
      "--wal.sync-interval",
      "interval for automatic, non-requested disk syncs (in milliseconds)",
      true);
  options->addObsoleteOption(
      "--wal.throttle-when-pending",
      "throttle writes when at least this many operations are waiting for "
      "collection (set to 0 to deactivate write-throttling)",
      true);
  options->addObsoleteOption(
      "--wal.throttle-wait",
      "maximum wait time per operation when write-throttled (in milliseconds)",
      true);
}

void ServerOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> /*options*/,
    ServerFeatureOptions& opts) {
  int count = 0;

  if (opts.console) {
    opts.operationMode = OperationMode::MODE_CONSOLE;
    ++count;
  }

  if (!opts.scripts.empty()) {
    opts.operationMode = OperationMode::MODE_SCRIPT;
    ++count;
  }

  if (1 < count) {
    LOG_TOPIC("353cd", FATAL, arangodb::Logger::FIXME)
        << "cannot combine '--console', '--javascript.unit-tests' and "
        << "'--javascript.script'";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
