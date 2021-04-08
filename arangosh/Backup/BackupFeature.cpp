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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "BackupFeature.h"

#include <chrono>
#include <regex>
#include <thread>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/HotBackupCommon.h"

namespace {

/// @brief name of the feature to report to application server
constexpr auto FeatureName = "Backup";

constexpr auto OperationCreate = "create";
constexpr auto OperationDelete = "delete";
constexpr auto OperationList = "list";
constexpr auto OperationRestore = "restore";
#ifdef USE_ENTERPRISE
constexpr auto OperationUpload = "upload";
constexpr auto OperationDownload = "download";
#endif
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
  using arangodb::basics::StringUtils::concatT;
  using arangodb::basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: " + client.getErrorMessage()};
  }
  if (response->wasHttpError()) {
    auto errorNum = TRI_ERROR_INTERNAL;
    std::string errorMsg = response->getHttpReturnMessage();
    std::shared_ptr<arangodb::velocypack::Builder> bodyBuilder;
    // Handle case of no body:
    try {
      bodyBuilder = response->getBodyVelocyPack();
      arangodb::velocypack::Slice error = bodyBuilder->slice();
      if (!error.isNone() && error.hasKey(arangodb::StaticStrings::ErrorMessage)) {
        errorNum = ErrorCode{
            error.get(arangodb::StaticStrings::ErrorNum).getNumericValue<int>()};
        errorMsg = error.get(arangodb::StaticStrings::ErrorMessage).copyString();
      }
    } catch(...) {
    }
    return {errorNum, concatT("got invalid response from server: HTTP ",
                         itoa(response->getHttpReturnCode()), ": ", errorMsg)};
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

  LOG_TOPIC("0dfda", INFO, arangodb::Logger::BACKUP)
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
          LOG_TOPIC("5caac", INFO, arangodb::Logger::BACKUP)
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

  VPackSlice const backups = resultObject.get("list");

  if (!backups.isObject()) {
    result.reset(TRI_ERROR_INTERNAL,
                 "expected 'result.list' to be an object");
    return result;
  }
  TRI_ASSERT(backups.isObject());

  if (backups.isEmptyObject()) {
    LOG_TOPIC("efc76", INFO, arangodb::Logger::BACKUP)
        << "There are no backups available.";
  } else {
    LOG_TOPIC("e0356", INFO, arangodb::Logger::BACKUP)
        << "The following backups are available:";
    for (auto const& backup : VPackObjectIterator(backups)) {
      LOG_TOPIC("9e6b6", INFO, arangodb::Logger::BACKUP) << " - " << backup.key.copyString();
      arangodb::ResultT<arangodb::BackupMeta> meta = arangodb::BackupMeta::fromSlice(backup.value);
      if (meta.ok()) {
        LOG_TOPIC("0f208", INFO, arangodb::Logger::BACKUP) << "      version:   " << meta.get()._version;
        LOG_TOPIC("55af7", INFO, arangodb::Logger::BACKUP) << "      date/time: " << meta.get()._datetime;
        if (!meta.get()._userSecretHashes.empty()) {
          LOG_TOPIC("56bf8", INFO, arangodb::Logger::BACKUP) << "      encryption secret sha256: "
            << meta.get()._userSecretHashes.front();
        }
        LOG_TOPIC("43522", INFO, arangodb::Logger::BACKUP) << "      size in bytes: " << meta.get()._sizeInBytes;
        LOG_TOPIC("12532", INFO, arangodb::Logger::BACKUP) << "      number of files: " << meta.get()._nrFiles;
        LOG_TOPIC("43212", INFO, arangodb::Logger::BACKUP) << "      number of DBServers: " << meta.get()._nrDBServers;
        LOG_TOPIC("12533", INFO, arangodb::Logger::BACKUP) << "      number of available pieces: " << meta.get()._nrPiecesPresent;
        if (!meta.get()._serverId.empty()) {
          LOG_TOPIC("11112", INFO, arangodb::Logger::BACKUP) << "      serverId: " << meta.get()._serverId;
        }
        if (meta.get()._potentiallyInconsistent) {
          LOG_TOPIC("56241", INFO, arangodb::Logger::BACKUP) << "      potentiallyInconsistent: true";
        } else {
          LOG_TOPIC("56242", INFO, arangodb::Logger::BACKUP) << "      potentiallyInconsistent: false";
        }
        if (meta.get()._isAvailable) {
          LOG_TOPIC("56244", INFO, arangodb::Logger::BACKUP) << "      available: true";
        } else {
          LOG_TOPIC("56246", INFO, arangodb::Logger::BACKUP) << "      available: false";
        }
      }
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
    bodyBuilder.add("allowInconsistent", VPackValue(options.allowInconsistent));
    if (!options.label.empty()) {
      bodyBuilder.add("label", VPackValue(options.label));
    }
    if (options.abortTransactionsIfNeeded) {
      bodyBuilder.add("force", VPackValue(true));
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

  VPackSlice const forced = resultObject.get("potentiallyInconsistent");
  if (forced.isTrue()) {
    LOG_TOPIC("f448b", WARN, arangodb::Logger::BACKUP)
        << "Failed to get write lock before proceeding with backup. Backup may "
           "contain some inconsistencies.";
  } else if (!forced.isBoolean() && !forced.isNone()) {
    result.reset(TRI_ERROR_INTERNAL,
                 "expected 'result.potentiallyInconsistent' to be an boolean");
    return result;
  }

  LOG_TOPIC("c4d37", INFO, arangodb::Logger::BACKUP)
      << "Backup succeeded. Generated identifier '" << identifier.copyString() << "'";
  VPackSlice sizeInBytes = resultObject.get("sizeInBytes");
  VPackSlice nrFiles = resultObject.get("nrFiles");
  if (sizeInBytes.isInteger() && nrFiles.isInteger()) {
    uint64_t size = sizeInBytes.getNumber<uint64_t>();
    uint64_t nr = nrFiles.getNumber<uint64_t>();
    LOG_TOPIC("ce423", INFO, arangodb::Logger::BACKUP)
      << "Total size of backup: " << size << ", number of files: " << nr;
  }
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
    if (options.ignoreVersion) {
      bodyBuilder.add("ignoreVersion", VPackValue(true));
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

  LOG_TOPIC("b6d4c", INFO, arangodb::Logger::BACKUP)
      << "Successfully restored '" << options.identifier << "'";

  bool cluster = false;

  // In a cluster, we do not have to wait for completion:
  VPackSlice resBody = parsedBody->slice();
  TRI_ASSERT(resBody.isObject());
  VPackSlice resultAttr = resBody.get("result");
  if (resultAttr.isObject()) {
    VPackSlice isCluster = resultAttr.get("isCluster");
    if (isCluster.isBool() && isCluster.isTrue()) {
      cluster = true;
    }
  }

  if (!cluster && options.maxWaitForRestart > 0.0) {
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

  LOG_TOPIC("a23cb", INFO, arangodb::Logger::BACKUP)
      << "Successfully deleted '" << options.identifier << "'";

  return result;
}

#ifdef USE_ENTERPRISE
struct TransferType {
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
    return "";  // just to please some compilers
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
    return "";  // just to please some compilers
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
    return "";  // just to please some compilers
  }
};

arangodb::Result executeStatusQuery(arangodb::httpclient::SimpleHttpClient& client,
  arangodb::BackupFeature::Options const& options, TransferType::type type) {
  arangodb::Result result;

  std::string const url = TransferType::asAdminPath(type);
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add(TransferType::asJsonId(type), VPackValue(options.statusId));
    if (options.abort) {
      bodyBuilder.add("abort", VPackValue(true));
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

  VPackSlice const resultObject = resBody.get("result");
  if (!resultObject.isObject()) {
    result.reset(TRI_ERROR_INTERNAL, "expected 'result' to be an object");
    return result;
  }
  TRI_ASSERT(resultObject.isObject());

  // Display status
  if (!options.abort) {
    VPackSlice const dbserversObject = resultObject.get("DBServers");

    for (auto const& server : VPackObjectIterator(dbserversObject)) {
      std::string statusMessage = server.key.copyString();

      if (server.value.hasKey("Status")) {
        statusMessage += " Status: " + server.value.get("Status").copyString();
      }
      LOG_TOPIC("24d75", INFO, arangodb::Logger::BACKUP) << statusMessage;


      if (server.value.hasKey("Progress")) {
        VPackSlice const progressSlice = server.value.get("Progress");

        LOG_TOPIC("68cc8", INFO, arangodb::Logger::BACKUP) << "Last progress update " << progressSlice.get("Time").copyString()
                                                  << ": " << progressSlice.get("Done").getInt() << "/" << progressSlice.get("Total").getInt() << " files done";
      }

      if (server.value.hasKey("Error")) {
        LOG_TOPIC("036de", ERR, arangodb::Logger::BACKUP) << "Error: " << server.value.get("Error").getInt();
      }

      if (server.value.hasKey("ErrorMessage")) {
        LOG_TOPIC("3c3c4", ERR, arangodb::Logger::BACKUP) << "ErrorMessage: " << server.value.get("ErrorMessage").copyString();
      }
    }
  } else {

    if (resBody.hasKey("error") && resBody.get("error").getBoolean()) {
      LOG_TOPIC("f3add", ERR, arangodb::Logger::BACKUP)
        << "error: " << resBody.get("errorMessage").copyString();
    } else {
      LOG_TOPIC("c7c73", INFO, arangodb::Logger::BACKUP)
        << "aborting transfer";
    }


  }
  return result;
}

arangodb::Result executeInitiateTransfer(arangodb::httpclient::SimpleHttpClient& client,
  arangodb::BackupFeature::Options const& options, TransferType::type type) {
  arangodb::Result result;

  // Load configuration file
  std::shared_ptr<VPackBuilder> configFile;

  result = arangodb::basics::catchVoidToResult([&options, &configFile]() {
    std::string configFileSource = arangodb::basics::FileUtils::slurp(options.rcloneConfigFile);
    auto configFileParsed = arangodb::velocypack::Parser::fromJson(configFileSource);
    configFile.swap(configFileParsed);
  });

  if (result.fail()) {
    return result;
  }

  // Initiate transfer
  std::string const url = TransferType::asAdminPath(type);
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("id", VPackValue(options.identifier));
    bodyBuilder.add("remoteRepository", VPackValue(options.remoteDirectory));
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

  std::string transferId = resultObject.get(TransferType::asJsonId(type)).copyString();

  LOG_TOPIC("a9597", INFO, arangodb::Logger::BACKUP) << "Backup initiated, use ";
  LOG_TOPIC("4c459", INFO, arangodb::Logger::BACKUP) << "    arangobackup " << TransferType::asString(type) << " --status-id=" << transferId;
  LOG_TOPIC("5cd70", INFO, arangodb::Logger::BACKUP) << " to query progress.";
  return result;
}

arangodb::Result executeTransfer(arangodb::httpclient::SimpleHttpClient& client,
  arangodb::BackupFeature::Options const& options, TransferType::type type) {

  if (!options.statusId.empty()) {
    // This is a status query
    return executeStatusQuery(client, options, type);
  } else {
    // This is a transfer
    return executeInitiateTransfer(client, options, type);
  }
}
#endif
}  // namespace

namespace arangodb {

BackupFeature::BackupFeature(application_features::ApplicationServer& server, int& exitCode)
    : ApplicationFeature(server, BackupFeature::featureName()), _clientManager{server, Logger::BACKUP}, _exitCode{exitCode} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter<ClientFeature>();
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
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--allow-inconsistent",
                     "whether to attempt to continue in face of errors; "
                     "may result in inconsistent backup state (create operation)",
                     new BooleanParameter(&_options.allowInconsistent));

  options->addOption("--ignore-version",
                     "ignore stored version of a backup. "
                     "Restore may not work if versions mismatch (restore operation)",
                     new BooleanParameter(&_options.ignoreVersion));

  options->addOption("--identifier",
                     "a unique identifier for a backup "
                     "(restore/upload/download operation)",
                     new StringParameter(&_options.identifier));

  options->addOption(
      "--label",
      "an additional label to add to the backup identifier (create operation)",
      new StringParameter(&_options.label));

  options->addOption("--max-wait-for-lock",
                     "maximum time to wait in seconds to acquire a lock on "
                     "all necessary resources (create operation)",
                     new DoubleParameter(&_options.maxWaitForLock));

  options->addOption(
      "--max-wait-for-restart",
      "maximum time to wait in seconds for the server to restart after a "
      "restore operation before reporting an error; if zero, arangobackup will "
      "not wait to check that the server restarts and will simply return the "
      "result of the restore request (restore operation)",
      new DoubleParameter(&_options.maxWaitForRestart));

#ifdef USE_ENTERPRISE
  options->addOption("--status-id",
                     "returns the status of a transfer process "
                     "(upload/download operation)",
                     new StringParameter(&_options.statusId),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise, arangodb::options::Flags::Command));

  options->addOption("--rclone-config-file",
                     "filename of the Rclone configuration file used for"
                     "file transfer (upload/download operation)",
                     new StringParameter(&_options.rcloneConfigFile),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise));

  options->addOption("--remote-path",
                     "remote Rclone path of directory used to store or "
                     "receive backups (upload/download operation)",
                     new StringParameter(&_options.remoteDirectory),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise));

  options->addOption("--abort",
                     "abort transfer with given status-id "
                     "(upload/download operation)",
                     new BooleanParameter(&_options.abort),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise, arangodb::options::Flags::Command));

  options->addOption("--force",
                     "abort transactions if needed to ensure a consistent snapshot. "
                     "This option can destroy the atomicity of your transactions in the "
                     "presence of intermediate commits! Use it with great care and only "
                     "if you really need a consistent backup at all costs (create operation)",
                     new BooleanParameter(&_options.abortTransactionsIfNeeded),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise));
