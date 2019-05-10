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

using namespace arangodb;
using namespace arangodb::aql;

ModificationBlock::ModificationBlock(ExecutionEngine* engine, ModificationNode const* ep)
    : ExecutionBlock(engine, ep),
      _outRegOld(ExecutionNode::MaxRegisterId),
      _outRegNew(ExecutionNode::MaxRegisterId),
      _collection(ep->_collection),
      _countStats(ep->countStats()),
      _readCompleteInput(ep->_options.readCompleteInput) {
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
}

ModificationBlock::~ModificationBlock() {}

/// @brief skips over the taken rows if the input value is no
/// array or empty. updates dstRow in this case and returns true!
bool ModificationBlock::skipEmptyValues(VPackSlice const& values, size_t n,
                                        AqlItemBlock const* src,
                                        AqlItemBlock* dst, size_t& dstRow) {
  TRI_ASSERT(src != nullptr);
  TRI_ASSERT(_operations.size() == n);

  if (values.isArray() && values.length() > 0) {
    return false;
  }

  if (dst == nullptr) {
    // fast-track exit. we don't have any output to write, so we
    // better try not to copy any of the register values from src to dst
    return true;
  }

  for (size_t i = 0; i < n; ++i) {
    if (_operations[i] != IGNORE_SKIP) {
      inheritRegisters(src, dst, i, dstRow);
      ++dstRow;
    }
  }

  return true;
}

void ModificationBlock::trimResult(std::unique_ptr<AqlItemBlock>& result, size_t numRowsWritten) {
  if (result == nullptr) {
    return;
  }
  if (numRowsWritten == 0) {
    AqlItemBlock* block = result.release();
    returnBlock(block);
  } else if (numRowsWritten < result->size()) {
    result->shrink(numRowsWritten);
  }
}

/// @brief determine the number of rows in a vector of blocks
size_t ModificationBlock::countBlocksRows() const {
  size_t count = 0;
  for (auto const& it : _blocks) {
    count += it->size();
  }
  return count;
}

ExecutionState ModificationBlock::getHasMoreState() {
  if (_readCompleteInput) {
    // In these blocks everything from upstream
    // is entirely processed in one go.
    // So if upstream is done, we are done.
    return _upstreamState;
  }

  return ExecutionBlock::getHasMoreState();
}

/// @brief get some - this accumulates all input and calls the work() method
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ModificationBlock::getSome(size_t atMost) {
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

  if (_readCompleteInput) {
    // loop over input until it is exhausted
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

std::pair<ExecutionState, size_t> ModificationBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  size_t skipped = 0;

  // we need to tell getSome that we don't have to read the entire input,
  // no matter what
  bool saved = _readCompleteInput;
  _readCompleteInput = false;
  auto guard = scopeGuard([this, saved]() { _readCompleteInput = saved; });

  auto res = getSome(atMost);

  if (res.first == ExecutionState::WAITING) {
    TRI_ASSERT(skipped == 0);
    traceSkipSomeEnd(skipped, ExecutionState::WAITING);
    return {ExecutionState::WAITING, skipped};
  }

  if (res.second) {
    skipped += res.second->size();
  }

  traceSkipSomeEnd(skipped, res.first);
  return {res.first, skipped};
}

std::pair<ExecutionState, arangodb::Result> ModificationBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  _blocks.clear();
  return ExecutionBlock::initializeCursor(items, pos);
}

