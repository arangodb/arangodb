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

#include "DumpFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Endpoint/Endpoint.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace arangodb::rest;

DumpFeature::DumpFeature(application_features::ApplicationServer* server,
                         int* result)
    : ApplicationFeature(server, "Dump"),
      _collections(),
      _chunkSize(1024 * 1024 * 2),
      _maxChunkSize(1024 * 1024 * 12),
      _dumpData(true),
      _force(false),
      _includeSystemCollections(false),
      _outputDirectory(),
      _overwrite(false),
      _progress(true),
      _tickStart(0),
      _tickEnd(0),
      _compat28(false),
      _result(result),
      _batchId(0),
      _clusterMode(false),
      _stats{ 0, 0, 0 } {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Logger");

  _outputDirectory =
      FileUtils::buildFilename(FileUtils::currentDirectory(), "dump");
}

void DumpFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_collections));

  options->addOption("--initial-batch-size",
                     "initial size for individual data batches (in bytes)",
                     new UInt64Parameter(&_chunkSize));

  options->addOption("--batch-size",
                     "initial size for individual data batches (in bytes)",
                     new UInt64Parameter(&_maxChunkSize));

  options->addOption("--dump-data", "dump collection data",
                     new BooleanParameter(&_dumpData));

  options->addOption(
      "--force", "continue dumping even in the face of some server-side errors",
      new BooleanParameter(&_force));

  options->addOption("--include-system-collections",
                     "include system collections",
                     new BooleanParameter(&_includeSystemCollections));

  options->addOption("--output-directory", "output directory",
                     new StringParameter(&_outputDirectory));

  options->addOption("--overwrite", "overwrite data in output directory",
                     new BooleanParameter(&_overwrite));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  options->addOption("--tick-start", "only include data after this tick",
                     new UInt64Parameter(&_tickStart));

  options->addOption("--tick-end", "last tick to be included in data dump",
                     new UInt64Parameter(&_tickEnd));
  
  options->addOption("--compat28", "produce a dump compatible with ArangoDB 2.8",
                     new BooleanParameter(&_compat28));
}

void DumpFeature::validateOptions(
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

  if (_chunkSize < 1024 * 128) {
    _chunkSize = 1024 * 128;
  }

  if (_maxChunkSize < _chunkSize) {
    _maxChunkSize = _chunkSize;
  }

  if (_tickStart < _tickEnd) {
    LOG(FATAL) << "invalid values for --tick-start or --tick-end";
    FATAL_ERROR_EXIT();
  }

  // trim trailing slash from path because it may cause problems on ...
  // Windows
  if (!_outputDirectory.empty() &&
      _outputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    TRI_ASSERT(_outputDirectory.size() > 0);
    _outputDirectory.pop_back();
  }
}

void DumpFeature::prepare() {
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

// start a batch
int DumpFeature::startBatch(std::string DBserver, std::string& errorMsg) {
  std::string const url = "/_api/replication/batch";
  std::string const body = "{\"ttl\":300}";

  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::POST, url + urlExt,
                           body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + _httpClient->getErrorMessage();

    if (_force) {
      return TRI_ERROR_NO_ERROR;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_INTERNAL;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    errorMsg = "got malformed JSON";
    return TRI_ERROR_INTERNAL;
  }
  VPackSlice const resBody = parsedBody->slice();

  // look up "id" value
  std::string const id =
      arangodb::basics::VelocyPackHelper::getStringValue(resBody, "id", "");

  _batchId = StringUtils::uint64(id);

  return TRI_ERROR_NO_ERROR;
}

// prolongs a batch
void DumpFeature::extendBatch(std::string DBserver) {
  TRI_ASSERT(_batchId > 0);

  std::string const url =
      "/_api/replication/batch/" + StringUtils::itoa(_batchId);
  std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::PUT, url + urlExt,
                           body.c_str(), body.size()));

  // ignore any return value
}

// end a batch
void DumpFeature::endBatch(std::string DBserver) {
  TRI_ASSERT(_batchId > 0);

  std::string const url =
      "/_api/replication/batch/" + StringUtils::itoa(_batchId);
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  _batchId = 0;

  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      rest::RequestType::DELETE_REQ, url + urlExt, nullptr, 0));

  // ignore any return value
}

