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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "ExportFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;

ExportFeature::ExportFeature(application_features::ApplicationServer* server,
                             int* result)
    : ApplicationFeature(server, "Export"),
      _collections(),
      _graphName(),
      _typeExport("json"),
      _outputDirectory(),
      _overwrite(false),
      _progress(true),
      _result(result) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Config");
  startsAfter("Logger");

  _outputDirectory =
      FileUtils::buildFilename(FileUtils::currentDirectory(), "export");
}

void ExportFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_collections));

  options->addOption("--graph-name", "name of a graph to export",
                     new StringParameter(&_graphName));

  options->addOption("--output-directory", "output directory",
                     new StringParameter(&_outputDirectory));

  options->addOption("--overwrite", "overwrite data in output directory",
                     new BooleanParameter(&_overwrite));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  std::unordered_set<std::string> exportsWithUpperCase = {"csv", "json", "xgmml",
                                                          "CSV", "JSON", "XGMML"};
  std::unordered_set<std::string> exports = {"csv", "json", "xgmml"};
  std::vector<std::string> exportsVector(exports.begin(), exports.end());
  std::string exportsJoined = StringUtils::join(exportsVector, ", ");
  options->addOption(
      "--type", "type of export (" + exportsJoined + ")",
      new DiscreteValuesParameter<StringParameter>(&_typeExport, exportsWithUpperCase));
}

void ExportFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _outputDirectory = positionals[0];
  } else if (1 < n) {
    LOG(FATAL) << "expecting at most one directory, got " +
                      StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // trim trailing slash from path because it may cause problems on ...
  // Windows
  if (!_outputDirectory.empty() &&
      _outputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    TRI_ASSERT(_outputDirectory.size() > 0);
    _outputDirectory.pop_back();
  }

  if (_graphName.empty() && _collections.empty()) {
    LOG(FATAL) << "expecting at least one collection ore one graph name";
    FATAL_ERROR_EXIT();
  }

  std::transform(_typeExport.begin(), _typeExport.end(), _typeExport.begin(), ::tolower);
}

void ExportFeature::prepare() {
  bool isDirectory = false;
  bool isEmptyDirectory = false;

  if (!_outputDirectory.empty()) {
    isDirectory = TRI_IsDirectory(_outputDirectory.c_str());

    if (isDirectory) {
      std::vector<std::string> files(TRI_FullTreeDirectory(_outputDirectory.c_str()));
      // we don't care if the target directory is empty
      isEmptyDirectory = (files.size() <= 1); // TODO: TRI_FullTreeDirectory always returns at least one element (""), even if directory is empty?
    }
  }

  if (_outputDirectory.empty() ||
      (TRI_ExistsFile(_outputDirectory.c_str()) && !isDirectory)) {
    LOG(FATAL) << "cannot write to output directory '" << _outputDirectory
               << "'";
    FATAL_ERROR_EXIT();
  }

  if (isDirectory && !isEmptyDirectory && !_overwrite) {
    LOG(FATAL) << "output directory '" << _outputDirectory
               << "' already exists. use \"--overwrite true\" to "
                  "overwrite data in it";
    FATAL_ERROR_EXIT();
  }

  if (!isDirectory) {
    long systemError;
    std::string errorMessage;
    int res = TRI_CreateDirectory(_outputDirectory.c_str(), systemError,
                                  errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to create output directory '" << _outputDirectory
               << "': " << errorMessage;
      FATAL_ERROR_EXIT();
    }
  }
}