/// @brief extract a key from the AqlValue passed
int ModificationBlock::extractKey(AqlValue const& value, std::string& key) {
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

int ModificationBlock::extractKeyAndRev(AqlValue const& value, std::string& key,
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
void ModificationBlock::handleBabyResult(OperationResult const& opRes,
                                         size_t numBabies, bool ignoreAllErrors,
                                         bool ignoreDocumentNotFound) {
  std::unordered_map<int, size_t> const& errorCounter = opRes.countErrorCodes;
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

  auto code = first->first;

  // check if we can find a specific error message
  std::string message;
  try {
    if (opRes.slice().isArray()) {
      for (auto doc : VPackArrayIterator(opRes.slice())) {
        if (!doc.isObject() || !doc.hasKey("errorNum") ||
            doc.get("errorNum").getInt() != code) {
           continue;
        }
        VPackSlice msg = doc.get("errorMessage");
        if (msg.isString()) {
          message = msg.copyString();
          break;
        }
      }
    }
  } catch (...) {
    // fall-through to returning the generic error message, which better than
    // forwarding an internal error here
  }

  if (!message.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(code, std::move(message));
  }

  // no specific error message found. now respond with a generic error message
  THROW_ARANGO_EXCEPTION(code);
}

RemoveBlock::RemoveBlock(ExecutionEngine* engine, RemoveNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for removing data
std::unique_ptr<AqlItemBlock> RemoveBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  auto ep = ExecutionNode::castTo<RemoveNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  bool const ignoreDocumentNotFound = ep->_options.ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr);

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.ignoreRevs = ep->_options.ignoreRevs;
  options.returnOld = producesOutput;
  options.isRestore = ep->_options.useIsRestore;

  std::unique_ptr<AqlItemBlock> result;
  if (producesOutput || ep->producesResults()) {
    result.reset(requestBlock(count, getNrOutputRegisters()));
  }

  // loop over all blocks
  size_t dstRow = 0;

  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

    throwIfKilled();  // check if we were aborted

    std::string key;
    std::string rev;

    _tempBuilder.clear();
    _tempBuilder.openArray();

    // loop over the complete block
    size_t const n = res->size();

    _operations.clear();
    _operations.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      AqlValue const& a = res->getValueReference(i, registerId);

      int errorCode = TRI_ERROR_NO_ERROR;

      if (!ep->_options.consultAqlWriteFilter ||
          !_collection->getCollection()->skipForAqlWrite(a.slice(), StaticStrings::Empty)) {
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
          _operations.push_back(APPLY_RETURN);
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
          _operations.push_back(IGNORE_SKIP);
          handleResult(errorCode, ep->_options.ignoreErrors, nullptr);
        }
      } else {
        // not relevant for ourselves... just pass it on to the next block
        _operations.push_back(IGNORE_RETURN);
      }
    }

    _tempBuilder.close();

    VPackSlice toRemove = _tempBuilder.slice();

    if (skipEmptyValues(toRemove, n, res, result.get(), dstRow)) {
      it->release();
      returnBlock(res);
      continue;
    }

    OperationResult opRes = _trx->remove(_collection->name(), toRemove, options);

    handleBabyResult(opRes, static_cast<size_t>(toRemove.length()),
                     ep->_options.ignoreErrors, ignoreDocumentNotFound);

    if (opRes.fail()) {
      THROW_ARANGO_EXCEPTION(opRes.result);
    }

    VPackSlice resultList = opRes.slice();
    
    if (ep->_options.ignoreErrors && options.silent) {
      resultList = VPackSlice::emptyArraySlice();
    }

    if (skipEmptyValues(resultList, n, res, result.get(), dstRow)) {
      it->release();
      returnBlock(res);
      continue;
    }

    TRI_ASSERT(resultList.isArray());
    auto iter = VPackArrayIterator(resultList);
    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(i < _operations.size());
      if (_operations[i] == APPLY_RETURN && result != nullptr) {
        TRI_ASSERT(iter.valid());
        auto elm = iter.value();
        bool wasError =
            arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

        if (!wasError) {
          inheritRegisters(res, result.get(), i, dstRow);

          if (ep->_outVariableOld != nullptr) {
            // store $OLD
            result->emplaceValue(dstRow, _outRegOld, elm.get("old"));
          }
          ++dstRow;
        }
        ++iter;
      } else if (_operations[i] == IGNORE_RETURN) {
        TRI_ASSERT(result != nullptr);
        inheritRegisters(res, result.get(), i, dstRow);
        ++dstRow;
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  trimResult(result, dstRow);
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

  auto ep = ExecutionNode::castTo<InsertNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  bool const producesNew = (ep->_outVariableNew != nullptr);
  bool const producesOld = (ep->_outVariableOld != nullptr);
  bool const producesOutput = producesNew || producesOld;

  OperationOptions options;
  // use "silent" mode if we do not access the results later on
  options.silent = !producesOutput;
  options.returnNew = producesNew;
  options.returnOld = producesOld;
  options.isRestore = ep->_options.useIsRestore;
  options.waitForSync = ep->_options.waitForSync;
  options.overwrite = ep->_options.overwrite;

  std::unique_ptr<AqlItemBlock> result;
  if (producesOutput || ep->producesResults()) {
    result.reset(requestBlock(count, getNrOutputRegisters()));
  }

  // loop over all blocks
  size_t dstRow = 0;

  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

    throwIfKilled();  // check if we were aborted

    _tempBuilder.clear();
    _tempBuilder.openArray();

    size_t const n = res->size();

    _operations.clear();
    _operations.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      AqlValue const& a = res->getValueReference(i, registerId);

      if (!ep->_options.consultAqlWriteFilter ||
          !_collection->getCollection()->skipForAqlWrite(a.slice(), StaticStrings::Empty)) {
        _operations.push_back(APPLY_RETURN);
        // TODO This may be optimized with externals
        _tempBuilder.add(a.slice());
      } else {
        // not relevant for ourselves... just pass it on to the next block
        _operations.push_back(IGNORE_RETURN);
      }
    }

    _tempBuilder.close();

    VPackSlice toInsert = _tempBuilder.slice();

    if (skipEmptyValues(toInsert, n, res, result.get(), dstRow)) {
      it->release();
      returnBlock(res);
      continue;
    }

    OperationResult opRes = _trx->insert(_collection->name(), toInsert, options);

    handleBabyResult(opRes, static_cast<size_t>(toInsert.length()), ep->_options.ignoreErrors);

    if (opRes.fail()) {
      THROW_ARANGO_EXCEPTION(opRes.result);
    }

    VPackSlice resultList = opRes.slice();
    
    if (ep->_options.ignoreErrors && options.silent) {
      resultList = VPackSlice::emptyArraySlice();
    }

    if (skipEmptyValues(resultList, n, res, result.get(), dstRow)) {
      it->release();
      returnBlock(res);
      continue;
    }

    TRI_ASSERT(resultList.isArray());
    auto iter = VPackArrayIterator(resultList);
    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(i < _operations.size());
      if (_operations[i] == APPLY_RETURN && result != nullptr) {
        TRI_ASSERT(iter.valid());
        auto elm = iter.value();
        bool wasError =
            arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

        if (!wasError) {
          inheritRegisters(res, result.get(), i, dstRow);

          if (producesNew) {
            // store $NEW
            result->emplaceValue(dstRow, _outRegNew, elm.get("new"));
          }
          if (producesOld) {
            // store $OLD
            auto slice = elm.get("old");
            if (slice.isNone()) {
              result->emplaceValue(dstRow, _outRegOld, VPackSlice::nullSlice());
            } else {
              result->emplaceValue(dstRow, _outRegOld, slice);
            }
          }
          ++dstRow;
        }
        ++iter;
      } else if (_operations[i] == IGNORE_RETURN) {
        TRI_ASSERT(result != nullptr);
        inheritRegisters(res, result.get(), i, dstRow);
        ++dstRow;
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  trimResult(result, dstRow);
  return result;
}

