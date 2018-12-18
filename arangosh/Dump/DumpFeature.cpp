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

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/algorithm/clamp.hpp>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Maskings/Maskings.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "Utils/ManagedDirectory.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace {

/// @brief fake client id we will send to the server. the server keeps
/// track of all connected clients
static uint64_t clientId = 0;

/// @brief name of the feature to report to application server
constexpr auto FeatureName = "Dump";

/// @brief minimum amount of data to fetch from server in a single batch
constexpr uint64_t MinChunkSize = 1024 * 128;

/// @brief maximum amount of data to fetch from server in a single batch
// NB: larger value may cause tcp issues (check exact limits)
constexpr uint64_t MaxChunkSize = 1024 * 1024 * 96;

/// @brief generic error for if server returns bad/unexpected json
const arangodb::Result ErrorMalformedJsonResponse = {
    TRI_ERROR_INTERNAL, "got malformed JSON response from server"};

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
    int errorNum = TRI_ERROR_INTERNAL;
    std::string errorMsg = response->getHttpReturnMessage();
    std::shared_ptr<arangodb::velocypack::Builder> bodyBuilder(response->getBodyVelocyPack());
    arangodb::velocypack::Slice error = bodyBuilder->slice();
    if (!error.isNone() && error.hasKey(arangodb::StaticStrings::ErrorMessage)) {
      errorNum = error.get(arangodb::StaticStrings::ErrorNum).getNumericValue<int>();
      errorMsg = error.get(arangodb::StaticStrings::ErrorMessage).copyString();
    }
    return {errorNum, "got invalid response from server: HTTP " +
                      itoa(response->getHttpReturnCode()) + ": " + errorMsg};
  }
  return {TRI_ERROR_NO_ERROR};
}

/// @brief checks that a file pointer is valid and file status is ok
bool fileOk(arangodb::ManagedDirectory::File* file) {
  return (file && file->status().ok());
}

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

/// @brief start a batch via the replication API
std::pair<arangodb::Result, uint64_t> startBatch(
    arangodb::httpclient::SimpleHttpClient& client, std::string const& DBserver) {
  using arangodb::basics::VelocyPackHelper;
  using arangodb::basics::StringUtils::uint64;

  std::string url = "/_api/replication/batch?serverId=" + std::to_string(clientId);
  std::string const body = "{\"ttl\":300}";
  std::string urlExt;
  if (!DBserver.empty()) {
    url += "&DBserver=" + DBserver;
  }

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::POST, url,
                     body.c_str(), body.size()));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    LOG_TOPIC(ERR, arangodb::Logger::DUMP) << "An error occurred while creating dump context: " << check.errorMessage();
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

/// @brief prolongs a batch to ensure we can complete our dump
void extendBatch(arangodb::httpclient::SimpleHttpClient& client,
                 std::string const& DBserver, uint64_t batchId) {
  using arangodb::basics::StringUtils::itoa;
  TRI_ASSERT(batchId > 0);

  std::string url = "/_api/replication/batch/" + itoa(batchId) + "?serverId=" + std::to_string(clientId);
  std::string const body = "{\"ttl\":300}";
  if (!DBserver.empty()) {
    url += "&DBserver=" + DBserver;
  }

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::PUT, url,
                     body.c_str(), body.size()));
  // ignore any return value
}

/// @brief mark our batch finished so resources can be freed on server
void endBatch(arangodb::httpclient::SimpleHttpClient& client,
              std::string DBserver, uint64_t& batchId) {
  using arangodb::basics::StringUtils::itoa;
  TRI_ASSERT(batchId > 0);

  std::string url = "/_api/replication/batch/" + itoa(batchId) + "?serverId=" + std::to_string(clientId);
  if (!DBserver.empty()) {
    url += "&DBserver=" + DBserver;
  }

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::DELETE_REQ, url,
                     nullptr, 0));
  // ignore any return value

  // overwrite the input id
  batchId = 0;
}

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
        << "Got invalid response from server when flushing WAL: " + check.errorMessage();
  }
}

