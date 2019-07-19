////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "ClusterMethods.h"

#include "Agency/TimeString.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterCollectionCreationInfo.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Graph/Traverser.h"
#include "StorageEngine/HotBackupCommon.h"
#include "Indexes/Index.h"
#include "RestServer/TtlFeature.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "RocksDBEngine/RocksDBHotBackup.h"
#include "Rest/Version.h"
#include "VocBase/Methods/Version.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include "velocypack/StringRef.h"
#include <velocypack/velocypack-aliases.h>

#include <algorithm>
#include <numeric>
#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// Timeout for read operations:
static double const CL_DEFAULT_TIMEOUT = 120.0;

// Timeout for write operations, note that these are used for communication
// with a shard leader and we always have to assume that some follower has
// stopped writes for some time to get in sync:
static double const CL_DEFAULT_LONG_TIMEOUT = 900.0;

namespace {
template <typename T>
T addFigures(VPackSlice const& v1, VPackSlice const& v2,
             std::vector<std::string> const& attr) {
  TRI_ASSERT(v1.isObject());
  TRI_ASSERT(v2.isObject());

  T value = 0;

  VPackSlice found = v1.get(attr);
  if (found.isNumber()) {
    value += found.getNumericValue<T>();
  }

  found = v2.get(attr);
  if (found.isNumber()) {
    value += found.getNumericValue<T>();
  }

  return value;
}

void recursiveAdd(VPackSlice const& value, std::shared_ptr<VPackBuilder>& builder) {
  TRI_ASSERT(value.isObject());
  TRI_ASSERT(builder->slice().isObject());
  TRI_ASSERT(builder->isClosed());

  VPackBuilder updated;

  updated.openObject();

  updated.add("alive", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                     {"alive", "count"})));
  updated.add("size", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                    {"alive", "size"})));
  updated.close();

  updated.add("dead", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                     {"dead", "count"})));
  updated.add("size", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                    {"dead", "size"})));
  updated.add("deletion", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                        {"dead", "deletion"})));
  updated.close();

  updated.add("indexes", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                     {"indexes", "count"})));
  updated.add("size", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                    {"indexes", "size"})));
  updated.close();

  updated.add("datafiles", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                     {"datafiles", "count"})));
  updated.add("fileSize",
              VPackValue(addFigures<size_t>(value, builder->slice(),
                                            {"datafiles", "fileSize"})));
  updated.close();

  updated.add("journals", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                     {"journals", "count"})));
  updated.add("fileSize",
              VPackValue(addFigures<size_t>(value, builder->slice(),
                                            {"journals", "fileSize"})));
  updated.close();

  updated.add("compactors", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(),
                                                     {"compactors", "count"})));
  updated.add("fileSize",
              VPackValue(addFigures<size_t>(value, builder->slice(),
                                            {"compactors", "fileSize"})));
  updated.close();

  updated.add("documentReferences",
              VPackValue(addFigures<size_t>(value, builder->slice(), {"documentReferences"})));

  updated.close();

  TRI_ASSERT(updated.slice().isObject());
  TRI_ASSERT(updated.isClosed());

  builder.reset(new VPackBuilder(
      VPackCollection::merge(builder->slice(), updated.slice(), true, false)));
  TRI_ASSERT(builder->slice().isObject());
  TRI_ASSERT(builder->isClosed());
}

/// @brief begin a transaction on some leader shards
template <typename ShardDocsMap>
Result beginTransactionOnSomeLeaders(TransactionState& state,
                                     std::shared_ptr<LogicalCollection> const& coll,
                                     ShardDocsMap const& shards) {
  TRI_ASSERT(state.isCoordinator());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));

  std::shared_ptr<ShardMap> shardMap = coll->shardIds();
  std::vector<std::string> leaders;

  for (auto const& pair : shards) {
    auto const& it = shardMap->find(pair.first);
    if (it->second.empty()) {
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;  // something is broken
    }
    // now we got the shard leader
    std::string const& leader = it->second[0];
    if (!state.knowsServer(leader)) {
      leaders.emplace_back(leader);
    }
  }
  return ClusterTrxMethods::beginTransactionOnLeaders(state, leaders);
}

// begin transaction on shard leaders
static Result beginTransactionOnAllLeaders(transaction::Methods& trx, ShardMap const& shards) {
  TRI_ASSERT(trx.state()->isCoordinator());
  TRI_ASSERT(trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED));
  std::vector<ServerID> leaders;
  for (std::pair<ShardID, std::vector<ServerID>> const& shardServers : shards) {
    ServerID const& srv = shardServers.second.at(0);
    if (!trx.state()->knowsServer(srv)) {
      leaders.emplace_back(srv);
    }
  }
  return ClusterTrxMethods::beginTransactionOnLeaders(*trx.state(), leaders);
}

