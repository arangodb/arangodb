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
/// @author Jan Steemann
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Aql/JoinExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Collection.h"
#include "Aql/QueryContext.h"
#include "VocBase/LogicalCollection.h"
#include "Logger/LogMacros.h"
#include "DocumentExpressionContext.h"

using namespace arangodb;
using namespace arangodb::aql;

#define LOG_JOIN LOG_DEVEL_IF(false)

JoinExecutor::~JoinExecutor() = default;

JoinExecutor::JoinExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher), _infos(infos), _trx{_infos.query->newTrxContext()} {
  constructStrategy();
  _documents.resize(_infos.indexes.size());
}

namespace {
struct SpanCoveringData : arangodb::IndexIteratorCoveringData {
  explicit SpanCoveringData(std::span<VPackSlice> data) : _data(data) {}
  bool isArray() const noexcept override { return true; }
  VPackSlice at(size_t i) const override {
    TRI_ASSERT(i < _data.size())
        << "accessing index " << i << " but covering data has size "
        << _data.size();
    return _data[i];
  }
  arangodb::velocypack::ValueLength length() const override {
    return _data.size();
  }

  std::span<VPackSlice> _data;
};
}  // namespace

namespace {

VPackSlice extractSlice(VPackSlice slice) { return slice; }

VPackSlice extractSlice(std::unique_ptr<std::string>& ptr) {
  return VPackSlice{reinterpret_cast<uint8_t*>(ptr->data())};
}

void moveValueIntoRegister(OutputAqlItemRow& output, RegisterId reg,
                           InputAqlItemRow& inputRow, VPackSlice slice) {
  output.moveValueInto(reg, inputRow, slice);
}

void moveValueIntoRegister(OutputAqlItemRow& output, RegisterId reg,
                           InputAqlItemRow& inputRow,
                           std::unique_ptr<std::string>& ptr) {
  output.moveValueInto(reg, inputRow, &ptr);
}

template<typename F>
struct DocumentCallbackOverload : F {
  DocumentCallbackOverload(F&& f) : F(std::forward<F>(f)) {}

  bool operator()(LocalDocumentId token, aql::DocumentData&& data,
                  VPackSlice doc) const {
    if (data) {
      return F::template operator()<std::unique_ptr<std::string>&>(token, data);
    }
    return F::template operator()<VPackSlice>(token, doc);
  }

  bool operator()(LocalDocumentId token,
                  std::unique_ptr<std::string>& docPtr) const {
    return F::template operator()<std::unique_ptr<std::string>&>(token, docPtr);
  }
};

}  // namespace

