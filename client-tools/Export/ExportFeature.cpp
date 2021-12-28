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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "ExportFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <boost/property_tree/detail/xml_parser_utils.hpp>
#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Slice.h>
#include <velocypack/Sink.h>
#include <velocypack/velocypack-aliases.h>
#include <iostream>
#include <regex>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace boost::property_tree::xml_parser;

namespace {
constexpr double ttlValue = 1200.;
}

namespace arangodb {

ExportFeature::ExportFeature(application_features::ApplicationServer& server,
                             int* result)
    : ApplicationFeature(server, "Export"),
      _xgmmlLabelAttribute("label"),
      _typeExport("json"),
      _customQueryMaxRuntime(0.0),
      _useMaxRuntime(false),
      _escapeCsvFormulae(true),
      _xgmmlLabelOnly(false),
      _overwrite(false),
      _progress(true),
      _useGzip(false),
      _firstLine(true),
      _documentsPerBatch(1000),
      _skippedDeepNested(0),
      _httpRequestsDone(0),
      _currentCollection(),
      _currentGraph(),
      _result(result) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();

  _outputDirectory = FileUtils::buildFilename(
      FileUtils::currentDirectory().result(), "export");
}

void ExportFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_collections));

  options->addOldOption("--query", "custom-query");
  options->addOption("--custom-query", "AQL query to run",
                     new StringParameter(&_customQuery));
  options->addOldOption("--query-max-runtime", "custom-query-max-runtime");
  options
      ->addOption(
          "--custom-query-max-runtime",
          "runtime threshold for AQL queries (in seconds, 0 = no limit)",
          new DoubleParameter(&_customQueryMaxRuntime))
      .setIntroducedIn(30800);

  options
      ->addOption("--custom-query-bindvars",
                  "bind parameters to be used in the 'custom-query' testcase.",
                  new StringParameter(&_customQueryBindVars))
      .setIntroducedIn(31000);

  options->addOption("--graph-name", "name of a graph to export",
                     new StringParameter(&_graphName));

  options->addOption("--xgmml-label-only", "export only xgmml label",
                     new BooleanParameter(&_xgmmlLabelOnly));

  options->addOption("--xgmml-label-attribute",
                     "specify document attribute that will be the xgmml label",
                     new StringParameter(&_xgmmlLabelAttribute));

  options->addOption("--output-directory", "output directory",
                     new StringParameter(&_outputDirectory));

  options
      ->addOption("--documents-per-batch",
                  "number of documents to return in each batch",
                  new UInt64Parameter(&_documentsPerBatch))
      .setIntroducedIn(30800);

  options
      ->addOption("--escape-csv-formulae",
                  "prefix string cells in CSV output with extra single quote "
                  "to prevent formula injection",
                  new BooleanParameter(&_escapeCsvFormulae))
      .setIntroducedIn(30805);

  options->addOption("--overwrite", "overwrite data in output directory",
                     new BooleanParameter(&_overwrite));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  options->addOption("--fields",
                     "comma separated list of fields to export into a csv file",
                     new StringParameter(&_csvFieldOptions));

  std::unordered_set<std::string> exports = {"csv", "json", "jsonl", "xgmml",
                                             "xml"};
  options->addOption(
      "--type", "type of export",
      new DiscreteValuesParameter<StringParameter>(&_typeExport, exports));

  options
      ->addOption(
          "--compress-output",
          "compress files containing collection contents using gzip format",
          new BooleanParameter(&_useGzip))
      .setIntroducedIn(30408)
      .setIntroducedIn(30501);
}

void ExportFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _outputDirectory = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("71137", FATAL, Logger::CONFIG)
        << "expecting at most one directory, got " +
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
  TRI_NormalizePath(_outputDirectory);

  if (_graphName.empty() && _collections.empty() && _customQuery.empty()) {
    LOG_TOPIC("488d8", FATAL, Logger::CONFIG)
        << "expecting at least one collection, a graph name or an AQL query";
    FATAL_ERROR_EXIT();
  }

  if (!_customQuery.empty() && (!_collections.empty() || !_graphName.empty())) {
    LOG_TOPIC("6ff88", FATAL, Logger::CONFIG)
        << "expecting either a list of collections or an AQL query";
    FATAL_ERROR_EXIT();
  }

  if (!_customQueryBindVars.empty()) {
    try {
      _customQueryBindVarsBuilder = VPackParser::fromJson(_customQueryBindVars);
    } catch (...) {
      LOG_TOPIC("bafc2", FATAL, arangodb::Logger::BENCH)
          << "For flag '--custom-query-bindvars " << _customQueryBindVars
          << "': invalid JSON format.";
      FATAL_ERROR_EXIT();
    }
  }

  if (_typeExport == "xgmml" && _graphName.empty()) {
    LOG_TOPIC("2c3be", FATAL, Logger::CONFIG)
        << "expecting a graph name to dump a graph";
    FATAL_ERROR_EXIT();
  }

  if ((_typeExport == "json" || _typeExport == "jsonl" ||
       _typeExport == "csv") &&
      _collections.empty() && _customQuery.empty()) {
    LOG_TOPIC("cdcf7", FATAL, Logger::CONFIG)
        << "expecting at least one collection or an AQL query";
    FATAL_ERROR_EXIT();
  }

  if (_typeExport == "csv") {
    if (_csvFieldOptions.empty()) {
      LOG_TOPIC("76fbf", FATAL, Logger::CONFIG)
          << "expecting at least one field definition";
      FATAL_ERROR_EXIT();
    }

    _csvFields = StringUtils::split(_csvFieldOptions, ',');
  }

  // we will use _maxRuntime only if the option was set by the user
  _useMaxRuntime =
      options->processingResult().touched("--custom-query-max-runtime");
}

void ExportFeature::prepare() {
  _directory = std::make_unique<ManagedDirectory>(server(), _outputDirectory,
                                                  !_overwrite, true, _useGzip);
  if (_directory->status().fail()) {
    switch (static_cast<int>(_directory->status().errorNumber())) {
      case static_cast<int>(TRI_ERROR_FILE_EXISTS):
        LOG_TOPIC("72723", FATAL, Logger::FIXME)
            << "cannot write to output directory '" << _outputDirectory << "'";
        break;
      case static_cast<int>(TRI_ERROR_CANNOT_OVERWRITE_FILE):
        LOG_TOPIC("81812", FATAL, Logger::FIXME)
            << "output directory '" << _outputDirectory
            << "' already exists. use \"--overwrite true\" to "
               "overwrite data in it";
        break;
      default:
        LOG_TOPIC("94945", ERR, Logger::FIXME)
            << _directory->status().errorMessage();
        break;
    }
    FATAL_ERROR_EXIT();
  }
}