/// @brief add the correct header for the shard
static void addTransactionHeaderForShard(transaction::Methods& trx,
                                         ShardMap const& shardMap, ShardID const& shard,
                                         std::unordered_map<std::string, std::string>& headers) {
  TRI_ASSERT(trx.state()->isCoordinator());
  if (!ClusterTrxMethods::isElCheapo(trx)) {
    return;  // no need
  }

  auto const& it = shardMap.find(shard);
  if (it != shardMap.end()) {
    if (it->second.empty()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
    }
    ServerID const& leader = it->second[0];
    ClusterTrxMethods::addTransactionHeader(trx, leader, headers);
  } else {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "couldnt find shard in shardMap");
  }
}
}  // namespace

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a numeric value from an hierarchical VelocyPack
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static T ExtractFigure(VPackSlice const& slice, char const* group, char const* name) {
  TRI_ASSERT(slice.isObject());
  VPackSlice g = slice.get(group);

  if (!g.isObject()) {
    return static_cast<T>(0);
  }
  return arangodb::basics::VelocyPackHelper::getNumericValue<T>(g, name, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge the baby-object results.
///        The shard map contains the ordering of elements, the vector in this
///        Map is expected to be sorted from front to back.
///        The second map contains the answers for each shard.
///        The builder in the third parameter will be cleared and will contain
///        the resulting array. It is guaranteed that the resulting array
///        indexes
///        are equal to the original request ordering before it was destructured
///        for babies.
////////////////////////////////////////////////////////////////////////////////

static void mergeResults(std::vector<std::pair<ShardID, VPackValueLength>> const& reverseMapping,
                         std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> const& resultMap,
                         std::shared_ptr<VPackBuilder>& resultBody) {
  resultBody->clear();
  resultBody->openArray();
  for (auto const& pair : reverseMapping) {
    VPackSlice arr = resultMap.find(pair.first)->second->slice();
    if (arr.isObject() && arr.hasKey(StaticStrings::Error) &&
        arr.get(StaticStrings::Error).isBoolean() &&
        arr.get(StaticStrings::Error).getBoolean()) {
      // an error occurred, now rethrow the error
      int res = arr.get(StaticStrings::ErrorNum).getNumericValue<int>();
      VPackSlice msg = arr.get(StaticStrings::ErrorMessage);
      if (msg.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(res, msg.copyString());
      } else {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
    resultBody->add(arr.at(pair.second));
  }
  resultBody->close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge the baby-object results. (all shards version)
///        results contians the result from all shards in any order.
///        resultBody will be cleared and contains the merged result after this
///        function
///        errorCounter will correctly compute the NOT_FOUND counter, all other
///        codes remain unmodified.
///
///        The merge is executed the following way:
///        FOR every expected document we scan iterate over the corresponding
///        response
///        of each shard. If any of them returned sth. different than NOT_FOUND
///        we take this result as correct.
///        If none returned sth different than NOT_FOUND we return NOT_FOUND as
///        well
////////////////////////////////////////////////////////////////////////////////

static void mergeResultsAllShards(std::vector<std::shared_ptr<VPackBuilder>> const& results,
                                  std::shared_ptr<VPackBuilder>& resultBody,
                                  std::unordered_map<int, size_t>& errorCounter,
                                  VPackValueLength const expectedResults) {
  // errorCounter is not allowed to contain any NOT_FOUND entry.
  TRI_ASSERT(errorCounter.find(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) ==
             errorCounter.end());
  size_t realNotFound = 0;
  VPackBuilder cmp;
  cmp.openObject();
  cmp.add(StaticStrings::Error, VPackValue(true));
  cmp.add(StaticStrings::ErrorNum, VPackValue(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
  cmp.close();
  VPackSlice notFound = cmp.slice();
  resultBody->clear();
  resultBody->openArray();
  for (VPackValueLength currentIndex = 0; currentIndex < expectedResults; ++currentIndex) {
    bool foundRes = false;
    for (auto const& it : results) {
      VPackSlice oneRes = it->slice();
      TRI_ASSERT(oneRes.isArray());
      oneRes = oneRes.at(currentIndex);
      if (!basics::VelocyPackHelper::equal(oneRes, notFound, false)) {
        // This is the correct result
        // Use it
        resultBody->add(oneRes);
        foundRes = true;
        break;
      }
    }
    if (!foundRes) {
      // Found none, use NOT_FOUND
      resultBody->add(notFound);
      realNotFound++;
    }
  }
  resultBody->close();
  if (realNotFound > 0) {
    errorCounter.emplace(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, realNotFound);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract all error baby-style error codes and store them in a map
////////////////////////////////////////////////////////////////////////////////

static void extractErrorCodes(ClusterCommResult const& res,
                              std::unordered_map<int, size_t>& errorCounter,
                              bool includeNotFound) {
  auto const& resultHeaders = res.answer->headers();
  auto codes = resultHeaders.find(StaticStrings::ErrorCodes);
  if (codes != resultHeaders.end()) {
    auto parsedCodes = VPackParser::fromJson(codes->second);
    VPackSlice codesSlice = parsedCodes->slice();
    TRI_ASSERT(codesSlice.isObject());
    for (auto const& code : VPackObjectIterator(codesSlice)) {
      VPackValueLength codeLength;
      char const* codeString = code.key.getString(codeLength);
      int codeNr = NumberUtils::atoi_zero<int>(codeString, codeString + codeLength);
      if (includeNotFound || codeNr != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        errorCounter[codeNr] += code.value.getNumericValue<size_t>();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap
////////////////////////////////////////////////////////////////////////////////

static int distributeBabyOnShards(std::unordered_map<ShardID, std::vector<VPackSlice>>& shardMap,
                                  ClusterInfo* ci, std::string const& collid,
                                  std::shared_ptr<LogicalCollection> const& collinfo,
                                  std::vector<std::pair<ShardID, VPackValueLength>>& reverseMapping,
                                  VPackSlice const& value) {
  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error;
  if (value.isString()) {
    VPackBuilder temp;
    temp.openObject();
    temp.add(StaticStrings::KeyString, value);
    temp.close();

    error = collinfo->getResponsibleShard(temp.slice(), false, shardID,
                                          usesDefaultShardingAttributes);
  } else {
    error = collinfo->getResponsibleShard(value, false, shardID, usesDefaultShardingAttributes);
  }
  if (error == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
    return TRI_ERROR_CLUSTER_SHARD_GONE;
  }
  if (error != TRI_ERROR_NO_ERROR) {
    // We can not find a responsible shard
    return error;
  }

  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    shardMap.emplace(shardID, std::vector<VPackSlice>{value});
    reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(value);
    reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap.
///        Also generates a key if necessary.
////////////////////////////////////////////////////////////////////////////////

static int distributeBabyOnShards(
    std::unordered_map<ShardID, std::vector<std::pair<VPackSlice, std::string>>>& shardMap,
    ClusterInfo* ci, std::string const& collid,
    std::shared_ptr<LogicalCollection> const& collinfo,
    std::vector<std::pair<ShardID, VPackValueLength>>& reverseMapping,
    VPackSlice const value, bool isRestore) {
  ShardID shardID;
  bool userSpecifiedKey = false;
  std::string _key = "";

  if (!value.isObject()) {
    // We have invalid input at this point.
    // However we can work with the other babies.
    // This is for compatibility with single server
    // We just assign it to any shard and pretend the user has given a key
    std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(collid);
    shardID = shards->at(0);
    userSpecifiedKey = true;
  } else {
    int r = transaction::Methods::validateSmartJoinAttribute(*(collinfo.get()), value);

    if (r != TRI_ERROR_NO_ERROR) {
      return r;
    }

    // Sort out the _key attribute:
    // The user is allowed to specify _key, provided that _key is the one
    // and only sharding attribute, because in this case we can delegate
    // the responsibility to make _key attributes unique to the responsible
    // shard. Otherwise, we ensure uniqueness here and now by taking a
    // cluster-wide unique number. Note that we only know the sharding
    // attributes a bit further down the line when we have determined
    // the responsible shard.

    VPackSlice keySlice = value.get(StaticStrings::KeyString);
    if (keySlice.isNone()) {
      // The user did not specify a key, let's create one:
      _key = collinfo->keyGenerator()->generate();
    } else {
      userSpecifiedKey = true;
      if (keySlice.isString()) {
        VPackValueLength l;
        char const* p = keySlice.getString(l);
        collinfo->keyGenerator()->track(p, l);
      }
    }

    // Now find the responsible shard:
    bool usesDefaultShardingAttributes;
    int error = TRI_ERROR_NO_ERROR;
    if (userSpecifiedKey) {
      error = collinfo->getResponsibleShard(value, true, shardID, usesDefaultShardingAttributes);
    } else {
      error = collinfo->getResponsibleShard(value, true, shardID,
                                            usesDefaultShardingAttributes, _key);
    }
    if (error == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
      return TRI_ERROR_CLUSTER_SHARD_GONE;
    }

    // Now perform the above mentioned check:
    if (userSpecifiedKey &&
        (!usesDefaultShardingAttributes || !collinfo->allowUserKeys()) && !isRestore) {
      return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
    }
  }

  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    shardMap.emplace(shardID, std::vector<std::pair<VPackSlice, std::string>>{{value, _key}});
    reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(value, _key);
    reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Collect the results from all shards (fastpath variant)
///        All result bodies are stored in resultMap
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void collectResultsFromAllShards(
    std::unordered_map<ShardID, std::vector<T>> const& shardMap,
    std::vector<ClusterCommRequest>& requests, std::unordered_map<int, size_t>& errorCounter,
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>>& resultMap,
    rest::ResponseCode& responseCode) {
  // If none of the shards responds we return a SERVER_ERROR;
  responseCode = rest::ResponseCode::SERVER_ERROR;
  for (auto const& req : requests) {
    auto res = req.result;

    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      auto tmpBuilder = std::make_shared<VPackBuilder>();
      // If there was no answer whatsoever, we cannot rely on the shardId
      // being present in the result struct:
      ShardID sId = req.destination.substr(6);
      auto weSend = shardMap.find(sId);
      TRI_ASSERT(weSend != shardMap.end());  // We send sth there earlier.
      size_t count = weSend->second.size();
      for (size_t i = 0; i < count; ++i) {
        tmpBuilder->openObject();
        tmpBuilder->add(StaticStrings::Error, VPackValue(true));
        tmpBuilder->add(StaticStrings::ErrorNum, VPackValue(commError));
        tmpBuilder->close();
      }
      resultMap.emplace(sId, tmpBuilder);
    } else {
      TRI_ASSERT(res.answer != nullptr);
      resultMap.emplace(res.shardID, res.answer->toVelocyPackBuilderPtrNoUniquenessChecks());
      extractErrorCodes(res, errorCounter, true);
      responseCode = res.answer_code;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a shard distribution for a new collection, the list
/// dbServers must be a list of DBserver ids to distribute across.
/// If this list is empty, the complete current list of DBservers is
/// fetched from ClusterInfo and with random_shuffle to mix it up.
////////////////////////////////////////////////////////////////////////////////

static std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> DistributeShardsEvenly(
    ClusterInfo* ci, uint64_t numberOfShards, uint64_t replicationFactor,
    std::vector<std::string>& dbServers, bool warnAboutReplicationFactor) {
  auto shards =
      std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();

  if (dbServers.size() == 0) {
    ci->loadCurrentDBServers();
    dbServers = ci->getCurrentDBServers();
    if (dbServers.empty()) {
      return shards;
    }
    random_shuffle(dbServers.begin(), dbServers.end());
  }

  // mop: distribute satellite collections on all servers
  if (replicationFactor == 0) {
    replicationFactor = dbServers.size();
  }

  // fetch a unique id for each shard to create
  uint64_t const id = ci->uniqid(numberOfShards);

  size_t leaderIndex = 0;
  size_t followerIndex = 0;
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    // determine responsible server(s)
    std::vector<std::string> serverIds;
    for (uint64_t j = 0; j < replicationFactor; ++j) {
      if (j >= dbServers.size()) {
        if (warnAboutReplicationFactor) {
          LOG_TOPIC("e16ec", WARN, Logger::CLUSTER)
              << "createCollectionCoordinator: replicationFactor is "
              << "too large for the number of DBservers";
        }
        break;
      }
      std::string candidate;
      // mop: leader
      if (serverIds.size() == 0) {
        candidate = dbServers[leaderIndex++];
        if (leaderIndex >= dbServers.size()) {
          leaderIndex = 0;
        }
      } else {
        do {
          candidate = dbServers[followerIndex++];
          if (followerIndex >= dbServers.size()) {
            followerIndex = 0;
          }
        } while (candidate == serverIds[0]);  // mop: ignore leader
      }
      serverIds.push_back(candidate);
    }

    // determine shard id
    std::string shardId = "s" + StringUtils::itoa(id + i);

    shards->emplace(shardId, serverIds);
  }

  return shards;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Clone shard distribution from other collection
////////////////////////////////////////////////////////////////////////////////

static std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> CloneShardDistribution(
    ClusterInfo* ci, LogicalCollection* col, TRI_voc_cid_t cid) {
  auto result =
      std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();
  TRI_ASSERT(cid != 0);
  std::string cidString = arangodb::basics::StringUtils::itoa(cid);
  TRI_ASSERT(col);
  auto other = ci->getCollection(col->vocbase().name(), cidString);

  // The function guarantees that no nullptr is returned
  TRI_ASSERT(other != nullptr);

  if (!other->distributeShardsLike().empty()) {
    std::string const errorMessage = "Cannot distribute shards like '" + other->name() +
                                     "' it is already distributed like '" +
                                     other->distributeShardsLike() + "'.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE, errorMessage);
  }

  // We need to replace the distribute with the cid.
  col->distributeShardsLike(cidString, other->shardingInfo());

  if (col->isSmart() && col->type() == TRI_COL_TYPE_EDGE) {
    return result;
  }

  auto shards = other->shardIds();
  auto shardList = ci->getShardList(cidString);

  auto numberOfShards = static_cast<uint64_t>(col->numberOfShards());
  // fetch a unique id for each shard to create
  uint64_t const id = ci->uniqid(numberOfShards);
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    // determine responsible server(s)
    std::string shardId = "s" + StringUtils::itoa(id + i);
    auto it = shards->find(shardList->at(i));
    if (it == shards->end()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Inconsistency in shard distribution detected. Is in the process of "
          "self-healing. Please retry the operation again after some seconds.");
    }
    result->emplace(shardId, it->second);
  }
  return result;
}

/// @brief convert ClusterComm error into arango error code
int handleGeneralCommErrors(arangodb::ClusterCommResult const* res) {
  // This function creates an error code from a ClusterCommResult,
  // but only if it is a communication error. If the communication
  // was successful and there was an HTTP error code, this function
  // returns TRI_ERROR_NO_ERROR.
  // If TRI_ERROR_NO_ERROR is returned, then the result was CL_COMM_RECEIVED
  // and .answer can safely be inspected.
  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    return TRI_ERROR_CLUSTER_TIMEOUT;
  } else if (res->status == CL_COMM_ERROR) {
    return TRI_ERROR_CLUSTER_CONNECTION_LOST;
  } else if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    if (res->result == nullptr) {
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    if (!res->result->isComplete()) {
      // there is no result
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::string> getForwardableRequestHeaders(arangodb::GeneralRequest* request) {
  std::unordered_map<std::string, std::string> const& headers = request->headers();
  std::unordered_map<std::string, std::string>::const_iterator it = headers.begin();

  std::unordered_map<std::string, std::string> result;

  while (it != headers.end()) {
    std::string const& key = (*it).first;

    // ignore the following headers
    if (key != "x-arango-async" && key != "authorization" &&
        key != "content-length" && key != "connection" && key != "expect" &&
        key != "host" && key != "origin" && key != StaticStrings::HLCHeader &&
        key != StaticStrings::ErrorCodes &&
        key.substr(0, 14) != "access-control") {
      result.emplace(key, (*it).second);
    }
    ++it;
  }

  result["content-length"] = StringUtils::itoa(request->contentLength());

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two vpack
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(LogicalCollection const& collection, VPackSlice const& oldValue,
                      VPackSlice const& newValue, bool isPatch) {
  if (!oldValue.isObject() || !newValue.isObject()) {
    // expecting two objects. everything else is an error
    return true;
  }
#ifdef DEBUG_SYNC_REPLICATION
  if (collection.vocbase().name() == "sync-replication-test") {
    return false;
  }
#endif

  std::vector<std::string> const& shardKeys = collection.shardKeys();

  for (size_t i = 0; i < shardKeys.size(); ++i) {
    if (shardKeys[i] == StaticStrings::KeyString) {
      continue;
    }

    VPackSlice n = newValue.get(shardKeys[i]);

    if (n.isNone() && isPatch) {
      // attribute not set in patch document. this means no update
      continue;
    }

    VPackSlice o = oldValue.get(shardKeys[i]);

    if (o.isNone()) {
      // if attribute is undefined, use "null" instead
      o = arangodb::velocypack::Slice::nullSlice();
    }

    if (n.isNone()) {
      // if attribute is undefined, use "null" instead
      n = arangodb::velocypack::Slice::nullSlice();
    }

    if (!arangodb::basics::VelocyPackHelper::equal(n, o, false)) {
      return true;
    }
  }

  return false;
}

bool smartJoinAttributeChanged(LogicalCollection const& collection, VPackSlice const& oldValue,
                               VPackSlice const& newValue, bool isPatch) {
  if (!collection.hasSmartJoinAttribute()) {
    return false;
  }
  if (!oldValue.isObject() || !newValue.isObject()) {
    // expecting two objects. everything else is an error
    return true;
  }

  std::string const& s = collection.smartJoinAttribute();

  VPackSlice n = newValue.get(s);
  if (!n.isString()) {
    if (isPatch && n.isNone()) {
      // attribute not set in patch document. this means no update
      return false;
    }

    // no string value... invalid!
    return true;
  }

  VPackSlice o = oldValue.get(s);
  TRI_ASSERT(o.isString());

  return !arangodb::basics::VelocyPackHelper::equal(n, o, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int revisionOnCoordinator(std::string const& dbname,
                          std::string const& collname, TRI_voc_rid_t& rid) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  rid = 0;

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(coordTransactionID, "shard:" + p.first,
                     arangodb::rest::RequestType::GET,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + StringUtils::urlEncode(p.first) +
                         "/revision",
                     std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        VPackSlice answer = res.answer->payload();

        if (answer.isObject()) {
          VPackSlice r = answer.get("revision");

          if (r.isString()) {
            VPackValueLength len;
            char const* p = r.getString(len);
            TRI_voc_rid_t cmp = TRI_StringToRid(p, len, false);

            if (cmp != UINT64_MAX && cmp > rid) {
              // get the maximum value
              rid = cmp;
            }
          }
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

int warmupOnCoordinator(std::string const& dbname, std::string const& cid) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, cid);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(coordTransactionID, "shard:" + p.first,
                     arangodb::rest::RequestType::GET,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + StringUtils::urlEncode(p.first) +
                         "/loadIndexesIntoMemory",
                     std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  // Well actually we don't care...
  int count;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int figuresOnCoordinator(std::string const& dbname, std::string const& collname,
                         std::shared_ptr<arangodb::velocypack::Builder>& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(coordTransactionID, "shard:" + p.first,
                     arangodb::rest::RequestType::GET,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + StringUtils::urlEncode(p.first) +
                         "/figures",
                     std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        VPackSlice answer = res.answer->payload();

        if (answer.isObject()) {
          VPackSlice figures = answer.get("figures");
          if (figures.isObject()) {
            // add to the total
            recursiveAdd(figures, result);
          }
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator, by shard
////////////////////////////////////////////////////////////////////////////////

int countOnCoordinator(transaction::Methods& trx, std::string const& cname,
                       std::vector<std::pair<std::string, uint64_t>>& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  result.clear();

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, cname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();
  const bool isManaged = trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isManaged) {
    Result res = ::beginTransactionOnAllLeaders(trx, *shardIds);
    if (res.fail()) {
      return res.errorNumber();
    }
  }

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>();
  for (std::pair<ShardID, std::vector<ServerID>> const& p : *shardIds) {
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    ClusterTrxMethods::addTransactionHeader(trx, /*leader*/ p.second[0], *headers);
    requests.emplace_back("shard:" + p.first, arangodb::rest::RequestType::GET,
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              "/_api/collection/" +
                              StringUtils::urlEncode(p.first) + "/count",
                          body, std::move(headers));
  }

  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::QUERIES,
                      /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);
  for (auto& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        VPackSlice answer = res.answer->payload();

        if (answer.isObject()) {
          // add to the total
          result.emplace_back(res.shardID,
                              arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
                                  answer, "count", 0));
        } else {
          return TRI_ERROR_INTERNAL;
        }
      } else {
        return static_cast<int>(res.answer_code);
      }
    } else {
      return handleGeneralCommErrors(&req.result);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the selectivity estimates from DBservers
////////////////////////////////////////////////////////////////////////////////

int selectivityEstimatesOnCoordinator(std::string const& dbname, std::string const& collname,
                                      std::unordered_map<std::string, double>& result,
                                      TRI_voc_tick_t tid) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  result.clear();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  std::vector<ClusterCommRequest> requests;
  std::string requestsUrl;
  auto body = std::make_shared<std::string>();
  for (auto const& p : *shards) {
    std::unique_ptr<std::unordered_map<std::string, std::string>> headers;
    if (tid != 0) {
      headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      headers->emplace(StaticStrings::TransactionId, std::to_string(tid));
    }

    requestsUrl = "/_db/" + StringUtils::urlEncode(dbname) +
                  "/_api/index/selectivity?collection=" + StringUtils::urlEncode(p.first);
    requests.emplace_back("shard:" + p.first, arangodb::rest::RequestType::GET,
                          requestsUrl, body, std::move(headers));
  }

  // format of expected answer:
  // in indexes is a map that has keys in the format
  // s<shardid>/<indexid> and index information as value

  // {"code":200
  // ,"error":false
  // ,"indexes":{ "s10004/0"    : 1.0,
  //              "s10004/10005": 0.5
  //            }
  // }

  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::QUERIES,
                      /*retryOnCollNotFound*/ true);

  std::map<std::string, std::vector<double>> indexEstimates;

  for (auto& req : requests) {
    int res = handleGeneralCommErrors(&req.result);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    ClusterCommResult const& comRes = req.result;

    if (comRes.answer_code == arangodb::rest::ResponseCode::OK) {
      VPackSlice answer = comRes.answer->payload();
      if (!answer.isObject()) {
        return TRI_ERROR_INTERNAL;
      }

      // add to the total
      for (auto const& pair : VPackObjectIterator(answer.get("indexes"), true)) {
        velocypack::StringRef shard_index_id(pair.key);
        auto split_point = std::find(shard_index_id.begin(), shard_index_id.end(), '/');
        std::string index(split_point + 1, shard_index_id.end());
        double estimate = basics::VelocyPackHelper::getNumericValue(pair.value, 0.0);
        indexEstimates[index].push_back(estimate);
      }

    } else {
      return static_cast<int>(comRes.answer_code);
    }
  }

  auto aggregate_indexes = [](std::vector<double> vec) -> double {
    TRI_ASSERT(!vec.empty());
    double rv = std::accumulate(vec.begin(), vec.end(), 0.0);
    rv /= static_cast<double>(vec.size());
    return rv;
  };

  for (auto const& p : indexEstimates) {
    result[p.first] = aggregate_indexes(p.second);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates one or many documents in a coordinator
///
/// In case of many documents (slice is a VPackArray) it will send to each
/// shard all the relevant documents for this shard only.
/// If one of them fails, this error is reported.
/// There is NO guarantee for the stored documents of all other shards, they may
/// be stored or not. All answers of these shards are dropped.
/// If we return with NO_ERROR it is guaranteed that all shards reported success
/// for their documents.
////////////////////////////////////////////////////////////////////////////////

Result createDocumentOnCoordinator(transaction::Methods& trx, std::string const& collname,
                                   arangodb::OperationOptions const& options,
                                   VPackSlice const& slice,
                                   arangodb::rest::ResponseCode& responseCode,
                                   std::unordered_map<int, size_t>& errorCounter,
                                   std::shared_ptr<VPackBuilder>& resultBody) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr should only happen during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  ClusterInfo* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  auto collid = std::to_string(collinfo->id());
  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();

  // create vars used in this function
  bool const useMultiple = slice.isArray();  // insert more than one document
  std::unordered_map<ShardID, std::vector<std::pair<VPackSlice, std::string>>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;

  {
    // create shard map
    int res = TRI_ERROR_NO_ERROR;
    if (useMultiple) {
      for (VPackSlice value : VPackArrayIterator(slice)) {
        res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                     reverseMapping, value, options.isRestore);
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }
    } else {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, slice, options.isRestore);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }

  // lazily begin transactions on leaders
  const bool isManaged = trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isManaged && shardMap.size() > 1) {
    Result res = beginTransactionOnSomeLeaders(*trx.state(), collinfo, shardMap);
    if (res.fail()) {
      return res.errorNumber();
    }
  }

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document?collection=";

  std::string const optsUrlPart =
      std::string("&waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnNew=" + (options.returnNew ? "true" : "false") +
      "&returnOld=" + (options.returnOld ? "true" : "false") +
      "&isRestore=" + (options.isRestore ? "true" : "false") + "&" +
      StaticStrings::OverWrite + "=" + (options.overwrite ? "true" : "false");

  VPackBuilder reqBuilder;

  // Now prepare the requests:
  std::vector<ClusterCommRequest> requests;
  std::shared_ptr<std::string> body;

  for (auto const& it : shardMap) {
    if (!useMultiple) {
      TRI_ASSERT(it.second.size() == 1);
      auto idx = it.second.front();
      if (idx.second.empty()) {
        body = std::make_shared<std::string>(slice.toJson());
      } else {
        reqBuilder.clear();
        reqBuilder.openObject();
        reqBuilder.add(StaticStrings::KeyString, VPackValue(idx.second));
        TRI_SanitizeObject(slice, reqBuilder);
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
      }
    } else {
      reqBuilder.clear();
      reqBuilder.openArray();
      for (auto const& idx : it.second) {
        if (idx.second.empty()) {
          reqBuilder.add(idx.first);
        } else {
          reqBuilder.openObject();
          reqBuilder.add(StaticStrings::KeyString, VPackValue(idx.second));
          TRI_SanitizeObject(idx.first, reqBuilder);
          reqBuilder.close();
        }
      }
      reqBuilder.close();
      body = std::make_shared<std::string>(reqBuilder.slice().toJson());
    }

    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first, *headers);
    requests.emplace_back("shard:" + it.first, arangodb::rest::RequestType::POST,
                          baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
                          body, std::move(headers));
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ true, /*retryOnBackUnavai*/ !isManaged);

  // Now listen to the results:
  if (!useMultiple) {
    TRI_ASSERT(requests.size() == 1);
    auto const& req = requests[0];
    auto& res = req.result;

    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return commError;
    }

    responseCode = res.answer_code;
    TRI_ASSERT(res.answer != nullptr);
    auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    resultBody.swap(parsedResult);
    return TRI_ERROR_NO_ERROR;
  }

  std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;

  collectResultsFromAllShards<std::pair<VPackSlice, std::string>>(shardMap, requests,
                                                                  errorCounter, resultMap,
                                                                  responseCode);

  responseCode = (options.waitForSync ? rest::ResponseCode::CREATED
                                      : rest::ResponseCode::ACCEPTED);
  mergeResults(reverseMapping, resultMap, resultBody);

  // the cluster operation was OK, however,
  // the DBserver could have reported an error.
  return Result{};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(arangodb::transaction::Methods& trx,
                                std::string const& collname, VPackSlice const slice,
                                arangodb::OperationOptions const& options,
                                arangodb::rest::ResponseCode& responseCode,
                                std::unordered_map<int, size_t>& errorCounter,
                                std::shared_ptr<arangodb::velocypack::Builder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  bool useDefaultSharding = collinfo->usesDefaultShardKeys();
  auto collid = std::to_string(collinfo->id());
  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();
  bool useMultiple = slice.isArray();

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";

  std::string const optsUrlPart =
      std::string("?waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnOld=" + (options.returnOld ? "true" : "false") +
      "&ignoreRevs=" + (options.ignoreRevs ? "true" : "false");

  VPackBuilder reqBuilder;

  if (useDefaultSharding) {
    // fastpath we know which server is responsible.

    // decompose the input into correct shards.
    // Send the correct documents to the correct shards
    // Merge the results with static merge helper

    std::unordered_map<ShardID, std::vector<VPackSlice>> shardMap;
    std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
    auto workOnOneNode = [&shardMap, &ci, &collid, &collinfo,
                          &reverseMapping](VPackSlice const value) -> int {
      // Sort out the _key attribute and identify the shard responsible for it.

      arangodb::velocypack::StringRef _key(transaction::helpers::extractKeyPart(value));
      ShardID shardID;
      if (_key.empty()) {
        // We have invalid input at this point.
        // However we can work with the other babies.
        // This is for compatibility with single server
        // We just assign it to any shard and pretend the user has given a key
        std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(collid);
        shardID = shards->at(0);
      } else {
        // Now find the responsible shard:
        bool usesDefaultShardingAttributes;
        int error =
            collinfo->getResponsibleShard(arangodb::velocypack::Slice::emptyObjectSlice(),
                                          true, shardID, usesDefaultShardingAttributes,
                                          _key.toString());

        if (error == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
          return TRI_ERROR_CLUSTER_SHARD_GONE;
        }
      }

      // We found the responsible shard. Add it to the list.
      auto it = shardMap.find(shardID);
      if (it == shardMap.end()) {
        shardMap.emplace(shardID, std::vector<VPackSlice>{value});
        reverseMapping.emplace_back(shardID, 0);
      } else {
        it->second.emplace_back(value);
        reverseMapping.emplace_back(shardID, it->second.size() - 1);
      }
      return TRI_ERROR_NO_ERROR;
    };

    if (useMultiple) {  // slice is array of document values
      for (VPackSlice value : VPackArrayIterator(slice)) {
        int res = workOnOneNode(value);
        if (res != TRI_ERROR_NO_ERROR) {
          // Is early abortion correct?
          return res;
        }
      }
    } else {
      int res = workOnOneNode(slice);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }

    // We sorted the shards correctly.

    // lazily begin transactions on leaders
    const bool isManaged = trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
    if (isManaged && shardMap.size() > 1) {
      Result res = beginTransactionOnSomeLeaders(*trx.state(), collinfo, shardMap);
      if (res.fail()) {
        return res.errorNumber();
      }
    }

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    for (auto const& it : shardMap) {
      std::shared_ptr<std::string> body;
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(slice.toJson());
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& value : it.second) {
          reqBuilder.add(value);
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
      }
      auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first, *headers);
      requests.emplace_back("shard:" + it.first, arangodb::rest::RequestType::DELETE_REQ,
                            baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
                            body, std::move(headers));
    }

    // Perform the requests
    cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, Logger::COMMUNICATION,
                        /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);

    // Now listen to the results:
    if (!useMultiple) {
      TRI_ASSERT(requests.size() == 1);
      auto const& req = requests[0];
      auto& res = req.result;

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackSlice>(shardMap, requests, errorCounter,
                                            resultMap, responseCode);
    mergeResults(reverseMapping, resultMap, resultBody);
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // slowpath we do not know which server is responsible ask all of them.

  // lazily begin transactions on leaders
  const bool isManaged = trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isManaged && shardIds->size() > 1) {
    Result res = ::beginTransactionOnAllLeaders(trx, *shardIds);
    if (res.fail()) {
      return res.errorNumber();
    }
  }

  // We simply send the body to all shards and await their results.
  // As soon as we have the results we merge them in the following way:
  // For 1 .. slice.length()
  //    for res : allResults
  //      if res != NOT_FOUND => insert this result. skip other results
  //    end
  //    if (!skipped) => insert NOT_FOUND

  auto body = std::make_shared<std::string>(slice.toJson());
  std::vector<ClusterCommRequest> requests;
  for (std::pair<ShardID, std::vector<ServerID>> const& shardServers : *shardIds) {
    ShardID const& shard = shardServers.first;
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    addTransactionHeaderForShard(trx, *shardIds, shard, *headers);
    requests.emplace_back("shard:" + shard, arangodb::rest::RequestType::DELETE_REQ,
                          baseUrl + StringUtils::urlEncode(shard) + optsUrlPart,
                          body, std::move(headers));
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    size_t count;
    int nrok = 0;
    for (count = requests.size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code != arangodb::rest::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1)) {
          nrok++;

          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
          resultBody.swap(parsedResult);
        }
      }
    }

    // Note that nrok is always at least 1!
    if (nrok > 1) {
      return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
    }
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // We select all results from all shards an merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(shardIds->size());
  // If no server responds we return 500
  responseCode = rest::ResponseCode::SERVER_ERROR;
  for (auto const& req : requests) {
    auto res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Local data structures are automatically freed
      return error;
    }
    if (res.answer_code == rest::ResponseCode::OK ||
        res.answer_code == rest::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPackBuilderPtrNoUniquenessChecks());
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardIds->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter,
                        static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

Result truncateCollectionOnCoordinator(transaction::Methods& trx, std::string const& collname) {
  Result res;
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  // Some stuff to prepare cluster-intern requests:
  // We have to contact everybody:
  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();

  // lazily begin transactions on all leader shards
  if (trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    res = ::beginTransactionOnAllLeaders(trx, *shardIds);
    if (res.fail()) {
      return res;
    }
  }

  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shardIds) {
    std::unordered_map<std::string, std::string> headers;
    addTransactionHeaderForShard(trx, *shardIds, /*shard*/ p.first, headers);
    cc->asyncRequest(coordTransactionID, "shard:" + p.first,
                     arangodb::rest::RequestType::PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + p.first + "/truncate",
                     std::shared_ptr<std::string>(), headers, nullptr, 600.0);
  }
  // Now listen to the results:
  unsigned int count;
  unsigned int nrok = 0;
  for (count = (unsigned int)shardIds->size(); count > 0; count--) {
    auto ccRes = cc->wait(coordTransactionID, 0, "", 0.0);
    if (ccRes.status == CL_COMM_RECEIVED) {
      if (ccRes.answer_code == arangodb::rest::ResponseCode::OK) {
        nrok++;
      } else if (ccRes.answer->payload().isObject()) {
        VPackSlice answer = ccRes.answer->payload();
        return res.reset(VelocyPackHelper::readNumericValue(answer, StaticStrings::ErrorNum,
                                                            TRI_ERROR_TRANSACTION_INTERNAL),
                         VelocyPackHelper::getStringValue(answer, StaticStrings::ErrorMessage,
                                                          ""));
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok < shardIds->size()) {
    return res.reset(TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(arangodb::transaction::Methods& trx, std::string const& collname,
                             VPackSlice slice, OperationOptions const& options,
                             arangodb::rest::ResponseCode& responseCode,
                             std::unordered_map<int, size_t>& errorCounter,
                             std::shared_ptr<VPackBuilder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  auto collid = std::to_string(collinfo->id());
  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();

  // If _key is the one and only sharding attribute, we can do this quickly,
  // because we can easily determine which shard is responsible for the
  // document. Otherwise we have to contact all shards and ask them to
  // delete the document. All but one will not know it.
  // Now find the responsible shard(s)

  ShardID shardID;

  std::unordered_map<ShardID, std::vector<VPackSlice>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  bool canUseFastPath = true;
  if (useMultiple) {
    for (VPackSlice value : VPackArrayIterator(slice)) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping, value);
      if (res != TRI_ERROR_NO_ERROR) {
        canUseFastPath = false;
        shardMap.clear();
        reverseMapping.clear();
        break;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping, slice);
    if (res != TRI_ERROR_NO_ERROR) {
      canUseFastPath = false;
    }
  }

  // lazily begin transactions on leaders
  const bool isManaged = trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  // Some stuff to prepare cluster-internal requests:

  std::string baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";
  std::string optsUrlPart =
      std::string("?ignoreRevs=") + (options.ignoreRevs ? "true" : "false");

  arangodb::rest::RequestType reqType;
  if (!useMultiple) {
    if (options.silent) {
      reqType = arangodb::rest::RequestType::HEAD;
    } else {
      reqType = arangodb::rest::RequestType::GET;
    }
  } else {
    reqType = arangodb::rest::RequestType::PUT;
    if (options.silent) {
      optsUrlPart += std::string("&silent=true");
    }
    optsUrlPart += std::string("&onlyget=true");
  }

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    if (isManaged && shardMap.size() > 1) {  // lazily begin the transaction
      Result res = beginTransactionOnSomeLeaders(*trx.state(), collinfo, shardMap);
      if (res.fail()) {
        return res.errorNumber();
      }
    }

    VPackBuilder reqBuilder;

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
        addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first, *headers);
        if (!options.ignoreRevs && slice.hasKey(StaticStrings::RevString)) {
          headers->emplace("if-match", slice.get(StaticStrings::RevString).copyString());
        }

        VPackSlice keySlice = slice;
        if (slice.isObject()) {
          keySlice = slice.get(StaticStrings::KeyString);
        }

        // We send to single endpoint
        requests.emplace_back("shard:" + it.first, reqType,
                              baseUrl + StringUtils::urlEncode(it.first) + "/" +
                                  StringUtils::urlEncode(keySlice.copyString()) + optsUrlPart,
                              body, std::move(headers));
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& value : it.second) {
          reqBuilder.add(value);
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
        auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
        addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first, *headers);
        // We send to Babies endpoint
        requests.emplace_back("shard:" + it.first, reqType,
                              baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
                              body, std::move(headers));
      }
    }

    // Perform the requests
    cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                        /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);

    // Now listen to the results:
    if (!useMultiple) {
      TRI_ASSERT(requests.size() == 1);
      auto const& req = requests[0];
      auto res = req.result;

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);

      auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackSlice>(shardMap, requests, errorCounter,
                                            resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  if (isManaged) {  // lazily begin the transaction
    Result res = ::beginTransactionOnAllLeaders(trx, *shardIds);
    if (res.fail()) {
      return res.errorNumber();
    }
  }

  std::vector<ClusterCommRequest> requests;
  if (!useMultiple) {
    const bool addMatch = !options.ignoreRevs && slice.hasKey(StaticStrings::RevString);
    for (std::pair<ShardID, std::vector<ServerID>> const& shardServers : *shardIds) {
      VPackSlice keySlice = slice;
      if (slice.isObject()) {
        keySlice = slice.get(StaticStrings::KeyString);
      }
      ShardID const& shard = shardServers.first;
      auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      addTransactionHeaderForShard(trx, *shardIds, shard, *headers);
      if (addMatch) {
        headers->emplace("if-match", slice.get(StaticStrings::RevString).copyString());
      }
      requests.emplace_back("shard:" + shard, reqType,
                            baseUrl + StringUtils::urlEncode(shard) + "/" +
                                StringUtils::urlEncode(keySlice.copyString()) + optsUrlPart,
                            nullptr, std::move(headers));
    }
  } else {
    auto body = std::make_shared<std::string>(slice.toJson());
    for (std::pair<ShardID, std::vector<ServerID>> const& shardServers : *shardIds) {
      ShardID const& shard = shardServers.first;
      auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      addTransactionHeaderForShard(trx, *shardIds, shard, *headers);
      requests.emplace_back("shard:" + shard, reqType,
                            baseUrl + StringUtils::urlEncode(shard) + optsUrlPart,
                            body, std::move(headers));
    }
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    size_t count;
    int nrok = 0;
    int commError = TRI_ERROR_NO_ERROR;
    for (count = requests.size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code != arangodb::rest::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1 && commError == TRI_ERROR_NO_ERROR)) {
          nrok++;
          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
          resultBody.swap(parsedResult);
        }
      } else {
        commError = handleGeneralCommErrors(&res);
      }
    }
    if (nrok == 0) {
      // This can only happen, if a commError was encountered!
      return commError;
    }
    if (nrok > 1) {
      return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
    }
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // We select all results from all shards and merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(shardIds->size());
  // If no server responds we return 500
  responseCode = rest::ResponseCode::SERVER_ERROR;
  for (auto const& req : requests) {
    auto& res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Local data structores are automatically freed
      return error;
    }
    if (res.answer_code == rest::ResponseCode::OK ||
        res.answer_code == rest::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPackBuilderPtrNoUniquenessChecks());
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardIds->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter,
                        static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

/// @brief fetch edges from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        All non-empty and non-cached results
///        of DBServers will be inserted in the
///        datalake. Slices used in the result
///        point to content inside of this lake
///        only and do not run out of scope unless
///        the lake is cleared.

int fetchEdgesFromEngines(std::string const& dbname,
                          std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
                          VPackSlice const vertexId, size_t depth,
                          std::unordered_map<arangodb::velocypack::StringRef, VPackSlice>& cache,
                          std::vector<VPackSlice>& result,
                          std::vector<std::shared_ptr<VPackBuilder>>& datalake,
                          VPackBuilder& builder, size_t& filtered, size_t& read) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  // TODO map id => ServerID if possible
  // And go fast-path

  // This function works for one specific vertex
  // or for a list of vertices.
  TRI_ASSERT(vertexId.isString() || vertexId.isArray());
  builder.clear();
  builder.openObject();
  builder.add("depth", VPackValue(depth));
  builder.add("keys", vertexId);
  builder.close();

  std::string const url =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_internal/traverser/edge/";

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& engine : *engines) {
    requests.emplace_back("server:" + engine.first, RequestType::PUT,
                          url + StringUtils::itoa(engine.second), body);
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ false);

  result.clear();
  // Now listen to the results:
  for (auto const& req : requests) {
    bool allCached = true;
    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      // oh-oh cluster is in a bad state
      return commError;
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }
    filtered +=
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(resSlice,
                                                                    "filtered", 0);
    read +=
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(resSlice,
                                                                    "readIndex", 0);
    VPackSlice edges = resSlice.get("edges");
    for (auto const& e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("a23b5", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      arangodb::velocypack::StringRef idRef(id);
      auto resE = cache.insert({idRef, e});
      if (resE.second) {
        // This edge is not yet cached.
        allCached = false;
        result.emplace_back(e);
      } else {
        result.emplace_back(resE.first->second);
      }
    }
    if (!allCached) {
      datalake.emplace_back(resBody);
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief fetch vertices from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        If any server responds with a document
///        it will be inserted into the result.
///        If no server responds with a document
///        a 'null' will be inserted into the result.

void fetchVerticesFromEngines(
    std::string const& dbname,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    std::unordered_set<arangodb::velocypack::StringRef>& vertexIds,
    std::unordered_map<arangodb::velocypack::StringRef, std::shared_ptr<VPackBuffer<uint8_t>>>& result,
    VPackBuilder& builder) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return;
  }
  // TODO map id => ServerID if possible
  // And go fast-path

  // slow path, sharding not deducable from _id
  builder.clear();
  builder.openObject();
  builder.add(VPackValue("keys"));
  builder.openArray();
  for (auto const& v : vertexIds) {
    // TRI_ASSERT(v.isString());
    builder.add(VPackValuePair(v.data(), v.length(), VPackValueType::String));
  }
  builder.close();  // 'keys' Array
  builder.close();  // base object

  std::string const url =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_internal/traverser/vertex/";

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& engine : *engines) {
    requests.emplace_back("server:" + engine.first, RequestType::PUT,
                          url + StringUtils::itoa(engine.second), body);
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION, false);

  // Now listen to the results:
  for (auto const& req : requests) {
    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      // oh-oh cluster is in a bad state
      THROW_ARANGO_EXCEPTION(commError);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      THROW_ARANGO_EXCEPTION(TRI_ERROR_HTTP_CORRUPTED_JSON);
    }
    if (res.answer_code != ResponseCode::OK) {
      int code =
          arangodb::basics::VelocyPackHelper::getNumericValue<int>(resSlice,
                                                                   "errorNum", TRI_ERROR_INTERNAL);
      // We have an error case here. Throw it.
      THROW_ARANGO_EXCEPTION_MESSAGE(code, arangodb::basics::VelocyPackHelper::getStringValue(
                                               resSlice, StaticStrings::ErrorMessage,
                                               TRI_errno_string(code)));
    }
    for (auto const& pair : VPackObjectIterator(resSlice)) {
      arangodb::velocypack::StringRef key(pair.key);
      if (vertexIds.erase(key) == 0) {
        // We either found the same vertex twice,
        // or found a vertex we did not request.
        // Anyways something somewhere went seriously wrong
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS);
      }
      TRI_ASSERT(result.find(key) == result.end());
      auto val = VPackBuilder::clone(pair.value);

      VPackSlice id = val.slice().get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("e0b50", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      TRI_ASSERT(id.isString());
      result.emplace(arangodb::velocypack::StringRef(id), val.steal());
    }
  }

  // Fill everything we did not find with NULL
  for (auto const& v : vertexIds) {
    result.emplace(v,
                   VPackBuilder::clone(arangodb::velocypack::Slice::nullSlice()).steal());
  }
  vertexIds.clear();
}

/// @brief fetch vertices from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        If any server responds with a document
///        it will be inserted into the result.
///        If no server responds with a document
///        a 'null' will be inserted into the result.
///        ShortestPathVariant

void fetchVerticesFromEngines(
    std::string const& dbname,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    std::unordered_set<arangodb::velocypack::StringRef>& vertexIds,
    std::unordered_map<arangodb::velocypack::StringRef, arangodb::velocypack::Slice>& result,
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>>& datalake,
    VPackBuilder& builder) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return;
  }
  // TODO map id => ServerID if possible
  // And go fast-path

  // slow path, sharding not deducable from _id
  builder.clear();
  builder.openObject();
  builder.add(VPackValue("keys"));
  builder.openArray();
  for (auto const& v : vertexIds) {
    // TRI_ASSERT(v.isString());
    builder.add(VPackValuePair(v.data(), v.length(), VPackValueType::String));
  }
  builder.close();  // 'keys' Array
  builder.close();  // base object

  std::string const url =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_internal/traverser/vertex/";

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& engine : *engines) {
    requests.emplace_back("server:" + engine.first, RequestType::PUT,
                          url + StringUtils::itoa(engine.second), body);
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ false);

  // Now listen to the results:
  for (auto const& req : requests) {
    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      // oh-oh cluster is in a bad state
      THROW_ARANGO_EXCEPTION(commError);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      THROW_ARANGO_EXCEPTION(TRI_ERROR_HTTP_CORRUPTED_JSON);
    }
    if (res.answer_code != ResponseCode::OK) {
      int code =
          arangodb::basics::VelocyPackHelper::getNumericValue<int>(resSlice,
                                                                   "errorNum", TRI_ERROR_INTERNAL);
      // We have an error case here. Throw it.
      THROW_ARANGO_EXCEPTION_MESSAGE(code, arangodb::basics::VelocyPackHelper::getStringValue(
                                               resSlice, StaticStrings::ErrorMessage,
                                               TRI_errno_string(code)));
    }
    bool cached = false;

    for (auto const& pair : VPackObjectIterator(resSlice)) {
      arangodb::velocypack::StringRef key(pair.key);
      if (vertexIds.erase(key) == 0) {
        // We either found the same vertex twice,
        // or found a vertex we did not request.
        // Anyways something somewhere went seriously wrong
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS);
      }
      TRI_ASSERT(result.find(key) == result.end());
      if (!cached) {
        datalake.emplace_back(resBody);
        cached = true;
      }
      // Protected by datalake
      result.emplace(key, pair.value);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all edges on coordinator using a Traverser Filter
////////////////////////////////////////////////////////////////////////////////

int getFilteredEdgesOnCoordinator(arangodb::transaction::Methods const& trx,
                                  std::string const& collname, std::string const& vertex,
                                  TRI_edge_direction_e const& direction,
                                  arangodb::rest::ResponseCode& responseCode,
                                  VPackBuilder& result) {
  TRI_ASSERT(result.isOpenObject());

  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> shards;
  if (collinfo->isSmart() && collinfo->type() == TRI_COL_TYPE_EDGE) {
    auto names = collinfo->realNamesForRead();
    shards = std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();
    for (auto const& n : names) {
      collinfo = ci->getCollection(dbname, n);
      auto smap = collinfo->shardIds();
      for (auto const& x : *smap) {
        shards->insert(x);
      }
    }
  } else {
    shards = collinfo->shardIds();
  }
  std::string queryParameters = "?vertex=" + StringUtils::urlEncode(vertex);
  if (direction == TRI_EDGE_IN) {
    queryParameters += "&direction=in";
  } else if (direction == TRI_EDGE_OUT) {
    queryParameters += "&direction=out";
  }
  std::vector<ClusterCommRequest> requests;
  std::string baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/edges/";

  auto body = std::make_shared<std::string>();
  for (auto const& p : *shards) {
    // this code is not used in transactions anyways
    auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
    requests.emplace_back("shard:" + p.first, arangodb::rest::RequestType::GET,
                          baseUrl + StringUtils::urlEncode(p.first) + queryParameters,
                          body, std::move(headers));
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ true);

  size_t filtered = 0;
  size_t scannedIndex = 0;
  responseCode = arangodb::rest::ResponseCode::OK;

  result.add("edges", VPackValue(VPackValueType::Array));

  // All requests send, now collect results.
  for (auto const& req : requests) {
    auto& res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Report.
      return error;
    }
    TRI_ASSERT(res.answer != nullptr);
    std::shared_ptr<VPackBuilder> shardResult =
        res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();

    if (shardResult == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    VPackSlice shardSlice = shardResult->slice();
    if (!shardSlice.isObject()) {
      return TRI_ERROR_INTERNAL;
    }

    bool const isError =
        arangodb::basics::VelocyPackHelper::getBooleanValue(shardSlice, "error", false);

    if (isError) {
      // shard returned an error
      return arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          shardSlice, "errorNum", TRI_ERROR_INTERNAL);
    }

    VPackSlice docs = shardSlice.get("edges");

    if (!docs.isArray()) {
      return TRI_ERROR_INTERNAL;
    }

    for (auto const& doc : VPackArrayIterator(docs)) {
      result.add(doc);
    }

    VPackSlice stats = shardSlice.get("stats");
    if (stats.isObject()) {
      filtered += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(stats,
                                                                              "filtered", 0);
      scannedIndex += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
          stats, "scannedIndex", 0);
    }
  }
  result.close();  // edges

  result.add("stats", VPackValue(VPackValueType::Object));
  result.add("scannedIndex", VPackValue(scannedIndex));
  result.add("filtered", VPackValue(filtered));
  result.close();  // stats

  // Leave outer Object open
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    transaction::Methods& trx, std::string const& collname, VPackSlice const& slice,
    arangodb::OperationOptions const& options, bool isPatch,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
    arangodb::rest::ResponseCode& responseCode, std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<VPackBuilder>& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo = ci->getCollection(dbname, collname);
  auto collid = std::to_string(collinfo->id());
  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();

  // We have a fast path and a slow path. The fast path only asks one shard
  // to do the job and the slow path asks them all and expects to get
  // "not found" from all but one shard. We have to cover the following
  // cases:
  //   isPatch == false    (this is a "replace" operation)
  //     Here, the complete new document is given, we assume that we
  //     can read off the responsible shard, therefore can use the fast
  //     path, this is always true if _key is the one and only sharding
  //     attribute, however, if there is any other sharding attribute,
  //     it is possible that the user has changed the values in any of
  //     them, in that case we will get a "not found" or a "sharding
  //     attributes changed answer" in the fast path. In the first case
  //     we have to delegate to the slow path.
  //   isPatch == true     (this is an "update" operation)
  //     In this case we might or might not have all sharding attributes
  //     specified in the partial document given. If _key is the one and
  //     only sharding attribute, it is always given, if not all sharding
  //     attributes are explicitly given (at least as value `null`), we must
  //     assume that the fast path cannot be used. If all sharding attributes
  //     are given, we first try the fast path, but might, as above,
  //     have to use the slow path after all.

  ShardID shardID;

  std::unordered_map<ShardID, std::vector<VPackSlice>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  bool canUseFastPath = true;
  if (useMultiple) {
    for (VPackSlice value : VPackArrayIterator(slice)) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping, value);
      if (res != TRI_ERROR_NO_ERROR) {
        if (!isPatch) {
          return res;
        }
        canUseFastPath = false;
        shardMap.clear();
        reverseMapping.clear();
        break;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping, slice);
    if (res != TRI_ERROR_NO_ERROR) {
      if (!isPatch) {
        return res;
      }
      canUseFastPath = false;
    }
  }

  // Some stuff to prepare cluster-internal requests:

  std::string baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";
  std::string optsUrlPart =
      std::string("?waitForSync=") + (options.waitForSync ? "true" : "false");
  optsUrlPart += std::string("&ignoreRevs=") + (options.ignoreRevs ? "true" : "false") +
                 std::string("&isRestore=") + (options.isRestore ? "true" : "false");

  arangodb::rest::RequestType reqType;
  if (isPatch) {
    reqType = arangodb::rest::RequestType::PATCH;
    if (!options.keepNull) {
      optsUrlPart += "&keepNull=false";
    }
    if (options.mergeObjects) {
      optsUrlPart += "&mergeObjects=true";
    } else {
      optsUrlPart += "&mergeObjects=false";
    }
  } else {
    reqType = arangodb::rest::RequestType::PUT;
  }
  if (options.returnNew) {
    optsUrlPart += "&returnNew=true";
  }

  if (options.returnOld) {
    optsUrlPart += "&returnOld=true";
  }

  const bool isManaged = trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    if (isManaged && shardMap.size() > 1) {  // lazily begin transactions on leaders
      Result res = beginTransactionOnSomeLeaders(*trx.state(), collinfo, shardMap);
      if (res.fail()) {
        return res.errorNumber();
      }
    }

    std::vector<ClusterCommRequest> requests;
    VPackBuilder reqBuilder;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first, *headers);

      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(slice.toJson());

        auto keySlice = slice.get(StaticStrings::KeyString);
        if (!keySlice.isString()) {
          return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
        }

        arangodb::velocypack::StringRef keyStr(keySlice);
        // We send to single endpoint
        requests.emplace_back("shard:" + it.first, reqType,
                              baseUrl + StringUtils::urlEncode(it.first) + "/" +
                                  StringUtils::urlEncode(keyStr.data(), keyStr.length()) + optsUrlPart,
                              body, std::move(headers));
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& value : it.second) {
          reqBuilder.add(value);
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
        // We send to Babies endpoint
        requests.emplace_back("shard:" + it.first, reqType,
                              baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
                              body, std::move(headers));
      }
    }

    // Perform the requests
    cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, Logger::COMMUNICATION,
                        /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);

    // Now listen to the results:
    if (!useMultiple) {
      TRI_ASSERT(requests.size() == 1);
      auto res = requests[0].result;

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackSlice>(shardMap, requests, errorCounter,
                                            resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  if (isManaged) {  // lazily begin the transaction
    Result res = ::beginTransactionOnAllLeaders(trx, *shardIds);
    if (res.fail()) {
      return res.errorNumber();
    }
  }

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(slice.toJson());
  if (!useMultiple) {
    std::string key = slice.get(StaticStrings::KeyString).copyString();
    for (std::pair<ShardID, std::vector<ServerID>> const& shardServers : *shardIds) {
      ShardID const& shard = shardServers.first;
      auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      addTransactionHeaderForShard(trx, *shardIds, /*shard*/ shard, *headers);
      requests.emplace_back("shard:" + shard, reqType,
                            baseUrl + StringUtils::urlEncode(shard) + "/" + key + optsUrlPart,
                            body, std::move(headers));
    }
  } else {
    for (std::pair<ShardID, std::vector<ServerID>> const& shardServers : *shardIds) {
      ShardID const& shard = shardServers.first;
      auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
      addTransactionHeaderForShard(trx, *shardIds, /*shard*/ shard, *headers);
      requests.emplace_back("shard:" + shard, reqType,
                            baseUrl + StringUtils::urlEncode(shard) + optsUrlPart,
                            body, std::move(headers));
    }
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ true, /*retryOnBackUnvlbl*/ !isManaged);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    int nrok = 0;
    int commError = TRI_ERROR_NO_ERROR;
    for (size_t count = shardIds->size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code != arangodb::rest::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1 && commError == TRI_ERROR_NO_ERROR)) {
          nrok++;
          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
          resultBody.swap(parsedResult);
        }
      } else {
        commError = handleGeneralCommErrors(&res);
      }
    }
    if (nrok == 0) {
      // This can only happen, if a commError was encountered!
      return commError;
    }
    if (nrok > 1) {
      return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
    }
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  responseCode = rest::ResponseCode::SERVER_ERROR;
  // We select all results from all shards an merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(requests.size());
  for (auto const& req : requests) {
    auto res = req.result;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Just report.
      // Local data structores are automatically freed
      return error;
    }
    if (res.answer_code == rest::ResponseCode::OK ||
        res.answer_code == rest::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPackBuilderPtrNoUniquenessChecks());
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardIds->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter,
                        static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush Wal on all DBservers
////////////////////////////////////////////////////////////////////////////////

int flushWalOnAllDBServers(bool waitForSync, bool waitForCollector, double maxWaitTime) {
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string url = std::string("/_admin/wal/flush?waitForSync=") +
                    (waitForSync ? "true" : "false") +
                    "&waitForCollector=" + (waitForCollector ? "true" : "false");
  if (maxWaitTime >= 0.0) {
    url += "&maxWaitTime=" + std::to_string(maxWaitTime);
  }

  auto body = std::make_shared<std::string const>();
  std::unordered_map<std::string, std::string> headers;
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    // set collection name (shard id)
    cc->asyncRequest(coordTransactionID, "server:" + *it, arangodb::rest::RequestType::PUT,
                     url, body, headers, nullptr, 120.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  int globalErrorCode = TRI_ERROR_INTERNAL;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        nrok++;
      } else {
        // got an error. Now try to find the errorNum value returned (if any)
        TRI_ASSERT(res.answer != nullptr);
        auto resBody = res.answer->toVelocyPackBuilderPtr();
        VPackSlice resSlice = resBody->slice();
        if (resSlice.isObject()) {
          int code = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
              resSlice, "errorNum", TRI_ERROR_INTERNAL);

          if (code != TRI_ERROR_NO_ERROR) {
            globalErrorCode = code;
          }
        }
      }
    }
  }

  if (nrok != (int)DBservers.size()) {
    LOG_TOPIC("48327", WARN, arangodb::Logger::CLUSTER)
        << "could not flush WAL on all servers. confirmed: " << nrok
        << ", expected: " << DBservers.size();
    return globalErrorCode;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief get TTL statistics from all DBservers and aggregate them
Result getTtlStatisticsFromAllDBServers(TtlStatistics& out) {
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return Result(TRI_ERROR_SHUTTING_DOWN);
  }
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string const url("/_api/ttl/statistics");

  auto body = std::make_shared<std::string const>();
  std::unordered_map<std::string, std::string> headers;
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    // set collection name (shard id)
    cc->asyncRequest(coordTransactionID, "server:" + *it, arangodb::rest::RequestType::GET,
                     url, body, headers, nullptr, 120.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  int globalErrorCode = TRI_ERROR_INTERNAL;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        VPackSlice answer = res.answer->payload();
        out += answer.get("result");
        nrok++;
      } else {
        // got an error. Now try to find the errorNum value returned (if any)
        TRI_ASSERT(res.answer != nullptr);
        auto resBody = res.answer->toVelocyPackBuilderPtr();
        VPackSlice resSlice = resBody->slice();
        if (resSlice.isObject()) {
          int code = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
              resSlice, "errorNum", TRI_ERROR_INTERNAL);

          if (code != TRI_ERROR_NO_ERROR) {
            globalErrorCode = code;
          }
        }
      }
    }
  }

  if (nrok != (int)DBservers.size()) {
    return Result(globalErrorCode);
  }

  return Result();
}

