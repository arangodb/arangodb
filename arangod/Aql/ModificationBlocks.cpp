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
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterMethods.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/OperationOptions.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

ModificationBlock::ModificationBlock(ExecutionEngine* engine,
                                     ModificationNode const* ep)
    : ExecutionBlock(engine, ep),
      _outRegOld(ExecutionNode::MaxRegisterId),
      _outRegNew(ExecutionNode::MaxRegisterId),
      _collection(ep->_collection),
      _isDBServer(false),
      _usesDefaultSharding(true) {

  _trx->pinData(_collection->cid());

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
  _isDBServer = transaction()->state()->isDBServer();

  if (_isDBServer) {
    _usesDefaultSharding = _collection->usesDefaultSharding();
  }
}

ModificationBlock::~ModificationBlock() {}

/// @brief get some - this accumulates all input and calls the work() method
AqlItemBlock* ModificationBlock::getSome(size_t atLeast, size_t atMost) {
  // for UPSERT operations, we read and write data in the same collection
  // we cannot use any batching here because if the search document is not
  // found, the UPSERTs INSERT operation may create it. after that, the
  // search document is present and we cannot use an already queried result
  // from the initial search batch
  traceGetSomeBegin();
  if (getPlanNode()->getType() == ExecutionNode::NodeType::UPSERT) {
    atLeast = 1;
    atMost = 1;
  }
  
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

  traceGetSomeEnd(replyBlocks.get());
  return replyBlocks.release();
}

/// @brief extract a key from the AqlValue passed
int ModificationBlock::extractKey(AqlValue const& value,
                                  std::string& key) {
  if (value.isObject()) {
    bool mustDestroy;
    AqlValue sub = value.get(_trx, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());
      return TRI_ERROR_NO_ERROR;
    }
  } else if (value.isString()) {
    key.assign(value.slice().copyString());
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
}

/// @brief process the result of a data-modification operation
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