UpdateReplaceBlock::UpdateReplaceBlock(ExecutionEngine* engine, ModificationNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief worker function for update/replace
std::unique_ptr<AqlItemBlock> UpdateReplaceBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

  auto ep = ExecutionNode::castTo<UpdateReplaceNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0;  // default initialization

  bool const ignoreDocumentNotFound = ep->_options.ignoreDocumentNotFound;
  bool producesOutput = (ep->_outVariableOld != nullptr || ep->_outVariableNew != nullptr);

  // check if we're a DB server in a cluster
  bool const isDBServer = ServerState::instance()->isDBServer();

  if (!producesOutput && isDBServer && ignoreDocumentNotFound) {
    // on a DB server, when we are told to ignore missing documents, we must
    // set this flag in order to not assert later on
    producesOutput = true;
  }

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  VPackBuilder object;

  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnOld = (producesOutput && ep->_outVariableOld != nullptr);
  options.returnNew = (producesOutput && ep->_outVariableNew != nullptr);
  options.ignoreRevs = ep->_options.ignoreRevs;
  options.isRestore = ep->_options.useIsRestore;

  std::unique_ptr<AqlItemBlock> result;
  if (producesOutput || ep->producesResults()) {
    result.reset(requestBlock(count, getNrOutputRegisters()));
  }

  std::string errorMessage;

  // loop over all blocks
  size_t dstRow = 0;

  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

    throwIfKilled();  // check if we were aborted

    object.clear();
    object.openArray();

    std::string key;
    std::string rev;

    size_t const n = res->size();

    // loop over the complete block
    _operations.clear();
    _operations.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      AqlValue const& a = res->getValueReference(i, docRegisterId);

      errorMessage.clear();
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an object
        key.clear();
        if (hasKeyVariable) {
          // separate key specification
          AqlValue const& k = res->getValueReference(i, keyRegisterId);
          if (options.ignoreRevs) {
            errorCode = extractKey(k, key);
          } else {
            rev.clear();
            errorCode = extractKeyAndRev(k, key, rev);
          }
        } else {
          errorCode = extractKey(a, key);
        }
      } else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        errorMessage = std::string("expecting 'Object', got: ") + a.slice().typeName() +
                       std::string(" while handling: ") + _exeNode->getTypeString();
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), key)) {
          _operations.push_back(APPLY_RETURN);

          if (hasKeyVariable) {
            _tempBuilder.clear();
            _tempBuilder.openObject();
            _tempBuilder.add(StaticStrings::KeyString, VPackValue(key));
            if (!options.ignoreRevs && !rev.empty()) {
              _tempBuilder.add(StaticStrings::RevString, VPackValue(rev));
            } else {
              // we must never take _rev from the document if there is a key
              // expression.
              _tempBuilder.add(StaticStrings::RevString, VPackValue(VPackValueType::Null));
            }
            _tempBuilder.close();

            VPackCollection::merge(object, a.slice(), _tempBuilder.slice(), false, true);
          } else {
            // use original slice for updating
            object.add(a.slice());
          }
        } else {
          // not relevant for ourselves... just pass it on to the next block
          _operations.push_back(IGNORE_RETURN);
        }
      } else {
        _operations.push_back(IGNORE_SKIP);
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      }
    }

    object.close();

    VPackSlice toUpdate = object.slice();

    if (skipEmptyValues(toUpdate, n, res, result.get(), dstRow)) {
      it->release();
      returnBlock(res);
      continue;
    }

    // perform update/replace
    OperationResult opRes = apply(toUpdate, options);

    handleBabyResult(opRes, static_cast<size_t>(toUpdate.length()),
                     ep->_options.ignoreErrors, ignoreDocumentNotFound);

    if (opRes.fail()) {
      THROW_ARANGO_EXCEPTION(opRes.result);
    }

    VPackSlice resultList = opRes.slice();
    
    if (ep->_options.ignoreErrors && options.silent) {
      resultList = VPackSlice::emptyArraySlice();
    }

    if (skipEmptyValues(resultList, n, res, result.get(), dstRow)) {
      it->release();
      returnBlock(res);
      continue;
    }

    TRI_ASSERT(resultList.isArray());
    auto iter = VPackArrayIterator(resultList);
    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(i < _operations.size());
      if (_operations[i] == APPLY_RETURN && result != nullptr) {
        TRI_ASSERT(iter.valid());
        auto elm = iter.value();
        bool wasError =
            arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

        if (!wasError) {
          inheritRegisters(res, result.get(), i, dstRow);

          if (ep->_outVariableOld != nullptr) {
            // store $OLD
            result->emplaceValue(dstRow, _outRegOld, elm.get("old"));
          }
          if (ep->_outVariableNew != nullptr) {
            // store $NEW
            result->emplaceValue(dstRow, _outRegNew, elm.get("new"));
          }
          ++dstRow;
        }
        ++iter;
      } else if (_operations[i] == IGNORE_SKIP) {
        TRI_ASSERT(result != nullptr);
        inheritRegisters(res, result.get(), i, dstRow);
        ++dstRow;
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  trimResult(result, dstRow);
  return result;
}