void ExportFeature::start() {
  ClientFeature& client =
      server().getFeature<HttpEndpointProvider, ClientFeature>();

  int ret = EXIT_SUCCESS;
  *_result = ret;

  std::unique_ptr<SimpleHttpClient> httpClient;

  try {
    httpClient = client.createHttpClient();
  } catch (...) {
    LOG_TOPIC("98a44", FATAL, Logger::COMMUNICATION)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  httpClient->params().setLocationRewriter(static_cast<void*>(&client),
                                           &rewriteLocation);
  httpClient->params().setUserNamePassword("/", client.username(),
                                           client.password());

  // must stay here in order to establish the connection
  httpClient->getServerVersion();

  if (!httpClient->isConnected()) {
    LOG_TOPIC("b620d", ERR, Logger::COMMUNICATION)
        << "Could not connect to endpoint '" << client.endpoint()
        << "', database: '" << client.databaseName() << "', username: '"
        << client.username() << "'";
    LOG_TOPIC("f251e", FATAL, Logger::COMMUNICATION)
        << httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << ClientFeature::buildConnectedMessage(
                   httpClient->getEndpointSpecification(),
                   httpClient->getServerVersion(),
                   /*role*/ "", /*mode*/ "", client.databaseName(),
                   client.username())
            << std::endl;

  uint64_t exportedSize = 0;

  if (_typeExport == "json" || _typeExport == "jsonl" || _typeExport == "xml" ||
      _typeExport == "csv") {
    if (_collections.size()) {
      collectionExport(httpClient.get());

      for (auto const& collection : _collections) {
        std::string filePath = _outputDirectory + TRI_DIR_SEPARATOR_STR +
                               collection + "." + _typeExport;
        if (_useGzip) {
          filePath.append(".gz");
        }  // if
        int64_t fileSize = TRI_SizeFile(filePath.c_str());

        if (0 < fileSize) {
          exportedSize += fileSize;
        }
      }
    } else if (!_customQuery.empty()) {
      queryExport(httpClient.get());

      std::string filePath =
          _outputDirectory + TRI_DIR_SEPARATOR_STR + "query." + _typeExport;
      if (_useGzip) {
        filePath.append(".gz");
      }  // if
      exportedSize += TRI_SizeFile(filePath.c_str());
    }
  } else if (_typeExport == "xgmml" && _graphName.size()) {
    graphExport(httpClient.get());
    std::string filePath = _outputDirectory + TRI_DIR_SEPARATOR_STR +
                           _graphName + "." + _typeExport;
    if (_useGzip) {
      filePath.append(".gz");
    }  // if
    int64_t fileSize = TRI_SizeFile(filePath.c_str());

    if (0 < fileSize) {
      exportedSize += fileSize;
    }
  }

  using arangodb::basics::StringUtils::formatSize;

  std::cout << "Processed " << _collections.size() << " collection(s), wrote "
            << formatSize(exportedSize) << ", " << _httpRequestsDone
            << " HTTP request(s)" << std::endl;

  *_result = ret;
}

void ExportFeature::collectionExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  for (auto const& collection : _collections) {
    if (_progress) {
      std::cout << "# Exporting collection '" << collection << "'..."
                << std::endl;
    }

    _currentCollection = collection;

    std::string const url = "_api/cursor";

    VPackBuilder post;
    post.openObject();
    post.add("query", VPackValue("FOR doc IN @@collection RETURN doc"));
    post.add("bindVars", VPackValue(VPackValueType::Object));
    post.add("@collection", VPackValue(collection));
    post.close();
    post.add("ttl", VPackValue(::ttlValue));
    post.add("batchSize", VPackValue(_documentsPerBatch));
    post.add("options", VPackValue(VPackValueType::Object));
    post.add("stream", VPackSlice::trueSlice());
    post.close();
    post.close();

    std::shared_ptr<VPackBuilder> parsedBody =
        httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
    VPackSlice body = parsedBody->slice();

    std::string fileName = collection + "." + _typeExport;

    std::unique_ptr<ManagedDirectory::File> fd =
        _directory->writableFile(fileName, _overwrite, 0, true);

    if (nullptr == fd.get() || !fd->status().ok()) {
      errorMsg = "cannot write to file '" + fileName + "'";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
    }

    writeFirstLine(*fd, fileName, collection);

    writeBatch(*fd, VPackArrayIterator(body.get("result")), fileName);

    while (body.hasKey("id")) {
      std::string const url = "/_api/cursor/" + body.get("id").copyString();
      parsedBody = httpCall(httpClient, url, rest::RequestType::POST);
      body = parsedBody->slice();

      writeBatch(*fd, VPackArrayIterator(body.get("result")), fileName);
    }

    if (_typeExport == "json") {
      std::string closingBracket = "\n]";
      writeToFile(*fd, closingBracket);
    } else if (_typeExport == "xml") {
      std::string xmlFooter = "</collection>";
      writeToFile(*fd, xmlFooter);
    }
  }
}

void ExportFeature::queryExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  if (_progress) {
    std::cout << "# Running AQL query '" << _customQuery << "'..." << std::endl;
  }

  std::string const url = "_api/cursor";

  VPackBuilder post;
  post.openObject();
  post.add("query", VPackValue(_customQuery));
  if (!_customQueryBindVars.empty()) {
    post.add("bindVars", _customQueryBindVarsBuilder->slice());
  }
  post.add("ttl", VPackValue(::ttlValue));
  post.add("batchSize", VPackValue(_documentsPerBatch));
  post.add("options", VPackValue(VPackValueType::Object));
  if (_useMaxRuntime) {
    post.add("maxRuntime", VPackValue(_customQueryMaxRuntime));
  }

  post.add("stream", VPackSlice::trueSlice());
  post.close();
  post.close();

  std::shared_ptr<VPackBuilder> parsedBody =
      httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
  VPackSlice body = parsedBody->slice();
  std::string fileName = "query." + _typeExport;

  std::unique_ptr<ManagedDirectory::File> fd =
      _directory->writableFile(fileName, _overwrite, 0, true);

  if (nullptr == fd.get() || !fd->status().ok()) {
    errorMsg = "cannot write to file '" + fileName + "'";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
  }

  writeFirstLine(*fd, fileName, "");

  writeBatch(*fd, VPackArrayIterator(body.get("result")), fileName);

  while (body.hasKey("id")) {
    std::string const url = "/_api/cursor/" + body.get("id").copyString();
    parsedBody = httpCall(httpClient, url, rest::RequestType::POST);
    body = parsedBody->slice();

    writeBatch(*fd, VPackArrayIterator(body.get("result")), fileName);
  }

  if (_typeExport == "json") {
    std::string closingBracket = "\n]";
    writeToFile(*fd, closingBracket);
  } else if (_typeExport == "xml") {
    std::string xmlFooter = "</collection>";
    writeToFile(*fd, xmlFooter);
  }
}

