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
#include "Aql/DocumentIndexExpressionContext.h"
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
                            VPackBuilder& b) {
  for (auto const& it : projections) {
    if (it.first == ProjectionType::IdAttribute) {
      b.add(it.second, VPackValue(transaction::helpers::extractIdString(trxPtr->resolver(), slice, slice)));
      continue;
    } else if (it.first == ProjectionType::KeyAttribute) {
      VPackSlice found = transaction::helpers::extractKeyFromDocument(slice);
      b.add(it.second, found);
      continue;
    }

    VPackSlice found = slice.get(it.second);
    if (found.isNone()) {
      // attribute not found
      b.add(it.second, VPackValue(VPackValueType::Null));
    } else {
      b.add(it.second, found);
    }
  }
}

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
                                                 DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice slice) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }
    
    context.incrScanned();
    
    if (context.hasFilter()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return false;
      }
    }

    if constexpr (skip) {
      return true;
    }

    // recycle our Builder object
    VPackBuilder& objectBuilder = context.getBuilder();
    objectBuilder.clear();
    objectBuilder.openObject(true);

    handleProjections(context.getProjections(), context.getTrxPtr(), slice,
                      objectBuilder);

    objectBuilder.close();
    
    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    TRI_ASSERT(!output.isFull());
    output.moveValueInto<InputAqlItemRow, VPackSlice const>(registerId, input, objectBuilder.slice());
    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
}

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(DocumentProducingCallbackVariant::DocumentCopy,
                                                 DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice const slice) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }
    
    context.incrScanned();
    
    if (context.hasFilter()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return false;
      }
    }
    
    if constexpr (skip) {
      return true;
    }

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    TRI_ASSERT(!output.isFull());
    output.moveValueInto(registerId, input, slice);
    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
}

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::buildDocumentCallback(DocumentProducingFunctionContext& context) {
  if constexpr (!skip) {
    if (!context.getProduceResult()) {
      // This callback is disallowed use getNullCallback instead
      TRI_ASSERT(false);
      return [](LocalDocumentId const&, VPackSlice slice) -> bool {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid callback");
      };
    }
  }
    
  if (!context.getProjections().empty()) {
    // return a projection
    if (!context.getCoveringIndexAttributePositions().empty()) {
      // projections from an index value (covering index)
      return getCallback<checkUniqueness, skip>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex{},
                                                context);
    } else {
      // projections from a "real" document
      return getCallback<checkUniqueness, skip>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex{},
                                                context);
    }
  }

  // return the document as is
  return getCallback<checkUniqueness, skip>(DocumentProducingCallbackVariant::DocumentCopy{}, context);
}

template <bool checkUniqueness>
std::function<bool(LocalDocumentId const& token)> aql::getNullCallback(DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token) {
    TRI_ASSERT(!context.hasFilter());

    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();
    // TODO: optimize this within the register planning mechanism?
    TRI_ASSERT(!output.isFull());
    output.cloneValueInto(registerId, input, AqlValue(AqlValueHintNull()));
    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
}

DocumentProducingFunctionContext::DocumentProducingFunctionContext(
    InputAqlItemRow const& inputRow, OutputAqlItemRow* outputRow,
    RegisterId const outputRegister, bool produceResult,
    aql::QueryContext& query, transaction::Methods& trx, Expression* filter,
    std::vector<std::string> const& projections, 
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool allowCoveringIndexOptimization, bool checkUniqueness)
    : _inputRow(inputRow),
      _outputRow(outputRow),
      _query(query),
      _trx(trx),
      _filter(filter),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _numScanned(0),
      _numFiltered(0),
      _outputRegister(outputRegister),
      _produceResult(produceResult),
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
  return &_trx;
}
  
arangodb::velocypack::Builder& DocumentProducingFunctionContext::getBuilder() noexcept {
  return _objectBuilder;
}

