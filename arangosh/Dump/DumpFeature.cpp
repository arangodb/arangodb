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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "DumpFeature.h"

#include <chrono>
#include <iostream>
#include <thread>

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/algorithm/clamp.hpp>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "Utils/ManagedDirectory.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace {
/// @brief name of the feature to report to application server
constexpr auto FeatureName = "Dump";

/// @brief minimum amount of data to fetch from server in a single batch
constexpr uint64_t MinChunkSize = 1024 * 128;

/// @brief maximum amount of data to fetch from server in a single batch
constexpr uint64_t MaxChunkSize = 1024 * 1024 * 96;
// NB: larger value may cause tcp issues (check exact limits)
}  // namespace

namespace {
/// @brief generic error for if server returns bad/unexpected json
const arangodb::Result ErrorMalformedJsonResponse = {
    TRI_ERROR_INTERNAL, "got malformed JSON response from server"};
}  // namespace

namespace {
/// @brief check whether HTTP response is valid, complete, and not an error
arangodb::Result checkHttpResponse(
    arangodb::httpclient::SimpleHttpClient& client,
    std::unique_ptr<arangodb::httpclient::SimpleHttpResult> const& response) {
  using arangodb::basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: " + client.getErrorMessage()};
  }
  if (response->wasHttpError()) {
    return {TRI_ERROR_INTERNAL, "got invalid response from server: HTTP " +
                                    itoa(response->getHttpReturnCode()) + ": " +
                                    response->getHttpReturnMessage()};
  }
  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

namespace {
/// @brief checks that a file pointer is valid and file status is ok
bool fileOk(arangodb::ManagedDirectory::File* file) {
  return (file && file->status().ok());
}
}  // namespace

namespace {
/// @brief assuming file pointer is not ok, generate/extract proper error
arangodb::Result fileError(arangodb::ManagedDirectory::File* file,
                           bool isWritable) {
  if (!file) {
    if (isWritable) {
      return {TRI_ERROR_CANNOT_WRITE_FILE};
    } else {
      return {TRI_ERROR_CANNOT_READ_FILE};
    }
  }
  return file->status();
}
}  // namespace

namespace {
/// @brief start a batch via the replication API
std::pair<arangodb::Result, uint64_t> startBatch(
    arangodb::httpclient::SimpleHttpClient& client, std::string DBserver) {
  using arangodb::basics::VelocyPackHelper;
  using arangodb::basics::StringUtils::uint64;

  static std::string const url = "/_api/replication/batch";
  static std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url + urlExt,
                     body.c_str(), body.size()));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return {check, 0};
  }

  // extract vpack body from response
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    return {::ErrorMalformedJsonResponse, 0};
  }
  VPackSlice const resBody = parsedBody->slice();

  // look up "id" value
  std::string const id = VelocyPackHelper::getStringValue(resBody, "id", "");

  return {{TRI_ERROR_NO_ERROR}, uint64(id)};
}
}  // namespace

namespace {
/// @brief prolongs a batch to ensure we can complete our dump
void extendBatch(arangodb::httpclient::SimpleHttpClient& client,
                 std::string DBserver, uint64_t batchId) {
  using arangodb::basics::StringUtils::itoa;
  TRI_ASSERT(batchId > 0);

  std::string const url = "/_api/replication/batch/" + itoa(batchId);
  static std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::PUT, url + urlExt,
                     body.c_str(), body.size()));
  // ignore any return value
}
}  // namespace

namespace {
/// @brief mark our batch finished so resources can be freed on server
void endBatch(arangodb::httpclient::SimpleHttpClient& client,
              std::string DBserver, uint64_t& batchId) {
  using arangodb::basics::StringUtils::itoa;
  TRI_ASSERT(batchId > 0);

  std::string const url = "/_api/replication/batch/" + itoa(batchId);
  std::string urlExt;
  if (!DBserver.empty()) {
    urlExt = "?DBserver=" + DBserver;
  }

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::DELETE_REQ, url + urlExt,
                     nullptr, 0));
  // ignore any return value

  // overwrite the input id
  batchId = 0;
}
}  // namespace

namespace {
/// @brief execute a WAL flush request
void flushWal(arangodb::httpclient::SimpleHttpClient& client) {
  static std::string const url =
      "/_admin/wal/flush?waitForSync=true&waitForCollector=true";

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::PUT, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    // TODO should we abort early here?
    LOG_TOPIC(ERR, arangodb::Logger::DUMP)
        << "got invalid response from server: " + check.errorMessage();
  }
}
}  // namespace

