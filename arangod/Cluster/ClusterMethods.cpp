////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "ClusterMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterCollectionCreationInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Cluster/ClusterTypes.h"
#include "Futures/Utilities.h"
#include "Graph/ClusterGraphDatalake.h"
#include "Graph/ClusterTraverserCache.h"
#include "Metrics/Counter.h"
#include "Metrics/Types.h"
#include "Network/ClusterUtils.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/Version.h"
#include "Sharding/ShardingInfo.h"
#include "StorageEngine/HotBackupCommon.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Utilities/NameValidator.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#ifdef USE_V8
#include "V8Server/FoxxFeature.h"
#endif
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Version.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBHotBackup/RocksDBHotBackup.h"
#include "Enterprise/VocBase/VirtualClusterSmartEdgeCollection.h"
#endif
#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

#include <absl/strings/str_cat.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::rest;

using Helper = arangodb::basics::VelocyPackHelper;

namespace {
std::string const edgeUrl = "/_internal/traverser/edge/";
std::string const vertexUrl = "/_internal/traverser/vertex/";

}  // namespace

// Timeout for write operations, note that these are used for communication
// with a shard leader and we always have to assume that some follower has
// stopped writes for some time to get in sync:
static double const CL_DEFAULT_LONG_TIMEOUT = 900.0;

namespace {
template<typename T>
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

/// @brief begin a transaction on some leader shards
template<typename ShardDocsMap>
Future<Result> beginTransactionOnSomeLeaders(TransactionState& state,
                                             LogicalCollection const& coll,
                                             ShardDocsMap const& shards,
                                             transaction::MethodsApi api) {
  TRI_ASSERT(state.isCoordinator());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));

  ClusterTrxMethods::SortedServersSet servers{};

  if (state.options().allowDirtyReads) {
    // In this case we do not always choose the leader, but take the
    // choice stored in the TransactionState. We might hit some followers
    // in this case, but this is the purpose of `allowDirtyReads`.
    for (auto const& pair : shards) {
      ServerID const& replica = state.whichReplica(pair.first);
      if (!state.knowsServer(replica)) {
        servers.emplace(replica);
      }
    }
  } else {
    std::shared_ptr<ShardMap> shardMap = coll.shardIds();
    for (auto const& pair : shards) {
      auto const& it = shardMap->find(pair.first);
      if (it->second.empty()) {
        return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;  // something is broken
      }
      // now we got the shard leader
      std::string const& leader = it->second[0];
      if (!state.knowsServer(leader)) {
        servers.emplace(leader);
      }
    }
  }
  return ClusterTrxMethods::beginTransactionOnLeaders(state.shared_from_this(),
                                                      std::move(servers), api);
}

// begin transaction on shard leaders
Future<Result> beginTransactionOnAllLeaders(transaction::Methods& trx,
                                            ShardMap const& shards,
                                            transaction::MethodsApi api) {
  TRI_ASSERT(trx.state()->isCoordinator());
  TRI_ASSERT(trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED));
  ClusterTrxMethods::SortedServersSet servers{};
  if (trx.state()->options().allowDirtyReads) {
    // In this case we do not always choose the leader, but take the
    // choice stored in the TransactionState. We might hit some followers
    // in this case, but this is the purpose of `allowDirtyReads`.
    for (auto const& pair : shards) {
      ServerID const& replica = trx.state()->whichReplica(pair.first);
      if (!trx.state()->knowsServer(replica)) {
        servers.emplace(replica);
      }
    }
  } else {
    for (auto const& shardServers : shards) {
      ServerID const& srv = shardServers.second.at(0);
      if (!trx.state()->knowsServer(srv)) {
        servers.emplace(srv);
      }
    }
  }
  return ClusterTrxMethods::beginTransactionOnLeaders(trx.stateShrdPtr(),
                                                      std::move(servers), api);
}

/// @brief add the correct header for the shard
void addTransactionHeaderForShard(transaction::Methods const& trx,
                                  ShardMap const& shardMap,
                                  ShardID const& shard,
                                  network::Headers& headers) {
  TRI_ASSERT(trx.state()->isCoordinator());
  if (!ClusterTrxMethods::isElCheapo(trx)) {
    return;  // no need
  }

  // If we are in a reading transaction and are supposed to read from followers
  // then we need to send transaction begin headers not only to leaders, but
  // sometimes also to followers. The `TransactionState` knows this and so
  // we must consult `whichReplica` instead of blindly taking the leader.
  // Note that this essentially only happens in `getDocumentOnCoordinator`.
  if (trx.state()->options().allowDirtyReads) {
    ServerID const& server = trx.state()->whichReplica(shard);
    ClusterTrxMethods::addTransactionHeader(trx, server, headers);
  } else {
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
                                     "couldn't find shard in shardMap");
    }
  }
}

/// @brief iterate over shard responses and compile a result
/// This will take care of checking the fuerte responses. If the response has
/// a body, then the callback will be called on the body, with access to the
/// result-so-far. In particular, a VPackBuilder is initialized to empty before
/// handling any response. It will be passed to the pre callback (default noop)
/// for initialization, then it will be passed to each instantiation of the
/// handler callback, reduce-style. Finally, it will be passed to the post
/// callback and then returned via the OperationResult.
OperationResult handleResponsesFromAllShards(
    OperationOptions const& options,
    std::vector<futures::Try<arangodb::network::Response>>& responses,
    std::function<void(Result&, VPackBuilder&, ShardID const&,
                       VPackSlice)> const& handler,
    std::function<void(Result&, VPackBuilder&)> const& pre =
        [](Result&, VPackBuilder&) -> void {},
    std::function<void(Result&, VPackBuilder&)> const& post =
        [](Result&, VPackBuilder&) -> void {}) {
  // If none of the shards responds we return a SERVER_ERROR;
  Result result;
  VPackBuilder builder;

  pre(result, builder);

  if (!result.fail()) {
    for (Try<arangodb::network::Response> const& tryRes : responses) {
      network::Response const& res = tryRes.get();  // throws exceptions upwards

      TRI_ASSERT(result.ok());
      result = res.combinedResult();
      if (result.ok()) {
        TRI_ASSERT(res.error == fuerte::Error::NoError);
        auto maybeShardID = res.destinationShard();
        if (ADB_UNLIKELY(maybeShardID.fail())) {
          THROW_ARANGO_EXCEPTION(maybeShardID.result());
        }
        handler(result, builder, maybeShardID.get(), res.slice());
      }

      if (result.fail()) {
        break;
      }
    }
    post(result, builder);
  }

  return OperationResult(result, builder.steal(), options);
}

// velocypack representation of object
// {"error":true,"errorMessage":"document not found","errorNum":1202}
const char* notFoundSlice =
    "\x14\x36\x45\x65\x72\x72\x6f\x72\x1a\x4c\x65\x72\x72\x6f\x72\x4d"
    "\x65\x73\x73\x61\x67\x65\x52\x64\x6f\x63\x75\x6d\x65\x6e\x74\x20"
    "\x6e\x6f\x74\x20\x66\x6f\x75\x6e\x64\x48\x65\x72\x72\x6f\x72\x4e"
    "\x75\x6d\x29\xb2\x04\x03";

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
void mergeResultsAllShards(
    std::vector<VPackSlice> const& results, VPackBuilder& resultBody,
    std::unordered_map<::ErrorCode, size_t>& errorCounter,
    VPackValueLength expectedResults, bool silent) {
  // errorCounter is not allowed to contain any NOT_FOUND entry.
  TRI_ASSERT(errorCounter.find(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) ==
             errorCounter.end());
  size_t realNotFound = 0;

  resultBody.clear();
  resultBody.openArray();
  for (VPackValueLength currentIndex = 0; currentIndex < expectedResults;
       ++currentIndex) {
    bool foundRes = false;
    for (VPackSlice oneRes : results) {
      TRI_ASSERT(oneRes.isArray());
      oneRes = oneRes.at(currentIndex);

      auto errorNum = TRI_ERROR_NO_ERROR;
      if (auto errorNumSlice = oneRes.get(StaticStrings::ErrorNum);
          errorNumSlice.isNumber()) {
        errorNum = ::ErrorCode{errorNumSlice.getNumber<int>()};
      }
      if ((errorNum != TRI_ERROR_NO_ERROR &&
           errorNum != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) ||
          oneRes.hasKey(StaticStrings::KeyString)) {
        // This is the correct result: Use it
        foundRes = true;
        if (!silent || (errorNum != TRI_ERROR_NO_ERROR &&
                        errorNum != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
          resultBody.add(oneRes);
        }
        break;
      }
    }
    if (!foundRes) {
      // Found none, use static NOT_FOUND
      resultBody.add(
          VPackSlice(reinterpret_cast<uint8_t const*>(::notFoundSlice)));
      realNotFound++;
    }
  }
  resultBody.close();
  if (realNotFound > 0) {
    errorCounter.try_emplace(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, realNotFound);
  }
}

struct InsertOperationCtx;

/// @brief handle CRUD api shard responses, fast path
template<typename F, typename CT>
OperationResult handleCRUDShardResponsesFast(
    F&& func, CT const& opCtx,
    std::vector<Try<network::Response>> const& results) {
  std::map<ShardID, VPackSlice> resultMap;
  std::map<ShardID, int> shardError;
  std::unordered_map<::ErrorCode, size_t> errorCounter;

  fuerte::StatusCode code =
      results.empty() ? fuerte::StatusOK : fuerte::StatusInternalError;
  // If the list of shards is not empty and none of the shards responded we
  // return a SERVER_ERROR;
  if constexpr (std::is_same_v<CT, InsertOperationCtx>) {
    if (opCtx.reverseMapping.size() == opCtx.localErrors.size()) {
      // all batch operations failed because of key errors, return Accepted
      code = fuerte::StatusAccepted;
    }
  }

  for (Try<arangodb::network::Response> const& tryRes : results) {
    network::Response const& res = tryRes.get();  // throws exceptions upwards
    auto maybeShardID = res.destinationShard();
    if (ADB_UNLIKELY(maybeShardID.fail())) {
      THROW_ARANGO_EXCEPTION(maybeShardID.result());
    }
    auto sId = std::move(maybeShardID.get());

    auto commError = network::fuerteToArangoErrorCode(res);
    if (commError != TRI_ERROR_NO_ERROR) {
      shardError.try_emplace(sId, commError);
    } else {
      VPackSlice result = res.slice();
      // we expect an array of baby-documents, but the response might
      // also be an error, if the DBServer threw a hissy fit
      if (result.isObject()) {
        if (auto error = result.get(StaticStrings::Error); error.isTrue()) {
          // an error occurred, now rethrow the error
          auto code = ::ErrorCode{
              result.get(StaticStrings::ErrorNum).getNumericValue<int>()};
          VPackSlice msg = result.get(StaticStrings::ErrorMessage);
          if (msg.isString()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(code, msg.copyString());
          } else {
            THROW_ARANGO_EXCEPTION(code);
          }
        }
      }
      resultMap.try_emplace(std::move(sId), result);
      network::errorCodesFromHeaders(res.response().header.meta(), errorCounter,
                                     true);
      code = res.statusCode();
    }
  }

  // merge the baby-object results. reverseMapping contains
  // the ordering of elements, the vector in this
  // map is expected to be sorted from front to back.
  // resultMap contains the answers for each shard.
  // It is guaranteed that the resulting array indexes are
  // equal to the original request ordering before it was destructured

  VPackBuilder resultBody;
  resultBody.openArray();
  for (auto const& pair : opCtx.reverseMapping) {
    ShardID const& sId = pair.first;
    if constexpr (std::is_same_v<CT, InsertOperationCtx>) {
      if (sId == ShardID::invalidShard()) {
        Result const& res = opCtx.localErrors[pair.second];
        resultBody.openObject(
            /*unindexed*/ true);
        resultBody.add(StaticStrings::Error, VPackValue(true));
        resultBody.add(StaticStrings::ErrorNum, VPackValue(res.errorNumber()));
        resultBody.add(StaticStrings::ErrorMessage,
                       VPackValue(res.errorMessage()));
        resultBody.close();
        ++errorCounter[res.errorNumber()];
        continue;
      }
    }
    if (auto const& it = resultMap.find(sId);
        it == resultMap.end()) {  // no answer from this shard
      auto const& it2 = shardError.find(sId);
      TRI_ASSERT(it2 != shardError.end());
      resultBody.openObject(/*unindexed*/ true);
      resultBody.add(StaticStrings::Error, VPackValue(true));
      resultBody.add(StaticStrings::ErrorNum, VPackValue(it2->second));
      resultBody.close();
    } else {
      VPackSlice arr = it->second;
      VPackSlice doc = arr.at(pair.second);
      TRI_ASSERT(doc.isObject());

      if (!opCtx.options.silent || doc.get(StaticStrings::Error).isTrue()) {
        // in silent mode we suppress all non-errors
        resultBody.add(arr.at(pair.second));
      }
    }
  }
  resultBody.close();

  return std::forward<F>(func)(code, resultBody.steal(),
                               std::move(opCtx.options),
                               std::move(errorCounter));
}

/// @brief handle CRUD api shard responses, slow path
template<typename F>
OperationResult handleCRUDShardResponsesSlow(
    F&& func, size_t expectedLen, OperationOptions options,
    std::vector<Try<network::Response>> const& responses) {
  if (expectedLen == 0) {  // Only one can answer, we react a bit differently
    std::shared_ptr<VPackBuffer<uint8_t>> buffer;

    int nrok = 0;
    auto commError = TRI_ERROR_NO_ERROR;
    fuerte::StatusCode code = fuerte::StatusUndefined;
    for (size_t i = 0; i < responses.size(); i++) {
      network::Response const& res = responses[i].get();

      if (res.error == fuerte::Error::NoError) {
        // if no shard has the document, use NF answer from last shard
        bool const isNotFound = res.statusCode() == fuerte::StatusNotFound;
        if (!isNotFound ||
            (isNotFound && nrok == 0 && i == responses.size() - 1)) {
          nrok++;
          code = res.statusCode();
          buffer = res.response().stealPayload();
        }
      } else {
        commError = network::fuerteToArangoErrorCode(res);
      }
    }

    if (nrok == 0) {  // This can only happen, if a commError was encountered!
      return OperationResult(commError, std::move(options));
    }
    if (nrok > 1) {
      return OperationResult(TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS,
                             std::move(options));
    }

    TRI_ASSERT(nrok == 1);
    // whenever we get here, nrok must be == 1, and code must have been set
    // to something other than StatusUndefined before
    TRI_ASSERT(code != fuerte::StatusUndefined);
    return std::forward<F>(func)(code, std::move(buffer), std::move(options),
                                 {});
  }

  // We select all results from all shards and merge them back again.
  std::vector<VPackSlice> allResults;
  allResults.reserve(responses.size());

  std::unordered_map<::ErrorCode, size_t> errorCounter;
  // If no server responds we return 500
  for (Try<network::Response> const& tryRes : responses) {
    network::Response const& res = tryRes.get();
    if (res.error != fuerte::Error::NoError) {
      return OperationResult(network::fuerteToArangoErrorCode(res),
                             std::move(options));
    }

    allResults.push_back(res.slice());
    network::errorCodesFromHeaders(res.response().header.meta(), errorCounter,
                                   /*includeNotFound*/ false);
  }
  VPackBuilder resultBody;
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == responses.size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, expectedLen,
                        options.silent);
  return OperationResult(Result(), resultBody.steal(), std::move(options),
                         std::move(errorCounter));
}

