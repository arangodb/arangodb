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

void aql::handleProjections(std::vector<AttributeProjection> const& projections,
                            transaction::Methods const* trxPtr, VPackSlice slice,
                            VPackBuilder& b) {
  for (auto const& it : projections) {
    if (it.type == AttributeNamePath::Type::IdAttribute) {
      // projection for "_id"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], VPackValue(transaction::helpers::extractIdString(trxPtr->resolver(), slice, slice)));
    } else if (it.type == AttributeNamePath::Type::KeyAttribute) {
      // projection for "_key"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractKeyFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::SingleAttribute) {
      // projection for any other top-level attribute
      TRI_ASSERT(it.path.size() == 1);
      VPackSlice found = slice.get(it.path.path[0]);
      if (found.isNone()) {
        // attribute not found
        b.add(it.path[0], VPackValue(VPackValueType::Null));
      } else {
        b.add(it.path[0], found);
      }
    } else if (it.type == AttributeNamePath::Type::FromAttribute) {
      // projection for "_from"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractFromFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::ToAttribute) {
      // projection for "_to"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractToFromDocument(slice));
    } else {
      // projection for a sub-attribute, e.g. a.b.c
      TRI_ASSERT(it.type == AttributeNamePath::Type::MultiAttribute);
      TRI_ASSERT(it.path.size() > 1);
      size_t level = 0;
      VPackSlice found = slice;
      size_t const n = it.path.size();
      while (level < n) {
        found = found.get(it.path[level]);
        if (found.isNone()) {
          b.add(it.path[level], VPackValue(VPackValueType::Null));
          break;
        }
        if (level < n - 1 && !found.isObject()) {
          // premature end
          b.add(it.path[level], VPackValue(VPackValueType::Null));
          break;
        }
        if (level == n - 1) {
          // target value
          b.add(it.path[level], found);
          break;
        }
        // recurse into sub-attribute object
        b.add(it.path[level], VPackValue(VPackValueType::Object));
        ++level;
      }
 
      // close all that we opened oursevles 
      while (level-- > 0) {
        b.close();
      }
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
    std::vector<arangodb::aql::AttributeNamePath> const& projections, 
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

  initializeProjections(projections);
}

void DocumentProducingFunctionContext::initializeProjections(std::vector<arangodb::aql::AttributeNamePath> const& projections) {
  TRI_ASSERT(_projections.empty());
  _projections.reserve(projections.size());

  // classify and order projections
  for (auto const& it : projections) {
    _projections.emplace_back(AttributeProjection{it, it.type()});
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_projections.size() > 1) {
    for (auto it = _projections.begin(); it != _projections.end(); ++it) {
      // we expect all our projections to not have any common prefixes,
      // because that would immensely complicate the logic inside 
      // handleProjections
      auto const& current = (*it).path;
      // this is a quadratic algorithm (:gut:), but it is only activated
      // as a safety check in maintainer mode, plus we are guaranteed to have
      // at most five projections right now
      auto it2 = std::find_if(_projections.begin(), _projections.end(), [&current](auto const& other) {
        return other.path[0] == current.path[0] && other.path.size() != current.path.size();
      });
      TRI_ASSERT(it2 == _projections.end());
    }
  }
#endif
}

void DocumentProducingFunctionContext::setOutputRow(OutputAqlItemRow* outputRow) {
  _outputRow = outputRow;
}

bool DocumentProducingFunctionContext::getProduceResult() const noexcept {
  return _produceResult;
}

std::vector<AttributeProjection> const& DocumentProducingFunctionContext::getProjections() const noexcept {
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
    if (!_alreadyReturned.insert(token).second) {
      // Document already in list. Skip this
      return false;
    }
  } else {
    // only check for duplicates
    if (_alreadyReturned.find(token) != _alreadyReturned.end()) {
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
      for (auto const& it : context.getProjections()) {
        if (isArray) {
          // we will get a Slice with an array of index values. now we need
          // to look up the array values from the correct positions to
          // populate the result with the projection values this case will
          // be triggered for indexes that can be set up on any number of
          // attributes (hash/skiplist)
          VPackSlice found = slice.at(context.getCoveringIndexAttributePositions()[i]);
          if (found.isNone()) {
            found = VPackSlice::nullSlice();
          }
          for (size_t j = 0; j < it.path.size() - 1; ++j) {
            objectBuilder.add(it.path[j], VPackValue(VPackValueType::Object));
          }
          objectBuilder.add(it.path.path.back(), found);
          for (size_t j = 0; j < it.path.size() - 1; ++j) {
            objectBuilder.close();
          }
          ++i;
        } else {
          // no array Slice... this case will be triggered for indexes that
          // contain simple string values, such as the primary index or the
          // edge index
          if (slice.isNone()) {
            slice = VPackSlice::nullSlice();
          }
          objectBuilder.add(it.path[0], slice);
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