/// @brief process the result of a data-modification operation
void ModificationBlock::handleBabyResult(std::unordered_map<int, size_t> const& errorCounter,
                                         size_t numBabies,
                                         bool ignoreAllErrors,
                                         bool ignoreDocumentNotFound) {
  if (errorCounter.empty()) {
    // update the success counter
    // All successful.
    _engine->_stats.writesExecuted += numBabies;
    return;
  }
  if (ignoreAllErrors) {
    for (auto const& pair : errorCounter) {
      // update the ignored counter
      _engine->_stats.writesIgnored += pair.second;
      numBabies -= pair.second;
    }

    // update the success counter
    _engine->_stats.writesExecuted += numBabies;
    return;
  }
  auto first = errorCounter.begin();
  if (ignoreDocumentNotFound && first->first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {

    if (errorCounter.size() == 1) {
      // We only have Document not found. Fix statistics and ignore
      // update the ignored counter
      _engine->_stats.writesIgnored += first->second;
      numBabies -= first->second;
      // update the success counter
      _engine->_stats.writesExecuted += numBabies;
      return;
    }

    // Sorry we have other errors as well.
    // No point in fixing statistics.
    // Throw other error.
    ++first;
    TRI_ASSERT(first != errorCounter.end());
  }

  THROW_ARANGO_EXCEPTION(first->first);
}
 
  
RemoveBlock::RemoveBlock(ExecutionEngine* engine, RemoveNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for removing data
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

  result.reset(requestBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  VPackBuilder keyBuilder;

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.ignoreRevs = true;
  options.returnOld = producesOutput;
  options.isRestore = ep->getOptions().useIsRestore;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();
    bool isMultiple = (n > 1);
    keyBuilder.clear();
    if (isMultiple) {
      // If we use multiple API we send an array
      keyBuilder.openArray();
    }

    std::string key;
    int errorCode = TRI_ERROR_NO_ERROR;
    // loop over the complete block
    // build the request block
    for (size_t i = 0; i < n; ++i) {
      AqlValue const& a = res->getValueReference(i, registerId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow + i);

      errorCode = TRI_ERROR_NO_ERROR;

      if (!ep->_options.consultAqlWriteFilter ||
          !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
        if (a.isObject()) {
          // value is an object. now extract the _key attribute
          key.clear();
          errorCode = extractKey(a, key);
        } else if (a.isString()) {
          // value is a string
          key = a.slice().copyString();
        } else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }

        if (errorCode == TRI_ERROR_NO_ERROR) {
          // no error. we expect to have a key
          // create a slice for the key
          keyBuilder.openObject();
          keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
          keyBuilder.close();
        } else {
          // We have an error, handle it
          handleResult(errorCode, ep->_options.ignoreErrors);
        }
      }
    }

    if (isMultiple) {
      // We have to close the array
      keyBuilder.close();
    }
    VPackSlice toRemove = keyBuilder.slice();

    if (!toRemove.isNone() &&
        !(toRemove.isArray() && toRemove.length() == 0)) {
      // all exceptions are caught in _trx->remove()
      OperationResult opRes = _trx->remove(_collection->name, toRemove, options);
      if (isMultiple) {
        TRI_ASSERT(opRes.successful());
        VPackSlice removedList = opRes.slice();
        TRI_ASSERT(removedList.isArray());
        if (producesOutput) {
          auto iter = VPackArrayIterator(removedList);
          for (size_t i = 0; i < n; ++i) {
            AqlValue const& a = res->getValueReference(i, registerId);
            if (!ep->_options.consultAqlWriteFilter ||
                !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
              TRI_ASSERT(iter.valid());
              auto it = iter.value();
              bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(
                  it, "error", false);
              errorCode = TRI_ERROR_NO_ERROR;
              if (wasError) {
                errorCode =
                    arangodb::basics::VelocyPackHelper::getNumericValue<int>(
                        it, "errorNum", TRI_ERROR_NO_ERROR);
              }
              if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
                  ignoreDocumentNotFound) {
                // Ignore document not found on the DBserver:
                errorCode = TRI_ERROR_NO_ERROR;
              }
              if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
                result->setValue(dstRow, _outRegOld,
                                 AqlValue(it.get("old")));
              }
              handleResult(errorCode, ep->_options.ignoreErrors);
              ++iter;
            }
            ++dstRow;
          }
        } else {
          handleBabyResult(opRes.countErrorCodes,
                           static_cast<size_t>(toRemove.length()),
                           ep->_options.ignoreErrors,
                           ignoreDocumentNotFound);
          dstRow += n;
        }
      } else {
        errorCode = opRes.code;
        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }
        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          result->setValue(dstRow, _outRegOld,
                           AqlValue(opRes.slice().get("old")));
        }
        handleResult(errorCode, ep->_options.ignoreErrors);
        ++dstRow;
      }
    } else {
      // Do not send request just increase the row
      dstRow += n;
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

/// @brief the actual work horse for inserting data
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

  result.reset(requestBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.returnNew = producesOutput;
  options.isRestore = ep->getOptions().useIsRestore;

  VPackBuilder babyBuilder;
  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);
    size_t const n = res->size();

    throwIfKilled();  // check if we were aborted
    bool const isMultiple = (n > 1);

    if (!isMultiple) {
      // loop over the complete block. Well it is one element only
      for (size_t i = 0; i < n; ++i) {
        AqlValue a = res->getValue(i, registerId);

        // only copy 1st row of registers inherited from previous frame(s)
        inheritRegisters(res, result.get(), i, dstRow);

        int errorCode = TRI_ERROR_NO_ERROR;

        if (!a.isObject()) {
          // value is no object
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        } else {
          if (!ep->_options.consultAqlWriteFilter ||
              !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
            OperationResult opRes = _trx->insert(_collection->name, a.slice(), options); 
            errorCode = opRes.code;

            if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
              // return $NEW
              result->setValue(dstRow, _outRegNew, AqlValue(opRes.slice().get("new")));
            }
          } else {
            errorCode = TRI_ERROR_NO_ERROR;
          }
        }

        handleResult(errorCode, ep->_options.ignoreErrors);
        ++dstRow;
      }
      // done with a block
    } else {
      babyBuilder.clear();
      babyBuilder.openArray();
      for (size_t i = 0; i < n; ++i) {
        AqlValue a = res->getValue(i, registerId);

        // only copy 1st row of registers inherited from previous frame(s)
        inheritRegisters(res, result.get(), i, dstRow);
        // TODO This may be optimized with externals
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
          babyBuilder.add(a.slice());
        }
        ++dstRow;
      }
      babyBuilder.close();
      VPackSlice toSend = babyBuilder.slice();
      OperationResult opRes;
      if (toSend.length() > 0) {
        opRes = _trx->insert(_collection->name, toSend, options);

        if (producesOutput) {
          // Reset dstRow
          dstRow -= n;
          VPackSlice resultList = opRes.slice();
          TRI_ASSERT(resultList.isArray());
          auto iter = VPackArrayIterator(resultList);
          for (size_t i = 0; i < n; ++i) {
            AqlValue a = res->getValue(i, registerId);
            if (!ep->_options.consultAqlWriteFilter ||
                !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
              TRI_ASSERT(iter.valid());
              auto elm = iter.value();
              bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(
                  elm, "error", false);
              if (!wasError) {
                // return $NEW
                result->setValue(dstRow, _outRegNew, AqlValue(elm.get("new")));
              }
              ++iter;
            }
            ++dstRow;
          }
        }

        handleBabyResult(opRes.countErrorCodes,
                         static_cast<size_t>(toSend.length()),
                         ep->_options.ignoreErrors);
      }
    }
    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

