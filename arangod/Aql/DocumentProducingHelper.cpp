////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/Executor/EnumerateCollectionExecutor.h"
#include "Aql/Executor/IndexExecutor.h"
#include "Aql/LateMaterializedExpressionContext.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Basics/DownCast.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context) {
  return [&context](LocalDocumentId token, aql::DocumentData&& data,
                    VPackSlice slice) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(token)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    if (context.hasFilter() && !context.checkFilter(slice)) {
      context.incrFiltered();
      // required as we point lookup the document to check the filter condition
      // as it is not covered by the index here.
      context.incrLookups();
      return false;
    }

    if constexpr (skip) {
      return true;
    }

    context.incrLookups();

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    TRI_ASSERT(!output.isFull());

    if (context.getProjectionsForRegisters().empty()) {
      // write all projections combined into the global output register
      // recycle our Builder object
      VPackBuilder& objectBuilder = context.getBuilder();
      objectBuilder.clear();
      objectBuilder.openObject(true);
      context.getProjections().toVelocyPackFromDocument(objectBuilder, slice,
                                                        context.getTrxPtr());
      objectBuilder.close();

      VPackSlice s = objectBuilder.slice();
      RegisterId registerId = context.getOutputRegister();
      output.moveValueInto(registerId, input, s);
    } else {
      // write projections into individual output registers
      context.getProjectionsForRegisters().produceFromDocument(
          context.getBuilder(), slice, context.getTrxPtr(),
          [&](Variable const* variable, velocypack::Slice slice) {
            if (slice.isNone()) {
              slice = VPackSlice::nullSlice();
            }
            RegisterId registerId = context.registerForVariable(variable->id);
            TRI_ASSERT(registerId != RegisterId::maxRegisterId);
            output.moveValueInto(registerId, input, slice);
          });
    }

    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
}

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::getCallback(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context) {
  return [&](LocalDocumentId t, aql::DocumentData&& v, velocypack::Slice s) {
    if constexpr (checkUniqueness) {
      if (!context.checkUniqueness(t)) {
        // Document already found, skip it
        return false;
      }
    }

    context.incrScanned();

    if (context.hasFilter() && !context.checkFilter(s)) {
      context.incrFiltered();
      // required as we point lookup the document to check the filter condition
      // as it is not covered by the index here.
      context.incrLookups();
      return false;
    }

    if constexpr (skip) {
      return true;
    }
    context.incrLookups();

    InputAqlItemRow const& input = context.getInputRow();
    OutputAqlItemRow& output = context.getOutputRow();
    RegisterId registerId = context.getOutputRegister();

    TRI_ASSERT(!output.isFull());
    if (v) {
      output.moveValueInto(registerId, input, &v);
    } else {
      output.moveValueInto(registerId, input, s);
    }
    TRI_ASSERT(output.produced());
    output.advanceRow();

    return true;
  };
}

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback aql::buildDocumentCallback(
    DocumentProducingFunctionContext& context) {
  auto const& p = context.getProjections();
  if (!p.empty()) {
    // return a projection
    TRI_ASSERT(!context.getProjections().usesCoveringIndex() ||
               !context.getAllowCoveringIndexOptimization());

    return getCallback<checkUniqueness, skip>(
        DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex{},
        context);
  }
  // return the document as is
  return getCallback<checkUniqueness, skip>(
      DocumentProducingCallbackVariant::DocumentCopy{}, context);
}