/// @brief class to move values across future handlers
struct CrudOperationCtx {
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  std::map<ShardID, std::vector<VPackSlice>> shardMap;
  arangodb::OperationOptions options;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap
////////////////////////////////////////////////////////////////////////////////

::ErrorCode distributeBabyOnShards(CrudOperationCtx& opCtx,
                                   LogicalCollection& collinfo,
                                   velocypack::Slice value) {
  TRI_ASSERT(!collinfo.isSmart() || collinfo.type() == TRI_COL_TYPE_DOCUMENT);

  ShardID shardID;
  if (!value.isString() && !value.isObject()) {
    // We have invalid input at this point.
    // However we can work with the other babies.
    // This is for compatibility with single server
    // We just assign it to any shard and pretend the user has given a key
    shardID = *collinfo.shardingInfo()->shardListAsShardID().begin();
  } else {
    // Now find the responsible shard:
    [[maybe_unused]] bool usesDefaultShardingAttributes;
    auto maybeShardID = collinfo.getResponsibleShard(
        value, /*docComplete*/ false, usesDefaultShardingAttributes);

    if (maybeShardID.fail()) {
      if (maybeShardID.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
        // Rewrite error response
        return TRI_ERROR_CLUSTER_SHARD_GONE;
      }
      return maybeShardID.result().errorNumber();
    }
    shardID = std::move(maybeShardID.get());
  }

  // We found the responsible shard. Add it to the list.
  auto& map = opCtx.shardMap[shardID];
  map.push_back(value);
  opCtx.reverseMapping.emplace_back(shardID, map.size() - 1);
  return TRI_ERROR_NO_ERROR;
}

struct InsertOperationCtx {
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  std::map<ShardID, std::vector<std::pair<velocypack::Slice, std::string>>>
      shardMap;
  arangodb::OperationOptions options;
  std::vector<Result> localErrors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap.
///        Also generates a key if necessary.
////////////////////////////////////////////////////////////////////////////////

::ErrorCode distributeInsertBatchOnShards(InsertOperationCtx& opCtx,
                                          LogicalCollection& collinfo,
                                          VPackSlice value) {
  // this function must not be called for smart edge collections
  TRI_ASSERT(collinfo.type() != TRI_COL_TYPE_EDGE || !collinfo.isSmart());

  bool isRestore = opCtx.options.isRestore;
  std::string key;

  auto addLocalError = [&](Result err) {
    TRI_ASSERT(err.fail());
    auto idx = opCtx.localErrors.size();
    opCtx.localErrors.emplace_back(std::move(err));
    opCtx.reverseMapping.emplace_back(ShardID::invalidShard(), idx);
  };

  ResultT<ShardID> maybeShardID = std::invoke([&]() -> ResultT<ShardID> {
    if (!value.isObject()) {
      // We have invalid input at this point.
      // However we can work with the other babies.
      // This is for compatibility with single server
      // We just assign it to any shard and pretend the user has given a key
      return *collinfo.shardingInfo()->shardListAsShardID().begin();
    } else {
      // Sort out the _key attribute:
      // The user is allowed to specify _key, provided that _key is the one
      // and only sharding attribute, because in this case we can delegate
      // the responsibility to make _key attributes unique to the responsible
      // shard. Otherwise, we ensure uniqueness here and now by taking a
      // cluster-wide unique number. Note that we only know the sharding
      // attributes a bit further down the line when we have determined
      // the responsible shard.

      bool userSpecifiedKey = false;
      VPackSlice keySlice = value.get(StaticStrings::KeyString);
      if (keySlice.isNone()) {
        // The user did not specify a key, let's (probably) create one...

        // if we only have a single shard, we can let the DB server generate
        // the key. this is beneficial so that the key generators can generate
        // an increasing sequence of keys, regardless of how many coordinators
        // there are.
        if (collinfo.mustCreateKeyOnCoordinator()) {
          key = collinfo.keyGenerator().generate(value);
        }
      } else {
        userSpecifiedKey = true;
        if (keySlice.isString()) {
          if (!keySlice.stringView().empty()) {
            // validate the key provided by the user
            auto res = collinfo.keyGenerator().validate(keySlice.stringView(),
                                                        value, isRestore);
            if (res != TRI_ERROR_NO_ERROR) {
              addLocalError(res);
              return ShardID::invalidShard();
            }
          }
        }
      }

      // Now find the responsible shard:
      bool usesDefaultShardingAttributes;
      ResultT<ShardID> result;
      if (userSpecifiedKey) {
        result = collinfo.getResponsibleShard(value, /*docComplete*/ true,
                                              usesDefaultShardingAttributes);
      } else {
        // we pass in the generated _key so we do not need to rebuild the input
        // slice
        TRI_ASSERT(!key.empty() || !collinfo.mustCreateKeyOnCoordinator());
        result = collinfo.getResponsibleShard(value, /*docComplete*/ true,
                                              usesDefaultShardingAttributes,
                                              std::string_view(key));
      }
      if (result.fail()) {
        return result;
      }
      // Now perform the above mentioned check:
      if (userSpecifiedKey &&
          (!usesDefaultShardingAttributes || !collinfo.allowUserKeys()) &&
          !isRestore) {
        addLocalError(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
        return ShardID::invalidShard();
      }
      return result;
    }
  });
  if (maybeShardID.fail()) {
    if (maybeShardID.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return TRI_ERROR_CLUSTER_SHARD_GONE;
    }
    return maybeShardID.result().errorNumber();
  }
  if (maybeShardID.get() == ShardID::invalidShard()) {
    return TRI_ERROR_NO_ERROR;
  }
  auto shardID = std::move(maybeShardID.get());
  // We found the responsible shard. Add it to the list.
  auto it = opCtx.shardMap.find(shardID);
  if (it == opCtx.shardMap.end()) {
    opCtx.shardMap.try_emplace(shardID,
                               std::vector<std::pair<VPackSlice, std::string>>{
                                   {value, std::move(key)}});
    opCtx.reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(value, std::move(key));
    opCtx.reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief Enterprise Relevant code to filter out hidden collections
///        that should not be triggered directly by operations.
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
bool ClusterMethods::filterHiddenCollections(LogicalCollection const& c) {
  return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Enterprise Relevant code to filter out hidden collections
///        that should not be included in links
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
bool ClusterMethods::includeHiddenCollectionInLink(std::string const& name) {
  return true;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Enterprise Relevant code to demangle hidden collections names
////////////////////////////////////////////////////////////////////////////////
#ifndef USE_ENTERPRISE
void ClusterMethods::realNameFromSmartName(std::string&) {}
#endif

namespace arangodb {

void aggregateClusterFigures(bool details, bool isSmartEdgeCollectionPart,
                             velocypack::Slice value,
                             velocypack::Builder& builder) {
  TRI_ASSERT(value.isObject());
  TRI_ASSERT(builder.slice().isObject());
  TRI_ASSERT(builder.isClosed());

  VPackBuilder updated;

  updated.openObject();

  bool cacheInUse = Helper::getBooleanValue(value, "cacheInUse", false);
  bool totalCacheInUse =
      cacheInUse ||
      Helper::getBooleanValue(builder.slice(), "cacheInUse", false);
  updated.add("cacheInUse", VPackValue(totalCacheInUse));

  if (cacheInUse) {
    updated.add("cacheLifeTimeHitRate",
                VPackValue(addFigures<double>(value, builder.slice(),
                                              {"cacheLifeTimeHitRate"})));
    updated.add("cacheWindowedHitRate",
                VPackValue(addFigures<double>(value, builder.slice(),
                                              {"cacheWindowedHitRate"})));
  }
  updated.add("cacheSize", VPackValue(addFigures<size_t>(value, builder.slice(),
                                                         {"cacheSize"})));
  updated.add("cacheUsage", VPackValue(addFigures<size_t>(
                                value, builder.slice(), {"cacheUsage"})));
  updated.add("documentsSize", VPackValue(addFigures<size_t>(
                                   value, builder.slice(), {"documentsSize"})));

  updated.add("indexes", VPackValue(VPackValueType::Object));
  VPackSlice indexes = builder.slice().get("indexes");
  if (isSmartEdgeCollectionPart || indexes.isObject()) {
    // don't count indexes multiple times - all shards have the same indexes!
    // in addition: don't count the indexes from the sub-collections of a smart
    // edge collection multiple times!
    updated.add("count", indexes.get("count"));
  } else {
    updated.add("count", VPackValue(addFigures<size_t>(value, builder.slice(),
                                                       {"indexes", "count"})));
  }
  updated.add("size", VPackValue(addFigures<size_t>(value, builder.slice(),
                                                    {"indexes", "size"})));
  updated.close();  // "indexes"

  if (details && value.hasKey("engine")) {
    updated.add("engine", VPackValue(VPackValueType::Object));
    if (isSmartEdgeCollectionPart) {
      // don't count documents from the sub-collections of a smart edge
      // collection multiple times
      updated.add("documents", builder.slice().get(std::vector<std::string>{
                                   "engine", "documents"}));
    } else {
      updated.add("documents",
                  VPackValue(addFigures<size_t>(value, builder.slice(),
                                                {"engine", "documents"})));
    }
    // merge indexes together
    std::map<uint64_t, std::pair<VPackSlice, VPackSlice>> indexes;

    updated.add("indexes", VPackValue(VPackValueType::Array));
    VPackSlice rocksDBValues = value.get("engine");

    if (!isSmartEdgeCollectionPart) {
      for (auto const& it : VPackArrayIterator(rocksDBValues.get("indexes"))) {
        VPackSlice idSlice = it.get("id");
        if (!idSlice.isNumber()) {
          continue;
        }
        indexes.emplace(idSlice.getNumber<uint64_t>(),
                        std::make_pair(it, VPackSlice::noneSlice()));
      }
    }

    rocksDBValues = builder.slice().get("engine");
    if (rocksDBValues.isObject()) {
      for (auto const& it : VPackArrayIterator(rocksDBValues.get("indexes"))) {
        VPackSlice idSlice = it.get("id");
        if (!idSlice.isNumber()) {
          continue;
        }
        auto idx = indexes.find(idSlice.getNumber<uint64_t>());
        if (idx == indexes.end()) {
          indexes.emplace(idSlice.getNumber<uint64_t>(),
                          std::make_pair(it, VPackSlice::noneSlice()));
        } else {
          (*idx).second.second = it;
        }
      }
    }

    for (auto const& it : indexes) {
      updated.openObject();
      updated.add("type", it.second.first.get("type"));
      updated.add("id", it.second.first.get("id"));
      uint64_t count = it.second.first.get("count").getNumber<uint64_t>();
      if (it.second.second.isObject()) {
        count += it.second.second.get("count").getNumber<uint64_t>();
      }
      updated.add("count", VPackValue(count));
      updated.close();
    }

    updated.close();  // "indexes" array
    updated.close();  // "engine" object
  }

  updated.close();

  TRI_ASSERT(updated.slice().isObject());
  TRI_ASSERT(updated.isClosed());

  builder =
      VPackCollection::merge(builder.slice(), updated.slice(), true, false);
  TRI_ASSERT(builder.slice().isObject());
  TRI_ASSERT(builder.isClosed());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

futures::Future<OperationResult> revisionOnCoordinator(
    ClusterFeature& feature, std::string const& dbname,
    std::string const& collname, OperationOptions const& options) {
  // Set a few variables needed for our work:
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  network::RequestOptions reqOpts;
  reqOpts.database = dbname;
  reqOpts.timeout = network::Timeout(300.0);

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  std::vector<Future<network::Response>> futures;
  futures.reserve(shards->size());

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  for (auto const& p : *shards) {
    auto future = network::sendRequestRetry(
        pool, "shard:" + p.first, fuerte::RestVerb::Get,
        "/_api/collection/" + p.first + "/revision", VPackBuffer<uint8_t>(),
        reqOpts);
    futures.emplace_back(std::move(future));
  }

  auto cb =
      [options](
          std::vector<Try<network::Response>>&& results) -> OperationResult {
    return handleResponsesFromAllShards(
        options, results,
        [](Result& result, VPackBuilder& builder, ShardID const&,
           VPackSlice answer) -> void {
          if (answer.isObject()) {
            VPackSlice r = answer.get("revision");
            if (r.isString() || r.isInteger()) {
              RevisionId cmp = RevisionId::fromSlice(r);
              RevisionId rid = RevisionId::fromSlice(builder.slice());
              if (cmp != RevisionId::max() && cmp > rid) {
                // get the maximum value
                builder.clear();
                builder.add(VPackValue(cmp.toString()));
              }
              return;
            }
          }
          // didn't get the expected response
          result.reset(TRI_ERROR_INTERNAL);
        });
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

futures::Future<OperationResult> checksumOnCoordinator(
    ClusterFeature& feature, std::string const& dbname,
    std::string const& collname, OperationOptions const& options,
    bool withRevisions, bool withData) {
  // Set a few variables needed for our work:
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  network::RequestOptions reqOpts;
  reqOpts.database = dbname;
  reqOpts.timeout = network::Timeout(600.0);
  reqOpts.param("withRevisions", withRevisions ? "true" : "false");
  reqOpts.param("withData", withData ? "true" : "false");

  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  std::vector<Future<network::Response>> futures;
  futures.reserve(shards->size());

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  for (auto const& p : *shards) {
    auto future = network::sendRequestRetry(
        pool, "shard:" + p.first, fuerte::RestVerb::Get,
        "/_api/collection/" + p.first + "/checksum", VPackBuffer<uint8_t>(),
        reqOpts);
    futures.emplace_back(std::move(future));
  }

  auto cb = [options](std::vector<Try<network::Response>>&& results) mutable
      -> OperationResult {
    auto pre = [](Result&, VPackBuilder& builder) -> void {
      VPackObjectBuilder b(&builder);
      builder.add("checksum", VPackValue(0));
      builder.add("revision", VPackValue(RevisionId::none().id()));
    };
    auto handler = [](Result& result, VPackBuilder& builder, ShardID const&,
                      VPackSlice answer) mutable -> void {
      if (!answer.isObject()) {
        result.reset(TRI_ERROR_INTERNAL,
                     "invalid data received for checksum calculation");
        return;
      }

      VPackSlice r = answer.get("revision");
      VPackSlice c = answer.get("checksum");
      if (!r.isString() || !c.isString()) {
        result.reset(TRI_ERROR_INTERNAL,
                     "invalid data received for checksum calculation");
        return;
      }

      VPackValueLength len;
      auto p = c.getString(len);
      bool valid = true;
      auto checksum = NumberUtils::atoi<std::uint64_t>(p, p + len, valid);
      if (!valid) {
        result.reset(TRI_ERROR_INTERNAL,
                     "invalid data received for checksum calculation");
        return;
      }

      // xor is commutative, so it doesn't matter in which order we combine
      // document checksums or partial results from different shards.
      checksum ^= builder.slice().get("checksum").getUInt();

      RevisionId cmp = RevisionId::fromSlice(r);
      RevisionId rid = RevisionId::fromSlice(builder.slice().get("revision"));
      if (cmp != RevisionId::max() && cmp > rid) {
        // get the maximum value
        rid = cmp;
      }

      builder.clear();
      VPackObjectBuilder b(&builder);
      builder.add("checksum", VPackValue(checksum));
      builder.add("revision", VPackValue(rid.toString()));
    };
    return handleResponsesFromAllShards(options, results, handler, pre);
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

futures::Future<Result> warmupOnCoordinator(ClusterFeature& feature,
                                            std::string const& dbname,
                                            std::string const& cid,
                                            OperationOptions const& options) {
  // Set a few variables needed for our work:
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo = ci.getCollectionNT(dbname, cid);
  if (collinfo == nullptr) {
    return futures::makeFuture(Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
  }
  std::vector<ShardID> shards;

  auto addShards = [](std::vector<ShardID>& result,
                      std::shared_ptr<LogicalCollection> const& collection) {
    if (collection == nullptr) {
      return;
    }
    auto shardIds = collection->shardIds();
    for (auto const& it : *shardIds) {
      result.emplace_back(it.first);
    }
  };

  addShards(shards, collinfo);

#ifdef USE_ENTERPRISE
  if (collinfo->isSmart() && collinfo->type() == TRI_COL_TYPE_EDGE) {
    // SmartGraph galore
    auto theEdge = dynamic_cast<arangodb::VirtualClusterSmartEdgeCollection*>(
        collinfo.get());
    if (theEdge != nullptr) {
      CollectionNameResolver resolver(collinfo->vocbase());

      std::string name =
          resolver.getCollectionNameCluster(theEdge->getLocalCid());
      auto c = ci.getCollectionNT(dbname, name);
      addShards(shards, c);

      name = resolver.getCollectionNameCluster(theEdge->getFromCid());
      c = ci.getCollectionNT(dbname, name);
      addShards(shards, c);

      name = resolver.getCollectionNameCluster(theEdge->getToCid());
      c = ci.getCollectionNT(dbname, name);
      addShards(shards, c);
    }
  }
#endif
  // now make shards in vector unique
  std::sort(shards.begin(), shards.end());
  shards.erase(std::unique(shards.begin(), shards.end()), shards.end());

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  network::RequestOptions opts;
  opts.database = dbname;
  opts.timeout = network::Timeout(300.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(shards.size());

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  for (auto const& p : shards) {
    // handler expects valid velocypack body (empty object minimum)
    VPackBuffer<uint8_t> buffer;
    buffer.append(VPackSlice::emptyObjectSlice().begin(), 1);

    auto future = network::sendRequestRetry(
        pool, "shard:" + p, fuerte::RestVerb::Put,
        "/_api/collection/" + p + "/loadIndexesIntoMemory", std::move(buffer),
        opts);
    futures.emplace_back(std::move(future));
  }

  auto cb =
      [options](
          std::vector<Try<network::Response>>&& results) -> OperationResult {
    return handleResponsesFromAllShards(
        options, results,
        [](Result&, VPackBuilder&, ShardID const&, VPackSlice) -> void {
          // we don't care about response bodies, just that the requests
          // succeeded
        });
  };
  return futures::collectAll(std::move(futures))
      .thenValue(std::move(cb))
      .thenValue(
          [](OperationResult&& opRes) -> Result { return opRes.result; });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

futures::Future<OperationResult> figuresOnCoordinator(
    ClusterFeature& feature, std::string const& dbname,
    std::string const& collname, bool details,
    OperationOptions const& options) {
  // Set a few variables needed for our work:
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  network::RequestOptions reqOpts;
  reqOpts.database = dbname;
  reqOpts.param("details", details ? "true" : "false");
  reqOpts.timeout = network::Timeout(300.0);

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  std::shared_ptr<ShardMap> shards = collinfo->shardIds();
  std::vector<Future<network::Response>> futures;
  futures.reserve(shards->size());

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  for (auto const& p : *shards) {
    auto future = network::sendRequestRetry(
        pool, "shard:" + p.first, fuerte::RestVerb::Get,
        absl::StrCat("/_api/collection/", std::string{p.first}, "/figures"),
        VPackBuffer<uint8_t>(), reqOpts);
    futures.emplace_back(std::move(future));
  }

  auto cb = [details,
             options](std::vector<Try<network::Response>>&& results) mutable
      -> OperationResult {
    auto handler = [details](Result& result, VPackBuilder& builder,
                             ShardID const& /*shardId*/,
                             VPackSlice answer) mutable -> void {
      if (answer.isObject()) {
        VPackSlice figures = answer.get("figures");
        // add to the total
        if (figures.isObject()) {
          aggregateClusterFigures(details, false, figures, builder);
          return;
        }
      }
      // didn't get the expected response
      result.reset(TRI_ERROR_INTERNAL);
    };
    auto pre = [](Result&, VPackBuilder& builder) -> void {
      // initialize to empty object
      builder.add(VPackSlice::emptyObjectSlice());
    };
    return handleResponsesFromAllShards(options, results, handler, pre);
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator, by shard
////////////////////////////////////////////////////////////////////////////////

futures::Future<OperationResult> countOnCoordinator(
    transaction::Methods& trx, std::string const& cname,
    OperationOptions const& options, transaction::MethodsApi api) {
  // Set a few variables needed for our work:
  ClusterFeature& feature = trx.vocbase().server().getFeature<ClusterFeature>();
  ClusterInfo& ci = feature.clusterInfo();

  std::string const& dbname = trx.vocbase().name();
  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci.getCollectionNT(dbname, cname);
  if (collinfo == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();
  bool const isManaged =
      trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isManaged) {
    Result res = ::beginTransactionOnAllLeaders(
                     trx, *shardIds, transaction::MethodsApi::Synchronous)
                     .waitAndGet();
    if (res.fail()) {
      return futures::makeFuture(OperationResult(res, options));
    }
  }

  network::RequestOptions reqOpts;
  reqOpts.database = dbname;
  reqOpts.retryNotFound = true;
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;

  if (NameValidator::isSystemName(cname) &&
      !(collinfo->isSmartChild() || collinfo->isSmartEdgeCollection())) {
    // system collection (e.g. _apps, _jobs, _graphs...) that is not
    // very likely this is an internal request that should not block other
    // processing in case we don't get a timely response
    reqOpts.timeout = network::Timeout(10.0);
  }

  network::addUserParameter(reqOpts, trx.username());

  std::vector<Future<network::Response>> futures;
  futures.reserve(shardIds->size());

  auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
  for (auto const& p : *shardIds) {
    if (p.second.empty()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
    }
    // extract leader
    std::string const& leader = p.second[0];
    network::Headers headers;
    ClusterTrxMethods::addTransactionHeader(trx, leader, headers);

    futures.emplace_back(network::sendRequestRetry(
        pool, "shard:" + p.first, fuerte::RestVerb::Get,
        absl::StrCat("/_api/collection/", std::string{p.first}, "/count"),
        VPackBuffer<uint8_t>(), reqOpts, std::move(headers)));
  }

  auto cb = [options](std::vector<Try<network::Response>>&& results) mutable
      -> OperationResult {
    auto handler = [](Result& result, VPackBuilder& builder,
                      ShardID const& shardId,
                      VPackSlice answer) mutable -> void {
      if (answer.isObject()) {
        if (VPackSlice count = answer.get("count"); count.isNumber()) {
          // add to the total
          VPackArrayBuilder array(&builder);
          array->add(VPackValue(shardId));
          array->add(count);
          return;
        }
      }
      // didn't get the expected response
      result.reset(TRI_ERROR_INTERNAL);
    };
    auto pre = [](Result&, VPackBuilder& builder) -> void {
      builder.openArray();
    };
    auto post = [](Result& result, VPackBuilder& builder) -> void {
      if (builder.isOpenArray()) {
        builder.close();
      } else {
        result.reset(TRI_ERROR_INTERNAL, "result was corrupted");
        builder.clear();
      }
    };
    return handleResponsesFromAllShards(options, results, handler, pre, post);
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the metrics from DBServers
////////////////////////////////////////////////////////////////////////////////

futures::Future<metrics::RawDBServers> metricsOnLeader(
    NetworkFeature& network, ClusterFeature& cluster) {
  LOG_TOPIC("badf0", TRACE, Logger::CLUSTER) << "Start collect metrics";
  auto* pool = network.pool();
  auto serverIds = cluster.clusterInfo().getCurrentDBServers();

  std::vector<Future<network::Response>> futures;
  futures.reserve(serverIds.size());
  for (auto const& id : serverIds) {
    network::Headers headers;
    headers.emplace(StaticStrings::Accept,
                    StaticStrings::MimeTypeJsonNoEncoding);
    futures.push_back(network::sendRequest(
        pool, "server:" + id, fuerte::RestVerb::Get, "/_admin/metrics", {},
        network::RequestOptions{}.param("type", metrics::kDBJson),
        std::move(headers)));
  }
  return collectAll(futures).then(
      [](Try<std::vector<Try<network::Response>>>&& responses) {
        TRI_ASSERT(responses.hasValue());  // collectAll always return value
        metrics::RawDBServers metrics;
        metrics.reserve(responses->size());
        for (auto& response : *responses) {
          if (!response.hasValue() || !response->hasResponse() ||
              response->fail()) {
            continue;  // Shit happens, just ignore it
          }
          auto payload = response->response().stealPayload();
          if (!payload) {
            TRI_ASSERT(false);
            continue;
          }
          velocypack::Slice slice{payload->data()};
          if (!slice.isArray()) {
            continue;  // some like 503
          }
          if (auto const size = slice.length(); size % 3 != 0) {
            TRI_ASSERT(false);
            continue;
          }
          metrics.push_back(std::move(payload));
        }
        return metrics;
      });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the metrics from leader Coordinator
////////////////////////////////////////////////////////////////////////////////

futures::Future<metrics::LeaderResponse> metricsFromLeader(
    NetworkFeature& network, ClusterFeature& cluster, std::string_view leader,
    std::string serverId, uint64_t rebootId, uint64_t version) {
  LOG_TOPIC("badf1", TRACE, Logger::CLUSTER) << "Start receive metrics";
  auto* pool = network.pool();
  network::Headers headers;
  headers.emplace(StaticStrings::Accept, StaticStrings::MimeTypeJsonNoEncoding);
  auto options = network::RequestOptions{}
                     .param("type", metrics::kCDJson)
                     // cppcheck-suppress accessMoved
                     .param("MetricsServerId", std::move(serverId))
                     .param("MetricsRebootId", std::to_string(rebootId))
                     .param("MetricsVersion", std::to_string(version));
  auto future = network::sendRequest(
      pool, absl::StrCat("server:", leader), fuerte::RestVerb::Get,
      "/_admin/metrics", {}, std::move(options), std::move(headers));
  return std::move(future).then([](Try<network::Response>&& response) {
    if (!response.hasValue() || !response->hasResponse() || response->fail()) {
      return metrics::LeaderResponse{};
    }
    return response->response().stealPayload();
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the selectivity estimates from DBservers
////////////////////////////////////////////////////////////////////////////////

Result selectivityEstimatesOnCoordinator(ClusterFeature& feature,
                                         std::string const& dbname,
                                         std::string const& collname,
                                         IndexEstMap& result,
                                         TransactionId tid) {
  // Set a few variables needed for our work:
  ClusterInfo& ci = feature.clusterInfo();

  result.clear();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  std::shared_ptr<ShardMap> shards = collinfo->shardIds();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = dbname;
  reqOpts.retryNotFound = true;
  reqOpts.skipScheduler = true;

  if (NameValidator::isSystemName(collname) &&
      !(collinfo->isSmartChild() || collinfo->isSmartEdgeCollection())) {
    // system collection (e.g. _apps, _jobs, _graphs...) that is not
    // very likely this is an internal request that should not block other
    // processing in case we don't get a timely response
    reqOpts.timeout = network::Timeout(10.0);
  }

  std::vector<Future<network::Response>> futures;
  futures.reserve(shards->size());

  std::string requestsUrl;
  for (auto const& p : *shards) {
    network::Headers headers;
    if (tid.isSet()) {
      headers.try_emplace(StaticStrings::TransactionId,
                          std::to_string(tid.id()));
    }

    reqOpts.param("collection", std::string{p.first});
    futures.emplace_back(network::sendRequestRetry(
        pool, "shard:" + p.first, fuerte::RestVerb::Get,
        "/_api/index/selectivity", VPackBufferUInt8(), reqOpts,
        std::move(headers)));
  }

  // format of expected answer:
  // in `indexes` is a map that has keys in the format
  // s<shardid>/<indexid> and index information as value
  // {"code":200
  // ,"error":false
  // ,"indexes":{ "s10004/0"    : 1.0,
  //              "s10004/10005": 0.5
  //            }
  // }

  containers::FlatHashMap<std::string, std::vector<double>> indexEstimates;
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    auto res = r.combinedResult();
    if (res.fail()) {
      return res;
    }

    VPackSlice answer = r.slice();
    if (!answer.isObject()) {
      return {TRI_ERROR_INTERNAL, "invalid response structure"};
    }

    answer = answer.get("indexes");
    if (!answer.isObject()) {
      return {TRI_ERROR_INTERNAL, "invalid response structure for 'indexes'"};
    }

    // add to the total
    for (auto pair : VPackObjectIterator(answer, true)) {
      std::string_view shardIndexId(pair.key.stringView());
      auto split = std::find(shardIndexId.begin(), shardIndexId.end(), '/');
      if (split != shardIndexId.end()) {
        std::string index(split + 1, shardIndexId.end());
        double estimate = Helper::getNumericValue(pair.value, 0.0);
        indexEstimates[std::move(index)].push_back(estimate);
      }
    }
  }

  auto aggregateIndexes = [](std::vector<double> const& vec) -> double {
    TRI_ASSERT(!vec.empty());
    double rv = std::accumulate(vec.begin(), vec.end(), 0.0);
    rv /= static_cast<double>(vec.size());
    return rv;
  };

  for (auto const& p : indexEstimates) {
    result[p.first] = aggregateIndexes(p.second);
  }

  return {};
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

futures::Future<OperationResult> insertDocumentOnCoordinator(
    transaction::Methods const& trx, LogicalCollection& coll, VPackSlice slice,
    OperationOptions const& options, transaction::MethodsApi api) {
  // create vars used in this function
  std::shared_ptr<ShardMap> shardIds = coll.shardIds();
  bool const useMultiple = slice.isArray();  // insert more than one document
  InsertOperationCtx opCtx;
  opCtx.options = options;

  // create shard map
  if (useMultiple) {
    for (VPackSlice value : VPackArrayIterator(slice)) {
      auto res = distributeInsertBatchOnShards(opCtx, coll, value);
      if (res != TRI_ERROR_NO_ERROR) {
        return makeFuture(OperationResult(res, options));
      }
    }
  } else {
    auto res = distributeInsertBatchOnShards(opCtx, coll, slice);
    if (res != TRI_ERROR_NO_ERROR) {
      return makeFuture(OperationResult(res, options));
    }
    if (!opCtx.localErrors.empty()) {
      return makeFuture(OperationResult(opCtx.localErrors.front(), options));
    }
  }

  if (opCtx.shardMap.empty()) {
    return handleCRUDShardResponsesFast(network::clusterResultInsert, opCtx,
                                        {});
    // all operations failed with a local error
  }

#ifdef USE_V8
  bool const isJobsCollection =
      coll.system() && coll.name() == StaticStrings::JobsCollection;
#endif

  Future<Result> f = makeFuture(Result());
  bool const isManaged =
      trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isManaged &&
      opCtx.shardMap.size() > 1) {  // lazily begin transactions on leaders
    f = beginTransactionOnSomeLeaders(*trx.state(), coll, opCtx.shardMap, api);
  }

  return std::move(f).thenValue([=, &trx,
                                 opCtx(std::move(opCtx))](Result&& r) mutable
                                -> Future<OperationResult> {
    if (r.fail()) {
      return OperationResult(std::move(r), options);
    }

    std::string const baseUrl = "/_api/document/";

    network::RequestOptions reqOpts;
    reqOpts.database = trx.vocbase().name();
    reqOpts.timeout = network::Timeout(CL_DEFAULT_LONG_TIMEOUT);
    reqOpts.retryNotFound = true;
    reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;
    reqOpts
        .param(StaticStrings::WaitForSyncString,
               (options.waitForSync ? "true" : "false"))
        .param(StaticStrings::ReturnNewString,
               (options.returnNew ? "true" : "false"))
        .param(StaticStrings::ReturnOldString,
               (options.returnOld ? "true" : "false"))
        .param(StaticStrings::IsRestoreString,
               (options.isRestore ? "true" : "false"))
        .param(StaticStrings::KeepNullString,
               (options.keepNull ? "true" : "false"))
        .param(StaticStrings::MergeObjectsString,
               (options.mergeObjects ? "true" : "false"))
        .param(StaticStrings::SkipDocumentValidation,
               (options.validate ? "false" : "true"));

    // note: the "silent" flag is not forwarded to the leader by the
    // coordinator. the coordinator handles the "silent" flag on its own.

    if (options.refillIndexCaches != RefillIndexCaches::kDefault) {
      // this attribute can have 3 values: default, true and false. only
      // expose it when it is not set to "default"
      reqOpts.param(StaticStrings::RefillIndexCachesString,
                    (options.refillIndexCaches == RefillIndexCaches::kRefill)
                        ? "true"
                        : "false");
    }
    if (!options.versionAttribute.empty()) {
      reqOpts.param(StaticStrings::VersionAttributeString,
                    options.versionAttribute);
    }
    if (options.isOverwriteModeSet()) {
      reqOpts.parameters.insert_or_assign(
          StaticStrings::OverwriteMode,
          OperationOptions::stringifyOverwriteMode(options.overwriteMode));
    }

    network::addUserParameter(reqOpts, trx.username());

    // Now prepare the requests:
    auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
    std::vector<Future<network::Response>> futures;
    futures.reserve(opCtx.shardMap.size());
    for (auto const& it : opCtx.shardMap) {
      VPackBuffer<uint8_t> reqBuffer;
      VPackBuilder reqBuilder(reqBuffer);

      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        auto idx = it.second.front();
        if (idx.second.empty()) {
          reqBuilder.add(slice);
        } else {
          reqBuilder.openObject();
          reqBuilder.add(StaticStrings::KeyString, VPackValue(idx.second));
          TRI_SanitizeObject(slice, reqBuilder);
          reqBuilder.close();
        }
      } else {
        reqBuilder.openArray(/*unindexed*/ true);
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
      }

      network::Headers headers;
      // Just make sure that no dirty read flag makes it here, since we
      // are writing and then `addTransactionHeaderForShard` might
      // misbehave!
      TRI_ASSERT(!trx.state()->options().allowDirtyReads);
      addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first, headers);
      auto future = network::sendRequestRetry(
          pool, "shard:" + it.first, fuerte::RestVerb::Post,
          absl::StrCat(baseUrl, std::string{it.first}), std::move(reqBuffer),
          reqOpts, std::move(headers));
      futures.emplace_back(std::move(future));
    }

#ifdef USE_V8
    // track that we have done a local insert into a Foxx queue.
    // this information will be broadcasted to other coordinators
    // in the cluster eventually via the agency.
    // because the agency update is posted asynchronously, there is the
    // possibility that this coordinator dies before the update is
    // broadcasted to the agency. this is a rather unlikely edge case,
    // and we currently do not optimize for that (i.e. posting updates
    // to the agency is currently best effort).
    if (isJobsCollection && trx.vocbase().server().hasFeature<FoxxFeature>()) {
      trx.vocbase().server().getFeature<FoxxFeature>().trackLocalQueueInsert();
    }
#endif

    // Now compute the result
    if (!useMultiple) {  // single-shard fast track
      TRI_ASSERT(futures.size() == 1);

      auto cb = [options](network::Response&& res) -> OperationResult {
        if (res.error != fuerte::Error::NoError) {
          return OperationResult(network::fuerteToArangoErrorCode(res),
                                 std::move(options));
        }

        return network::clusterResultInsert(res.statusCode(),
                                            res.response().stealPayload(),
                                            std::move(options), {});
      };
      return std::move(futures[0]).thenValue(cb);
    }

    return futures::collectAll(std::move(futures))
        .thenValue([opCtx(std::move(opCtx))](
                       std::vector<Try<network::Response>>&& results)
                       -> OperationResult {
          return handleCRUDShardResponsesFast(network::clusterResultInsert,
                                              opCtx, results);
        });
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

futures::Future<OperationResult> removeDocumentOnCoordinator(
    transaction::Methods& trx, LogicalCollection& coll, VPackSlice const slice,
    OperationOptions const& options, transaction::MethodsApi api) {
  // Set a few variables needed for our work:

  // First determine the collection ID from the name:
  std::shared_ptr<ShardMap> shardIds = coll.shardIds();

  CrudOperationCtx opCtx;
  opCtx.options = options;
  bool useMultiple = slice.isArray();

  bool canUseFastPath = true;
  if (useMultiple) {
    for (VPackSlice value : VPackArrayIterator(slice)) {
      auto res = distributeBabyOnShards(opCtx, coll, value);
      if (res != TRI_ERROR_NO_ERROR) {
        canUseFastPath = false;
        break;
      }
    }
  } else {
    auto res = distributeBabyOnShards(opCtx, coll, slice);
    if (res != TRI_ERROR_NO_ERROR) {
      canUseFastPath = false;
    }
  }
  // We sorted the shards correctly.

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.timeout = network::Timeout(CL_DEFAULT_LONG_TIMEOUT);
  reqOpts.retryNotFound = true;
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;
  reqOpts
      .param(StaticStrings::WaitForSyncString,
             (options.waitForSync ? "true" : "false"))
      .param(StaticStrings::ReturnOldString,
             (options.returnOld ? "true" : "false"))
      .param(StaticStrings::IgnoreRevsString,
             (options.ignoreRevs ? "true" : "false"));

  // note: the "silent" flag is not forwarded to the leader by the
  // coordinator. the coordinator handles the "silent" flag on its own.

  if (options.refillIndexCaches != RefillIndexCaches::kDefault) {
    // this attribute can have 3 values: default, true and false. only
    // expose it when it is not set to "default"
    reqOpts.param(StaticStrings::RefillIndexCachesString,
                  (options.refillIndexCaches == RefillIndexCaches::kRefill)
                      ? "true"
                      : "false");
  }

  network::addUserParameter(reqOpts, trx.username());

  bool const isManaged =
      trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    // lazily begin transactions on leaders
    Future<Result> f = makeFuture(Result());
    if (isManaged && opCtx.shardMap.size() > 1) {
      f = beginTransactionOnSomeLeaders(*trx.state(), coll, opCtx.shardMap,
                                        api);
    }

    return std::move(f).thenValue(
        [=, &trx, opCtx(std::move(opCtx))](
            Result&& r) mutable -> Future<OperationResult> {
          if (r.fail()) {
            return OperationResult(std::move(r), options);
          }

          // Now prepare the requests:
          auto* pool =
              trx.vocbase().server().getFeature<NetworkFeature>().pool();
          std::vector<Future<network::Response>> futures;
          futures.reserve(opCtx.shardMap.size());

          for (auto const& it : opCtx.shardMap) {
            VPackBuffer<uint8_t> buffer;
            if (!useMultiple) {
              TRI_ASSERT(it.second.size() == 1);
              buffer.append(slice.begin(), slice.byteSize());
            } else {
              VPackBuilder reqBuilder(buffer);
              reqBuilder.openArray(/*unindexed*/ true);
              for (VPackSlice value : it.second) {
                reqBuilder.add(value);
              }
              reqBuilder.close();
            }

            network::Headers headers;
            // Just make sure that no dirty read flag makes it here, since we
            // are writing and then `addTransactionHeaderForShard` might
            // misbehave!
            TRI_ASSERT(!trx.state()->options().allowDirtyReads);
            addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first,
                                         headers);
            futures.emplace_back(network::sendRequestRetry(
                pool, "shard:" + it.first, fuerte::RestVerb::Delete,
                absl::StrCat("/_api/document/", std::string{it.first}),
                std::move(buffer), reqOpts, std::move(headers)));
          }

          // Now listen to the results:
          if (!useMultiple) {
            TRI_ASSERT(futures.size() == 1);
            auto cb = [options](network::Response&& res) -> OperationResult {
              if (res.error != fuerte::Error::NoError) {
                return OperationResult(network::fuerteToArangoErrorCode(res),
                                       options);
              }
              return network::clusterResultRemove(res.statusCode(),
                                                  res.response().stealPayload(),
                                                  std::move(options), {});
            };
            return std::move(futures[0]).thenValue(cb);
          }

          return futures::collectAll(std::move(futures))
              .thenValue([opCtx = std::move(opCtx)](
                             std::vector<Try<network::Response>>&& results)
                             -> OperationResult {
                return handleCRUDShardResponsesFast(
                    network::clusterResultRemove, opCtx, results);
              });
        });
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  // lazily begin transactions on leaders
  Future<Result> f = makeFuture(Result());
  if (isManaged && shardIds->size() > 1) {
    f = ::beginTransactionOnAllLeaders(trx, *shardIds, api);
  }

  return std::move(f).thenValue(
      [=, &trx](Result&& r) mutable -> Future<OperationResult> {
        if (r.fail()) {
          return OperationResult(r, options);
        }

        // We simply send the body to all shards and await their results.
        // As soon as we have the results we merge them in the following way:
        // For 1 .. slice.length()
        //    for res : allResults
        //      if res != NOT_FOUND => insert this result. skip other results
        //    end
        //    if (!skipped) => insert NOT_FOUND

        auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
        std::vector<Future<network::Response>> futures;
        futures.reserve(shardIds->size());

        const size_t expectedLen = useMultiple ? slice.length() : 0;
        VPackBuffer<uint8_t> buffer;
        buffer.append(slice.begin(), slice.byteSize());

        for (auto const& shardServers : *shardIds) {
          ShardID const& shard = shardServers.first;
          network::Headers headers;
          // Just make sure that no dirty read flag makes it here, since we
          // are writing and then `addTransactionHeaderForShard` might
          // misbehave!
          TRI_ASSERT(!trx.state()->options().allowDirtyReads);
          addTransactionHeaderForShard(trx, *shardIds, shard, headers);
          futures.emplace_back(network::sendRequestRetry(
              pool, "shard:" + shard, fuerte::RestVerb::Delete,
              absl::StrCat("/_api/document/", std::string{shard}),
              /*cannot move*/ buffer, reqOpts, std::move(headers)));
        }

        return futures::collectAll(std::move(futures))
            .thenValue(
                [=](std::vector<Try<network::Response>>&& responses) mutable
                -> OperationResult {
                  return ::handleCRUDShardResponsesSlow(
                      network::clusterResultRemove, expectedLen,
                      std::move(options), responses);
                });
      });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

futures::Future<OperationResult> truncateCollectionOnCoordinator(
    transaction::Methods& trx, std::string const& collname,
    OperationOptions const& options, transaction::MethodsApi api) {
  Result res;
  // Set a few variables needed for our work:
  ClusterInfo& ci =
      trx.vocbase().server().getFeature<ClusterFeature>().clusterInfo();

  // First determine the collection ID from the name:
  std::shared_ptr<LogicalCollection> collinfo =
      ci.getCollectionNT(trx.vocbase().name(), collname);
  if (collinfo == nullptr) {
    return futures::makeFuture(OperationResult(
        res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND), options));
  }

  // Some stuff to prepare cluster-intern requests:
  // We have to contact everybody:
  std::shared_ptr<ShardMap> shardIds = collinfo->shardIds();

  // lazily begin transactions on all leader shards
  if (trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    res = ::beginTransactionOnAllLeaders(trx, *shardIds,
                                         transaction::MethodsApi::Synchronous)
              .waitAndGet();
    if (res.fail()) {
      return futures::makeFuture(OperationResult(res, options));
    }
  }

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.timeout = network::Timeout(600.0);
  reqOpts.retryNotFound = true;
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;
  reqOpts.param(StaticStrings::Compact,
                (options.truncateCompact ? "true" : "false"));
  network::addUserParameter(reqOpts, trx.username());

  std::vector<Future<network::Response>> futures;
  futures.reserve(shardIds->size());

  auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
  for (auto const& p : *shardIds) {
    // handler expects valid velocypack body (empty object minimum)
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    builder.openObject();
    builder.close();

    network::Headers headers;
    // Just make sure that no dirty read flag makes it here, since we
    // are writing and then `addTransactionHeaderForShard` might
    // misbehave!
    TRI_ASSERT(!trx.state()->options().allowDirtyReads);
    addTransactionHeaderForShard(trx, *shardIds, /*shard*/ p.first, headers);
    auto future = network::sendRequestRetry(
        pool, "shard:" + p.first, fuerte::RestVerb::Put,
        absl::StrCat("/_api/collection/", std::string{p.first}, "/truncate"),
        std::move(buffer), reqOpts, std::move(headers));
    futures.emplace_back(std::move(future));
  }

  auto cb =
      [options](
          std::vector<Try<network::Response>>&& results) -> OperationResult {
    return handleResponsesFromAllShards(
        options, results,
        [](Result&, VPackBuilder&, ShardID const&, VPackSlice) -> void {});
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

Future<OperationResult> getDocumentOnCoordinator(
    transaction::Methods& trx, LogicalCollection& coll, VPackSlice slice,
    OperationOptions const& options, transaction::MethodsApi api) {
  // Set a few variables needed for our work:
  std::shared_ptr<ShardMap> shardIds = coll.shardIds();

  // If _key is the one and only sharding attribute, we can do this quickly,
  // because we can easily determine which shard is responsible for the
  // document. Otherwise we have to contact all shards and ask them to
  // delete the document. All but one will not know it.
  // Now find the responsible shard(s)

  CrudOperationCtx opCtx;
  opCtx.options = options;
  bool useMultiple = slice.isArray();

  bool canUseFastPath = true;
  if (useMultiple) {
    for (VPackSlice value : VPackArrayIterator(slice)) {
      auto res = distributeBabyOnShards(opCtx, coll, value);
      if (res != TRI_ERROR_NO_ERROR) {
        canUseFastPath = false;
        break;
      }
    }
  } else {
    auto res = distributeBabyOnShards(opCtx, coll, slice);
    if (res != TRI_ERROR_NO_ERROR) {
      canUseFastPath = false;
    }
  }

  // lazily begin transactions on leaders
  bool const isManaged =
      trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  bool const allowDirtyReads = trx.state()->options().allowDirtyReads;

  // Some stuff to prepare cluster-internal requests:

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.retryNotFound = true;
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;
  reqOpts.param(StaticStrings::IgnoreRevsString,
                (options.ignoreRevs ? "true" : "false"));

  fuerte::RestVerb restVerb;
  if (!useMultiple) {
    restVerb = options.silent ? fuerte::RestVerb::Head : fuerte::RestVerb::Get;
  } else {
    restVerb = fuerte::RestVerb::Put;
    reqOpts.param(StaticStrings::SilentString,
                  options.silent ? "true" : "false");
    reqOpts.param("onlyget", "true");
  }

  network::addUserParameter(reqOpts, trx.username());

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    Future<Result> f = makeFuture(Result());
    if (isManaged &&
        opCtx.shardMap.size() > 1) {  // lazily begin the transaction
      f = beginTransactionOnSomeLeaders(*trx.state(), coll, opCtx.shardMap,
                                        api);
    }

    return std::move(f).thenValue([=, &trx,
                                   opCtx(std::move(opCtx))](Result&& r) mutable
                                  -> Future<OperationResult> {
      if (r.fail()) {
        return OperationResult(std::move(r), options);
      }

      // Now prepare the requests:
      auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
      std::vector<Future<network::Response>> futures;
      futures.reserve(opCtx.shardMap.size());

      for (auto const& it : opCtx.shardMap) {
        network::Headers headers;
        addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first,
                                     headers);
        if (options.documentCallFromAql) {
          headers.try_emplace(StaticStrings::AqlDocumentCall, "true");
        }
        std::string url;
        VPackBuffer<uint8_t> buffer;

        if (!useMultiple) {
          TRI_ASSERT(it.second.size() == 1);

          if (!options.ignoreRevs && slice.hasKey(StaticStrings::RevString)) {
            headers.try_emplace(
                "if-match", slice.get(StaticStrings::RevString).copyString());
          }
          VPackSlice keySlice = slice;
          if (slice.isObject()) {
            keySlice = slice.get(StaticStrings::KeyString);
          }
          std::string_view ref = keySlice.stringView();
          // We send to single endpoint
          url.append("/_api/document/").append(it.first).push_back('/');
          url.append(StringUtils::urlEncode(ref.data(), ref.length()));
        } else {
          // We send to Babies endpoint
          url.append("/_api/document/").append(it.first);

          VPackBuilder builder(buffer);
          builder.openArray(/*unindexed*/ true);
          for (auto const& value : it.second) {
            builder.add(value);
          }
          builder.close();
        }
        if (allowDirtyReads) {
          auto& cf = trx.vocbase().server().getFeature<ClusterFeature>();
          ++cf.potentiallyDirtyDocumentReadsCounter();
          reqOpts.overrideDestination = trx.state()->whichReplica(it.first);
          headers.try_emplace(StaticStrings::AllowDirtyReads, "true");
        }
        futures.emplace_back(network::sendRequestRetry(
            pool, "shard:" + it.first, restVerb, std::move(url),
            std::move(buffer), reqOpts, std::move(headers)));
      }

      // Now compute the result
      if (!useMultiple) {  // single-shard fast track
        TRI_ASSERT(futures.size() == 1);
        return std::move(futures[0])
            .thenValue([options](network::Response res) -> OperationResult {
              if (res.error != fuerte::Error::NoError) {
                return OperationResult(network::fuerteToArangoErrorCode(res),
                                       options);
              }
              return network::clusterResultDocument(
                  res.statusCode(), res.response().stealPayload(),
                  std::move(options), {});
            });
      }

      return futures::collectAll(std::move(futures))
          .thenValue([opCtx = std::move(opCtx)](
                         std::vector<Try<network::Response>>&& results) {
            return handleCRUDShardResponsesFast(network::clusterResultDocument,
                                                opCtx, results);
          });
    });
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  if (isManaged) {  // lazily begin the transaction
    Result res = ::beginTransactionOnAllLeaders(
                     trx, *shardIds, transaction::MethodsApi::Synchronous)
                     .waitAndGet();
    if (res.fail()) {
      return makeFuture(OperationResult(res, options));
    }
  }

  // Now prepare the requests:
  std::vector<Future<network::Response>> futures;
  futures.reserve(shardIds->size());

  auto& cf = trx.vocbase().server().getFeature<ClusterFeature>();
  auto& nf = trx.vocbase().server().getFeature<NetworkFeature>();
  auto* pool = nf.pool();
  const size_t expectedLen = useMultiple ? slice.length() : 0;
  if (!useMultiple) {
    std::string_view const key(
        (slice.isObject() ? slice.get(StaticStrings::KeyString) : slice)
            .stringView());

    bool addMatch =
        !options.ignoreRevs && slice.hasKey(StaticStrings::RevString);
    for (auto const& shardServers : *shardIds) {
      ShardID const& shard = shardServers.first;

      network::Headers headers;
      addTransactionHeaderForShard(trx, *shardIds, shard, headers);
      if (addMatch) {
        headers.try_emplace("if-match",
                            slice.get(StaticStrings::RevString).copyString());
      }
      if (options.documentCallFromAql) {
        headers.try_emplace(StaticStrings::AqlDocumentCall, "true");
      }

      if (allowDirtyReads) {
        ++cf.potentiallyDirtyDocumentReadsCounter();
        reqOpts.overrideDestination = trx.state()->whichReplica(shard);
        headers.try_emplace(StaticStrings::AllowDirtyReads, "true");
      }

      futures.emplace_back(network::sendRequestRetry(
          pool, "shard:" + shard, restVerb,
          absl::StrCat("/_api/document/", std::string{shard}, "/",
                       StringUtils::urlEncode(key.data(), key.size())),
          VPackBuffer<uint8_t>(), reqOpts, std::move(headers)));
    }
  } else {
    VPackBuffer<uint8_t> buffer;
    buffer.append(slice.begin(), slice.byteSize());
    for (auto const& shardServers : *shardIds) {
      ShardID const& shard = shardServers.first;
      network::Headers headers;
      addTransactionHeaderForShard(trx, *shardIds, shard, headers);

      if (allowDirtyReads) {
        ++cf.potentiallyDirtyDocumentReadsCounter();
        reqOpts.overrideDestination = trx.state()->whichReplica(shard);
        headers.try_emplace(StaticStrings::AllowDirtyReads, "true");
      }

      futures.emplace_back(network::sendRequestRetry(
          pool, "shard:" + shard, restVerb,
          absl::StrCat("/_api/document/", std::string{shard}),
          /*cannot move*/ buffer, reqOpts, std::move(headers)));
    }
  }

  return futures::collectAll(std::move(futures))
      .thenValue([=](std::vector<Try<network::Response>>&& responses) mutable
                 -> OperationResult {
        return ::handleCRUDShardResponsesSlow(network::clusterResultDocument,
                                              expectedLen, std::move(options),
                                              responses);
      });
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

Result fetchEdgesFromEngines(
    transaction::Methods& trx, graph::ClusterTraverserCache& travCache,
    arangodb::aql::FixedVarExpressionContext const& expressionContext,
    std::string_view vertexId, size_t depth, std::vector<VPackSlice>& result) {
  auto const* engines = travCache.engines();
  auto& cache = travCache.cache();
  uint64_t& filtered = travCache.filteredDocuments();
  uint64_t& read = travCache.insertedDocuments();

  // TODO map id => ServerID if possible
  // And go fast-path
  transaction::BuilderLeaser leased(&trx);
  leased->openObject(true);
  leased->add("depth", VPackValue(depth));
  leased->add("keys", VPackValuePair(vertexId.data(), vertexId.length(),
                                     VPackValueType::String));
  leased->add(VPackValue("variables"));
  {
    leased->openArray();
    expressionContext.serializeAllVariables(trx.vpackOptions(),
                                            *(leased.get()));
    leased->close();
  }
  leased->close();

  auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  std::vector<Future<network::Response>> futures;
  futures.reserve(engines->size());

  for (auto const& engine : *engines) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + engine.first, fuerte::RestVerb::Put,
        absl::StrCat(::edgeUrl, engine.second), leased->bufferRef(), reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    auto res = r.combinedResult();
    if (res.fail()) {
      return res;
    }

    auto payload = r.response().stealPayload();
    VPackSlice resSlice(payload->data());
    if (!resSlice.isObject()) {
      // Response has invalid format
      return {TRI_ERROR_HTTP_CORRUPTED_JSON,
              "unexpected response structure for edges response"};
    }

    filtered += Helper::getNumericValue<size_t>(resSlice, "filtered", 0);
    read += Helper::getNumericValue<size_t>(resSlice, "readIndex", 0);

    VPackSlice edges = resSlice.get("edges");
    bool allCached = true;
    VPackArrayIterator allEdges(edges);
    // Reserve additional space for allEdges, they will
    // all be added within this function, the continue case
    // is only triggered on invalid network requests (unlikely)
    result.reserve(allEdges.size() + result.size());

    for (VPackSlice e : allEdges) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("a23b5", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }

      arangodb::velocypack::HashedStringRef idRef(id);
      auto resE = cache.try_emplace(idRef, e);
      if (resE.second) {
        // This edge is not yet cached.
        allCached = false;
        result.emplace_back(e);
      } else {
        result.emplace_back(resE.first->second);
      }
    }
    if (!allCached) {
      travCache.datalake().add(std::move(payload));
    }
  }
  return {};
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

Result fetchEdgesFromEngines(transaction::Methods& trx,
                             graph::ClusterTraverserCache& travCache,
                             VPackSlice vertexId, bool backward,
                             std::vector<VPackSlice>& result, uint64_t& read) {
  auto const* engines = travCache.engines();
  auto& cache = travCache.cache();
  // TODO map id => ServerID if possible
  // And go fast-path

  // This function works for one specific vertex
  // or for a list of vertices.
  TRI_ASSERT(vertexId.isString() || vertexId.isArray());
  transaction::BuilderLeaser leased(&trx);
  leased->openObject(true);
  leased->add("backward", VPackValue(backward));
  leased->add("keys", vertexId);
  leased->close();

  auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  std::vector<Future<network::Response>> futures;
  futures.reserve(engines->size());

  for (auto const& engine : *engines) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + engine.first, fuerte::RestVerb::Put,
        absl::StrCat(::edgeUrl, engine.second), leased->bufferRef(), reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    auto res = r.combinedResult();
    if (res.fail()) {
      return res;
    }

    auto payload = r.response().stealPayload();
    VPackSlice resSlice(payload->data());
    if (!resSlice.isObject()) {
      // Response has invalid format
      return {TRI_ERROR_HTTP_CORRUPTED_JSON,
              "invalid response structure for edges response"};
    }
    read += Helper::getNumericValue<size_t>(resSlice, "readIndex", 0);

    bool allCached = true;
    VPackSlice edges = resSlice.get("edges");
    for (VPackSlice e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("da49d", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }

      if (result.capacity() == 0) {
        result.reserve(16);
      }

      arangodb::velocypack::HashedStringRef idRef(id);
      auto resE = cache.try_emplace(idRef, e);
      if (resE.second) {
        // This edge is not yet cached.
        allCached = false;
        result.emplace_back(e);
      } else {
        result.emplace_back(resE.first->second);
      }
    }
    if (!allCached) {
      travCache.datalake().add(std::move(payload));
    }
  }
  return {};
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
    transaction::Methods& trx, graph::ClusterTraverserCache& travCache,
    std::unordered_set<arangodb::velocypack::HashedStringRef>& vertexIds,
    std::unordered_map<arangodb::velocypack::HashedStringRef, VPackSlice>&
        result,
    bool forShortestPath) {
  auto const* engines = travCache.engines();

  // TODO map id => ServerID if possible
  // And go fast-path

  // slow path, sharding not deducable from _id
  transaction::BuilderLeaser leased(&trx);
  leased->openObject();
  leased->add("keys", VPackValue(VPackValueType::Array));
  for (auto const& v : vertexIds) {
    leased->add(VPackValuePair(v.data(), v.length(), VPackValueType::String));
  }
  leased->close();  // 'keys' Array
  leased->close();  // base object

  auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  std::vector<Future<network::Response>> futures;
  futures.reserve(engines->size());

  for (auto const& engine : *engines) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + engine.first, fuerte::RestVerb::Put,
        absl::StrCat(::vertexUrl, engine.second), leased->bufferRef(),
        reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    auto res = r.combinedResult();
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    auto payload = r.response().stealPayload();
    VPackSlice resSlice(payload->data());
    if (!resSlice.isObject()) {
      // Response has invalid format
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_HTTP_CORRUPTED_JSON,
          "invalid response structure for vertices response");
    }
    bool cached = false;
    for (auto pair : VPackObjectIterator(resSlice, /*sequential*/ true)) {
      arangodb::velocypack::HashedStringRef key(pair.key);
      if (ADB_UNLIKELY(vertexIds.erase(key) == 0)) {
        // This case is unlikely and can only happen for
        // Satellite Vertex collections. There it is expected.
        // If we fix above todo (fast-path) this case should
        // be impossible.
        TRI_ASSERT(result.find(key) != result.end());
        TRI_ASSERT(VelocyPackHelper::equal(result.find(key)->second, pair.value,
                                           true));
      } else {
        TRI_ASSERT(result.find(key) == result.end());
        if (!cached) {
          travCache.datalake().add(std::move(payload));
          cached = true;
        }
        // Protected by datalake
        result.try_emplace(key, pair.value);
      }
    }
  }

  if (!forShortestPath) {
    // Fill everything we did not find with NULL
    for (auto const& v : vertexIds) {
      result.try_emplace(v, VPackSlice::nullSlice());
    }
    vertexIds.clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

futures::Future<OperationResult> modifyDocumentOnCoordinator(
    transaction::Methods& trx, LogicalCollection& coll,
    arangodb::velocypack::Slice const& slice, OperationOptions const& options,
    bool isPatch, transaction::MethodsApi api) {
  // Set a few variables needed for our work:

  // First determine the collection ID from the name:
  std::shared_ptr<ShardMap> shardIds = coll.shardIds();

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

  CrudOperationCtx opCtx;
  opCtx.options = options;
  bool useMultiple = slice.isArray();

  bool canUseFastPath = true;
  if (useMultiple) {
    for (VPackSlice value : VPackArrayIterator(slice)) {
      auto res = distributeBabyOnShards(opCtx, coll, value);
      if (res != TRI_ERROR_NO_ERROR) {
        if (!isPatch) {  // shard keys cannot be changed, error out early
          return makeFuture(OperationResult(res, options));
        }
        canUseFastPath = false;
        break;
      }
    }
  } else {
    auto res = distributeBabyOnShards(opCtx, coll, slice);
    if (res != TRI_ERROR_NO_ERROR) {
      if (!isPatch) {  // shard keys cannot be changed, error out early
        return makeFuture(OperationResult(res, options));
      }
      canUseFastPath = false;
    }
  }

  // Some stuff to prepare cluster-internal requests:

  network::RequestOptions reqOpts;
  reqOpts.database = trx.vocbase().name();
  reqOpts.timeout = network::Timeout(CL_DEFAULT_LONG_TIMEOUT);
  reqOpts.retryNotFound = true;
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;
  reqOpts
      .param(StaticStrings::WaitForSyncString,
             (options.waitForSync ? "true" : "false"))
      .param(StaticStrings::IgnoreRevsString,
             (options.ignoreRevs ? "true" : "false"))
      .param(StaticStrings::SkipDocumentValidation,
             (options.validate ? "false" : "true"))
      .param(StaticStrings::IsRestoreString,
             (options.isRestore ? "true" : "false"));

  // note: the "silent" flag is not forwarded to the leader by the
  // coordinator. the coordinator handles the "silent" flag on its own.

  if (options.refillIndexCaches != RefillIndexCaches::kDefault) {
    // this attribute can have 3 values: default, true and false. only
    // expose it when it is not set to "default"
    reqOpts.param(StaticStrings::RefillIndexCachesString,
                  (options.refillIndexCaches == RefillIndexCaches::kRefill)
                      ? "true"
                      : "false");
  }
  if (!options.versionAttribute.empty()) {
    reqOpts.param(StaticStrings::VersionAttributeString,
                  options.versionAttribute);
  }

  fuerte::RestVerb restVerb;
  if (isPatch) {
    restVerb = fuerte::RestVerb::Patch;
    if (!options.keepNull) {
      reqOpts.param(StaticStrings::KeepNullString, "false");
    }
    reqOpts.param(StaticStrings::MergeObjectsString,
                  (options.mergeObjects ? "true" : "false"));
  } else {
    restVerb = fuerte::RestVerb::Put;
  }
  if (options.returnNew) {
    reqOpts.param(StaticStrings::ReturnNewString, "true");
  }
  if (options.returnOld) {
    reqOpts.param(StaticStrings::ReturnOldString, "true");
  }

  network::addUserParameter(reqOpts, trx.username());

  bool isManaged =
      trx.state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    Future<Result> f = makeFuture(Result());
    if (isManaged &&
        opCtx.shardMap.size() > 1) {  // lazily begin transactions on leaders
      f = beginTransactionOnSomeLeaders(*trx.state(), coll, opCtx.shardMap,
                                        api);
    }

    return std::move(f).thenValue([=, &trx,
                                   opCtx(std::move(opCtx))](Result&& r) mutable
                                  -> Future<OperationResult> {
      if (r.fail()) {  // bail out
        return OperationResult(r, opCtx.options);
      }

      // Now prepare the requests:
      auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
      std::vector<Future<network::Response>> futures;
      futures.reserve(opCtx.shardMap.size());

      for (auto const& it : opCtx.shardMap) {
        std::string url;
        VPackBuffer<uint8_t> buffer;

        if (!useMultiple) {
          TRI_ASSERT(it.second.size() == 1);
          TRI_ASSERT(slice.isObject());
          std::string_view const ref(
              slice.get(StaticStrings::KeyString).stringView());
          // We send to single endpoint
          url = absl::StrCat("/_api/document/", std::string{it.first}, "/",
                             StringUtils::urlEncode(ref.data(), ref.size()));
          buffer.append(slice.begin(), slice.byteSize());

        } else {
          // We send to Babies endpoint
          url = absl::StrCat("/_api/document/", std::string{it.first});

          VPackBuilder builder(buffer);
          builder.clear();
          builder.openArray(/*unindexed*/ true);
          for (auto const& value : it.second) {
            builder.add(value);
          }
          builder.close();
        }

        network::Headers headers;
        // Just make sure that no dirty read flag makes it here, since we
        // are writing and then `addTransactionHeaderForShard` might
        // misbehave!
        TRI_ASSERT(!trx.state()->options().allowDirtyReads);
        addTransactionHeaderForShard(trx, *shardIds, /*shard*/ it.first,
                                     headers);
        futures.emplace_back(network::sendRequestRetry(
            pool, "shard:" + it.first, restVerb, std::move(url),
            std::move(buffer), reqOpts, std::move(headers)));
      }

      // Now listen to the results:
      if (!useMultiple) {
        TRI_ASSERT(futures.size() == 1);
        auto cb = [options](network::Response&& res) -> OperationResult {
          if (res.error != fuerte::Error::NoError) {
            return OperationResult(network::fuerteToArangoErrorCode(res),
                                   options);
          }
          return network::clusterResultModify(res.statusCode(),
                                              res.response().stealPayload(),
                                              std::move(options), {});
        };
        return std::move(futures[0]).thenValue(cb);
      }

      return futures::collectAll(std::move(futures))
          .thenValue([opCtx = std::move(opCtx)](
                         std::vector<Try<network::Response>>&& results)
                         -> OperationResult {
            return handleCRUDShardResponsesFast(network::clusterResultModify,
                                                opCtx, results);
          });
    });
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  Future<Result> f = makeFuture(Result());
  if (isManaged && shardIds->size() > 1) {  // lazily begin the transaction
    f = ::beginTransactionOnAllLeaders(trx, *shardIds, api);
  }

  return std::move(f).thenValue([=, &trx](Result&&) mutable
                                -> Future<OperationResult> {
    auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
    std::vector<Future<network::Response>> futures;
    futures.reserve(shardIds->size());

    size_t expectedLen = useMultiple ? slice.length() : 0;
    VPackBuffer<uint8_t> buffer;
    buffer.append(slice.begin(), slice.byteSize());

    for (auto const& shardServers : *shardIds) {
      ShardID const& shard = shardServers.first;
      network::Headers headers;
      // Just make sure that no dirty read flag makes it here, since we
      // are writing and then `addTransactionHeaderForShard` might
      // misbehave!
      TRI_ASSERT(!trx.state()->options().allowDirtyReads);
      addTransactionHeaderForShard(trx, *shardIds, shard, headers);

      std::string url;
      if (!useMultiple) {
        // send to single document API
        std::string_view key(slice.get(StaticStrings::KeyString).stringView());
        url = absl::StrCat("/_api/document/", std::string{shard}, "/",
                           StringUtils::urlEncode(key.data(), key.size()));
      } else {
        // send to batch API
        url = absl::StrCat("/_api/document/", std::string{shard});
      }
      futures.emplace_back(network::sendRequestRetry(
          pool, "shard:" + shard, restVerb, std::move(url),
          /*cannot move*/ buffer, reqOpts, std::move(headers)));
    }

    return futures::collectAll(std::move(futures))
        .thenValue([=](std::vector<Try<network::Response>>&& responses) mutable
                   -> OperationResult {
          return ::handleCRUDShardResponsesSlow(network::clusterResultModify,
                                                expectedLen, std::move(options),
                                                responses);
        });
  });
}

/// @brief flush WAL on all DBservers
Result flushWalOnAllDBServers(ClusterFeature& feature, bool waitForSync,
                              bool flushColumnFamilies) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue
  reqOpts
      .param(StaticStrings::WaitForSyncString, (waitForSync ? "true" : "false"))
      .param("waitForCollector", (flushColumnFamilies ? "true" : "false"));

  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  VPackBufferUInt8 buffer;
  buffer.append(VPackSlice::noneSlice().begin(),
                1);  // necessary for some reason
  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Put, "/_admin/wal/flush",
        buffer, reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    Result res = f.waitAndGet().combinedResult();
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

// recalculate counts on all DB servers
Result recalculateCountsOnAllDBServers(ClusterFeature& feature,
                                       std::string_view dbname,
                                       std::string_view collname) {
  // Set a few variables needed for our work:
  NetworkFeature const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  auto collinfo = ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  std::string const baseUrl = "/_api/collection/";

  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.add(VPackSlice::emptyObjectSlice());

  network::Headers headers;
  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(600.0);

  // now we notify all leader and follower shards
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<network::FutureRes> futures;
  for (auto const& shard : *shardList) {
    for (ServerID const& serverId : shard.second) {
      std::string uri = baseUrl + shard.first + "/recalculateCount";
      auto f = network::sendRequestRetry(pool, "server:" + serverId,
                                         fuerte::RestVerb::Put, std::move(uri),
                                         body, options, headers);
      futures.emplace_back(std::move(f));
    }
  }

  auto responses = futures::collectAll(futures).waitAndGet();
  for (auto const& r : responses) {
    Result res = r.get().combinedResult();
    if (res.fail()) {
      return res;
    }
  }

  return {};
}

/// @brief compact the entire dataset on all DB servers
Result compactOnAllDBServers(ClusterFeature& feature, bool changeLevel,
                             bool compactBottomMostLevel) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(3600);
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue
  reqOpts.param("changeLevel", (changeLevel ? "true" : "false"))
      .param("compactBottomMostLevel",
             (compactBottomMostLevel ? "true" : "false"));

  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  VPackBufferUInt8 buffer;
  VPackSlice s = VPackSlice::emptyObjectSlice();
  buffer.append(s.start(), s.byteSize());
  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Put, "/_admin/compact",
        buffer, reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    Result res = f.waitAndGet().combinedResult();
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

/// @brief compact the data of a single collection on all DB servers
Result compactOnAllDBServers(ClusterFeature& feature, std::string const& dbname,
                             std::string const& collname) {
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  auto collinfo = ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  std::string const baseUrl = "/_api/collection/";

  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.add(VPackSlice::emptyObjectSlice());

  network::Headers headers;
  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(3600.0);

  // now we notify all leader and follower shards
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<network::FutureRes> futures;
  for (auto const& shard : *shardList) {
    for (ServerID const& serverId : shard.second) {
      std::string uri =
          absl::StrCat(baseUrl, std::string{shard.first}, "/compact");
      auto f = network::sendRequestRetry(pool, "server:" + serverId,
                                         fuerte::RestVerb::Put, std::move(uri),
                                         body, options, headers);
      futures.emplace_back(std::move(f));
    }
  }

  for (Future<network::Response>& f : futures) {
    Result res = f.waitAndGet().combinedResult();
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

std::string const apiStr("/_admin/backup/");

arangodb::Result hotBackupList(
    network::ConnectionPool* pool, std::vector<ServerID> const& dbServers,
    VPackSlice const idSlice,
    std::unordered_map<std::string, BackupMeta>& hotBackups,
    VPackBuilder& plan) {
  TRI_ASSERT(pool);
  hotBackups.clear();
  TRI_ASSERT(idSlice.isArray() || idSlice.isString() || idSlice.isNone());

  std::map<std::string, std::vector<BackupMeta>> dbsBackups;

  VPackBufferUInt8 body;
  VPackBuilder b(body);
  b.openObject();
  if (!idSlice.isNone()) {
    b.add("id", idSlice);
  }
  b.close();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::string const url = apiStr + "list";

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbServers.size());
  for (auto const& dbServer : dbServers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  size_t nrGood = 0;
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();
    if (!r.ok()) {
      continue;
    }
    if (r.response().checkStatus({fuerte::StatusOK, fuerte::StatusCreated,
                                  fuerte::StatusAccepted,
                                  fuerte::StatusNoContent})) {
      nrGood++;
    }
  }

  LOG_TOPIC("410a1", DEBUG, Logger::BACKUP)
      << "Got " << nrGood << " of " << futures.size()
      << " lists of local backups";

  // Any error if no id presented
  if (idSlice.isNone() && nrGood < futures.size()) {
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_DBSERVERS_AWOL,
        std::string("not all db servers could be reached for backup listing"));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();
    if (!r.ok()) {
      continue;
    }
    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                              std::string("result to list request to ") +
                                  r.destination + " not an object");
    }

    if (resSlice.get(StaticStrings::Error).getBoolean()) {
      auto res =
          ::ErrorCode{resSlice.get(StaticStrings::ErrorNum).getNumber<int>()};
      return arangodb::Result(
          res, resSlice.get(StaticStrings::ErrorMessage).copyString());
    }

    if (!resSlice.hasKey("result") || !resSlice.get("result").isObject()) {
      return arangodb::Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                              std::string("invalid response ") +
                                  resSlice.toJson() + "from " + r.destination);
    }

    resSlice = resSlice.get("result");

    if (!resSlice.hasKey("list") || !resSlice.get("list").isObject()) {
      continue;
    }

    if (!idSlice.isNone() && plan.slice().isNone()) {
      if (!resSlice.hasKey("agency-dump") ||
          !resSlice.get("agency-dump").isArray() ||
          resSlice.get("agency-dump").length() != 1) {
        return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,
                                std::string("result ") + resSlice.toJson() +
                                    " is missing agency dump");
      }
      plan.add(resSlice.get("agency-dump")[0]);
    }

    for (auto backup : VPackObjectIterator(resSlice.get("list"))) {
      ResultT<BackupMeta> meta = BackupMeta::fromSlice(backup.value);
      if (meta.ok()) {
        dbsBackups[backup.key.copyString()].push_back(std::move(meta.get()));
      }
    }
  }

  for (auto& i : dbsBackups) {
    // check if the backup is on all dbservers
    bool valid = true;

    // check here that the backups are all made with the same version
    std::string version;
    size_t totalSize = 0;
    size_t totalFiles = 0;

    for (BackupMeta const& meta : i.second) {
      if (version.empty()) {
        version = meta._version;
      } else {
        if (version != meta._version) {
          LOG_TOPIC("aaaaa", WARN, Logger::BACKUP)
              << "Backup " << meta._id
              << " has different versions accross dbservers: " << version
              << " and " << meta._version;
          valid = false;
          break;
        }
      }
      totalSize += meta._sizeInBytes;
      totalFiles += meta._nrFiles;
    }

    if (valid) {  // backup is identical on all servers
      BackupMeta& front = i.second.front();
      front._sizeInBytes = totalSize;
      front._nrFiles = totalFiles;
      front._serverId = "";  // makes no sense for whole cluster
      front._isAvailable = i.second.size() == dbServers.size() &&
                           i.second.size() == front._nrDBServers;
      front._nrPiecesPresent = static_cast<unsigned int>(i.second.size());
      hotBackups.insert(std::make_pair(front._id, front));
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
                                    std::map<ServerID, ServerID>& match) {
  std::vector<std::string> ap{"arango", "Plan", "DBServers"};

  if (!agencyDump.hasKey(ap)) {
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "agency dump must contain key DBServers");
  }
  auto planServers = agencyDump.get(ap);

  return matchBackupServersSlice(planServers, dbServers, match);
}

arangodb::Result matchBackupServersSlice(VPackSlice const planServers,
                                         std::vector<ServerID> const& dbServers,
                                         std::map<ServerID, ServerID>& match) {
  // LOG_TOPIC("711d8", DEBUG, Logger::BACKUP) << "matching db servers between
  // snapshot: " <<
  //  planServers.toJson() << " and this cluster's db servers " << dbServers;

  if (!planServers.isObject()) {
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "agency dump's arango.Plan.DBServers must be object");
  }

  if (dbServers.size() < planServers.length()) {
    return Result(TRI_ERROR_BACKUP_TOPOLOGY,
                  std::string("number of db servers in the backup (") +
                      std::to_string(planServers.length()) +
                      ") and in this cluster (" +
                      std::to_string(dbServers.size()) + ") do not match");
  }

  // Clear match container
  match.clear();

  // Local copy of my servers
  // Note that we use a `std::set` here for the following reasons:
  //  - this function is supposed to return a canonical result, which
  //    does not depend on the order of the input in `planServers` or
  //    `dbServers`, therefore we use a sorted set here and a sorted
  //    map as the result.
  //  - Performance does not matter at all, this method is called for
  //    hotbackup download and hotbackup restore, both methods are
  //    using considerable amounts of time. The server lists are usually
  //    quite small (3 or 5 or a few dozen at most).
  // So please do not "optimize" this.
  std::set<std::string> localCopy;
  std::copy(dbServers.begin(), dbServers.end(),
            std::inserter(localCopy, localCopy.end()));
  // dbServers should be a list of pairwise different servers, this
  // is usually ensured since it is a vector manufactured from the keys
  // of a map:
  TRI_ASSERT(localCopy.size() == dbServers.size());

  // Skip all direct matching names in pair and remove them from localCopy.
  // If later a server does not occur it means that no translation has to
  // happen for it.
  for (auto planned : VPackObjectIterator(planServers)) {
    auto const plannedStr = planned.key.copyString();
    auto it = localCopy.find(plannedStr);
    if (it != localCopy.end()) {
      localCopy.erase(it);
    } else {
      match.try_emplace(plannedStr, std::string());
    }
  }
  // At this stage, we know the following:
  //  - initially, dbServers had at least as many servers as planServers
  //  - initially, localCopy was a perfect copy of dbServers
  //  - now, for every element of planServers, which is not contained in
  //    localCopy, we put an element in match, each element of planServers,
  //    which is in localCopy, is removed from localCopy and no entry is
  //    added to match.
  // Therefore, localCopy has at least as many entries as match and we can
  // just blindly run the iterator:
  TRI_ASSERT(match.size() <= localCopy.size());
  auto it2 = localCopy.begin();
  for (auto& m : match) {  // alphabetical order!
    m.second = *it2++;     // alphabetical order, too!
  }

  LOG_TOPIC("a201e", DEBUG, Logger::BACKUP) << "DB server matches: " << match;

  return arangodb::Result();
}

arangodb::Result controlMaintenanceFeature(
    network::ConnectionPool* pool, std::string const& command,
    std::string const& backupId, std::vector<ServerID> const& dbServers) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder b(&builder);
    builder.add("execute", VPackValue(command));
    builder.add("reason", VPackValue("backup"));
    builder.add("duration", VPackValue(30));
    builder.add("id", VPackValue(backupId));
  }

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbServers.size());
  std::string const url = "/_admin/actions";

  for (auto const& dbServer : dbServers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("3d080", DEBUG, Logger::BACKUP)
      << "Attempting to execute " << command
      << " maintenance features for hot backup id " << backupId << " using "
      << builder.toJson();

  // Now listen to the results:
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          absl::StrCat("Communication error while executing ", command,
                       " maintenance on ", r.destination, ": ",
                       r.combinedResult().errorMessage()));
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject() || !resSlice.hasKey(StaticStrings::Error) ||
        !resSlice.get(StaticStrings::Error).isBoolean()) {
      // Response has invalid format
      return arangodb::Result(
          TRI_ERROR_HTTP_CORRUPTED_JSON,
          absl::StrCat("result of executing ", command,
                       " request to maintenance feature on ", r.destination,
                       " is invalid"));
    }

    if (resSlice.get(StaticStrings::Error).getBoolean()) {
      return arangodb::Result(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          absl::StrCat("failed to execute ", command,
                       " on maintenance feature for ", backupId, " on server ",
                       r.destination));
    }

    LOG_TOPIC("d7e7c", DEBUG, Logger::BACKUP)
        << "maintenance is paused on " << r.destination;
  }

  return arangodb::Result();
}

arangodb::Result restoreOnDBServers(network::ConnectionPool* pool,
                                    std::string const& backupId,
                                    std::vector<std::string> const& dbServers,
                                    std::string& previous, bool ignoreVersion) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder o(&builder);
    builder.add("id", VPackValue(backupId));
    builder.add("ignoreVersion", VPackValue(ignoreVersion));
  }

  std::string const url = apiStr + "restore";

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbServers.size());

  for (auto const& dbServer : dbServers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("37960", DEBUG, Logger::BACKUP) << "Restoring backup " << backupId;

  // Now listen to the results:
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      // oh-oh cluster is in a bad state
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          absl::StrCat("Communication error list backups on ", r.destination,
                       ": ", r.combinedResult().errorMessage()));
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                              absl::StrCat("result to restore request ",
                                           r.destination, "not an object"));
    }

    if (!resSlice.hasKey(StaticStrings::Error) ||
        !resSlice.get(StaticStrings::Error).isBoolean() ||
        resSlice.get(StaticStrings::Error).getBoolean()) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              std::string("failed to restore ") + backupId +
                                  " on server " + r.destination + ": " +
                                  resSlice.toJson());
    }

    if (!resSlice.hasKey("result") || !resSlice.get("result").isObject()) {
      return arangodb::Result(
          TRI_ERROR_HOT_RESTORE_INTERNAL,
          std::string("failed to restore ") + backupId + " on server " +
              r.destination +
              " as response is missing result object: " + resSlice.toJson());
    }

    auto result = resSlice.get("result");

    if (!result.hasKey("previous") || !result.get("previous").isString()) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              std::string("failed to restore ") + backupId +
                                  " on server " + r.destination);
    }

    previous = result.get("previous").copyString();
    LOG_TOPIC("9a5c4", DEBUG, Logger::BACKUP)
        << "received failsafe name " << previous << " from db server "
        << r.destination;
  }

  LOG_TOPIC("755a2", DEBUG, Logger::BACKUP)
      << "Restored " << backupId << " successfully";

  return arangodb::Result();
}

arangodb::Result applyDBServerMatchesToPlan(
    VPackSlice const plan, std::map<ServerID, ServerID> const& matches,
    VPackBuilder& newPlan) {
  std::function<void(VPackSlice const, std::map<ServerID, ServerID> const&,
                     bool)>
      replaceDBServer;
  /*
   * This recursive function replaces all occurences of DBServer names with
   * their handed replacement map. In Replication2 also remove the currentTerm
   * entry, to enforce leader election.
   */
  replaceDBServer = [&newPlan, &replaceDBServer](
                        VPackSlice const s,
                        std::map<ServerID, ServerID> const& matches,
                        bool inReplicatedLogs) {
    if (s.isObject()) {
      VPackObjectBuilder o(&newPlan);
      for (auto it : VPackObjectIterator(s)) {
        newPlan.add(it.key);
        if (it.key.isEqualString("ReplicatedLogs")) {
          replaceDBServer(it.value, matches, true);
        } else if (inReplicatedLogs && it.key.isEqualString("currentTerm")) {
          newPlan.add(VPackSlice::emptyObjectSlice());
        } else {
          replaceDBServer(it.value, matches, inReplicatedLogs);
        }
      }
    } else if (s.isArray()) {
      VPackArrayBuilder a(&newPlan);
      for (VPackSlice it : VPackArrayIterator(s)) {
        replaceDBServer(it, matches, inReplicatedLogs);
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

  replaceDBServer(plan, matches, false);

  return arangodb::Result();
}

arangodb::Result hotRestoreCoordinator(ClusterFeature& feature,
                                       VPackSlice const payload,
                                       VPackBuilder& report) {
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

  VPackSlice idSlice;
  if (!payload.isObject() || !(idSlice = payload.get("id")).isString()) {
    events::RestoreHotbackup("", TRI_ERROR_BAD_PARAMETER);
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        "restore payload must be an object with string attribute 'id'");
  }
  TRI_ASSERT(idSlice.isString());

  bool ignoreVersion =
      payload.hasKey("ignoreVersion") && payload.get("ignoreVersion").isTrue();

  std::string const backupId = idSlice.copyString();
  VPackBuilder plan;
  ClusterInfo& ci = feature.clusterInfo();

  auto const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    // shutdown, leave here
    return TRI_ERROR_SHUTTING_DOWN;
  }

  std::vector<ServerID> dbServers = ci.getCurrentDBServers();
  std::unordered_map<std::string, BackupMeta> list;

  auto result = hotBackupList(pool, dbServers, idSlice, list, plan);
  if (!result.ok()) {
    LOG_TOPIC("ed4dd", ERR, Logger::BACKUP)
        << "failed to find backup " << backupId
        << " on all db servers: " << result.errorMessage();
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }
  if (list.empty()) {
    events::RestoreHotbackup(backupId, TRI_ERROR_HTTP_NOT_FOUND);
    return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,
                            "result is missing backup list");
  }

  if (plan.slice().isNone()) {
    LOG_TOPIC("54b9a", ERR, Logger::BACKUP)
        << "failed to find agency dump for " << backupId
        << " on any db server: " << result.errorMessage();
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  TRI_ASSERT(list.size() == 1);
  BackupMeta& meta = list.begin()->second;
  if (!meta._isAvailable) {
    LOG_TOPIC("ed4df", ERR, Logger::BACKUP)
        << "backup not available" << backupId;
    events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
    return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                            "backup not available for restore");
  }

  // Check if the version matches the current version
  if (!ignoreVersion) {
    TRI_ASSERT(list.size() == 1);
    using arangodb::methods::Version;
    using arangodb::methods::VersionResult;
#ifdef USE_ENTERPRISE
    // Will never be called in Community Edition
    bool autoUpgradeNeeded;  // not actually used
    if (!RocksDBHotBackup::versionTestRestore(meta._version,
                                              autoUpgradeNeeded)) {
      events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Version mismatch");
    }
#endif
  }

  // Match my db servers to those in the backups's agency dump
  std::map<ServerID, ServerID> matches;
  result = matchBackupServers(plan.slice(), dbServers, matches);
  if (!result.ok()) {
    LOG_TOPIC("5a746", ERR, Logger::BACKUP)
        << "failed to match db servers: " << result.errorMessage();
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // Apply matched servers to create new plan, if any matches to be done,
  // else just take
  VPackBuilder newPlan;
  if (!matches.empty()) {
    result = applyDBServerMatchesToPlan(plan.slice(), matches, newPlan);
    if (!result.ok()) {
      events::RestoreHotbackup(backupId, result.errorNumber());
      return result;
    }
  }

  // Pause maintenance feature everywhere, fail, if not succeeded everywhere
  result = controlMaintenanceFeature(pool, "pause", backupId, dbServers);
  if (!result.ok()) {
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // Enact new plan upon the agency
  result = (matches.empty()) ? ci.agencyReplan(plan.slice())
                             : ci.agencyReplan(newPlan.slice());
  if (!result.ok()) {
    // We ignore the result of the Proceed here.
    // In case one of the servers does not proceed now, it will automatically
    // reactivate maintenance after 30s.
    std::ignore =
        controlMaintenanceFeature(pool, "proceed", backupId, dbServers);
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // Now I will have to wait for the plan to trickle down
  std::this_thread::sleep_for(std::chrono::seconds(5));

  // We keep the currently registered timestamps in Current/ServersRegistered,
  // such that we can wait until all have reregistered and are up:

  ci.loadCurrentDBServers();
  auto const preServersKnown = ci.rebootIds();

  // Restore all db servers
  std::string previous;
  result =
      restoreOnDBServers(pool, backupId, dbServers, previous, ignoreVersion);
  if (!result.ok()) {  // This is disaster!
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // no need to keep connections to shut-down servers, they auto close when
  // unused
  if (pool) {
    pool->drainConnections();
  }

  auto startTime = std::chrono::steady_clock::now();
  while (true) {  // will be left by a timeout
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (feature.server().isStopping()) {
      events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Shutdown of coordinator!");
    }
    if (std::chrono::steady_clock::now() - startTime >
        std::chrono::minutes(15)) {
      events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Not all DBservers came back in time!");
    }
    ci.loadCurrentDBServers();
    auto const postServersKnown = ci.rebootIds();
    if (ci.getCurrentDBServers().size() < dbServers.size()) {
      LOG_TOPIC("8dce7", INFO, Logger::BACKUP)
          << "Waiting for all db servers to return";
      continue;
    }

    // Check timestamps of all dbservers:
    size_t good = 0;  // Count restarted servers
    for (auto const& dbs : dbServers) {
      if (postServersKnown.at(dbs).rebootId !=
          preServersKnown.at(dbs).rebootId) {
        ++good;
      }
    }
    LOG_TOPIC("8dc7e", INFO, Logger::BACKUP)
        << "Backup restore: So far " << good << "/" << dbServers.size()
        << " dbServers have reregistered.";
    if (good >= dbServers.size()) {
      break;
    }
  }

  // and Wait for Shards to decide on a leader
  ci.syncWaitForAllShardsToEstablishALeader();

  {
    VPackObjectBuilder o(&report);
    report.add("previous", VPackValue(previous));
    report.add("isCluster", VPackValue(true));
  }
  events::RestoreHotbackup(backupId, TRI_ERROR_NO_ERROR);
  return arangodb::Result();
}

std::vector<std::string> lockPath =
    std::vector<std::string>{"result", "lockId"};

arangodb::Result lockServersTrxCommit(network::ConnectionPool* pool,
                                      std::string const& backupId,
                                      std::vector<ServerID> const& servers,
                                      double lockWait,
                                      std::vector<ServerID>& lockedServers) {
  // Make sure all servers have the backup with backup Id

  std::string const url = apiStr + "lock";

  VPackBufferUInt8 body;
  VPackBuilder lock(body);
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
    lock.add("timeout", VPackValue(lockWait));
    // unlock timeout for commit lock on coordinator
    lock.add("unlockTimeout", VPackValue(30.0 + lockWait));
  }

  LOG_TOPIC("707ed", DEBUG, Logger::BACKUP)
      << "Trying to acquire global transaction locks using body "
      << lock.toJson();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(lockWait + 5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& server : servers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Post, url, body, reqOpts));
  }

  // Now listen to the results and report the aggregated final result:
  arangodb::Result finalRes(TRI_ERROR_NO_ERROR);
  auto reportError = [&](::ErrorCode c, std::string const& m) {
    if (finalRes.ok()) {
      finalRes = arangodb::Result(c, m);
    } else {
      // If we see at least one TRI_ERROR_LOCAL_LOCK_FAILED it is a failure
      // if all errors are TRI_ERROR_LOCK_TIMEOUT, then we report this and
      // this will lead to a retry:
      if (finalRes.is(TRI_ERROR_LOCAL_LOCK_FAILED)) {
        c = TRI_ERROR_LOCAL_LOCK_FAILED;
      }
      finalRes =
          arangodb::Result(c, absl::StrCat(finalRes.errorMessage(), ", ", m));
    }
  };
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("Communication error locking transactions on ",
                       r.destination, ": ", r.combinedResult().errorMessage()));
      continue;
    }
    VPackSlice slc = r.slice();

    if (!slc.isObject() || !slc.hasKey(StaticStrings::Error) ||
        !slc.get(StaticStrings::Error).isBoolean()) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("invalid response from ", r.destination,
                       " when trying to freeze transactions for hot backup ",
                       backupId, ": ", slc.toJson()));
      continue;
    }

