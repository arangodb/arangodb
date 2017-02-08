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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;

ExportFeature::ExportFeature(application_features::ApplicationServer* server,
                             int* result)
    : ApplicationFeature(server, "Export"),
      _collections(),
      _graphName(),
      _xgmmlLabelAttribute("label"),
      _typeExport("json"),
      _xgmmlLabelOnly(false),
      _outputDirectory(),
      _overwrite(false),
      _progress(true),
      _firstLine(true),
      _skippedDeepNested(0),
      _currentCollection(),
      _currentGraph(),
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

  options->addOption("--xgmml-label-only", "export only xgmml label",
                     new BooleanParameter(&_xgmmlLabelOnly));

  options->addOption("--xgmml-label-attribute", "specify document attribute that will be the xgmml label",
                     new StringParameter(&_xgmmlLabelAttribute));

  options->addOption("--output-directory", "output directory",
                     new StringParameter(&_outputDirectory));

  options->addOption("--overwrite", "overwrite data in output directory",
                     new BooleanParameter(&_overwrite));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  std::unordered_set<std::string> exportsWithUpperCase = {"json", "jsonl", "xgmml",
                                                          "JSON", "JSONL", "XGMML"};
  std::unordered_set<std::string> exports = {"json", "jsonl", "xgmml"};
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
    LOG(FATAL) << "expecting at least one collection or one graph name";
    FATAL_ERROR_EXIT();
  }

  std::transform(_typeExport.begin(), _typeExport.end(), _typeExport.begin(), ::tolower);

  if (_typeExport == "xgmml" && _graphName.empty() ) {
    LOG(FATAL) << "expecting a graph name to dump a graph";
    FATAL_ERROR_EXIT();
  }
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

  if (_typeExport == "json" || _typeExport == "jsonl") {
    if (_collections.size()) {
      collectionExport(httpClient.get());
    } else if (_typeExport == "xgmml" && _graphName.size()) {
      graphExport(httpClient.get());
    }
  }

  *_result = ret;
}

void ExportFeature::collectionExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  for (auto const& collection : _collections) {
    if (_progress) {
      std::cout << "# Exporting collection '" << collection << "'..." << std::endl;
    }

    _currentCollection = collection;

    std::string fileName =
        _outputDirectory + TRI_DIR_SEPARATOR_STR + collection + "." + _typeExport;

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    int fd = -1;
    TRI_DEFER(TRI_CLOSE(fd));

    std::string const url = "_api/cursor";

    VPackBuilder post;
    post.openObject();
    post.add("query", VPackValue("FOR doc IN @@collection RETURN doc"));
    post.add("bindVars", VPackValue(VPackValueType::Object));
    post.add("@collection", VPackValue(collection));
    post.close();
    post.close();

    std::shared_ptr<VPackBuilder> parsedBody = httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
    VPackSlice body = parsedBody->slice();

    fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                    S_IRUSR | S_IWUSR);

    if (fd < 0) {
      errorMsg = "cannot write to file '" + fileName + "'";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
    }

    _firstLine = true;
    if (_typeExport == "json") {
      std::string openingBracket = "[\n";
      writeToFile(fd, openingBracket, fileName);
    }

    writeCollectionBatch(fd, VPackArrayIterator(body.get("result")), fileName);

    while (body.hasKey("id")) {
      std::string const url = "/_api/cursor/"+body.get("id").copyString();
      parsedBody = httpCall(httpClient, url, rest::RequestType::PUT);
      body = parsedBody->slice();

      writeCollectionBatch(fd, VPackArrayIterator(body.get("result")), fileName);
    }
    if (_typeExport == "json") {
      std::string closingBracket = "]\n";
      writeToFile(fd, closingBracket , fileName);
    }
  }
}

void ExportFeature::writeCollectionBatch(int fd, VPackArrayIterator it, std::string const& fileName) {
  std::string line;

  for (auto const& doc : it) {
    line.clear();

    if (_firstLine && _typeExport == "json") {
      _firstLine = false;
    } else if(!_firstLine && _typeExport == "json") {
      line.push_back(',');
    }

    line += doc.toJson();
    line.push_back('\n');
    writeToFile(fd, line, fileName);
  }
}

void ExportFeature::writeToFile(int fd, std::string& line, std::string const& fileName) {
    if (!TRI_WritePointer(fd, line.c_str(), line.size())) {
      std::string errorMsg = "cannot write to file '" + fileName + "'";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
    }
}

