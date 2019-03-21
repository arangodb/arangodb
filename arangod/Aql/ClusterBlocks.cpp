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

#include "ClusterBlocks.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlTransaction.h"
#include "Aql/AqlValue.h"
#include "Aql/BlockCollector.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionStats.h"
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

BlockWithClients::BlockWithClients(ExecutionEngine* engine, ExecutionNode const* ep,
                                   std::vector<std::string> const& shardIds)
    : ExecutionBlock(engine, ep), _nrClients(shardIds.size()), _wasShutdown(false) {
  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}

std::pair<ExecutionState, Result> BlockWithClients::initializeCursor(AqlItemBlock* items,
                                                                     size_t pos) {
  return ExecutionBlock::initializeCursor(items, pos);
}

/// @brief shutdown
std::pair<ExecutionState, Result> BlockWithClients::shutdown(int errorCode) {
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
size_t BlockWithClients::getClientId(std::string const& shardId) const {
  if (shardId.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "got empty shard id");
  }

  auto it = _shardIdMap.find(shardId);
  if (it == _shardIdMap.end()) {
    std::string message("AQL: unknown shard id ");
    message.append(shardId);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }
  return ((*it).second);
}

/// @brief timeout
double const SingleRemoteOperationBlock::defaultTimeOut = 3600.0;

/// @brief creates a remote block
SingleRemoteOperationBlock::SingleRemoteOperationBlock(ExecutionEngine* engine,
                                                       SingleRemoteOperationNode const* en)
    : ExecutionBlock(engine, static_cast<ExecutionNode const*>(en)),
      _collection(en->collection()),
      _key(en->key()) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
}

namespace {
std::unique_ptr<VPackBuilder> merge(VPackSlice document, std::string const& key,
                                    TRI_voc_rid_t revision) {
  auto builder = std::make_unique<VPackBuilder>();
  {
    VPackObjectBuilder guard(builder.get());
    TRI_SanitizeObject(document, *builder);
    VPackSlice keyInBody = document.get(StaticStrings::KeyString);

    if (keyInBody.isNone() || keyInBody.isNull() ||
        (keyInBody.isString() && keyInBody.copyString() != key) ||
        ((revision != 0) && (TRI_ExtractRevisionId(document) != revision))) {
      // We need to rewrite the document with the given revision and key:
      builder->add(StaticStrings::KeyString, VPackValue(key));
      if (revision != 0) {
        builder->add(StaticStrings::RevString, VPackValue(TRI_RidToString(revision)));
      }
    }
  }
  return builder;
}
}  // namespace

