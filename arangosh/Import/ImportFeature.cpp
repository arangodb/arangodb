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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ImportFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Import/ImportHelper.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"

#include <iostream>
#include <regex>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;

ImportFeature::ImportFeature(application_features::ApplicationServer* server,
                             int* result)
    : ApplicationFeature(server, "Import"),
      _filename(""),
      _useBackslash(false),
      _convert(true),
      _chunkSize(1024 * 1024 * 16),
      _threadCount(2),
      _collectionName(""),
      _fromCollectionPrefix(""),
      _toCollectionPrefix(""),
      _createCollection(false),
      _createCollectionType("document"),
      _typeImport("json"),
      _overwrite(false),
      _quote("\""),
      _separator(""),
      _progress(true),
      _onDuplicateAction("error"),
      _rowsToSkip(0),
      _result(result) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Config");
  startsAfter("Logger");
}

void ImportFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption("--file", "file name (\"-\" for STDIN)",
                     new StringParameter(&_filename));

  options->addOption(
      "--backslash-escape",
      "use backslash as the escape character for quotes, used for csv",
      new BooleanParameter(&_useBackslash));

  options->addOption("--batch-size",
                     "size for individual data batches (in bytes)",
                     new UInt64Parameter(&_chunkSize));

  options->addOption(
      "--threads",
      "Number of parallel import threads. Most useful for the rocksdb engine",
      new UInt32Parameter(&_threadCount));

  options->addOption("--collection", "collection name",
                     new StringParameter(&_collectionName));

  options->addOption("--from-collection-prefix",
                     "_from collection name prefix (will be prepended to all "
                     "values in '_from')",
                     new StringParameter(&_fromCollectionPrefix));

  options->addOption(
      "--to-collection-prefix",
      "_to collection name prefix (will be prepended to all values in '_to')",
      new StringParameter(&_toCollectionPrefix));

  options->addOption("--create-collection",
                     "create collection if it does not yet exist",
                     new BooleanParameter(&_createCollection));

  options->addOption("--skip-lines",
                     "number of lines to skip for formats (csv and tsv only)",
                     new UInt64Parameter(&_rowsToSkip));

  options->addOption("--convert",
                     "convert the strings 'null', 'false', 'true' and strings "
                     "containing numbers into non-string types (csv and tsv "
                     "only)",
                     new BooleanParameter(&_convert));

  options->addOption("--translate",
                     "translate an attribute name (use as --translate "
                     "\"from=to\", for csv and tsv only)",
                     new VectorParameter<StringParameter>(&_translations));
  
  options->addOption("--remove-attribute",
                     "remove an attribute before inserting an attribute"
                     " into a collection (for csv and tsv only)",
                     new VectorParameter<StringParameter>(&_removeAttributes));

  std::unordered_set<std::string> types = {"document", "edge"};
  std::vector<std::string> typesVector(types.begin(), types.end());
  std::string typesJoined = StringUtils::join(typesVector, " or ");

  options->addOption(
      "--create-collection-type",
      "type of collection if collection is created (" + typesJoined + ")",
      new DiscreteValuesParameter<StringParameter>(&_createCollectionType,
                                                   types));

  std::unordered_set<std::string> imports = {"csv", "tsv", "json", "jsonl",
                                             "auto"};

  options->addOption(
      "--type", "type of import file",
      new DiscreteValuesParameter<StringParameter>(&_typeImport, imports));

  options->addOption(
      "--overwrite",
      "overwrite collection if it exist (WARNING: this will remove any data "
      "from the collection)",
      new BooleanParameter(&_overwrite));

  options->addOption("--quote", "quote character(s), used for csv",
                     new StringParameter(&_quote));

  options->addOption("--separator", "field separator, used for csv and tsv",
                     new StringParameter(&_separator));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  std::unordered_set<std::string> actions = {"error", "update", "replace",
                                             "ignore"};
  std::vector<std::string> actionsVector(actions.begin(), actions.end());
  std::string actionsJoined = StringUtils::join(actionsVector, ", ");

  options->addOption("--on-duplicate",
                     "action to perform when a unique key constraint "
                     "violation occurs. Possible values: " +
                         actionsJoined,
                     new DiscreteValuesParameter<StringParameter>(
                         &_onDuplicateAction, actions));
}

void ImportFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    // only take positional file name attribute into account if user
    // did not specify the --file option as well
    if (!options->processingResult().touched("--file")) {
      _filename = positionals[0];
    }
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expecting at most one filename, got " +
               StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  static unsigned const MaxBatchSize = 768 * 1024 * 1024;

  if (_chunkSize > MaxBatchSize) {
    // it's not sensible to raise the batch size beyond this value
    // because the server has a built-in limit for the batch size too
    // and will reject bigger HTTP request bodies
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "capping --batch-size value to "
                                             << MaxBatchSize;
    _chunkSize = MaxBatchSize;
  }

  if (_threadCount < 1) {
    // it's not sensible to use just one thread
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "capping --threads value to "
                                             << 1;
    _threadCount = 1;
  }
  if (_threadCount > TRI_numberProcessors()) {
    // it's not sensible to use just one thread
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "capping --threads value to "
                                             << TRI_numberProcessors();
    _threadCount = (uint32_t)TRI_numberProcessors();
  }

  for (auto const& it : _translations) {
    auto parts = StringUtils::split(it, "=");
    if (parts.size() != 2) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid translation '" << it
                                                << "'";
      FATAL_ERROR_EXIT();
    }
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);

    if (parts[0].empty() || parts[1].empty()) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid translation '" << it
                                                << "'";
      FATAL_ERROR_EXIT();
    }
  }
  for (std::string& str : _removeAttributes) {
    StringUtils::trimInPlace(str);
    if (str.empty()) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot remove an empty attribute";
      FATAL_ERROR_EXIT();
    }
  }
}