UpdateBlock::UpdateBlock(ExecutionEngine* engine, UpdateNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for inserting data
AqlItemBlock* UpdateBlock::work(std::vector<AqlItemBlock*>& blocks) {
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

  result.reset(requestBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnOld = (producesOutput && ep->_outVariableOld != nullptr);
  options.returnNew = (producesOutput && ep->_outVariableNew != nullptr);
  options.ignoreRevs = true;
  options.isRestore = ep->getOptions().useIsRestore;
        
  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);  // This is intentionally a copy!

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();
    bool isMultiple = (n > 1);
    object.clear();
    if (isMultiple) {
      object.openArray();
    }
      
    std::string key;

    // loop over the complete block
    std::vector<bool> wasTaken;
    wasTaken.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      inheritRegisters(res, result.get(), i, dstRow + i);

      AqlValue const& a = res->getValueReference(i, docRegisterId);

      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an object
        key.clear();
        if (hasKeyVariable) {
          // separate key specification
          AqlValue const& k = res->getValueReference(i, keyRegisterId);
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
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), key)) {
          wasTaken.push_back(true);
          if (hasKeyVariable) {
            keyBuilder.clear();
            keyBuilder.openObject();
            keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
            keyBuilder.close();

            VPackBuilder tmp = VPackCollection::merge(
                a.slice(), keyBuilder.slice(), false, false);
            if (isMultiple) {
              object.add(tmp.slice());
            } else {
              object = std::move(tmp);
            }
          }
          else {
            // use original slice for updating
            object.add(a.slice());
          }
        } else {
          wasTaken.push_back(false);
        }
      } else {
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
        wasTaken.push_back(false);
      }
    }

    if (isMultiple) {
      object.close();
    }

    VPackSlice toUpdate = object.slice();

    if (toUpdate.isNone() ||
        (toUpdate.isArray() && toUpdate.length() == 0)) {
      dstRow += n;
      continue;
    }

    // fetch old revision
    OperationResult opRes = _trx->update(_collection->name, toUpdate, options); 
    if (!isMultiple) {
      int errorCode = opRes.code;

      if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
        if (ep->_outVariableOld != nullptr) {
          // store $OLD
          result->setValue(dstRow, _outRegOld, AqlValue(opRes.slice().get("old")));
        }
        if (ep->_outVariableNew != nullptr) {
          // store $NEW
          result->setValue(dstRow, _outRegNew, AqlValue(opRes.slice().get("new")));
        }
      }

      if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
          ignoreDocumentNotFound) {
        // Ignore document not found on the DBserver:
        errorCode = TRI_ERROR_NO_ERROR;
      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      ++dstRow;
    } else {
      handleBabyResult(opRes.countErrorCodes,
                       static_cast<size_t>(toUpdate.length()),
                       ep->_options.ignoreErrors, ignoreDocumentNotFound);
      if (producesOutput) {
        VPackSlice resultList = opRes.slice();
        TRI_ASSERT(resultList.isArray());
        auto iter = VPackArrayIterator(resultList);
        for (size_t i = 0; i < n; ++i) {
          if (wasTaken[i]) {
            TRI_ASSERT(iter.valid());
            auto elm = iter.value();
            bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(elm, "error", false);
            if (!wasError) {
              if (ep->_outVariableOld != nullptr) {
                // store $OLD
                result->setValue(dstRow, _outRegOld, AqlValue(elm.get("old")));
              }
              if (ep->_outVariableNew != nullptr) {
                // store $NEW
                result->setValue(dstRow, _outRegNew, AqlValue(elm.get("new")));
              }
            }
            ++iter;
          }
          dstRow++;
        }
      } else {
        dstRow += n;
      }
    }
    // done with a block

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

