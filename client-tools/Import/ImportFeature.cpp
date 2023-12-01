////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ImportFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
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
#include <iostream>
#include <regex>

using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;

namespace arangodb {

ImportFeature::ImportFeature(Server& server, int* result)
    : ArangoImportFeature{server, *this},
      _useBackslash(false),
      _convert(true),
      _autoChunkSize(false),
      _chunkSize(1024 * 1024 * 8),
      _threadCount(2),
      _overwriteCollectionPrefix(false),
      _createCollection(false),
      _createDatabase(false),
      _createCollectionType("document"),
      _typeImport("auto"),
      _overwrite(false),
      _quote("\""),
      _separator(""),
      _progress(true),
      _ignoreMissing(false),
      _onDuplicateAction("error"),
      _rowsToSkip(0),
      _maxErrors(20),
      _result(result),
      _skipValidation(false),
      _latencyStats(false) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();
  _threadCount = std::max(uint32_t(_threadCount),
                          static_cast<uint32_t>(NumberOfCores::getValue()));
}

ImportFeature::~ImportFeature() = default;

void ImportFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption("--file", "The file to import (\"-\" for stdin).",
                     new StringParameter(&_filename));

  options->addOption("--auto-rate-limit",
                     "Adjust the data loading rate automatically, starting at "
                     "`--batch-size` bytes per thread per second.",
                     new BooleanParameter(&_autoChunkSize));

  options->addOption("--backslash-escape",
                     "Use backslash as the escape character for quotes. Used "
                     "for CSV and TSV imports.",
                     new BooleanParameter(&_useBackslash));

  options->addOption("--batch-size",
                     "The size for individual data batches (in bytes).",
                     new UInt64Parameter(&_chunkSize));

  options->addOption(
      "--threads", "Number of parallel import threads.",
      new UInt32Parameter(&_threadCount),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--collection",
                     "The name of the collection to import into.",
                     new StringParameter(&_collectionName));

  options->addOption(
      "--from-collection-prefix",
      "The collection name prefix to prepend to all values in the "
      "`_from` attribute.",
      new StringParameter(&_fromCollectionPrefix));

  options->addOption(
      "--to-collection-prefix",
      "The collection name prefix to prepend to all values in the "
      "`_to` attribute.",
      new StringParameter(&_toCollectionPrefix));

  options->addOption("--overwrite-collection-prefix",
                     "If the collection name is already prefixed, overwrite "
                     "the prefix. Only useful in combination with "
                     "`--from-collection-prefix` / `--to-collection-prefix`.",
                     new BooleanParameter(&_overwriteCollectionPrefix));

  options->addOption("--create-collection",
                     "create collection if it does not yet exist",
                     new BooleanParameter(&_createCollection));

  options->addOption("--create-database",
                     "Create the target database if it does not exist.",
                     new BooleanParameter(&_createDatabase));

  options
      ->addOption("--headers-file",
                  "The file to read the CSV or TSV header from. If specified, "
                  "no header is expected in the regular input file.",
                  new StringParameter(&_headersFile))
      .setIntroducedIn(30800);

  options->addOption(
      "--skip-lines",
      "The number of lines to skip of the input file (CSV and TSV only).",
      new UInt64Parameter(&_rowsToSkip));

  options
      ->addOption(
          "--max-errors",
          "The maxium number of errors after which the import will stop.",
          new UInt64Parameter(&_maxErrors))
      .setIntroducedIn(31200)
      .setLongDescription(R"(The maximum number of errors after which the
import is stopped. 

Note that this is not an exact limit for the number of errors.
arangoimport will send data to the server in batches, and likely also in parallel. 
The server will process these in-flight batches regardless of the maximum number
of errors configured here. arangoimport will however stop processing more input
data once the server reported at least this many errors back.)");

  options->addOption(
      "--convert",
      "Convert the strings `null`, `false`, `true` and strings "
      "containing numbers into non-string types. For CSV and TSV "
      "only.",
      new BooleanParameter(&_convert));

  options->addOption("--translate",
                     "Translate an attribute name using the syntax "
                     "\"from=to\". For CSV and TSV only.",
                     new VectorParameter<StringParameter>(&_translations));

  options
      ->addOption(
          "--datatype",
          "Force a specific datatype for an attribute "
          "(null/boolean/number/string) using the syntax \"attribute=type\". "
          "For CSV and TSV only. Takes precedence over `--convert`.",
          new VectorParameter<StringParameter>(&_datatypes))
      .setIntroducedIn(30900);

  options->addOption("--remove-attribute",
                     "remove an attribute before inserting documents"
                     " into collection (for CSV, TSV and JSON only)",
                     new VectorParameter<StringParameter>(&_removeAttributes));

  std::unordered_set<std::string> types = {"document", "edge"};
  std::vector<std::string> typesVector(types.begin(), types.end());
  std::string typesJoined = StringUtils::join(typesVector, " or ");

  options->addOption("--create-collection-type",
                     "The type of the collection if it needs to be created (" +
                         typesJoined + ").",
                     new DiscreteValuesParameter<StringParameter>(
                         &_createCollectionType, types));

  std::unordered_set<std::string> imports = {"csv", "tsv", "json", "jsonl",
                                             "auto"};

  options->addOption(
      "--type", "The format of import file.",
      new DiscreteValuesParameter<StringParameter>(&_typeImport, imports));

  options->addOption(
      "--overwrite",
      "Overwrite the collection if it exists. WARNING: This removes any data "
      "from the collection!",
      new BooleanParameter(&_overwrite));

  options->addOption("--quote", "Quote character(s). Used for CSV and TSV.",
                     new StringParameter(&_quote));

  options->addOption(
      "--separator",
      "The field separator. Used for CSV and TSV imports. "
      "Defaults to a comma (CSV) or a tabulation character (TSV).",
      new StringParameter(&_separator),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--progress", "Show the progress.",
                     new BooleanParameter(&_progress));

  options->addOption("--ignore-missing",
                     "Ignore missing columns in CSV and TSV input.",
                     new BooleanParameter(&_ignoreMissing));

  std::unordered_set<std::string> actions = {"error", "update", "replace",
                                             "ignore"};
  std::vector<std::string> actionsVector(actions.begin(), actions.end());
  std::string actionsJoined = StringUtils::join(actionsVector, ", ");

  options->addOption("--on-duplicate",
                     "The action to perform when a unique key constraint "
                     "violation occurs. Possible values: " +
                         actionsJoined,
                     new DiscreteValuesParameter<StringParameter>(
                         &_onDuplicateAction, actions));

  options
      ->addOption("--merge-attributes",
                  "Merge attributes into new document attribute (e.g. "
                  "\"mergedAttribute=[someAttribute]-[otherAttribute]\") "
                  "(CSV and TSV only)",
                  new VectorParameter<StringParameter>(&_mergeAttributes))
      .setIntroducedIn(30901);

  options->addOption(
      "--latency",
      "Show 10 second latency statistics (values in microseconds).",
      new BooleanParameter(&_latencyStats));

  options->addOption("--skip-validation",
                     "Skip document schema validation during import.",
                     new BooleanParameter(&_skipValidation));
}

void ImportFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if ((1 == n) && (!options->processingResult().touched("--file"))) {
    // only take positional file name attribute into account if user
    // did not specify the --file option as well
    _filename = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("0dc12", FATAL, arangodb::Logger::FIXME)
        << "expecting at most one filename, got " +
               StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  } else if (n > 0) {
    LOG_TOPIC("0dc13", FATAL, arangodb::Logger::FIXME)
        << "Unused commandline arguments: " << positionals;
    FATAL_ERROR_EXIT();
  }

  if (_chunkSize > arangodb::import::ImportHelper::kMaxBatchSize) {
    // it's not sensible to raise the batch size beyond this value
    // because the server has a built-in limit for the batch size too
    // and will reject bigger HTTP request bodies
    LOG_TOPIC("e6d71", WARN, arangodb::Logger::FIXME)
        << "capping --batch-size value to "
        << arangodb::import::ImportHelper::kMaxBatchSize;
    _chunkSize = arangodb::import::ImportHelper::kMaxBatchSize;
  }

  if (_threadCount < 1) {
    // it's not sensible to use just one thread
    LOG_TOPIC("9e3f9", WARN, arangodb::Logger::FIXME)
        << "capping --threads value to " << 1;
    _threadCount = 1;
  }
  if (_threadCount > NumberOfCores::getValue() * 2) {
    // it's not sensible to use just one thread ...
    //  and import's CPU usage is negligible, real limit is cluster cores
    LOG_TOPIC("aca46", WARN, arangodb::Logger::FIXME)
        << "capping --threads value to " << NumberOfCores::getValue() * 2;
    _threadCount = static_cast<uint32_t>(NumberOfCores::getValue()) * 2;
  }

  for (auto const& it : _translations) {
    auto parts = StringUtils::split(it, '=');
    if (parts.size() < 2) {
      parts.push_back("");
    }
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);

    if (parts.size() != 2 || parts[0].empty() || parts[1].empty()) {
      LOG_TOPIC("83ae7", FATAL, arangodb::Logger::FIXME)
          << "invalid translation '" << it << "'";
      FATAL_ERROR_EXIT();
    }
  }
  for (auto const& it : _datatypes) {
    auto parts = StringUtils::split(it, '=');
    if (parts.size() < 2) {
      parts.push_back("");
    }
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);

    if (parts.size() != 2 || parts[0].empty() ||
        (parts[1] != "boolean" && parts[1] != "number" && parts[1] != "null" &&
         parts[1] != "string")) {
      LOG_TOPIC("13e75", FATAL, arangodb::Logger::FIXME)
          << "invalid datatype '" << it << "'. valid types are: "
          << "boolean, number, null, string";
      FATAL_ERROR_EXIT();
    }
  }
  for (std::string& str : _removeAttributes) {
    StringUtils::trimInPlace(str);
    if (str.empty()) {
      LOG_TOPIC("74cfc", FATAL, arangodb::Logger::FIXME)
          << "cannot remove an empty attribute";
      FATAL_ERROR_EXIT();
    }
  }
}

