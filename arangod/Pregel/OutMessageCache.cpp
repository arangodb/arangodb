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

#include "OutMessageCache.h"
#include "WorkerContext.h"
#include "InMessageCache.h"
#include "Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "VocBase/LogicalCollection.h"
#include "Cluster/ClusterComm.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

OutMessageCache::OutMessageCache(std::shared_ptr<WorkerContext> context) : _ctx(context) {
  _ci = ClusterInfo::instance();
  _collInfo = _ci->getCollection(_ctx->database(), _ctx->vertexCollectionPlanId());
  _baseUrl = Utils::baseUrl(_ctx->database());
  
  auto shardMap = _ci->getShardList(_ctx->vertexCollectionPlanId());
  for (ShardID const &it : *shardMap) {
    _map[it];
  }
}

OutMessageCache::~OutMessageCache() {
  for (auto const &it : _map) {
    for (auto const &it2 : it.second) {
      delete(it2.second);
    }
  }
  _map.clear();
}

void OutMessageCache::clear() {
  //TODO better way?
  for (auto const &it : _map) {
    for (auto const &it2 : it.second) {
      it2.second->clear();// clears VPackBuilder
    }
  }
  _map.clear();
}

void OutMessageCache::addMessage(std::string key, VPackSlice slice) {
  LOG(INFO) << "Adding messages to out queue\n";
  
  ShardID responsibleShard;
  bool usesDefaultShardingAttributes;
  VPackBuilder keyDoc;
  keyDoc.openObject();
  keyDoc.add(StaticStrings::KeyString, VPackValue(key));
  keyDoc.close();
  int res = _ci->getResponsibleShard(_collInfo.get(), keyDoc.slice(), true,
                                     responsibleShard, usesDefaultShardingAttributes);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, "OutMessageCache could not resolve the responsible shard");
  }
  TRI_ASSERT(usesDefaultShardingAttributes);// should be true anyway
    
  //std::unordered_map<std::string, VPackBuilder*> vertexMap =;
  auto it = _map[responsibleShard].find(key);
  if (it != _map[responsibleShard].end()) {// more than one message
    VPackBuilder *b = it->second;
    
    // hardcoded combiner
    int64_t oldValue = b->slice().get("value").getInt();
    int64_t newValue = slice.get("value").getInt();
    if (newValue < oldValue) {
      b->clear();
      b->add(slice);
    }
    
  } else {// first message for this vertex
     std::unique_ptr<VPackBuilder> b(new VPackBuilder());
    b->add(slice);
    _map[responsibleShard][key] = b.get();
    b.release();
  }
    
    _numVertices++;
}

void OutMessageCache::getMessages(ShardID const& shardId, VPackBuilder &outBuilder) {
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

void OutMessageCache::sendMessages() {
    LOG(INFO) << "Sending messages to shards";
    std::shared_ptr<std::vector<ShardID>> shards = _ci->getShardList(_ctx->vertexCollectionPlanId());
    LOG(INFO) << "Seeing shards: " << shards->size();
    
    std::vector<ClusterCommRequest> requests;
    for (auto const &it : *shards) {
        
        if (_ctx->vertexShardId() == it) {
            LOG(INFO) << "Worker: Getting messages for myself";
            VPackBuilder messages;
            messages.openArray();
            getMessages(it, messages);
            messages.close();
            _ctx->writeableIncomingCache()->addMessages(VPackArrayIterator(messages.slice()));
        } else {
            LOG(INFO) << "Worker: Sending messages for shard " << it;
            
            VPackBuilder package;
            package.openObject();
            package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
            package.add(Utils::executionNumberKey, VPackValue(_ctx->executionNumber()));
            package.add(Utils::globalSuperstepKey, VPackValue(_ctx->globalSuperstep()));
            package.add(Utils::messagesKey, VPackValue(VPackValueType::Array));
            getMessages(it, package);
            package.close();
            package.close();
            // add a request
            auto body = std::make_shared<std::string const>(package.toJson());
            requests.emplace_back("shard:" + it, rest::RequestType::POST, _baseUrl + Utils::messagesPath, body);
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
