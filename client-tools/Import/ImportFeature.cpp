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

#include "ImportFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/application-exit.h"
#include "Basics/system-functions.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Import/ImportHelper.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/ClientManager.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif
#include <filesystem>
#include <iostream>
#include <regex>

using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;

namespace arangodb {

ImportFeature::ImportFeature(application_features::ApplicationServer& server,
                             int* result)
    : ImportFeature(server, result, ImportFeatureOptions{}) {}

ImportFeature::ImportFeature(application_features::ApplicationServer& server,
                             int* result, ImportFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _result(result) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();
  _options.threadCount =
      std::max(uint32_t(_options.threadCount),
               static_cast<uint32_t>(NumberOfCores::getValue()));
}

ImportFeature::~ImportFeature() = default;

void ImportFeature::prepare() { logLGPLNotice(); }

void ImportFeature::start() {
  ClientFeature& client =
      server().getFeature<HttpEndpointProvider, ClientFeature>();

  int ret = EXIT_SUCCESS;
  *_result = ret;

  // filename
  if (_options.filename == "") {
    LOG_TOPIC("10531", FATAL, arangodb::Logger::FIXME)
        << "File name is missing.";
    FATAL_ERROR_EXIT();
  }

  std::error_code fsEc;
  if (_options.filename != "-" &&
      !std::filesystem::is_regular_file(_options.filename, fsEc)) {
    if (!std::filesystem::exists(_options.filename, fsEc)) {
      LOG_TOPIC("6f83e", FATAL, arangodb::Logger::FIXME)
          << "Cannot open file '" << _options.filename << "'. File not found.";
    } else if (std::filesystem::is_directory(_options.filename, fsEc)) {
      LOG_TOPIC("70dac", FATAL, arangodb::Logger::FIXME)
          << "Specified file '" << _options.filename
          << "' is a directory. Please use a regular file.";
    } else {
      LOG_TOPIC("8699d", FATAL, arangodb::Logger::FIXME)
          << "Cannot open '" << _options.filename << "'. Invalid file type.";
    }

    FATAL_ERROR_EXIT();
  }

  if (_options.typeImport == "auto") {
    std::regex re =
        std::regex(".*?\\.([a-zA-Z]+)(.gz|)", std::regex::ECMAScript);
    std::smatch match;
    if (std::regex_match(_options.filename, match, re)) {
      std::string extension = StringUtils::tolower(match[1].str());
      if (extension == "json" || extension == "jsonl" || extension == "csv" ||
          extension == "tsv") {
        _options.typeImport = extension;
        LOG_TOPIC("4271d", INFO, arangodb::Logger::FIXME)
            << "Auto-detected file type '" << _options.typeImport
            << "' from filename '" << _options.filename << "'";
      }
    }
  }
  if (_options.typeImport == "auto") {
    LOG_TOPIC("0ee99", WARN, arangodb::Logger::FIXME)
        << "Unable to auto-detect file type from filename '"
        << _options.filename << "'. using filetype 'json'";
    _options.typeImport = "json";
  }

  try {
    _httpClient = client.createHttpClient();
  } catch (...) {
    LOG_TOPIC("8477c", FATAL, arangodb::Logger::FIXME)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  // must stay here in order to establish the connection

  auto err = TRI_ERROR_NO_ERROR;
  auto versionString = _httpClient->getServerVersion(&err);
  auto const dbName = client.databaseName();

  auto successfulConnection = [&]() {
    std::cout << ClientFeature::buildConnectedMessage(
                     _httpClient->getEndpointSpecification(), versionString,
                     /*role*/ "",
                     /*mode*/ "", client.databaseName(), client.username())
              << std::endl;

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "database:               " << client.databaseName()
              << std::endl;
    std::cout << "collection:             " << _options.collectionName
              << std::endl;
    if (!_options.fromCollectionPrefix.empty()) {
      std::cout << "from collection prefix: " << _options.fromCollectionPrefix
                << std::endl;
    }
    if (!_options.toCollectionPrefix.empty()) {
      std::cout << "to collection prefix:   " << _options.toCollectionPrefix
                << std::endl;
    }
    std::cout << "overwrite coll. prefix: "
              << (_options.overwriteCollectionPrefix ? "yes" : "no")
              << std::endl;
    std::cout << "create:                 "
              << (_options.createCollection ? "yes" : "no") << std::endl;
    std::cout << "create database:        "
              << (_options.createDatabase ? "yes" : "no") << std::endl;
    std::cout << "source filename:        " << _options.filename << std::endl;
    std::cout << "file type:              " << _options.typeImport << std::endl;

    if (_options.typeImport == "csv") {
      std::cout << "quote:                  " << _options.quote << std::endl;
    }
    if (_options.typeImport == "csv" || _options.typeImport == "tsv") {
      std::cout << "separator:              " << _options.separator
                << std::endl;
      std::cout << "headers file:           " << _options.headersFile
                << std::endl;
    }
    std::cout << "threads:                " << _options.threadCount
              << std::endl;
    std::cout << "on duplicate:           " << _options.onDuplicateAction
              << std::endl;

    std::cout << "connect timeout:        " << client.connectionTimeout()
              << std::endl;
    std::cout << "request timeout:        " << client.requestTimeout()
              << std::endl;
    std::cout << "----------------------------------------" << std::endl;
  };

  if (_options.createDatabase && err == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
    // database not found, but database creation requested
    std::cout << "Creating database '" << dbName << "'" << std::endl;

    client.setDatabaseName("_system");

    auto res = tryCreateDatabase(client, dbName);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("90431", ERR, arangodb::Logger::FIXME)
          << "Could not create database '" << dbName << "'";
      LOG_TOPIC("891eb", FATAL, arangodb::Logger::FIXME)
          << _httpClient->getErrorMessage() << "'";
      FATAL_ERROR_EXIT();
    }

    // restore old database name
    client.setDatabaseName(dbName);
    err = TRI_ERROR_NO_ERROR;
    versionString = _httpClient->getServerVersion(&err);

    if (err != TRI_ERROR_NO_ERROR) {
      // disconnecting here will abort arangoimport a few lines below
      _httpClient->disconnect();
    }
  }

  if (!_httpClient->isConnected()) {
    LOG_TOPIC("541c6", ERR, arangodb::Logger::FIXME)
        << "Could not connect to endpoint '" << client.endpoint()
        << "', database: '" << client.databaseName() << "', username: '"
        << client.username() << "'";
    LOG_TOPIC("034c9", FATAL, arangodb::Logger::FIXME)
        << _httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(client.databaseName() == dbName);

  // successfully connected
  // print out connection info
  successfulConnection();

  _httpClient->disconnect();  // we do not reuse this anymore

  EncryptionFeature* encryption{};
#ifdef USE_ENTERPRISE
  TRI_ASSERT(server().hasFeature<EncryptionFeature>());
  encryption = &server().getFeature<EncryptionFeature>();
#endif

  SimpleHttpClientParams params = _httpClient->params();
  params.setCompressRequestThreshold(
      client.compressTransfer() ? client.compressRequestThreshold() : 0);
  arangodb::import::ImportHelper ih(
      encryption, client, client.endpoint(), params, _options.chunkSize,
      _options.threadCount, _options.maxErrors, _options.autoChunkSize);

  // create colletion
  if (_options.createCollection) {
    ih.setCreateCollection(true);
  }

  if (_options.createCollectionType == "document" ||
      _options.createCollectionType == "edge") {
    ih.setCreateCollectionType(_options.createCollectionType);
  }

  ih.setConversion(_options.convert);
  ih.setRowsToSkip(static_cast<size_t>(_options.rowsToSkip));
  ih.setOverwrite(_options.overwrite);
  ih.useBackslash(_options.useBackslash);
  ih.ignoreMissing(_options.ignoreMissing);
  ih.setSkipValidation(_options.skipValidation);

  // translations (a.k.a. renaming of attributes)
  std::unordered_map<std::string, std::string> translations;
  for (auto const& it : _options.translations) {
    auto parts = StringUtils::split(it, '=');
    TRI_ASSERT(parts.size() == 2);  // already validated before
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);
    translations.emplace(parts[0], parts[1]);
  }
  ih.setTranslations(translations);

  // datatypes (a.k.a. forcing an attribute to a specific type)
  std::unordered_map<std::string, std::string> datatypes;
  for (auto const& it : _options.datatypes) {
    auto parts = StringUtils::split(it, '=');
    TRI_ASSERT(parts.size() == 2);  // already validated before
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);
    datatypes.emplace(parts[0], parts[1]);
  }
  ih.setDatatypes(datatypes);