void ExportFeature::writeFirstLine(ManagedDirectory::File& fd,
                                   std::string const& fileName,
                                   std::string const& collection) {
  _firstLine = true;
  if (_typeExport == "json") {
    std::string openingBracket = "[";
    writeToFile(fd, openingBracket);

  } else if (_typeExport == "xml") {
    std::string xmlHeader =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<collection name=\"";
    xmlHeader.append(encode_char_entities(collection));
    xmlHeader.append("\">\n");
    writeToFile(fd, xmlHeader);

  } else if (_typeExport == "csv") {
    std::string firstLine;
    bool isFirstValue = true;
    for (auto const& str : _csvFields) {
      if (isFirstValue) {
        isFirstValue = false;
      } else {
        firstLine.push_back(',');
      }
      appendCsvStringValue(firstLine, str);
    }
    firstLine.push_back('\n');
    writeToFile(fd, firstLine);
  }
}

void ExportFeature::writeBatch(ManagedDirectory::File& fd,
                               VPackArrayIterator it,
                               std::string const& fileName) {
  std::string line;
  line.reserve(1024);

  if (_typeExport == "jsonl") {
    VPackStringSink sink(&line);
    VPackDumper dumper(&sink);

    for (auto const& doc : it) {
      line.clear();
      dumper.dump(doc);
      line.push_back('\n');
      writeToFile(fd, line);
    }
  } else if (_typeExport == "json") {
    VPackStringSink sink(&line);
    VPackDumper dumper(&sink);

    for (auto const& doc : it) {
      line.clear();
      if (!_firstLine) {
        line.append(",\n  ", 4);
      } else {
        line.append("\n  ", 3);
        _firstLine = false;
      }
      dumper.dump(doc);
      writeToFile(fd, line);
    }
  } else if (_typeExport == "csv") {
    std::string value;
    for (auto const& doc : it) {
      line.clear();
      bool isFirstValue = true;

      for (auto const& key : _csvFields) {
        if (isFirstValue) {
          isFirstValue = false;
        } else {
          line.push_back(',');
        }

        VPackSlice val = doc.get(key);
        if (val.isNone()) {
          continue;
        }
        bool escape = false;
        if (val.isArray() || val.isObject()) {
          value = val.toJson();
          escape = true;
        } else if (val.isNull() || val.isBoolean() || val.isNumber()) {
          value = val.toString();
          escape = false;
        } else {
          if (val.isString()) {
            value = val.copyString();
          } else {
            value = val.toString();
          }
          escape = true;
        }

        if (escape) {
          appendCsvStringValue(line, value);
        } else {
          // write unescaped
          TRI_ASSERT(!val.isString());
          line.append(value);
        }
      }
      line.push_back('\n');
      writeToFile(fd, line);
    }
  } else if (_typeExport == "xml") {
    for (auto const& doc : it) {
      line.clear();
      line.append("<doc key=\"");
      line.append(encode_char_entities(doc.get("_key").copyString()));
      line.append("\">\n");
      writeToFile(fd, line);
      for (auto const& att : VPackObjectIterator(doc)) {
        xgmmlWriteOneAtt(fd, att.value, att.key.copyString(), 2);
      }
      line.clear();
      line.append("</doc>\n");
      writeToFile(fd, line);
    }
  }
}