bool isIgnoredHiddenEnterpriseCollection(
    arangodb::DumpFeature::Options const& options, std::string const& name) {
#ifdef USE_ENTERPRISE
  if (!options.force && name[0] == '_') {
    if (strncmp(name.c_str(), "_local_", 7) == 0 ||
        strncmp(name.c_str(), "_from_", 6) == 0 ||
        strncmp(name.c_str(), "_to_", 4) == 0) {
      LOG_TOPIC(INFO, arangodb::Logger::DUMP)
          << "Dump is ignoring collection '" << name
          << "'. Will be created via SmartGraphs of a full dump. If you want to "
             "dump this collection anyway use 'arangodump --force'. "
             "However this is not recommended and you should instead dump "
             "the EdgeCollection of the SmartGraph instead.";
      return true;
    }
  }
#endif
  return false;
}

arangodb::Result dumpJsonObjects(arangodb::DumpFeature::JobData& jobData,
                                 arangodb::ManagedDirectory::File& file,
                                 arangodb::basics::StringBuffer const& body) {
  arangodb::basics::StringBuffer masked(1, false);
  arangodb::basics::StringBuffer const* result = &body;

  if (jobData.maskings != nullptr) {
    jobData.maskings->mask(jobData.name, body, masked);
    result = &masked;
  }
  
  file.write(result->c_str(), result->length());

  if (file.status().fail()) {
    return {TRI_ERROR_CANNOT_WRITE_FILE};
  } 

  jobData.stats.totalWritten += static_cast<uint64_t>(result->length());

  return {TRI_ERROR_NO_ERROR};
}

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
  using arangodb::basics::StringUtils::urlEncode;

  uint64_t fromTick = minTick;
  uint64_t chunkSize =
      jobData.options.initialChunkSize;  // will grow adaptively up to max
  std::string baseUrl = "/_api/replication/dump?collection=" + urlEncode(name) +
                        "&batchId=" + itoa(batchId) + "&ticks=false";
  if (jobData.options.clusterMode) {
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
      LOG_TOPIC(ERR, arangodb::Logger::DUMP) << "An error occurred while dumping collection '" << name << "': " << check.errorMessage();
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
              std::string("got invalid response from server: required header is missing while dumping collection '") + name + "'"};
    }

    // now actually write retrieved data to dump file
    arangodb::basics::StringBuffer const& body = response->getBody();
    arangodb::Result result = dumpJsonObjects(jobData, file, body);

    if (result.fail()) {
      return result;
    }

    if (!checkMore || fromTick == 0) {
      // all done, return successful
      return {TRI_ERROR_NO_ERROR};
    }

    // more data to retrieve, adaptively increase chunksize
    if (chunkSize < jobData.options.maxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);
      if (chunkSize > jobData.options.maxChunkSize) {
        chunkSize = jobData.options.maxChunkSize;
      }
    }
  }

  // should never get here, but need to make compiler play nice
  TRI_ASSERT(false);
  return {TRI_ERROR_INTERNAL};
}

