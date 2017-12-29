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

#include <iostream>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/Result.h"
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

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace {
/// @brief check whether HTTP response is valid, complete, and not an error
Result checkHttpResponse(SimpleHttpClient& client,
                         std::unique_ptr<SimpleHttpResult>& response) {
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: " + client.getErrorMessage()};
  }
  if (response->wasHttpError()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: HTTP " +
                StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                response->getHttpReturnMessage()};
  }
  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

namespace {
/// @brief prepare file for writing
Result prepareFileForWriting(FileHandler& fileHandler,
                             std::string const& filename, int& fd,
                             bool overwrite) {
  // deal with existing file first if it exists
  bool fileExists = TRI_ExistsFile(filename.c_str());
  if (fileExists) {
    if (overwrite) {
      TRI_UnlinkFile(filename.c_str());
    } else {
      return {TRI_ERROR_CANNOT_WRITE_FILE,
              "cannot write to file '" + filename + "', already exists!"};
    }
  }

  fd = TRI_TRACKED_CREATE_FILE(filename.c_str(),
                               O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                               S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return {TRI_ERROR_CANNOT_WRITE_FILE,
            "cannot write to file '" + filename + "'"};
  }

  fileHandler.beginEncryption(fd);
  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

namespace {
/// @brief start a batch via the replication API
std::pair<Result, uint64_t> startBatch(SimpleHttpClient& client,
                                       std::string DBserver) {
  std::string const url = "/_api/replication/batch";
  std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(client.request(
      rest::RequestType::POST, url + urlExt, body.c_str(), body.size()));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return {check, 0};
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    return {{TRI_ERROR_INTERNAL, "got malformed JSON"}, 0};
  }
  VPackSlice const resBody = parsedBody->slice();

  // look up "id" value
  std::string const id =
      arangodb::basics::VelocyPackHelper::getStringValue(resBody, "id", "");

  return {{TRI_ERROR_NO_ERROR}, StringUtils::uint64(id)};
}
}  // namespace

namespace {
/// @brief prolongs a batch to ensure we can complete our dump
void extendBatch(SimpleHttpClient& client, std::string DBserver,
                 uint64_t batchId) {
  TRI_ASSERT(batchId > 0);

  std::string const url =
      "/_api/replication/batch/" + StringUtils::itoa(batchId);
  std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(client.request(
      rest::RequestType::PUT, url + urlExt, body.c_str(), body.size()));
  // ignore any return value
}
}  // namespace

namespace {
/// @brief mark our batch finished so resources can be freed on server
void endBatch(SimpleHttpClient& client, std::string DBserver,
              uint64_t& batchId) {
  TRI_ASSERT(batchId > 0);

  std::string const url =
      "/_api/replication/batch/" + StringUtils::itoa(batchId);
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<SimpleHttpResult> response(
      client.request(rest::RequestType::DELETE_REQ, url + urlExt, nullptr, 0));
  // ignore any return value

  // overwrite the input id
  batchId = 0;
}
}  // namespace

namespace {
/// @brief execute a WAL flush request
void flushWal(httpclient::SimpleHttpClient& client) {
  std::string const url =
      "/_admin/wal/flush?waitForSync=true&waitForCollector=true";

  std::unique_ptr<SimpleHttpResult> response(
      client.request(rest::RequestType::PUT, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    // TODO should we abort early here?
    std::cerr << "got invalid response from server: " + check.errorMessage()
              << std::endl;
  }
}
}  // namespace

namespace {
/// @brief dump the actual data from an individual collection
Result dumpCollection(httpclient::SimpleHttpClient& client,
                      DumpFeatureJobData& jobData, int fd, std::string const& name, std::string const& server, uint64_t batchId, uint64_t minTick, uint64_t maxTick) {
  uint64_t fromTick = minTick;
  uint64_t chunkSize =
      jobData.initialChunkSize;  // will grow adaptively up to max
  std::string baseUrl =
      "/_api/replication/dump?collection=" + name +
      "&batchId=" + StringUtils::itoa(batchId) +
      "&ticks=false&flush=false";
  if (server != "") {
    baseUrl += "&DBserver=" + server;
  }

  while (true) {
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick) +
                      "&chunkSize=" + StringUtils::itoa(chunkSize);
    if (maxTick > 0) {  // limit to a certain timeframe
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    jobData.stats.totalBatches++;  // count how many chunks we are fetching

    // make the actual request for data
    std::unique_ptr<SimpleHttpResult> response(
        client.request(rest::RequestType::GET, url, nullptr, 0));
    auto check = ::checkHttpResponse(client, response);
    if (check.fail()) {
      return check;
    }

    // find out whether there are more results to fetch
    bool checkMore = false;

    // TODO: fix hard-coded headers
    bool headerExtracted;
    std::string header = response->getHeaderField(
        "x-arango-replication-checkmore", headerExtracted);
    if (headerExtracted) {
      // first check the basic flag
      checkMore = StringUtils::boolean(header);
      if (checkMore) {
        // TODO: fix hard-coded headers
        // now check if the actual tick has changed
        header = response->getHeaderField("x-arango-replication-lastincluded",
                                          headerExtracted);
        if (headerExtracted) {
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
    if (!headerExtracted) {  // not else, fallthrough from outer or inner above
      return {TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              "got invalid response server: required header is missing"};
    }

    // now actually write retrieved data to dump file
    StringBuffer const& body = response->getBody();
    bool written =
        jobData.fileHandler.writeData(fd, body.c_str(), body.length());
    if (!written) {
      return {TRI_ERROR_CANNOT_WRITE_FILE};
    } else {
      jobData.stats.totalWritten += (uint64_t)body.length();
    }

    if (!checkMore || fromTick == 0) {
      // all done, return successful
      return {TRI_ERROR_NO_ERROR};
    }

    // more data to retrieve, adaptively increase chunksize
    if (chunkSize < jobData.maxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);
      if (chunkSize > jobData.maxChunkSize) {
        chunkSize = jobData.maxChunkSize;
      }
    }
  }

  // should never get here, but need to make compiler play nice
  TRI_ASSERT(false);
  return {TRI_ERROR_INTERNAL};
}
}  // namespace

namespace {
/// @brief processes a single collection dumping job in single-server mode
Result handleCollection(httpclient::SimpleHttpClient& client,
                        DumpFeatureJobData& jobData) {
  Result result;
  // prep hex string of job name
  std::string const hexString(
      arangodb::rest::SslInterface::sslMD5(jobData.name));

  if (jobData.showProgress) {
    std::cout << "# Dumping " << jobData.type << " collection '" << jobData.name
              << "'..." << std::endl;
  }
  jobData.stats.totalCollections++;

  // now save the collection meta data
  {
    std::string filename = jobData.outputDirectory + TRI_DIR_SEPARATOR_STR +
                           jobData.name + "_" + hexString + ".structure.json";
    int fd;
    result = prepareFileForWriting(jobData.fileHandler, filename, fd, true);
    if (result.fail()) {
      return result;
    }

    // write data out to file
    std::string const collectionInfo = jobData.collectionInfo.toJson();
    bool written = jobData.fileHandler.writeData(fd, collectionInfo.c_str(),
                                                 collectionInfo.size());
    if (!written) {
      result = {TRI_ERROR_CANNOT_WRITE_FILE,
                "cannot write to file '" + filename + "'"};
    }

    // close file
    jobData.fileHandler.endEncryption(fd);
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  // save the actual data if requested
  if (result.ok() && jobData.dumpData) {
    std::string filename = jobData.outputDirectory + TRI_DIR_SEPARATOR_STR +
                           jobData.name + "_" + hexString + ".data.json";
    int fd;
    result = prepareFileForWriting(jobData.fileHandler, filename, fd, true);
    if (result.fail()) {
      return result;
    }

    // keep the batch alive
    ::extendBatch(client, "", jobData.batchId);

    // do the hard work in another function...
    result = ::dumpCollection(client, jobData, fd, jobData.name, "", jobData.batchId, jobData.tickStart, jobData.maxTick);

    // close file
    jobData.fileHandler.endEncryption(fd);
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  return result;
}
}  // namespace

/*namespace {
// TODO FIXME eliminate `dumpShard`, unify with `dumpCollection`
/// @brief dump the actual data from an individual shard
Result dumpShard(httpclient::SimpleHttpClient& client,
                 DumpFeatureJobData& jobData, std::string const& DBserver,
                 int fd, std::string const& name) {
                   // no setting tick ranges in cluster mode, only full-range
                   uint64_t fromTick = 0;
                   uint64_t maxTick = UINT64_MAX;
  uint64_t chunkSize = jobData.initialChunkSize; // will grow adaptively to max

  std::string const baseUrl = "/_api/replication/dump?DBserver=" + DBserver +
                              "&batchId=" + StringUtils::itoa(jobData.batchId) +
                              "&collection=" + shardName + "&ticks=false";

  while (true) {
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick) +
                      "&chunkSize=" + StringUtils::itoa(chunkSize);
    if (jobData.maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    jobData.stats.totalBatches++; // count how many chunks we are fetching

    // make the actual request for data
    std::unique_ptr<SimpleHttpResult> response(
        client.request(rest::RequestType::GET, url, nullptr, 0));
    auto check = checkHttpResponse(client, response);
    if (check.fail()) {
      return check;
    }

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
      return {TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              "got invalid response server: required header is missing"};
    }

    if (res == TRI_ERROR_NO_ERROR) {
      StringBuffer const& body = response->getBody();
      bool result =
          jobData.fileHandler.writeData(fd, body.c_str(), body.length());

      if (!result) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
      } else {
        jobData.stats.totalWritten += (uint64_t)body.length();
      }
    }

    if (res != TRI_ERROR_NO_ERROR || !checkMore || fromTick == 0) {
      // done
      return {res};
    }

    if (chunkSize < jobData.maxChunkSize) {
      // adaptively increase chunksize
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);

      if (chunkSize > jobData.maxChunkSize) {
        chunkSize = jobData.maxChunkSize;
      }
    }
  }

  TRI_ASSERT(false);
  return {TRI_ERROR_INTERNAL};
}
}  // namespace*/

namespace {
/// @brief handle a single collection dumping job in cluster mode
Result handleCollectionCluster(httpclient::SimpleHttpClient& client,
                               DumpFeatureJobData& jobData) {
  // found a collection!
  if (jobData.showProgress) {
    std::cout << "# Dumping collection '" << jobData.name << "'..."
              << std::endl;
  }

  // now save the collection meta data and/or the actual data
  jobData.stats.totalCollections++;

  {
    // save meta data
    std::string fileName = jobData.outputDirectory + TRI_DIR_SEPARATOR_STR +
                           jobData.name + ".structure.json";

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    int fd = TRI_TRACKED_CREATE_FILE(fileName.c_str(),
                                     O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                                     S_IRUSR | S_IWUSR);

    if (fd < 0) {
      return {TRI_ERROR_CANNOT_WRITE_FILE,
              "cannot write to file '" + fileName + "'"};
    }

    jobData.fileHandler.beginEncryption(fd);

    std::string const collectionInfo = jobData.collectionInfo.toJson();
    bool result = jobData.fileHandler.writeData(fd, collectionInfo.c_str(),
                                                collectionInfo.size());

    if (!result) {
      jobData.fileHandler.endEncryption(fd);
      TRI_TRACKED_CLOSE_FILE(fd);
      return {TRI_ERROR_CANNOT_WRITE_FILE,
              "cannot write to file '" + fileName + "'"};
    }

    jobData.fileHandler.endEncryption(fd);
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  if (jobData.dumpData) {
    // save the actual data

    // Now set up the output file:
    std::string const hexString(
        arangodb::rest::SslInterface::sslMD5(jobData.name));
    std::string fileName = jobData.outputDirectory + TRI_DIR_SEPARATOR_STR +
                           jobData.name + "_" + hexString + ".data.json";

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    int fd = TRI_TRACKED_CREATE_FILE(fileName.c_str(),
                                     O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                                     S_IRUSR | S_IWUSR);

    if (fd < 0) {
      return {TRI_ERROR_CANNOT_WRITE_FILE,
              "cannot write to file '" + fileName + "'"};
    }

    jobData.fileHandler.beginEncryption(fd);

    // First we have to go through all the shards, what are they?
    VPackSlice const parameters = jobData.collectionInfo.get("parameters");
    VPackSlice const shards = parameters.get("shards");

    // Iterate over the Map of shardId to server list
    for (auto const it : VPackObjectIterator(shards)) {
      TRI_ASSERT(it.key.isString());

      std::string shardName = it.key.copyString();

      if (!it.value.isArray() || it.value.length() == 0 ||
          !it.value[0].isString()) {
        jobData.fileHandler.endEncryption(fd);
        TRI_TRACKED_CLOSE_FILE(fd);
        return {TRI_ERROR_BAD_PARAMETER,
                "unexpected value for 'shards' attribute"};
      }

      std::string DBserver = it.value[0].copyString();

      if (jobData.showProgress) {
        std::cout << "# Dumping shard '" << shardName << "' from DBserver '"
                  << DBserver << "' ..." << std::endl;
      }

      Result result;
      uint64_t batchId;
      std::tie(result, batchId) = ::startBatch(client, DBserver);
      if (result.fail()) {
        jobData.fileHandler.endEncryption(fd);
        TRI_TRACKED_CLOSE_FILE(fd);
        return result;
      }

      result = ::dumpCollection(client, jobData, fd, shardName, DBserver, batchId, 0, UINT64_MAX);
      if (result.fail()) {
        jobData.fileHandler.endEncryption(fd);
        TRI_TRACKED_CLOSE_FILE(fd);
        return result;
      }

      ::endBatch(client, DBserver, batchId);
    }

    jobData.fileHandler.endEncryption(fd);
    int res = TRI_TRACKED_CLOSE_FILE(fd);
    if (res != TRI_ERROR_NO_ERROR) {
      return {res};
    }
  }

  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

namespace {
/// @brief process a single job from the queue
Result processJob(httpclient::SimpleHttpClient& client,
                  DumpFeatureJobData& jobData) noexcept {
  if (jobData.clusterMode) {
    return ::handleCollectionCluster(client, jobData);
  }

  return ::handleCollection(client, jobData);
}
}  // namespace

namespace {
/// @brief handle the result of a single job
void handleJobResult(std::unique_ptr<DumpFeatureJobData>&& jobData,
                     Result const& result) noexcept {
  // notify progress?
}
}  // namespace

DumpFeatureStats::DumpFeatureStats(uint64_t b, uint64_t c, uint64_t w) noexcept
    : totalBatches{b}, totalCollections{c}, totalWritten{w} {}

DumpFeatureJobData::DumpFeatureJobData(
    DumpFeature& feat, FileHandler& handler, VPackSlice const& info,
    std::string const& c, std::string const& n, std::string const& t,
    uint64_t const& batch, uint64_t const& tStart, uint64_t const& tMax,
    uint64_t const& chunkInitial, uint64_t const& chunkMax, bool const& cluster,
    bool const& prog, bool const& data, std::string const& directory,
    DumpFeatureStats& s) noexcept
    : feature{feat},
      fileHandler{handler},
      collectionInfo{info},
      cid{c},
      name{n},
      type{t},
      batchId{batch},
      tickStart{tStart},
      maxTick{tMax},
      initialChunkSize{chunkInitial},
      maxChunkSize{chunkMax},
      clusterMode{cluster},
      showProgress{prog},
      dumpData{data},
      outputDirectory{directory},
      stats{s} {}

DumpFeature::DumpFeature(application_features::ApplicationServer* server,
                         int* result)
    : ApplicationFeature(server, DumpFeature::featureName()),
      _result{result},
      _clientManager{},
      _clientTaskQueue{::processJob, ::handleJobResult},
      _fileHandler{},
      _stats{0, 0, 0},
      _collections{},
      _chunkSize{1024 * 1024 * 8},
      _maxChunkSize{1024 * 1024 * 64},
      _dumpData{true},
      _force{false},
      _ignoreDistributeShardsLikeErrors{false},
      _includeSystemCollections{false},
      _overwrite{false},
      _progress{true},
      _clusterMode{false},
      _batchId{0},
      _tickStart{0},
      _tickEnd{0},
      _outputDirectory{} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Logger");

#ifdef USE_ENTERPRISE
  startsAfter("Encryption");
#endif

  _outputDirectory =
      FileUtils::buildFilename(FileUtils::currentDirectory().result(), "dump");
}

std::string DumpFeature::featureName() { return "Dump"; }

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
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_maxChunkSize));

  options->addOption("--dump-data", "dump collection data",
                     new BooleanParameter(&_dumpData));

  options->addOption(
      "--force", "continue dumping even in the face of some server-side errors",
      new BooleanParameter(&_force));

  options->addOption("--ignore-distribute-shards-like-errors",
                     "continue dump even if sharding prototype collection is "
                     "not backed up along",
                     new BooleanParameter(&_ignoreDistributeShardsLikeErrors));

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
}

void DumpFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _outputDirectory = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expecting at most one directory, got " +
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
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid values for --tick-start or --tick-end";
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
      std::vector<std::string> files(
          TRI_FullTreeDirectory(_outputDirectory.c_str()));
      // we don't care if the target directory is empty
      isEmptyDirectory = (files.size() <= 1);  // TODO: TRI_FullTreeDirectory
                                               // always returns at least one
                                               // element (""), even if
                                               // directory is empty?
    }
  }