    if (slc.get(StaticStrings::Error).getBoolean()) {
      LOG_TOPIC("f4b8f", DEBUG, Logger::BACKUP)
          << "failed to acquire lock from " << r.destination << ": "
          << slc.toJson();
      auto errorNum = slc.get(StaticStrings::ErrorNum).getNumber<int>();
      auto err = ::ErrorCode{errorNum};
      if (err == TRI_ERROR_LOCK_TIMEOUT) {
        reportError(err, slc.get(StaticStrings::ErrorMessage).copyString());
        continue;
      }
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("lock was denied from ", r.destination,
                       " when trying to check for lockId for hot backup ",
                       backupId, ": ", slc.toJson()));
      continue;
    }

    if (!slc.hasKey(lockPath) || !slc.get(lockPath).isNumber() ||
        !slc.hasKey("result") || !slc.get("result").isObject()) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("invalid response from ", r.destination,
                       " when trying to check for lockId for hot backup ",
                       backupId, ": ", slc.toJson()));
      continue;
    }

    uint64_t lockId = 0;
    try {
      lockId = slc.get(lockPath).getNumber<uint64_t>();
      LOG_TOPIC("14457", DEBUG, Logger::BACKUP)
          << "acquired lock from " << r.destination << " for backupId "
          << backupId << " with lockId " << lockId;
    } catch (std::exception const& e) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("invalid response from ", r.destination,
                       " when trying to get lockId for hot backup ", backupId,
                       ": ", slc.toJson(), ", msg: ", e.what()));
      continue;
    }

    lockedServers.push_back(
        r.destination.substr(strlen("server:"), std::string::npos));
  }

  if (finalRes.ok()) {
    LOG_TOPIC("c1869", DEBUG, Logger::BACKUP)
        << "acquired transaction locks on all coordinators";
  } else {
    LOG_TOPIC("8226a", DEBUG, Logger::BACKUP)
        << "unable to acquire transaction locks on all coordinators: "
        << finalRes.errorMessage();
  }

  return finalRes;
}

