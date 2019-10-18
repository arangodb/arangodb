////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "DocumentProducingHelper.h"

#include "Aql/AqlValue.h"
#include "Aql/DocumentExpressionContext.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

void aql::handleProjections(std::vector<std::pair<ProjectionType, std::string>> const& projections,
                            transaction::Methods const* trxPtr, VPackSlice slice,
                            VPackBuilder& b, bool useRawDocumentPointers) {
  for (auto const& it : projections) {
    if (it.first == ProjectionType::IdAttribute) {
      b.add(it.second, VPackValue(transaction::helpers::extractIdString(trxPtr->resolver(), slice, slice)));
      continue;
    } else if (it.first == ProjectionType::KeyAttribute) {
      VPackSlice found = transaction::helpers::extractKeyFromDocument(slice);
      if (useRawDocumentPointers) {
        b.add(VPackValue(it.second));
        b.addExternal(found.begin());
      } else {
        b.add(it.second, found);
      }
      continue;
    }

    VPackSlice found = slice.get(it.second);
    if (found.isNone()) {
      // attribute not found
      b.add(it.second, VPackValue(VPackValueType::Null));
    } else {
      if (useRawDocumentPointers) {
        b.add(VPackValue(it.second));
        b.addExternal(found.begin());
      } else {
        b.add(it.second, found);
      }
    }
  }
}

template <bool checkUniqueness>
DocumentProducingFunction aql::getCallback(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
                                           DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice slice) {
    if (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return;
      }
    }
    if (context.hasFilter()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return;
      }
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    transaction::BuilderLeaser b(context.getTrxPtr());
    b->openObject(true);

    handleProjections(context.getProjections(), context.getTrxPtr(), slice,
                      *b.get(), context.getUseRawDocumentPointers());

    b->close();

    AqlValue v(b.get());
    AqlValueGuard guard{v, true};
    TRI_ASSERT(!output.isFull());
    output.moveValueInto(registerId, input, guard);
    TRI_ASSERT(output.produced());
    output.advanceRow();
    context.incrScanned();
  };
}

template <bool checkUniqueness>
DocumentProducingFunction aql::getCallback(DocumentProducingCallbackVariant::DocumentWithRawPointer,
                                           DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice slice) {
    if (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return;
      }
    }
    if (context.hasFilter()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return;
      }
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();
    uint8_t const* vpack = slice.begin();
    // With NoCopy we do not clone
    TRI_ASSERT(!output.isFull());
    AqlValue v{AqlValueHintDocumentNoCopy{vpack}};
    AqlValueGuard guard{v, false};
    output.moveValueInto(registerId, input, guard);
    TRI_ASSERT(output.produced());
    output.advanceRow();
    context.incrScanned();
  };
}

template <bool checkUniqueness>
DocumentProducingFunction aql::getCallback(DocumentProducingCallbackVariant::DocumentCopy,
                                           DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice slice) {
    if (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return;
      }
    }
    if (context.hasFilter()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return;
      }
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();
    uint8_t const* vpack = slice.begin();

    // Here we do a clone, so clone once, then move into
    AqlValue v{AqlValueHintCopy{vpack}};
    AqlValueGuard guard{v, true};
    TRI_ASSERT(!output.isFull());
    output.moveValueInto(registerId, input, guard);
    TRI_ASSERT(output.produced());
    output.advanceRow();
    context.incrScanned();
  };
}

template <bool checkUniqueness>
DocumentProducingFunction aql::buildCallback(DocumentProducingFunctionContext& context) {
  if (!context.getProduceResult()) {
    // This callback is disallowed use getNullCallback instead
    TRI_ASSERT(false);
    return [](LocalDocumentId const&, VPackSlice slice) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    };
  }

  if (!context.getProjections().empty()) {
    // return a projection
    if (!context.getCoveringIndexAttributePositions().empty()) {
      // projections from an index value (covering index)
      return getCallback<checkUniqueness>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex{},
                                          context);
    } else {
      // projections from a "real" document
      return getCallback<checkUniqueness>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex{},
                                          context);
    }
  }

  // return the document as is
  if (context.getUseRawDocumentPointers()) {
    return getCallback<checkUniqueness>(DocumentProducingCallbackVariant::DocumentWithRawPointer{},
                                        context);
  } else {
    return getCallback<checkUniqueness>(DocumentProducingCallbackVariant::DocumentCopy{}, context);
  }
}