std::shared_ptr<VPackBuilder> ExportFeature::httpCall(SimpleHttpClient* httpClient, std::string const& url, rest::RequestType requestType, std::string postBody) {
  std::string errorMsg;

  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(requestType, url, postBody.c_str(), postBody.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + httpClient->getErrorMessage();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }

  std::shared_ptr<VPackBuilder> parsedBody;

  if (response->wasHttpError()) {

    if (response->getHttpReturnCode() == 404) {
      if (_currentGraph.size()) {
        LOG(FATAL) << "Graph '" << _currentGraph << "' not found.";
      } else if (_currentCollection.size()) {
        LOG(FATAL) << "Collection " << _currentCollection << "not found.";
      }

      FATAL_ERROR_EXIT();
    } else {
      parsedBody = response->getBodyVelocyPack();
      std::cout << parsedBody->toJson() << std::endl;
      errorMsg = "got invalid response from server: HTTP " +
                StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                response->getHttpReturnMessage();
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
    }
  }

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

  return parsedBody;
}

void ExportFeature::graphExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  _currentGraph = _graphName;

  if (_collections.empty()) {
    if (_progress) {
      std::cout << "# Export graph '" << _graphName << "'" << std::endl;
    }
    std::string const url = "/_api/gharial/" + _graphName;
    std::shared_ptr<VPackBuilder> parsedBody = httpCall(httpClient, url, rest::RequestType::GET);
    VPackSlice body = parsedBody->slice();

    std::unordered_set<std::string> collections;

    for(auto const& edgeDefs : VPackArrayIterator(body.get("graph").get("edgeDefinitions"))) {
      collections.insert(edgeDefs.get("collection").copyString());

      for(auto const& from : VPackArrayIterator(edgeDefs.get("from"))) {
        collections.insert(from.copyString());
      }

      for(auto const& to : VPackArrayIterator(edgeDefs.get("to"))) {
        collections.insert(to.copyString());
      }
    }

    for (auto const& cn : collections) {
      _collections.push_back(cn);
    }
  } else {
    if (_progress) {
      std::cout << "# Export graph with collections " << StringUtils::join(_collections, ", ") << " as '" << _graphName << "'" << std::endl;
    }
  }

  std::string fileName = _outputDirectory + TRI_DIR_SEPARATOR_STR + _graphName + "." + _typeExport;

  // remove an existing file first
  if (TRI_ExistsFile(fileName.c_str())) {
    TRI_UnlinkFile(fileName.c_str());
  }

  int fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    errorMsg = "cannot write to file '" + fileName + "'";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
  }
  TRI_DEFER(TRI_CLOSE(fd));

  std::string xmlHeader = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<graph label=")";
  writeToFile(fd, xmlHeader, fileName);
  writeToFile(fd, _graphName, fileName);

  xmlHeader = R"(" 
xmlns:dc="http://purl.org/dc/elements/1.1/" 
xmlns:xlink="http://www.w3.org/1999/xlink" 
xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#" 
xmlns:cy="http://www.cytoscape.org" 
xmlns="http://www.cs.rpi.edu/XGMML" 
directed="1">
)";
  writeToFile(fd, xmlHeader, fileName);

  for (auto const& collection : _collections) {
    if (_progress) {
      std::cout << "# Exporting collection '" << collection << "'..." << std::endl;
    }

    std::string const url = "_api/cursor";

    VPackBuilder post;
    post.openObject();
    post.add("query", VPackValue("FOR doc IN @@collection RETURN doc"));
    post.add("bindVars", VPackValue(VPackValueType::Object));
    post.add("@collection", VPackValue(collection));
    post.close();
    post.close();

    std::shared_ptr<VPackBuilder> parsedBody = httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
    VPackSlice body = parsedBody->slice();

    writeGraphBatch(fd, VPackArrayIterator(body.get("result")), fileName);

    while (body.hasKey("id")) {
      std::string const url = "/_api/cursor/"+body.get("id").copyString();
      parsedBody = httpCall(httpClient, url, rest::RequestType::PUT);
      body = parsedBody->slice();

      writeGraphBatch(fd, VPackArrayIterator(body.get("result")), fileName);
    }
  }
  std::string closingGraphTag = "</graph>\n";
  writeToFile(fd, closingGraphTag, fileName);

  if (_skippedDeepNested) {
    std::cout << "skipped " << _skippedDeepNested << " deep nested objects / arrays" << std::endl;
  }
}

