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

#include <chrono>
#include <regex>
#include <thread>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

namespace {

/// @brief name of the feature to report to application server
constexpr auto FeatureName = "Backup";

constexpr auto OperationCreate = "create";
constexpr auto OperationDelete = "delete";
constexpr auto OperationList = "list";
constexpr auto OperationRestore = "restore";
constexpr auto OperationUpload = "upload";
constexpr auto OperationDownload = "download";
std::unordered_set<std::string> const Operations = {OperationCreate, OperationDelete,
                                                    OperationList, OperationRestore,
#ifdef USE_ENTERPRISE
                                                    OperationUpload, OperationDownload,
#endif
                                                    };
/// @brief generic error for if server returns bad/unexpected json
const arangodb::Result ErrorMalformedJsonResponse = {
    TRI_ERROR_INTERNAL, "got malformed JSON response from server"};

/// @brief check whether HTTP response is valid, complete, and not an error
arangodb::Result checkHttpResponse(arangodb::httpclient::SimpleHttpClient& client,
                                   std::unique_ptr<arangodb::httpclient::SimpleHttpResult> const& response) {
  using arangodb::basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: " + client.getErrorMessage()};
  }
  if (response->wasHttpError()) {
    int errorNum = TRI_ERROR_INTERNAL;
    std::string errorMsg = response->getHttpReturnMessage();
    std::shared_ptr<arangodb::velocypack::Builder> bodyBuilder(response->getBodyVelocyPack());
    arangodb::velocypack::Slice error = bodyBuilder->slice();
    if (!error.isNone() && error.hasKey(arangodb::StaticStrings::ErrorMessage)) {
      errorNum = error.get(arangodb::StaticStrings::ErrorNum).getNumericValue<int>();
      errorMsg = error.get(arangodb::StaticStrings::ErrorMessage).copyString();
    }
    return {errorNum, "got invalid response from server: HTTP " +
                          itoa(response->getHttpReturnCode()) + ": " + errorMsg};
  }
  return {TRI_ERROR_NO_ERROR};
}

arangodb::Result getUptime(arangodb::httpclient::SimpleHttpClient& client, double& uptime) {
  arangodb::Result result;

  std::string const url = "/_admin/statistics";
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::GET, url, nullptr, 0));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }

  VPackSlice const resBody = parsedBody->slice();
  if (!resBody.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected reponse to be an object");
    return result;
  }
  TRI_ASSERT(resBody.isObject());

  VPackSlice const serverObject = resBody.get("server");
  if (!serverObject.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'server' to be an object");
    return result;
  }
  TRI_ASSERT(serverObject.isObject());

  VPackSlice const uptimeSlice = serverObject.get("uptime");
  if (!uptimeSlice.isNumber()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'server.uptime' to be numeric");
    return result;
  }
  TRI_ASSERT(uptimeSlice.isNumber());
  uptime = uptimeSlice.getNumericValue<double>();

  return result;
}

