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

#include <regex>

#include <velocypack/Iterator.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
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
std::unordered_set<std::string> const Operations = {OperationCreate, OperationDelete,
                                                    OperationList, OperationRestore};

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

arangodb::Result executeList(arangodb::httpclient::SimpleHttpClient& client,
                             arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;

  std::string const url = "/_admin/hotbackup/list";
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return check;
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
  TRI_ASSERT(resultObject.isObject());
  VPackSlice const backups = resultObject.get("hotbackups");
  TRI_ASSERT(backups.isArray());

  if (backups.isEmptyArray()) {
    LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
        << "There are no backups available.";
  } else {
    LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
        << "The following backups are available:";
    for (auto const& backup : VPackArrayIterator(backups)) {
      TRI_ASSERT(backup.isString());
      LOG_TOPIC(INFO, arangodb::Logger::BACKUP) << " - '" << backup.copyString() << "'";
    }
  }

  return result;
}

arangodb::Result executeCreate(arangodb::httpclient::SimpleHttpClient& client,
                               arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;

  std::string const url = "/_admin/hotbackup/create";
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("timeout", VPackValue(options.maxWaitTime));
    bodyBuilder.add("forceBackup", VPackValue(options.force));
    if (!options.label.empty()) {
      bodyBuilder.add("userString", VPackValue(options.label));
    }
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(), body.size()));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return check;
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
  TRI_ASSERT(resultObject.isObject());
  VPackSlice const identifier = resultObject.get("directory");
  TRI_ASSERT(identifier.isString());
  VPackSlice const forced = resultObject.get("forced");
  TRI_ASSERT(forced.isBoolean());

  if (forced.getBoolean()) {
    LOG_TOPIC(WARN, arangodb::Logger::BACKUP)
        << "Failed to get write lock before proceeding with backup. Backup may "
           "contain some inconsistencies.";
  }
  LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
      << "Backup succeeded. Generated identifier '" << identifier.copyString() << "'";

  return result;
}

arangodb::Result executeRestore(arangodb::httpclient::SimpleHttpClient& client,
                                arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;

  std::string const url = "/_admin/hotbackup/restore";
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("directory", VPackValue(options.identifier));
    bodyBuilder.add("saveCurrent", VPackValue(options.saveCurrent));
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url, body.c_str(), body.size()));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return check;
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
    VPackSlice const resultObject = resBody.get("result");
    TRI_ASSERT(resultObject.isObject());
    VPackSlice const previous = resultObject.get("previous");
    TRI_ASSERT(previous.isString());

    LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
        << "current state was saved as backup with identifier '"
        << previous.copyString() << "'";
  }

  LOG_TOPIC(INFO, arangodb::Logger::BACKUP)
      << "Successfully restored '" << options.identifier << "'";

  return result;
}

arangodb::Result executeDelete(arangodb::httpclient::SimpleHttpClient& client,
                               arangodb::BackupFeature::Options const& options) {
  arangodb::Result result;

  std::string const url = "/_admin/hotbackup/create";
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder guard(&bodyBuilder);
    bodyBuilder.add("directory", VPackValue(options.identifier));
  }
  std::string const body = bodyBuilder.slice().toJson();
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::DELETE_REQ, url, body.c_str(),
                     body.size()));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return check;
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

  std::string list = operations.front();
  for (size_t i = 1; i < operations.size(); i++) {
    list.append(separator);
    list.append(operations[i]);
  }
  return list;
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

  options->addOption("--max-wait-time",
                     "maximum time to wait (in seconds) to aquire a lock on "
                     "all necessary resources",
                     new DoubleParameter(&_options.maxWaitTime));

  options->addOption("--save-current",
                     "whether to save the current state as a backup before "
                     "restoring to another state",
                     new BooleanParameter(&_options.saveCurrent));

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
        << basics::StringUtils::join(positionals, ",") << "'";
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
      std::regex re = std::regex("^([a-zA-Z0-9\\-_]+)$", std::regex::ECMAScript);
      if (!std::regex_match(_options.label, re)) {
        LOG_TOPIC(FATAL, Logger::BACKUP)
            << "--label value may only contain numbers, letters, dashes, and "
               "underscores";
        FATAL_ERROR_EXIT();
      }
    }

    if (_options.maxWaitTime < 0.0) {
      LOG_TOPIC(FATAL, Logger::BACKUP)
          << "expected --max-wait-time to be a positive number, got '"
          << _options.maxWaitTime << "'";
      FATAL_ERROR_EXIT();
    }
  } else if (_options.operation == ::OperationDelete || _options.operation == ::OperationRestore) {
    if (_options.identifier.empty()) {
      LOG_TOPIC(FATAL, Logger::BACKUP)
          << "must specify a backup via --identifier";
      FATAL_ERROR_EXIT();
    }
  }
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
    result = ::executeRestore(*client, _options);
  } else if (_options.operation == OperationDelete) {
    result = ::executeDelete(*client, _options);
  }

  if (result.fail()) {
    LOG_TOPIC(ERR, Logger::BACKUP)
        << "Error during backup operation '" << _options.operation
        << "': " << result.errorMessage();
    FATAL_ERROR_EXIT();
  }

  _exitCode = EXIT_SUCCESS;
}

}  // namespace arangodb