/// @brief get TTL properties from all DBservers
Result getTtlPropertiesFromAllDBServers(VPackBuilder& out) {
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return Result(TRI_ERROR_SHUTTING_DOWN);
  }
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string const url("/_api/ttl/properties");

  auto body = std::make_shared<std::string const>();
  std::unordered_map<std::string, std::string> headers;
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    // set collection name (shard id)
    cc->asyncRequest(coordTransactionID, "server:" + *it, arangodb::rest::RequestType::GET,
                     url, body, headers, nullptr, 120.0);
  }

  // Now listen to the results:
  bool set = false;
  int count;
  int nrok = 0;
  int globalErrorCode = TRI_ERROR_INTERNAL;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        VPackSlice answer = res.answer->payload();
        if (!set) {
          out.add(answer.get("result"));
          set = true;
        }
        nrok++;
      } else {
        // got an error. Now try to find the errorNum value returned (if any)
        TRI_ASSERT(res.answer != nullptr);
        auto resBody = res.answer->toVelocyPackBuilderPtr();
        VPackSlice resSlice = resBody->slice();
        if (resSlice.isObject()) {
          int code = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
              resSlice, "errorNum", TRI_ERROR_INTERNAL);

          if (code != TRI_ERROR_NO_ERROR) {
            globalErrorCode = code;
          }
        }
      }
    }
  }

  if (nrok != (int)DBservers.size()) {
    return Result(globalErrorCode);
  }

  return Result();
}

