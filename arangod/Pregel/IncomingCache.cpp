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

#include "IncomingCache.h"
#include "Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template<typename M>
IncomingCache<M>::~IncomingCache() {
  for (auto const &it : _messages) {
    delete(it.second);
  }
  _messages.clear();
}

template<typename M>
void IncomingCache<M>::clear() {
  for (auto const &it : _messages) {
    it.second->clear();
  }
}

template<typename M>
void IncomingCache<M>::parseMessages(VPackSlice incomingMessages) {
  MUTEX_LOCKER(locker, writeMutex);
  LOG(INFO) << "Adding messages to In-Queue " << incomingMessages.toJson();
    
    VPackValueLength length = incomingMessages.length();
    if (length % 2) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "There must always be an even number of entries in messages");
    }
  
    VPackValueLength i = 0;
    std::string vertexId;
    for (i = 0; i < length; i++) {
      VPackSlice current = incomingMessages.at(i);
      if (i % 2 == 0) {// TODO support multiple recipients
        vertexId = current.copyString();
      } else {
        int64_t newValue = current.get("value").getInt();
        auto vmsg = _messages.find(vertexId);
        if (vmsg != _messages.end()) {
          
          // if no combiner
          // vmsg->add(it.slice())
          
          // TODO hardcoded combiner
          VPackBuilder *current = vmsg->second;
          int64_t oldValue = current->slice().get("value").getInt();
          if (newValue < oldValue) {
            current->clear();
            current->openObject();
            current->add("value", VPackValue(newValue));
            current->close();
          }
        } else {
          // with a combiner
          std::unique_ptr<VPackBuilder> b(new VPackBuilder());
          b->add(current);
          _messages[vertexId] = b.get();
          b.release();
          
          // if no combiner
          // VPackBuilder *arr = new VPackBuilder(it);
          // arr->openArray();
          // arr->add(it)
          // _messages[vertexId] = arr;
        }
      }
    }
    
  //unordered_map<string, vector<VPackSlice>> messageBucket;
  //VPackSlice messages = data.get(Utils::messagesKey);
}

template<typename M>
void IncomingCache<M>::setDirect(std::string const& vertexId, M const& data) {
    auto vmsg = _messages.find(vertexId);
    if (vmsg != _messages.end()) {
        VPackBuilder *b = vmsg->second;
         b->add(data);
    } else {
        std::unique_ptr<VPackBuilder> b(new VPackBuilder());
        b->add(data);
        _messages[vertexId] = b.get();
        b.release();
    }
}

template<typename M>
MessageIterator<M> IncomingCache<M>::getMessages(std::string const& vertexId) {
  LOG(INFO) << "Querying messages for " << vertexId;
  auto vmsg = _messages.find(vertexId);
  if (vmsg != _messages.end()) {
    return MessageIterator(vmsg->second->slice());
  }
  else return MessageIterator();
}