bool SingleRemoteOperationBlock::getOne(arangodb::aql::AqlItemBlock* aqlres,
                                        size_t outputCounter) {
  int possibleWrites = 0;  // TODO - get real statistic values!
  auto node = ExecutionNode::castTo<SingleRemoteOperationNode const*>(getPlanNode());
  auto out = node->_outVariable;
  auto in = node->_inVariable;
  auto OLD = node->_outVariableOld;
  auto NEW = node->_outVariableNew;

  RegisterId inRegId = ExecutionNode::MaxRegisterId;
  RegisterId outRegId = ExecutionNode::MaxRegisterId;
  RegisterId oldRegId = ExecutionNode::MaxRegisterId;
  RegisterId newRegId = ExecutionNode::MaxRegisterId;

  if (in != nullptr) {
    auto itIn = node->getRegisterPlan()->varInfo.find(in->id);
    TRI_ASSERT(itIn != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    inRegId = (*itIn).second.registerId;
  }

  if (_key.empty() && in == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                   "missing document reference");
  }

  if (out != nullptr) {
    auto itOut = node->getRegisterPlan()->varInfo.find(out->id);
    TRI_ASSERT(itOut != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    outRegId = (*itOut).second.registerId;
  }

  if (OLD != nullptr) {
    auto itOld = node->getRegisterPlan()->varInfo.find(OLD->id);
    TRI_ASSERT(itOld != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOld).second.registerId < ExecutionNode::MaxRegisterId);
    oldRegId = (*itOld).second.registerId;
  }

  if (NEW != nullptr) {
    auto itNew = node->getRegisterPlan()->varInfo.find(NEW->id);
    TRI_ASSERT(itNew != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itNew).second.registerId < ExecutionNode::MaxRegisterId);
    newRegId = (*itNew).second.registerId;
  }

  VPackBuilder inBuilder;
  VPackSlice inSlice = VPackSlice::emptyObjectSlice();
  if (in) {  // IF NOT REMOVE OR SELECT
    if (_buffer.size() < 1) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                     "missing document reference in Register");
    }
    AqlValue const& inDocument = _buffer.front()->getValueReference(_pos, inRegId);
    inBuilder.add(inDocument.slice());
    inSlice = inBuilder.slice();
  }

  auto const& nodeOps = node->_options;

  OperationOptions opOptions;
  opOptions.ignoreRevs = nodeOps.ignoreRevs;
  opOptions.keepNull = !nodeOps.nullMeansRemove;
  opOptions.mergeObjects = nodeOps.mergeObjects;
  opOptions.returnNew = !!NEW;
  opOptions.returnOld = (!!OLD) || out;
  opOptions.waitForSync = nodeOps.waitForSync;
  opOptions.silent = false;
  opOptions.overwrite = nodeOps.overwrite;

  std::unique_ptr<VPackBuilder> mergedBuilder;
  if (!_key.empty()) {
    mergedBuilder = merge(inSlice, _key, 0);
    inSlice = mergedBuilder->slice();
  }

  OperationResult result;
  if (node->_mode == ExecutionNode::NodeType::INDEX) {
    result = _trx->document(_collection->name(), inSlice, opOptions);
  } else if (node->_mode == ExecutionNode::NodeType::INSERT) {
    if (opOptions.returnOld && !opOptions.overwrite) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN,
          "OLD is only available when using INSERT with the overwrite option");
    }
    result = _trx->insert(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::REMOVE) {
    result = _trx->remove(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::REPLACE) {
    if (node->_replaceIndexNode && in == nullptr) {
      // we have a FOR .. IN FILTER doc._key == ... REPLACE - no WITH.
      // in this case replace needs to behave as if it was UPDATE.
      result = _trx->update(_collection->name(), inSlice, opOptions);
    } else {
      result = _trx->replace(_collection->name(), inSlice, opOptions);
    }
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::UPDATE) {
    result = _trx->update(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  }

  // check operation result
  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
        ((node->_mode == ExecutionNode::NodeType::INDEX) ||
         (node->_mode == ExecutionNode::NodeType::UPDATE && node->_replaceIndexNode) ||
         (node->_mode == ExecutionNode::NodeType::REMOVE && node->_replaceIndexNode) ||
         (node->_mode == ExecutionNode::NodeType::REPLACE && node->_replaceIndexNode))) {
      // document not there is not an error in this situation.
      // FOR ... FILTER ... REMOVE wouldn't invoke REMOVE in first place, so
      // don't throw an excetpion.
      return false;
    } else if (!nodeOps.ignoreErrors) {  // TODO remove if
      THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
    }

    if (node->_mode == ExecutionNode::NodeType::INDEX) {
      return false;
    }
  }

  _engine->_stats.writesExecuted += possibleWrites;
  _engine->_stats.scannedIndex++;

  if (!(out || OLD || NEW)) {
    return node->hasParent();
  }

  // Fill itemblock
  // create block that can hold a result with one entry and a number of
  // variables corresponding to the amount of out variables

  // only copy 1st row of registers inherited from previous frame(s)
  TRI_ASSERT(result.ok());
  VPackSlice outDocument = VPackSlice::nullSlice();
  if (result.buffer) {
    outDocument = result.slice().resolveExternal();
  }

  VPackSlice oldDocument = VPackSlice::nullSlice();
  VPackSlice newDocument = VPackSlice::nullSlice();
  if (outDocument.isObject()) {
    if (NEW && outDocument.hasKey("new")) {
      newDocument = outDocument.get("new");
    }
    if (outDocument.hasKey("old")) {
      outDocument = outDocument.get("old");
      if (OLD) {
        oldDocument = outDocument;
      }
    }
  }

  TRI_ASSERT(out || OLD || NEW);

  // place documents as in the out variable slots of the result
  if (out) {
    aqlres->emplaceValue(outputCounter,
                         static_cast<arangodb::aql::RegisterId>(outRegId), outDocument);
  }

  if (OLD) {
    TRI_ASSERT(opOptions.returnOld);
    aqlres->emplaceValue(outputCounter,
                         static_cast<arangodb::aql::RegisterId>(oldRegId), oldDocument);
  }

  if (NEW) {
    TRI_ASSERT(opOptions.returnNew);
    aqlres->emplaceValue(outputCounter,
                         static_cast<arangodb::aql::RegisterId>(newRegId), newDocument);
  }

  throwIfKilled();  // check if we were aborted

  TRI_IF_FAILURE("SingleRemoteOperationBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return true;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> SingleRemoteOperationBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_done) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  RegisterId nrRegs =
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];
  std::unique_ptr<AqlItemBlock> aqlres(requestBlock(atMost, nrRegs));

  int outputCounter = 0;
  if (_buffer.empty()) {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    ExecutionState state = ExecutionState::HASMORE;
    bool blockAppended = false;

    std::tie(state, blockAppended) = ExecutionBlock::getBlock(toFetch);
    if (state == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return {state, nullptr};
    }
    if (!blockAppended) {
      _done = true;
      traceGetSomeEnd(nullptr, ExecutionState::DONE);
      return {ExecutionState::DONE, nullptr};
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  arangodb::aql::AqlItemBlock* cur = _buffer.front();
  TRI_ASSERT(cur != nullptr);
  size_t n = cur->size();
  for (size_t i = 0; i < n; i++) {
    inheritRegisters(cur, aqlres.get(), _pos);
    if (getOne(aqlres.get(), outputCounter)) {
      outputCounter++;
    }
    _done = true;
    _pos++;
  }
  _buffer.pop_front();  // does not throw
  returnBlock(cur);
  _pos = 0;
  if (outputCounter == 0) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }
  aqlres->shrink(outputCounter);

  // Clear out registers no longer needed later:
  clearRegisters(aqlres.get());
  traceGetSomeEnd(aqlres.get(), ExecutionState::DONE);
  return {ExecutionState::DONE, std::move(aqlres)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> SingleRemoteOperationBlock::skipSome(size_t atMost) {
  TRI_ASSERT(false);  // as soon as we need to support LIMIT change me.
  return {ExecutionState::DONE, 0};
}