UpsertBlock::UpsertBlock(ExecutionEngine* engine, UpsertNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for inserting data
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

  result.reset(requestBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnNew = producesOutput;
  options.ignoreRevs = true;
  options.isRestore = ep->getOptions().useIsRestore;
  
  VPackBuilder keyBuilder;
  VPackBuilder insertBuilder;
  VPackBuilder updateBuilder;

  // loop over all blocks
  size_t dstRow = 0;
  std::vector<size_t> insRows;
  std::vector<size_t> upRows;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = *it; 

    throwIfKilled();  // check if we were aborted
    
    insertBuilder.clear();
    updateBuilder.clear();

    size_t const n = res->size();
      
    bool const isMultiple = (n > 1);
    if (isMultiple) {
      insertBuilder.openArray();
      updateBuilder.openArray();
    }
    insRows.clear();
    upRows.clear();

    int errorCode;
    std::string key;

    // loop over the complete block
    // Prepare both builders
    std::vector<bool> wasTaken;
    wasTaken.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      AqlValue const& a = res->getValueReference(i, docRegisterId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      errorCode = TRI_ERROR_NO_ERROR;

      bool tookThis = false;

      if (a.isObject()) {
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
          // old document present => update case
          key.clear();
          errorCode = extractKey(a, key);

          if (errorCode == TRI_ERROR_NO_ERROR) {
            AqlValue const& updateDoc = res->getValueReference(i, updateRegisterId);

            if (updateDoc.isObject()) {
              tookThis = true;
              VPackSlice toUpdate = updateDoc.slice();
           
              keyBuilder.clear();
              keyBuilder.openObject();
              keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
              keyBuilder.close();
              if (isMultiple) {
                VPackBuilder tmp = VPackCollection::merge(toUpdate, keyBuilder.slice(), false, false);
                updateBuilder.add(tmp.slice());
                upRows.emplace_back(dstRow);
              } else {
                updateBuilder = VPackCollection::merge(toUpdate, keyBuilder.slice(), false, false);
              }

            } else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
            }
          }
        }
      } else {
        // no document found => insert case
        AqlValue const& insertDoc = res->getValueReference(i, insertRegisterId);
        VPackSlice toInsert = insertDoc.slice();
        if (toInsert.isObject()) {
          if (!ep->_options.consultAqlWriteFilter ||
              !_collection->getCollection()->skipForAqlWrite(toInsert, "")) {
            tookThis = true;
            insertBuilder.add(toInsert);
            insRows.emplace_back(dstRow);
          }
        } else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }
      }

      if (errorCode != TRI_ERROR_NO_ERROR) {
        // Handle the error here, it won't be send to server
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      }

      wasTaken.push_back(tookThis);
      ++dstRow;
    }
    // done with collecting a block

    if (isMultiple) {
      insertBuilder.close();
      updateBuilder.close();
    }

    VPackSlice toInsert = insertBuilder.slice();
    VPackSlice toUpdate = updateBuilder.slice();

    if (!toInsert.isNone()) {
      if (isMultiple) {
        TRI_ASSERT(toInsert.isArray());
        if (toInsert.length() != 0) {
          OperationResult opRes = _trx->insert(_collection->name, toInsert, options);
          if (producesOutput) {
            VPackSlice resultList = opRes.slice();
            TRI_ASSERT(resultList.isArray());
            size_t i = 0;
            for (auto const& elm : VPackArrayIterator(resultList)) {
              bool wasError =
                  arangodb::basics::VelocyPackHelper::getBooleanValue(
                      elm, "error", false);
              if (!wasError) {
                // return $NEW
                result->setValue(insRows[i], _outRegNew, AqlValue(elm.get("new")));
              }
              ++i;
            }
          }
          handleBabyResult(opRes.countErrorCodes,
                           static_cast<size_t>(toInsert.length()),
                           ep->_options.ignoreErrors);
        }
      } else {
        OperationResult opRes = _trx->insert(_collection->name, toInsert, options);
        errorCode = opRes.code; 

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          result->setValue(dstRow - 1, _outRegNew, AqlValue(opRes.slice().get("new")));
        }
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      }
    }

    if (!toUpdate.isNone()) {
      if (isMultiple) {
        TRI_ASSERT(toUpdate.isArray());
        if (toUpdate.length() != 0) {
          OperationResult opRes;
          if (ep->_isReplace) {
            // replace
            opRes = _trx->replace(_collection->name, toUpdate, options);
          } else {
            // update
            opRes = _trx->update(_collection->name, toUpdate, options);
          }
          handleBabyResult(opRes.countErrorCodes,
                           static_cast<size_t>(toUpdate.length()),
                           ep->_options.ignoreErrors);
          if (producesOutput) {
            VPackSlice resultList = opRes.slice();
            TRI_ASSERT(resultList.isArray());
            size_t i = 0;
            for (auto const& elm : VPackArrayIterator(resultList)) {
              bool wasError =
                  arangodb::basics::VelocyPackHelper::getBooleanValue(
                      elm, "error", false);
              if (!wasError) {
                // return $NEW
                result->setValue(upRows[i], _outRegNew, AqlValue(elm.get("new")));
              }
              ++i;
            }
          }
        }
      } else {
        OperationResult opRes;
        if (ep->_isReplace) {
          // replace
          opRes = _trx->replace(_collection->name, toUpdate, options);
        } else {
          // update
          opRes = _trx->update(_collection->name, toUpdate, options);
        }
        errorCode = opRes.code;

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          // store $NEW
          result->setValue(dstRow - 1, _outRegNew, AqlValue(opRes.slice().get("new")));
        }
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      }
    }

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}