  if (_outputDirectory.empty() ||
      (TRI_ExistsFile(_outputDirectory.c_str()) && !isDirectory)) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot write to output directory '" << _outputDirectory << "'";
    FATAL_ERROR_EXIT();
  }

  if (isDirectory && !isEmptyDirectory && !_overwrite) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "output directory '" << _outputDirectory
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
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "unable to create output directory '" << _outputDirectory
          << "': " << errorMessage;
      FATAL_ERROR_EXIT();
    }
  }
}

std::string DumpFeature::getHttpErrorMessage(
    httpclient::SimpleHttpResult* result, int& err) noexcept {
  return _clientManager.getHttpErrorMessage(result, err);
}

// dump data from server
Result DumpFeature::runDump(std::string& dbName) {
  std::string const url =
      "/_api/replication/inventory?includeSystem=" +
      std::string(_includeSystemCollections ? "true" : "false") +
      "&batchId=" + StringUtils::itoa(_batchId);

  auto httpClient = _clientManager.getConnectedClient();
  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(rest::RequestType::GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL, "got invalid response from server: " +
                                    httpClient->getErrorMessage()};
  }

  if (response->wasHttpError()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: HTTP " +
                StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                response->getHttpReturnMessage()};
  }

  flushWal(*httpClient);
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
  }
  VPackSlice const body = parsedBody->slice();

  if (!body.isObject()) {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
  }

  VPackSlice const collections = body.get("collections");

  if (!collections.isArray()) {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
  }

  // read the server's max tick value
  std::string const tickString =
      arangodb::basics::VelocyPackHelper::getStringValue(body, "tick", "");

  if (tickString == "") {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
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
    meta.close();

    // save last tick in file
    std::string fileName =
        _outputDirectory + TRI_DIR_SEPARATOR_STR + "dump.json";

    int fd;

    // remove an existing file first
    if (TRI_ExistsFile(fileName.c_str())) {
      TRI_UnlinkFile(fileName.c_str());
    }

    fd = TRI_TRACKED_CREATE_FILE(fileName.c_str(),
                                 O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                                 S_IRUSR | S_IWUSR);

    if (fd < 0) {
      return {TRI_ERROR_CANNOT_WRITE_FILE,
              "cannot write to file '" + fileName + "'"};
    }

    _fileHandler.beginEncryption(fd);

    std::string const metaString = meta.slice().toJson();
    bool result =
        _fileHandler.writeData(fd, metaString.c_str(), metaString.size());

    if (!result) {
      _fileHandler.endEncryption(fd);
      TRI_TRACKED_CLOSE_FILE(fd);
      return {TRI_ERROR_CANNOT_WRITE_FILE,
              "cannot write to file '" + fileName + "'"};
    }

    _fileHandler.endEncryption(fd);
    TRI_TRACKED_CLOSE_FILE(fd);
  } catch (basics::Exception const& ex) {
    return {ex.code(), ex.what()};
  } catch (std::exception const& ex) {
    return {TRI_ERROR_INTERNAL, ex.what()};
  } catch (...) {
    return {TRI_ERROR_OUT_OF_MEMORY, "out of memory"};
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }

  // iterate over collections
  for (VPackSlice const& collection : VPackArrayIterator(collections)) {
    if (!collection.isObject()) {
      return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
    }

    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
    }

    uint64_t const cid =
        arangodb::basics::VelocyPackHelper::extractIdValue(parameters);
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);
    int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        parameters, "type", 2);
    std::string const collectionType(type == 2 ? "document" : "edge");

    if (cid == 0 || name == "") {
      return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
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

    auto jobData = std::make_unique<DumpFeatureJobData>(
        *this, _fileHandler, collection, std::to_string(cid), name,
        collectionType, _batchId, _tickStart, maxTick, _chunkSize,
        _maxChunkSize, _clusterMode, _progress, _dumpData, _outputDirectory,
        _stats);
    auto result = processJob(*httpClient, *jobData);
    if (result.fail()) {
      return result;
    }
  }

  {
    // wait for jobs and handle errors?
  }

  return {TRI_ERROR_NO_ERROR};
}

