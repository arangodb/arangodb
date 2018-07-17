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
#include "Basics/StaticStrings.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
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
      _usesDefaultSharding(true),
      _countStats(ep->countStats()) {

  _trx->pinData(_collection->id());

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
  _isDBServer = ServerState::instance()->isDBServer();

  if (_isDBServer) {
    _usesDefaultSharding = _collection->usesDefaultSharding();
  }
}

ModificationBlock::~ModificationBlock() {}

/// @brief determine the number of rows in a vector of blocks
size_t ModificationBlock::countBlocksRows() const {
  size_t count = 0;
  for (auto const& it : _blocks) {
    count += it->size();
  }
  return count;
}

ExecutionState ModificationBlock::getHasMoreState() {
  // In these blocks everything from upstream
  // is entirely processed in one go.
  // So if upstream is done, we are done.
  return _upstreamState;
}

/// @brief get some - this accumulates all input and calls the work() method
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ModificationBlock::getSome(size_t atMost) {
  // for UPSERT operations, we read and write data in the same collection
  // we cannot use any batching here because if the search document is not
  // found, the UPSERTs INSERT operation may create it. after that, the
  // search document is present and we cannot use an already queried result
  // from the initial search batch
  traceGetSomeBegin(atMost);
  if (getPlanNode()->getType() == ExecutionNode::NodeType::UPSERT) {
    atMost = 1;
  }

  std::unique_ptr<AqlItemBlock> replyBlocks;

  // loop over input until it is exhausted
  if (ExecutionNode::castTo<ModificationNode const*>(_exeNode)
          ->_options.readCompleteInput) {
    // read all input into a buffer first
    while (_upstreamState == ExecutionState::HASMORE) {
      auto upstreamRes = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
      if (upstreamRes.first == ExecutionState::WAITING) {
        traceGetSomeEnd(nullptr, ExecutionState::WAITING);
        return upstreamRes;
      }

      _upstreamState = upstreamRes.first;
      if (upstreamRes.second != nullptr) {
        _blocks.emplace_back(std::move(upstreamRes.second));
      } else {
        TRI_ASSERT(_upstreamState == ExecutionState::DONE);
      }
      TRI_IF_FAILURE("ModificationBlock::getSome") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    // now apply the modifications for the complete input
    replyBlocks = work();
    _blocks.clear();
  } else {
    // read input in chunks, and process it in chunks
    // this reduces the amount of memory used for storing the input
    while (_upstreamState == ExecutionState::HASMORE) {
      auto upstreamRes = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
      if (upstreamRes.first == ExecutionState::WAITING) {
        traceGetSomeEnd(nullptr, ExecutionState::WAITING);
        return upstreamRes;
      }

      _upstreamState = upstreamRes.first;

      if (upstreamRes.second != nullptr) {
        _blocks.emplace_back(std::move(upstreamRes.second));
        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        replyBlocks = work();
        _blocks.clear();
        if (replyBlocks != nullptr) {
          break;
        }
      }
    }
  }

  traceGetSomeEnd(replyBlocks.get(), getHasMoreState());
  
  return {getHasMoreState(), std::move(replyBlocks)};
}

std::pair<ExecutionState, arangodb::Result> ModificationBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  _blocks.clear();
  return ExecutionBlock::initializeCursor(items, pos);
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

int ModificationBlock::extractKeyAndRev(AqlValue const& value,
                                         std::string& key,
                                         std::string& rev) {
  if (value.isObject()) {
    bool mustDestroy;
    AqlValue sub = value.get(_trx, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());

      bool mustDestroyToo;
      AqlValue subTwo = value.get(_trx, StaticStrings::RevString, mustDestroyToo, false);
      AqlValueGuard guard(subTwo, mustDestroyToo);
      if (subTwo.isString()) {
        rev.assign(subTwo.slice().copyString());
      }
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
    if (_countStats) {
      ++_engine->_stats.writesExecuted;
    }
    return;
  }

  if (ignoreErrors) {
    // update the ignored counter
    if (_countStats) {
      ++_engine->_stats.writesIgnored;
    }
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
    if (_countStats) {
      _engine->_stats.writesExecuted += numBabies;
    }
    return;
  }
  if (ignoreAllErrors) {
    for (auto const& pair : errorCounter) {
      // update the ignored counter
      if (_countStats) {
        _engine->_stats.writesIgnored += pair.second;
      }
      numBabies -= pair.second;
    }

    // update the success counter
    if (_countStats) {
      _engine->_stats.writesExecuted += numBabies;
    }
    return;
  }
  auto first = errorCounter.begin();
  if (ignoreDocumentNotFound && first->first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {

    if (errorCounter.size() == 1) {
      // We only have Document not found. Fix statistics and ignore
      // update the ignored counter
      if (_countStats) {
        _engine->_stats.writesIgnored += first->second;
      }
      numBabies -= first->second;
      // update the success counter
      if (_countStats) {
        _engine->_stats.writesExecuted += numBabies;
      }
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
std::unique_ptr<AqlItemBlock> RemoveBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;

  auto ep = ExecutionNode::castTo<RemoveNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr);

  result.reset(requestBlock(count, getNrOutputRegisters()));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.ignoreRevs = ep->getOptions().ignoreRevs;
  options.returnOld = producesOutput;
  options.isRestore = ep->getOptions().useIsRestore;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

    throwIfKilled();  // check if we were aborted

    size_t const n = res->size();
    bool isMultiple = (n > 1);
    _tempBuilder.clear();
    if (isMultiple) {
      // If we use multiple API we send an array
      _tempBuilder.openArray();
    }

    std::string key;
    std::string rev;
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
          key.clear();
          if (!options.ignoreRevs) {
            rev.clear();
            // value is an object. now extract the _key and _rev attribute
            errorCode = extractKeyAndRev(a, key, rev);
          } else {
            // value is an object. now extract the _key attribute
            errorCode = extractKey(a, key);
          }
        } else if (a.isString()) {
          // value is a string
          key = a.slice().copyString();
        } else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }

        if (errorCode == TRI_ERROR_NO_ERROR) {
          // no error. we expect to have a key
          // create a slice for the key
          _tempBuilder.openObject();
          _tempBuilder.add(StaticStrings::KeyString, VPackValue(key));
          if (!options.ignoreRevs && !rev.empty()) {
            _tempBuilder.add(StaticStrings::RevString, VPackValue(rev));
          }
          _tempBuilder.close();
        } else {
          // We have an error, handle it
          handleResult(errorCode, ep->_options.ignoreErrors, nullptr);
        }
      }
    }

    if (isMultiple) {
      // We have to close the array
      _tempBuilder.close();
    }
    VPackSlice toRemove = _tempBuilder.slice();

    if (!toRemove.isNone() &&
        !(toRemove.isArray() && toRemove.length() == 0)) {
      // all exceptions are caught in _trx->remove()
      OperationResult opRes = _trx->remove(_collection->name(), toRemove, options);

      if (isMultiple) {
        TRI_ASSERT(opRes.ok());
        if (producesOutput) {
          TRI_ASSERT(options.returnOld);
          VPackSlice removedList = opRes.slice();
          TRI_ASSERT(removedList.isArray());
          auto iter = VPackArrayIterator(removedList);
          for (size_t i = 0; i < n; ++i) {
            AqlValue const& a = res->getValueReference(i, registerId);
            if (!ep->_options.consultAqlWriteFilter ||
                !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
              TRI_ASSERT(iter.valid());
              auto it = iter.value();
              bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(
                  it, "error", false);
              errorCode =
                  arangodb::basics::VelocyPackHelper::getNumericValue<int>(
                      it, "errorNum", TRI_ERROR_NO_ERROR);
              if (!wasError) {
                if (errorCode == TRI_ERROR_NO_ERROR) {
                  result->emplaceValue(dstRow, _outRegOld, it.get("old"));
                }
              }
              ++iter;
              if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
                  ignoreDocumentNotFound) {
                // Ignore document not found on the DBserver:
                continue;
              }
              handleResult(errorCode, ep->_options.ignoreErrors, nullptr);
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
        errorCode = opRes.errorNumber();

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          // do not emit a new row
          continue;
        }
        if (options.returnOld && errorCode == TRI_ERROR_NO_ERROR) {
          result->emplaceValue(dstRow, _outRegOld, opRes.slice().get("old"));
        }
        handleResult(errorCode, ep->_options.ignoreErrors, nullptr);
        ++dstRow;
      }
    } else {
      // Do not send request just increase the row
      dstRow += n;
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  return result;
}

InsertBlock::InsertBlock(ExecutionEngine* engine, InsertNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for inserting data
std::unique_ptr<AqlItemBlock> InsertBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;

  auto ep = ExecutionNode::castTo<InsertNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  std::string errorMessage;
  bool const producesNew = (ep->_outVariableNew != nullptr);
  bool const producesOld = (ep->_outVariableOld != nullptr);
  bool const producesOutput = producesNew || producesOld;

  result.reset(requestBlock(count, getNrOutputRegisters()));

  OperationOptions options;
  // use "silent" mode if we do not access the results later on
  options.silent = !producesOutput;
  options.returnNew = producesNew;
  options.returnOld = producesOld;
  options.isRestore = ep->_options.useIsRestore;
  options.waitForSync = ep->_options.waitForSync;
  options.overwrite = ep->_options.overwrite;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();
    size_t const n = res->size();

    throwIfKilled();  // check if we were aborted
    bool const isMultiple = (n > 1);

    if (!isMultiple) { // single - case
      // loop over the complete block. Well it is one element only
      for (size_t i = 0; i < n; ++i) {
        AqlValue const& a = res->getValueReference(i, registerId);

        // only copy 1st row of registers inherited from previous frame(s)
        inheritRegisters(res, result.get(), i, dstRow);

        errorMessage.clear();
        int errorCode = TRI_ERROR_NO_ERROR;

        if (!a.isObject()) {
          // value is no object
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        } else {
          if (!ep->_options.consultAqlWriteFilter ||
              !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
            OperationResult opRes = _trx->insert(_collection->name(), a.slice(), options);
            errorCode = opRes.errorNumber();

            if (errorCode == TRI_ERROR_NO_ERROR) {
              if (options.returnNew) {
                // return $NEW
                result->emplaceValue(dstRow, _outRegNew, opRes.slice().get("new"));
              }
              if (options.returnOld) {
                // return $OLD
                auto slice = opRes.slice().get("old");
                if(slice.isNone()){
                  result->emplaceValue(dstRow, _outRegOld, VPackSlice::nullSlice());
                } else {
                  result->emplaceValue(dstRow, _outRegOld, slice);
                }
              }
            } else {
              errorMessage.assign(opRes.errorMessage());
            }
          } else {
            errorCode = TRI_ERROR_NO_ERROR;
          }
        }

        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
        ++dstRow;
      }
      // done with a block
    } else { // many - case
      _tempBuilder.clear();
      _tempBuilder.openArray();
      for (size_t i = 0; i < n; ++i) {
        AqlValue const& a = res->getValueReference(i, registerId);

        // only copy 1st row of registers inherited from previous frame(s)
        inheritRegisters(res, result.get(), i, dstRow);
        // TODO This may be optimized with externals
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
          _tempBuilder.add(a.slice());
        }
        ++dstRow;
      }
      _tempBuilder.close();
      VPackSlice toSend = _tempBuilder.slice();
      OperationResult opRes;
      if (toSend.length() > 0) {
        opRes = _trx->insert(_collection->name(), toSend, options);

        if (producesOutput) {
          // Reset dstRow
          dstRow -= n;
          VPackSlice resultList = opRes.slice();
          TRI_ASSERT(resultList.isArray());
          auto iter = VPackArrayIterator(resultList);
          for (size_t i = 0; i < n; ++i) {
            AqlValue const& a = res->getValueReference(i, registerId);
            if (!ep->_options.consultAqlWriteFilter ||
                !_collection->getCollection()->skipForAqlWrite(a.slice(), "")) {
              TRI_ASSERT(iter.valid());
              auto elm = iter.value();
              bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(
                  elm, "error", false);
              if (!wasError) {
                if (producesNew) {
                  // store $NEW
                  result->emplaceValue(dstRow, _outRegNew, elm.get("new"));
                }
                if (producesOld) {
                  // store $OLD
                  auto slice = elm.get("old");
                  if(slice.isNone()){
                    result->emplaceValue(dstRow, _outRegOld, VPackSlice::nullSlice());
                  } else {
                    result->emplaceValue(dstRow, _outRegOld, slice);
                  }
                }
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
    } // single / many - case

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  return result;
}

UpdateBlock::UpdateBlock(ExecutionEngine* engine, UpdateNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for inserting data
std::unique_ptr<AqlItemBlock> UpdateBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  auto ep = ExecutionNode::castTo<UpdateNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0;  // default initialization

  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool producesOutput = (ep->_outVariableOld != nullptr || ep->_outVariableNew != nullptr);

  if (!producesOutput && _isDBServer && ignoreDocumentNotFound) {
    // on a DB server, when we are told to ignore missing documents, we must
    // set this flag in order to not assert later on
    producesOutput = true;
  }

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  std::string errorMessage;
  VPackBuilder object;

  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  std::unique_ptr<AqlItemBlock> result(
      requestBlock(count, getNrOutputRegisters()));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnOld = (producesOutput && ep->_outVariableOld != nullptr);
  options.returnNew = (producesOutput && ep->_outVariableNew != nullptr);
  options.ignoreRevs = ep->getOptions().ignoreRevs;
  options.isRestore = ep->getOptions().useIsRestore;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

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

      errorMessage.clear();
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
        errorMessage = std::string("expecting 'Object', got: ") +
                       a.slice().typeName() + std::string(" while handling: ") +
                       _exeNode->getTypeString();
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), key)) {
          wasTaken.push_back(true);
          if (hasKeyVariable) {
            _tempBuilder.clear();
            _tempBuilder.openObject();
            _tempBuilder.add(StaticStrings::KeyString, VPackValue(key));
            _tempBuilder.close();

            VPackCollection::merge(object, a.slice(), _tempBuilder.slice(), false, false);
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
    OperationResult opRes = _trx->update(_collection->name(), toUpdate, options);
    if (!isMultiple) {
      int errorCode = opRes.errorNumber();

      if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
          ignoreDocumentNotFound) {
        // Ignore document not found on the DBserver:
        // do not emit a new row
        continue;
      }

      if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
        if (ep->_outVariableOld != nullptr) {
          // store $OLD
          result->emplaceValue(dstRow, _outRegOld, opRes.slice().get("old"));
        }
        if (ep->_outVariableNew != nullptr) {
          // store $NEW
          result->emplaceValue(dstRow, _outRegNew, opRes.slice().get("new"));
        }
      }

      if (errorCode != TRI_ERROR_NO_ERROR) {
        errorMessage.assign(opRes.errorMessage());
      } else {
        errorMessage.clear();
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
          TRI_ASSERT(i < wasTaken.size());
          if (wasTaken[i]) {
            TRI_ASSERT(iter.valid());
            auto elm = iter.value();
            bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(elm, "error", false);
            if (!wasError) {
              if (ep->_outVariableOld != nullptr) {
                // store $OLD
                result->emplaceValue(dstRow, _outRegOld, elm.get("old"));
              }
              if (ep->_outVariableNew != nullptr) {
                // store $NEW
                result->emplaceValue(dstRow, _outRegNew, elm.get("new"));
              }
            }
            ++iter;

            if (wasError) {
              // do not increase dstRow here
              continue;
            }
          }
          dstRow++;
        }
      } else {
        dstRow += n;
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  if (dstRow < result->size()) {
    if (dstRow == 0) {
      result.reset();
    } else {
      result->shrink(dstRow);
    }
  }

  return result;
}

UpsertBlock::UpsertBlock(ExecutionEngine* engine, UpsertNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for inserting data
std::unique_ptr<AqlItemBlock> UpsertBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = ExecutionNode::castTo<UpsertNode const*>(getPlanNode());

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

  result.reset(requestBlock(count, getNrOutputRegisters()));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnNew = producesOutput;
  options.ignoreRevs = ep->getOptions().ignoreRevs;
  options.isRestore = ep->getOptions().useIsRestore;

  VPackBuilder insertBuilder;
  VPackBuilder updateBuilder;

  // loop over all blocks
  size_t dstRow = 0;
  std::vector<size_t> insRows;
  std::vector<size_t> upRows;
  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

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

      errorMessage.clear();
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

              _tempBuilder.clear();
              _tempBuilder.openObject();
              _tempBuilder.add(StaticStrings::KeyString, VPackValue(key));
              _tempBuilder.close();
              if (isMultiple) {
                VPackBuilder tmp = VPackCollection::merge(toUpdate, _tempBuilder.slice(), false, false);
                updateBuilder.add(tmp.slice());
                upRows.emplace_back(dstRow);
              } else {
                updateBuilder = VPackCollection::merge(toUpdate, _tempBuilder.slice(), false, false);
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
          OperationResult opRes = _trx->insert(_collection->name(), toInsert, options);
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
                result->emplaceValue(insRows[i], _outRegNew, elm.get("new"));
              }
              ++i;
            }
          }
          handleBabyResult(opRes.countErrorCodes,
                           static_cast<size_t>(toInsert.length()),
                           ep->_options.ignoreErrors);
        }
      } else {
        OperationResult opRes = _trx->insert(_collection->name(), toInsert, options);
        errorCode = opRes.errorNumber();

        if (options.returnNew && errorCode == TRI_ERROR_NO_ERROR) {
          result->emplaceValue(dstRow - 1, _outRegNew, opRes.slice().get("new"));
        }
        if (errorCode != TRI_ERROR_NO_ERROR) {
          errorMessage.assign(opRes.errorMessage());
        } else {
          errorMessage.clear();
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
            opRes = _trx->replace(_collection->name(), toUpdate, options);
          } else {
            // update
            opRes = _trx->update(_collection->name(), toUpdate, options);
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
                result->emplaceValue(upRows[i], _outRegNew, elm.get("new"));
              }
              ++i;
            }
          }
        }
      } else {
        OperationResult opRes;
        if (ep->_isReplace) {
          // replace
          opRes = _trx->replace(_collection->name(), toUpdate, options);
        } else {
          // update
          opRes = _trx->update(_collection->name(), toUpdate, options);
        }
        errorCode = opRes.errorNumber();

        if (options.returnNew && errorCode == TRI_ERROR_NO_ERROR) {
          // store $NEW
          result->emplaceValue(dstRow - 1, _outRegNew, opRes.slice().get("new"));
        }
        if (errorCode != TRI_ERROR_NO_ERROR) {
          errorMessage.assign(opRes.errorMessage());
        } else {
          errorMessage.clear();
        }
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  return result;
}

