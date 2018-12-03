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
////////////////////////////////////////////////////////////////////////////////

#include "ClusterMethods.h"
#include "Basics/conversions.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Graph/Traverser.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Helpers.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>
#include <numeric>
#include <vector>

using namespace arangodb::basics;
using namespace arangodb::rest;

// Timeout for read operations:
static double const CL_DEFAULT_TIMEOUT = 120.0;

// Timeout for write operations, note that these are used for communication
// with a shard leader and we always have to assume that some follower has
// stopped writes for some time to get in sync:
static double const CL_DEFAULT_LONG_TIMEOUT = 900.0;

namespace {
template<typename T>
T addFigures(VPackSlice const& v1, VPackSlice const& v2, std::vector<std::string> const& attr) {
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
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(), { "alive", "count" })));
  updated.add("size", VPackValue(addFigures<size_t>(value, builder->slice(), { "alive", "size" })));
  updated.close();

  updated.add("dead", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(), { "dead", "count" })));
  updated.add("size", VPackValue(addFigures<size_t>(value, builder->slice(), { "dead", "size" })));
  updated.add("deletion", VPackValue(addFigures<size_t>(value, builder->slice(), { "dead", "deletion" })));
  updated.close();

  updated.add("indexes", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(), { "indexes", "count" })));
  updated.add("size", VPackValue(addFigures<size_t>(value, builder->slice(), { "indexes", "size" })));
  updated.close();

  updated.add("datafiles", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(), { "datafiles", "count" })));
  updated.add("fileSize", VPackValue(addFigures<size_t>(value, builder->slice(), { "datafiles", "fileSize" })));
  updated.close();

  updated.add("journals", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(), { "journals", "count" })));
  updated.add("fileSize", VPackValue(addFigures<size_t>(value, builder->slice(), { "journals", "fileSize" })));
  updated.close();

  updated.add("compactors", VPackValue(VPackValueType::Object));
  updated.add("count", VPackValue(addFigures<size_t>(value, builder->slice(), { "compactors", "count" })));
  updated.add("fileSize", VPackValue(addFigures<size_t>(value, builder->slice(), { "compactors", "fileSize" })));
  updated.close();

  updated.add("documentReferences", VPackValue(addFigures<size_t>(value, builder->slice(), { "documentReferences" })));

  updated.close();

  TRI_ASSERT(updated.slice().isObject());
  TRI_ASSERT(updated.isClosed());

  builder.reset(new VPackBuilder(VPackCollection::merge(builder->slice(), updated.slice(), true, false)));
  TRI_ASSERT(builder->slice().isObject());
  TRI_ASSERT(builder->isClosed());
}

static void InjectNoLockHeader(
    arangodb::transaction::Methods const& trx, std::string const& shard,
    std::unordered_map<std::string, std::string>* headers) {
  if (trx.isLockedShard(shard)) {
    headers->emplace(arangodb::StaticStrings::XArangoNoLock, shard);
  }
}

static std::unique_ptr<std::unordered_map<std::string, std::string>>
CreateNoLockHeader(arangodb::transaction::Methods const& trx,
                   std::string const& shard) {
  if (trx.isLockedShard(shard)) {
    auto headers =
        std::make_unique<std::unordered_map<std::string, std::string>>();
    headers->emplace(arangodb::StaticStrings::XArangoNoLock, shard);
    return headers;
  }
  return nullptr;
}
}