std::vector<size_t> const& DocumentProducingFunctionContext::getCoveringIndexAttributePositions() const
    noexcept {
  return _coveringIndexAttributePositions;
}

bool DocumentProducingFunctionContext::getAllowCoveringIndexOptimization() const noexcept {
  return _allowCoveringIndexOptimization;
}

void DocumentProducingFunctionContext::setAllowCoveringIndexOptimization(bool allowCoveringIndexOptimization) noexcept {
  _allowCoveringIndexOptimization = allowCoveringIndexOptimization;
}

void DocumentProducingFunctionContext::incrScanned() noexcept { ++_numScanned; }

void DocumentProducingFunctionContext::incrFiltered() noexcept { 
  ++_numFiltered; 
}

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
  TRI_ASSERT(_checkUniqueness);
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
  return true;
}

bool DocumentProducingFunctionContext::checkFilter(velocypack::Slice slice) {
  DocumentExpressionContext ctx(_trx, _query, _aqlFunctionsInternalCache, slice);
  return checkFilter(ctx);
}

bool DocumentProducingFunctionContext::checkFilter(
    AqlValue (*getValue)(void const* ctx, Variable const* var, bool doCopy),
    void const* filterContext) {
  DocumentIndexExpressionContext ctx(_trx, _query, _aqlFunctionsInternalCache, getValue, filterContext);
  return checkFilter(ctx);
}

bool DocumentProducingFunctionContext::checkFilter(ExpressionContext& ctx) {
  if (ADB_UNLIKELY(_numScanned % 1024 == 0 && _query.killed())) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _filter->execute(&ctx, mustDestroy);
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

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
                                                 DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId const& token, VPackSlice slice) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    bool checkFilter = context.hasFilter();
    if (checkFilter && !context.getAllowCoveringIndexOptimization()) {
      if (!context.checkFilter(slice)) {
        context.incrFiltered();
        return false;
      }
      // no need to check later again
      checkFilter = false;
    }

    // recycle our Builder object
    VPackBuilder& objectBuilder = context.getBuilder();
    objectBuilder.clear();
    objectBuilder.openObject(true);

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
          objectBuilder.add(it.second, VPackValue(VPackValueType::Null));
        } else {
          objectBuilder.add(it.second, found);
        }
      }
    } else {
      // projections from a "real" document
      handleProjections(context.getProjections(), context.getTrxPtr(), slice,
                        objectBuilder);
    }

    objectBuilder.close();
    
    if (checkFilter && !context.checkFilter(objectBuilder.slice())) {
      context.incrFiltered();
      return false;
    }
    
    if constexpr (!skip) {
      InputAqlItemRow const& input = context.getInputRow();
      OutputAqlItemRow& output = context.getOutputRow();
      RegisterId registerId = context.getOutputRegister();
      TRI_ASSERT(!output.isFull());
      output.moveValueInto<InputAqlItemRow, VPackSlice const>(registerId, input, objectBuilder.slice());
      TRI_ASSERT(output.produced());
      output.advanceRow();
    }

    return true;
  };
}

template IndexIterator::DocumentCallback aql::getCallback<false, false>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, false>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<false, true>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, true>(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex, DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::getCallback<false, false>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, false>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<false, true>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, true>(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex, DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::getCallback<false, false>(DocumentProducingCallbackVariant::DocumentCopy, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, false>(DocumentProducingCallbackVariant::DocumentCopy, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<false, true>(DocumentProducingCallbackVariant::DocumentCopy, DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, true>(DocumentProducingCallbackVariant::DocumentCopy, DocumentProducingFunctionContext& context);

template IndexIterator::LocalDocumentIdCallback aql::getNullCallback<false>(DocumentProducingFunctionContext& context);
template IndexIterator::LocalDocumentIdCallback aql::getNullCallback<true>(DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::buildDocumentCallback<false, false>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<true, false>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<false, true>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<true, true>(DocumentProducingFunctionContext& context);