arangodb::Result unlockServersTrxCommit(
    network::ConnectionPool* pool, std::string const& backupId,
    std::vector<ServerID> const& lockedServers) {
  LOG_TOPIC("2ba8f", DEBUG, Logger::BACKUP)
      << "best effort attempt to kill all locks on coordinators "
      << lockedServers;

  // Make sure all servers have the backup with backup Id

  std::string const url = apiStr + "unlock";

  VPackBufferUInt8 body;
  VPackBuilder lock(body);
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
  }

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(lockedServers.size());

  for (auto const& server : lockedServers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Post, url, body, reqOpts));
  }

  auto responses = futures::collectAll(std::move(futures)).waitAndGet();

  Result res;
  for (auto const& tryRes : responses) {
    network::Response const& r = tryRes.get();
    if (r.combinedResult().fail() && res.ok()) {
      res = r.combinedResult();
    }
  }

  LOG_TOPIC("48510", DEBUG, Logger::BACKUP)
      << "killing all locks on coordinators resulted in: "
      << res.errorMessage();

  // return value is ignored by callers, but we'll return our status
  // anyway.
  return res;
}

std::vector<std::string> idPath{"result", "id"};

arangodb::Result hotBackupDBServers(network::ConnectionPool* pool,
                                    std::string const& backupId,
                                    std::string const& timeStamp,
                                    std::vector<ServerID> servers,
                                    VPackSlice agencyDump, bool force,
                                    BackupMeta& meta) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder b(&builder);
    builder.add("label", VPackValue(backupId));
    builder.add("agency-dump", agencyDump);
    builder.add("timestamp", VPackValue(timeStamp));
    builder.add("allowInconsistent", VPackValue(force));
    builder.add("nrDBServers", VPackValue(servers.size()));
  }

  std::string const url = apiStr + "create";

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& dbServer : servers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("478ef", DEBUG, Logger::BACKUP)
      << "Inquiring about backup " << backupId;

  // Now listen to the results:
  size_t totalSize = 0;
  size_t totalFiles = 0;
  std::vector<std::string> secretHashes;
  std::string version;
  bool sizeValid = true;
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          std::string("Communication error list backups on ") + r.destination);
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject() || !resSlice.hasKey("result")) {
      // Response has invalid format
      return arangodb::Result(
          TRI_ERROR_HTTP_CORRUPTED_JSON,
          std::string("result to take snapshot on ") + r.destination +
              " not an object or has no 'result' attribute: " +
              resSlice.toJson());
    }
    resSlice = resSlice.get("result");

    // TODO: shouldn't the id be checked ?
    VPackSlice value = resSlice.get(BackupMeta::ID);
    if (!value.isString()) {
      LOG_TOPIC("6240a", ERR, Logger::BACKUP)
          << "DB server " << r.destination << "is missing backup " << backupId;
      return arangodb::Result(TRI_ERROR_FILE_NOT_FOUND,
                              std::string("no backup with id ") + backupId +
                                  " on server " + r.destination);
    }

    value = resSlice.get(BackupMeta::SECRETHASH);
    if (value.isArray()) {
      for (VPackSlice hash : VPackArrayIterator(value)) {
        if (hash.isString()) {
          secretHashes.push_back(hash.copyString());
        }
      }
    }

    if (resSlice.hasKey(BackupMeta::SIZEINBYTES)) {
      totalSize +=
          Helper::getNumericValue<size_t>(resSlice, BackupMeta::SIZEINBYTES, 0);
    } else {
      sizeValid = false;
    }
    if (resSlice.hasKey(BackupMeta::NRFILES)) {
      totalFiles +=
          Helper::getNumericValue<size_t>(resSlice, BackupMeta::NRFILES, 0);
    } else {
      sizeValid = false;
    }
    if (version.empty() && resSlice.hasKey(BackupMeta::VERSION)) {
      VPackSlice verSlice = resSlice.get(BackupMeta::VERSION);
      if (verSlice.isString()) {
        version = verSlice.copyString();
      }
    }

    LOG_TOPIC("b370d", DEBUG, Logger::BACKUP)
        << r.destination << " created local backup "
        << resSlice.get(BackupMeta::ID).stringView();
  }

  // remove duplicate hashes
  std::sort(secretHashes.begin(), secretHashes.end());
  secretHashes.erase(std::unique(secretHashes.begin(), secretHashes.end()),
                     secretHashes.end());

  if (sizeValid) {
    meta = BackupMeta(backupId, version, timeStamp, secretHashes, totalSize,
                      totalFiles, static_cast<unsigned int>(servers.size()), "",
                      force);
  } else {
    meta = BackupMeta(backupId, version, timeStamp, secretHashes, 0, 0,
                      static_cast<unsigned int>(servers.size()), "", force);
    LOG_TOPIC("54265", WARN, Logger::BACKUP)
        << "Could not determine total size of backup with id '" << backupId
        << "'!";
  }
  LOG_TOPIC("5c5e9", DEBUG, Logger::BACKUP)
      << "Have created backup " << backupId;

  return arangodb::Result();
}