void ImportFeature::start() {
  ClientFeature& client =
      server().getFeature<HttpEndpointProvider, ClientFeature>();

  int ret = EXIT_SUCCESS;
  *_result = ret;

  // filename
  if (_filename == "") {
    LOG_TOPIC("10531", FATAL, arangodb::Logger::FIXME)
        << "File name is missing.";
    FATAL_ERROR_EXIT();
  }

  if (_filename != "-" && !FileUtils::isRegularFile(_filename)) {
    if (!FileUtils::exists(_filename)) {
      LOG_TOPIC("6f83e", FATAL, arangodb::Logger::FIXME)
          << "Cannot open file '" << _filename << "'. File not found.";
    } else if (FileUtils::isDirectory(_filename)) {
      LOG_TOPIC("70dac", FATAL, arangodb::Logger::FIXME)
          << "Specified file '" << _filename
          << "' is a directory. Please use a regular file.";
    } else {
      LOG_TOPIC("8699d", FATAL, arangodb::Logger::FIXME)
          << "Cannot open '" << _filename << "'. Invalid file type.";
    }

    FATAL_ERROR_EXIT();
  }

  if (_typeImport == "auto") {
    std::regex re =
        std::regex(".*?\\.([a-zA-Z]+)(.gz|)", std::regex::ECMAScript);
    std::smatch match;
    if (std::regex_match(_filename, match, re)) {
      std::string extension = StringUtils::tolower(match[1].str());
      if (extension == "json" || extension == "jsonl" || extension == "csv" ||
          extension == "tsv") {
        _typeImport = extension;
        LOG_TOPIC("4271d", INFO, arangodb::Logger::FIXME)
            << "Aauto-detected file type '" << _typeImport
            << "' from filename '" << _filename << "'";
      }
    }
  }
  if (_typeImport == "auto") {
    LOG_TOPIC("0ee99", WARN, arangodb::Logger::FIXME)
        << "Unable to auto-detect file type from filename '" << _filename
        << "'. using filetype 'json'";
    _typeImport = "json";
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
    std::cout << "collection:             " << _collectionName << std::endl;
    if (!_fromCollectionPrefix.empty()) {
      std::cout << "from collection prefix: " << _fromCollectionPrefix
                << std::endl;
    }
    if (!_toCollectionPrefix.empty()) {
      std::cout << "to collection prefix:   " << _toCollectionPrefix
                << std::endl;
    }
    std::cout << "overwrite coll. prefix: "
              << (_overwriteCollectionPrefix ? "yes" : "no") << std::endl;
    std::cout << "create:                 "
              << (_createCollection ? "yes" : "no") << std::endl;
    std::cout << "create database:        " << (_createDatabase ? "yes" : "no")
              << std::endl;
    std::cout << "source filename:        " << _filename << std::endl;
    std::cout << "file type:              " << _typeImport << std::endl;

    if (_typeImport == "csv") {
      std::cout << "quote:                  " << _quote << std::endl;
    }
    if (_typeImport == "csv" || _typeImport == "tsv") {
      std::cout << "separator:              " << _separator << std::endl;
      std::cout << "headers file:           " << _headersFile << std::endl;
    }
    std::cout << "threads:                " << _threadCount << std::endl;
    std::cout << "on duplicate:           " << _onDuplicateAction << std::endl;

    std::cout << "connect timeout:        " << client.connectionTimeout()
              << std::endl;
    std::cout << "request timeout:        " << client.requestTimeout()
              << std::endl;
    std::cout << "----------------------------------------" << std::endl;
  };

  if (_createDatabase && err == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
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
  if constexpr (Server::contains<EncryptionFeature>()) {
    if (server().hasFeature<EncryptionFeature>()) {
      encryption = &server().getFeature<EncryptionFeature>();
    }
  }

  SimpleHttpClientParams params = _httpClient->params();
  arangodb::import::ImportHelper ih(encryption, client, client.endpoint(),
                                    params, _chunkSize, _threadCount,
                                    _maxErrors, _autoChunkSize);

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
  ih.ignoreMissing(_ignoreMissing);
  ih.setSkipValidation(_skipValidation);

  // translations (a.k.a. renaming of attributes)
  std::unordered_map<std::string, std::string> translations;
  for (auto const& it : _translations) {
    auto parts = StringUtils::split(it, '=');
    TRI_ASSERT(parts.size() == 2);  // already validated before
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);
    translations.emplace(parts[0], parts[1]);
  }
  ih.setTranslations(translations);

  // datatypes (a.k.a. forcing an attribute to a specific type)
  std::unordered_map<std::string, std::string> datatypes;
  for (auto const& it : _datatypes) {
    auto parts = StringUtils::split(it, '=');
    TRI_ASSERT(parts.size() == 2);  // already validated before
    StringUtils::trimInPlace(parts[0]);
    StringUtils::trimInPlace(parts[1]);
    datatypes.emplace(parts[0], parts[1]);
  }
  ih.setDatatypes(datatypes);

  // attributes to remove
  ih.setRemoveAttributes(_removeAttributes);

  // quote
  if (_quote.length() <= 1) {
    ih.setQuote(_quote);
  } else {
    LOG_TOPIC("f0b3a", FATAL, arangodb::Logger::FIXME)
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
    LOG_TOPIC("59186", FATAL, arangodb::Logger::FIXME)
        << "_separator must be exactly one character.";
    FATAL_ERROR_EXIT();
  }

  // collection name
  if (_collectionName == "") {
    LOG_TOPIC("a64ef", FATAL, arangodb::Logger::FIXME)
        << "Collection name is missing.";
    FATAL_ERROR_EXIT();
  }

  // progress
  if (_progress) {
    ih.setProgress(true);
  }

  // progress
  if (_latencyStats) {
    ih.startHistogram();
  }

  if (!_mergeAttributes.empty()) {
    ih.parseMergeAttributes(_mergeAttributes);
  }

  if (_onDuplicateAction != "error" && _onDuplicateAction != "update" &&
      _onDuplicateAction != "replace" && _onDuplicateAction != "ignore") {
    LOG_TOPIC("6ad02", FATAL, arangodb::Logger::FIXME)
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
    ih.setOverwritePrefix(_overwriteCollectionPrefix);

    TRI_NormalizePath(_filename);
    // import type
    if (_typeImport == "csv") {
      std::cout << "Starting CSV import..." << std::endl;
      ok = ih.importDelimited(_collectionName, _filename, _headersFile,
                              arangodb::import::ImportHelper::CSV);
    } else if (_typeImport == "tsv") {
      std::cout << "Starting TSV import..." << std::endl;
      ih.setQuote("");
      ok = ih.importDelimited(_collectionName, _filename, _headersFile,
                              arangodb::import::ImportHelper::TSV);
    } else if (_typeImport == "json" || _typeImport == "jsonl") {
      std::cout << "Starting JSON import..." << std::endl;
      if (_removeAttributes.empty()) {
        ok =
            ih.importJson(_collectionName, _filename, (_typeImport == "jsonl"));
      } else {
        // This variant does more parsing, on the client side
        // and in general is considered slower, so only use it if necessary.
        ok = ih.importJsonWithRewrite(_collectionName, _filename,
                                      (_typeImport == "jsonl"));
      }
    } else {
      LOG_TOPIC("8941e", FATAL, arangodb::Logger::FIXME)
          << "Wrong type '" << _typeImport << "'.";
      FATAL_ERROR_EXIT();
    }

    std::cout << std::endl;

    // give information about import (even if errors occur)
    std::cout << "created:          " << ih.getNumberCreated() << std::endl;
    std::cout << "warnings/errors:  " << ih.getNumberErrors() << std::endl;
    std::cout << "updated/replaced: " << ih.getNumberUpdated() << std::endl;
    std::cout << "ignored:          " << ih.getNumberIgnored() << std::endl;

    if (_typeImport == "csv" || _typeImport == "tsv") {
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
