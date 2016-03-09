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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ModificationBlocks.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/json-utilities.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterMethods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

ModificationBlock::ModificationBlock(ExecutionEngine* engine,
                                     ModificationNode const* ep)
    : ExecutionBlock(engine, ep),
      _outRegOld(ExecutionNode::MaxRegisterId),
      _outRegNew(ExecutionNode::MaxRegisterId),
      _collection(ep->_collection),
      _isDBServer(false),
      _usesDefaultSharding(true),
      _buffer(TRI_UNKNOWN_MEM_ZONE) {

  _trx->orderDitch(_collection->cid());

  auto const& registerPlan = ep->getRegisterPlan()->varInfo;

  if (ep->_outVariableOld != nullptr) {
    auto it = registerPlan.find(ep->_outVariableOld->id);
    TRI_ASSERT(it != registerPlan.end());
    _outRegOld = (*it).second.registerId;
  }
  if (ep->_outVariableNew != nullptr) {
    auto it = registerPlan.find(ep->_outVariableNew->id);
    TRI_ASSERT(it != registerPlan.end());
    _outRegNew = (*it).second.registerId;
  }

  // check if we're a DB server in a cluster
  _isDBServer = arangodb::ServerState::instance()->isDBServer();

  if (_isDBServer) {
    _usesDefaultSharding = _collection->usesDefaultSharding();
  }
}