/**
 * @brief delete all backups with backupId from the db servers
 */
arangodb::Result removeLocalBackups(network::ConnectionPool* pool,
                                    std::string const& backupId,
                                    std::vector<ServerID> const& servers,
                                    std::vector<std::string>& deleted) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder b(&builder);
    builder.add("id", VPackValue(backupId));
  }

  std::string const url = apiStr + "delete";

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& dbServer : servers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("33e85", DEBUG, Logger::BACKUP) << "Deleting backup " << backupId;

  size_t notFoundCount = 0;

  // Now listen to the results:
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          std::string("Communication error while deleting backup") + backupId +
              " on " + r.destination);
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                              std::string("failed to remove backup from ") +
                                  r.destination + ", result not an object");
    }

    if (!resSlice.hasKey(StaticStrings::Error) ||
        !resSlice.get(StaticStrings::Error).isBoolean() ||
        resSlice.get(StaticStrings::Error).getBoolean()) {
      auto errorNum = resSlice.get(StaticStrings::ErrorNum).getNumber<int>();
      auto res = ::ErrorCode{errorNum};

      if (res == TRI_ERROR_FILE_NOT_FOUND) {
        notFoundCount += 1;
        continue;
      }

      std::string errorMsg =
          std::string("failed to delete backup ") + backupId + " on " +
          r.destination + ":" +
          resSlice.get(StaticStrings::ErrorMessage).copyString() + " (" +
          std::to_string(errorNum) + ")";

      LOG_TOPIC("9b94f", ERR, Logger::BACKUP) << errorMsg;
      return arangodb::Result(res, errorMsg);
    }
  }

  LOG_TOPIC("1b318", DEBUG, Logger::BACKUP)
      << "removeLocalBackups: notFoundCount = " << notFoundCount << " "
      << servers.size();

  if (notFoundCount == servers.size()) {
    return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,
                            "Backup " + backupId + " not found.");
  }

  deleted.emplace_back(backupId);
  LOG_TOPIC("04e97", DEBUG, Logger::BACKUP)
      << "Have located and deleted " << backupId;

  return arangodb::Result();
}