/// @brief set TTL properties on all DBservers
Result setTtlPropertiesOnAllDBServers(VPackSlice const& properties, VPackBuilder& out) {
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return Result(TRI_ERROR_SHUTTING_DOWN);
  }
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string const url("/_api/ttl/properties");

  auto body = std::make_shared<std::string>(properties.toJson());
  std::unordered_map<std::string, std::string> headers;
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    // set collection name (shard id)
    cc->asyncRequest(coordTransactionID, "server:" + *it, arangodb::rest::RequestType::PUT,
                     url, body, headers, nullptr, 120.0);
  }

  // Now listen to the results:
  bool set = false;
  int count;
  int nrok = 0;
  int globalErrorCode = TRI_ERROR_INTERNAL;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        VPackSlice answer = res.answer->payload();
        if (!set) {
          out.add(answer.get("result"));
          set = true;
        }
        nrok++;
      } else {
        // got an error. Now try to find the errorNum value returned (if any)
        TRI_ASSERT(res.answer != nullptr);
        auto resBody = res.answer->toVelocyPackBuilderPtr();
        VPackSlice resSlice = resBody->slice();
        if (resSlice.isObject()) {
          int code = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
              resSlice, "errorNum", TRI_ERROR_INTERNAL);

          if (code != TRI_ERROR_NO_ERROR) {
            globalErrorCode = code;
          }
        }
      }
    }
  }

  if (nrok != (int)DBservers.size()) {
    return Result(globalErrorCode);
  }

  return Result();
}