template <bool checkUniqueness>
std::function<void(LocalDocumentId const& token)> aql::getNullCallback(DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token) {
    TRI_ASSERT(!context.hasFilter());

    if (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return;
      }
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();
    // TODO: optimize this within the register planning mechanism?
    TRI_ASSERT(!output.isFull());
    output.cloneValueInto(registerId, input, AqlValue(AqlValueHintNull()));
    TRI_ASSERT(output.produced());
    output.advanceRow();
    context.incrScanned();
  };
}

DocumentProducingFunctionContext::DocumentProducingFunctionContext(
    InputAqlItemRow const& inputRow, OutputAqlItemRow* outputRow,
    RegisterId const outputRegister, bool produceResult,
    Query* query, Expression* filter,
    std::vector<std::string> const& projections, 
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool allowCoveringIndexOptimization, bool useRawDocumentPointers, bool checkUniqueness)
    : _inputRow(inputRow),
      _outputRow(outputRow),
      _query(query),
      _filter(filter),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _numScanned(0),
      _numFiltered(0),
      _outputRegister(outputRegister),
      _produceResult(produceResult),
      _useRawDocumentPointers(useRawDocumentPointers),
      _allowCoveringIndexOptimization(allowCoveringIndexOptimization),
      _isLastIndex(false),
      _checkUniqueness(checkUniqueness) {

  _projections.reserve(projections.size());
  for (auto const& it : projections) {
    ProjectionType type = ProjectionType::OtherAttribute;
    if (it == StaticStrings::IdString) {
      type = ProjectionType::IdAttribute;
    } else if (it == StaticStrings::KeyString) {
      type = ProjectionType::KeyAttribute;
    }
    _projections.emplace_back(type, it);
  }
}

void DocumentProducingFunctionContext::setOutputRow(OutputAqlItemRow* outputRow) {
  _outputRow = outputRow;
}

bool DocumentProducingFunctionContext::getProduceResult() const noexcept {
  return _produceResult;
}

std::vector<std::pair<ProjectionType, std::string>> const& DocumentProducingFunctionContext::getProjections() const noexcept {
  return _projections;
}

transaction::Methods* DocumentProducingFunctionContext::getTrxPtr() const noexcept {
  return _query->trx();
}

std::vector<size_t> const& DocumentProducingFunctionContext::getCoveringIndexAttributePositions() const
    noexcept {
  return _coveringIndexAttributePositions;
}

bool DocumentProducingFunctionContext::getAllowCoveringIndexOptimization() const noexcept {
  return _allowCoveringIndexOptimization;
}

bool DocumentProducingFunctionContext::getUseRawDocumentPointers() const noexcept {
  return _useRawDocumentPointers;
}

void DocumentProducingFunctionContext::setAllowCoveringIndexOptimization(bool allowCoveringIndexOptimization) noexcept {
  _allowCoveringIndexOptimization = allowCoveringIndexOptimization;
}

void DocumentProducingFunctionContext::incrScanned() noexcept { ++_numScanned; }

void DocumentProducingFunctionContext::incrFiltered() noexcept { ++_numFiltered; }

size_t DocumentProducingFunctionContext::getAndResetNumScanned() noexcept {
  size_t const numScanned = _numScanned;
  _numScanned = 0;
  return numScanned;
}

size_t DocumentProducingFunctionContext::getAndResetNumFiltered() noexcept {
  size_t const numFiltered = _numFiltered;
  _numFiltered = 0;
  return numFiltered;
}

InputAqlItemRow const& DocumentProducingFunctionContext::getInputRow() const noexcept {
  return _inputRow;
}

OutputAqlItemRow& DocumentProducingFunctionContext::getOutputRow() const noexcept {
  return *_outputRow;
}

RegisterId DocumentProducingFunctionContext::getOutputRegister() const noexcept {
  return _outputRegister;
}