void ExportFeature::writeGraphBatch(int fd, VPackArrayIterator it, std::string const& fileName) {
  std::string xmlTag;

  for(auto const& doc : it) {
    if (doc.hasKey("_from")) {
      xmlTag = "<edge label=\"" + (doc.hasKey("label") ? doc.get("label").copyString() : "Default-Label") + "\" source=\"" + doc.get("_from").copyString() + "\" target=\"" + doc.get("_to").copyString() + "\"";
      writeToFile(fd, xmlTag, fileName);
      if (!_xgmmlLabelOnly) {
        xmlTag = ">\n";
        writeToFile(fd, xmlTag, fileName);

        for (auto const& it : VPackObjectIterator(doc)) {
          xmlTag = it.key.copyString();
          xgmmlWriteOneAtt(fd, fileName, it.value, xmlTag);
        }

        xmlTag = "</edge>\n";
        writeToFile(fd, xmlTag, fileName);

      } else {
        xmlTag = " />\n";
        writeToFile(fd, xmlTag, fileName);
      }

    } else {
      xmlTag = "<node label=\"" + (doc.hasKey("label") ? doc.get("label").copyString() : "Default-Label") + "\" id=\"" + doc.get("_id").copyString() + "\"";
      writeToFile(fd, xmlTag, fileName);
      if (!_xgmmlLabelOnly) {
        xmlTag = ">\n";
        writeToFile(fd, xmlTag, fileName);

        for (auto const& it : VPackObjectIterator(doc)) {
          xmlTag = it.key.copyString();
          xgmmlWriteOneAtt(fd, fileName, it.value, xmlTag);
        }

        xmlTag = "</node>\n";
        writeToFile(fd, xmlTag, fileName);

      } else {
        xmlTag = " />\n";
        writeToFile(fd, xmlTag, fileName);
      }
    }
  }
}

void ExportFeature::xgmmlWriteOneAtt(int fd, std::string const& fileName, VPackSlice const& slice, std::string& name, int deep) {
  std::string value, type, xmlTag;

  if (deep == 0 &&
     (name == "_id" || name == "_key" || name == "_rev" || name == "_from" || name == "_to")) {
    return;
  }

  if (slice.isInteger()) {
    type = "integer";
    value = "\"" + slice.toString() + "\"";

  } else if (slice.isDouble()) {
    type = "real";
    value = "\"" + slice.toString() + "\"";

  } else if (slice.isBool()) {
    type = "boolean";
    value = "\"" + slice.toString() + "\"";

  } else if (slice.isString()) {
    type = "string";
    value = slice.toString();

  } else if (slice.isArray() || slice.isObject()) {
    if (0 < deep) {
      if (_skippedDeepNested == 0) {
        std::cout << "Warning: skip deep nested objects / arrays" << std::endl;
      }
      _skippedDeepNested++;
      return;
    }

  } else {
    xmlTag = "  <att name=\"" + name + "\" type=\"string\" value=\"" + slice.toString() + "\"/>\n";
    writeToFile(fd, xmlTag, fileName);
    return;
  }

  if (!type.empty()) {
    xmlTag = "  <att name=\"" + name + "\" type=\"" + type + "\" value=" + value + "/>\n";
    writeToFile(fd, xmlTag, fileName);

  } else if (slice.isArray()) {
    xmlTag = "  <att name=\"" + name + "\" type=\"list\">\n";
    writeToFile(fd, xmlTag, fileName);

    for (auto const& val : VPackArrayIterator(slice)) {
      xgmmlWriteOneAtt(fd, fileName, val, name, deep + 1);
    }

    xmlTag = "  </att>\n";
    writeToFile(fd, xmlTag, fileName);

  } else if (slice.isObject()) {
    xmlTag = "  <att name=\"" + name + "\" type=\"list\">\n";
    writeToFile(fd, xmlTag, fileName);

    for (auto const& it : VPackObjectIterator(slice)) {
      std::string name = it.key.copyString();
      xgmmlWriteOneAtt(fd, fileName, it.value, name, deep + 1);
    }

    xmlTag = "  </att>\n";
    writeToFile(fd, xmlTag, fileName);
  }
}
