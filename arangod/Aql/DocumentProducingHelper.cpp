////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "DocumentProducingHelper.h"

#include "Aql/AqlValue.h"
#include "Aql/DocumentExpressionContext.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/Expression.h"
#include "Aql/IndexExecutor.h"
#include "Aql/LateMaterializedExpressionContext.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Basics/DownCast.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
namespace {

template<typename Func>
struct MultiFunc2MultiFunc {
  bool operator()(LocalDocumentId token, velocypack::Slice doc) const {
    return func(token, doc, doc);
  }
  bool operator()(LocalDocumentId token,
                  std::unique_ptr<std::string>& doc) const {
    TRI_ASSERT(doc);
    VPackSlice slice{reinterpret_cast<uint8_t const*>(doc->data())};
    return func(token, &doc, slice);
  }

  Func func;
};

template<typename Func>
IndexIterator::DocumentCallback makeDocumentCallback(Func&& func) {
  return {MultiFunc2MultiFunc<std::decay_t<Func>>{std::forward<Func>(func)}};
}

}  // namespace

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context) {
  auto cb = [&context](LocalDocumentId token, VPackSlice slice) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    if (context.hasFilter() && !context.checkFilter(slice)) {
      context.incrFiltered();
      return false;
    }

    if constexpr (skip) {
      return true;
    }

    // recycle our Builder object
    VPackBuilder& objectBuilder = context.getBuilder();
    objectBuilder.clear();
    objectBuilder.openObject(true);
    context.getProjections().toVelocyPackFromDocument(objectBuilder, slice,
                                                      context.getTrxPtr());
    objectBuilder.close();

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    TRI_ASSERT(!output.isFull());
    VPackSlice s = objectBuilder.slice();
    output.moveValueInto(registerId, input, s);
    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
  return IndexIterator::makeDocumentCallbackF(cb);
}

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context) {
  return makeDocumentCallback(
      [&](LocalDocumentId token, auto v, velocypack::Slice s) {
        if constexpr (checkUniqueness) {
          if (!context.checkUniqueness(token)) {
            // Document already found, skip it
            return false;
          }
        }

        context.incrScanned();

        if (context.hasFilter() && !context.checkFilter(s)) {
          context.incrFiltered();
          return false;
        }

        if constexpr (skip) {
          return true;
        }

        InputAqlItemRow const& input = context.getInputRow();
        OutputAqlItemRow& output = context.getOutputRow();
        RegisterId registerId = context.getOutputRegister();

        TRI_ASSERT(!output.isFull());
        output.moveValueInto(registerId, input, v);
        TRI_ASSERT(output.produced());
        output.advanceRow();

        return true;
      });
}

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::buildDocumentCallback(
    DocumentProducingFunctionContext& context) {
  if constexpr (!skip) {
    if (!context.getProduceResult()) {
      // This callback is disallowed use getNullCallback instead
      TRI_ASSERT(false);
      return IndexIterator::makeDocumentCallbackF(
          [](LocalDocumentId, VPackSlice /*slice*/) -> bool {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           "invalid callback");
          });
    }
  }

  if (!context.getProjections().empty()) {
    // return a projection
    TRI_ASSERT(!context.getProjections().usesCoveringIndex() ||
               !context.getAllowCoveringIndexOptimization());
    // projections from a "real" document
    return getCallback<checkUniqueness, skip>(
        DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex{},
        context);
  }
  // return the document as is
  return getCallback<checkUniqueness, skip>(
      DocumentProducingCallbackVariant::DocumentCopy{}, context);
}