bool DocumentProducingFunctionContext::checkUniqueness(LocalDocumentId const& token) {
  if (_checkUniqueness) {
    if (!_isLastIndex) {
      // insert & check for duplicates in one go
      if (!_alreadyReturned.insert(token.id()).second) {
        // Document already in list. Skip this
        return false;
      }
    } else {
      // only check for duplicates
      if (_alreadyReturned.find(token.id()) != _alreadyReturned.end()) {
        // Document found, skip
        return false;
      }
    }
  }
  return true;
}

bool DocumentProducingFunctionContext::checkFilter(velocypack::Slice slice) {
  DocumentExpressionContext ctx(_query, slice);

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _filter->execute(getTrxPtr(), &ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  return a.toBoolean();
}

void DocumentProducingFunctionContext::reset() {
  if (_checkUniqueness) {
    _alreadyReturned.clear();
    _isLastIndex = false;
  }
}

void DocumentProducingFunctionContext::setIsLastIndex(bool val) {
  _isLastIndex = val;
}

bool DocumentProducingFunctionContext::hasFilter() const noexcept {
  return _filter != nullptr;
}

template <bool checkUniqueness>
DocumentProducingFunction aql::getCallback(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
                                           DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice slice) {
    if (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return;
      }
    }
    if (context.hasFilter()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return;
      }
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    transaction::BuilderLeaser b(context.getTrxPtr());
    b->openObject(true);

    if (context.getAllowCoveringIndexOptimization()) {
      // a potential call by a covering index iterator...
      bool const isArray = slice.isArray();
      size_t i = 0;
      VPackSlice found;
      for (auto const& it : context.getProjections()) {
        if (isArray) {
          // we will get a Slice with an array of index values. now we need
          // to look up the array values from the correct positions to
          // populate the result with the projection values this case will
          // be triggered for indexes that can be set up on any number of
          // attributes (hash/skiplist)
          found = slice.at(context.getCoveringIndexAttributePositions()[i]);
          ++i;
        } else {
          // no array Slice... this case will be triggered for indexes that
          // contain simple string values, such as the primary index or the
          // edge index
          found = slice;
        }
        if (found.isNone()) {
          // attribute not found
          b->add(it.second, VPackValue(VPackValueType::Null));
        } else {
          if (context.getUseRawDocumentPointers()) {
            b->add(VPackValue(it.second));
            b->addExternal(found.begin());
          } else {
            b->add(it.second, found);
          }
        }
      }
    } else {
      // projections from a "real" document
      handleProjections(context.getProjections(), context.getTrxPtr(), slice,
                        *b.get(), context.getUseRawDocumentPointers());
    }

    b->close();
    AqlValue v(b.get());
    AqlValueGuard guard{v, true};
    TRI_ASSERT(!output.isFull());
    output.moveValueInto(registerId, input, guard);
    TRI_ASSERT(output.produced());
    output.advanceRow();
    context.incrScanned();
  };
}

template DocumentProducingFunction aql::getCallback<false>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex, DocumentProducingFunctionContext& context);
template DocumentProducingFunction aql::getCallback<true>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex, DocumentProducingFunctionContext& context);

template DocumentProducingFunction aql::getCallback<false>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex, DocumentProducingFunctionContext& context);
template DocumentProducingFunction aql::getCallback<true>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex, DocumentProducingFunctionContext& context);

template DocumentProducingFunction aql::getCallback<false>(DocumentProducingCallbackVariant::DocumentWithRawPointer, DocumentProducingFunctionContext& context);
template DocumentProducingFunction aql::getCallback<true>(DocumentProducingCallbackVariant::DocumentWithRawPointer, DocumentProducingFunctionContext& context);

template DocumentProducingFunction aql::getCallback<false>(DocumentProducingCallbackVariant::DocumentCopy, DocumentProducingFunctionContext& context);
template DocumentProducingFunction aql::getCallback<true>(DocumentProducingCallbackVariant::DocumentCopy, DocumentProducingFunctionContext& context);

template std::function<void(LocalDocumentId const& token)> aql::getNullCallback<false>(DocumentProducingFunctionContext& context);
template std::function<void(LocalDocumentId const& token)> aql::getNullCallback<true>(DocumentProducingFunctionContext& context);

template DocumentProducingFunction aql::buildCallback<false>(DocumentProducingFunctionContext& context);
template DocumentProducingFunction aql::buildCallback<true>(DocumentProducingFunctionContext& context);