void ExportFeature::writeToFile(ManagedDirectory::File& fd,
                                std::string const& line) {
  fd.write(line.c_str(), line.size());
  auto res = fd.status();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(std::move(res));
  }
}

std::shared_ptr<VPackBuilder> ExportFeature::httpCall(
    SimpleHttpClient* httpClient, std::string const& url,
    rest::RequestType requestType, std::string postBody) {
  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(requestType, url, postBody.c_str(), postBody.size()));
  _httpRequestsDone++;

  auto check = arangodb::HttpResponseChecker::check(
      httpClient->getErrorMessage(), response.get());
  if (check.fail()) {
    LOG_TOPIC("c590f", FATAL, Logger::CONFIG) << check.errorMessage();
    FATAL_ERROR_EXIT();
  }

  std::shared_ptr<VPackBuilder> parsedBody;

  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    LOG_TOPIC("2ce26", FATAL, Logger::CONFIG)
        << "got malformed JSON response from server";
    FATAL_ERROR_EXIT();
  }

  VPackSlice body = parsedBody->slice();

  if (!body.isObject()) {
    LOG_TOPIC("e3f71", FATAL, Logger::CONFIG)
        << "got malformed JSON response from server";
    FATAL_ERROR_EXIT();
  }

  return parsedBody;
}

void ExportFeature::graphExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  _currentGraph = _graphName;

  if (_collections.empty()) {
    if (_progress) {
      std::cout << "# Export graph '" << _graphName << "'" << std::endl;
    }
    std::string const url =
        "/_api/gharial/" + StringUtils::urlEncode(_graphName);
    std::shared_ptr<VPackBuilder> parsedBody =
        httpCall(httpClient, url, rest::RequestType::GET);
    VPackSlice body = parsedBody->slice();

    std::unordered_set<std::string> collections;

    for (auto const& edgeDefs :
         VPackArrayIterator(body.get("graph").get("edgeDefinitions"))) {
      collections.insert(edgeDefs.get("collection").copyString());

      for (VPackSlice from : VPackArrayIterator(edgeDefs.get("from"))) {
        collections.insert(from.copyString());
      }

      for (auto const& to : VPackArrayIterator(edgeDefs.get("to"))) {
        collections.insert(to.copyString());
      }
    }

    for (auto const& cn : collections) {
      _collections.push_back(cn);
    }
  } else {
    if (_progress) {
      std::cout << "# Export graph with collections "
                << StringUtils::join(_collections, ", ") << " as '"
                << _graphName << "'" << std::endl;
    }
  }

  std::string fileName = _graphName + "." + _typeExport;

  std::unique_ptr<ManagedDirectory::File> fd =
      _directory->writableFile(fileName, _overwrite, 0, true);

  if (nullptr == fd.get() || !fd->status().ok()) {
    errorMsg = "cannot write to file '" + fileName + "'";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
  }

  std::string xmlHeader =
      R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<graph label=")";
  writeToFile(*fd, xmlHeader);
  writeToFile(*fd, _graphName);

  xmlHeader = R"(" 