#ifndef USE_ENTERPRISE

std::vector<std::shared_ptr<LogicalCollection>> ClusterMethods::createCollectionOnCoordinator(
    TRI_vocbase_t& vocbase, velocypack::Slice parameters, bool ignoreDistributeShardsLikeErrors,
    bool waitForSyncReplication, bool enforceReplicationFactor) {
  TRI_ASSERT(parameters.isArray());
  // Collections are temporary collections object that undergoes sanity checks
  // etc. It is not used anywhere and will be cleaned up after this call.
  std::vector<std::shared_ptr<LogicalCollection>> cols;
  for (VPackSlice p : VPackArrayIterator(parameters)) {
    cols.emplace_back(std::make_shared<LogicalCollection>(vocbase, p, true, 0));
  }

  // Persist collection will return the real object.
  auto usableCollectionPointers =
      persistCollectionsInAgency(cols, ignoreDistributeShardsLikeErrors,
                                 waitForSyncReplication, enforceReplicationFactor);
  TRI_ASSERT(usableCollectionPointers.size() == cols.size());
  return usableCollectionPointers;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Persist collection in Agency and trigger shard creation process
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalCollection>> ClusterMethods::persistCollectionsInAgency(
    std::vector<std::shared_ptr<LogicalCollection>>& collections,
    bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
    bool enforceReplicationFactor) {
  TRI_ASSERT(!collections.empty());
  if (collections.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Trying to create an empty list of collections on coordinator.");
  }
  // We have at least one, take this collections DB name
  auto& dbName = collections[0]->vocbase().name();
  ClusterInfo* ci = ClusterInfo::instance();
  ci->loadCurrentDBServers();
  std::vector<std::string> dbServers = ci->getCurrentDBServers();
  std::vector<ClusterCollectionCreationInfo> infos;
  std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> vpackData;
  infos.reserve(collections.size());
  vpackData.reserve(collections.size());
  for (auto& col : collections) {
    // We can only serve on Database at a time with this call.
    // We have the vocbase context around this calls anyways, so this is save.
    TRI_ASSERT(col->vocbase().name() == dbName);
    std::string distributeShardsLike = col->distributeShardsLike();
    std::vector<std::string> avoid = col->avoidServers();
    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> shards = nullptr;

    if (!distributeShardsLike.empty()) {
      CollectionNameResolver resolver(col->vocbase());
      TRI_voc_cid_t otherCid = resolver.getCollectionIdCluster(distributeShardsLike);

      if (otherCid != 0) {
        shards = CloneShardDistribution(ci, col.get(), otherCid);
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
                                       "Could not find collection " + distributeShardsLike +
                                           " to distribute shards like it.");
      }
    } else {
      // system collections should never enforce replicationfactor
      // to allow them to come up with 1 dbserver
      if (col->system()) {
        enforceReplicationFactor = false;
      }

      size_t replicationFactor = col->replicationFactor();
      size_t numberOfShards = col->numberOfShards();

      // the default behavior however is to bail out and inform the user
      // that the requested replicationFactor is not possible right now
      if (dbServers.size() < replicationFactor) {
        LOG_TOPIC("9ce2e", DEBUG, Logger::CLUSTER)
            << "Do not have enough DBServers for requested replicationFactor,"
            << " nrDBServers: " << dbServers.size()
            << " replicationFactor: " << replicationFactor;
        if (enforceReplicationFactor) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
        }
      }

      if (!avoid.empty()) {
        // We need to remove all servers that are in the avoid list
        if (dbServers.size() - avoid.size() < replicationFactor) {
          LOG_TOPIC("03682", DEBUG, Logger::CLUSTER)
              << "Do not have enough DBServers for requested replicationFactor,"
              << " (after considering avoid list),"
              << " nrDBServers: " << dbServers.size() << " replicationFactor: " << replicationFactor
              << " avoid list size: " << avoid.size();
          // Not enough DBServers left
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
        }
        dbServers.erase(std::remove_if(dbServers.begin(), dbServers.end(),
                                       [&](const std::string& x) {
                                         return std::find(avoid.begin(), avoid.end(),
                                                          x) != avoid.end();
                                       }),
                        dbServers.end());
      }
      std::random_shuffle(dbServers.begin(), dbServers.end());
      shards = DistributeShardsEvenly(ci, numberOfShards, replicationFactor,
                                      dbServers, !col->system());
    }

    if (shards->empty() && !col->isSmart()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "no database servers found in cluster");
    }

    col->setShardMap(shards);

    std::unordered_set<std::string> const ignoreKeys{
        "allowUserKeys", "cid",     "globallyUniqueId", "count",
        "planId",        "version", "objectId"};
    col->setStatus(TRI_VOC_COL_STATUS_LOADED);
    VPackBuilder velocy =
        col->toVelocyPackIgnore(ignoreKeys, LogicalDataSource::makeFlags());

    infos.emplace_back(
        ClusterCollectionCreationInfo{std::to_string(col->id()),
                                      col->numberOfShards(), col->replicationFactor(),
                                      waitForSyncReplication, velocy.slice()});
    vpackData.emplace_back(velocy.steal());
  }

  Result res = ci->createCollectionsCoordinator(dbName, infos, 240.0);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  ci->loadPlan();

  // Produce list of shared_ptr wrappers
  std::vector<std::shared_ptr<LogicalCollection>> usableCollectionPointers;
  usableCollectionPointers.reserve(infos.size());
  for (auto const& i : infos) {
    auto c = ci->getCollection(dbName, i.collectionID);
    TRI_ASSERT(c.get() != nullptr);
    // We never get a nullptr here because an exception is thrown if the
    // collection does not exist. Also, the create collection should have
    // failed before.
    usableCollectionPointers.emplace_back(std::move(c));
  }
  return usableCollectionPointers;
}