arangodb::Result waitForRestart(arangodb::ClientManager& clientManager,
                                double originalUptime, double maxWaitForRestart) {
  arangodb::Result result;

  auto start = std::chrono::high_resolution_clock::now();
  auto timeSinceStart = [start]() -> double {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now - start;
    return (static_cast<double>(duration.count()) /
            decltype(duration)::period::den* decltype(duration)::period::num);
  };

  LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
      << "Waiting for server to restart...";

  // sleep once to allow shutdown to start
  std::this_thread::sleep_for(std::chrono::seconds(1));

  while (timeSinceStart() < maxWaitForRestart) {
    std::unique_ptr<arangodb::httpclient::SimpleHttpClient> client;
    try {
      result = clientManager.getConnectedClient(client, true, false, false, true);
      if (result.ok() && client != nullptr) {
        double uptime = 0.0;
        result = ::getUptime(*client, uptime);
        if (result.ok() && uptime < originalUptime) {
          LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
              << "...server back up and running!";
          return result;
        }
        // server still down, or still up prior to restart; fall through, wait, retry
      }
    } catch (arangodb::basics::Exception const& ex) {
      return {ex.code(), ex.what()};
    } catch (std::exception const& ex) {
      return {TRI_ERROR_INTERNAL, std::string("Caught exception: ") + ex.what()};
    } catch (...) {
      return {TRI_ERROR_INTERNAL, "Caught unknown exception."};
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // okay, we timed out
  result.reset(
      TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
      "Server failed to respond to requests in the expected timeframe.");

  return result;
}

arangodb::Result executeList(arangodb::httpclient::SimpleHttpClient& client,
                             arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;

  std::string const url = "/_admin/backup/list";
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, nullptr, 0));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }

  VPackSlice const resBody = parsedBody->slice();
  if (!resBody.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected response to be an object");
    return result;
  }
  TRI_ASSERT(resBody.isObject());

  VPackSlice const resultObject = resBody.get("result");
  if (!resultObject.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'result' to be an object");
    return result;
  }
  TRI_ASSERT(resultObject.isObject());

  VPackSlice const backups = resultObject.get("id");
  
  if (!backups.isArray()) {
    result.reset(TRI_ERROR_INTERNAL,
                 "expected 'result.hotbackups' to be an array");
    return result;
  }
  TRI_ASSERT(backups.isArray());

  if (backups.isEmptyArray()) {
    LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
        << "There are no backups available.";
  } else {
    LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
        << "The following backups are available:";
    for (auto const& backup : VPackArrayIterator(backups)) {
      TRI_ASSERT(backup.isString());
      LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << " - " << backup.copyString();
    }
  }

  return result;
}

arangodb::Result executeCreate(arangodb::httpclient::SimpleHttpClient& client,
                               arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;


  std::string const url = "/_admin/backup/create";
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("timeout", VPackValue(options.maxWaitForLock));
    bodyBuilder.add("forceBackup", VPackValue(options.force));
    if (!options.label.empty()) {
      bodyBuilder.add("userString", VPackValue(options.label));
    }
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(), body.size()));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }
  VPackSlice const resBody = parsedBody->slice();
  if (!resBody.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected response to be an object");
    return result;
  }
  TRI_ASSERT(resBody.isObject());

  VPackSlice const resultObject = resBody.get("result");
  if (!resultObject.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'result'' to be an object");
    return result;
  }
  TRI_ASSERT(resultObject.isObject());

  VPackSlice const identifier = resultObject.get("id");
  if (!identifier.isString()) {
    result.reset(TRI_ERROR_INTERNAL,
                 "expected 'result.directory' to be a string");
    return result;
  }
  TRI_ASSERT(identifier.isString());

  VPackSlice const forced = resultObject.get("forced");
  if (forced.isTrue()) {
    LOG_TOPIC(WARN, arangodb::Logger::BACKUP)
        << "Failed to get write lock before proceeding with backup. Backup may "
           "contain some inconsistencies.";
  } else if (!forced.isBoolean() && !forced.isNone()) {
    result.reset(TRI_ERROR_INTERNAL,
                 "expected 'result.forced'' to be an boolean");
    return result;
  }

  LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
      << "Backup succeeded. Generated identifier '" << identifier.copyString() << "'";

  return result;
}

arangodb::Result executeRestore(arangodb::httpclient::SimpleHttpClient& client,
                                arangodb::BackupFeature::Options const& options,
                                arangodb::ClientManager& clientManager) {
  arangodb::Result result;

  double originalUptime = 0.0;
  if (options.maxWaitForRestart > 0.0) {
    result = ::getUptime(client, originalUptime);
    if (result.fail()) {
      return result;
    }
  }

  std::string const url = "/_admin/backup/restore";
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("id", VPackValue(options.identifier));
    bodyBuilder.add("saveCurrent", VPackValue(options.saveCurrent));
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(), body.size()));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }

  if (options.saveCurrent) {
    VPackSlice const resBody = parsedBody->slice();
    if (!resBody.isObject()) {
      result.reset(TRI_ERROR_INTERNAL, "expected response to be an object");
      return result;
    }
    TRI_ASSERT(resBody.isObject());

    VPackSlice const resultObject = resBody.get("result");
    if (!resultObject.isObject()) {
      result.reset(TRI_ERROR_INTERNAL, "expected 'result' to be an object");
      return result;
    }
    TRI_ASSERT(resultObject.isObject());

    VPackSlice const previous = resultObject.get("previous");
    if (!previous.isString()) {
      result.reset(TRI_ERROR_INTERNAL, "expected previous to be a string");
      return result;
    }
    TRI_ASSERT(previous.isString());

    LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
        << "current state was saved as backup with identifier '"
        << previous.copyString() << "'";
  }

  LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
      << "Successfully restored '" << options.identifier << "'";

  if (options.maxWaitForRestart > 0.0) {
    result = ::waitForRestart(clientManager, originalUptime, options.maxWaitForRestart);
  }

  return result;
}