/// @brief processes a single collection dumping job in single-server mode
arangodb::Result handleCollection(
    arangodb::httpclient::SimpleHttpClient& client,
    arangodb::DumpFeature::JobData& jobData,
    arangodb::ManagedDirectory::File& file) {
  // keep the batch alive
  ::extendBatch(client, "", jobData.batchId);

  // do the hard work in another function...
  return ::dumpCollection(client, jobData, file, jobData.name, "",
                          jobData.batchId, jobData.options.tickStart,
                          jobData.options.tickEnd);
}

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

    if (jobData.options.progress) {
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

/// @brief process a single job from the queue
arangodb::Result processJob(arangodb::httpclient::SimpleHttpClient& client,
                            arangodb::DumpFeature::JobData& jobData) {
  using arangodb::velocypack::ObjectBuilder;

  arangodb::Result result{TRI_ERROR_NO_ERROR};

  bool dumpStructure = true;

  if (dumpStructure && jobData.maskings != nullptr) {
    dumpStructure = jobData.maskings->shouldDumpStructure(jobData.name);
  }

  if (!dumpStructure) {
    if (jobData.options.progress) {
      LOG_TOPIC(INFO, arangodb::Logger::DUMP)
          << "# Dumping collection '" << jobData.name << "'...";
    }

    return result;
  }

  // prep hex string of collection name
  std::string const hexString(
      arangodb::rest::SslInterface::sslMD5(jobData.name));

  // found a collection!
  if (jobData.options.progress) {
    LOG_TOPIC(INFO, arangodb::Logger::DUMP)
        << "# Dumping collection '" << jobData.name << "'...";
  }
  ++(jobData.stats.totalCollections);

  {
    // save meta data
    auto file = jobData.directory.writableFile(
        jobData.name + (jobData.options.clusterMode ? "" : ("_" + hexString)) +
            ".structure.json",
        true);
    if (!::fileOk(file.get())) {
      return ::fileError(file.get(), true);
    }

    VPackBuilder excludes;
    {  // { parameters: { shadowCollections: null } }
      ObjectBuilder object(&excludes);
      {
        ObjectBuilder subObject(&excludes, "parameters");
        subObject->add("shadowCollections", VPackSlice::nullSlice());
      }
    }

    VPackBuilder collectionWithExcludedParametersBuilder =
        VPackCollection::merge(jobData.collectionInfo, excludes.slice(), true,
                               true);

    std::string const collectionInfo =
        collectionWithExcludedParametersBuilder.slice().toJson();

    file->write(collectionInfo.c_str(), collectionInfo.size());
    if (file->status().fail()) {
      // close file and bail out
      result = file->status();
    }
  }

  if (result.ok()) {
    bool dumpData = jobData.options.dumpData;

    if (dumpData && jobData.maskings != nullptr) {
      dumpData = jobData.maskings->shouldDumpData(jobData.name);
    }

    if (dumpData) {
      // save the actual data
      auto file = jobData.directory.writableFile(
          jobData.name + "_" + hexString + ".data.json", true);
      if (!::fileOk(file.get())) {
        return ::fileError(file.get(), true);
      }

      if (jobData.options.clusterMode) {
        result = ::handleCollectionCluster(client, jobData, *file);
      } else {
        result = ::handleCollection(client, jobData, *file);
      }
    }
  }

  return result;
}

/// @brief handle the result of a single job
void handleJobResult(std::unique_ptr<arangodb::DumpFeature::JobData>&& jobData,
                     arangodb::Result const& result) {
  if (result.fail()) {
    jobData->feature.reportError(result);
  }
}

}  // namespace

namespace arangodb {

DumpFeature::JobData::JobData(ManagedDirectory& dir, DumpFeature& feat,
                              Options const& opts, maskings::Maskings* maskings,
                              Stats& stat, VPackSlice const& info, uint64_t const batch,
                              std::string const& c, std::string const& n,
                              std::string const& t)
    : directory{dir},
      feature{feat},
      options{opts},
      maskings{maskings},
      stats{stat},
      collectionInfo{info},
      batchId{batch},
      cid{c},
      name{n},
      type{t} {}

DumpFeature::DumpFeature(application_features::ApplicationServer& server,
                         int& exitCode)
    : ApplicationFeature(server, DumpFeature::featureName()),
      _clientManager{Logger::DUMP},
      _clientTaskQueue{::processJob, ::handleJobResult},
      _exitCode{exitCode} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("BasicsPhase");