namespace arangodb {

static int handleGeneralCommErrors(ClusterCommResult const* res) {
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
/// @brief extracts a numeric value from an hierarchical VelocyPack
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static T ExtractFigure(VPackSlice const& slice, char const* group,
                       char const* name) {
  TRI_ASSERT(slice.isObject());
  VPackSlice g = slice.get(group);

  if (!g.isObject()) {
    return static_cast<T>(0);
  }
  return arangodb::basics::VelocyPackHelper::getNumericValue<T>(g, name, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts answer from response into a VPackBuilder.
///        If there was an error extracting the answer the builder will be
///        empty.
///        No Error can be thrown.
////////////////////////////////////////////////////////////////////////////////

static std::shared_ptr<VPackBuilder> ExtractAnswer(
    ClusterCommResult const& res) {
  auto rv = std::make_shared<VPackBuilder>();
  rv->add(res.answer->payload());
  return rv;
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

static void mergeResults(
    std::vector<std::pair<ShardID, VPackValueLength>> const& reverseMapping,
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> const& resultMap,
    std::shared_ptr<VPackBuilder>& resultBody) {
  resultBody->clear();
  resultBody->openArray();
  for (auto const& pair : reverseMapping) {
    VPackSlice arr = resultMap.find(pair.first)->second->slice();
    if (arr.isObject() && arr.hasKey(StaticStrings::Error) &&
        arr.get(StaticStrings::Error).isBoolean() && arr.get(StaticStrings::Error).getBoolean()) {
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

static void mergeResultsAllShards(
    std::vector<std::shared_ptr<VPackBuilder>> const& results,
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
  for (VPackValueLength currentIndex = 0; currentIndex < expectedResults;
       ++currentIndex) {
    bool foundRes = false;
    for (auto const& it : results) {
      VPackSlice oneRes = it->slice();
      TRI_ASSERT(oneRes.isArray());
      oneRes = oneRes.at(currentIndex);
      if (!oneRes.equals(notFound)) {
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

static int distributeBabyOnShards(
    std::unordered_map<ShardID, std::vector<VPackSlice>>& shardMap,
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

    error = collinfo->getResponsibleShard(temp.slice(), false, shardID, usesDefaultShardingAttributes);
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
      error = collinfo->getResponsibleShard(value, true, shardID, usesDefaultShardingAttributes, _key);
    }
    if (error == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
      return TRI_ERROR_CLUSTER_SHARD_GONE;
    }

    // Now perform the above mentioned check:
    if (userSpecifiedKey &&
        (!usesDefaultShardingAttributes || !collinfo->allowUserKeys()) &&
        !isRestore) {
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
    std::vector<ClusterCommRequest>& requests,
    std::unordered_map<int, size_t>& errorCounter,
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
      resultMap.emplace(res.shardID,
                        res.answer->toVelocyPackBuilderPtrNoUniquenessChecks());
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
    ClusterInfo* ci,
    uint64_t numberOfShards,
    uint64_t replicationFactor,
    std::vector<std::string>& dbServers,
    bool warnAboutReplicationFactor) {

  auto shards = std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();

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
          LOG_TOPIC(WARN, Logger::CLUSTER)
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
        } while (candidate == serverIds[0]); // mop: ignore leader
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

static std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>>
CloneShardDistribution(ClusterInfo* ci, LogicalCollection* col,
                       TRI_voc_cid_t cid) {
  auto result = std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();
  TRI_ASSERT(cid != 0);
  std::string cidString = arangodb::basics::StringUtils::itoa(cid);
  TRI_ASSERT(col);
  auto other = ci->getCollection(col->vocbase().name(), cidString);

  // The function guarantees that no nullptr is returned
  TRI_ASSERT(other != nullptr);

  if (!other->distributeShardsLike().empty()) {
    std::string const errorMessage = "Cannot distribute shards like '" + other->name() + "' it is already distributed like '" + other->distributeShardsLike() + "'.";
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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Inconsistency in shard distribution detected. Is in the process of self-healing. Please retry the operation again after some seconds.");
    }
    result->emplace(shardId, it->second);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::string> getForwardableRequestHeaders(
    arangodb::GeneralRequest* request) {
  std::unordered_map<std::string, std::string> const& headers =
      request->headers();
  std::unordered_map<std::string, std::string>::const_iterator it =
      headers.begin();

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

bool shardKeysChanged(LogicalCollection const& collection,
                      VPackSlice const& oldValue, VPackSlice const& newValue,
                      bool isPatch) {
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

    if (arangodb::basics::VelocyPackHelper::compare(n, o, false) != 0) {
      return true;
    }
  }

  return false;
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
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(
        coordTransactionID, "shard:" + p.first,
        arangodb::rest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/revision",
        std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

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

int warmupOnCoordinator(std::string const& dbname,
                        std::string const& cid) {
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
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(
        coordTransactionID, "shard:" + p.first,
        arangodb::rest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/loadIndexesIntoMemory",
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
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(
        coordTransactionID, "shard:" + p.first,
        arangodb::rest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/figures",
        std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

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

int countOnCoordinator(std::string const& dbname, std::string const& cname,
                       transaction::Methods const& trx,
                       std::vector<std::pair<std::string, uint64_t>>& result) {
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
  collinfo = ci->getCollectionNT(dbname, cname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  auto shards = collinfo->shardIds();
  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>();
  for (auto const& p : *shards) {
    requests.emplace_back(
        "shard:" + p.first, arangodb::rest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/count",
        body, ::CreateNoLockHeader(trx, p.first));
  }
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::QUERIES, true);
  for (auto& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          // add to the total
          result.emplace_back(res.shardID, arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
                  answer, "count", 0));
        } else {
          return TRI_ERROR_INTERNAL;
        }
      } else {
        return static_cast<int>(res.answer_code);
      }
    } else {
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the selectivity estimates from DBservers
////////////////////////////////////////////////////////////////////////////////

int selectivityEstimatesOnCoordinator(
  std::string const& dbname,
  std::string const& collname,
  std::unordered_map<std::string, double>& result) {

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

  auto shards = collinfo->shardIds();
  std::vector<ClusterCommRequest> requests;
  std::string requestsUrl;
  auto body = std::make_shared<std::string>();
  for (auto const& p : *shards) {
    requestsUrl = "/_db/" + StringUtils::urlEncode(dbname) +
                  "/_api/index?collection=" + StringUtils::urlEncode(p.first);
    requests.emplace_back("shard:" + p.first,
                          arangodb::rest::RequestType::GET,
                          requestsUrl, body);
  }

  // format of expected answer:
  // in identifiers is a map that has keys in the format
  // s<shardid>/<indexid> and index information as value

  // {"code":200
  // ,"error":false
  // ,"identifiers":{ "s10004/0"    :{"fields":["_key"]
  //                                 ,"id":"s10004/0"
  //                                 ,"selectivityEstimate":1
  //                                 ,"sparse":false
  //                                 ,"type":"primary"
  //                                 ,"unique":true
  //                                 }
  //                 ,"s10004/10005":{"deduplicate":true
  //                                 ,"fields":["user"]
  //                                 ,"id":"s10004/10005"
  //                                 ,"selectivityEstimate":1
  //                                 ,"sparse":true
  //                                 ,"type":"hash"
  //                                 ,"unique":true
  //                                 }
  //                 }
  // }

  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::QUERIES, true);

  std::map<std::string,std::vector<double>> indexEstimates;

  for (auto& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          // add to the total
          for (auto const& identifier : VPackObjectIterator(answer.get("identifiers"))) {
            if (identifier.value.hasKey("selectivityEstimate")) {
              StringRef shard_index_id(identifier.key);
              auto split_point = std::find(shard_index_id.begin(), shard_index_id.end(), '/');
              std::string index(split_point + 1, shard_index_id.end());

              double estimate = arangodb::basics::VelocyPackHelper::getNumericValue(
                identifier.value, "selectivityEstimate", 0.0
              );
              indexEstimates[index].push_back(estimate);
            }
          }
        } else {
          return TRI_ERROR_INTERNAL;
        }
      } else {
        return static_cast<int>(res.answer_code);
      }
    } else {
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }
  }

  auto aggregate_indexes = [](std::vector<double> vec) -> double {
    TRI_ASSERT(!vec.empty());
    double rv = std::accumulate(vec.begin(),vec.end(), 0.0);
    rv /= static_cast<double>(vec.size());
    return rv;
  };

  for (auto const& p : indexEstimates){
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

Result createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    transaction::Methods const& trx,
    arangodb::OperationOptions const& options, VPackSlice const& slice,
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

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  auto collid = std::to_string(collinfo->id());


  // create vars used in this function
  bool const useMultiple = slice.isArray(); // insert more than one document
  std::unordered_map<ShardID, std::vector<std::pair<VPackSlice, std::string>>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;

  {
    // create shard map
    int res = TRI_ERROR_NO_ERROR;
    if (useMultiple) {
      for (VPackSlice value : VPackArrayIterator(slice)) {
        res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                     reverseMapping, value,
                                     options.isRestore);
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }
    } else {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                   slice, options.isRestore);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document?collection=";

  std::string const optsUrlPart =
      std::string("&waitForSync=") + (options.waitForSync ? "true" : "false") +
                  "&returnNew=" + (options.returnNew ? "true" : "false") +
                  "&returnOld=" + (options.returnOld ? "true" : "false") +
                  "&isRestore=" + (options.isRestore ? "true" : "false") +
                  "&" + StaticStrings::OverWrite + "=" + (options.overwrite ? "true" : "false");

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

    requests.emplace_back(
        "shard:" + it.first, arangodb::rest::RequestType::POST,
        baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body,
        ::CreateNoLockHeader(trx, it.first));
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

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

  collectResultsFromAllShards<std::pair<VPackSlice, std::string>>(
      shardMap, requests, errorCounter, resultMap, responseCode);

  responseCode =
      (options.waitForSync ? rest::ResponseCode::CREATED
                           : rest::ResponseCode::ACCEPTED);
  mergeResults(reverseMapping, resultMap, resultBody);

  // the cluster operation was OK, however,
  // the DBserver could have reported an error.
  return Result{};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    arangodb::transaction::Methods const& trx,
    VPackSlice const slice, arangodb::OperationOptions const& options,
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

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  bool useDefaultSharding = collinfo->usesDefaultShardKeys();
  auto collid = std::to_string(collinfo->id());
  bool useMultiple = slice.isArray();

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";

  std::string const optsUrlPart =
      std::string("?waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnOld=" + (options.returnOld ? "true" : "false") + "&ignoreRevs=" +
      (options.ignoreRevs ? "true" : "false");

  VPackBuilder reqBuilder;

  if (useDefaultSharding) {
    // fastpath we know which server is responsible.

    // decompose the input into correct shards.
    // Send the correct documents to the correct shards
    // Merge the results with static merge helper

    std::unordered_map<ShardID, std::vector<VPackSlice>> shardMap;
    std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
    auto workOnOneNode = [&shardMap, &ci, &collid, &collinfo, &reverseMapping](
        VPackSlice const value) -> int {
      // Sort out the _key attribute and identify the shard responsible for it.

      StringRef _key(transaction::helpers::extractKeyPart(value));
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
        int error = collinfo->getResponsibleShard(
            arangodb::velocypack::Slice::emptyObjectSlice(), true,
            shardID, usesDefaultShardingAttributes, _key.toString());

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

    if (useMultiple) { // slice is array of document values
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

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
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
      requests.emplace_back(
          "shard:" + it.first,
          arangodb::rest::RequestType::DELETE_REQ,
          baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body,
          ::CreateNoLockHeader(trx, it.first));
    }

    // Perform the requests
    size_t nrDone = 0;
    cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

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
    collectResultsFromAllShards<VPackSlice>(
        shardMap, requests, errorCounter, resultMap, responseCode);
    mergeResults(reverseMapping, resultMap, resultBody);
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // slowpath we do not know which server is responsible ask all of them.

  // We simply send the body to all shards and await their results.
  // As soon as we have the results we merge them in the following way:
  // For 1 .. slice.length()
  //    for res : allResults
  //      if res != NOT_FOUND => insert this result. skip other results
  //    end
  //    if (!skipped) => insert NOT_FOUND

  auto body = std::make_shared<std::string>(slice.toJson());
  std::vector<ClusterCommRequest> requests;
  auto shardList = ci->getShardList(collid);
  for (auto const& shard : *shardList) {
    requests.emplace_back(
        "shard:" + shard, arangodb::rest::RequestType::DELETE_REQ,
        baseUrl + StringUtils::urlEncode(shard) + optsUrlPart, body,
        ::CreateNoLockHeader(trx, shard));
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    size_t count;
    int nrok = 0;
    for (count = requests.size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::rest::ResponseCode::NOT_FOUND ||
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
  allResults.reserve(shardList->size());
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
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter,
                        static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

int truncateCollectionOnCoordinator(std::string const& dbname,
                                    std::string const& collname) {
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

  // Some stuff to prepare cluster-intern requests:
  // We have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::unordered_map<std::string, std::string> headers;
  for (auto const& p : *shards) {
    cc->asyncRequest(coordTransactionID, "shard:" + p.first,
                     arangodb::rest::RequestType::PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + p.first + "/truncate",
                     std::shared_ptr<std::string>(), headers, nullptr, 600.0);
  }
  // Now listen to the results:
  unsigned int count;
  unsigned int nrok = 0;
  for (count = (unsigned int)shards->size(); count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        nrok++;
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok < shards->size()) {
    return TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION;
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    arangodb::transaction::Methods const& trx,
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

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo;
  collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  auto collid = std::to_string(collinfo->id());

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
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, value);
      if (res != TRI_ERROR_NO_ERROR) {
        canUseFastPath = false;
        shardMap.clear();
        reverseMapping.clear();
        break;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                 slice);
    if (res != TRI_ERROR_NO_ERROR) {
      canUseFastPath = false;
    }
  }

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

  auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    VPackBuilder reqBuilder;

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        if (!options.ignoreRevs && slice.hasKey(StaticStrings::RevString)) {
          headers->emplace("if-match",
                           slice.get(StaticStrings::RevString).copyString());
        }

        VPackSlice keySlice = slice;
        if (slice.isObject()) {
          keySlice = slice.get(StaticStrings::KeyString);
        }

        ::InjectNoLockHeader(trx, it.first, headers.get());
        // We send to single endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + "/" +
                StringUtils::urlEncode(keySlice.copyString()) +
                optsUrlPart, body,
                std::move(headers));
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& value : it.second) {
          reqBuilder.add(value);
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
        // We send to Babies endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body,
            ::CreateNoLockHeader(trx, it.first));
      }
    }

    // Perform the requests
    size_t nrDone = 0;
    cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

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
    collectResultsFromAllShards<VPackSlice>(
        shardMap, requests, errorCounter, resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  std::vector<ClusterCommRequest> requests;
  auto shardList = ci->getShardList(collid);
  if (!useMultiple) {
    if (!options.ignoreRevs && slice.hasKey(StaticStrings::RevString)) {
      headers->emplace("if-match",
                       slice.get(StaticStrings::RevString).copyString());
    }
    for (auto const& shard : *shardList) {
      VPackSlice keySlice = slice;
      if (slice.isObject()) {
        keySlice = slice.get(StaticStrings::KeyString);
      }
      auto headersCopy =
          std::make_unique<std::unordered_map<std::string, std::string>>(
              *headers);
      ::InjectNoLockHeader(trx, shard, headersCopy.get());
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + "/" +
              StringUtils::urlEncode(keySlice.copyString()) +
              optsUrlPart, nullptr,
              std::move(headersCopy));
    }
  } else {
    auto body = std::make_shared<std::string>(slice.toJson());
    for (auto const& shard : *shardList) {
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + optsUrlPart, body,
          ::CreateNoLockHeader(trx, shard));
    }
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

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
        if (res.answer_code !=
                arangodb::rest::ResponseCode::NOT_FOUND ||
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
  allResults.reserve(shardList->size());
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
  TRI_ASSERT(allResults.size() == shardList->size());
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

int fetchEdgesFromEngines(
    std::string const& dbname,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    VPackSlice const vertexId,
    size_t depth,
    std::unordered_map<StringRef, VPackSlice>& cache,
    std::vector<VPackSlice>& result,
    std::vector<std::shared_ptr<VPackBuilder>>& datalake,
    VPackBuilder& builder,
    size_t& filtered,
    size_t& read) {
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
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, false);

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
    filtered += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
        resSlice, "filtered", 0);
    read += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
        resSlice, "readIndex", 0);
    VPackSlice edges = resSlice.get("edges");
    for (auto const& e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC(ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      StringRef idRef(id);
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
    std::unordered_set<StringRef>& vertexIds,
    std::unordered_map<StringRef, std::shared_ptr<VPackBuffer<uint8_t>>>&
        result,
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
    //TRI_ASSERT(v.isString());
    builder.add(VPackValuePair(v.data(), v.length(), VPackValueType::String));
  }
  builder.close(); // 'keys' Array
  builder.close(); // base object

  std::string const url =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_internal/traverser/vertex/";

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& engine : *engines) {
    requests.emplace_back("server:" + engine.first, RequestType::PUT,
                          url + StringUtils::itoa(engine.second), body);
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, false);

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
      int code = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          resSlice, "errorNum", TRI_ERROR_INTERNAL);
      // We have an error case here. Throw it.
      THROW_ARANGO_EXCEPTION_MESSAGE(
          code, arangodb::basics::VelocyPackHelper::getStringValue(
                    resSlice, StaticStrings::ErrorMessage, TRI_errno_string(code)));
    }
    for (auto const& pair : VPackObjectIterator(resSlice)) {
      StringRef key(pair.key);
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
        LOG_TOPIC(ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      TRI_ASSERT(id.isString());
      result.emplace(StringRef(id), val.steal());
    }
  }

  // Fill everything we did not find with NULL
  for (auto const& v : vertexIds) {
    result.emplace(
        v, VPackBuilder::clone(arangodb::velocypack::Slice::nullSlice())
               .steal());
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
    std::unordered_set<StringRef>& vertexIds,
    std::unordered_map<StringRef, arangodb::velocypack::Slice>& result,
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
    //TRI_ASSERT(v.isString());
    builder.add(VPackValuePair(v.data(), v.length(), VPackValueType::String));
  }
  builder.close(); // 'keys' Array
  builder.close(); // base object

  std::string const url =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_internal/traverser/vertex/";

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(builder.toJson());
  for (auto const& engine : *engines) {
    requests.emplace_back("server:" + engine.first, RequestType::PUT,
                          url + StringUtils::itoa(engine.second), body);
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, false);

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
      int code = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          resSlice, "errorNum", TRI_ERROR_INTERNAL);
      // We have an error case here. Throw it.
      THROW_ARANGO_EXCEPTION_MESSAGE(
          code, arangodb::basics::VelocyPackHelper::getStringValue(
                    resSlice, StaticStrings::ErrorMessage, TRI_errno_string(code)));
    }
    bool cached = false;

    for (auto const& pair : VPackObjectIterator(resSlice)) {
      StringRef key(pair.key);
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

int getFilteredEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    arangodb::transaction::Methods const& trx,
    std::string const& vertex, TRI_edge_direction_e const& direction,
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

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci->getCollection(dbname, collname);

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
  std::string baseUrl = "/_db/" + StringUtils::urlEncode(dbname) + "/_api/edges/";

  auto body = std::make_shared<std::string>();
  for (auto const& p : *shards) {
    requests.emplace_back(
        "shard:" + p.first, arangodb::rest::RequestType::GET,
        baseUrl + StringUtils::urlEncode(p.first) + queryParameters, body,
        ::CreateNoLockHeader(trx, p.first));
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

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
    std::shared_ptr<VPackBuilder> shardResult = res.answer->toVelocyPackBuilderPtrNoUniquenessChecks();

    if (shardResult == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    VPackSlice shardSlice = shardResult->slice();
    if (!shardSlice.isObject()) {
      return TRI_ERROR_INTERNAL;
    }

    bool const isError = arangodb::basics::VelocyPackHelper::getBooleanValue(
        shardSlice, "error", false);

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
      filtered += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
          stats, "filtered", 0);
      scannedIndex +=
          arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
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
    std::string const& dbname, std::string const& collname,
    arangodb::transaction::Methods const& trx,
    VPackSlice const& slice, arangodb::OperationOptions const& options,
    bool isPatch,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
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

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci->getCollection(dbname, collname);
  auto collid = std::to_string(collinfo->id());

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
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, value);
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
  optsUrlPart +=
      std::string("&ignoreRevs=") + (options.ignoreRevs ? "true" : "false") +
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

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.
    std::vector<ClusterCommRequest> requests;
    VPackBuilder reqBuilder;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(slice.toJson());

        auto keySlice = slice.get(StaticStrings::KeyString);
        if (!keySlice.isString()) {
          return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
        }

        StringRef keyStr(keySlice);
        // We send to single endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + "/" +
                StringUtils::urlEncode(keyStr.data(), keyStr.length()) +
                optsUrlPart,
            body, ::CreateNoLockHeader(trx, it.first));
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& value : it.second) {
          reqBuilder.add(value);
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(reqBuilder.slice().toJson());
        // We send to Babies endpoint
        requests.emplace_back(
            "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart, body,
            ::CreateNoLockHeader(trx, it.first));
      }
    }

    // Perform the requests
    size_t nrDone = 0;
    cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

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
    collectResultsFromAllShards<VPackSlice>(
        shardMap, requests, errorCounter, resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>(slice.toJson());
  auto shardList = ci->getShardList(collid);
  if (!useMultiple) {
    std::string key = slice.get(StaticStrings::KeyString).copyString();
    for (auto const& shard : *shardList) {
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + "/" + key + optsUrlPart,
          body);
    }
  } else {
    for (auto const& shard : *shardList) {
      requests.emplace_back(
          "shard:" + shard, reqType,
          baseUrl + StringUtils::urlEncode(shard) + optsUrlPart, body,
          ::CreateNoLockHeader(trx, shard));

    }
  }

  // Perform the requests
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_LONG_TIMEOUT, nrDone, Logger::COMMUNICATION, true);

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    int nrok = 0;
    int commError = TRI_ERROR_NO_ERROR;
    for (size_t count = shardList->size(); count > 0; count--) {
      auto const& req = requests[count - 1];
      auto res = req.result;
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::rest::ResponseCode::NOT_FOUND ||
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
  TRI_ASSERT(allResults.size() == shardList->size());
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
                    (waitForSync ? "true" : "false") + "&waitForCollector=" +
                    (waitForCollector ? "true" : "false");
  if (maxWaitTime >= 0.0) {
    url += "&maxWaitTime=" + std::to_string(maxWaitTime);
  }

  auto body = std::make_shared<std::string const>();
  std::unordered_map<std::string, std::string> headers;
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    // set collection name (shard id)
    cc->asyncRequest(coordTransactionID, "server:" + *it,
                     arangodb::rest::RequestType::PUT, url, body,
                     headers, nullptr, 120.0);
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
    LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
        << "could not flush WAL on all servers. confirmed: " << nrok
        << ", expected: " << DBservers.size();
    return globalErrorCode;
  }

  return TRI_ERROR_NO_ERROR;
}

#ifndef USE_ENTERPRISE
std::shared_ptr<LogicalCollection> ClusterMethods::createCollectionOnCoordinator(
    TRI_col_type_e collectionType,
    TRI_vocbase_t& vocbase,
    velocypack::Slice parameters,
    bool ignoreDistributeShardsLikeErrors,
    bool waitForSyncReplication,
    bool enforceReplicationFactor
) {
  auto col = std::make_unique<LogicalCollection>(vocbase, parameters, true, 0);
    // Collection is a temporary collection object that undergoes sanity checks etc.
    // It is not used anywhere and will be cleaned up after this call.
    // Persist collection will return the real object.
  return persistCollectionInAgency(
    col.get(), ignoreDistributeShardsLikeErrors, waitForSyncReplication,
    enforceReplicationFactor, parameters);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Persist collection in Agency and trigger shard creation process
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LogicalCollection> ClusterMethods::persistCollectionInAgency(
  LogicalCollection* col, bool ignoreDistributeShardsLikeErrors,
  bool waitForSyncReplication, bool enforceReplicationFactor,
  VPackSlice) {

  std::string distributeShardsLike = col->distributeShardsLike();
  std::vector<std::string> avoid = col->avoidServers();
  ClusterInfo* ci = ClusterInfo::instance();
  ci->loadCurrentDBServers();
  std::vector<std::string> dbServers = ci->getCurrentDBServers();
  std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> shards = nullptr;

  if (!distributeShardsLike.empty()) {
    CollectionNameResolver resolver(col->vocbase());
    TRI_voc_cid_t otherCid =
      resolver.getCollectionIdCluster(distributeShardsLike);

    if (otherCid != 0) {
      shards = CloneShardDistribution(ci, col, otherCid);
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
          "Could not find collection " + distributeShardsLike + " to distribute shards like it.");
    }
  } else {
    // system collections should never enforce replicationfactor
    // to allow them to come up with 1 dbserver
    if (col->system()) {
      enforceReplicationFactor = false;
    }

    size_t replicationFactor = col->replicationFactor();
    size_t numberOfShards = col->numberOfShards();

    // the default behaviour however is to bail out and inform the user
    // that the requested replicationFactor is not possible right now
    if (dbServers.size() < replicationFactor) {
      LOG_TOPIC(DEBUG, Logger::CLUSTER)
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
        LOG_TOPIC(DEBUG, Logger::CLUSTER)
          << "Do not have enough DBServers for requested replicationFactor,"
          << " (after considering avoid list),"
          << " nrDBServers: " << dbServers.size()
          << " replicationFactor: " << replicationFactor
          << " avoid list size: " << avoid.size();
        // Not enough DBServers left
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
      }
      dbServers.erase(
        std::remove_if(
          dbServers.begin(), dbServers.end(), [&](const std::string& x) {
            return std::find(avoid.begin(), avoid.end(), x) != avoid.end();
          }), dbServers.end());
    }
    std::random_shuffle(dbServers.begin(), dbServers.end());
    shards = DistributeShardsEvenly(
      ci, numberOfShards, replicationFactor, dbServers, !col->system()
    );
  }

  if (shards->empty() && !col->isSmart()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "no database servers found in cluster");
  }

  col->setShardMap(shards);

  std::unordered_set<std::string> const ignoreKeys{
      "allowUserKeys", "cid", "globallyUniqueId",
      "count", "planId", "version", "objectId"
  };
  col->setStatus(TRI_VOC_COL_STATUS_LOADED);
  VPackBuilder velocy = col->toVelocyPackIgnore(ignoreKeys, false, false);

  auto& dbName = col->vocbase().name();
  std::string errorMsg;
  int myerrno = ci->createCollectionCoordinator(
      dbName,
      std::to_string(col->id()),
      col->numberOfShards(), col->replicationFactor(),
      waitForSyncReplication, velocy.slice(), errorMsg, 240.0);

  if (myerrno != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      errorMsg = TRI_errno_string(myerrno);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(myerrno, errorMsg);
  }

  ci->loadPlan();

  auto c = ci->getCollection(dbName, std::to_string(col->id()));
  // We never get a nullptr here because an exception is thrown if the
  // collection does not exist. Also, the create collection should have
  // failed before.
  TRI_ASSERT(c.get() != nullptr);
  return c;
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

int fetchEdgesFromEngines(
    std::string const& dbname,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    VPackSlice const vertexId,
    bool backward,
    std::unordered_map<StringRef, VPackSlice>& cache,
    std::vector<VPackSlice>& result,
    std::vector<std::shared_ptr<VPackBuilder>>& datalake,
    VPackBuilder& builder,
    size_t& read) {
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
  size_t nrDone = 0;
  cc->performRequests(requests, CL_DEFAULT_TIMEOUT, nrDone, Logger::COMMUNICATION, false);

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
    read += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
        resSlice, "readIndex", 0);
    VPackSlice edges = resSlice.get("edges");
    for (auto const& e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC(ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      StringRef idRef(id);
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

}  // namespace arangodb