/// @brief fetch edges from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        All non-empty and non-cached results
///        of DBServers will be inserted in the
///        datalake. Slices used in the result
///        point to content inside of this lake
///        only and do not run out of scope unless
///        the lake is cleared.

int fetchEdgesFromEngines(std::string const& dbname,
                          std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
                          VPackSlice const vertexId, bool backward,
                          std::unordered_map<arangodb::velocypack::StringRef, VPackSlice>& cache,
                          std::vector<VPackSlice>& result,
                          std::vector<std::shared_ptr<VPackBuilder>>& datalake,
                          VPackBuilder& builder, size_t& read) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  // TODO map id => ServerID if possible
  // And go fast-path

  // This function works for one specific vertex
  // or for a list of vertices.
  TRI_ASSERT(vertexId.isString() || vertexId.isArray());
  builder.clear();
  builder.openObject();
  builder.add("backward", VPackValue(backward));
  builder.add("keys", vertexId);
  builder.close();

  std::string const url =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_internal/traverser/edge/";

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& engine : *engines) {
    requests.emplace_back("server:" + engine.first, RequestType::PUT,
                          url + StringUtils::itoa(engine.second), body);
  }

  // Perform the requests
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION, false);

  result.clear();
  // Now listen to the results:
  for (auto const& req : requests) {
    bool allCached = true;
    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      // oh-oh cluster is in a bad state
      return commError;
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }
    read +=
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(resSlice,
                                                                    "readIndex", 0);
    VPackSlice edges = resSlice.get("edges");
    for (auto const& e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("da49d", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      arangodb::velocypack::StringRef idRef(id);
      auto resE = cache.insert({idRef, e});
      if (resE.second) {
        // This edge is not yet cached.
        allCached = false;
        result.emplace_back(e);
      } else {
        result.emplace_back(resE.first->second);
      }
    }
    if (!allCached) {
      datalake.emplace_back(resBody);
    }
  }
  return TRI_ERROR_NO_ERROR;
}

std::string const apiStr("/_admin/backup/");

arangodb::Result hotBackupList(
  std::vector<ServerID> const& dbServers, VPackSlice const payload,
  std::unordered_map<std::string, BackupMeta>& hotBackups, VPackBuilder& plan) {

  hotBackups.clear();

  std::map<std::string, std::vector<BackupMeta>> dbsBackups;

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // shutdown, leave here
    return TRI_ERROR_SHUTTING_DOWN;
  }

  auto body = std::make_shared<std::string>(payload.toJson());
  std::string const url = apiStr + "list";

  std::vector<ClusterCommRequest> requests;
  for (auto const& dbServer : dbServers) {
    requests.emplace_back(
      "server:" + dbServer, RequestType::POST, url, body);
  }

  // Perform the requests
  cc->performRequests(
    requests, CL_DEFAULT_TIMEOUT, Logger::BACKUP, false, false);

  LOG_TOPIC("410a1", DEBUG, Logger::BACKUP) << "Getting list of local backups";

  // Now listen to the results:
  for (auto const& req : requests) {

    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return arangodb::Result(
        commError,
        std::string("Communication error while getting list of backups from ")
        + req.destination);
    }

    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        std::string("result to list request to ") + req.destination + " not an object");
    }

    if (resSlice.get("error").getBoolean()) {
      return arangodb::Result(resSlice.get("errorNum").getNumber<uint64_t>(),
                              resSlice.get("errorMessage").copyString());
    }

    if (!resSlice.hasKey("result") || !resSlice.get("result").isObject()) {
      return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string("invalid response ") + resSlice.toJson() + "from " + req.destination);
    }

    resSlice = resSlice.get("result");

    if (!resSlice.hasKey("list") || !resSlice.get("list").isObject()) {
      return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,  "result is missing backup list");
    }

    if (!payload.isNone() && plan.slice().isNone()) {
      if (!resSlice.hasKey("agency-dump") || !resSlice.get("agency-dump").isArray() ||
          resSlice.get("agency-dump").length() != 1) {
        return arangodb::Result(
          TRI_ERROR_HTTP_NOT_FOUND,
          std::string("result ") + resSlice.toJson() +  " is missing agency dump");
      }
      plan.add(resSlice.get("agency-dump")[0]);
    }

    for (auto const& backup : VPackObjectIterator(resSlice.get("list"))) {
      ResultT<BackupMeta> meta = BackupMeta::fromSlice(backup.value);
      if (meta.ok()) {
        dbsBackups[backup.key.copyString()].push_back(std::move(meta.get()));
      }
    }
  }

  //LOG_TOPIC("4ccd8", DEBUG, Logger::BACKUP) << "found: " << dbsBackups;

  for (auto& i : dbsBackups) {
    // check if the backup is on all dbservers
    if (i.second.size() == dbServers.size()) {

      bool valid = true;

      // check here that the backups are all made with the same version
      std::string version;

      for (BackupMeta const& meta : i.second) {
        if (version.empty()) {
          version = meta._version;
        } else {
          if (version != meta._version) {
            LOG_TOPIC("aaaaa", WARN, Logger::BACKUP) << "Backup " << meta._id << " has different versions accross dbservers: " << version << " and " << meta._version;
            valid = false;
            break ;
          }
        }
      }

      if (valid) {
        BackupMeta & front = i.second.front();
        hotBackups.insert(std::make_pair(front._id, front));
      }
    }
  }

  return arangodb::Result();
}

/**
 * @brief Match existing servers with those in the backup
 *
 * @param  agencyDump
 * @param  my own DB server list
 * @param  Result container
 */
arangodb::Result matchBackupServers(VPackSlice const agencyDump,
                                    std::vector<ServerID> const& dbServers,
                                    std::map<ServerID,ServerID>& match) {

  std::vector<std::string> ap {"arango", "Plan", "DBServers"};

  if (!agencyDump.hasKey(ap)) {
    return Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL, "agency dump must contain key DBServers");
  }
  auto planServers = agencyDump.get(ap);

  return matchBackupServersSlice(planServers, dbServers, match);
}

arangodb::Result matchBackupServersSlice(VPackSlice const planServers,
                                    std::vector<ServerID> const& dbServers,
                                    std::map<ServerID,ServerID>& match) {
  //LOG_TOPIC("711d8", DEBUG, Logger::BACKUP) << "matching db servers between snapshot: " <<
  //  planServers.toJson() << " and this cluster's db servers " << dbServers;

  if (!planServers.isObject()) {
    return Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL, "agency dump's arango.Plan.DBServers must be object");
  }

  if (dbServers.size() < planServers.length()) {
    return Result(
      TRI_ERROR_BACKUP_TOPOLOGY,
      std::string("number of db servers in the backup (")
      + std::to_string(planServers.length()) +
      ") and in this cluster (" + std::to_string(dbServers.size()) + ") do not match");
  }

  // Clear match container
  match.clear();

  // Local copy of my servers
  std::unordered_set<std::string> localCopy;
  std::copy(dbServers.begin(), dbServers.end(),
            std::inserter(localCopy, localCopy.end()));

  // Skip all direct matching names in pair and remove them from localCopy
  std::unordered_set<std::string>::iterator it;
  for (auto const& planned : VPackObjectIterator(planServers)) {
    auto const plannedStr = planned.key.copyString();
    if ((it = std::find(localCopy.begin(), localCopy.end(), plannedStr)) != localCopy.end()) {
      localCopy.erase(it);
    } else {
      match.emplace(plannedStr, std::string());
    }
  }
  // match all remaining
  auto it2 = localCopy.begin();
  for (auto& m : match) {
    m.second = *it2++;
  }

  LOG_TOPIC("a201e", DEBUG, Logger::BACKUP) << "DB server matches: " << match;

  return arangodb::Result();

}