/// @brief dump a single collection
int DumpFeature::dumpCollection(int fd, std::string const& cid,
                                std::string const& name, uint64_t maxTick,
                                std::string& errorMsg) {
  uint64_t chunkSize = _chunkSize;

  std::string const baseUrl = "/_api/replication/dump?collection=" + cid +
                              "&ticks=false&flush=false";

  uint64_t fromTick = _tickStart;

  while (true) {
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick) +
                      "&chunkSize=" + StringUtils::itoa(chunkSize);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    if (_compat28) {
      url += "&compat28=true";
    }

    _stats._totalBatches++;

    std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
        rest::RequestType::GET, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      errorMsg =
          "got invalid response from server: " + _httpClient->getErrorMessage();

      return TRI_ERROR_INTERNAL;
    }

    if (response->wasHttpError()) {
      errorMsg = getHttpErrorMessage(response.get(), nullptr);

      return TRI_ERROR_INTERNAL;
    }

    int res = TRI_ERROR_NO_ERROR;  // just to please the compiler
    bool checkMore = false;
    bool found;

    // TODO: fix hard-coded headers
    std::string header =
        response->getHeaderField("x-arango-replication-checkmore", found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        // TODO: fix hard-coded headers
        header = response->getHeaderField("x-arango-replication-lastincluded",
                                          found);

        if (found) {
          uint64_t tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          } else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (!found) {
      errorMsg = "got invalid response server: required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer const& body = response->getBody();

      if (!TRI_WritePointer(fd, body.c_str(), body.length())) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      } else {
        _stats._totalWritten += (uint64_t)body.length();
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      return res;
    }

    if (chunkSize < _maxChunkSize) {
      // adaptively increase chunksize
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);

      if (chunkSize > _maxChunkSize) {
        chunkSize = _maxChunkSize;
      }
    }
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

// execute a WAL flush request
void DumpFeature::flushWal() {
  std::string const url =
      "/_admin/wal/flush?waitForSync=true&waitForCollector=true";

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::PUT, url, nullptr, 0));

  if (response == nullptr || !response->isComplete() ||
      response->wasHttpError()) {
    std::cerr << "got invalid response from server: " +
                     _httpClient->getErrorMessage()
              << std::endl;
  }
}