// dump data from cluster via a coordinator
Result DumpFeature::runClusterDump() {
  std::string const url =
      "/_api/replication/clusterInventory?includeSystem=" +
      std::string(_includeSystemCollections ? "true" : "false");

  auto httpClient = _clientManager.getConnectedClient();
  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(rest::RequestType::GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL, "got invalid response from server: " +
                                    httpClient->getErrorMessage()};
  }

  if (response->wasHttpError()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: HTTP " +
                StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                response->getHttpReturnMessage()};
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
  }

  VPackSlice const collections = body.get("collections");

  if (!collections.isArray()) {
    return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }

  // iterate over collections
  for (auto const& collection : VPackArrayIterator(collections)) {
    if (!collection.isObject()) {
      return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
    }
    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
    }

    uint64_t const cid =
        arangodb::basics::VelocyPackHelper::extractIdValue(parameters);
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);

    if (cid == 0 || name == "") {
      return {TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
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

    if (!_ignoreDistributeShardsLikeErrors) {
      std::string prototypeCollection =
          arangodb::basics::VelocyPackHelper::getStringValue(
              parameters, "distributeShardsLike", "");

      if (!prototypeCollection.empty() && !restrictList.empty()) {
        if (std::find(_collections.begin(), _collections.end(),
                      prototypeCollection) == _collections.end()) {
          return {
              TRI_ERROR_INTERNAL,
              std::string("Collection ") + name +
                  "'s shard distribution is based on a that of collection " +
                  prototypeCollection +
                  ", which is not dumped along. You may dump the collection "
                  "regardless of the missing prototype collection by using the "
                  "--ignore-distribute-shards-like-errors parameter."};
        }
      }
    }

    auto jobData = std::make_unique<DumpFeatureJobData>(
        *this, _fileHandler, collection, std::to_string(cid), name,
        "" /*collectionType*/, _batchId, _tickStart, 0 /*maxTick*/, _chunkSize,
        _maxChunkSize, _clusterMode, _progress, _dumpData, _outputDirectory,
        _stats);
    auto result = processJob(*httpClient, *jobData);
    if (result.fail()) {
      return result;
    }
  }

  {
    // wait for jobs and handle errors?
  }

  return {TRI_ERROR_NO_ERROR};
}