xmlns="http://www.cs.rpi.edu/XGMML" 
directed="1">
)";
  writeToFile(*fd, xmlHeader);

  for (auto const& collection : _collections) {
    if (_progress) {
      std::cout << "# Exporting collection '" << collection << "'..."
                << std::endl;
    }

    std::string const url = "_api/cursor";

    VPackBuilder post;
    post.openObject();
    post.add("query", VPackValue("FOR doc IN @@collection RETURN doc"));
    post.add("bindVars", VPackValue(VPackValueType::Object));
    post.add("@collection", VPackValue(collection));
    post.close();
    post.add("ttl", VPackValue(::ttlValue));
    post.add("batchSize", VPackValue(_documentsPerBatch));
    post.add("options", VPackValue(VPackValueType::Object));
    post.add("stream", VPackSlice::trueSlice());
    post.close();
    post.close();

    std::shared_ptr<VPackBuilder> parsedBody =
        httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
    VPackSlice body = parsedBody->slice();

    writeGraphBatch(*fd, VPackArrayIterator(body.get("result")), fileName);

    while (body.hasKey("id")) {
      std::string const url = "/_api/cursor/" + body.get("id").copyString();
      parsedBody = httpCall(httpClient, url, rest::RequestType::POST);
      body = parsedBody->slice();

      writeGraphBatch(*fd, VPackArrayIterator(body.get("result")), fileName);
    }
  }
  std::string closingGraphTag = "</graph>\n";
  writeToFile(*fd, closingGraphTag);

  if (_skippedDeepNested) {
    std::cout << "skipped " << _skippedDeepNested
              << " deep nested objects / arrays" << std::endl;
  }
}

void ExportFeature::writeGraphBatch(ManagedDirectory::File& fd,
                                    VPackArrayIterator it,
                                    std::string const& fileName) {
  std::string xmlTag;

  for (auto const& doc : it) {
    if (doc.hasKey("_from")) {
      xmlTag =
          "<edge label=\"" +
          encode_char_entities(doc.hasKey(_xgmmlLabelAttribute) &&
                                       doc.get(_xgmmlLabelAttribute).isString()
                                   ? doc.get(_xgmmlLabelAttribute).copyString()
                                   : "Default-Label") +
          "\" source=\"" + encode_char_entities(doc.get("_from").copyString()) +
          "\" target=\"" + encode_char_entities(doc.get("_to").copyString()) +
          "\"";
      writeToFile(fd, xmlTag);
      if (!_xgmmlLabelOnly) {
        xmlTag = ">\n";
        writeToFile(fd, xmlTag);

        for (auto it : VPackObjectIterator(doc)) {
          xgmmlWriteOneAtt(fd, it.value, it.key.copyString());
        }

        xmlTag = "</edge>\n";
        writeToFile(fd, xmlTag);

      } else {
        xmlTag = " />\n";
        writeToFile(fd, xmlTag);
      }

    } else {
      xmlTag =
          "<node label=\"" +
          encode_char_entities(doc.hasKey(_xgmmlLabelAttribute) &&
                                       doc.get(_xgmmlLabelAttribute).isString()
                                   ? doc.get(_xgmmlLabelAttribute).copyString()
                                   : "Default-Label") +
          "\" id=\"" + encode_char_entities(doc.get("_id").copyString()) + "\"";
      writeToFile(fd, xmlTag);
      if (!_xgmmlLabelOnly) {
        xmlTag = ">\n";
        writeToFile(fd, xmlTag);

        for (auto it : VPackObjectIterator(doc)) {
          xgmmlWriteOneAtt(fd, it.value, it.key.copyString());
        }

        xmlTag = "</node>\n";
        writeToFile(fd, xmlTag);

      } else {
        xmlTag = " />\n";
        writeToFile(fd, xmlTag);
      }
    }
  }
}