#endif
  /*
    options->addSection(
        "remote", "Options detailing a remote connection to use for operations");

    options->addOption("--remote.credentials",
                       "the credentials used for the remote endpoint",
                       new StringParameter(&_options.credentials));

    options->addOption("--remote.endpoint",
                       "the remote endpoint",
                       new StringParameter(&_options.endpoint));
  */
}

void BackupFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {

  using namespace arangodb::application_features;

  auto const& positionals = options->processingResult()._positionals;
  auto& client = server().getFeature<HttpEndpointProvider, ClientFeature>();

  if (client.databaseName() != "_system") {
    LOG_TOPIC("6b53c", FATAL, Logger::BACKUP)
      << "hot backups are global and must be performed on the _system database with super user privileges";
    FATAL_ERROR_EXIT();
  }

  if (1 == positionals.size()) {
    _options.operation = positionals[0];
  } else {
    LOG_TOPIC("48e32", FATAL, Logger::BACKUP)
        << "expected exactly one operation of create|delete|download|list|restore|upload, got '"
        << basics::StringUtils::join(positionals, ", ") << "'";
    FATAL_ERROR_EXIT();
  }

  auto const it = ::Operations.find(_options.operation);
  if (it == ::Operations.end()) {
    LOG_TOPIC("138ed", FATAL, Logger::BACKUP)
        << "expected operation to be one of: " << operationList(", ");
    FATAL_ERROR_EXIT();
  }

  if (_options.operation == ::OperationCreate) {
    if (!_options.label.empty()) {
      std::regex re = std::regex("^([a-zA-Z0-9\\._\\-]+)$", std::regex::ECMAScript);
      if (!std::regex_match(_options.label, re)) {
        LOG_TOPIC("7829b", FATAL, Logger::BACKUP)
            << "--label value may only contain numbers, letters, periods, "
               "dashes, and underscores";
        FATAL_ERROR_EXIT();
      }
    }

    if (_options.maxWaitForLock < 0.0) {
      LOG_TOPIC("6caeb", FATAL, Logger::BACKUP)
          << "expected --max-wait-for-lock to be a non-negative number, got '"
          << _options.maxWaitForLock << "'";
      FATAL_ERROR_EXIT();
    }
  }

  if (_options.operation == ::OperationDelete || _options.operation == ::OperationRestore) {
    if (_options.identifier.empty()) {
      LOG_TOPIC("e83ef", FATAL, Logger::BACKUP)
          << "must specify a backup via --identifier";
      FATAL_ERROR_EXIT();
    }
  }

  if (_options.operation == ::OperationRestore) {
    if (_options.maxWaitForRestart < 0.0) {
      LOG_TOPIC("efa20", FATAL, Logger::BACKUP) << "expected --max-wait-for-restart to "
                                          "be a non-negative number, got '"
                                       << _options.maxWaitForRestart << "'";
      FATAL_ERROR_EXIT();
    }
  }
#ifdef USE_ENTERPRISE
  if (_options.operation == ::OperationUpload || _options.operation == ::OperationDownload) {

    if (_options.statusId.empty() == _options.identifier.empty()) {
      // Either both or none are set
      LOG_TOPIC("2d0fa", FATAL, Logger::BACKUP) << "either --status-id or --identifier"
                                          " must be set";
      FATAL_ERROR_EXIT();
    }

    if (_options.abort == true &&
        (_options.statusId.empty() || !_options.identifier.empty())) {
      LOG_TOPIC("62375", FATAL, Logger::BACKUP) << "--abort true expects --status-id to be set";
      FATAL_ERROR_EXIT();
    }

    if (!_options.identifier.empty()) {
      if (_options.rcloneConfigFile.empty() || _options.remoteDirectory.empty()) {
        LOG_TOPIC("6063d", FATAL, Logger::BACKUP) << "for data transfer --rclone-config-file"
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
    result = ::executeTransfer(*client, _options, TransferType::type::UPLOAD);
  } else if (_options.operation == OperationDownload) {
    result = ::executeTransfer(*client, _options, TransferType::type::DOWNLOAD);
#endif
  }

  if (result.fail()) {
    LOG_TOPIC("8bde3", ERR, Logger::BACKUP)
        << "Error during backup operation '" << _options.operation
        << "': " << result.errorMessage();
    //FATAL_ERROR_EXIT();
    _exitCode = EXIT_FAILURE;
  }

  _exitCode = EXIT_SUCCESS;
}

}  // namespace arangodb