namespace {
/// @brief dump the actual data from an individual collection
arangodb::Result dumpCollection(arangodb::httpclient::SimpleHttpClient& client,
                                arangodb::DumpFeature::JobData& jobData,
                                arangodb::ManagedDirectory::File& file,
                                std::string const& name,
                                std::string const& server, uint64_t batchId,
                                uint64_t minTick, uint64_t maxTick) {
  using arangodb::basics::StringUtils::boolean;
  using arangodb::basics::StringUtils::itoa;
  using arangodb::basics::StringUtils::uint64;

  uint64_t fromTick = minTick;
  uint64_t chunkSize =
      jobData.initialChunkSize;  // will grow adaptively up to max
  std::string baseUrl = "/_api/replication/dump?collection=" + name +
                        "&batchId=" + itoa(batchId) + "&ticks=false";
  if (jobData.clusterMode) {
    // we are in cluster mode, must specify dbserver
    baseUrl += "&DBserver=" + server;
  } else {
    // we are in single-server mode, we already flushed the wal
    baseUrl += "&flush=false";
  }

  while (true) {
    std::string url =
        baseUrl + "&from=" + itoa(fromTick) + "&chunkSize=" + itoa(chunkSize);
    if (maxTick > 0) {  // limit to a certain timeframe
      url += "&to=" + itoa(maxTick);
    }

    ++(jobData.stats.totalBatches);  // count how many chunks we are fetching

    // make the actual request for data
    std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
        client.request(arangodb::rest::RequestType::GET, url, nullptr, 0));
    auto check = ::checkHttpResponse(client, response);
    if (check.fail()) {
      return check;
    }

    // find out whether there are more results to fetch
    bool checkMore = false;

    bool headerExtracted;
    std::string header = response->getHeaderField(
        arangodb::StaticStrings::ReplicationHeaderCheckMore, headerExtracted);
    if (headerExtracted) {
      // first check the basic flag
      checkMore = boolean(header);
      if (checkMore) {
        // TODO: fix hard-coded headers
        // now check if the actual tick has changed
        header = response->getHeaderField(
            arangodb::StaticStrings::ReplicationHeaderLastIncluded,
            headerExtracted);
        if (headerExtracted) {
          uint64_t tick = uint64(header);
          if (tick > fromTick) {
            fromTick = tick;
          } else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }
    if (!headerExtracted) {  // NOT else, fallthrough from outer or inner above
      return {TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              "got invalid response server: required header is missing"};
    }

    // now actually write retrieved data to dump file
    arangodb::basics::StringBuffer const& body = response->getBody();
    file.write(body.c_str(), body.length());
    if (file.status().fail()) {
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
arangodb::Result handleCollection(
    arangodb::httpclient::SimpleHttpClient& client,
    arangodb::DumpFeature::JobData& jobData,
    arangodb::ManagedDirectory::File& file) {
  // keep the batch alive
  ::extendBatch(client, "", jobData.batchId);

  // do the hard work in another function...
  return ::dumpCollection(client, jobData, file, jobData.name, "",
                          jobData.batchId, jobData.tickStart, jobData.tickEnd);
}
}  // namespace

namespace {
/// @brief handle a single collection dumping job in cluster mode
arangodb::Result handleCollectionCluster(
    arangodb::httpclient::SimpleHttpClient& client,
    arangodb::DumpFeature::JobData& jobData,
    arangodb::ManagedDirectory::File& file) {
  arangodb::Result result{TRI_ERROR_NO_ERROR};

  // First we have to go through all the shards, what are they?
  VPackSlice const parameters = jobData.collectionInfo.get("parameters");
  VPackSlice const shards = parameters.get("shards");

  // Iterate over the Map of shardId to server list
  for (auto const it : VPackObjectIterator(shards)) {
    // extract shard name
    TRI_ASSERT(it.key.isString());
    std::string shardName = it.key.copyString();

    // extract dbserver id
    if (!it.value.isArray() || it.value.length() == 0 ||
        !it.value[0].isString()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "unexpected value for 'shards' attribute"};
    }
    std::string DBserver = it.value[0].copyString();

    if (jobData.showProgress) {
      LOG_TOPIC(INFO, arangodb::Logger::DUMP)
          << "# Dumping shard '" << shardName << "' from DBserver '" << DBserver
          << "' ...";
    }

    // make sure we have a batch on this dbserver
    uint64_t batchId;
    std::tie(result, batchId) = ::startBatch(client, DBserver);
    if (result.ok()) {
      // do the hard work elsewhere
      result = ::dumpCollection(client, jobData, file, shardName, DBserver,
                                batchId, 0, UINT64_MAX);
      ::endBatch(client, DBserver, batchId);
    }

    if (result.fail()) {
      // fail early for collection if a given shard fails
      break;
    }
  }

  return result;
}
}  // namespace

namespace {
/// @brief process a single job from the queue
arangodb::Result processJob(arangodb::httpclient::SimpleHttpClient& client,
                            arangodb::DumpFeature::JobData& jobData) {
  arangodb::Result result{TRI_ERROR_NO_ERROR};

  // prep hex string of collection name
  std::string const hexString(
      arangodb::rest::SslInterface::sslMD5(jobData.name));

  // found a collection!
  if (jobData.showProgress) {
    LOG_TOPIC(INFO, arangodb::Logger::DUMP)
        << "# Dumping collection '" << jobData.name << "'...";
  }
  ++(jobData.stats.totalCollections);

  {
    // save meta data
    auto file = jobData.directory.writableFile(
        jobData.name + (jobData.clusterMode ? "" : ("_" + hexString)) +
            ".structure.json",
        true);
    if (!::fileOk(file.get())) {
      return ::fileError(file.get(), true);
    }

    VPackBuilder excludes;
    { // { parameters: { shadowCollections: null } }
      excludes.add(VPackValue(VPackValueType::Object));
      excludes.add("parameters", VPackValue(VPackValueType::Object));
      excludes.add("shadowCollections", VPackSlice::nullSlice());
      excludes.close();
      excludes.close();
    }

    VPackBuilder collectionWithExcludedParametersBuilder
      = VPackCollection::merge(jobData.collectionInfo, excludes.slice(), true, true);

    std::string const collectionInfo =
      collectionWithExcludedParametersBuilder.slice().toJson();

    file->write(collectionInfo.c_str(), collectionInfo.size());
    if (file->status().fail()) {
      // close file and bail out
      result = file->status();
    }
  }

  if (result.ok() && jobData.dumpData) {
    // save the actual data
    auto file = jobData.directory.writableFile(
        jobData.name + "_" + hexString + ".data.json", true);
    if (!::fileOk(file.get())) {
      return ::fileError(file.get(), true);
    }

    if (jobData.clusterMode) {
      result = ::handleCollectionCluster(client, jobData, *file);
    } else {
      result = ::handleCollection(client, jobData, *file);
    }
  }

  return result;
}
}  // namespace

namespace {
/// @brief handle the result of a single job
void handleJobResult(std::unique_ptr<arangodb::DumpFeature::JobData>&& jobData,
                     arangodb::Result const& result) {
  if (result.fail()) {
    jobData->feature.reportError(result);
  }
}
}  // namespace

namespace arangodb {

DumpFeature::Stats::Stats(uint64_t b, uint64_t c, uint64_t w)
    : totalBatches{b}, totalCollections{c}, totalWritten{w} {}

DumpFeature::JobData::JobData(DumpFeature& feat, ManagedDirectory& dir,
                              Stats& stat, VPackSlice const& info,
                              bool const& cluster, bool const& prog,
                              bool const& data, uint64_t const& chunkInitial,
                              uint64_t const& chunkMax, uint64_t const& tStart,
                              uint64_t const& tEnd, uint64_t const batch,
                              std::string const& c, std::string const& n,
                              std::string const& t)
    : feature{feat},
      directory{dir},
      stats{stat},
      collectionInfo{info},
      clusterMode{cluster},
      showProgress{prog},
      dumpData{data},
      initialChunkSize{chunkInitial},
      maxChunkSize{chunkMax},
      tickStart{tStart},
      tickEnd{tEnd},
      batchId{batch},
      cid{c},
      name{n},
      type{t} {}

DumpFeature::DumpFeature(application_features::ApplicationServer* server,
                         int& exitCode)
    : ApplicationFeature(server, DumpFeature::featureName()),
      _exitCode{exitCode},
      _clientManager{Logger::DUMP},
      _clientTaskQueue{::processJob, ::handleJobResult},
      _directory{nullptr},
      _stats{0, 0, 0},
      _collections{},
      _workerErrorLock{},
      _workerErrors{},
      _dumpData{true},
      _force{false},
      _ignoreDistributeShardsLikeErrors{false},
      _includeSystemCollections{false},
      _overwrite{false},
      _progress{true},
      _clusterMode{false},
      _initialChunkSize{1024 * 1024 * 8},
      _maxChunkSize{1024 * 1024 * 64},
      _tickStart{0},
      _tickEnd{0},
      _threadCount{2},
      _outputPath{arangodb::basics::FileUtils::buildFilename(
          arangodb::basics::FileUtils::currentDirectory().result(), "dump")} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Logger");

#ifdef USE_ENTERPRISE
  startsAfter("Encryption");
#endif
}

std::string DumpFeature::featureName() { return ::FeatureName; }

void DumpFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::options::BooleanParameter;
  using arangodb::options::StringParameter;
  using arangodb::options::UInt32Parameter;
  using arangodb::options::UInt64Parameter;
  using arangodb::options::VectorParameter;

  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_collections));

