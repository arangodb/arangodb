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

#include "Backup/BackupOptionsProvider.h"

#include <algorithm>
#include <regex>
#include <unordered_set>
#include <vector>

#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace {

constexpr auto OperationCreate = "create";
constexpr auto OperationDelete = "delete";
constexpr auto OperationRestore = "restore";
#ifdef USE_ENTERPRISE
constexpr auto OperationUpload = "upload";
constexpr auto OperationDownload = "download";
#endif

std::unordered_set<std::string> const Operations = {
    OperationCreate, OperationDelete,   "list", OperationRestore,
#ifdef USE_ENTERPRISE
    OperationUpload, OperationDownload,
#endif
};

std::string operationList(std::string const& separator) {
  std::vector<std::string> operations(Operations.begin(), Operations.end());
  std::sort(operations.begin(), operations.end());
  return arangodb::basics::StringUtils::join(operations, separator);
}

}  // namespace

namespace arangodb {

using namespace arangodb::options;

void BackupOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, BackupFeatureOptions& options) {
  opts->addOption(
      "--operation",
      "The operation to perform (may be specified as positional "
      "argument without '--operation').",
      new DiscreteValuesParameter<StringParameter>(&options.operation,
                                                   ::Operations),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption("--allow-inconsistent",
                  "Whether to attempt to continue in face of errors; "
                  "may result in inconsistent backup state (create operation).",
                  new BooleanParameter(&options.allowInconsistent));

  opts->addOption(
      "--ignore-version",
      "Ignore stored version of a backup. "
      "Restore may not work if versions mismatch (restore operation).",
      new BooleanParameter(&options.ignoreVersion));

  opts->addOption("--identifier",
                  "a unique identifier for a backup "
                  "(restore/upload/download operation)",
                  new StringParameter(&options.identifier));

  opts->addOption(
      "--label",
      "An additional label to add to the backup identifier (create operation)-",
      new StringParameter(&options.label));

  opts->addOption("--max-wait-for-lock",
                  "The maximum time to wait (in seconds) to acquire a lock "
                  "on all necessary resources (create operation).",
                  new DoubleParameter(&options.maxWaitForLock));

  opts->addOption(
      "--max-wait-for-restart",
      "The maximum time to wait (in seconds) for the server to restart after a "
      "restore operation before reporting an error; if zero, arangobackup does "
      "not wait to check that the server restarts and simply returns the "
      "result of the restore request (restore operation).",
      new DoubleParameter(&options.maxWaitForRestart));

#ifdef USE_ENTERPRISE
  opts->addOption(
      "--status-id",
      "Return the status of a transfer process "
      "(upload/download operation).",
      new StringParameter(&options.statusId),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise,
                                          arangodb::options::Flags::Command));

  opts->addOption("--rclone-config-file",
                  "A path to the rclone configuration file to use for"
                  "file transfer (upload/download operation).",
                  new StringParameter(&options.rcloneConfigFile),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Enterprise));

  opts->addOption("--remote-path",
                  "The remote rclone path of a directory to use to store or "
                  "receive backups (upload/download operation).",
                  new StringParameter(&options.remoteDirectory),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Enterprise));

  opts->addOption(
      "--abort",
      "Abort the transfer with the given status-id "
      "(upload/download operation).",
      new BooleanParameter(&options.abort),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise,
                                          arangodb::options::Flags::Command));

  opts->addOption(
      "--force",
      "Abort transactions if needed to ensure a consistent snapshot. "
      "This option can destroy the atomicity of your transactions in the "
      "presence of intermediate commits! Use it with great care and only "
      "if you really need a consistent backup at all costs (create operation).",
      new BooleanParameter(&options.abortTransactionsIfNeeded),
      arangodb::options::makeDefaultFlags(
          arangodb::options::Flags::Enterprise));
#endif
}

void BackupOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, BackupFeatureOptions& options) {
  auto const& positionals = opts->processingResult()._positionals;

  if (opts->processingResult().touched("--version") ||
      opts->processingResult().touched("--version-json")) {
    return;
  }

  if (1 == positionals.size()) {
    options.operation = positionals[0];
  } else {
    LOG_TOPIC("48e32", FATAL, Logger::BACKUP)
        << "expected exactly one operation of "
           "create|delete|download|list|restore|upload, got '"
        << basics::StringUtils::join(positionals, ", ") << "'";
    FATAL_ERROR_EXIT();
  }

  auto const it = ::Operations.find(options.operation);
  if (it == ::Operations.end()) {
    LOG_TOPIC("138ed", FATAL, Logger::BACKUP)
        << "expected operation to be one of: " << ::operationList(", ");
    FATAL_ERROR_EXIT();
  }

  if (options.operation == ::OperationCreate) {
    if (!options.label.empty()) {
      std::regex re =
          std::regex("^([a-zA-Z0-9\\._\\-]+)$", std::regex::ECMAScript);
      if (!std::regex_match(options.label, re)) {
        LOG_TOPIC("7829b", FATAL, Logger::BACKUP)
            << "--label value may only contain numbers, letters, periods, "
               "dashes, and underscores";
        FATAL_ERROR_EXIT();
      }
    }

    if (options.maxWaitForLock < 0.0) {
      LOG_TOPIC("6caeb", FATAL, Logger::BACKUP)
          << "expected --max-wait-for-lock to be a non-negative number, got '"
          << options.maxWaitForLock << "'";
      FATAL_ERROR_EXIT();
    }
  }

  if (options.operation == ::OperationDelete ||
      options.operation == ::OperationRestore) {
    if (options.identifier.empty()) {
      LOG_TOPIC("e83ef", FATAL, Logger::BACKUP)
          << "must specify a backup via --identifier";
      FATAL_ERROR_EXIT();
    }
  }

  if (options.operation == ::OperationRestore) {
    if (options.maxWaitForRestart < 0.0) {
      LOG_TOPIC("efa20", FATAL, Logger::BACKUP)
          << "expected --max-wait-for-restart to "
             "be a non-negative number, got '"
          << options.maxWaitForRestart << "'";
      FATAL_ERROR_EXIT();
    }
  }
#ifdef USE_ENTERPRISE
  if (options.operation == ::OperationUpload ||
      options.operation == ::OperationDownload) {
    if (options.statusId.empty() == options.identifier.empty()) {
      LOG_TOPIC("2d0fa", FATAL, Logger::BACKUP)
          << "either --status-id or --identifier"
             " must be set";
      FATAL_ERROR_EXIT();
    }

    if (options.abort == true &&
        (options.statusId.empty() || !options.identifier.empty())) {
      LOG_TOPIC("62375", FATAL, Logger::BACKUP)
          << "--abort true expects --status-id to be set";
      FATAL_ERROR_EXIT();
    }

    if (!options.identifier.empty()) {
      if (options.rcloneConfigFile.empty() || options.remoteDirectory.empty()) {
        LOG_TOPIC("6063d", FATAL, Logger::BACKUP)
            << "for data transfer --rclone-config-file"
               " and --remote-path must be set";
        FATAL_ERROR_EXIT();
      }
    }
  }
#endif
}

}  // namespace arangodb