  using arangodb::basics::FileUtils::buildFilename;
  using arangodb::basics::FileUtils::currentDirectory;
  _options.outputPath = buildFilename(currentDirectory().result(), "dump");
};

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
      new VectorParameter<StringParameter>(&_options.collections));

  options->addOption("--initial-batch-size",
                     "initial size for individual data batches (in bytes)",
                     new UInt64Parameter(&_options.initialChunkSize));

  options->addOption("--batch-size",
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_options.maxChunkSize));

  options->addOption("--threads",
                     "maximum number of collections to process in parallel",
                     new UInt32Parameter(&_options.threadCount));

  options->addOption("--dump-data", "dump collection data",
                     new BooleanParameter(&_options.dumpData));

  options->addOption(
      "--force", "continue dumping even in the face of some server-side errors",
      new BooleanParameter(&_options.force));

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "continue dump even if sharding prototype collection is "
      "not backed up along",
      new BooleanParameter(&_options.ignoreDistributeShardsLikeErrors));

  options->addOption("--include-system-collections",
                     "include system collections",
                     new BooleanParameter(&_options.includeSystemCollections));
  
  options->addOption("--output-directory", "output directory",
                     new StringParameter(&_options.outputPath));

  options->addOption("--overwrite", "overwrite data in output directory",
                     new BooleanParameter(&_options.overwrite));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_options.progress));

  options->addOption("--tick-start", "only include data after this tick",
                     new UInt64Parameter(&_options.tickStart));

  options->addOption("--tick-end", "last tick to be included in data dump",
                     new UInt64Parameter(&_options.tickEnd));

  options->addOption("--maskings", "file with maskings definition",
                     new StringParameter(&_options.maskingsFile));
}

void DumpFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _options.outputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expecting at most one directory, got " +
               arangodb::basics::StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // clamp chunk values to allowed ranges
  _options.initialChunkSize = boost::algorithm::clamp(
      _options.initialChunkSize, ::MinChunkSize, ::MaxChunkSize);
  _options.maxChunkSize = boost::algorithm::clamp(
      _options.maxChunkSize, _options.initialChunkSize, ::MaxChunkSize);

  if (_options.tickStart < _options.tickEnd) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid values for --tick-start or --tick-end";
    FATAL_ERROR_EXIT();
  }

  // trim trailing slash from path because it may cause problems on ...
  // Windows
  if (!_options.outputPath.empty() &&
      _options.outputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
    TRI_ASSERT(_options.outputPath.size() > 0);
    _options.outputPath.pop_back();
  }

  uint32_t clamped = boost::algorithm::clamp(
      _options.threadCount, 1,
      4 * static_cast<uint32_t>(TRI_numberProcessors()));
  if (_options.threadCount != clamped) {
    LOG_TOPIC(WARN, Logger::FIXME) << "capping --threads value to " << clamped;
    _options.threadCount = clamped;
  }
}

