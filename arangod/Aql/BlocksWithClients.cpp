////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "BlocksWithClients.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlTransaction.h"
#include "Aql/AqlValue.h"
#include "Aql/BlockCollector.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionStats.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/WakeupQueryCallback.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;
using StringBuffer = arangodb::basics::StringBuffer;

BlocksWithClients::BlocksWithClients(ExecutionEngine* engine, ExecutionNode const* ep,
                                     std::vector<std::string> const& shardIds)
    : ExecutionBlock(engine, ep),
      _nrClients(shardIds.size()),
      _type(ScatterNode::ScatterType::SHARD),
      _wasShutdown(false) {
  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
  auto scatter = ExecutionNode::castTo<ScatterNode const*>(ep);
  TRI_ASSERT(scatter != nullptr);
  _type = scatter->getScatterType();
}

std::pair<ExecutionState, bool> BlocksWithClients::getBlock(size_t atMost) {
  throwIfKilled();  // check if we were aborted

  auto res = _dependencies[0]->getSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    return {res.first, false};
  }

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _upstreamState = res.first;

  if (res.second != nullptr) {
    _buffer.emplace_back(std::move(res.second));
    return {res.first, true};
  }

  return {res.first, false};
}

/// @brief shutdown
std::pair<ExecutionState, Result> BlocksWithClients::shutdown(int errorCode) {
  if (_wasShutdown) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  auto res = ExecutionBlock::shutdown(errorCode);
  if (res.first == ExecutionState::WAITING) {
    return res;
  }
  _wasShutdown = true;
  return res;
}

/// @brief getClientId: get the number <clientId> (used internally)
/// corresponding to <shardId>
size_t BlocksWithClients::getClientId(std::string const& shardId) const {
  if (shardId.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "got empty distribution id");
  }
  auto it = _shardIdMap.find(shardId);
  if (it == _shardIdMap.end()) {
    std::string message("AQL: unknown distribution id ");
    message.append(shardId);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }
  return it->second;
}

void BlocksWithClients::throwIfKilled() {
  if (_engine->getQuery()->killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}
std::pair<ExecutionState, SharedAqlItemBlockPtr> BlocksWithClients::getSome(size_t) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::pair<ExecutionState, size_t> BlocksWithClients::skipSome(size_t) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