template<bool checkUniqueness, bool produceResult>
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
    if constexpr (produceResult) {
      RegisterId registerId = context.getOutputRegister();
      // TODO: optimize this within the register planning mechanism?
      TRI_ASSERT(!output.isFull());
      output.cloneValueInto(registerId, input, AqlValue(AqlValueHintNull()));
      TRI_ASSERT(output.produced());
    } else {
      output.handleEmptyRow(input);
    }
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
      _projectionsForRegisters(_projections),  // do a full copy first
      _resourceMonitor(infos.getResourceMonitor()),
      _numScanned(0),
      _numFiltered(0),
      _numLookups(0),
      _outputVariable(infos.getOutVariable()),
      _outputRegister(infos.getOutputRegisterId()),
      _readOwnWrites(infos.canReadOwnWrites()),
      _checkUniqueness(false),
      _allowCoveringIndexOptimization(false),
      _isLastIndex(false) {
  // now erase all projections for which there is no output register
  _projectionsForRegisters.erase(
      [](Projections::Projection& p) { return p.variable == nullptr; });

  // build ExpressionContext for filtering if we need one
  if (hasFilter()) {
    TRI_ASSERT(_outputVariable != nullptr);
    auto const& filterVars = infos.getFilterVarsToRegister();
    if (filterVars.size() == 1 && filterVars[0].first == _outputVariable->id) {
      // filter condition only refers to the current document, but no other
      // variables. we can get away with building a very simple expression
      // context
      _expressionContext = std::make_unique<SimpleDocumentExpressionContext>(
          _trx, _query, _aqlFunctionsInternalCache, filterVars, _inputRow,
          _outputVariable);
    } else {
      // filter condition refers to additional variables.
      // we have to use a more generic expression context
      _expressionContext = std::make_unique<GenericDocumentExpressionContext>(
          _trx, _query, _aqlFunctionsInternalCache, filterVars, _inputRow,
          _outputVariable);
    }
  }

  if (_expressionContext == nullptr) {
    // we also need an ExpressionContext in case we write projections into
    // output registers
    for (size_t i = 0; i < _projections.size(); ++i) {
      if (_projections[i].variable != nullptr) {
        _expressionContext = std::make_unique<SimpleDocumentExpressionContext>(
            _trx, _query, _aqlFunctionsInternalCache,
            infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
        break;
      }
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
      _projectionsForRegisters(_projections),  // do a full copy first
      _resourceMonitor(infos.getResourceMonitor()),
      _numScanned(0),
      _numFiltered(0),
      _numLookups(0),
      _outputVariable(infos.getOutVariable()),
      _outputRegister(infos.getOutputRegisterId()),
      _readOwnWrites(infos.canReadOwnWrites()),
      _checkUniqueness(infos.getIndexes().size() > 1 ||
                       infos.hasMultipleExpansions()),
      _allowCoveringIndexOptimization(false),  // can be updated later
      _isLastIndex(false) {
  // now erase all projections for which there is no output register
  _projectionsForRegisters.erase(
      [](Projections::Projection& p) { return p.variable == nullptr; });

  // build ExpressionContext for filtering if we need one
  if (hasFilter()) {
    if (infos.isLateMaterialized()) {
      // special handling for late materialization
      _expressionContext = std::make_unique<LateMaterializedExpressionContext>(
          _trx, _query, _aqlFunctionsInternalCache,
          infos.getFilterVarsToRegister(), _inputRow,
          infos.getFilterCoveringVars());
    } else {
      TRI_ASSERT(_outputVariable != nullptr);
      if (_filterProjections.usesCoveringIndex()) {
        _expressionContext =
            std::make_unique<LateMaterializedExpressionContext>(
                _trx, _query, _aqlFunctionsInternalCache,
                infos.getFilterVarsToRegister(), _inputRow,
                infos.getFilterCoveringVars());
      } else {
        auto const& filterVars = infos.getFilterVarsToRegister();
        if (filterVars.size() == 1 &&
            filterVars[0].first == _outputVariable->id) {
          // filter condition only refers to the current document, but no other
          // variables. we can get away with building a very simple expression
          // context
          _expressionContext =
              std::make_unique<SimpleDocumentExpressionContext>(
                  _trx, _query, _aqlFunctionsInternalCache,
                  infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
        } else {
          // filter condition refers to additional variables.
          // we have to use a more generic expression context
          _expressionContext =
              std::make_unique<GenericDocumentExpressionContext>(
                  _trx, _query, _aqlFunctionsInternalCache,
                  infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
        }
      }
    }
  }

  if (_expressionContext == nullptr) {
    // we also need an ExpressionContext in case we write projections into
    // output registers
    for (size_t i = 0; i < _projections.size(); ++i) {
      if (_projections[i].variable != nullptr) {
        _expressionContext = std::make_unique<SimpleDocumentExpressionContext>(
            _trx, _query, _aqlFunctionsInternalCache,
            infos.getFilterVarsToRegister(), _inputRow, _outputVariable);
        break;
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

aql::Projections const& DocumentProducingFunctionContext::getProjections()
    const noexcept {
  return _projections;
}

aql::Projections const& DocumentProducingFunctionContext::getFilterProjections()
    const noexcept {
  return _filterProjections;
}

aql::Projections const&
DocumentProducingFunctionContext::getProjectionsForRegisters() const noexcept {
  return _projectionsForRegisters;
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

RegisterId DocumentProducingFunctionContext::registerForVariable(
    VariableId id) const noexcept {
  TRI_ASSERT(_expressionContext != nullptr);
  return _expressionContext->registerForVariable(id);
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

void DocumentProducingFunctionContext::incrLookups() noexcept { ++_numLookups; }

uint64_t DocumentProducingFunctionContext::getAndResetNumScanned() noexcept {
  return std::exchange(_numScanned, 0);
}

uint64_t DocumentProducingFunctionContext::getAndResetNumFiltered() noexcept {
  return std::exchange(_numFiltered, 0);
}

uint64_t DocumentProducingFunctionContext::getAndResetNumLookups() noexcept {
  return std::exchange(_numLookups, 0);
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
  if (context.hasFilter()) {
    TRI_ASSERT(!context.getFilterProjections().empty());
    TRI_ASSERT(context.getFilterProjections().usesCoveringIndex())
        << "not using covering index " << context.getFilterProjections()
        << " projections are: " << context.getProjections();
  }

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

    if (context.hasFilter() && !context.checkFilter(&covering)) {
      context.incrFiltered();
      return false;
    }

    if constexpr (!skip) {
      OutputAqlItemRow& output = context.getOutputRow();
      InputAqlItemRow const& input = context.getInputRow();

      if (context.getProjectionsForRegisters().empty()) {
        // write all projections combined into the global output register
        // recycle our Builder object
        VPackBuilder& objectBuilder = context.getBuilder();
        objectBuilder.clear();
        objectBuilder.openObject(true);

        // projections from a covering index
        context.getProjections().toVelocyPackFromIndex(objectBuilder, covering,
                                                       context.getTrxPtr());

        objectBuilder.close();

        RegisterId registerId = context.getOutputRegister();
        TRI_ASSERT(!output.isFull());
        VPackSlice s = objectBuilder.slice();
        output.moveValueInto(registerId, input, s);
      } else {
        // write projections into individual output registers
        context.getProjectionsForRegisters().produceFromIndex(
            context.getBuilder(), covering, context.getTrxPtr(),
            [&](Variable const* variable, velocypack::Slice slice) {
              if (slice.isNone()) {
                slice = VPackSlice::nullSlice();
              }
              RegisterId registerId = context.registerForVariable(variable->id);
              TRI_ASSERT(registerId != RegisterId::maxRegisterId);
              output.moveValueInto(registerId, input, slice);
            });
      }

      TRI_ASSERT(output.produced());
      output.advanceRow();
    }

    return true;
  };
}

template<bool checkUniqueness, bool skip, bool produceResult>
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

    // skip and produceResult cannot be true at the same time
    if constexpr (skip) {
      static_assert(!produceResult);
    }

    context.incrScanned();

    if (!context.checkFilter(&covering)) {
      context.incrFiltered();
      return false;
    }

    if constexpr (!skip) {
      if constexpr (produceResult) {
        // read the full document from the storage engine only now,
        // after checking the filter condition
        auto cb = [&](LocalDocumentId token, aql::DocumentData&& v,
                      VPackSlice s) {
          OutputAqlItemRow& output = context.getOutputRow();
          TRI_ASSERT(!output.isFull());

          RegisterId registerId = context.getOutputRegister();
          InputAqlItemRow const& input = context.getInputRow();

          if (context.getProjections().empty()) {
            if (v) {
              output.moveValueInto(registerId, input, &v);
            } else {
              output.moveValueInto(registerId, input, s);
            }
          } else if (context.getProjectionsForRegisters().empty()) {
            // write all projections combined into the global output register
            // recycle our Builder object
            VPackBuilder& objectBuilder = context.getBuilder();
            objectBuilder.clear();
            objectBuilder.openObject(true);

            // projections from the index for the filter condition
            context.getProjections().toVelocyPackFromDocument(
                objectBuilder, s, context.getTrxPtr());

            objectBuilder.close();

            VPackSlice projectedSlice = objectBuilder.slice();
            output.moveValueInto(registerId, input, projectedSlice);
          } else {
            // write projections into individual output registers
            context.getProjectionsForRegisters().produceFromDocument(
                context.getBuilder(), s, context.getTrxPtr(),
                [&](Variable const* variable, velocypack::Slice slice) {
                  if (slice.isNone()) {
                    slice = VPackSlice::nullSlice();
                  }
                  RegisterId registerId =
                      context.registerForVariable(variable->id);
                  TRI_ASSERT(registerId != RegisterId::maxRegisterId);
                  output.moveValueInto(registerId, input, slice);
                });
          }

          TRI_ASSERT(output.produced());
          output.advanceRow();

          return false;
        };
        context.incrLookups();
        context.getPhysical().lookup(
            context.getTrxPtr(), token, cb,
            {.readOwnWrites = static_cast<bool>(context.getReadOwnWrites()),
             .countBytes = true});
      } else {
        OutputAqlItemRow& output = context.getOutputRow();
        InputAqlItemRow const& input = context.getInputRow();
        output.handleEmptyRow(input);
        TRI_ASSERT(output.produced());
        output.advanceRow();
      }
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

// there are only six valid instantiations for WithFilterCoveredByIndex,
// because if skip = true, then we can't have produceResult = true.
template IndexIterator::CoveringCallback aql::getCallback<false, false, false>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, false, false>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<false, true, false>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, true, false>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<false, false, true>(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);
template IndexIterator::CoveringCallback aql::getCallback<true, false, true>(
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

template IndexIterator::LocalDocumentIdCallback
aql::getNullCallback<false, false>(DocumentProducingFunctionContext& context);
template IndexIterator::LocalDocumentIdCallback
aql::getNullCallback<true, false>(DocumentProducingFunctionContext& context);
template IndexIterator::LocalDocumentIdCallback
aql::getNullCallback<false, true>(DocumentProducingFunctionContext& context);
template IndexIterator::LocalDocumentIdCallback
aql::getNullCallback<true, true>(DocumentProducingFunctionContext& context);

template IndexIterator::DocumentCallback aql::buildDocumentCallback<
    false, false>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<
    true, false>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<
    false, true>(DocumentProducingFunctionContext& context);
template IndexIterator::DocumentCallback aql::buildDocumentCallback<true, true>(
    DocumentProducingFunctionContext& context);
