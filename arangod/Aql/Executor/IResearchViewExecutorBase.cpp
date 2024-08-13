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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "IResearch/IResearchFilterFactoryCommon.h"
#include "IResearchViewExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionStats.h"
#include "Aql/MultiGet.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Futures/Try.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchReadUtils.h"
#include "IResearch/Search.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <analysis/token_attributes.hpp>
#include <search/boolean_filter.hpp>
#include <search/cost.hpp>
#include <utility>

// TODO Eliminate access to the plan if possible!
// I think it is used for two things only:
//  - to get the Ast, which can simply be passed on its own, and
//  - to call plan->getVarSetBy() in aql::Expression, which could be removed by
//    passing (a reference to) plan->_varSetBy instead.
// But changing this, even only for the IResearch part, would involve more
// refactoring than I currently find appropriate for this.
#include "Aql/ExecutionPlan.h"

namespace arangodb::aql {

void resetColumn(ColumnIterator& column,
                 irs::doc_iterator::ptr&& itr) noexcept {
  TRI_ASSERT(itr);
  column.itr = std::move(itr);
  column.value = irs::get<irs::payload>(*column.itr);
  if (ADB_UNLIKELY(!column.value)) {
    column.value = &iresearch::NoPayload;
  }
}

void commonReadPK(ColumnIterator& it, irs::doc_id_t docId,
                  LocalDocumentId& documentId) {
  TRI_ASSERT(it.itr);
  TRI_ASSERT(it.value);
  bool r = it.itr->seek(docId) == docId;
  if (ADB_LIKELY(r)) {
    r = iresearch::DocumentPrimaryKey::read(documentId, it.value->value);
    TRI_ASSERT(r == documentId.isSet());
  }
  if (ADB_UNLIKELY(!r)) {
    LOG_TOPIC("6442f", WARN, iresearch::TOPIC)
        << "failed to read document primary key while reading document from "
           "arangosearch view, doc_id: "
        << docId;
  }
}

size_t calculateSkipAllCount(iresearch::CountApproximate approximation,
                             size_t currentPos, irs::doc_iterator* docs) {
  TRI_ASSERT(docs);
  size_t skipped{0};
  switch (approximation) {
    case iresearch::CountApproximate::Cost: {
      auto* cost = irs::get<irs::cost>(*docs);
      if (ADB_LIKELY(cost)) {
        skipped = cost->estimate();
        skipped -= std::min(skipped, currentPos);
        break;
      }
    }
      [[fallthrough]];
    default:
      // check for unknown approximation or absence of the cost attribute.
      // fallback to exact anyway
      TRI_ASSERT(iresearch::CountApproximate::Exact == approximation);
      while (docs->next()) {
        skipped++;
      }
      break;
  }
  return skipped;
}

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    iresearch::ViewSnapshotPtr reader, RegisterId outRegister,
    RegisterId searchDocRegister, std::vector<RegisterId> scoreRegisters,
    QueryContext& query,
#ifdef USE_ENTERPRISE
    iresearch::IResearchOptimizeTopK const& optimizeTopK,
#endif
    std::vector<iresearch::SearchFunc> const& scorers,
    std::pair<iresearch::IResearchSortBase const*, size_t> sort,
    iresearch::IResearchViewStoredValues const& storedValues,
    ExecutionPlan const& plan, Variable const& outVariable,
    AstNode const& filterCondition, std::pair<bool, bool> volatility,
    uint32_t immutableParts, VarInfoMap const& varInfoMap, int depth,
    iresearch::IResearchViewNode::ViewValuesRegisters&&
        outNonMaterializedViewRegs,
    iresearch::CountApproximate countApproximate,
    iresearch::FilterOptimization filterOptimization,
    std::vector<iresearch::HeapSortElement> const& heapSort,
    size_t heapSortLimit, iresearch::SearchMeta const* meta, size_t parallelism,
    iresearch::IResearchExecutionPool& parallelExecutionPool)
    : _searchDocOutReg{searchDocRegister},
      _documentOutReg{outRegister},
      _scoreRegisters{std::move(scoreRegisters)},
      _scoreRegistersCount{_scoreRegisters.size()},
      _reader{std::move(reader)},
      _query{query},
#ifdef USE_ENTERPRISE
      _optimizeTopK{optimizeTopK},
#endif
      _scorers{scorers},
      _sort{std::move(sort)},
      _storedValues{storedValues},
      _plan{plan},
      _outVariable{outVariable},
      _filterCondition{filterCondition},
      _varInfoMap{varInfoMap},
      _outNonMaterializedViewRegs{std::move(outNonMaterializedViewRegs)},
      _countApproximate{countApproximate},
      _filterOptimization{filterOptimization},
      _heapSort{heapSort},
      _heapSortLimit{heapSortLimit},
      _parallelism{parallelism},
      _meta{meta},
      _parallelExecutionPool{parallelExecutionPool},
      _depth{depth},
      _immutableParts{immutableParts},
      _filterConditionIsEmpty{
          iresearch::isFilterConditionEmpty(&_filterCondition) &&
          !_reader->hasNestedFields()},
      _volatileSort{volatility.second},
      // `_volatileSort` implies `_volatileFilter`
      _volatileFilter{_volatileSort || volatility.first} {
  TRI_ASSERT(_reader != nullptr);
}

ExecutionStats& operator+=(ExecutionStats& executionStats,
                           IResearchViewStats const& viewStats) noexcept {
  executionStats.scannedIndex += viewStats.getScanned();
  return executionStats;
}

ScoreIterator::ScoreIterator(std::span<float_t> scoreBuffer, size_t keyIdx,
                             size_t numScores) noexcept
    : _scoreBuffer(scoreBuffer),
      _scoreBaseIdx(keyIdx * numScores),
      _numScores(numScores) {
  TRI_ASSERT(_scoreBaseIdx + _numScores <= _scoreBuffer.size());
}

std::span<float_t>::iterator ScoreIterator::begin() noexcept {
  return _scoreBuffer.begin() + static_cast<ptrdiff_t>(_scoreBaseIdx);
}

std::span<float_t>::iterator ScoreIterator::end() noexcept {
  return _scoreBuffer.begin() +
         static_cast<ptrdiff_t>(_scoreBaseIdx + _numScores);
}

}  // namespace arangodb::aql