void DumpFeature::start() {
  _fileHandler.initializeEncryption();

  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  std::string dbName = client->databaseName();

  auto httpClient = _clientManager.getConnectedClient(_force, true);

  int errorNum = {TRI_ERROR_NO_ERROR};
  _clusterMode = _clientManager.getArangoIsCluster(errorNum, *httpClient);
  if (errorNum != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Error: could not detect ArangoDB instance type";
    FATAL_ERROR_EXIT();
  }

  if (_clusterMode) {
    if (_tickStart != 0 || _tickEnd != 0) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "Error: cannot use tick-start or tick-end on a cluster";
      FATAL_ERROR_EXIT();
    }
  }

  if (!httpClient->isConnected()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Lost connection to endpoint '" << client->endpoint()
        << "', database: '" << dbName << "', username: '" << client->username()
        << "'";
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "Error message: '" << httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  if (_progress) {
    std::cout << "Connected to ArangoDB '" << client->endpoint()
              << "', database: '" << dbName << "', username: '"
              << client->username() << "'" << std::endl;

    std::cout << "Writing dump to output directory '" << _outputDirectory << "'"
              << std::endl;
  }

#ifdef USE_ENTERPRISE
  if (_encryption != nullptr) {
    _encryption->writeEncryptionFile(_outputDirectory);
  }
#endif

  Result res;
  try {
    if (!_clusterMode) {
      std::tie(res, _batchId) = ::startBatch(*httpClient, "");
      if (res.ok()) {
        res = runDump(dbName);
      }

      if (_batchId > 0) {
        ::endBatch(*httpClient, "", _batchId);
      }
    } else {
      res = runClusterDump();
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception " << ex.what();
    res = {TRI_ERROR_INTERNAL};
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Error: caught unknown exception";
    res = {TRI_ERROR_INTERNAL};
  }

  if (res.fail()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "An error occurred: " + res.errorMessage();
    ret = EXIT_FAILURE;
  }

  if (_progress) {
    if (_dumpData) {
      std::cout << "Processed " << _stats.totalCollections.load()
                << " collection(s), wrote " << _stats.totalWritten.load()
                << " byte(s) into datafiles, sent "
                << _stats.totalBatches.load() << " batch(es)" << std::endl;
    } else {
      std::cout << "Processed " << _stats.totalCollections << " collection(s)"
                << std::endl;
    }
  }

  *_result = ret;
}
