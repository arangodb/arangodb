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

template <typename M>
IncomingCache<M>::~IncomingCache() {
  LOG(INFO) << "~IncomingCache";
  _messages.clear();
}

template <typename M>
void IncomingCache<M>::clear() {
  /*for (auto const& it : _messages) {
    it.second->clear();
  }*/
  _receivedMessageCount = 0;
  _messages.clear();
}

template <typename M>
void IncomingCache<M>::parseMessages(VPackSlice incomingMessages) {
  MUTEX_LOCKER(locker, writeMutex);

  VPackValueLength length = incomingMessages.length();
  if (length % 2) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "There must always be an even number of entries in messages");
  }

  VPackValueLength i = 0;
  std::string vertexId;
  for (i = 0; i < length; i++) {
    VPackSlice current = incomingMessages.at(i);
    if (i % 2 == 0) {  // TODO support multiple recipients
      vertexId = current.copyString();
    } else {
      M newValue;
      bool sucess = _format->unwrapValue(current, newValue);
      if (!sucess) {
        LOG(WARN) << "Invalid message format supplied";
        continue;
      }

      _receivedMessageCount++;
      auto vmsg = _messages.find(vertexId);
      if (vmsg != _messages.end()) {  // got a message for the same vertex
        vmsg->second = _combiner->combine(vmsg->second, newValue);
      } else {
        _messages[vertexId] = newValue;
      }
    }
  }
}

template <typename M>
void IncomingCache<M>::setDirect(std::string const& vertexId, M const& data) {
  MUTEX_LOCKER(locker, writeMutex);
  _receivedMessageCount++;

  auto vmsg = _messages.find(vertexId);
  if (vmsg != _messages.end()) {
    vmsg->second = data;  // TODO combine1!!!
  } else {
    _messages[vertexId] = data;
  }
}

template <typename M>
MessageIterator<M> IncomingCache<M>::getMessages(std::string const& vertexId) {
  LOG(INFO) << "Querying messages for " << vertexId;
  auto vmsg = _messages.find(vertexId);
  if (vmsg != _messages.end()) {
    return MessageIterator<M>(&vmsg->second);
  } else {
    return MessageIterator<M>();
  }
}

// template types to create
template class arangodb::pregel::IncomingCache<int64_t>;