template<bool checkUniqueness>
IndexIterator::LocalDocumentIdCallback aql::getNullCallback(
    DocumentProducingFunctionContext& context) {
  TRI_ASSERT(!context.hasFilter());

  return [&context](LocalDocumentId token) {
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
    transaction::Methods& trx, InputAqlItemRow const& inputRow,
    EnumerateCollectionExecutorInfos& infos)
    : _inputRow(inputRow),
      _outputRow(nullptr),
      _query(infos.getQuery()),
      _trx(trx),
      _physical(*infos.getCollection()->getCollection()->getPhysical()),
      _filter(infos.getFilter()),
      _projections(infos.getProjections()),
      _filterProjections(infos.getFilterProjections()),
      _resourceMonitor(infos.getResourceMonitor()),
      _numScanned(0),
      _numFiltered(0),
      _outputVariable(infos.getOutVariable()),
      _outputRegister(infos.getOutputRegisterId()),
      _readOwnWrites(infos.canReadOwnWrites()),
      _checkUniqueness(false),
      _produceResult(infos.getProduceResult()),
      _allowCoveringIndexOptimization(false),
      _isLastIndex(false) {
  // build ExpressionContext for filtering if we need one
  if (hasFilter()) {
    TRI_ASSERT(_outputVariable != nullptr);
    auto const& filterVars = infos.getFilterVarsToRegister();
    if (filterVars.size() == 1 && filterVars[0].first == _outputVariable->id) {
      // filter condition only refers to the current document, but no other
      // variables. we can get away with building a very simple expression
      // context
      _expressionContext = std::make_unique<SimpleDocumentExpressionContext>(
          _trx, _query, _aqlFunctionsInternalCache,
          infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
    } else {
      // filter condition refers to addition variables.
      // we have to use a more generic expression context
      _expressionContext = std::make_unique<GenericDocumentExpressionContext>(
          _trx, _query, _aqlFunctionsInternalCache,
          infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
    }
  }
}

DocumentProducingFunctionContext::DocumentProducingFunctionContext(
    transaction::Methods& trx, InputAqlItemRow const& inputRow,
    IndexExecutorInfos& infos)
    : _inputRow(inputRow),
      _outputRow(nullptr),
      _query(infos.query()),
      _trx(trx),
      _physical(*infos.getCollection()->getCollection()->getPhysical()),
      _filter(infos.getFilter()),
      _projections(infos.getProjections()),
      _filterProjections(infos.getFilterProjections()),
      _resourceMonitor(infos.getResourceMonitor()),
      _numScanned(0),
      _numFiltered(0),
      _outputVariable(infos.getOutVariable()),
      _outputRegister(infos.getOutputRegisterId()),
      _readOwnWrites(infos.canReadOwnWrites()),
      _checkUniqueness(infos.getIndexes().size() > 1 ||
                       infos.hasMultipleExpansions()),
      _produceResult(infos.getProduceResult()),
      _allowCoveringIndexOptimization(false),  // can be updated later
      _isLastIndex(false) {
  // build ExpressionContext for filtering if we need one
  if (hasFilter()) {
    if (infos.isLateMaterialized()) {
      // special handling for late materialization
      _expressionContext = std::make_unique<LateMaterializedExpressionContext>(
          _trx, _query, _aqlFunctionsInternalCache,
          infos.getFilterVarsToRegister(), _inputRow,
          infos.getOutNonMaterializedIndVars());
    } else {
      TRI_ASSERT(_outputVariable != nullptr);
      auto const& filterVars = infos.getFilterVarsToRegister();
      if (filterVars.size() == 1 &&
          filterVars[0].first == _outputVariable->id) {
        // filter condition only refers to the current document, but no other
        // variables. we can get away with building a very simple expression
        // context
        _expressionContext = std::make_unique<SimpleDocumentExpressionContext>(
            _trx, _query, _aqlFunctionsInternalCache,
            infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
      } else {
        // filter condition refers to additional variables.
        // we have to use a more generic expression context
        _expressionContext = std::make_unique<GenericDocumentExpressionContext>(
            _trx, _query, _aqlFunctionsInternalCache,
            infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
      }
    }
  }
}

DocumentProducingFunctionContext::~DocumentProducingFunctionContext() {
  // must be called to count down memory usage
  reset();
}

void DocumentProducingFunctionContext::setOutputRow(
    OutputAqlItemRow* outputRow) {
  _outputRow = outputRow;
}

bool DocumentProducingFunctionContext::getProduceResult() const noexcept {
  return _produceResult;
}

aql::Projections const& DocumentProducingFunctionContext::getProjections()
    const noexcept {
  return _projections;
}

aql::Projections const& DocumentProducingFunctionContext::getFilterProjections()
    const noexcept {
  return _filterProjections;
}

transaction::Methods* DocumentProducingFunctionContext::getTrxPtr()
    const noexcept {
  return &_trx;
}

PhysicalCollection& DocumentProducingFunctionContext::getPhysical()
    const noexcept {
  return _physical;
}

velocypack::Builder& DocumentProducingFunctionContext::getBuilder() noexcept {
  return _objectBuilder;
}

bool DocumentProducingFunctionContext::getAllowCoveringIndexOptimization()
    const noexcept {
  return _allowCoveringIndexOptimization;
}

void DocumentProducingFunctionContext::setAllowCoveringIndexOptimization(
    bool allowCoveringIndexOptimization) noexcept {
  _allowCoveringIndexOptimization = allowCoveringIndexOptimization;
}

void DocumentProducingFunctionContext::incrScanned() noexcept { ++_numScanned; }

void DocumentProducingFunctionContext::incrFiltered() noexcept {
  ++_numFiltered;
}

uint64_t DocumentProducingFunctionContext::getAndResetNumScanned() noexcept {
  return std::exchange(_numScanned, 0);
}

uint64_t DocumentProducingFunctionContext::getAndResetNumFiltered() noexcept {
  return std::exchange(_numFiltered, 0);
}

InputAqlItemRow const& DocumentProducingFunctionContext::getInputRow()
    const noexcept {
  return _inputRow;
}

OutputAqlItemRow& DocumentProducingFunctionContext::getOutputRow()
    const noexcept {
  TRI_ASSERT(_outputRow != nullptr);
  return *_outputRow;
}

RegisterId DocumentProducingFunctionContext::getOutputRegister()
    const noexcept {
  return _outputRegister;
}

ReadOwnWrites DocumentProducingFunctionContext::getReadOwnWrites()
    const noexcept {
  return _readOwnWrites;
}

bool DocumentProducingFunctionContext::checkUniqueness(LocalDocumentId token) {
  TRI_ASSERT(_checkUniqueness);
  if (!_isLastIndex) {
    // insert & check for duplicates in one go
    if (!_alreadyReturned.emplace(token).second) {
      // Document already in list. Skip this
      return false;
    }
    // increase memory usage for unique values in set.
    // note: we assume that there is some overhead here for
    // managing the set. right now there is no way to track
    // this overhead accurately, so we don't even try.
    // this is ok as long as recorded memory usage grows
    // with each item in the set
    _resourceMonitor.increaseMemoryUsage(
        sizeof(decltype(_alreadyReturned)::value_type));
  } else {
    // only check for duplicates
    if (_alreadyReturned.contains(token)) {
      // Document found, skip
      return false;
    }
  }
  return true;
}

bool DocumentProducingFunctionContext::checkFilter(velocypack::Slice slice) {
  TRI_ASSERT(_expressionContext != nullptr);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!_expressionContext->isLateMaterialized());
#endif

  auto& ctx = basics::downCast<DocumentExpressionContext>(*_expressionContext);
  ctx.setCurrentDocument(slice);
  return checkFilter(ctx);
}

bool DocumentProducingFunctionContext::checkFilter(
    IndexIteratorCoveringData const* covering) {
  TRI_ASSERT(covering != nullptr);
  TRI_ASSERT(_expressionContext != nullptr);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_expressionContext->isLateMaterialized());
#endif

  auto& ctx =
      basics::downCast<LateMaterializedExpressionContext>(*_expressionContext);
  ctx.setCurrentCoveringData(covering);
  return checkFilter(ctx);
}

bool DocumentProducingFunctionContext::checkFilter(
    DocumentProducingExpressionContext& ctx) {
  _killCheckCounter = (_killCheckCounter + 1) % 1024;
  if (ADB_UNLIKELY(_killCheckCounter == 0 && _query.killed())) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _filter->execute(&ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  return a.toBoolean();
}

void DocumentProducingFunctionContext::reset() {
  if (_checkUniqueness) {
    // count down memory usage
    _resourceMonitor.decreaseMemoryUsage(
        _alreadyReturned.size() *
        sizeof(decltype(_alreadyReturned)::value_type));
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

template<bool checkUniqueness, bool skip>
IndexIterator::CoveringCallback aql::getCallback(
    DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
    DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId token,
                    IndexIteratorCoveringData& covering) {
    TRI_ASSERT(context.getAllowCoveringIndexOptimization());
    if constexpr (checkUniqueness) {
      TRI_ASSERT(token.isSet());
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    // recycle our Builder object
    VPackBuilder& objectBuilder = context.getBuilder();
    if (!skip || context.hasFilter()) {
      objectBuilder.clear();
      objectBuilder.openObject(true);

      // projections from a covering index
      context.getProjections().toVelocyPackFromIndex(objectBuilder, covering,
                                                     context.getTrxPtr());

      objectBuilder.close();

      if (context.hasFilter() && !context.checkFilter(objectBuilder.slice())) {
        context.incrFiltered();
        return false;
      }
    }
    if constexpr (!skip) {
      InputAqlItemRow const& input = context.getInputRow();
      OutputAqlItemRow& output = context.getOutputRow();
      RegisterId registerId = context.getOutputRegister();
      TRI_ASSERT(!output.isFull());
      VPackSlice s = objectBuilder.slice();
      output.moveValueInto(registerId, input, s);
      TRI_ASSERT(output.produced());
      output.advanceRow();
    }

    return true;
  };
}

template<bool checkUniqueness, bool skip>
IndexIterator::CoveringCallback aql::getCallback(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context) {
  return [&](LocalDocumentId token, IndexIteratorCoveringData& covering) {
    // this must only be used if we have a filter!
    TRI_ASSERT(context.hasFilter());
    TRI_ASSERT(context.getAllowCoveringIndexOptimization());
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    // recycle our Builder object
    VPackBuilder& objectBuilder = context.getBuilder();
    objectBuilder.clear();
    objectBuilder.openObject(true);

    // projections from the index for the filter condition
    context.getFilterProjections().toVelocyPackFromIndex(
        objectBuilder, covering, context.getTrxPtr());

    objectBuilder.close();

    if (!context.checkFilter(objectBuilder.slice())) {
      context.incrFiltered();
      return false;
    }

    if constexpr (!skip) {
      // read the full document from the storage engine only now,
      // after checking the filter condition
      auto cb = makeDocumentCallback(
          [&](LocalDocumentId token, auto v, VPackSlice s) {
            OutputAqlItemRow& output = context.getOutputRow();
            TRI_ASSERT(!output.isFull());

            RegisterId registerId = context.getOutputRegister();
            InputAqlItemRow const& input = context.getInputRow();

            if (context.getProjections().empty()) {
              output.moveValueInto(registerId, input, v);
            } else {
              objectBuilder.clear();
              objectBuilder.openObject(true);

              // projections from the index for the filter condition
              context.getProjections().toVelocyPackFromDocument(
                  objectBuilder, s, context.getTrxPtr());

              objectBuilder.close();

              VPackSlice projectedSlice = objectBuilder.slice();
              output.moveValueInto(registerId, input, projectedSlice);
            }

            TRI_ASSERT(output.produced());
            output.advanceRow();
            return false;
          });
      context.getPhysical().lookup(
          context.getTrxPtr(), token, cb,
          {.readOwnWrites = static_cast<bool>(context.getReadOwnWrites())});
    }

    return true;
  };
}

template IndexIterator::CoveringCallback aql::getCallback<false, false>(
    DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, false>(
    DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<false, true>(
    DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, true>(
    DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
    DocumentProducingFunctionContext& context);

template IndexIterator::CoveringCallback aql::getCallback<false, false>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, false>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<false, true>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, true>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::getCallback<false, false>(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, false>(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<false, true>(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, true>(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::getCallback<false, false>(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, false>(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<false, true>(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::getCallback<true, true>(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context);

template IndexIterator::LocalDocumentIdCallback aql::getNullCallback<false>(
    DocumentProducingFunctionContext& context);
template IndexIterator::LocalDocumentIdCallback aql::getNullCallback<true>(
    DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::buildDocumentCallback<
    false, false>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<
    true, false>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<
    false, true>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<true, true>(
    DocumentProducingFunctionContext& context);