ModificationBlock::~ModificationBlock() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief get some - this accumulates all input and calls the work() method
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ModificationBlock::getSome(size_t atLeast, size_t atMost) {
  std::vector<AqlItemBlock*> blocks;
  std::unique_ptr<AqlItemBlock> replyBlocks;

  auto freeBlocks = [](std::vector<AqlItemBlock*>& blocks) {
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      if ((*it) != nullptr) {
        delete (*it);
      }
    }
    blocks.clear();
  };

  // loop over input until it is exhausted
  try {
    if (static_cast<ModificationNode const*>(_exeNode)
            ->_options.readCompleteInput) {
      // read all input into a buffer first
      while (true) {
        std::unique_ptr<AqlItemBlock> res(
            ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

        if (res.get() == nullptr) {
          break;
        }

        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        blocks.emplace_back(res.get());
        res.release();
      }

      // now apply the modifications for the complete input
      replyBlocks.reset(work(blocks));
    } else {
      // read input in chunks, and process it in chunks
      // this reduces the amount of memory used for storing the input
      while (true) {
        freeBlocks(blocks);
        std::unique_ptr<AqlItemBlock> res(
            ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

        if (res.get() == nullptr) {
          break;
        }

        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        blocks.emplace_back(res.get());
        res.release();

        replyBlocks.reset(work(blocks));

        if (replyBlocks.get() != nullptr) {
          break;
        }
      }
    }
  } catch (...) {
    freeBlocks(blocks);
    throw;
  }

  freeBlocks(blocks);

  return replyBlocks.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from the AqlValue$ passed
////////////////////////////////////////////////////////////////////////////////

int ModificationBlock::extractKey(AqlValue$ const& value,
                                  std::string& key) {
  if (value.isObject()) {
    VPackSlice const slice = value.get(TRI_VOC_ATTRIBUTE_KEY).slice();
    if (slice.isString()) {
      key.assign(slice.copyString());
      return TRI_ERROR_NO_ERROR;
    }
  } else if (value.slice().isString()) {
    key.assign(value.slice().copyString());
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a shard key value has changed
////////////////////////////////////////////////////////////////////////////////

bool ModificationBlock::isShardKeyChange(TRI_json_t const* oldJson,
                                         TRI_json_t const* newJson,
                                         bool isPatch) const {
  TRI_ASSERT(_isDBServer);

  auto planId = _collection->documentCollection()->_info.planId();
  auto vocbase = static_cast<ModificationNode const*>(_exeNode)->_vocbase;
  return arangodb::shardKeysChanged(vocbase->_name, std::to_string(planId),
                                    oldJson, newJson, isPatch);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the _key attribute is specified when it must not be
/// specified
////////////////////////////////////////////////////////////////////////////////

bool ModificationBlock::isShardKeyError(TRI_json_t const* json) const {
  TRI_ASSERT(_isDBServer);

  if (_usesDefaultSharding) {
    return false;
  }

  return (TRI_LookupObjectJson(json, TRI_VOC_ATTRIBUTE_KEY) != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the result of a data-modification operation
////////////////////////////////////////////////////////////////////////////////

void ModificationBlock::handleResult(int code, bool ignoreErrors,
                                     std::string const* errorMessage) {
  if (code == TRI_ERROR_NO_ERROR) {
    // update the success counter
    ++_engine->_stats.writesExecuted;
    return;
  }

  if (ignoreErrors) {
    // update the ignored counter
    ++_engine->_stats.writesIgnored;
    return;
  }

  // bubble up the error
  if (errorMessage != nullptr && !errorMessage->empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(code, *errorMessage);
  }

  THROW_ARANGO_EXCEPTION(code);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief execute an update or replace document modification operation
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ModificationBlock::modify(std::vector<AqlItemBlock*>& blocks, 
                                        bool isReplace) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<UpdateNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0;  // default initialization

  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput =
      (ep->_outVariableOld != nullptr || ep->_outVariableNew != nullptr);

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  std::string errorMessage;
  VPackBuilder keyBuilder;
  VPackBuilder object;

  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  result.reset(new AqlItemBlock(
      count,
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.ignoreRevs = true;
        
  OperationResult opResOld; // old document ($OLD)

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);  // This is intentionally a copy!

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();

    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue$ const& a = res->getValueReference(i, docRegisterId);

      int errorCode = TRI_ERROR_NO_ERROR;
      std::string key;

      if (a.isObject()) {
        // value is an object
        if (hasKeyVariable) {
          // seperate key specification
          AqlValue$ const& k = res->getValueReference(i, keyRegisterId);
          errorCode = extractKey(k, key);
        } else {
          errorCode = extractKey(a, key);
        }
      } else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        errorMessage += std::string("expecting 'Object', got: ") +
                        a.slice().typeName() + std::string(" while handling: ") +
                        _exeNode->getTypeString();
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        keyBuilder.clear();
        keyBuilder.openObject();
        keyBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
        keyBuilder.close();

        VPackSlice toInsert;

        if (hasKeyVariable) {
          object = VPackCollection::merge(a.slice(), keyBuilder.slice(), false, false);
          toInsert = object.slice();
        }
        else {
          // use original slice for inserting
          toInsert = a.slice();
        }

        // fetch old revision
        if (producesOutput && ep->_outVariableOld != nullptr) {
          opResOld = _trx->document(_collection->name, keyBuilder.slice(), options);
        }

        OperationResult opRes = _trx->update(_collection->name, toInsert, options); 
        errorCode = opRes.code;

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          if (ep->_outVariableOld != nullptr) {
            // store $OLD
            result->setValue(dstRow, _outRegOld, AqlValue$(opResOld.slice()));
          }

          if (ep->_outVariableNew != nullptr) {
            // store $NEW
            OperationResult opResNew = _trx->document(_collection->name, keyBuilder.slice(), options);

            if (opResNew.successful()) {
              result->setValue(dstRow, _outRegNew, AqlValue$(opResNew.slice()));
            }
          }
        }

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

RemoveBlock::RemoveBlock(ExecutionEngine* engine, RemoveNode const* ep)
    : ModificationBlock(engine, ep) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for removing data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* RemoveBlock::work(std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;

  auto ep = static_cast<RemoveNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr);

  result.reset(new AqlItemBlock(
      count,
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  VPackBuilder keyBuilder;

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.ignoreRevs = true;

  OperationResult opResOld; // old document ($OLD)

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();

    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue$ const& a = res->getValueReference(i, registerId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      std::string key;
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an array. now extract the _key attribute
        errorCode = extractKey(a, key);
      } else if (a.slice().isString()) {
        // value is a string
        key = a.slice().copyString();
      } else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        // no error. we expect to have a key
        // create a slice for the key
        keyBuilder.clear();
        keyBuilder.openObject();
        keyBuilder.add(VPackValue(key));
        keyBuilder.close();

        VPackSlice toRemove = keyBuilder.slice();
      
        if (producesOutput) {
          // read "old" version
          // need to fetch the old document
          opResOld = _trx->document(_collection->name, toRemove, options);
        }

        // all exceptions are caught in _trx->remove()
        OperationResult opRes = _trx->remove(_collection->name, toRemove, options);
        errorCode = opRes.code;

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR && opResOld.successful()) {
          result->setValue(dstRow, _outRegOld, AqlValue$(opResOld.slice()));
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors);
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

InsertBlock::InsertBlock(ExecutionEngine* engine, InsertNode const* ep)
    : ModificationBlock(engine, ep) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* InsertBlock::work(std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;

  auto ep = static_cast<InsertNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  bool const producesOutput = (ep->_outVariableNew != nullptr);

  result.reset(new AqlItemBlock(
      count,
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);
    size_t const n = res->size();

    throwIfKilled();  // check if we were aborted

    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue$ a = res->getValue(i, registerId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      int errorCode = TRI_ERROR_NO_ERROR;

      if (!a.isObject()) {
        // value is no object
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      } else {
        OperationResult opRes = _trx->insert(_collection->name, a.slice(), options); 
        errorCode = opRes.code;

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          // return $NEW
          OperationResult opResNew = _trx->document(_collection->name, a.slice(), options);

          if (opResNew.successful()) {
            result->setValue(dstRow, _outRegNew, AqlValue$(opResNew.slice()));
          }
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors);
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

UpdateBlock::UpdateBlock(ExecutionEngine* engine, UpdateNode const* ep)
    : ModificationBlock(engine, ep) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* UpdateBlock::work(std::vector<AqlItemBlock*>& blocks) {
  return modify(blocks, false);
}

UpsertBlock::UpsertBlock(ExecutionEngine* engine, UpsertNode const* ep)
    : ModificationBlock(engine, ep) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* UpsertBlock::work(std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<UpsertNode const*>(getPlanNode());

  auto const& registerPlan = ep->getRegisterPlan()->varInfo;
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;

  it = registerPlan.find(ep->_insertVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  RegisterId const insertRegisterId = it->second.registerId;

  it = registerPlan.find(ep->_updateVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  RegisterId const updateRegisterId = it->second.registerId;

  bool const producesOutput = (ep->_outVariableNew != nullptr);

  std::string errorMessage;

  result.reset(new AqlItemBlock(
      count,
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.ignoreRevs = true;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);  // This is intentionally a copy!

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();

    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue$ const& a = res->getValueReference(i, docRegisterId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      std::string key;

      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // old document present => update case
        errorCode = extractKey(a, key);

        if (errorCode == TRI_ERROR_NO_ERROR) {
          AqlValue$ const& updateDoc = res->getValueReference(i, updateRegisterId);

          if (updateDoc.isObject()) {
            VPackSlice toUpdate = updateDoc.slice();

            if (ep->_isReplace) {
              // replace
              OperationResult opRes = _trx->replace(_collection->name, toUpdate, options);
              errorCode = opRes.code;
            } else {
              // update
              OperationResult opRes = _trx->update(_collection->name, toUpdate, options);
              errorCode = opRes.code;
            }

            if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
              // store $NEW
              OperationResult opResNew = _trx->document(_collection->name, toUpdate, options);

              if (opResNew.successful()) {
                result->setValue(dstRow, _outRegNew, AqlValue$(opResNew.slice()));
              }
            }
          } else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
          }
        }

      } else {
        // no document found => insert case
        AqlValue$ const& insertDoc = res->getValueReference(i, insertRegisterId);

        if (insertDoc.isObject()) {
          OperationResult opRes = _trx->insert(_collection->name, insertDoc.slice(), options);
          errorCode = opRes.code; 

          if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
            OperationResult opResNew = _trx->document(_collection->name, insertDoc.slice(), options);

            if (opResNew.successful()) {
              result->setValue(dstRow, _outRegNew, AqlValue$(opResNew.slice()));
            }
          }
        } else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

ReplaceBlock::ReplaceBlock(ExecutionEngine* engine, ReplaceNode const* ep)
    : ModificationBlock(engine, ep) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for replacing data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ReplaceBlock::work(std::vector<AqlItemBlock*>& blocks) {
  return modify(blocks, true);
}