  options->addOption("--initial-batch-size",
                     "initial size for individual data batches (in bytes)",
                     new UInt64Parameter(&_initialChunkSize));

  options->addOption("--batch-size",
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_maxChunkSize));

  options->addOption("--threads",
                     "maximum number of collections to process in parallel",
                     new UInt32Parameter(&_threadCount));

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
                     new StringParameter(&_outputPath));

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
    _outputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expecting at most one directory, got " +
               arangodb::basics::StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // clamp chunk values to allowed ranges
  _initialChunkSize = boost::algorithm::clamp(_initialChunkSize, ::MinChunkSize,
                                              ::MaxChunkSize);
  _maxChunkSize =
      boost::algorithm::clamp(_maxChunkSize, _initialChunkSize, ::MaxChunkSize);

  if (_tickStart < _tickEnd) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid values for --tick-start or --tick-end";
    FATAL_ERROR_EXIT();
  }

  // trim trailing slash from path because it may cause problems on ...
  // Windows
  if (!_outputPath.empty() && _outputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
    TRI_ASSERT(_outputPath.size() > 0);
    _outputPath.pop_back();
  }

  uint32_t clamped = boost::algorithm::clamp(
      _threadCount, 1, 4 * static_cast<uint32_t>(TRI_numberProcessors()));
  if (_threadCount != clamped) {
    LOG_TOPIC(WARN, Logger::FIXME) << "capping --threads value to " << clamped;
    _threadCount = clamped;
  }
}