arangodb::Result executeDelete(arangodb::httpclient::SimpleHttpClient& client,
                               arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;

  std::string const url = "/_admin/backup/delete";
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("id", VPackValue(options.identifier));
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(),
                     body.size()));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }

  LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
      << "Successfully deleted '" << options.identifier << "'";

  return result;
}

#ifdef USE_ENTERPRISE
struct TransfereType {
  enum type {
    UPLOAD,
    DOWNLOAD
  };

  static std::string asString(type t) {
    switch (t) {
      case UPLOAD:
        return "upload";
      case DOWNLOAD:
        return "download";
      default:
        TRI_ASSERT(false);
    }
  }

  static std::string asAdminPath(type t) {
    switch (t) {
      case UPLOAD:
        return "/_admin/backup/upload";
      case DOWNLOAD:
        return "/_admin/backup/download";
      default:
        TRI_ASSERT(false);
    }
  }

  static std::string asJsonId(type t) {
    switch (t) {
      case UPLOAD:
        return "uploadId";
      case DOWNLOAD:
        return "downloadId";
      default:
        TRI_ASSERT(false);
    }
  }
};

arangodb::Result executeStatusQuery(arangodb::httpclient::SimpleHttpClient& client,
  arangodb::BackupFeature::Options const& options, TransfereType::type type) {
  arangodb::Result result;

  std::string const url = TransfereType::asAdminPath(type);
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add(TransfereType::asJsonId(type), VPackValue(options.statusId));
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(), body.size()));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }
  VPackSlice const resBody = parsedBody->slice();

  VPackSlice const resultObject = resBody.get("result");
  if (!resultObject.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'result' to be an object");
    return result;
  }
  TRI_ASSERT(resultObject.isObject());

  // Display status
  VPackSlice const dbserversObject = resultObject.get("DBServers");

  for (auto const& server : VPackObjectIterator(dbserversObject)) {
    std::string statusMessage = server.key.copyString();

    if (server.value.hasKey("Status")) {
      statusMessage += " Status: " + server.value.get("Status").copyString();
    }
    LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << statusMessage;


    if (server.value.hasKey("Progress")) {
      VPackSlice const progressSlice = server.value.get("Progress");

      LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << "Last progress update " << progressSlice.get("Time").copyString()
        << ": " << progressSlice.get("Done").getInt() << "/" << progressSlice.get("Total").getInt() << " files done";
    }
  }

  return result;
}

arangodb::Result executeInitiateTransfere(arangodb::httpclient::SimpleHttpClient& client,
  arangodb::BackupFeature::Options const& options, TransfereType::type type) {
  arangodb::Result result;

  // Load configuration file
  std::shared_ptr<VPackBuilder> configFile;

  result = arangodb::basics::catchVoidToResult([&options, &configFile](){
    std::string configFileSource = arangodb::basics::FileUtils::slurp(options.rcloneConfigFile);
    auto configFileParsed = arangodb::velocypack::Parser::fromJson(configFileSource);
    configFile.swap(configFileParsed);
  });

  if (result.fail()) {
    return result;
  }

  // Initiate transfere
  std::string const url = TransfereType::asAdminPath(type);
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("id", VPackValue(options.identifier));
    bodyBuilder.add("remoteBackupDir", VPackValue(options.remoteDirectory));
    bodyBuilder.add("config", configFile->slice());
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(), body.size()));
  result = ::checkHttpResponse(client, response);
  if (result.fail()) {
    return result;
  }

  // Print command for checking status
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    result.reset(::ErrorMalformedJsonResponse);
    return result;
  }
  VPackSlice const resBody = parsedBody->slice();

  VPackSlice const resultObject = resBody.get("result");
  if (!resultObject.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'result' to be an object");
    return result;
  }
  TRI_ASSERT(resultObject.isObject());

  std::string transfereId = resultObject.get(TransfereType::asJsonId(type)).copyString();

  LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << "Backup initiated, use ";
  LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << "    arangobackup " << TransfereType::asString(type) << " --status-id=" << transfereId;
  LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << "to query progress.";
  return result;
}