auto JoinExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                               OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  bool hasMore = false;
  while (inputRange.hasDataRow() && !output.isFull()) {
    if (!_currentRow) {
      std::tie(_currentRowState, _currentRow) = inputRange.peekDataRow();
      _strategy->reset();
    }

    std::size_t rowCount = 0;
    hasMore = _strategy->next([&](std::span<LocalDocumentId> docIds,
                                  std::span<VPackSlice> projections) -> bool {
      LOG_JOIN << "BEGIN OF ROW " << rowCount++;

      LOG_JOIN << "PROJECTIONS: ";
      for (auto p : projections) {
        LOG_JOIN << p.toJson();
      }

      auto lookupDocument = [&](std::size_t index, LocalDocumentId id,
                                auto cb) {
        auto result =
            _infos.indexes[index]
                .collection->getCollection()
                ->getPhysical()
                ->lookup(&_trx, id,
                         {DocumentCallbackOverload{
                             [&](LocalDocumentId token, auto docPtr) {
                               cb.template operator()<decltype(docPtr)>(docPtr);
                               return true;
                             }}},
                         {});
        if (result.fail()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              result.errorNumber(),
              basics::StringUtils::concatT(
                  "failed to lookup indexed document ", id.id(),
                  " for collection ", _infos.indexes[index].collection->name(),
                  ": ", result.errorMessage()));
        }
      };

      // first do all the filtering and only if all indexes produced a
      // value write it into the aql output block
      // TODO make a member

      std::size_t projectionsOffset = 0;

      auto buildProjections = [&](size_t k, aql::Projections& proj) {
        // build the document from projections
        std::span<VPackSlice> projectionRange = {
            projections.begin() + projectionsOffset,
            projections.begin() + projectionsOffset + proj.size()};

        auto data = SpanCoveringData{projectionRange};
        _projectionsBuilder.clear();
        _projectionsBuilder.openObject(true);
        proj.toVelocyPackFromIndexCompactArray(_projectionsBuilder, data,
                                               &_trx);
        _projectionsBuilder.close();
      };

      for (std::size_t k = 0; k < docIds.size(); k++) {
        auto& idx = _infos.indexes[k];
        if (idx.projections.usesCoveringIndex(idx.index)) {
          projectionsOffset += idx.projections.size();
        }
        // evaluate filter conditions
        if (!idx.filter.has_value()) {
          continue;
        }

        bool const useFilterProjections =
            idx.filter->projections.usesCoveringIndex();
        bool filtered = false;

        auto filterCallback = [&](auto docPtr) {
          auto doc = extractSlice(docPtr);
          LOG_JOIN << "INDEX " << k << " read document " << doc.toJson();
          GenericDocumentExpressionContext ctx{_trx,
                                               *_infos.query,
                                               _functionsCache,
                                               idx.filter->filterVarsToRegs,
                                               _currentRow,
                                               idx.filter->documentVariable};
          ctx.setCurrentDocument(doc);
          bool mustDestroy;
          AqlValue result = idx.filter->expression->execute(&ctx, mustDestroy);
          AqlValueGuard guard(result, mustDestroy);
          filtered = !result.toBoolean();
          LOG_JOIN << "INDEX " << k << " filter = " << std::boolalpha
                   << filtered;

          if (!filtered && !useFilterProjections) {
            // add document to the list
            _documents[k] = std::make_unique<std::string>(
                doc.template startAs<char>(), doc.byteSize());
          }
        };

        auto filterWithProjectionsCallback =
            [&](std::span<VPackSlice> projections) {
              GenericDocumentExpressionContext ctx{
                  _trx,
                  *_infos.query,
                  _functionsCache,
                  idx.filter->filterVarsToRegs,
                  _currentRow,
                  idx.filter->documentVariable};
              ctx.setCurrentDocument(VPackSlice::noneSlice());

              TRI_ASSERT(idx.filter->projections.size() == projections.size());
              for (size_t j = 0; j < projections.size(); j++) {
                TRI_ASSERT(projections[j].start() != nullptr);
                LOG_JOIN << "INDEX " << k << " set "
                         << idx.filter->filterProjectionVars[j]->id << " = "
                         << projections[j].toJson();
                ctx.setVariable(idx.filter->filterProjectionVars[j],
                                projections[j]);
              }

              bool mustDestroy;
              AqlValue result =
                  idx.filter->expression->execute(&ctx, mustDestroy);
              AqlValueGuard guard(result, mustDestroy);
              filtered = !result.toBoolean();
            };

        if (useFilterProjections) {
          LOG_JOIN << "projectionsOffset = " << projectionsOffset;
          std::span<VPackSlice> projectionRange = {
              projections.begin() + projectionsOffset,
              projections.begin() + projectionsOffset +
                  idx.filter->projections.size()};
          LOG_JOIN << "INDEX " << k << " unsing filter projections";
          filterWithProjectionsCallback(projectionRange);
          projectionsOffset += idx.filter->projections.size();
        } else {
          LOG_JOIN << "INDEX " << k << " looking up document " << docIds[k];
          lookupDocument(k, docIds[k], filterCallback);
        }

        if (filtered) {
          // forget about this row
          LOG_JOIN << "INDEX " << k << " eliminated pair";
          LOG_JOIN << "FILTERED ROW " << (rowCount - 1);
          return true;
        }
      }

      // Now produce the documents
      projectionsOffset = 0;
      for (std::size_t k = 0; k < docIds.size(); k++) {
        auto& idx = _infos.indexes[k];

        auto docProduceCallback = [&](auto docPtr) {
          auto doc = extractSlice(docPtr);
          if (idx.projections.empty()) {
            moveValueIntoRegister(output,
                                  _infos.indexes[k].documentOutputRegister,
                                  _currentRow, docPtr);
          } else {
            _projectionsBuilder.clear();
            _projectionsBuilder.openObject(true);
            idx.projections.toVelocyPackFromDocument(_projectionsBuilder, doc,
                                                     &_trx);
            _projectionsBuilder.close();
            output.moveValueInto(_infos.indexes[k].documentOutputRegister,
                                 _currentRow, _projectionsBuilder.slice());
          }
        };

        if (auto& docPtr = _documents[k]; docPtr) {
          TRI_ASSERT(idx.filter.has_value() &&
                     !idx.filter->projections.usesCoveringIndex());
          docProduceCallback.operator()<std::unique_ptr<std::string>&>(docPtr);
        } else {
          if (idx.projections.usesCoveringIndex(idx.index)) {
            buildProjections(k, idx.projections);
            output.moveValueInto(_infos.indexes[k].documentOutputRegister,
                                 _currentRow, _projectionsBuilder.slice());

            projectionsOffset += idx.projections.size();
          } else {
            lookupDocument(k, docIds[k], docProduceCallback);
          }
        }

        if (idx.filter && idx.filter->projections.usesCoveringIndex()) {
          projectionsOffset += idx.filter->projections.size();
        }
      }

      output.advanceRow();
      LOG_JOIN << "OUTPUT ROW " << (rowCount - 1);
      return !output.isFull();
    });

    if (!hasMore) {
      _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      inputRange.advanceDataRow();
    }
  }

  return {inputRange.upstreamState(), Stats{}, upstreamCall};
}

