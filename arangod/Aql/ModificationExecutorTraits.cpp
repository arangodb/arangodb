////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ModificationExecutorTraits.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "Basics/StaticStrings.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <algorithm>

#include "AqlItemBlockUtils.h"
#include "velocypack/Collection.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

int extractKeyAndRev(transaction::Methods* trx, AqlValue const& value,
                     std::string& key, std::string& rev, bool keyOnly = false) {
  if (value.isObject()) {
    bool mustDestroy;
    auto resolver = trx->resolver();
    TRI_ASSERT(resolver != nullptr);
    AqlValue sub = value.get(*resolver, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());

      if (!keyOnly) {
        bool mustDestroyToo;
        AqlValue subTwo =
            value.get(*resolver, StaticStrings::RevString, mustDestroyToo, false);
        AqlValueGuard guard(subTwo, mustDestroyToo);
        if (subTwo.isString()) {
          rev.assign(subTwo.slice().copyString());
        }
      }

      return TRI_ERROR_NO_ERROR;
    }
  } else if (value.isString()) {
    key.assign(value.slice().copyString());
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
}

int extractKey(transaction::Methods* trx, AqlValue const& value, std::string& key) {
  std::string optimizeAway;
  return extractKeyAndRev(trx, value, key, optimizeAway, true /*key only*/);
}

