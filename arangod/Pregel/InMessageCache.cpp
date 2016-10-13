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

#include "InMessageCache.h"
#include "Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

InMessageCache::~InMessageCache() {
  for (auto const &it : _messages) {
    delete(it.second);
  }
  _messages.clear();
}

void InMessageCache::clear() {
  for (auto const &it : _messages) {
    it.second->clear();
  }
}

void InMessageCache::addMessages(VPackArrayIterator incomingMessages) {
  MUTEX_LOCKER(locker, writeMutex);
  LOG(INFO) << "Adding messages to in queue";
  
  //unordered_map<string, vector<VPackSlice>> messageBucket;
  //VPackSlice messages = data.get(Utils::messagesKey);
  for (auto const &it : incomingMessages) {
    std::string vertexId = it.get(StaticStrings::ToString).copyString();
    
    int64_t newValue = it.get("value").getInt();
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
        current->add(StaticStrings::ToString, it.get(StaticStrings::ToString));
        current->add("value", VPackValue(newValue));
        current->close();
      }
    } else {
      // with a combiner
      std::unique_ptr<VPackBuilder> b(new VPackBuilder());
      b->add(it);
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

VPackSlice InMessageCache::getMessages(std::string const& vertexId) {
  LOG(INFO) << "Querying messages from for " << vertexId;
  auto vmsg = _messages.find(vertexId);
  if (vmsg != _messages.end()) {
    return vmsg->second->slice();
  }
  else return VPackSlice();
}