  // attributes to remove
  ih.setRemoveAttributes(_options.removeAttributes);

  // quote
  if (_options.quote.length() <= 1) {
    ih.setQuote(_options.quote);
  } else {
    LOG_TOPIC("f0b3a", FATAL, arangodb::Logger::FIXME)
        << "Wrong length of quote character.";
    FATAL_ERROR_EXIT();
  }

  if (_options.separator.empty()) {
    _options.separator = ",";
    if (_options.typeImport == "tsv") {
      _options.separator = "\\t";
    }
  }

  // separator
  if (_options.separator.length() == 1 || _options.separator == "\\r" ||
      _options.separator == "\\n" || _options.separator == "\\t") {
    ih.setSeparator(_options.separator);
  } else {
    LOG_TOPIC("59186", FATAL, arangodb::Logger::FIXME)
        << "The separator must be exactly one character.";
    FATAL_ERROR_EXIT();
  }

  // collection name
  if (_options.collectionName == "") {
    LOG_TOPIC("a64ef", FATAL, arangodb::Logger::FIXME)
        << "Collection name is missing.";
    FATAL_ERROR_EXIT();
  }

  // progress
  if (_options.progress) {
    ih.setProgress(true);
  }

  // progress
  if (_options.latencyStats) {
    ih.startHistogram();
  }