auto JoinExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                 AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  bool hasMore = false;
  while (inputRange.hasDataRow() && clientCall.needSkipMore()) {
    if (!_currentRow) {
      std::tie(_currentRowState, _currentRow) = inputRange.peekDataRow();
      _strategy->reset();
    }

    hasMore = _strategy->next([&](std::span<LocalDocumentId> docIds,
                                  std::span<VPackSlice> projections) -> bool {
      // TODO post filtering based on projections
      for (std::size_t k = 0; k < docIds.size(); k++) {
        // TODO post filter based on document value
      }

      clientCall.didSkip(1);
      return clientCall.needSkipMore();
    });

    if (!hasMore) {
      _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    }

    inputRange.advanceDataRow();
  }

  return {inputRange.upstreamState(), Stats{}, clientCall.getSkipCount(),
          AqlCall{}};
}

void JoinExecutor::constructStrategy() {
  std::vector<IndexJoinStrategyFactory::Descriptor> indexDescription;

  for (auto const& idx : _infos.indexes) {
    IndexStreamOptions options;
    // TODO right now we only support the first indexed field
    options.usedKeyFields = {0};

    auto& desc = indexDescription.emplace_back();
    desc.isUnique = idx.index->unique();
    desc.numProjections = 0;

    if (idx.projections.usesCoveringIndex()) {
      TRI_ASSERT(!idx.filter.has_value() ||
                 idx.filter->projections.usesCoveringIndex());
      LOG_JOIN << "USING DOCUMENT PROJECTIONS";
      std::transform(idx.projections.projections().begin(),
                     idx.projections.projections().end(),
                     std::back_inserter(options.projectedFields),
                     [&](Projections::Projection const& proj) {
                       return proj.coveringIndexPosition;
                     });

      desc.numProjections += idx.projections.size();
    }
    if (idx.filter && idx.filter->projections.usesCoveringIndex()) {
      LOG_JOIN << "USING FILTER PROJECTIONS";
      std::transform(idx.filter->projections.projections().begin(),
                     idx.filter->projections.projections().end(),
                     std::back_inserter(options.projectedFields),
                     [&](Projections::Projection const& proj) {
                       return proj.coveringIndexPosition;
                     });

      desc.numProjections += idx.filter->projections.size();
    }
    LOG_JOIN << "PROJECTIONS FOR INDEX " << options.projectedFields;
    auto stream = idx.index->streamForCondition(&_trx, options);
    TRI_ASSERT(stream != nullptr);
    desc.iter = std::move(stream);
  }

  // TODO actually we want to have different strategies, like hash join and
  // special implementations for n = 2, 3, ...
  // TODO maybe make this an template parameter
  _strategy =
      IndexJoinStrategyFactory{}.createStrategy(std::move(indexDescription), 1);
}