// dump data from server
Result DumpFeature::runDump(httpclient::SimpleHttpClient& client,
                            std::string const& dbName) {
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
      std::string(_options.includeSystemCollections ? "true" : "false") +
      "&batchId=" + basics::StringUtils::itoa(batchId);
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    LOG_TOPIC(ERR, arangodb::Logger::DUMP) << "An error occurred while fetching inventory: " << check.errorMessage();
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

  // get the view list
  VPackSlice views = body.get("views");
  if (!views.isArray()) {
    views = VPackSlice::emptyArraySlice();
  }

  // Step 1. Store view definition files
  Result res = storeDumpJson(body, dbName);
  if (res.fail()) {
    return res;
  }

  // Step 2. Store view definition files
  res = storeViews(views);
  if (res.fail()) {
    return res;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _options.collections.size(); ++i) {
    restrictList.insert(
        std::pair<std::string, bool>(_options.collections[i], true));
  }

  // Step 3. iterate over collections, queue dump jobs
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
      parameters, StaticStrings::DataSourceName, ""
    );
    bool const deleted = arangodb::basics::VelocyPackHelper::getBooleanValue(
      parameters, StaticStrings::DataSourceDeleted.c_str(), false
    );
    int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      parameters, StaticStrings::DataSourceType.c_str(), 2
    );
    std::string const collectionType(type == 2 ? "document" : "edge");

    // basic filtering
    if (cid == 0 || name == "") {
      return ::ErrorMalformedJsonResponse;
    }

    if (deleted) {
      continue;
    }

    if (name[0] == '_' && !_options.includeSystemCollections) {
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
        *_directory, *this, _options, _maskings.get(), _stats, collection, batchId,
        std::to_string(cid), name, collectionType);
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
Result DumpFeature::runClusterDump(httpclient::SimpleHttpClient& client,
                                   std::string const& dbname) {
  // get the cluster inventory
  std::string const url =
      "/_api/replication/clusterInventory?includeSystem=" +
      std::string(_options.includeSystemCollections ? "true" : "false");
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, url, nullptr, 0));
  auto check = ::checkHttpResponse(client, response);
  if (check.fail()) {
    LOG_TOPIC(ERR, arangodb::Logger::DUMP) << "An error occurred while fetching inventory: " << check.errorMessage();
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

  // get the view list
  VPackSlice views = body.get("views");
  if (!views.isArray()) {
    views = VPackSlice::emptyArraySlice();
  }

  // Step 1. Store view definition files
  Result res = storeDumpJson(body, dbname);
  if (res.fail()) {
    return res;
  }

  // Step 2. Store view definition files
  res = storeViews(views);
  if (res.fail()) {
    return res;
  }

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _options.collections.size(); ++i) {
    restrictList.insert(
        std::pair<std::string, bool>(_options.collections[i], true));
  }

  // Step 3. iterate over collections
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
    if (name[0] == '_' && !_options.includeSystemCollections) {
      continue;
    }

    // filter by specified names
    if (!restrictList.empty() &&
        restrictList.find(name) == restrictList.end()) {
      // collection name not in list
      continue;
    }

    if (isIgnoredHiddenEnterpriseCollection(_options, name)) {
      continue;
    }

    // verify distributeShardsLike info
    if (!_options.ignoreDistributeShardsLikeErrors) {
      std::string prototypeCollection =
          basics::VelocyPackHelper::getStringValue(parameters,
                                                   "distributeShardsLike", "");

      if (!prototypeCollection.empty() && !restrictList.empty()) {
        if (std::find(_options.collections.begin(), _options.collections.end(),
                      prototypeCollection) == _options.collections.end()) {
          return {
              TRI_ERROR_INTERNAL,
              std::string("Collection ") + name +
                  "'s shard distribution is based on that of collection " +
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
        *_directory, *this, _options, _maskings.get(), _stats, collection, 0 /* batchId */,
        std::to_string(cid), name, "" /* collectionType */);
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

Result DumpFeature::storeDumpJson(VPackSlice const& body,
                                  std::string const& dbName) const {

  // read the server's max tick value
  std::string const tickString =
  basics::VelocyPackHelper::getStringValue(body, "tick", "");
  if (tickString == "") {
    return ::ErrorMalformedJsonResponse;
  }
  LOG_TOPIC(INFO, Logger::DUMP)
  << "Last tick provided by server is: " << tickString;

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
  return {};
}

Result DumpFeature::storeViews(VPackSlice const& views) const {
  for (VPackSlice view : VPackArrayIterator(views)) {
    auto nameSlice = view.get(StaticStrings::DataSourceName);
    if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
      continue; // ignore
    }

    try {
      std::string fname = nameSlice.copyString();
      fname.append(".view.json");
      // save last tick in file
      auto file = _directory->writableFile(fname, true);
      if (!::fileOk(file.get())) {
        return ::fileError(file.get(), true);
      }

      std::string const viewString = view.toJson();
      file->write(viewString.c_str(), viewString.size());
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
  }
  return {};
}

void DumpFeature::reportError(Result const& error) {
  try {
    MUTEX_LOCKER(lock, _workerErrorLock);
    _workerErrors.emplace(error);
    _clientTaskQueue.clearQueue();
  } catch (...) {
  }
}

void DumpFeature::start() {
  if (!_options.maskingsFile.empty()) {
    maskings::MaskingsResult m = maskings::Maskings::fromFile(_options.maskingsFile);

    if (m.status != maskings::MaskingsResult::VALID) {
      LOG_TOPIC(FATAL, Logger::CONFIG) << m.message;
      FATAL_ERROR_EXIT();
    }

    _maskings = std::move(m.maskings);
  }

  _exitCode = EXIT_SUCCESS;

  // generate a fake client id that we sent to the server
  ::clientId = RandomGenerator::interval(static_cast<uint64_t>(0x0000FFFFFFFFFFFFULL));

  double const start = TRI_microtime();

  // set up the output directory, not much else
  _directory =
      std::make_unique<ManagedDirectory>(_options.outputPath, !_options.overwrite, true);
  if (_directory->status().fail()) {
    switch (_directory->status().errorNumber()) {
      case TRI_ERROR_FILE_EXISTS:
        LOG_TOPIC(FATAL, Logger::FIXME) << "cannot write to output directory '"
                                        << _options.outputPath << "'";

        break;
      case TRI_ERROR_CANNOT_OVERWRITE_FILE:
        LOG_TOPIC(FATAL, Logger::FIXME)
            << "output directory '" << _options.outputPath
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
  auto httpClient = _clientManager.getConnectedClient(_options.force, true, true);

  // check if we are in cluster or single-server mode
  Result result{TRI_ERROR_NO_ERROR};
  std::tie(result, _options.clusterMode) =
      _clientManager.getArangoIsCluster(*httpClient);
  if (result.fail()) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE) << "Error: could not detect ArangoDB instance type: " << result.errorMessage();
    FATAL_ERROR_EXIT();
  }

  // special cluster-mode parameter checks
  if (_options.clusterMode) {
    if (_options.tickStart != 0 || _options.tickEnd != 0) {
      LOG_TOPIC(ERR, Logger::FIXME)
          << "Error: cannot use tick-start or tick-end on a cluster";
      FATAL_ERROR_EXIT();
    }
  }

  // set up threads and workers
  _clientTaskQueue.spawnWorkers(_clientManager, _options.threadCount);

  if (_options.progress) {
    LOG_TOPIC(INFO, Logger::DUMP)
        << "Connected to ArangoDB '" << client->endpoint() << "', database: '"
        << dbName << "', username: '" << client->username() << "'";

    LOG_TOPIC(INFO, Logger::DUMP)
        << "Writing dump to output directory '" << _directory->path() << "' with " << _options.threadCount << " thread(s)";
  }

  Result res;
  try {
    if (!_options.clusterMode) {
      res = runDump(*httpClient, dbName);
    } else {
      res = runClusterDump(*httpClient, dbName);
    }
  } catch (basics::Exception const& ex) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception: " << ex.what();
    res = {ex.code(), ex.what()};
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception: " << ex.what();
    res = {TRI_ERROR_INTERNAL, ex.what()};
  } catch (...) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught unknown exception";
    res = {TRI_ERROR_INTERNAL};
  }

  if (res.fail()) {
    LOG_TOPIC(ERR, Logger::FIXME) << "An error occurred: " + res.errorMessage();
    _exitCode = EXIT_FAILURE;
  }

  if (_options.progress) {
    double totalTime = TRI_microtime() - start;

    if (_options.dumpData) {
      LOG_TOPIC(INFO, Logger::DUMP)
          << "Processed " << _stats.totalCollections.load()
          << " collection(s) in " << Logger::FIXED(totalTime, 6) << " s,"
          << " wrote " << _stats.totalWritten.load()
          << " byte(s) into datafiles, sent " << _stats.totalBatches.load()
          << " batch(es)";
    } else {
      LOG_TOPIC(INFO, Logger::DUMP)
          << "Processed " << _stats.totalCollections.load()
          << " collection(s) in " << Logger::FIXED(totalTime, 6) << " s";
    }
  }
}

}  // namespace arangodb