/// @brief process the result of a data-modification operation
void handleStats(ModificationStats& stats, ModificationExecutorInfos& info, int code,
                 bool ignoreErrors, std::string const* errorMessage = nullptr) {
  if (code == TRI_ERROR_NO_ERROR) {
    // update the success counter
    if (info._doCount) {
      stats.incrWritesExecuted();
    }
    return;
  }

  if (ignoreErrors) {
    // update the ignored counter
    if (info._doCount) {
      stats.incrWritesIgnored();
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
void handleBabyStats(ModificationStats& stats, ModificationExecutorInfos& info,
                     OperationResult const& opRes, uint64_t numBabies,
                     bool ignoreErrors, bool ignoreDocumentNotFound = false) {

  auto const& errorCounter = opRes.countErrorCodes;

  size_t numberBabies = numBabies;  // from uint64_t to size_t
  if (errorCounter.empty()) {
    // update the success counter
    if (info._doCount) {
      stats.addWritesExecuted(numberBabies);
    }
    return;
  }

  if (ignoreErrors) {
    for (auto const& pair : errorCounter) {
      // update the ignored counter
      if (info._doCount) {
        stats.addWritesIgnored(pair.second);
      }
      numberBabies -= pair.second;
    }

    // update the success counter
    if (info._doCount) {
      stats.addWritesExecuted(numberBabies);
    }
    return;
  }

  auto first = errorCounter.begin();

  if (ignoreDocumentNotFound && first->first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    if (errorCounter.size() == 1) {
      // We only have Document not found. Fix statistics and ignore
      // update the ignored counter
      if (info._doCount) {
        stats.addWritesIgnored(first->second);
      }
      numberBabies -= first->second;
      // update the success counter
      if (info._doCount) {
        stats.addWritesExecuted(numberBabies);
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
  std::string message;

  try {
    if (opRes.slice().isArray()) {
      for (auto doc : VPackArrayIterator(opRes.slice())) {
        if (doc.isObject() && doc.hasKey(StaticStrings::ErrorNum) &&
            doc.get(StaticStrings::ErrorNum).getInt() == code) {
          VPackSlice s = doc.get(StaticStrings::ErrorMessage);
          if (s.isString()) {
            message = s.copyString();
            break;
          }
        }
      }
    }
  } catch (...) {
    // Fall-through to returning the generic error message,
    // which better than forwarding an internal error here.
  }

  if (!message.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(code, message);
  }
  // Throw generic error, as no error message was found.
  THROW_ARANGO_EXCEPTION(code);
}
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// INSERT /////////////////////////////////////////////////////////////////////
bool Insert::doModifications(ModificationExecutorInfos& info, ModificationStats& stats) {
  reset();
  _tmpBuilder.openArray();

  RegisterId const inReg = info._input1RegisterId;
  TRI_ASSERT(_block != nullptr);
  itemBlock::forRowInBlock(_block, [this, inReg, &info](InputAqlItemRow&& row) {
    auto const& inVal = row.getValue(inReg);
    if (!info._consultAqlWriteFilter ||
        !info._aqlCollection->getCollection()->skipForAqlWrite(inVal.slice(),
                                                               StaticStrings::Empty)) {
      _operations.push_back(ModOperationType::APPLY_RETURN);
      // TODO This may be optimized with externals
      _tmpBuilder.add(inVal.slice());
    } else {
      // not relevant for ourselves... just pass it on to the next block
      _operations.push_back(ModOperationType::IGNORE_RETURN);
    }
  });

  TRI_ASSERT(_operations.size() == _block->size());

  _tmpBuilder.close();
  auto toInsert = _tmpBuilder.slice();

  // At this point _tempbuilder contains the objects to insert
  // and _operations the information if the data is to be kept or not

  if (toInsert.length() == 0) {
    _justCopy = true;
    return !_operations.empty();
  }

  // execute insert
  TRI_ASSERT(info._trx);
  auto operationResult =
      info._trx->insert(info._aqlCollection->name(), toInsert, info._options);
  setOperationResult(std::move(operationResult));

  // handle statisitcs
  handleBabyStats(stats, info, _operationResult, toInsert.length(), info._ignoreErrors);

  _tmpBuilder.clear();

  if (_operationResult.fail()) {
    THROW_ARANGO_EXCEPTION(_operationResult.result);
  }

  if (!info._options.silent) {
    TRI_ASSERT(_operationResult.buffer != nullptr);
    TRI_ASSERT(_operationResult.slice().isArray());

    if (_operationResultArraySlice.length() == 0) {
      // there is nothing to update we just need to copy
      // if there is anything other than IGNORE_SKIP the
      // block is prepared.
      _justCopy = true;
      TRI_ASSERT(false);
      return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
    }
  }
  return true;
}

bool Insert::doOutput(ModificationExecutorInfos& info, OutputAqlItemRow& output) {
  TRI_ASSERT(_block != nullptr);

  std::size_t blockSize = _block->size();
  TRI_ASSERT(_blockIndex < blockSize);

  // ignore-skip values
  while (_blockIndex < _operations.size() &&
         _operations[_blockIndex] == ModOperationType::IGNORE_SKIP) {
    _blockIndex++;
  }

  InputAqlItemRow input = InputAqlItemRow(_block, _blockIndex);

  if (_justCopy || info._options.silent) {
    output.copyRow(input);
  } else {
    if (_operations[_blockIndex] == ModOperationType::APPLY_RETURN) {
      TRI_ASSERT(_operationResultIterator.valid());
      auto elm = _operationResultIterator.value();

      bool wasError =
          arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

      if (!wasError) {
        if (info._options.returnNew) {
          AqlValue value(elm.get("new"));
          AqlValueGuard guard(value, true);
          // store $NEW
          output.moveValueInto(info._outputNewRegisterId, input, guard);
        }
        if (info._options.returnOld) {
          // store $OLD
          auto slice = elm.get("old");
          if (slice.isNone()) {
            AqlValue value(VPackSlice::nullSlice());
            AqlValueGuard guard(value, true);
            output.moveValueInto(info._outputOldRegisterId, input, guard);
          } else {
            AqlValue value(slice);
            AqlValueGuard guard(value, true);
            output.moveValueInto(info._outputOldRegisterId, input, guard);
          }
        }
      }  // !wasError - end
      ++_operationResultIterator;
    } else if (_operations[_blockIndex] == ModOperationType::IGNORE_RETURN) {
      output.copyRow(input);
    } else {
      TRI_ASSERT(false);
    }
  }

  return ++_blockIndex < blockSize;
}

///////////////////////////////////////////////////////////////////////////////
// REMOVE /////////////////////////////////////////////////////////////////////
bool Remove::doModifications(ModificationExecutorInfos& info, ModificationStats& stats) {
  reset();
  _tmpBuilder.openArray();

  auto* trx = info._trx;
  int errorCode = TRI_ERROR_NO_ERROR;
  std::string key;
  std::string rev;

  RegisterId const inReg = info._input1RegisterId;
  TRI_ASSERT(_block != nullptr);
  itemBlock::forRowInBlock(_block, [this, &stats, &errorCode, &key, &rev, trx,
                                    inReg, &info](InputAqlItemRow&& row) {
    auto const& inVal = row.getValue(inReg);

    if (!info._consultAqlWriteFilter ||
        !info._aqlCollection->getCollection()->skipForAqlWrite(inVal.slice(),
                                                               StaticStrings::Empty)) {
      key.clear();
      rev.clear();

      if (inVal.isObject()) {
        errorCode = extractKeyAndRev(trx, inVal, key, rev, info._options.ignoreRevs /*key only*/);
      } else if (inVal.isString()) {
        // value is a string
        key = inVal.slice().copyString();
      } else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (errorCode == TRI_ERROR_NO_ERROR && fetchMore()) {
        _operations.push_back(ModOperationType::APPLY_RETURN);

        // no error. we expect to have a key
        // create a slice for the key
        _tmpBuilder.openObject();
        _tmpBuilder.add(StaticStrings::KeyString, VPackValue(key));
        if (!info._options.ignoreRevs && !rev.empty()) {
          _tmpBuilder.add(StaticStrings::RevString, VPackValue(rev));
        }
        _tmpBuilder.close();
        this->_last_not_skip = _operations.size();
      } else {
        // We have an error, handle it
        _operations.push_back(ModOperationType::IGNORE_SKIP);
        handleStats(stats, info, errorCode, info._ignoreErrors);
      }

    } else {
      // not relevant for ourselves... just pass it on to the next block
      _operations.push_back(ModOperationType::IGNORE_RETURN);
      this->_last_not_skip = _operations.size();
    }

  });

  TRI_ASSERT(_operations.size() == _block->size());

  _tmpBuilder.close();
  auto toRemove = _tmpBuilder.slice();

  if (toRemove.length() == 0) {
    _justCopy = true;
    return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
  }

  TRI_ASSERT(info._trx);
  auto operationResult =
      info._trx->remove(info._aqlCollection->name(), toRemove, info._options);
  setOperationResult(std::move(operationResult));

  handleBabyStats(stats, info, _operationResult, toRemove.length(),
                  info._ignoreErrors, info._ignoreDocumentNotFound);

  _tmpBuilder.clear();

  if (_operationResult.fail()) {
    THROW_ARANGO_EXCEPTION(_operationResult.result);
  }

  if (!info._options.silent) {
    TRI_ASSERT(_operationResult.buffer != nullptr);
    TRI_ASSERT(_operationResult.slice().isArray());

    if (_operationResultArraySlice.length() == 0) {
      _justCopy = true;
      TRI_ASSERT(false);
      return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
    }
  }
  return true;
}

bool Remove::doOutput(ModificationExecutorInfos& info, OutputAqlItemRow& output) {
  TRI_ASSERT(_block != nullptr);

  std::size_t blockSize = _block->size();
  TRI_ASSERT(_last_not_skip <= blockSize);
  TRI_ASSERT(_blockIndex < blockSize);

  while (_blockIndex < _operations.size() &&
         _operations[_blockIndex] == ModOperationType::IGNORE_SKIP) {
    _blockIndex++;
  }

  InputAqlItemRow input = InputAqlItemRow(_block, _blockIndex);
  if (_justCopy || info._options.silent) {
    output.copyRow(input);
  } else {
    if (_operations[_blockIndex] == ModOperationType::APPLY_RETURN) {
      TRI_ASSERT(_operationResultIterator.valid());
      auto elm = _operationResultIterator.value();

      bool wasError =
          arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

      if (!wasError) {
        if (info._options.returnOld) {
          // store $OLD
          auto slice = elm.get("old");

          AqlValue value(slice);
          AqlValueGuard guard(value, true);
          output.moveValueInto(info._outputOldRegisterId, input, guard);
        }
      }
      ++_operationResultIterator;
    } else if (_operations[_blockIndex] == ModOperationType::IGNORE_RETURN) {
      output.copyRow(input);
    } else {
      TRI_ASSERT(false);
    }
  }

  return ++_blockIndex < _last_not_skip;
}

///////////////////////////////////////////////////////////////////////////////
// UPSERT /////////////////////////////////////////////////////////////////////
bool Upsert::doModifications(ModificationExecutorInfos& info, ModificationStats& stats) {
  reset();

  _insertBuilder.openArray();
  _updateBuilder.openArray();

  int errorCode = TRI_ERROR_NO_ERROR;
  std::string errorMessage;
  std::string key;
  auto* trx = info._trx;
  RegisterId const inDocReg = info._input1RegisterId;
  RegisterId const insertReg = info._input2RegisterId;
  RegisterId const updateReg = info._input3RegisterId;

  itemBlock::forRowInBlock(_block, [this, &stats, &errorCode, &errorMessage,
                                    &key, trx, inDocReg, insertReg, updateReg,
                                    &info](InputAqlItemRow&& row) {
    errorMessage.clear();
    errorCode = TRI_ERROR_NO_ERROR;
    auto const& inVal = row.getValue(inDocReg);

    if (inVal.isObject()) /*update case, as old doc is present*/ {
      if (!info._consultAqlWriteFilter ||
          !info._aqlCollection->getCollection()->skipForAqlWrite(inVal.slice(),
                                                                 StaticStrings::Empty)) {
        key.clear();
        errorCode = extractKey(trx, inVal, key);
        if (errorCode == TRI_ERROR_NO_ERROR) {
          auto const& updateDoc = row.getValue(updateReg);
          if (updateDoc.isObject()) {
            VPackSlice toUpdate = updateDoc.slice();

            _tmpBuilder.clear();
            _tmpBuilder.openObject();
            _tmpBuilder.add(StaticStrings::KeyString, VPackValue(key));
            _tmpBuilder.close();

            VPackBuilder tmp =
                VPackCollection::merge(toUpdate, _tmpBuilder.slice(), false, false);
            _updateBuilder.add(tmp.slice());
            _operations.push_back(ModOperationType::APPLY_UPDATE);
            this->_last_not_skip = _operations.size();
          } else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
            errorMessage = std::string("expecting 'Object', got: ") +
                           updateDoc.slice().typeName() +
                           std::string(" while handling: UPSERT");
          }
        }
      } else /*Doc is not relevant ourselves. Just pass Row to the next block*/ {
        _operations.push_back(ModOperationType::IGNORE_RETURN);
        this->_last_not_skip = _operations.size();
      }
    } else /*insert case*/ {
      auto const& toInsert = row.getValue(insertReg).slice();
      if (toInsert.isObject()) {
        if (!info._consultAqlWriteFilter ||
            !info._aqlCollection->getCollection()->skipForAqlWrite(toInsert, StaticStrings::Empty)) {
          _insertBuilder.add(toInsert);
          _operations.push_back(ModOperationType::APPLY_INSERT);
        } else {
          // not relevant for ourselves... just pass it on to the next block
          _operations.push_back(ModOperationType::IGNORE_RETURN);
        }
        this->_last_not_skip = _operations.size();
      } else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        errorMessage = std::string("expecting 'Object', got: ") + toInsert.typeName() +
                       std::string(" while handling: UPSERT");
      }
    }

    if (errorCode != TRI_ERROR_NO_ERROR) {
      _operations.push_back(ModOperationType::IGNORE_SKIP);
      handleStats(stats, info, errorCode, info._ignoreErrors, &errorMessage);
    }
  });

  TRI_ASSERT(_operations.size() == _block->size());

  _insertBuilder.close();
  _updateBuilder.close();

  auto toInsert = _insertBuilder.slice();
  auto toUpdate = _updateBuilder.slice();

  if (toInsert.length() == 0 && toUpdate.length() == 0) {
    _justCopy = true;
    return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
  }

  OperationOptions const& options = info._options;

  TRI_ASSERT(info._trx);
  OperationResult opRes;  // temporary value

  if (toInsert.isArray() && toInsert.length() > 0) {
    OperationResult opRes =
        info._trx->insert(info._aqlCollection->name(), toInsert, options);
    setOperationResult(std::move(opRes));

    if (_operationResult.fail()) {
      THROW_ARANGO_EXCEPTION(_operationResult.result);
    }

    handleBabyStats(stats, info, _operationResult, toInsert.length(), info._ignoreErrors);

    _insertBuilder.clear();
  }

  if (toUpdate.isArray() && toUpdate.length() > 0) {
    if (info._isReplace) {
      opRes = info._trx->replace(info._aqlCollection->name(), toUpdate, options);
    } else {
      opRes = info._trx->update(info._aqlCollection->name(), toUpdate, options);
    }
    setOperationResultUpdate(std::move(opRes));

    if (_operationResultUpdate.fail()) {
      THROW_ARANGO_EXCEPTION(_operationResultUpdate.result);
    }

    handleBabyStats(stats, info, _operationResultUpdate, toUpdate.length(), info._ignoreErrors);

    _tmpBuilder.clear();
    _updateBuilder.clear();
  }

  if (_operationResultArraySlice.length() == 0 &&
      _operationResultArraySliceUpdate.length() == 0) {
    _justCopy = true;
    return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
  }
  return true;
}

bool Upsert::doOutput(ModificationExecutorInfos& info, OutputAqlItemRow& output) {
  TRI_ASSERT(_block != nullptr);

  std::size_t blockSize = _block->size();
  TRI_ASSERT(_last_not_skip <= blockSize);
  TRI_ASSERT(_blockIndex < blockSize);

  while (_blockIndex < _operations.size() &&
         _operations[_blockIndex] == ModOperationType::IGNORE_SKIP) {
    _blockIndex++;
  }

  InputAqlItemRow input = InputAqlItemRow(_block, _blockIndex);
  if (_justCopy || info._options.silent) {
    output.copyRow(input);
  } else {
    auto& op = _operations[_blockIndex];
    if (op == ModOperationType::APPLY_UPDATE || op == ModOperationType::APPLY_INSERT) {
      TRI_ASSERT(_operationResultIterator.valid() ||
                 _operationResultUpdateIterator.valid());

      // fetch operation type (insert or update/replace)
      VPackArrayIterator* iter = &_operationResultIterator;
      if (_operations[_blockIndex] == ModOperationType::APPLY_UPDATE) {
        iter = &_operationResultUpdateIterator;
      }
      auto elm = iter->value();

      bool wasError =
          arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

      if (!wasError) {
        if (info._options.returnNew) {
          AqlValue value(elm.get("new"));
          AqlValueGuard guard(value, true);
          // store $NEW
          output.moveValueInto(info._outputNewRegisterId, input, guard);
        }
      }

      ++*iter;
    } else if (_operations[_blockIndex] == ModOperationType::IGNORE_SKIP) {
      output.copyRow(input);
    } else {
      TRI_ASSERT(false);
    }
  }

  return ++_blockIndex < _last_not_skip;
}

///////////////////////////////////////////////////////////////////////////////
// UPDATE / REPLACE ///////////////////////////////////////////////////////////
template <typename ModType>
bool UpdateReplace<ModType>::doModifications(ModificationExecutorInfos& info,
                                             ModificationStats& stats) {
  OperationOptions const& options = info._options;

  // check if we're a DB server in a cluster
  auto* trx = info._trx;
  bool const isDBServer = trx->state()->isDBServer();
  info._producesResults = ProducesResults(
      info._producesResults || (isDBServer && info._ignoreDocumentNotFound));

  reset();
  _updateOrReplaceBuilder.openArray();

  int errorCode = TRI_ERROR_NO_ERROR;
  std::string errorMessage;
  std::string key;
  std::string rev;
  RegisterId const inDocReg = info._input1RegisterId;
  RegisterId const keyReg = info._input2RegisterId;
  bool const hasKeyVariable = keyReg != ExecutionNode::MaxRegisterId;

  itemBlock::forRowInBlock(_block, [this, &options, &stats, &errorCode,
                                    &errorMessage, &key, &rev, trx, inDocReg, keyReg,
                                    hasKeyVariable, &info](InputAqlItemRow&& row) {
    auto const& inVal = row.getValue(inDocReg);
    errorCode = TRI_ERROR_NO_ERROR;
    errorMessage.clear();

    key.clear();
    rev.clear();

    auto const& inDoc = row.getValue(inDocReg);
    if (inDoc.isObject()) {
      if (hasKeyVariable) {
        AqlValue const& keyVal = row.getValue(keyReg);
        if (options.ignoreRevs) {
          errorCode = extractKey(trx, keyVal, key);
        } else {
          errorCode = extractKeyAndRev(trx, keyVal, key, rev);
        }
      } else /*!hasKeyVariable*/ {
        errorCode = extractKey(trx, inVal, key);
      }
    } else /*inDoc is not an object*/ {
      errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      errorMessage = std::string("expecting 'Object', got: ") + inVal.slice().typeName() +
                     std::string(" while handling: ") + _name;
    }

    if (errorCode == TRI_ERROR_NO_ERROR) {
      if (!info._consultAqlWriteFilter ||
          !info._aqlCollection->getCollection()->skipForAqlWrite(inVal.slice(), key)) {
        _operations.push_back(ModOperationType::APPLY_RETURN);
        if (hasKeyVariable) {
          _tmpBuilder.clear();
          _tmpBuilder.openObject();
          _tmpBuilder.add(StaticStrings::KeyString, VPackValue(key));
          if (!options.ignoreRevs && !rev.empty()) {
            _tmpBuilder.add(StaticStrings::RevString, VPackValue(rev));
          } else {
            // we must never take _rev from the document if there is a key
            // expression.
            _tmpBuilder.add(StaticStrings::RevString, VPackValue(VPackValueType::Null));
          }
          _tmpBuilder.close();
          VPackCollection::merge(_updateOrReplaceBuilder, inVal.slice(),
                                 _tmpBuilder.slice(), false, true);
        } else {
          // use original slice for updating
          _updateOrReplaceBuilder.add(inVal.slice());
        }
      } else {
        // not relevant for ourselves... just pass it on to the next block
        _operations.push_back(ModOperationType::IGNORE_RETURN);
      }
      this->_last_not_skip = _operations.size();
    } else {
      _operations.push_back(ModOperationType::IGNORE_SKIP);
      handleStats(stats, info, errorCode, info._ignoreErrors, &errorMessage);
    }
  });

  TRI_ASSERT(_operations.size() == _block->size());

  _updateOrReplaceBuilder.close();
  auto toUpdateOrReplace = _updateOrReplaceBuilder.slice();

  if (toUpdateOrReplace.length() == 0) {
    _justCopy = true;
    return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
  }

  TRI_ASSERT(info._trx);

  if (toUpdateOrReplace.isArray() && toUpdateOrReplace.length() > 0) {
    OperationResult opRes =
        (info._trx->*_method)(info._aqlCollection->name(), toUpdateOrReplace, options);
    setOperationResult(std::move(opRes));

    if (_operationResult.fail()) {
      THROW_ARANGO_EXCEPTION(_operationResult.result);
    }

    handleBabyStats(stats, info, _operationResult, toUpdateOrReplace.length(),
                    info._ignoreErrors, info._ignoreDocumentNotFound);
  }

  _tmpBuilder.clear();
  _updateOrReplaceBuilder.clear();

  if (_operationResultArraySlice.length() == 0) {
    // there is nothing to update we just need to copy
    // if there is anything other than IGNORE_SKIP the
    // block is prepared.
    _justCopy = true;
    return _last_not_skip != std::numeric_limits<decltype(_last_not_skip)>::max();
  }

  return true;
}

template <typename ModType>
bool UpdateReplace<ModType>::doOutput(ModificationExecutorInfos& info,
                                      OutputAqlItemRow& output) {
  TRI_ASSERT(_block != nullptr);

  std::size_t blockSize = _block->size();
  TRI_ASSERT(_last_not_skip <= blockSize);
  TRI_ASSERT(_blockIndex < blockSize);
  TRI_ASSERT(_operationResultArraySlice.isArray());

  while (_blockIndex < _operations.size() &&
         _operations[_blockIndex] == ModOperationType::IGNORE_SKIP) {
    _blockIndex++;
  }

  InputAqlItemRow input = InputAqlItemRow(_block, _blockIndex);
  if (_justCopy) {
    output.copyRow(input);
  } else {
    if (_operations[_blockIndex] == ModOperationType::APPLY_RETURN) {
      TRI_ASSERT(_operationResultIterator.valid());
      auto elm = _operationResultIterator.value();

      bool wasError =
          arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

      if (!wasError) {
        if (info._options.returnNew) {
          AqlValue value(elm.get("new"));
          AqlValueGuard guard(value, true);
          // store $NEW
          output.moveValueInto(info._outputNewRegisterId, input, guard);
        }
        if (info._options.returnOld) {
          auto slice = elm.get("old");
          AqlValue value(slice);
          AqlValueGuard guard(value, true);
          // store $OLD
          output.moveValueInto(info._outputOldRegisterId, input, guard);
        }
      }
      ++_operationResultIterator;
    } else if (_operations[_blockIndex] == ModOperationType::IGNORE_SKIP) {
      output.copyRow(input);
    } else {
      TRI_ASSERT(false);
    }
  }

  return ++_blockIndex < _last_not_skip;
}

template struct arangodb::aql::UpdateReplace<Update>;
template struct arangodb::aql::UpdateReplace<Replace>;