// dump data from server
int DumpFeature::runDump(std::string& dbName, std::string& errorMsg) {
  std::string const url =
      "/_api/replication/inventory?includeSystem=" +
      std::string(_includeSystemCollections ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + _httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();
    return TRI_ERROR_INTERNAL;
  }

  flushWal();
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }
  VPackSlice const body = parsedBody->slice();

  if (!body.isObject()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  VPackSlice const collections = body.get("collections");

  if (!collections.isArray()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  // read the server's max tick value
  std::string const tickString =
      arangodb::basics::VelocyPackHelper::getStringValue(body, "tick", "");

  if (tickString == "") {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  std::cout << "Last tick provided by server is: " << tickString << std::endl;

  uint64_t maxTick = StringUtils::uint64(tickString);
  // check if the user specified a max tick value
  if (_tickEnd > 0 && maxTick > _tickEnd) {
    maxTick = _tickEnd;
  }

  try {
    VPackBuilder meta;
    meta.openObject();
    meta.add("database", VPackValue(dbName));
    meta.add("lastTickAtDumpStart", VPackValue(tickString));

    // save last tick in file
    std::string fileName =
        _outputDirectory + TRI_DIR_SEPARATOR_STR + "dump.json";

    int fd;

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    fd = TRI_CREATE(fileName.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                    S_IRUSR | S_IWUSR);

    if (fd < 0) {
      errorMsg = "cannot write to file '" + fileName + "'";

      return TRI_ERROR_CANNOT_WRITE_FILE;
    }
    meta.close();

    std::string const metaString = meta.slice().toJson();
    if (!TRI_WritePointer(fd, metaString.c_str(), metaString.size())) {
      TRI_CLOSE(fd);
      errorMsg = "cannot write to file '" + fileName + "'";

      return TRI_ERROR_CANNOT_WRITE_FILE;
    }

    TRI_CLOSE(fd);
  } catch (...) {
    errorMsg = "out of memory";

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }

  // iterate over collections
  for (VPackSlice const& collection : VPackArrayIterator(collections)) {
    if (!collection.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    uint64_t const cid = arangodb::basics::VelocyPackHelper::extractIdValue(parameters);
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);
    int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        parameters, "type", 2);
    std::string const collectionType(type == 2 ? "document" : "edge");

    if (cid == 0 || name == "") {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && !_includeSystemCollections) {
      continue;
    }

    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    std::string const hexString(arangodb::rest::SslInterface::sslMD5(name));

    // found a collection!
    if (_progress) {
      std::cout << "# Dumping " << collectionType << " collection '" << name
                << "'..." << std::endl;
    }

    // now save the collection meta data and/or the actual data
    _stats._totalCollections++;

    {
      // save meta data
      std::string fileName = _outputDirectory + TRI_DIR_SEPARATOR_STR + name +
                             "_" + hexString + ".structure.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(),
                      O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      std::string const collectionInfo = collection.toJson();

      if (!TRI_WritePointer(fd, collectionInfo.c_str(),
                            collectionInfo.size())) {
        TRI_CLOSE(fd);
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      TRI_CLOSE(fd);
    }

    if (_dumpData) {
      // save the actual data
      std::string fileName;
      fileName = _outputDirectory + TRI_DIR_SEPARATOR_STR + name + "_" +
                 hexString + ".data.json";

      int fd;

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      fd = TRI_CREATE(fileName.c_str(),
                      O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      extendBatch("");
      int res = dumpCollection(fd, std::to_string(cid), name, maxTick, errorMsg);

      TRI_CLOSE(fd);

      if (res != TRI_ERROR_NO_ERROR) {
        if (errorMsg.empty()) {
          errorMsg = "cannot write to file '" + fileName + "'";
        }

        return res;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief dump a single shard, that is a collection on a DBserver
int DumpFeature::dumpShard(int fd, std::string const& DBserver,
                           std::string const& name, std::string& errorMsg) {
  std::string const baseUrl = "/_api/replication/dump?DBserver=" + DBserver +
                              "&collection=" + name + "&chunkSize=" +
                              StringUtils::itoa(_chunkSize) +
                              "&ticks=false";

  uint64_t fromTick = 0;
  uint64_t maxTick = UINT64_MAX;

  while (true) {
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    _stats._totalBatches++;

    std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
        rest::RequestType::GET, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      errorMsg =
          "got invalid response from server: " + _httpClient->getErrorMessage();

      return TRI_ERROR_INTERNAL;
    }

    if (response->wasHttpError()) {
      errorMsg = getHttpErrorMessage(response.get(), nullptr);

      return TRI_ERROR_INTERNAL;
    }

    int res = TRI_ERROR_NO_ERROR;  // just to please the compiler
    bool checkMore = false;
    bool found;

    // TODO: fix hard-coded headers
    std::string header =
        response->getHeaderField("x-arango-replication-checkmore", found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        // TODO: fix hard-coded headers
        header = response->getHeaderField("x-arango-replication-lastincluded",
                                          found);

        if (found) {
          uint64_t tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          } else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (!found) {
      errorMsg = "got invalid response server: required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer const& body = response->getBody();

      if (!TRI_WritePointer(fd, body.c_str(), body.length())) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      } else {
        _stats._totalWritten += (uint64_t)body.length();
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      return res;
    }
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

// dump data from cluster via a coordinator
int DumpFeature::runClusterDump(std::string& errorMsg) {
  int res;

  std::string const url =
      "/_api/replication/clusterInventory?includeSystem=" +
      std::string(_includeSystemCollections ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + _httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from server: HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_INTERNAL;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }
  VPackSlice const body = parsedBody->slice();

  if (!body.isObject()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  VPackSlice const collections = body.get("collections");

  if (!collections.isArray()) {
    errorMsg = "got malformed JSON response from server";

    return TRI_ERROR_INTERNAL;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }

  // iterate over collections
  for (auto const& collection : VPackArrayIterator(collections)) {
    if (!collection.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }
    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    uint64_t const cid = arangodb::basics::VelocyPackHelper::extractIdValue(parameters);
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);

    if (cid == 0 || name == "") {
      errorMsg = "got malformed JSON response from server";

      return TRI_ERROR_INTERNAL;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && !_includeSystemCollections) {
      continue;
    }

    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    // found a collection!
    if (_progress) {
      std::cout << "# Dumping collection '" << name << "'..." << std::endl;
    }

    // now save the collection meta data and/or the actual data
    _stats._totalCollections++;

    {
      // save meta data
      std::string fileName =
          _outputDirectory + TRI_DIR_SEPARATOR_STR + name + ".structure.json";

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      int fd = TRI_CREATE(fileName.c_str(),
                          O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                          S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      std::string const collectionInfo = collection.toJson();

      if (!TRI_WritePointer(fd, collectionInfo.c_str(),
                            collectionInfo.size())) {
        TRI_CLOSE(fd);
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      TRI_CLOSE(fd);
    }

    if (_dumpData) {
      // save the actual data

      // Now set up the output file:
      std::string const hexString(arangodb::rest::SslInterface::sslMD5(name));
      std::string fileName = _outputDirectory + TRI_DIR_SEPARATOR_STR + name +
                             "_" + hexString + ".data.json";

      // remove an existing file first
      if (TRI_ExistsFile(fileName.c_str())) {
        TRI_UnlinkFile(fileName.c_str());
      }

      int fd = TRI_CREATE(fileName.c_str(),
                          O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                          S_IRUSR | S_IWUSR);

      if (fd < 0) {
        errorMsg = "cannot write to file '" + fileName + "'";

        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      // First we have to go through all the shards, what are they?
      VPackSlice const shards = parameters.get("shards");

      // Iterate over the Map of shardId to server list
      for (auto const it : VPackObjectIterator(shards)) {
        TRI_ASSERT(it.key.isString());

        std::string shardName = it.key.copyString();

        if (!it.value.isArray() || it.value.length() == 0 ||
            !it.value[0].isString()) {
          TRI_CLOSE(fd);
          errorMsg = "unexpected value for 'shards' attribute";

          return TRI_ERROR_BAD_PARAMETER;
        }

        std::string DBserver = it.value[0].copyString();

        if (_progress) {
          std::cout << "# Dumping shard '" << shardName << "' from DBserver '"
                    << DBserver << "' ..." << std::endl;
        }
        res = startBatch(DBserver, errorMsg);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_CLOSE(fd);
          return res;
        }
        res = dumpShard(fd, DBserver, shardName, errorMsg);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_CLOSE(fd);
          return res;
        }
        endBatch(DBserver);
      }

      res = TRI_CLOSE(fd);

      if (res != TRI_ERROR_NO_ERROR) {
        if (errorMsg.empty()) {
          errorMsg = "cannot write to file '" + fileName + "'";
        }

        return res;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

void DumpFeature::start() {
  ClientFeature* client = application_features::ApplicationServer::getFeature<ClientFeature>("Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  try {
    _httpClient = client->createHttpClient();
  } catch (...) {
    LOG(FATAL) << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  std::string dbName = client->databaseName();

  _httpClient->setLocationRewriter(static_cast<void*>(client), &rewriteLocation);
  _httpClient->setUserNamePassword("/", client->username(), client->password());

  std::string const versionString = _httpClient->getServerVersion();

  if (!_httpClient->isConnected()) {
    LOG(ERR) << "Could not connect to endpoint '" << client->endpoint()
             << "', database: '" << dbName << "', username: '"
             << client->username() << "'";
    LOG(FATAL) << "Error message: '" << _httpClient->getErrorMessage() << "'";

    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Server version: " << versionString << std::endl;

  // validate server version
  std::pair<int, int> version = Version::parseVersionString(versionString);

  if (version.first < 3) {
    // we can connect to 3.x
    LOG(ERR) << "Error: got incompatible server version '" << versionString
             << "'";

    if (!_force) {
      FATAL_ERROR_EXIT();
    }
  }

  _clusterMode = getArangoIsCluster(nullptr);

  if (_clusterMode) {
    if (_tickStart != 0 || _tickEnd != 0) {
      LOG(ERR) << "Error: cannot use tick-start or tick-end on a cluster";
      FATAL_ERROR_EXIT();
    }
  }

  if (!_httpClient->isConnected()) {
    LOG(ERR) << "Lost connection to endpoint '" << client->endpoint()
             << "', database: '" << dbName << "', username: '"
             << client->username() << "'";
    LOG(FATAL) << "Error message: '" << _httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  if (_progress) {
    std::cout << "Connected to ArangoDB '" << client->endpoint()
              << "', database: '" << dbName << "', username: '"
              << client->username() << "'" << std::endl;

    std::cout << "Writing dump to output directory '" << _outputDirectory << "'"
              << std::endl;
  }

  std::string errorMsg = "";

  int res;

  try {
    if (!_clusterMode) {
      res = startBatch("", errorMsg);

      if (res != TRI_ERROR_NO_ERROR && _force) {
        res = TRI_ERROR_NO_ERROR;
      }

      if (res == TRI_ERROR_NO_ERROR) {
        res = runDump(dbName, errorMsg);
      }

      if (_batchId > 0) {
        endBatch("");
      }
    } else {
      res = runClusterDump(errorMsg);
    }
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(ERR) << "Error: caught unknown exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (!errorMsg.empty()) {
      LOG(ERR) << errorMsg;
    } else {
      LOG(ERR) << "An error occurred";
    }
    ret = EXIT_FAILURE;
  }

  if (_progress) {
    if (_dumpData) {
      std::cout << "Processed " << _stats._totalCollections
                << " collection(s), "
                << "wrote " << _stats._totalWritten
                << " byte(s) into datafiles, "
                << "sent " << _stats._totalBatches << " batch(es)" << std::endl;
    } else {
      std::cout << "Processed " << _stats._totalCollections << " collection(s)"
                << std::endl;
    }
  }

  *_result = ret;
}
