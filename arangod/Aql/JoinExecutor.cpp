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

using namespace arangodb::aql;

JoinExecutor::~JoinExecutor() = default;

JoinExecutor::JoinExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher), _infos(infos), _trx{_infos.query->newTrxContext()} {
  constructStrategy();
}

namespace {
struct SpanCoveringData : arangodb::IndexIteratorCoveringData {
  explicit SpanCoveringData(std::span<VPackSlice> data) : _data(data) {}
  bool isArray() const noexcept override { return true; }
  VPackSlice at(size_t i) const override { return _data[i]; }
  arangodb::velocypack::ValueLength length() const override {
    return _data.size();
  }

  std::span<VPackSlice> _data;
};
}  // namespace

auto JoinExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                               OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  bool hasMore = false;
  while (inputRange.hasDataRow() && !output.isFull()) {
    if (!_currentRow) {
      std::tie(_currentRowState, _currentRow) = inputRange.peekDataRow();
      _strategy->reset();
    }

    hasMore = _strategy->next([&](std::span<LocalDocumentId> docIds,
                                  std::span<VPackSlice> projections) -> bool {
      // TODO post filtering based on projections
      // TODO store projections in registers
      std::size_t projectionsOffset = 0;
      for (std::size_t k = 0; k < docIds.size(); k++) {
        auto& idx = _infos.indexes[k];

        if (idx.projections.usesCoveringIndex(idx.index)) {
          // build the document from projections
          std::span<VPackSlice> projectionRange = {
              projections.begin() + projectionsOffset,
              projections.begin() + projectionsOffset + idx.projections.size()};

          auto data = SpanCoveringData{projectionRange};
          _projectionsBuilder.clear();
          _projectionsBuilder.openObject(true);
          idx.projections.toVelocyPackFromIndex(_projectionsBuilder, data,
                                                &_trx);
          _projectionsBuilder.close();
          output.moveValueInto(_infos.indexes[k].documentOutputRegister,
                               _currentRow, _projectionsBuilder.slice());

          projectionsOffset += idx.projections.size();
        } else {
          // load the whole document

          // TODO post filter based on document value
          // TODO do we really want to check/populate cache?
          auto result =
              _infos.indexes[k]
                  .collection->getCollection()
                  ->getPhysical()
                  ->lookup(&_trx, docIds[k],
                           {[&](LocalDocumentId token, VPackSlice doc) {
                              if (idx.projections.empty()) {
                                output.moveValueInto(
                                    _infos.indexes[k].documentOutputRegister,
                                    _currentRow, doc);
                              } else {
                                _projectionsBuilder.clear();
                                _projectionsBuilder.openObject(true);
                                idx.projections.toVelocyPackFromDocument(
                                    _projectionsBuilder, doc, &_trx);
                                _projectionsBuilder.close();
                                output.moveValueInto(
                                    _infos.indexes[k].documentOutputRegister,
                                    _currentRow, _projectionsBuilder.slice());
                              }

                              return true;
                            },
                            [&](LocalDocumentId token,
                                std::unique_ptr<std::string>& doc) {
                              if (idx.projections.empty()) {
                                output.moveValueInto(
                                    _infos.indexes[k].documentOutputRegister,
                                    _currentRow, &doc);
                              } else {
                                _projectionsBuilder.clear();
                                _projectionsBuilder.openObject(true);
                                idx.projections.toVelocyPackFromDocument(
                                    _projectionsBuilder,
                                    VPackSlice(reinterpret_cast<uint8_t const*>(
                                        doc->data())),
                                    &_trx);
                                _projectionsBuilder.close();
                                output.moveValueInto(
                                    _infos.indexes[k].documentOutputRegister,
                                    _currentRow, _projectionsBuilder.slice());
                              }
                              return true;
                            }},
                           {});
          if (result.fail()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                result.errorNumber(),
                basics::StringUtils::concatT(
                    "failed to lookup indexed document ", docIds[k].id(),
                    " for collection ", idx.collection->name(), ": ",
                    result.errorMessage()));
          }
        }
      }

      output.advanceRow();
      return !output.isFull();
    });

    if (!hasMore) {
      _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    }

    inputRange.advanceDataRow();
  }

  return {inputRange.upstreamState(), Stats{}, AqlCall{}};
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

    if (idx.projections.usesCoveringIndex()) {
      std::transform(idx.projections.projections().begin(),
                     idx.projections.projections().end(),
                     std::back_inserter(options.projectedFields),
                     [&](Projections::Projection const& proj) {
                       return proj.coveringIndexPosition;
                     });

      desc.numProjections = idx.projections.size();
    }

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
