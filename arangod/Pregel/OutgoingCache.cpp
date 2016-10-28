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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "OutgoingCache.h"
#include "IncomingCache.h"
#include "Utils.h"
#include "WorkerContext.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E, typename M>
OutgoingCache<V, E, M>::OutgoingCache(
    std::shared_ptr<WorkerContext<V, E, M>> context)
    : _ctx(context), _containedMessages(0), _sendMessages(0) {
  _ci = ClusterInfo::instance();
  _collInfo =
      _ci->getCollection(_ctx->database(), _ctx->vertexCollectionPlanId());
  _baseUrl = Utils::baseUrl(_ctx->database());
  _format = context->algorithm()->messageFormat();
  _combiner = context->algorithm()->messageCombiner();
}

template <typename V, typename E, typename M>
OutgoingCache<V, E, M>::~OutgoingCache() {
  clear();
}

template <typename V, typename E, typename M>
void OutgoingCache<V, E, M>::clear() {
  // TODO better way?
  /*for (auto const& it : _map) {
    for (auto const& it2 : it.second) {
      it2.second->clear();  // clears VPackBuilder
    }
  }*/
  _map.clear();
  _containedMessages = 0;
}

static void resolveResponsibleShard(ClusterInfo* ci, LogicalCollection* info,
                                    std::string const& vertexKey,
                                    std::string& responsibleShard) {
  bool usesDefaultShardingAttributes;
  VPackBuilder partial;
  partial.openObject();
  partial.add(StaticStrings::KeyString, VPackValue(vertexKey));
  partial.close();
  LOG(INFO) << "Partial doc: " << partial.toJson();
  int res =
      ci->getResponsibleShard(info, partial.slice(), true, responsibleShard,
                              usesDefaultShardingAttributes);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res, "OutgoingCache could not resolve the responsible shard");
  }
  TRI_ASSERT(usesDefaultShardingAttributes);  // should be true anyway
}

template <typename V, typename E, typename M>
void OutgoingCache<V, E, M>::sendMessageTo(std::string const& toValue,
                                           M const& data) {
  assert(_combiner);

  LOG(INFO) << "Adding outgoing messages for " << toValue;
  std::string vertexKey = Utils::vertexKeyFromToValue(toValue);
  ShardID responsibleShard;
  resolveResponsibleShard(_ci, _collInfo.get(), vertexKey, responsibleShard);
  LOG(INFO) << "Responsible shard: " << responsibleShard;

  std::vector<ShardID> const& localShards = _ctx->localVertexShardIDs();
  if (std::find(localShards.begin(), localShards.end(), responsibleShard) !=
      localShards.end()) {
    auto incoming = _ctx->writeableIncomingCache();
    incoming->setDirect(vertexKey, data);
    LOG(INFO) << "Worker: Got messages for myself " << vertexKey << " <- "
              << data;

  } else {
    // std::unordered_map<std::string, VPackBuilder*> vertexMap =;
    std::unordered_map<std::string, M>& vertexMap = _map[responsibleShard];
    auto it = vertexMap.find(vertexKey);
    if (it != vertexMap.end()) {  // more than one message
      vertexMap[vertexKey] = _combiner->combine(vertexMap[vertexKey], data);
    } else {  // first message for this vertex
      vertexMap[vertexKey] = data;
    }
    _containedMessages++;
  }

  // TODO treshold  sending
}

template <typename V, typename E, typename M>
void OutgoingCache<V, E, M>::sendMessages() {
  LOG(INFO) << "Sending messages to other machines";

  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _map) {
    ShardID const& shard = it.first;
    std::unordered_map<std::string, M> const& vertexMessageMap = it.second;
    if (vertexMessageMap.size() == 0) {
      continue;
    }

    VPackBuilder package;
    package.openObject();
    package.add(Utils::messagesKey, VPackValue(VPackValueType::Array));
    for (auto const& vertexMessagePair : vertexMessageMap) {
      package.add(VPackValue(vertexMessagePair.first));
      _format->addValue(package, vertexMessagePair.second);
      _sendMessages++;
    }
    package.close();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey, VPackValue(_ctx->executionNumber()));
    package.add(Utils::globalSuperstepKey, VPackValue(_ctx->globalSuperstep()));
    package.close();
    // add a request
    auto body = std::make_shared<std::string>(package.toJson());
    requests.emplace_back("shard:" + shard, rest::RequestType::POST,
                          _baseUrl + Utils::messagesPath, body);

    LOG(INFO) << "Worker: Sending data to other Shard: " << shard
              << ". Message: " << package.toJson();
  }
  size_t nrDone = 0;
  ClusterComm::instance()->performRequests(requests, 120, nrDone,
                                           LogTopic("Pregel message transfer"));
  // readResults(requests);
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      LOG(INFO) << res.answer->payload().toJson();
    }
  }
  this->clear();
}

// template types to create
template class arangodb::pregel::OutgoingCache<int64_t, int64_t, int64_t>;
