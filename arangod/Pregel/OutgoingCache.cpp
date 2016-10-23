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
#include "WorkerContext.h"
#include "IncomingCache.h"
#include "Combiner.h"
#include "Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "VocBase/LogicalCollection.h"
#include "Cluster/ClusterComm.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename M>
OutgoingCache<M>::OutgoingCache(std::shared_ptr<WorkerContext> context, Combiner<M> const& combiner) : _ctx(context) {
  _ci = ClusterInfo::instance();
  _collInfo = _ci->getCollection(_ctx->database(), _ctx->vertexCollectionPlanId());
  _baseUrl = Utils::baseUrl(_ctx->database());
  _combiner = combiner;
}

template <typename M>

OutgoingCache<M>::~OutgoingCache<M>() {
    clear();
}

template <typename M>
void OutgoingCache<M>::clear() {
  //TODO better way?
  for (auto const &it : _map) {
    for (auto const &it2 : it.second) {
      it2.second->clear();// clears VPackBuilder
    }
  }
  _map.clear();
  _numVertices = 0;
}

template <typename M>
void OutgoingCache<M>::sendMessageTo(std::string const& toValue, M const& data) {
  assert(_combiner);

  LOG(INFO) << "Adding outgoing messages for " << toValue;
  std::string vertexKey = Utils::vertexKeyFromToValue(toValue);
    
  ShardID responsibleShard;
  bool usesDefaultShardingAttributes;
  VPackBuilder partial;
  partial.openObject();
  partial.add(StaticStrings::KeyString, VPackValue(vertexKey));
  partial.close();
  LOG(INFO) << "Partial doc: " << partial.toJson();
  int res = _ci->getResponsibleShard(_collInfo.get(), partial.slice(), true,
                                     responsibleShard, usesDefaultShardingAttributes);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, "OutgoingCache could not resolve the responsible shard");
  }
  TRI_ASSERT(usesDefaultShardingAttributes);// should be true anyway
  LOG(INFO) << "Responsible shard: " << responsibleShard;
    
  //std::unordered_map<std::string, VPackBuilder*> vertexMap =;
  auto const& vertexMap = _map[responsibleShard];
  auto it = vertexMap.find(vertexKey);
  if (it != vertexMap.end()) {// more than one message
    
    vertexMap[vertexKey] = _combiner->combine(vertexMap[vertexKey], data);
    
    /*VPackBuilder *b = it->second;
    
    // hardcoded combiner
    int64_t oldValue = b->slice().get("value").getInt();
    int64_t newValue = mData.get("value").getInt();
    if (newValue < oldValue) {
      b->clear();
      b->add(mData);
    }*/
    
  } else {// first message for this vertex
    //std::unique_ptr<VPackBuilder> b(new VPackBuilder());
    //b->add(mData);
    vertexMap[vertexKey] = data;
    //b.release();
  }
    
    _numVertices++;
}
/*
void OutgoingCache::getMessages(ShardID const& shardId, VPackBuilder &outBuilder) {
  auto shardIt = _map.find(shardId);
  if (shardIt != _map.end()) {
    //auto vertices = *shardIt;
    for (auto messagesPair : shardIt->second) {
      VPackSlice vertexMsgs = messagesPair.second->slice();
      if (vertexMsgs.isArray()) outBuilder.add(VPackArrayIterator(vertexMsgs));
      else outBuilder.add(vertexMsgs);
    }
    //return ArrayIterator(vmsg->second->slice())
  }
  //outBuilder.close();
  //else return VPackSlice();
}
*/
template <typename M>
void OutgoingCache<M>::sendMessages() {
    LOG(INFO) << "Sending messages to other machines";
    auto localShards = _ctx->localVertexShardIDs();
    
    std::vector<ClusterCommRequest> requests;
    for (auto const &it : _map) {
        
        ShardID const& shard = it.first;
        std::unordered_map<std::string, VPackBuilder*> const& vertexMessageMap = it.second;
        if (vertexMessageMap.size() == 0) {
            continue;
        }
        
        if (std::find(localShards.begin(), localShards.end(), shard) != localShards.end()) {
            for (auto const& pair : vertexMessageMap) {
              
              
              
                _ctx->writeableIncomingCache()->setDirect(pair.first, pair.second->slice());
                LOG(INFO) << "Worker: Got messages for myself: " << pair.second->slice().toJson();
            }
        } else {
            VPackBuilder package;
            package.openObject();
            package.add(Utils::messagesKey, VPackValue(VPackValueType::Array));
            for (auto const& vertexMessagePair : vertexMessageMap) {
                package.add(VPackValue(vertexMessagePair.first));
                VPackSlice msgs = vertexMessagePair.second->slice();
                if (msgs.isArray()) {
                    package.add(msgs);
                } else {
                    package.add(msgs);
                }
            }
            package.close();
            package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
            package.add(Utils::executionNumberKey, VPackValue(_ctx->executionNumber()));
            package.add(Utils::globalSuperstepKey, VPackValue(_ctx->globalSuperstep()));
            package.close();
            // add a request
            auto body = std::make_shared<std::string const>(package.toJson());
            requests.emplace_back("shard:" + shard, rest::RequestType::POST, _baseUrl + Utils::messagesPath, body);
            
            LOG(INFO) << "Worker: Sending messages to other DBServer " << package.toJson();
        }
    }
    size_t nrDone = 0;
    ClusterComm::instance()->performRequests(requests, 120, nrDone, LogTopic("Pregel message transfer"));
    //readResults(requests);
    for (auto const& req : requests) {
        auto& res = req.result;
        if (res.status == CL_COMM_RECEIVED) {
            LOG(INFO) << res.answer->payload().toJson();
        }
    }
}