arangodb::Result executeTransfere(arangodb::httpclient::SimpleHttpClient& client,
  arangodb::BackupFeature::Options const& options, TransfereType::type type) {

  if (!options.statusId.empty()) {
    // This is a status query
    return executeStatusQuery(client, options, type);
  } else {
    // This is a transfere
    return executeInitiateTransfere(client, options, type);
  }
}
#endif
}  // namespace

namespace arangodb {

BackupFeature::BackupFeature(application_features::ApplicationServer& server, int& exitCode)
    : ApplicationFeature(server, BackupFeature::featureName()), _clientManager{Logger::BACKUP}, _exitCode{exitCode} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("BasicsPhase");
};

std::string BackupFeature::featureName() { return ::FeatureName; }

std::string BackupFeature::operationList(std::string const& separator) {
  TRI_ASSERT(::Operations.size() > 0);

  // construct an ordered list for sorted output
  std::vector<std::string> operations(::Operations.begin(), ::Operations.end());
  std::sort(operations.begin(), operations.end());

  return basics::StringUtils::join(operations, separator);
}

void BackupFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::options::BooleanParameter;
  using arangodb::options::DiscreteValuesParameter;
  using arangodb::options::DoubleParameter;
  using arangodb::options::Flags;
  using arangodb::options::StringParameter;
  using arangodb::options::UInt32Parameter;
  using arangodb::options::UInt64Parameter;
  using arangodb::options::VectorParameter;

  options->addOption("--operation",
                     "operation to perform (may be specified as positional "
                     "argument without '--operation')",
                     new DiscreteValuesParameter<StringParameter>(&_options.operation, ::Operations),
                     static_cast<std::underlying_type<Flags>::type>(Flags::Hidden));

  options->addOption("--force",
                     "whether to attempt to continue in face of errors (may "
                     "result in inconsistent backup state)",
                     new BooleanParameter(&_options.force));

  options->addOption("--identifier", "a unique identifier for a backup",
                     new StringParameter(&_options.identifier));

  //  options->addOption("--include-search", "whether to include ArangoSearch data",
  //                     new BooleanParameter(&_options.includeSearch));

  options->addOption(
      "--label",
      "an additional label to add to the backup identifier upon creation",
      new StringParameter(&_options.label));

  options->addOption("--max-wait-for-lock",
                     "maximum time to wait (in seconds) to aquire a lock on "
                     "all necessary resources",
                     new DoubleParameter(&_options.maxWaitForLock));

  options->addOption(
      "--max-wait-for-restart",
      "maximum time to wait (in seconds) for the server to restart after a "
      "restore operation before reporting an error; if zero, arangobackup will "
      "not wait to check that the server restarts and will simply return the "
      "result of the restore request",
      new DoubleParameter(&_options.maxWaitForRestart));

  options->addOption("--save-current",
                     "whether to save the current state as a backup before "
                     "restoring to another state",
                     new BooleanParameter(&_options.saveCurrent));
#ifdef USE_ENTERPRISE
  options->addOption("--status-id",
                     "returns the status of a transfere process",
                     new StringParameter(&_options.statusId));

  options->addOption("--rclone-config-file",
                     "filename of the rclone configuration file used for"
                     "file transfere",
                     new StringParameter(&_options.rcloneConfigFile));

  options->addOption("--remote-path",
                     "remote rclone path of directory used to store or "
                     "receive backups",
                     new StringParameter(&_options.remoteDirectory));
#endif
  /*
    options->addSection(
        "remote", "Options detailing a remote connection to use for operations");

    options->addOption("--remote.credentials",
                       "the credentials used for the remote endpoint (see manual "
                       "for more details)",
                       new StringParameter(&_options.credentials));

    options->addOption("--remote.endpoint",
                       "the remote endpoint (see manual for more details)",
                       new StringParameter(&_options.endpoint));
  */
}

void BackupFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    _options.operation = positionals[0];
  } else {
    LOG_TOPIC(FATAL, Logger::BACKUP)
        << "expected exactly one operation, got '"
        << basics::StringUtils::join(positionals, ", ") << "'";
    FATAL_ERROR_EXIT();
  }

  auto const it = ::Operations.find(_options.operation);
  if (it == ::Operations.end()) {
    LOG_TOPIC(FATAL, Logger::BACKUP)
        << "expected operation to be one of: " << operationList(", ");
    FATAL_ERROR_EXIT();
  }

  if (_options.operation == ::OperationCreate) {
    if (!_options.label.empty()) {
      std::regex re = std::regex("^([a-zA-Z0-9\\.\\-_]+)$", std::regex::ECMAScript);
      if (!std::regex_match(_options.label, re)) {
        LOG_TOPIC(FATAL, Logger::BACKUP)
            << "--label value may only contain numbers, letters, periods, "
               "dashes, and underscores";
        FATAL_ERROR_EXIT();
      }
    }

    if (_options.maxWaitForLock < 0.0) {
      LOG_TOPIC(FATAL, Logger::BACKUP)
          << "expected --max-wait-for-lock to be a non-negative number, got '"
          << _options.maxWaitForLock << "'";
      FATAL_ERROR_EXIT();
    }
  }

  if (_options.operation == ::OperationDelete || _options.operation == ::OperationRestore) {
    if (_options.identifier.empty()) {
      LOG_TOPIC(FATAL, Logger::BACKUP)
          << "must specify a backup via --identifier";
      FATAL_ERROR_EXIT();
    }
  }

  if (_options.operation == ::OperationRestore) {
    if (_options.maxWaitForRestart < 0.0) {
      LOG_TOPIC(FATAL, Logger::BACKUP) << "expected --max-wait-for-restart to "
                                          "be a non-negative number, got '"
                                       << _options.maxWaitForRestart << "'";
      FATAL_ERROR_EXIT();
    }
  }
#ifdef USE_ENTERPRISE
  if (_options.operation == ::OperationUpload || _options.operation == ::OperationDownload) {

    if (_options.statusId.empty() == _options.identifier.empty()) {
      // Either both or none are set
      LOG_TOPIC(FATAL, Logger::BACKUP) << "either --status-id or --identifier"
                                          "must be set";
      FATAL_ERROR_EXIT();
    }

    if (!_options.identifier.empty()) {
      if (_options.rcloneConfigFile.empty() || _options.remoteDirectory.empty()) {
        LOG_TOPIC(FATAL, Logger::BACKUP) << "for data transfere --rclone-config-file"
                                            " and --remote-path must be set";
        FATAL_ERROR_EXIT();
      }
    }
  }
#endif
}

void BackupFeature::start() {
  Result result;
  std::unique_ptr<httpclient::SimpleHttpClient> client =
      _clientManager.getConnectedClient(false, true, true);
  if (_options.operation == OperationList) {
    result = ::executeList(*client, _options);
  } else if (_options.operation == OperationCreate) {
    result = ::executeCreate(*client, _options);
  } else if (_options.operation == OperationRestore) {
    result = ::executeRestore(*client, _options, _clientManager);
  } else if (_options.operation == OperationDelete) {
    result = ::executeDelete(*client, _options);
#ifdef USE_ENTERPRISE
  } else if (_options.operation == OperationUpload) {
    result = ::executeTransfere(*client, _options, TransfereType::type::UPLOAD);
  } else if (_options.operation == OperationDownload) {
    result = ::executeTransfere(*client, _options, TransfereType::type::DOWNLOAD);
#endif
  }

  if (result.fail()) {
    LOG_TOPIC(ERR, Logger::BACKUP)
        << "Error during backup operation '" << _options.operation
        << "': " << result.errorMessage();
    //FATAL_ERROR_EXIT();
    _exitCode = EXIT_FAILURE;
  }

  _exitCode = EXIT_SUCCESS;
}

}  // namespace arangodb