// dump data from server
Result DumpFeature::runDump(httpclient::SimpleHttpClient& client,
                            std::string& dbName) {
  Result result;
  uint64_t batchId;
  std::tie(result, batchId) = ::startBatch(client, "");
  if (result.fail()) {
    return result;
  }
  TRI_DEFER(::endBatch(client, "", batchId));

  // flush the wal and so we know we are getting everything
  flushWal(client);

  // fetch the collection inventory
  std::string const url =
      "/_api/replication/inventory?includeSystem=" +
      std::string(_includeSystemCollections ? "true" : "false") +
      "&batchId=" + basics::StringUtils::itoa(batchId);
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return check;
  }

  // extract the vpack body inventory
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    return ::ErrorMalformedJsonResponse;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    return ::ErrorMalformedJsonResponse;
  }

  // get the collections list
  VPackSlice const collections = body.get("collections");
  if (!collections.isArray()) {
    return ::ErrorMalformedJsonResponse;
  }

  // read the server's max tick value
  std::string const tickString =
      basics::VelocyPackHelper::getStringValue(body, "tick", "");
  if (tickString == "") {
    return ::ErrorMalformedJsonResponse;
  }
  LOG_TOPIC(INFO, Logger::DUMP)
      << "Last tick provided by server is: " << tickString;

  // set the local max tick value
  uint64_t maxTick = basics::StringUtils::uint64(tickString);
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
    auto file = _directory->writableFile("dump.json", true);
    if (!::fileOk(file.get())) {
      return ::fileError(file.get(), true);
    }

    std::string const metaString = meta.slice().toJson();
    file->write(metaString.c_str(), metaString.size());
    if (file->status().fail()) {
      return file->status();
    }
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
    // extract parameters about the individual collection
    if (!collection.isObject()) {
      return ::ErrorMalformedJsonResponse;
    }
    VPackSlice const parameters = collection.get("parameters");
    if (!parameters.isObject()) {
      return ::ErrorMalformedJsonResponse;
    }

    // extract basic info about the collection
    uint64_t const cid = basics::VelocyPackHelper::extractIdValue(parameters);
    std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
        parameters, "deleted", false);
    int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        parameters, "type", 2);
    std::string const collectionType(type == 2 ? "document" : "edge");

    // basic filtering
    if (cid == 0 || name == "") {
      return ::ErrorMalformedJsonResponse;
    }
    if (deleted) {
      continue;
    }
    if (name[0] == '_' && !_includeSystemCollections) {
      continue;
    }

    // filter by specified names
    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    // queue job to actually dump collection
    auto jobData = std::make_unique<JobData>(
        *this, *_directory, _stats, collection, _clusterMode, _progress,
        _dumpData, _initialChunkSize, _maxChunkSize, _tickStart, maxTick,
        batchId, std::to_string(cid), name, collectionType);
    _clientTaskQueue.queueJob(std::move(jobData));
  }

  // wait for all jobs to finish, then check for errors
  _clientTaskQueue.waitForIdle();
  {
    MUTEX_LOCKER(lock, _workerErrorLock);
    if (!_workerErrors.empty()) {
      return _workerErrors.front();
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

// dump data from cluster via a coordinator
Result DumpFeature::runClusterDump(httpclient::SimpleHttpClient& client) {
  // get the cluster inventory
  std::string const url =
      "/_api/replication/clusterInventory?includeSystem=" +
      std::string(_includeSystemCollections ? "true" : "false");
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    return check;
  }

  // parse the inventory vpack body
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = response->getBodyVelocyPack();
  } catch (...) {
    return ::ErrorMalformedJsonResponse;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    return ::ErrorMalformedJsonResponse;
  }

  // parse collections array
  VPackSlice const collections = body.get("collections");
  if (!collections.isArray()) {
    return ::ErrorMalformedJsonResponse;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }

  // iterate over collections
  for (auto const& collection : VPackArrayIterator(collections)) {
    // extract parameters about the individual collection
    if (!collection.isObject()) {
      return ::ErrorMalformedJsonResponse;
    }
    VPackSlice const parameters = collection.get("parameters");

    if (!parameters.isObject()) {
      return ::ErrorMalformedJsonResponse;
    }

    // extract basic info about the collection
    uint64_t const cid = basics::VelocyPackHelper::extractIdValue(parameters);
    std::string const name =
        basics::VelocyPackHelper::getStringValue(parameters, "name", "");
    bool const deleted =
        basics::VelocyPackHelper::getBooleanValue(parameters, "deleted", false);

    // simple filtering
    if (cid == 0 || name == "") {
      return ::ErrorMalformedJsonResponse;
    }
    if (deleted) {
      continue;
    }
    if (name[0] == '_' && !_includeSystemCollections) {
      continue;
    }

    // filter by specified names
    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    if (isIgnoredHiddenEnterpriseCollection(name)) {
      continue;
    }

    // verify distributeShardsLike info
    if (!_ignoreDistributeShardsLikeErrors) {
      std::string prototypeCollection =
          basics::VelocyPackHelper::getStringValue(parameters,
                                                   "distributeShardsLike", "");

      if (!prototypeCollection.empty() && !restrictList.empty()) {
        if (std::find(_collections.begin(), _collections.end(),
                      prototypeCollection) == _collections.end()) {
          return {
              TRI_ERROR_INTERNAL,
              std::string("Collection ") + name +
                  "'s shard distribution is based on a that of collection " +
                  prototypeCollection +
                  ", which is not dumped along. You may dump the collection "
                  "regardless of the missing prototype collection by using "
                  "the "
                  "--ignore-distribute-shards-like-errors parameter."};
        }
      }
    }

    // queue job to actually dump collection
    auto jobData = std::make_unique<JobData>(
        *this, *_directory, _stats, collection, _clusterMode, _progress,
        _dumpData, _initialChunkSize, _maxChunkSize, _tickStart,
        0 /* _tickEnd */, 0 /* batchId */, std::to_string(cid), name,
        "" /* collectionType */);
    _clientTaskQueue.queueJob(std::move(jobData));
  }

  // wait for all jobs to finish, then check for errors
  _clientTaskQueue.waitForIdle();
  {
    MUTEX_LOCKER(lock, _workerErrorLock);
    if (!_workerErrors.empty()) {
      return _workerErrors.front();
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

void DumpFeature::reportError(Result const& error) {
  try {
    MUTEX_LOCKER(lock, _workerErrorLock);
    _workerErrors.emplace(error);
    _clientTaskQueue.clearQueue();
  } catch (...) {
  }
}

/// @brief main method to run dump
void DumpFeature::start() {
  _exitCode = EXIT_SUCCESS;

  // set up the output directory, not much else
  _directory = std::make_unique<ManagedDirectory>(_outputPath, true, true);
  if (_directory->status().fail()) {
    switch (_directory->status().errorNumber()) {
      case TRI_ERROR_FILE_EXISTS:
        LOG_TOPIC(FATAL, Logger::FIXME)
            << "cannot write to output directory '" << _outputPath << "'";

        break;
      case TRI_ERROR_CANNOT_OVERWRITE_FILE:
        LOG_TOPIC(FATAL, Logger::FIXME)
            << "output directory '" << _outputPath
            << "' already exists. use \"--overwrite true\" to "
               "overwrite data in it";
        break;
      default:
        LOG_TOPIC(ERR, Logger::FIXME) << _directory->status().errorMessage();
        break;
    }
    FATAL_ERROR_EXIT();
  }

  // get database name to operate on
  auto client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  auto dbName = client->databaseName();

  // get a client to use in main thread
  auto httpClient = _clientManager.getConnectedClient(_force, true);

  // check if we are in cluster or single-server mode
  Result result{TRI_ERROR_NO_ERROR};
  std::tie(result, _clusterMode) =
      _clientManager.getArangoIsCluster(*httpClient);
  if (result.fail()) {
    LOG_TOPIC(ERR, Logger::FIXME)
        << "Error: could not detect ArangoDB instance type";
    FATAL_ERROR_EXIT();
  }

  // special cluster-mode parameter checks
  if (_clusterMode) {
    if (_tickStart != 0 || _tickEnd != 0) {
      LOG_TOPIC(ERR, Logger::FIXME)
          << "Error: cannot use tick-start or tick-end on a cluster";
      FATAL_ERROR_EXIT();
    }
  }

  // set up threads and workers
  _clientTaskQueue.spawnWorkers(_clientManager, _threadCount);

  if (_progress) {
    LOG_TOPIC(INFO, Logger::DUMP)
        << "Connected to ArangoDB '" << client->endpoint() << "', database: '"
        << dbName << "', username: '" << client->username() << "'";

    LOG_TOPIC(INFO, Logger::DUMP)
        << "Writing dump to output directory '" << _directory->path() << "'";
  }

  Result res;
  try {
    if (!_clusterMode) {
      res = runDump(*httpClient, dbName);
    } else {
      res = runClusterDump(*httpClient);
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception " << ex.what();
    res = {TRI_ERROR_INTERNAL};
  } catch (...) {
    LOG_TOPIC(ERR, Logger::FIXME) << "Error: caught unknown exception";
    res = {TRI_ERROR_INTERNAL};
  }

  if (res.fail()) {
    LOG_TOPIC(ERR, Logger::FIXME) << "An error occurred: " + res.errorMessage();
    _exitCode = EXIT_FAILURE;
  }

  if (_progress) {
    if (_dumpData) {
      LOG_TOPIC(INFO, Logger::DUMP)
          << "Processed " << _stats.totalCollections.load()
          << " collection(s), wrote " << _stats.totalWritten.load()
          << " byte(s) into datafiles, sent " << _stats.totalBatches.load()
          << " batch(es)";
    } else {
      LOG_TOPIC(INFO, Logger::DUMP)
          << "Processed " << _stats.totalCollections << " collection(s)";
    }
  }
}

bool DumpFeature::isIgnoredHiddenEnterpriseCollection(
    std::string const& name) const {
#ifdef USE_ENTERPRISE
  if (!_force && name[0] == '_') {
    if (strncmp(name.c_str(), "_local_", 7) == 0 ||
        strncmp(name.c_str(), "_from_", 6) == 0 ||
        strncmp(name.c_str(), "_to_", 4) == 0) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME)
          << "Dump ignoring collection " << name
          << ". Will be created via SmartGraphs of a full dump. If you want to "
             "dump this collection anyway use 'arangodump --force'. "
             "However this is not recommended and you should instead dump "
             "the EdgeCollection of the SmartGraph instead.";
      return true;
    }
  }
#endif
  return false;
}

}  // namespace arangodb