std::vector<std::string> const versionPath =
    std::vector<std::string>{"arango", "Plan", "Version"};

arangodb::Result hotbackupAsyncLockCoordinatorsTransactions(
    network::ConnectionPool* pool, std::string const& backupId,
    std::vector<ServerID> const& coordinators, double const& lockWait,
    std::unordered_map<std::string, std::string>& serverLockIds) {
  std::string const url = apiStr + "lock";

  VPackBufferUInt8 body;
  VPackBuilder lock(body);
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
    lock.add("timeout", VPackValue(lockWait));
    lock.add("unlockTimeout", VPackValue(5.0 + lockWait));
  }

  LOG_TOPIC("707ee", DEBUG, Logger::BACKUP)
      << "Trying to acquire async global transaction locks using body "
      << lock.toJson();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(lockWait + 5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(coordinators.size());

  for (auto const& coordinator : coordinators) {
    network::Headers headers;
    headers.emplace(StaticStrings::Async, "store");
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + coordinator, fuerte::RestVerb::Post, url, body,
        reqOpts, std::move(headers)));
  }

  // Perform the requests
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("Communication error locking transactions on ",
                       r.destination, ": ", r.combinedResult().errorMessage()));
    }

    if (r.statusCode() != 202) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("lock was denied from ", r.destination,
                       " when trying to check for lockId for hot backup ",
                       backupId));
    }

    bool hasJobID;
    std::string jobId =
        r.response().header.metaByKey(StaticStrings::AsyncId, hasJobID);
    if (!hasJobID) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("lock was denied from ") + r.destination +
              " when trying to check for lockId for hot backup " + backupId);
    }

    serverLockIds[r.serverId()] = jobId;
  }

  return arangodb::Result();
}