void ExportFeature::start() {
  ClientFeature* client = application_features::ApplicationServer::getFeature<ClientFeature>("Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  std::unique_ptr<SimpleHttpClient> httpClient;

  try {
    httpClient = client->createHttpClient();
  } catch (...) {
    LOG(FATAL) << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  httpClient->setLocationRewriter(static_cast<void*>(client), &rewriteLocation);
  httpClient->setUserNamePassword("/", client->username(), client->password());

  // must stay here in order to establish the connection
  httpClient->getServerVersion();

  if (!httpClient->isConnected()) {
    LOG(ERR) << "Could not connect to endpoint '" << client->endpoint()
             << "', database: '" << client->databaseName() << "', username: '"
             << client->username() << "'";
    LOG(FATAL) << httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Connected to ArangoDB '"
            << httpClient->getEndpointSpecification() << "', version "
            << httpClient->getServerVersion() << ", database: '"
            << client->databaseName() << "', username: '" << client->username()
            << "'" << std::endl;

  if (_typeExport == "xgmml") {
    graphExport(httpClient.get());
  }

  if (_typeExport == "json" || _typeExport == "csv") {
    if (_collections.size()) {
      collectionExport(httpClient.get());
    } else if (_graphName.size()) {
      graphExport(httpClient.get());
    }
  }

  *_result = ret;
}

void ExportFeature::collectionExport(SimpleHttpClient* httpClient) {
  std::cout << "collection export" << std::endl;

  std::string errorMsg;

  for (auto const& collection : _collections) {
    if (_progress) {
      std::cout << "export collection " << collection << std::endl;
    }

    std::string fileName =
        _outputDirectory + TRI_DIR_SEPARATOR_STR + collection + ".json";

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    int fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                    S_IRUSR | S_IWUSR);

    if (fd < 0) {
      errorMsg = "cannot write to file '" + fileName + "'";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
    }
    TRI_DEFER(TRI_CLOSE(fd));
    std::string const url = "/_api/export?collection="+StringUtils::urlEncode(collection);

    std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(rest::RequestType::POST, url, "" , 0));

    if (response == nullptr || !response->isComplete()) {
      errorMsg =
          "got invalid response from server: " + httpClient->getErrorMessage();
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
    }

    if (response->wasHttpError()) {
      // TODO 404
      errorMsg = "got invalid response from server: HTTP " +
                StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                response->getHttpReturnMessage();
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
    }

    std::shared_ptr<VPackBuilder> parsedBody;
    try {
      parsedBody = response->getBodyVelocyPack();
    } catch (...) {
      errorMsg = "got malformed JSON response from server";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
    }
    VPackSlice body = parsedBody->slice();

    if (!body.isObject()) {
      errorMsg = "got malformed JSON response from server";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
    }

    std::string json;
    bool firstLine = true;
    // TODO: first line

      for (auto const& doc : VPackArrayIterator(body.get("result"))) {
        json = doc.toJson();
        json.push_back('\n');
        if (!TRI_WritePointer(fd, json.c_str(), json.size())) {
          errorMsg = "cannot write to file '" + fileName + "'";
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
        }
      }

    while (body.hasKey("id")) {
      std::string const url = "/_api/export/"+body.get("id").copyString();

      std::unique_ptr<SimpleHttpResult> response(
        httpClient->request(rest::RequestType::PUT, url, "" , 0));

      if (response == nullptr || !response->isComplete()) {
        errorMsg =
            "got invalid response from server: " + httpClient->getErrorMessage();
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
      }

      if (response->wasHttpError()) {
        errorMsg = "got invalid response from server: HTTP " +
                  StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                  response->getHttpReturnMessage();
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
      }

      try {
        parsedBody = response->getBodyVelocyPack();
      } catch (...) {
        errorMsg = "got malformed JSON response from server";
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
      }

      body = parsedBody->slice();

      for (auto const& doc : VPackArrayIterator(body.get("result"))) {
        json = doc.toJson();
        json.push_back('\n');
        if (!TRI_WritePointer(fd, json.c_str(), json.size())) {
          errorMsg = "cannot write to file '" + fileName + "'";
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
        }
      }
    }
  }
}

void ExportFeature::graphExport(SimpleHttpClient* httpClient) {
  if (_progress) {
    std::cout << "graph export" << std::endl;
  }
}