ReplaceBlock::ReplaceBlock(ExecutionEngine* engine, ReplaceNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for replacing data
std::unique_ptr<AqlItemBlock> ReplaceBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  auto ep = ExecutionNode::castTo<ReplaceNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0;  // default initialization

  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool producesOutput = (ep->_outVariableOld != nullptr || ep->_outVariableNew != nullptr);

  if (!producesOutput && _isDBServer && ignoreDocumentNotFound) {
    // on a DB server, when we are told to ignore missing documents, we must
    // set this flag in order to not assert later on
    producesOutput = true;
  }

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  std::string errorMessage;
  VPackBuilder object;

  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  std::unique_ptr<AqlItemBlock> result(
      requestBlock(count, getNrOutputRegisters()));

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnOld = (producesOutput && ep->_outVariableOld != nullptr);
  options.returnNew = (producesOutput && ep->_outVariableNew != nullptr);
  options.ignoreRevs = ep->getOptions().ignoreRevs;
  options.isRestore = ep->getOptions().useIsRestore;

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

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

      errorMessage.clear();
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
        errorMessage = std::string("expecting 'Object', got: ") +
                       a.slice().typeName() + std::string(" while handling: ") +
                       _exeNode->getTypeString();
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), key)) {
          wasTaken.push_back(true);
          if (hasKeyVariable) {
            _tempBuilder.clear();
            _tempBuilder.openObject();
            _tempBuilder.add(StaticStrings::KeyString, VPackValue(key));
            _tempBuilder.close();
            VPackCollection::merge(object, a.slice(), _tempBuilder.slice(), false, false);
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
    OperationResult opRes = _trx->replace(_collection->name(), toUpdate, options);
    if (!isMultiple) {
      int errorCode = opRes.errorNumber();

      if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer &&
          ignoreDocumentNotFound) {
        // Ignore document not found on the DBserver:
        // do not emit a new row
        continue;
      }

      if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
        if (ep->_outVariableOld != nullptr) {
          // store $OLD
          result->emplaceValue(dstRow, _outRegOld, opRes.slice().get("old"));
        }
        if (ep->_outVariableNew != nullptr) {
          // store $NEW
          result->emplaceValue(dstRow, _outRegNew, opRes.slice().get("new"));
        }
      }

      if (errorCode != TRI_ERROR_NO_ERROR) {
        errorMessage.assign(opRes.errorMessage());
      } else {
        errorMessage.clear();
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
          TRI_ASSERT(i < wasTaken.size());
          if (wasTaken[i]) {
            TRI_ASSERT(iter.valid());
            auto elm = iter.value();
            bool wasError = arangodb::basics::VelocyPackHelper::getBooleanValue(elm, "error", false);
            if (!wasError) {
              if (ep->_outVariableOld != nullptr) {
                // store $OLD
                result->emplaceValue(dstRow, _outRegOld, elm.get("old"));
              }
              if (ep->_outVariableNew != nullptr) {
                // store $NEW
                result->emplaceValue(dstRow, _outRegNew, elm.get("new"));
              }
            }
            ++iter;

            if (wasError) {
              // do not increase dstRow here
              continue;
            }
          }
          dstRow++;
        }
      } else {
        dstRow += n;
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  if (dstRow < result->size()) {
    if (dstRow == 0) {
      result.reset();
    } else {
      result->shrink(dstRow);
    }
  }

  return result;
}