arangodb::Result hotbackupWaitForLockCoordinatorsTransactions(
    network::ConnectionPool* pool, std::string const& backupId,
    std::unordered_map<std::string, std::string>& serverLockIds,
    std::vector<ServerID>& lockedServers, double const& lockWait) {
  // query all remaining jobs here

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(lockWait + 5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(serverLockIds.size());

  VPackBufferUInt8 body;  // empty body
  for (auto const& lock : serverLockIds) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + lock.first, fuerte::RestVerb::Put,
        "/_api/job/" + lock.second, body, reqOpts));
  }

  // Perform the requests
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("Communication error locking transactions on ",
                       r.destination, ": ", r.combinedResult().errorMessage()));
    }
    // continue on 204 No Content
    if (r.statusCode() == 204) {
      continue;
    }

    VPackSlice slc = r.slice();

    if (!slc.isObject() || !slc.hasKey(StaticStrings::Error) ||
        !slc.get(StaticStrings::Error).isBoolean()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("invalid response from ") + r.destination +
              " when trying to freeze transactions for hot backup " + backupId +
              ": " + slc.toJson());
    }

    if (slc.get(StaticStrings::Error).getBoolean()) {
      LOG_TOPIC("d7a8a", DEBUG, Logger::BACKUP)
          << "failed to acquire lock from " << r.destination << ": "
          << slc.toJson();
      auto errorNum =
          ::ErrorCode{slc.get(StaticStrings::ErrorNum).getNumber<int>()};
      if (errorNum == TRI_ERROR_LOCK_TIMEOUT) {
        return arangodb::Result(
            errorNum, slc.get(StaticStrings::ErrorMessage).copyString());
      }
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("lock was denied from ") + r.destination +
              " when trying to check for lockId for hot backup " + backupId +
              ": " + slc.toJson());
    }

    if (!slc.hasKey(lockPath) || !slc.get(lockPath).isNumber() ||
        !slc.hasKey("result") || !slc.get("result").isObject()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("invalid response from ") + r.destination +
              " when trying to check for lockId for hot backup " + backupId +
              ": " + slc.toJson());
    }

    uint64_t lockId = 0;
    try {
      lockId = slc.get(lockPath).getNumber<uint64_t>();
      LOG_TOPIC("144f5", DEBUG, Logger::BACKUP)
          << "acquired lock from " << r.destination << " for backupId "
          << backupId << " with lockId " << lockId;
    } catch (std::exception const& e) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("invalid response from ") + r.destination +
              " when trying to get lockId for hot backup " + backupId + ": " +
              slc.toJson() + ", msg: " + e.what());
    }

    lockedServers.push_back(r.serverId());
    serverLockIds.erase(r.serverId());
  }

  return arangodb::Result();
}