arangodb::Result controlMaintenanceFeature(
  std::string const& command, std::string const& backupId,
  std::vector<ServerID> const& dbServers) {

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN, "Shutting down");
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add("execute", VPackValue(command));
    builder.add("reason", VPackValue("backup"));
    builder.add("duration", VPackValue(30));
    builder.add("id", VPackValue(backupId));
  }

  std::vector<ClusterCommRequest> requests;
  std::string const url = "/_admin/actions";
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& dbServer : dbServers) {
    requests.emplace_back("server:" + dbServer, RequestType::POST, url, body);
  }

  LOG_TOPIC("3d080", DEBUG, Logger::BACKUP)
    << "Attempting to execute " << command << " maintenance features for hot backup id "
    << backupId << " using " << *body;

  // Perform the requests
  cc->performRequests(
    requests, CL_DEFAULT_TIMEOUT, Logger::BACKUP, false, false);

  // Now listen to the results:
  for (auto const& req : requests) {

    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return arangodb::Result(
        commError,
        std::string("Communication error while executing " + command + " maintenance on ")
        + req.destination);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject() || !resSlice.hasKey("error") || !resSlice.get("error").isBoolean()) {
      // Response has invalid format
      return arangodb::Result(
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        std::string("result of executing " + command + " request to maintenance feature on ")
        + req.destination + " is invalid");
    }

    if (resSlice.get("error").getBoolean()) {
      return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string("failed to execute " + command + " on maintenance feature for ")
        + backupId + " on server " + req.destination);
    }

    LOG_TOPIC("d7e7c", DEBUG, Logger::BACKUP) << "maintenance is paused on " << req.destination;

  }

  return arangodb::Result();

}



arangodb::Result restoreOnDBServers(
  std::string const& backupId, std::vector<std::string> const& dbServers, std::string& previous, bool ignoreVersion) {

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN, "Shutting down");
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder o(&builder);
    builder.add("id", VPackValue(backupId));
    builder.add("ignoreVersion", VPackValue(ignoreVersion));
  }
  auto body = std::make_shared<std::string>(builder.toJson());

  std::string const url = apiStr + "restore";
  std::vector<ClusterCommRequest> requests;

  for (auto const& dbServer : dbServers) {
    requests.emplace_back("server:" + dbServer, RequestType::POST, url, body);
  }

  // Perform the requests
  cc->performRequests(
    requests, CL_DEFAULT_TIMEOUT, Logger::BACKUP, false, false);

  LOG_TOPIC("37960", DEBUG, Logger::BACKUP) << "Restoring backup " << backupId;

  // Now listen to the results:
  for (auto const& req : requests) {

    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      // oh-oh cluster is in a bad state
      return arangodb::Result(
        commError, std::string("Communication error list backups on ") + req.destination);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        std::string("result to restore request ") + req.destination + "not an object");
    }

    if (!resSlice.hasKey("error") || !resSlice.get("error").isBoolean() ||
        resSlice.get("error").getBoolean()) {
      return arangodb::Result(
        TRI_ERROR_HOT_RESTORE_INTERNAL,
        std::string("failed to restore ") + backupId + " on server " + req.destination
        + ": " + resSlice.toJson());
    }

    if (!resSlice.hasKey("result") || !resSlice.get("result").isObject()) {
      return arangodb::Result(
        TRI_ERROR_HOT_RESTORE_INTERNAL,
        std::string("failed to restore ") + backupId + " on server " + req.destination
        + " as response is missing result object: " + resSlice.toJson());
    }

    auto result = resSlice.get("result");

    if (!result.hasKey("previous") || !result.get("previous").isString()) {
      return arangodb::Result(
        TRI_ERROR_HOT_RESTORE_INTERNAL,
        std::string("failed to restore ") + backupId + " on server " + req.destination);
    }

    previous = result.get("previous").copyString();
    LOG_TOPIC("9a5c4", DEBUG, Logger::BACKUP)
      << "received failsafe name " << previous << " from db server " << req.destination;
  }

  LOG_TOPIC("755a2", DEBUG, Logger::BACKUP) << "Restored " << backupId << " successfully";

  return arangodb::Result();

}


arangodb::Result applyDBServerMatchesToPlan(
  VPackSlice const plan, std::map<ServerID,ServerID> const& matches,
  VPackBuilder& newPlan) {


  std::function<void(
    VPackSlice const, std::map<ServerID,ServerID> const&)> replaceDBServer;

  replaceDBServer = [&newPlan, &replaceDBServer](
    VPackSlice const s, std::map<ServerID,ServerID> const& matches) {
    if (s.isObject()) {
      VPackObjectBuilder o(&newPlan);
      for (auto const& it : VPackObjectIterator(s)) {
        newPlan.add(it.key);
        replaceDBServer(it.value, matches);
      }
    } else if (s.isArray()) {
      VPackArrayBuilder a(&newPlan);
      for (auto const& it : VPackArrayIterator(s)) {
        replaceDBServer(it, matches);
      }
    } else {
      bool swapped = false;
      if (s.isString()) {
        for (auto const& match : matches) {
          if (s.isString() && s.isEqualString(match.first)) {
            newPlan.add(VPackValue(match.second));
            swapped = true;
            break;
          }
        }
      }
      if (!swapped) {
        newPlan.add(s);
      }
    }

  };

  replaceDBServer(plan, matches);

  return arangodb::Result();
}


arangodb::Result hotRestoreCoordinator(VPackSlice const payload, VPackBuilder& report) {

  // 1. Find local backup with id
  //    - fail if not found
  // 2. Match db servers
  //    - fail if not matching
  // 3. Check if they have according backup with backupId
  //    - fail if not
  // 4. Stop maintenance feature on all db servers
  // 5. a. Replay agency
  //    b. Initiate DB server restores
  // 6. Wait until all dbservers up again and good
  //    - fail if not

  if (!payload.isObject() || !payload.hasKey("id") || !payload.get("id").isString()) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER,
      "restore payload must be an object with string attribute 'id'");
  }

  bool ignoreVersion = payload.hasKey("ignoreVersion") && payload.get("ignoreVersion").isTrue();

  std::string const backupId = payload.get("id").copyString();
  VPackBuilder plan;
  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<ServerID> dbServers = ci->getCurrentDBServers();
  std::unordered_map<std::string, BackupMeta> list;

  auto result = hotBackupList(dbServers, payload, list, plan);
  if (!result.ok()) {
    LOG_TOPIC("ed4dd", ERR, Logger::BACKUP)
      << "failed to find backup " << backupId << " on all db servers: "
      << result.errorMessage();
      return result;
  }
  if (plan.slice().isNone()) {
    LOG_TOPIC("54b9a", ERR, Logger::BACKUP)
      << "failed to find agency dump for " << backupId << " on any db server: "
      << result.errorMessage();
    return result;
  }

  // Check if the version matches the current version
  if (!ignoreVersion) {
    TRI_ASSERT(list.size() == 1);
    using arangodb::methods::Version;
    using arangodb::methods::VersionResult;
    BackupMeta &meta = list.begin()->second;
    if (!RocksDBHotBackup::versionTestRestore(meta._version)) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Version mismatch");
    }
  }

  // Match my db servers to those in the backups's agency dump
  std::map<ServerID, ServerID> matches;
  result = matchBackupServers(plan.slice(), dbServers, matches);
  if (!result.ok()) {
    LOG_TOPIC("5a746", ERR, Logger::BACKUP) << "failed to match db servers: " <<
      result.errorMessage();
    return result;
  }

  // Apply matched servers to create new plan, if any matches to be done,
  // else just take
  VPackBuilder newPlan;
  if (!matches.empty()) {
    result = applyDBServerMatchesToPlan(plan.slice(), matches, newPlan);
    if (!result.ok()) {
      return result;
    }
  }

  // Pause maintenance feature everywhere, fail, if not succeeded everywhere
  result = controlMaintenanceFeature("pause", backupId, dbServers);
  if (!result.ok()) {
    return result;
  }

  // Enact new plan upon the agency
  result = (matches.empty()) ?
    ci->agencyReplan(plan.slice()): ci->agencyReplan(newPlan.slice());
  if (!result.ok()) {
    result = controlMaintenanceFeature("proceed", backupId, dbServers);
    return result;
  }

  // Now I will have to wait for the plan to trickle down
  std::this_thread::sleep_for(std::chrono::seconds(5));

  // We keep the currently registered timestamps in Current/ServersRegistered,
  // such that we can wait until all have reregistered and are up:
  ci->loadServers();
  std::unordered_map<std::string, std::string> serverTimestamps
    = ci->getServerTimestamps();

  // Restore all db servers
  std::string previous;
  result = restoreOnDBServers(backupId, dbServers, previous, ignoreVersion);
  if (!result.ok()) {  // This is disaster!
    return result;
  }

  auto startTime = std::chrono::steady_clock::now();
  while (true) {   // will be left by a timeout
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (application_features::ApplicationServer::isStopping()) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Shutdown of coordinator!");
    }
    if (std::chrono::steady_clock::now() - startTime >
        std::chrono::minutes(15)) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Not all DBservers came back in time!");
    }
    ci->loadServers();
    std::unordered_map<std::string, std::string> newServerTimestamps
      = ci->getServerTimestamps();
    // Check timestamps of all dbservers:
    size_t good = 0;   // Count restarted servers
    for (auto const& dbs : dbServers) {
      if (serverTimestamps[dbs] != newServerTimestamps[dbs]) {
        ++good;
      }
    }
    LOG_TOPIC("8dc7e", INFO, Logger::BACKUP) << "Backup restore: So far "
      << good << "/" << dbServers.size() << " dbServers have reregistered.";
    if (good >= dbServers.size()) {
      break;
    }
  }

  {
    VPackObjectBuilder o(&report);
    report.add("previous", VPackValue(previous));
    report.add("isCluster", VPackValue(true));
  }
  return arangodb::Result();

}


std::vector<std::string> lockPath = std::vector<std::string>{"result","lockId"};

arangodb::Result lockDBServerTransactions(
  std::string const& backupId, std::vector<ServerID> const& dbServers,
  double const& lockWait, std::vector<ServerID>& lockedServers) {

  using namespace std::chrono;

  // Make sure all db servers have the backup with backup Id
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN, "Shutting down");
  }

  std::string const url = apiStr + "lock";

  std::unordered_map<std::string, std::string> headers;

  VPackBuilder lock;
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
    lock.add("timeout", VPackValue(lockWait));
  }

  LOG_TOPIC("707ed", DEBUG, Logger::BACKUP)
    << "Trying to acquire global transaction locks using body " << lock.toJson();

  auto body = std::make_shared<std::string const>(lock.toJson());
  std::vector<ClusterCommRequest> requests;
  for (auto const& dbServer : dbServers) {
    requests.emplace_back("server:" + dbServer, RequestType::POST, url, body);
  }

  // Perform the requests
  cc->performRequests(
    requests, lockWait+1.0, Logger::BACKUP, false, false);

  // Now listen to the results:
  for (auto const& req : requests) {

    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return arangodb::Result(
        TRI_ERROR_LOCAL_LOCK_FAILED,
        std::string("Communication error locking transactions on ") + req.destination);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice slc = resBody->slice();

    if (!slc.isObject() ||
        !slc.hasKey("error") || !slc.get("error").isBoolean()) {
      return arangodb::Result(
        TRI_ERROR_LOCAL_LOCK_FAILED,
        std::string("invalid response from ") + req.destination
        + " when trying to freeze transactions for hot backup " + backupId + ": "
        + slc.toJson());
    }

    if (slc.get("error").getBoolean()) {
      LOG_TOPIC("d7a8a", DEBUG, Logger::BACKUP)
        << "failed to acquire lock from " << req.destination << ": " << slc.toJson();
      auto errorNum = slc.get("errorNum").getNumber<int>();
      if (errorNum == TRI_ERROR_LOCK_TIMEOUT) {
        return arangodb::Result(errorNum, slc.get("errorMessage").copyString());
      }
      return arangodb::Result(
        TRI_ERROR_LOCAL_LOCK_FAILED,
        std::string("lock was denied from ") + req.destination
        + " when trying to check for lockId for hot backup " + backupId + ": "
        + slc.toJson());
    }

    if (!slc.hasKey(lockPath) || !slc.get(lockPath).isNumber() ||
        !slc.hasKey("result") || !slc.get("result").isObject()) {
      return arangodb::Result(
        TRI_ERROR_LOCAL_LOCK_FAILED,
        std::string("invalid response from ") + req.destination
        + " when trying to check for lockId for hot backup " + backupId + ": "
        + slc.toJson());
    }

    uint64_t lockId = 0;
    try {
      lockId = slc.get(lockPath).getNumber<uint64_t>();
      LOG_TOPIC("14457", DEBUG, Logger::BACKUP)
        << "acquired lock from " << req.destination << " for backupId " << backupId
        << " with lockId " << lockId;
    } catch (std::exception const& e) {
      return arangodb::Result(
        TRI_ERROR_LOCAL_LOCK_FAILED,
        std::string("invalid response from ") + req.destination
        + " when trying to get lockId for hot backup " + backupId + ": "
        + slc.toJson());
    }

    lockedServers.push_back(req.destination.substr(strlen("server:"), std::string::npos));

  }

  LOG_TOPIC("c1869", DEBUG, Logger::BACKUP) << "acquired transaction locks on all db servers";

  return arangodb::Result();

}


arangodb::Result unlockDBServerTransactions(
  std::string const& backupId, std::vector<ServerID> const& lockedServers) {

  using namespace std::chrono;

  // Make sure all db servers have the backup with backup Id
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN, "Shutting down");
  }

  std::string const url = apiStr + "unlock";
  VPackBuilder lock;
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
  }

  std::unordered_map<std::string, std::string> headers;
  auto body = std::make_shared<std::string const>(lock.toJson());
  std::vector<ClusterCommRequest> requests;
  for (auto const& dbServer : lockedServers) {
    requests.emplace_back("server:" + dbServer, RequestType::POST, url, body);
  }

  // Perform the requests
  cc->performRequests(
    requests, CL_DEFAULT_TIMEOUT, Logger::BACKUP, false, false);

  LOG_TOPIC("2ba8f", DEBUG, Logger::BACKUP) << "best try to kill all locks on db servers";

  return arangodb::Result();

}