  if (!_options.mergeAttributes.empty()) {
    ih.parseMergeAttributes(_options.mergeAttributes);
  }

  if (_options.onDuplicateAction != "error" &&
      _options.onDuplicateAction != "update" &&
      _options.onDuplicateAction != "replace" &&
      _options.onDuplicateAction != "ignore") {
    LOG_TOPIC("6ad02", FATAL, arangodb::Logger::FIXME)
        << "Invalid value for '--on-duplicate'. Possible values: 'error', "
           "'update', 'replace', 'ignore'.";
    FATAL_ERROR_EXIT();
  }

  ih.setOnDuplicateAction(_options.onDuplicateAction);

  try {
    bool ok = false;
    // set prefixes
    ih.setFrom(_options.fromCollectionPrefix);
    ih.setTo(_options.toCollectionPrefix);
    ih.setOverwritePrefix(_options.overwriteCollectionPrefix);

    TRI_NormalizePath(_options.filename);
    // import type
    if (_options.typeImport == "csv") {
      std::cout << "Starting CSV import..." << std::endl;
      ok = ih.importDelimited(_options.collectionName, _options.filename,
                              _options.headersFile,
                              arangodb::import::ImportHelper::CSV);
    } else if (_options.typeImport == "tsv") {
      std::cout << "Starting TSV import..." << std::endl;
      ih.setQuote("");
      ok = ih.importDelimited(_options.collectionName, _options.filename,
                              _options.headersFile,
                              arangodb::import::ImportHelper::TSV);
    } else if (_options.typeImport == "json" ||
               _options.typeImport == "jsonl") {
      std::cout << "Starting JSON import..." << std::endl;
      if (_options.removeAttributes.empty()) {
        ok = ih.importJson(_options.collectionName, _options.filename,
                           (_options.typeImport == "jsonl"));
      } else {
        // This variant does more parsing, on the client side
        // and in general is considered slower, so only use it if necessary.
        ok =
            ih.importJsonWithRewrite(_options.collectionName, _options.filename,
                                     (_options.typeImport == "jsonl"));
      }
    } else {
      LOG_TOPIC("8941e", FATAL, arangodb::Logger::FIXME)
          << "Wrong type '" << _options.typeImport << "'.";
      FATAL_ERROR_EXIT();
    }

    std::cout << std::endl;

    // give information about import (even if errors occur)
    std::cout << "created:          " << ih.getNumberCreated() << std::endl;
    std::cout << "warnings/errors:  " << ih.getNumberErrors() << std::endl;
    std::cout << "updated/replaced: " << ih.getNumberUpdated() << std::endl;
    std::cout << "ignored:          " << ih.getNumberIgnored() << std::endl;

    if (_options.typeImport == "csv" || _options.typeImport == "tsv") {
      std::cout << "lines read:       " << ih.getReadLines() << std::endl;
    }

    if (!ok) {
      auto const& msgs = ih.getErrorMessages();
      if (!msgs.empty()) {
        LOG_TOPIC("46995", ERR, arangodb::Logger::FIXME) << "error message(s):";
        for (std::string const& msg : msgs) {
          LOG_TOPIC("25049", ERR, arangodb::Logger::FIXME) << msg;
        }
      }
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC("a7dca", ERR, arangodb::Logger::FIXME)
        << "caught exception: " << ex.what();
  } catch (...) {
    LOG_TOPIC("fc131", ERR, arangodb::Logger::FIXME)
        << "caught unknown exception";
  }

  *_result = ret;
}

ErrorCode ImportFeature::tryCreateDatabase(ClientFeature& client,
                                           std::string const& name) {
  VPackBuilder builder;
  builder.openObject();
  builder.add("name", VPackValue(name));
  builder.add("users", VPackValue(VPackValueType::Array));
  builder.openObject();
  builder.add("username", VPackValue(client.username()));
  builder.add("passwd", VPackValue(client.password()));
  builder.close();
  builder.close();
  builder.close();

  std::string const body = builder.slice().toJson();

  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      rest::RequestType::POST, "/_api/database", body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return TRI_ERROR_INTERNAL;
  }

  auto returnCode = response->getHttpReturnCode();

  if (returnCode == static_cast<int>(rest::ResponseCode::OK) ||
      returnCode == static_cast<int>(rest::ResponseCode::CREATED)) {
    // all ok
    return TRI_ERROR_NO_ERROR;
  }
  if (returnCode == static_cast<int>(rest::ResponseCode::UNAUTHORIZED) ||
      returnCode == static_cast<int>(rest::ResponseCode::FORBIDDEN)) {
    // invalid authorization
    _httpClient->setErrorMessage(
        ClientManager::getHttpErrorMessage(response.get()).errorMessage(),
        false);
    return TRI_ERROR_FORBIDDEN;
  }

  // any other error
  _httpClient->setErrorMessage(
      ClientManager::getHttpErrorMessage(response.get()).errorMessage(), false);
  return TRI_ERROR_INTERNAL;
}

}  // namespace arangodb