void hotbackupCancelAsyncLocks(
    network::ConnectionPool* pool,
    std::unordered_map<std::string, std::string>& dbserverLockIds,
    std::vector<ServerID>& lockedServers) {
  // abort all the jobs
  // if a job can not be aborted, assume it has started and add the server to
  // the unlock list

  // cancel all remaining jobs here

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbserverLockIds.size());

  VPackBufferUInt8 body;  // empty body
  for (auto const& lock : dbserverLockIds) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + lock.first, fuerte::RestVerb::Put,
        "/_api/job/" + lock.second + "/cancel", body, reqOpts));
  }
}

arangodb::Result hotBackupCoordinator(ClusterFeature& feature,
                                      VPackSlice const payload,
                                      VPackBuilder& report) {
  // ToDo: mode
  // HotBackupMode const mode = CONSISTENT;

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
         (payload.hasKey("timeout") && !payload.get("timeout").isNumber()) ||
         (payload.hasKey("allowInconsistent") &&
          !payload.get("allowInconsistent").isBoolean()) ||
         (payload.hasKey("force") && !payload.get("force").isBoolean()))) {
      events::CreateHotbackup("", TRI_ERROR_BAD_PARAMETER);
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, BAD_PARAMS_CREATE);
    }

    bool allowInconsistent =
        payload.isNone() ? false : payload.get("allowInconsistent").isTrue();
    bool force = payload.isNone() ? false : payload.get("force").isTrue();

    std::string const backupId =
        (payload.isObject() && payload.hasKey("label"))
            ? payload.get("label").copyString()
            : to_string(boost::uuids::random_generator()());
    std::string timeStamp = timepointToString(std::chrono::system_clock::now());

    double timeout = (payload.isObject() && payload.hasKey("timeout"))
                         ? payload.get("timeout").getNumber<double>()
                         : 120.;
    // unreasonably short even under allowInconsistent
    if (timeout < 2.5) {
      auto const tmp = timeout;
      timeout = 2.5;
      LOG_TOPIC("67ae2", WARN, Logger::BACKUP)
          << "Backup timeout " << tmp << " is too short - raising to "
          << timeout;
    }

    using namespace std::chrono;
    auto end = steady_clock::now() +
               milliseconds(static_cast<uint64_t>(1000 * timeout));
    ClusterInfo& ci = feature.clusterInfo();

    auto const& nf = feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (!pool) {
      // nullptr happens only during controlled shutdown
      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_SHUTTING_DOWN);
      return Result(TRI_ERROR_SHUTTING_DOWN, "server is shutting down");
    }

    // Go to backup mode for *timeout* if and only if not already in
    // backup mode. Otherwise we cannot know, why backup mode was activated
    // We specifically want to make sure that no other backup is going on.
    bool supervisionOff = false;
    auto result = ci.agencyHotBackupLock(backupId, timeout, supervisionOff);

    if (!result.ok()) {
      // Failed to go to backup mode
      result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                   StringUtils::concatT("agency lock operation resulted in ",
                                        result.errorMessage()));
      LOG_TOPIC("6c73d", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_HOT_BACKUP_INTERNAL);
      return result;
    }

    auto releaseAgencyLock = scopeGuard([&]() noexcept {
      try {
        LOG_TOPIC("52416", DEBUG, Logger::BACKUP)
            << "Releasing agency lock with scope guard! backupId: " << backupId;
        ci.agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      } catch (std::exception const& e) {
        LOG_TOPIC("a163b", ERR, Logger::BACKUP)
            << "Failed to unlock hotbackup lock: " << e.what();
      }
    });

    if (end < steady_clock::now()) {
      LOG_TOPIC("352d6", INFO, Logger::BACKUP)
          << "hot backup didn't get to locking phase within " << timeout
          << "s.";
      // release the lock
      releaseAgencyLock.fire();

      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_CLUSTER_TIMEOUT);
      return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT,
                              "hot backup timeout before locking phase");
    }

    // acquire agency dump
    auto agency = std::make_shared<VPackBuilder>();
    result = ci.agencyPlan(agency);

    if (!result.ok()) {
      // release the lock
      releaseAgencyLock.fire();
      result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                   StringUtils::concatT("failed to acquire agency dump: ",
                                        result.errorMessage()));
      LOG_TOPIC("c014d", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_HOT_BACKUP_INTERNAL);
      return result;
    }

    // Call lock on all database servers

    std::vector<ServerID> dbServers = ci.getCurrentDBServers();
    std::vector<ServerID> serversToBeLocked = ci.getCurrentCoordinators();
    std::vector<ServerID> lockedServers;
    // We try to hold all write transactions on all servers at the same time.
    // The default timeout to get to this state is 120s. We first try for a
    // certain time t, and if not everybody has stopped all transactions within
    // t seconds, we release all locks and try again with t doubled, until the
    // total timeout has been reached. We start with t=15, which gives us
    // 15, 30 and 60 to try before the default timeout of 120s has been reached.
    double lockWait(15.0);
    while (steady_clock::now() < end && !feature.server().isStopping()) {
      result = lockServersTrxCommit(pool, backupId, serversToBeLocked, lockWait,
                                    lockedServers);
      if (!result.ok()) {
        unlockServersTrxCommit(pool, backupId, lockedServers);
        lockedServers.clear();
        if (result.is(TRI_ERROR_LOCAL_LOCK_FAILED)) {  // Unrecoverable
          LOG_TOPIC("99dbe", WARN, Logger::BACKUP)
              << "unable to lock servers for hot backup: "
              << result.errorMessage();
          // release the lock
          releaseAgencyLock.fire();
          events::CreateHotbackup(timeStamp + "_" + backupId,
                                  TRI_ERROR_LOCAL_LOCK_FAILED);
          return result;
        }
      } else {
        break;
      }
      if (lockWait < 3600.0) {
        lockWait *= 2.0;
      }
      std::this_thread::sleep_for(milliseconds(300));
    }

    // TODO: the force attribute is still present and offered by arangobackup,
    // but it can likely be removed nowadays.
    if (!result.ok() && force) {
      // About this code:
      // it first creates async requests to lock all coordinators.
      //    the corresponding lock ids are stored int the map lockJobIds.
      // Then we continously abort all trx while checking all the above jobs
      //    for completion.
      // If a job was completed then its id is removed from lockJobIds
      //  and the server is added to the lockedServers list.
      // Once lockJobIds is empty or an error occured we exit the loop
      //  and continue on the normal path (as if all servers would have been
      //  locked or error-exit)

      // dbserver -> jobId
      std::unordered_map<std::string, std::string> lockJobIds;

      auto releaseLocks = scopeGuard([&]() noexcept {
        try {
          hotbackupCancelAsyncLocks(pool, lockJobIds, lockedServers);
          unlockServersTrxCommit(pool, backupId, lockedServers);
        } catch (std::exception const& ex) {
          LOG_TOPIC("3449d", ERR, Logger::BACKUP)
              << "Failed to unlock hot backup: " << ex.what();
        }
      });

      // we have to reset the timeout, otherwise the code below will exit too
      // soon
      end = steady_clock::now() +
            milliseconds(static_cast<uint64_t>(1000 * timeout));

      // send the locks
      result = hotbackupAsyncLockCoordinatorsTransactions(
          pool, backupId, serversToBeLocked, lockWait, lockJobIds);
      if (result.fail()) {
        events::CreateHotbackup(timeStamp + "_" + backupId,
                                result.errorNumber());
        return result;
      }

      transaction::Manager* mgr = transaction::ManagerFeature::manager();

      while (!lockJobIds.empty()) {
        if (steady_clock::now() > end) {
          return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT,
                                  "hot backup timeout before locking phase");
        }

        // kill all transactions
        result =
            mgr->abortAllManagedWriteTrx(ExecContext::current().user(), true);
        if (result.fail()) {
          events::CreateHotbackup(timeStamp + "_" + backupId,
                                  result.errorNumber());
          return result;
        }

        // wait for locks, servers that got the lock are removed from lockJobIds
        result = hotbackupWaitForLockCoordinatorsTransactions(
            pool, backupId, lockJobIds, lockedServers, lockWait);
        if (result.fail()) {
          LOG_TOPIC("b6496", WARN, Logger::BACKUP)
              << "Waiting for hot backup server locks failed: "
              << result.errorMessage();
          events::CreateHotbackup(timeStamp + "_" + backupId,
                                  result.errorNumber());
          return result;
        }

        std::this_thread::sleep_for(milliseconds(300));
      }

      releaseLocks.cancel();
    }

    bool gotLocks = result.ok();

    // In the case we left the above loop with a negative result,
    // and we are in the case of a force backup we want to continue here
    if (!gotLocks && !allowInconsistent) {
      unlockServersTrxCommit(pool, backupId, serversToBeLocked);
      // release the lock
      releaseAgencyLock.fire();
      result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          StringUtils::concatT(
              "failed to acquire global transaction lock on all coordinators: ",
              result.errorMessage()));
      LOG_TOPIC("b7d09", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    BackupMeta meta(backupId, "", timeStamp, std::vector<std::string>{}, 0, 0,
                    static_cast<unsigned int>(serversToBeLocked.size()), "",
                    !gotLocks);  // Temporary
    std::vector<std::string> dummy;
    result = hotBackupDBServers(pool, backupId, timeStamp, dbServers,
                                agency->slice(),
                                /* force */ !gotLocks, meta);
    if (!result.ok()) {
      unlockServersTrxCommit(pool, backupId, serversToBeLocked);
      // release the lock
      releaseAgencyLock.fire();
      result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          StringUtils::concatT("failed to hot backup on all coordinators: ",
                               result.errorMessage()));
      LOG_TOPIC("6b333", ERR, Logger::BACKUP) << result.errorMessage();
      removeLocalBackups(pool, backupId, dbServers, dummy);
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    unlockServersTrxCommit(pool, backupId, serversToBeLocked);
    // release the lock
    releaseAgencyLock.fire();

    auto agencyCheck = std::make_shared<VPackBuilder>();
    result = ci.agencyPlan(agencyCheck);
    if (!result.ok()) {
      if (!allowInconsistent) {
        removeLocalBackups(pool, backupId, dbServers, dummy);
      }
      result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          StringUtils::concatT("failed to acquire agency dump post backup: ",
                               result.errorMessage(),
                               " backup's integrity is not guaranteed"));
      LOG_TOPIC("d4229", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    try {
      if (!Helper::equal(agency->slice()[0].get(versionPath),
                         agencyCheck->slice()[0].get(versionPath), false)) {
        if (!allowInconsistent) {
          removeLocalBackups(pool, backupId, dbServers, dummy);
        }
        result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                     "data definition of cluster was changed during hot "
                     "backup: backup's integrity is not guaranteed");
        LOG_TOPIC("0ad21", ERR, Logger::BACKUP) << result.errorMessage();
        events::CreateHotbackup(timeStamp + "_" + backupId,
                                result.errorNumber());
        return result;
      }
    } catch (std::exception const& e) {
      removeLocalBackups(pool, backupId, dbServers, dummy);
      result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                   std::string("invalid agency state: ") + e.what());
      LOG_TOPIC("037eb", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    std::replace(timeStamp.begin(), timeStamp.end(), ':', '.');
    {
      VPackObjectBuilder o(&report);
      report.add("id", VPackValue(timeStamp + "_" + backupId));
      report.add("sizeInBytes", VPackValue(meta._sizeInBytes));
      report.add("nrFiles", VPackValue(meta._nrFiles));
      report.add("nrDBServers", VPackValue(meta._nrDBServers));
      report.add("datetime", VPackValue(meta._datetime));
      if (!gotLocks) {
        report.add("potentiallyInconsistent", VPackValue(true));
      }
    }

    events::CreateHotbackup(timeStamp + "_" + backupId, TRI_ERROR_NO_ERROR);
    return arangodb::Result();

  } catch (std::exception const& e) {
    events::CreateHotbackup("", TRI_ERROR_HOT_BACKUP_INTERNAL);
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string("caught exception creating cluster backup: ") + e.what());
  }
}

arangodb::Result listHotBackupsOnCoordinator(ClusterFeature& feature,
                                             VPackSlice const payload,
                                             VPackBuilder& report) {
  auto const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    // shutdown, leave here
    return TRI_ERROR_SHUTTING_DOWN;
  }

  ClusterInfo& ci = feature.clusterInfo();
  std::vector<ServerID> dbServers = ci.getCurrentDBServers();

  std::unordered_map<std::string, BackupMeta> list;

  VPackSlice idSlice;
  if (payload.isObject() && payload.hasKey("id")) {
    idSlice = payload.get("id");
    if (idSlice.isArray()) {
      for (auto const i : VPackArrayIterator(payload.get("id"))) {
        if (!i.isString()) {
          return arangodb::Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                                  "invalid list JSON: all ids must be string.");
        }
      }
    } else if (!idSlice.isString()) {
      return arangodb::Result(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "invalid JSON: id must be string or array of strings.");
    }
  } else if (!payload.isNone()) {
    return arangodb::Result(
        TRI_ERROR_HTTP_BAD_PARAMETER,
        "invalid JSON: body must be empty or object with attribute 'id'.");
  }  // allow continuation with None slice

  VPackBuilder dummy;

  // Try to get complete listing for 2 minutes
  using namespace std::chrono;
  auto timeout = steady_clock::now() + duration<double>(120.0);
  arangodb::Result result;
  std::chrono::duration<double> wait(1.0);
  while (true) {
    if (feature.server().isStopping()) {
      return Result(TRI_ERROR_SHUTTING_DOWN, "server is shutting down");
    }

    result = hotBackupList(pool, dbServers, idSlice, list, dummy);

    if (!result.ok()) {
      if (payload.isObject() && !idSlice.isNone() &&
          result.is(TRI_ERROR_HTTP_NOT_FOUND)) {
        auto error =
            std::string("failed to locate backup '") + idSlice.toJson() + "'";
        LOG_TOPIC("2020b", DEBUG, Logger::BACKUP) << error;
        return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND, error);
      }
      if (steady_clock::now() > timeout) {
        return arangodb::Result(
            TRI_ERROR_CLUSTER_TIMEOUT,
            "timeout waiting for all db servers to report backup list");
      } else {
        LOG_TOPIC("76865", DEBUG, Logger::BACKUP)
            << "failed to get a hot backup listing from all db servers waiting "
            << wait.count() << " seconds";
        std::this_thread::sleep_for(wait);
        wait *= 1.1;
      }
    } else {
      break;
    }
  }

  // Build report
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

arangodb::Result deleteHotBackupsOnCoordinator(ClusterFeature& feature,
                                               VPackSlice const payload,
                                               VPackBuilder& report) {
  std::vector<std::string> deleted;
  VPackBuilder dummy;
  arangodb::Result result;

  auto const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {  // shutdown, leave here
    events::DeleteHotbackup("", TRI_ERROR_SHUTTING_DOWN);
    return TRI_ERROR_SHUTTING_DOWN;
  }

  ClusterInfo& ci = feature.clusterInfo();
  std::vector<ServerID> dbServers = ci.getCurrentDBServers();

  if (!payload.isObject() || !payload.hasKey("id") ||
      !payload.get("id").isString()) {
    events::DeleteHotbackup("", TRI_ERROR_HTTP_BAD_PARAMETER);
    return arangodb::Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                            "Expecting object with key `id` set to backup id.");
  }

  std::string id = payload.get("id").copyString();

  result = removeLocalBackups(pool, id, dbServers, deleted);
  if (!result.ok()) {
    events::DeleteHotbackup(id, result.errorNumber());
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
  events::DeleteHotbackup(id, TRI_ERROR_NO_ERROR);
  return result;
}

arangodb::Result getEngineStatsFromDBServers(ClusterFeature& feature,
                                             VPackBuilder& report) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Get, "/_api/engine/stats",
        VPackBuffer<uint8_t>(), reqOpts));
  }

  auto responses = futures::collectAll(std::move(futures)).waitAndGet();

  report.openObject();
  for (auto const& tryRes : responses) {
    network::Response const& r = tryRes.get();

    if (r.fail()) {
      return {network::fuerteToArangoErrorCode(r),
              network::fuerteToArangoErrorMessage(r)};
    }

    // cut off "server:" from the destination
    report.add(r.destination.substr(7), r.slice());
  }
  report.close();

  return {};
}

}  // namespace arangodb