void ImportFeature::start() {
  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  if (_typeImport == "auto") {
    std::regex re = std::regex(".*?\\.([a-zA-Z]+)", std::regex::ECMAScript);
    std::smatch match;
    if (std::regex_match(_filename, match, re)) {
      std::string extension = StringUtils::tolower(match[1].str());
      if (extension == "json" || extension == "jsonl" || extension == "csv" ||
          extension == "tsv") {
        _typeImport = extension;
      } else {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
            << "Unsupported file extension '" << extension << "'";
        FATAL_ERROR_EXIT();
      }
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "Unable to auto-detect file type from filename '" << _filename << "'. using filetype 'json'";
      _typeImport = "json";
    }
  }

  std::unique_ptr<SimpleHttpClient> httpClient;

  try {
    httpClient = client->createHttpClient();
  } catch (...) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  httpClient->params().setLocationRewriter(static_cast<void*>(client),
                                           &rewriteLocation);
  httpClient->params().setUserNamePassword("/", client->username(),
                                           client->password());

  // must stay here in order to establish the connection
  httpClient->getServerVersion();

  if (!httpClient->isConnected()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Could not connect to endpoint '" << client->endpoint()
        << "', database: '" << client->databaseName() << "', username: '"
        << client->username() << "'";
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << httpClient->getErrorMessage()
                                              << "'";
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Connected to ArangoDB '"
            << httpClient->getEndpointSpecification() << "', version "
            << httpClient->getServerVersion() << ", database: '"
            << client->databaseName() << "', username: '" << client->username()
            << "'" << std::endl;

  std::cout << "----------------------------------------" << std::endl;
  std::cout << "database:               " << client->databaseName()
            << std::endl;
  std::cout << "collection:             " << _collectionName << std::endl;
  if (!_fromCollectionPrefix.empty()) {
    std::cout << "from collection prefix: " << _fromCollectionPrefix
              << std::endl;
  }
  if (!_toCollectionPrefix.empty()) {
    std::cout << "to collection prefix:   " << _toCollectionPrefix << std::endl;
  }
  std::cout << "create:                 " << (_createCollection ? "yes" : "no")
            << std::endl;
  std::cout << "source filename:        " << _filename << std::endl;
  std::cout << "file type:              " << _typeImport << std::endl;

  if (_typeImport == "csv") {
    std::cout << "quote:                  " << _quote << std::endl;
  }
  if (_typeImport == "csv" || _typeImport == "tsv") {
    std::cout << "separator:              " << _separator << std::endl;
  }
  std::cout << "threads:                " << _threadCount << std::endl;

  std::cout << "connect timeout:        " << client->connectionTimeout()
            << std::endl;
  std::cout << "request timeout:        " << client->requestTimeout()
            << std::endl;
  std::cout << "----------------------------------------" << std::endl;
  httpClient->disconnect();  // we do not reuse this anymore

  SimpleHttpClientParams params = httpClient->params();
  arangodb::import::ImportHelper ih(client, client->endpoint(), params,
                                    _chunkSize, _threadCount);

  // create colletion
  if (_createCollection) {
    ih.setCreateCollection(true);
  }

  if (_createCollectionType == "document" || _createCollectionType == "edge") {
    ih.setCreateCollectionType(_createCollectionType);
  }

  ih.setConversion(_convert);
  ih.setRowsToSkip(static_cast<size_t>(_rowsToSkip));
  ih.setOverwrite(_overwrite);
  ih.useBackslash(_useBackslash);

  std::unordered_map<std::string, std::string> translations;
  for (auto const& it : _translations) {
    auto parts = StringUtils::split(it, "=");
    TRI_ASSERT(parts.size() == 2);  // already validated before
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);

    translations.emplace(parts[0], parts[1]);
  }

  ih.setTranslations(translations);
  ih.setRemoveAttributes(_removeAttributes);

  // quote
  if (_quote.length() <= 1) {
    ih.setQuote(_quote);
  } else {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "Wrong length of quote character.";
    FATAL_ERROR_EXIT();
  }

  if (_separator.empty()) {
    _separator = ",";
    if (_typeImport == "tsv") {
      _separator = "\\t";
    }
  }

  // separator
  if (_separator.length() == 1 || _separator == "\\r" || _separator == "\\n" ||
      _separator == "\\t") {
    ih.setSeparator(_separator);
  } else {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "_separator must be exactly one character.";
    FATAL_ERROR_EXIT();
  }

  // collection name
  if (_collectionName == "") {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Collection name is missing.";
    FATAL_ERROR_EXIT();
  }

  // filename
  if (_filename == "") {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "File name is missing.";
    FATAL_ERROR_EXIT();
  }

  if (_filename != "-" && !FileUtils::isRegularFile(_filename)) {
    if (!FileUtils::exists(_filename)) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "Cannot open file '" << _filename << "'. File not found.";
    } else if (FileUtils::isDirectory(_filename)) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "Specified file '" << _filename
          << "' is a directory. Please use a regular file.";
    } else {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Cannot open '" << _filename
                                                << "'. Invalid file type.";
    }

    FATAL_ERROR_EXIT();
  }

  // progress
  if (_progress) {
    ih.setProgress(true);
  }

  if (_onDuplicateAction != "error" && _onDuplicateAction != "update" &&
      _onDuplicateAction != "replace" && _onDuplicateAction != "ignore") {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "Invalid value for '--on-duplicate'. Possible values: 'error', "
           "'update', 'replace', 'ignore'.";
    FATAL_ERROR_EXIT();
  }

  ih.setOnDuplicateAction(_onDuplicateAction);

  try {
    bool ok = false;
    // set prefixes
    ih.setFrom(_fromCollectionPrefix);
    ih.setTo(_toCollectionPrefix);

    // import type
    if (_typeImport == "csv") {
      std::cout << "Starting CSV import..." << std::endl;
      ok = ih.importDelimited(_collectionName, _filename,
                              arangodb::import::ImportHelper::CSV);
    }

    else if (_typeImport == "tsv") {
      std::cout << "Starting TSV import..." << std::endl;
      ih.setQuote("");
      ok = ih.importDelimited(_collectionName, _filename,
                              arangodb::import::ImportHelper::TSV);
    }

    else if (_typeImport == "json" || _typeImport == "jsonl") {
      std::cout << "Starting JSON import..." << std::endl;
      ok = ih.importJson(_collectionName, _filename, (_typeImport == "jsonl"));
    }

    else {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Wrong type '" << _typeImport
                                                << "'.";
      FATAL_ERROR_EXIT();
    }

    std::cout << std::endl;

    // give information about import
    if (ok) {
      std::cout << "created:          " << ih.getNumberCreated() << std::endl;
      std::cout << "warnings/errors:  " << ih.getNumberErrors() << std::endl;
      std::cout << "updated/replaced: " << ih.getNumberUpdated() << std::endl;
      std::cout << "ignored:          " << ih.getNumberIgnored() << std::endl;

      if (_typeImport == "csv" || _typeImport == "tsv") {
        std::cout << "lines read:       " << ih.getReadLines() << std::endl;
      }

    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error message(s):";
      for (std::string const& msg : ih.getErrorMessages()) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << msg;
      }
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Caught exception " << ex.what()
                                            << " during import";
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Got an unknown exception during import";
  }

  *_result = ret;
}