std::vector<std::string> idPath {"result","id"};

arangodb::Result hotBackupDBServers(
  std::string const& backupId, std::string const& timeStamp,
  std::vector<ServerID> dbServers, VPackSlice agencyDump) {

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN, "Shutting down");
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add("label", VPackValue(backupId));
    builder.add("agency-dump", agencyDump);
    builder.add("timestamp", VPackValue(timeStamp));
  }
  auto body = std::make_shared<std::string>(builder.toJson());

  std::string const url = apiStr + "create";
  std::vector<ClusterCommRequest> requests;

  for (auto const& dbServer : dbServers) {
    requests.emplace_back("server:" + dbServer, RequestType::POST, url, body);
  }

  // Perform the requests
  cc->performRequests(
    requests, CL_DEFAULT_TIMEOUT, Logger::BACKUP, false, false);

  LOG_TOPIC("478ef", DEBUG, Logger::BACKUP) << "Inquiring about backup " << backupId;

  // Now listen to the results:
  for (auto const& req : requests) {

    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return arangodb::Result(
        commError, std::string("Communication error list backups on ") + req.destination);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        std::string("result to take snapshot on ") + req.destination + " not an object");
    }

    if (!resSlice.hasKey(idPath) || !resSlice.get(idPath).isString()) {
      LOG_TOPIC("6240a", ERR, Logger::BACKUP)
        << "DB server " << req.destination << "is missing backup " << backupId;
      return arangodb::Result(
        TRI_ERROR_FILE_NOT_FOUND,
        std::string("no backup with id ") + backupId + " on server " + req.destination);
    }

    LOG_TOPIC("b370d", DEBUG, Logger::BACKUP)
      << req.destination << " created local backup " << resSlice.get(idPath).copyString();

  }

  LOG_TOPIC("5c5e9", DEBUG, Logger::BACKUP) << "Have created backup " << backupId;

  return arangodb::Result();

}


/**
 * @brief delete all backups with backupId from the db servers
 */
arangodb::Result removeLocalBackups(
  std::string const& backupId, std::vector<ServerID> const& dbServers,
  std::vector<std::string>& deleted) {

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN, "Shutting down");
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add("id", VPackValue(backupId));
  }
  auto body = std::make_shared<std::string>(builder.toJson());

  std::string const url = apiStr + "delete";
  std::vector<ClusterCommRequest> requests;

  for (auto const& dbServer : dbServers) {
    requests.emplace_back("server:" + dbServer, RequestType::POST, url, body);
  }

  // Perform the requests
  cc->performRequests(
    requests, CL_DEFAULT_TIMEOUT, Logger::BACKUP, false, false);

  LOG_TOPIC("33e85", DEBUG, Logger::BACKUP) << "Deleting backup " << backupId;

  size_t notFoundCount = 0;

  // Now listen to the results:
  for (auto const& req : requests) {

    auto res = req.result;
    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return arangodb::Result(
        commError, std::string("Communication error while deleting backup")
        + backupId + " on " + req.destination);
    }
    TRI_ASSERT(res.answer != nullptr);
    auto resBody = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();
    VPackSlice resSlice = resBody->slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        std::string("failed to remove backup from ") + req.destination + ", result not an object");
    }

    if (!resSlice.hasKey("error") || !resSlice.get("error").isBoolean() ||
        resSlice.get("error").getBoolean()) {

      int64_t errorNum = resSlice.get("errorNum").getNumber<int64_t>();

      if (errorNum == TRI_ERROR_FILE_NOT_FOUND) {
        notFoundCount += 1;
        continue;
      }

      std::string errorMsg = std::string("failed to delete backup ") + backupId + " on " + req.destination
        + ":" + resSlice.get("errorMessage").copyString() + " (" + std::to_string(errorNum) + ")";

      LOG_TOPIC("9b94f", ERR, Logger::BACKUP) << errorMsg;
      return arangodb::Result(errorNum, errorMsg);
    }
  }

  LOG_TOPIC("1b318", DEBUG, Logger::BACKUP) << "removeLocalBackups: notFoundCount = " << notFoundCount << " " << dbServers.size();

  if (notFoundCount == dbServers.size()) {
    return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND, "Backup " + backupId + " not found.");
  }

  deleted.emplace_back(backupId);
  LOG_TOPIC("04e97", DEBUG, Logger::BACKUP) << "Have located and deleted " << backupId;

  return arangodb::Result();

}

std::vector<std::string> const versionPath = std::vector<std::string>{"arango", "Plan", "Version"};

arangodb::Result hotBackupCoordinator(VPackSlice const payload, VPackBuilder& report) {

  // ToDo: mode
  //HotBackupMode const mode = CONSISTENT;

  /*
    Suggestion for procedure for cluster hotbackup:
    1. Check that ToDo and Pending are empty, if not, delay, back to 1.
       Timeout for giving up.
    2. Stop Supervision, remember if it was on or not.
    3. Check that ToDo and Pending are empty, if not, start Supervision again,
       back to 1.
       4. Get Plan, will have no resigned leaders.
    5. Stop Transactions, if this does not work in time, restore Supervision
       and give up.
    6. Take hotbackups everywhere, if any fails, all failed.
    7. Resume Transactions.
    8. Resume Supervision if it was on.
    9. Keep Maintenance on dbservers on all the time.
  */

  try {

    if (!payload.isNone() &&
        (!payload.isObject() ||
         (payload.hasKey("label") && !payload.get("label").isString()) ||
         (payload.hasKey("timeout") && !payload.get("timeout").isNumber()))) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, BAD_PARAMS_CREATE);
    }

    std::string const backupId =
      (payload.isObject() && payload.hasKey("label")) ?
      payload.get("label").copyString() : to_string(boost::uuids::random_generator()());
    std::string timeStamp = timepointToString(std::chrono::system_clock::now());

    double timeout = (payload.isObject() && payload.hasKey("timeout")) ?
      payload.get("timeout").getNumber<double>() : 120.;

    using namespace std::chrono;
    auto end = steady_clock::now() + milliseconds(static_cast<uint64_t>(1000*timeout));
    ClusterInfo* ci = ClusterInfo::instance();

    // Go to backup mode for *timeout* if and only if not already in
    // backup mode. Otherwise we cannot know, why backup mode was activated
    // We specifically want to make sure that no other backup is going on.
    bool supervisionOff = false;
    auto result = ci->agencyHotBackupLock(backupId, timeout, supervisionOff);
    if (!result.ok()) {
      // Failed to go to backup mode
      result.reset(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string("agency lock operation resulted in ") + result.errorMessage());
      LOG_TOPIC("6c73d", ERR, Logger::BACKUP) << result.errorMessage();
      return result;
    }

    if (end < steady_clock::now()) {
      LOG_TOPIC("352d6", INFO, Logger::BACKUP)
        << "hot backup didn't get to locking phase within " << timeout
        << "s.";
      auto hlRes = ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      return arangodb::Result(
        TRI_ERROR_CLUSTER_TIMEOUT, "hot backup timeout before locking phase");
    }

    // acquire agency dump
    auto agency = std::make_shared<VPackBuilder>();
    result = ci->agencyPlan(agency);
    if (!result.ok()) {
      ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      result.reset(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string ("failed to acquire agency dump: ") + result.errorMessage());
      LOG_TOPIC("c014d", ERR, Logger::BACKUP) << result.errorMessage();
      return result;
    }

    // Call lock on all database servers
    auto cc = ClusterComm::instance();
    if (cc == nullptr) {
      // nullptr happens only during controlled shutdown
      return Result(TRI_ERROR_SHUTTING_DOWN, "server is shutting down");
    }
    std::vector<ServerID> dbServers = ci->getCurrentDBServers();
    std::vector<ServerID> lockedServers;
    double lockWait = 2.0;
    while(cc != nullptr && steady_clock::now() < end) {
      auto iterEnd = steady_clock::now() + duration<double>(lockWait);

      result = lockDBServerTransactions(backupId, dbServers, lockWait, lockedServers);
      if (!result.ok()) {
        unlockDBServerTransactions(backupId, lockedServers);
        if (result.is(TRI_ERROR_LOCAL_LOCK_FAILED)) { // Unrecoverable
          ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);
          return result;
        }
      } else {
        break;
      }
      if (lockWait < 30.0) {
        lockWait *= 1.1;
      }
      double tmp = duration<double>(iterEnd - steady_clock::now()).count();
      if (tmp > 0) {
        std::this_thread::sleep_for(duration<double>(tmp));
      }
    }

    if (!result.ok()) {
      unlockDBServerTransactions(backupId, dbServers);
      ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      result.reset(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string ("failed to acquire global transaction lock on all db servers: ") + result.errorMessage());
      LOG_TOPIC("b7d09", ERR, Logger::BACKUP) << result.errorMessage();
      return result;
    }

    result = hotBackupDBServers(backupId, timeStamp, dbServers, agency->slice());
    if (!result.ok()) {
      unlockDBServerTransactions(backupId, dbServers);
      ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      result.reset(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string ("failed to hot backup on all db servers: ") + result.errorMessage());
      LOG_TOPIC("6b333", ERR, Logger::BACKUP) << result.errorMessage();
      std::vector<std::string> dummy;
      removeLocalBackups(backupId, dbServers, dummy);
      return result;
    }

    unlockDBServerTransactions(backupId, dbServers);
    ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);

    auto agencyCheck = std::make_shared<VPackBuilder>();
    result = ci->agencyPlan(agencyCheck);
    if (!result.ok()) {
      ci->agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      result.reset(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string ("failed to acquire agency dump post backup: ") + result.errorMessage()
        + " backup's consistency is not guaranteed" );
      LOG_TOPIC("d4229", ERR, Logger::BACKUP) << result.errorMessage();
      return result;
    }

    try {
      if (!basics::VelocyPackHelper::equal(agency->slice()[0].get(versionPath), agencyCheck->slice()[0].get(versionPath), false)) {
        result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          "data definition of cluster was changed during hot backup: backup's consistency is not guaranteed");
        LOG_TOPIC("0ad21", ERR, Logger::BACKUP) << result.errorMessage();
        return result;
      }
    } catch (std::exception const& e) {
      result.reset(
        TRI_ERROR_HOT_BACKUP_INTERNAL, std::string("invalid agency state: ") + e.what());
      LOG_TOPIC("037eb", ERR, Logger::BACKUP) << result.errorMessage();
      return result;
    }

    std::replace(timeStamp.begin(), timeStamp.end(), ':', '.');
    {
      VPackObjectBuilder o(&report);
      report.add("id", VPackValue(timeStamp + "_" + backupId));
    }

    return arangodb::Result();

  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_HOT_BACKUP_INTERNAL,
      std::string("caught exception cretaing cluster backup: ") + e.what());
  }
}


arangodb::Result listHotBackupsOnCoordinator(
  VPackSlice const payload, VPackBuilder& report) {

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return Result(TRI_ERROR_SHUTTING_DOWN, "server is shutting down");
  }

  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<ServerID> dbServers = ci->getCurrentDBServers();

  std::unordered_map<std::string, BackupMeta> list;

  if (!payload.isNone()) {
    if (payload.isObject() && payload.hasKey("id")) {
      if (payload.get("id").isArray()) {
        for (auto const i : VPackArrayIterator(payload.get("id"))) {
          if (!i.isString()) {
            return arangodb::Result(
              TRI_ERROR_HTTP_BAD_PARAMETER,
              "invalid list JSON: all ids must be string.");
          }
        }
      } else {
        if (!payload.get("id").isString()) {
          return arangodb::Result(
            TRI_ERROR_HTTP_BAD_PARAMETER,
            "invalid JSON: id must be string or array of strings.");
        }
     }
    } else {
      return arangodb::Result(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "invalid JSON: body must be empty or object with attribute 'id'.");
    }
  } // allow continuation with None slice

  VPackBuilder dummy;
  arangodb::Result result = hotBackupList(dbServers, payload, list, dummy);

  if (!result.ok()) {
    return result;
  }

  {
    VPackObjectBuilder o(&report);
    report.add(VPackValue("list"));
    {
      VPackObjectBuilder a(&report);
      for (auto const& i : list) {
        report.add(VPackValue(i.first));
        i.second.toVelocyPack(report);
      }
    }
  }

  return arangodb::Result();

}

arangodb::Result deleteHotBackupsOnCoordinator(
  VPackSlice const payload, VPackBuilder& report) {

  std::unordered_map<std::string, BackupMeta> listIds;
  std::vector<std::string> deleted;
  VPackBuilder dummy;
  arangodb::Result result;

  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<ServerID> dbServers = ci->getCurrentDBServers();

  if (!payload.isObject() || !payload.hasKey("id") || !payload.get("id").isString()) {
    return arangodb::Result(TRI_ERROR_HTTP_BAD_PARAMETER, "Expecting object with key `id` set to backup id.");
  }

  std::string id = payload.get("id").copyString();

  result = removeLocalBackups(id, dbServers, deleted);
  if (!result.ok()) {
    return result;
  }

  {
    VPackObjectBuilder o(&report);
    report.add(VPackValue("id"));
    {
      VPackArrayBuilder a(&report);
      for (auto const& i : deleted) {
        report.add(VPackValue(i));
      }
    }
  }

  result.reset();
  return result;

}

}  // namespace arangodb