ReplaceBlock::ReplaceBlock(ExecutionEngine* engine, ReplaceNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for replacing data
AqlItemBlock* ReplaceBlock::work(std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<ReplaceNode const*>(getPlanNode());
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

  result.reset(requestBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnOld = (producesOutput && ep->_outVariableOld != nullptr);
  options.returnNew = (producesOutput && ep->_outVariableNew != nullptr);
  options.ignoreRevs = true;
  options.isRestore = ep->getOptions().useIsRestore;
        
  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);  // This is intentionally a copy!

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();
    bool isMultiple = (n > 1);
    object.clear();
    if (isMultiple) {
      object.openArray();
    }
      
    std::string key;

    // loop over the complete block
    std::vector<bool> wasTaken;
    wasTaken.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      inheritRegisters(res, result.get(), i, dstRow + i);

      AqlValue const& a = res->getValueReference(i, docRegisterId);

      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an object
        key.clear();
        if (hasKeyVariable) {
          // separate key specification
          AqlValue const& k = res->getValueReference(i, keyRegisterId);
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
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), key)) {
          wasTaken.push_back(true);
          if (hasKeyVariable) {
            keyBuilder.clear();
            keyBuilder.openObject();
            keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
            keyBuilder.close();
            VPackBuilder tmp = VPackCollection::merge(
                a.slice(), keyBuilder.slice(), false, false);
            if (isMultiple) {
              object.add(tmp.slice());
            } else {
              object = std::move(tmp);
            }
          } else {
            // Use the original slice for updating
            object.add(a.slice());
          }
        } else {
          wasTaken.push_back(false);
        }
      } else {
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
        wasTaken.push_back(false);
      }
    }

    if (isMultiple) {
      object.close();
    }

    VPackSlice toUpdate = object.slice();

    if (toUpdate.isNone() ||
        (toUpdate.isArray() && toUpdate.length() == 0)) {
      dstRow += n;
      continue;
    }
    // fetch old revision
    OperationResult opRes = _trx->replace(_collection->name, toUpdate, options); 
    if (!isMultiple) {
      int errorCode = opRes.code;

      if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
        if (ep->_outVariableOld != nullptr) {
          // store $OLD
          result->setValue(dstRow, _outRegOld,
                           AqlValue(opRes.slice().get("old")));
        }
        if (ep->_outVariableNew != nullptr) {
          // store $NEW
          result->setValue(dstRow, _outRegNew,
                           AqlValue(opRes.slice().get("new")));
        }
      }

      if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
          ignoreDocumentNotFound) {
        // Ignore document not found on the DBserver:
        errorCode = TRI_ERROR_NO_ERROR;
      }
      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      ++dstRow;
    } else {
      handleBabyResult(opRes.countErrorCodes,
                       static_cast<size_t>(toUpdate.length()),
                       ep->_options.ignoreErrors, ignoreDocumentNotFound);
      if (producesOutput) {
        VPackSlice resultList = opRes.slice();
        TRI_ASSERT(resultList.isArray());
        auto iter = VPackArrayIterator(resultList);
        for (size_t i = 0; i < n; ++i) {
          if (wasTaken[i]) {
            TRI_ASSERT(iter.valid());
            auto elm = iter.value();
            bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(elm, "error", false);
            if (!wasError) {
              if (ep->_outVariableOld != nullptr) {
                // store $OLD
                result->setValue(dstRow, _outRegOld, AqlValue(elm.get("old")));
              }
              if (ep->_outVariableNew != nullptr) {
                // store $NEW
                result->setValue(dstRow, _outRegNew, AqlValue(elm.get("new")));
              }
            }
            ++iter;
          }
          dstRow++;
        }
      } else {
        dstRow += n;
      }
    }

    // done with a block

    // now free it already
    (*it) = nullptr;
    delete res;
  }

  return result.release();
}