UpdateBlock::UpdateBlock(ExecutionEngine* engine, UpdateNode const* ep)
    : UpdateReplaceBlock(engine, ep) {}

OperationResult UpdateBlock::apply(arangodb::velocypack::Slice values,
                                   OperationOptions const& options) {
  return _trx->update(_collection->name(), values, options);
}

ReplaceBlock::ReplaceBlock(ExecutionEngine* engine, ReplaceNode const* ep)
    : UpdateReplaceBlock(engine, ep) {}

OperationResult ReplaceBlock::apply(arangodb::velocypack::Slice values,
                                    OperationOptions const& options) {
  return _trx->replace(_collection->name(), values, options);
}

UpsertBlock::UpsertBlock(ExecutionEngine* engine, UpsertNode const* ep)
    : ModificationBlock(engine, ep) {}

/// @brief the actual work horse for upsert
std::unique_ptr<AqlItemBlock> UpsertBlock::work() {
  size_t const count = countBlocksRows();

  if (count == 0) {
    return nullptr;
  }

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

  OperationOptions options;
  options.silent = !producesOutput;
  options.waitForSync = ep->_options.waitForSync;
  options.mergeObjects = ep->_options.mergeObjects;
  options.keepNull = !ep->_options.nullMeansRemove;
  options.returnNew = producesOutput;
  options.ignoreRevs = ep->_options.ignoreRevs;
  options.isRestore = ep->_options.useIsRestore;

  std::unique_ptr<AqlItemBlock> result;
  if (producesOutput || ep->producesResults()) {
    result.reset(requestBlock(count, getNrOutputRegisters()));
  }

  std::string errorMessage;

  VPackBuilder insertBuilder;
  VPackBuilder updateBuilder;

  // loop over all blocks
  size_t dstRow = 0;

  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    auto* res = it->get();

    throwIfKilled();  // check if we were aborted

    insertBuilder.clear();
    updateBuilder.clear();

    // Prepare both builders
    insertBuilder.openArray();
    updateBuilder.openArray();

    std::string key;

    // loop over the complete block
    size_t const n = res->size();

    _operations.clear();
    _operations.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      AqlValue const& a = res->getValueReference(i, docRegisterId);

      errorMessage.clear();
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        if (!ep->_options.consultAqlWriteFilter ||
            !_collection->getCollection()->skipForAqlWrite(a.slice(), StaticStrings::Empty)) {
          // old document present => update case
          key.clear();
          errorCode = extractKey(a, key);

          if (errorCode == TRI_ERROR_NO_ERROR) {
            AqlValue const& updateDoc = res->getValueReference(i, updateRegisterId);

            if (updateDoc.isObject()) {
              VPackSlice toUpdate = updateDoc.slice();

              _tempBuilder.clear();
              _tempBuilder.openObject();
              _tempBuilder.add(StaticStrings::KeyString, VPackValue(key));
              _tempBuilder.close();

              VPackBuilder tmp =
                  VPackCollection::merge(toUpdate, _tempBuilder.slice(), false, false);
              updateBuilder.add(tmp.slice());
              _operations.push_back(APPLY_UPDATE);
            } else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
              errorMessage = std::string("expecting 'Object', got: ") +
                             updateDoc.slice().typeName() +
                             std::string(" while handling: ") +
                             _exeNode->getTypeString();
            }
          }
        } else {
          // not relevant for ourselves... just pass it on to the next block
          _operations.push_back(IGNORE_RETURN);
        }
      } else {
        // no document found => insert case
        AqlValue const& insertDoc = res->getValueReference(i, insertRegisterId);
        VPackSlice toInsert = insertDoc.slice();
        if (toInsert.isObject()) {
          if (!ep->_options.consultAqlWriteFilter ||
              !_collection->getCollection()->skipForAqlWrite(toInsert, StaticStrings::Empty)) {
            insertBuilder.add(toInsert);
            _operations.push_back(APPLY_INSERT);
          } else {
            // not relevant for ourselves... just pass it on to the next block
            _operations.push_back(IGNORE_RETURN);
          }
        } else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
          errorMessage =
              std::string("expecting 'Object', got: ") + toInsert.typeName() +
              std::string(" while handling: ") + _exeNode->getTypeString();
        }
      }

      if (errorCode != TRI_ERROR_NO_ERROR) {
        _operations.push_back(IGNORE_SKIP);
        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      }
    }
    // done with collecting a block

    insertBuilder.close();
    updateBuilder.close();

    VPackSlice toInsert = insertBuilder.slice();
    VPackSlice toUpdate = updateBuilder.slice();

    if (toInsert.length() == 0 && toUpdate.length() == 0) {
      // nothing to do
      if (skipEmptyValues(toInsert, n, res, result.get(), dstRow)) {
        it->release();
        returnBlock(res);
        continue;
      }
    }

    OperationResult opResInsert;
    VPackSlice resultListInsert = VPackSlice::emptyArraySlice();

    if (toInsert.isArray() && toInsert.length() > 0) {
      opResInsert = _trx->insert(_collection->name(), toInsert, options);

      if (opResInsert.fail()) {
        THROW_ARANGO_EXCEPTION(opResInsert.result);
      }

      handleBabyResult(opResInsert, static_cast<size_t>(toInsert.length()),
                       ep->_options.ignoreErrors);

      resultListInsert = opResInsert.slice();
      if (!resultListInsert.isArray()) {
        resultListInsert = VPackSlice::emptyArraySlice();
      } else if (ep->_options.ignoreErrors && options.silent) {
        resultListInsert = VPackSlice::emptyArraySlice();
      }
    }

    OperationResult opResUpdate;
    VPackSlice resultListUpdate = VPackSlice::emptyArraySlice();

    if (toUpdate.isArray() && toUpdate.length() > 0) {
      if (ep->_isReplace) {
        opResUpdate = _trx->replace(_collection->name(), toUpdate, options);
      } else {
        opResUpdate = _trx->update(_collection->name(), toUpdate, options);
      }

      if (opResUpdate.fail()) {
        THROW_ARANGO_EXCEPTION(opResUpdate.result);
      }

      handleBabyResult(opResUpdate, static_cast<size_t>(toUpdate.length()),
                       ep->_options.ignoreErrors);

      resultListUpdate = opResUpdate.slice();
      if (!resultListUpdate.isArray()) {
        resultListUpdate = VPackSlice::emptyArraySlice();
      } else if (ep->_options.ignoreErrors && options.silent) {
        resultListUpdate = VPackSlice::emptyArraySlice();
      }
    }

    TRI_ASSERT(resultListInsert.isArray());
    TRI_ASSERT(resultListUpdate.isArray());

    if (resultListInsert.length() == 0 && resultListUpdate.length() == 0) {
      // nothing to do
      if (skipEmptyValues(resultListInsert, n, res, result.get(), dstRow)) {
        it->release();
        returnBlock(res);
        continue;
      }
    }

    auto iterInsert = VPackArrayIterator(resultListInsert);
    auto iterUpdate = VPackArrayIterator(resultListUpdate);

    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(i < _operations.size());
      if ((_operations[i] == APPLY_UPDATE || _operations[i] == APPLY_INSERT) &&
          result != nullptr) {
        // fetch operation type (insert or update/replace)
        VPackArrayIterator* iter;
        if (_operations[i] == APPLY_INSERT) {
          iter = &iterInsert;
        } else {
          iter = &iterUpdate;
        }

        TRI_ASSERT(iter->valid());
        auto elm = iter->value();
        bool wasError =
            arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

        if (!wasError) {
          TRI_ASSERT(result != nullptr);
          inheritRegisters(res, result.get(), i, dstRow);

          if (ep->_outVariableNew != nullptr) {
            // return $NEW
            result->emplaceValue(dstRow, _outRegNew, elm.get("new"));
          }
          ++dstRow;
        }
      } else if (_operations[i] == IGNORE_SKIP) {
        TRI_ASSERT(result != nullptr);
        inheritRegisters(res, result.get(), i, dstRow);
        ++dstRow;
      }
    }

    // done with block. now unlink it and return it to block manager
    it->release();
    returnBlock(res);
  }

  trimResult(result, dstRow);

  return result;
}
