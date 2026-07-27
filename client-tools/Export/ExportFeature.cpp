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

#include "ExportFeature.h"

#include <filesystem>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
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
    : ExportFeature(server, result, ExportFeatureOptions{}) {}

ExportFeature::ExportFeature(application_features::ApplicationServer& server,
                             int* result, ExportFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _result(result) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();

  auto const cwd = std::filesystem::current_path();
  _options.outputDirectory = FileUtils::buildFilename(cwd.string(), "export");
}

ExportFeature::~ExportFeature() = default;

void ExportFeature::prepare() {
  logLGPLNotice();
  EncryptionFeature* encryption{};
#ifdef USE_ENTERPRISE
  TRI_ASSERT(server().hasFeature<EncryptionFeature>());
  encryption = &server().getFeature<EncryptionFeature>();
#endif

  _directory = std::make_unique<ManagedDirectory>(
      encryption, _options.outputDirectory, !_options.overwrite, true,
      _options.useGzip);
  if (_directory->status().fail()) {
    switch (static_cast<int>(_directory->status().errorNumber())) {
      case static_cast<int>(TRI_ERROR_FILE_EXISTS):
        LOG_TOPIC("72723", FATAL, Logger::FIXME)
            << "cannot write to output directory '" << _options.outputDirectory
            << "'";
        break;
      case static_cast<int>(TRI_ERROR_CANNOT_OVERWRITE_FILE):
        LOG_TOPIC("81812", FATAL, Logger::FIXME)
            << "output directory '" << _options.outputDirectory
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
  std::string progressDetails;

  if (_options.typeExport == "json" || _options.typeExport == "jsonl" ||
      _options.typeExport == "xml" || _options.typeExport == "csv") {
    if (_options.collections.size()) {
      progressDetails =
          std::to_string(_options.collections.size()) + " collection(s)";
      collectionExport(httpClient.get());

      for (auto const& collection : _options.collections) {
        std::string filePath = _options.outputDirectory +
                               TRI_DIR_SEPARATOR_STR + collection + "." +
                               _options.typeExport;
        if (_options.useGzip) {
          filePath.append(".gz");
        }  // if
        int64_t fileSize = TRI_SizeFile(filePath.c_str());

        if (0 < fileSize) {
          exportedSize += fileSize;
        }
      }
    } else if (!_options.customQuery.empty()) {
      progressDetails = "1 query";
      queryExport(httpClient.get());

      std::string filePath = _options.outputDirectory + TRI_DIR_SEPARATOR_STR +
                             "query." + _options.typeExport;
      if (_options.useGzip) {
        filePath.append(".gz");
      }  // if
      exportedSize += TRI_SizeFile(filePath.c_str());
    }
  } else if (_options.typeExport == "xgmml" && _options.graphName.size()) {
    progressDetails = "1 graph";
    graphExport(httpClient.get());
    std::string filePath = _options.outputDirectory + TRI_DIR_SEPARATOR_STR +
                           _options.graphName + "." + _options.typeExport;
    if (_options.useGzip) {
      filePath.append(".gz");
    }  // if
    int64_t fileSize = TRI_SizeFile(filePath.c_str());

    if (0 < fileSize) {
      exportedSize += fileSize;
    }
  }

  using arangodb::basics::StringUtils::formatSize;

  std::cout << "Processed " << progressDetails << ", wrote "
            << formatSize(exportedSize) << ", " << _httpRequestsDone
            << " HTTP request(s)" << std::endl;

  *_result = ret;
}

void ExportFeature::collectionExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  for (auto const& collection : _options.collections) {
    if (_options.progress) {
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
    post.add("batchSize", VPackValue(_options.documentsPerBatch));
    post.add("options", VPackValue(VPackValueType::Object));
    post.add("stream", VPackSlice::trueSlice());
    post.close();
    post.close();

    std::shared_ptr<VPackBuilder> parsedBody =
        httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
    VPackSlice body = parsedBody->slice();

    std::string fileName = collection + "." + _options.typeExport;

    std::unique_ptr<ManagedDirectory::File> fd =
        _directory->writableFile(fileName, _options.overwrite, 0, true);

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

    if (_options.typeExport == "json") {
      std::string closingBracket = "\n]";
      writeToFile(*fd, closingBracket);
    } else if (_options.typeExport == "xml") {
      std::string xmlFooter = "</collection>";
      writeToFile(*fd, xmlFooter);
    }
  }
}

void ExportFeature::queryExport(SimpleHttpClient* httpClient) {
  std::string errorMsg;

  if (_options.progress) {
    std::cout << "# Running AQL query '" << _options.customQuery << "'..."
              << std::endl;
  }

  std::string const url = "_api/cursor";

  VPackBuilder post;
  post.openObject();
  post.add("query", VPackValue(_options.customQuery));
  if (!_options.customQueryBindVars.empty()) {
    post.add("bindVars", _options.customQueryBindVarsBuilder->slice());
  }
  post.add("ttl", VPackValue(::ttlValue));
  post.add("batchSize", VPackValue(_options.documentsPerBatch));
  post.add("options", VPackValue(VPackValueType::Object));
  if (_options.useMaxRuntime) {
    post.add("maxRuntime", VPackValue(_options.customQueryMaxRuntime));
  }

  post.add("stream", VPackSlice::trueSlice());
  post.close();
  post.close();

  std::shared_ptr<VPackBuilder> parsedBody =
      httpCall(httpClient, url, rest::RequestType::POST, post.toJson());
  VPackSlice body = parsedBody->slice();
  std::string fileName = "query." + _options.typeExport;

  std::unique_ptr<ManagedDirectory::File> fd =
      _directory->writableFile(fileName, _options.overwrite, 0, true);

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

  if (_options.typeExport == "json") {
    std::string closingBracket = "\n]";
    writeToFile(*fd, closingBracket);
  } else if (_options.typeExport == "xml") {
    std::string xmlFooter = "</collection>";
    writeToFile(*fd, xmlFooter);
  }
}

void ExportFeature::writeFirstLine(ManagedDirectory::File& fd,
                                   std::string const& fileName,
                                   std::string const& collection) {
  _firstLine = true;
  if (_options.typeExport == "json") {
    std::string openingBracket = "[";
    writeToFile(fd, openingBracket);

  } else if (_options.typeExport == "xml") {
    std::string xmlHeader =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<collection name=\"";
    xmlHeader.append(encode_char_entities(collection));
    xmlHeader.append("\">\n");
    writeToFile(fd, xmlHeader);

  } else if (_options.typeExport == "csv") {
    std::string firstLine;
    bool isFirstValue = true;
    for (auto const& str : _options.csvFields) {
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

  if (_options.typeExport == "jsonl") {
    VPackStringSink sink(&line);
    VPackDumper dumper(&sink);

    for (auto const& doc : it) {
      line.clear();
      dumper.dump(doc);
      line.push_back('\n');
      writeToFile(fd, line);
    }
  } else if (_options.typeExport == "json") {
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
  } else if (_options.typeExport == "csv") {
    std::string value;
    for (auto const& doc : it) {
      line.clear();
      bool isFirstValue = true;

      for (auto const& key : _options.csvFields) {
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
  } else if (_options.typeExport == "xml") {
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

  _currentGraph = _options.graphName;

  if (_options.collections.empty()) {
    if (_options.progress) {
      std::cout << "# Export graph '" << _options.graphName << "'" << std::endl;
    }
    std::string const url =
        "/_api/gharial/" + StringUtils::urlEncode(_options.graphName);
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
      _options.collections.push_back(cn);
    }
  } else {
    if (_options.progress) {
      std::cout << "# Export graph with collections "
                << StringUtils::join(_options.collections, ", ") << " as '"
                << _options.graphName << "'" << std::endl;
    }
  }

  std::string fileName = _options.graphName + "." + _options.typeExport;

  std::unique_ptr<ManagedDirectory::File> fd =
      _directory->writableFile(fileName, _options.overwrite, 0, true);

  if (nullptr == fd.get() || !fd->status().ok()) {
    errorMsg = "cannot write to file '" + fileName + "'";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CANNOT_WRITE_FILE, errorMsg);
  }

  std::string xmlHeader =
      R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<graph label=")";
  writeToFile(*fd, xmlHeader);
  writeToFile(*fd, _options.graphName);

  xmlHeader = R"("
xmlns="http://www.cs.rpi.edu/XGMML"
directed="1">
)";
  writeToFile(*fd, xmlHeader);

  for (auto const& collection : _options.collections) {
    if (_options.progress) {
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
    post.add("batchSize", VPackValue(_options.documentsPerBatch));
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
      xmlTag = "<edge label=\"" +
               encode_char_entities(
                   doc.hasKey(_options.xgmmlLabelAttribute) &&
                           doc.get(_options.xgmmlLabelAttribute).isString()
                       ? doc.get(_options.xgmmlLabelAttribute).copyString()
                       : "Default-Label") +
               "\" source=\"" +
               encode_char_entities(doc.get("_from").copyString()) +
               "\" target=\"" +
               encode_char_entities(doc.get("_to").copyString()) + "\"";
      writeToFile(fd, xmlTag);
      if (!_options.xgmmlLabelOnly) {
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
      xmlTag = "<node label=\"" +
               encode_char_entities(
                   doc.hasKey(_options.xgmmlLabelAttribute) &&
                           doc.get(_options.xgmmlLabelAttribute).isString()
                       ? doc.get(_options.xgmmlLabelAttribute).copyString()
                       : "Default-Label") +
               "\" id=\"" + encode_char_entities(doc.get("_id").copyString()) +
               "\"";
      writeToFile(fd, xmlTag);
      if (!_options.xgmmlLabelOnly) {
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
  if (_options.escapeCsvFormulae && !value.empty()) {
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