void ExportFeature::xgmmlWriteOneAtt(ManagedDirectory::File& fd,
                                     VPackSlice const& slice,
                                     std::string const& name, int deep) {
  std::string value, type, xmlTag;

  if (deep == 0 && (name == "_id" || name == "_key" || name == "_rev" ||
                    name == "_from" || name == "_to")) {
    return;
  }

  if (slice.isInteger()) {
    type = "integer";
    value = slice.toString();

  } else if (slice.isDouble()) {
    type = "real";
    value = slice.toString();

  } else if (slice.isBool()) {
    type = "boolean";
    value = slice.toString();

  } else if (slice.isString()) {
    type = "string";
    value = slice.copyString();

  } else if (slice.isArray() || slice.isObject()) {
    if (0 < deep) {
      if (_skippedDeepNested == 0) {
        std::cout << "Warning: skip deep nested objects / arrays" << std::endl;
      }
      _skippedDeepNested++;
      return;
    }

  } else {
    xmlTag = "  <att name=\"" + encode_char_entities(name) +
             "\" type=\"string\" value=\"" +
             encode_char_entities(slice.toString()) + "\"/>\n";
    writeToFile(fd, xmlTag);
    return;
  }

  if (!type.empty()) {
    xmlTag = "  <att name=\"" + encode_char_entities(name) + "\" type=\"" +
             type + "\" value=\"" + encode_char_entities(value) + "\"/>\n";
    writeToFile(fd, xmlTag);

  } else if (slice.isArray()) {
    xmlTag =
        "  <att name=\"" + encode_char_entities(name) + "\" type=\"list\">\n";
    writeToFile(fd, xmlTag);

    for (VPackSlice val : VPackArrayIterator(slice)) {
      xgmmlWriteOneAtt(fd, val, name, deep + 1);
    }

    xmlTag = "  </att>\n";
    writeToFile(fd, xmlTag);

  } else if (slice.isObject()) {
    xmlTag =
        "  <att name=\"" + encode_char_entities(name) + "\" type=\"list\">\n";
    writeToFile(fd, xmlTag);

    for (auto it : VPackObjectIterator(slice)) {
      xgmmlWriteOneAtt(fd, it.value, it.key.copyString(), deep + 1);
    }

    xmlTag = "  </att>\n";
    writeToFile(fd, xmlTag);
  }
}

void ExportFeature::appendCsvStringValue(std::string& output,
                                         std::string const& value) {
  // escape value and put it in quotes
  output.push_back('\"');
  // if we are going to emit a string, we have to take some security
  // precautions. for example, to prevent formula injection in MS Excel and
  // LibreOffice calc, any string cells starting with one of the characters =,
  // +, -, @ need to be escaped with an extra single quote (') so that their
  // contents will not be interpreted as formulae 🙄
  // https://infosecwriteups.com/formula-injection-exploiting-csv-functionality-cd3d8efd02ec
  if (_escapeCsvFormulae && !value.empty()) {
    bool escapeFormula = value.front() == '=' || value.front() == '+' ||
                         value.front() == '-' || value.front() == '@';
    if (escapeFormula) {
      output.push_back('\'');
    }
  }
  output.append(basics::StringUtils::replace(value, "\"", "\"\""));
  output.push_back('\"');
}

}  // namespace arangodb
