////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include <velocypack/Builder.h>

#include "Aql/types.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Projections.h"
#include "Containers/FlatHashSet.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace transaction {
class Methods;
}
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
class PhysicalCollection;
enum class ReadOwnWrites : bool;
namespace aql {
struct AqlValue;
class DocumentProducingExpressionContext;
class EnumerateCollectionExecutorInfos;
class Expression;
class ExpressionContext;
class IndexExecutorInfos;
class InputAqlItemRow;
class OutputAqlItemRow;
class QueryContext;
struct Variable;

struct DocumentProducingFunctionContext {
 public:
  // constructor called from EnumerateCollectionExecutor
  DocumentProducingFunctionContext(transaction::Methods& trx,
                                   InputAqlItemRow const& inputRow,
                                   EnumerateCollectionExecutorInfos& infos);

  // constructor called from IndexExecutor
  DocumentProducingFunctionContext(transaction::Methods& trx,
                                   InputAqlItemRow const& inputRow,
                                   IndexExecutorInfos& infos);

  ~DocumentProducingFunctionContext();

  void setOutputRow(OutputAqlItemRow* outputRow);

  bool getProduceResult() const noexcept;

  arangodb::aql::Projections const& getProjections() const noexcept;

  arangodb::aql::Projections const& getFilterProjections() const noexcept;

  transaction::Methods* getTrxPtr() const noexcept;

  PhysicalCollection& getPhysical() const noexcept;

  bool getAllowCoveringIndexOptimization() const noexcept;

  void setAllowCoveringIndexOptimization(
      bool allowCoveringIndexOptimization) noexcept;

  void incrScanned() noexcept;

  void incrFiltered() noexcept;

  [[nodiscard]] uint64_t getAndResetNumScanned() noexcept;

  [[nodiscard]] uint64_t getAndResetNumFiltered() noexcept;

  InputAqlItemRow const& getInputRow() const noexcept;

  OutputAqlItemRow& getOutputRow() const noexcept;

  RegisterId getOutputRegister() const noexcept;

  ReadOwnWrites getReadOwnWrites() const noexcept;

  bool checkUniqueness(LocalDocumentId const& token);

  // called for documents and indexes
  bool checkFilter(velocypack::Slice slice);

  // called only for late materialization
  bool checkFilter(IndexIteratorCoveringData const* covering);

  void reset();

  void setIsLastIndex(bool val);

  bool hasFilter() const noexcept;

  aql::AqlFunctionsInternalCache& aqlFunctionsInternalCache() noexcept {
    return _aqlFunctionsInternalCache;
  }

  arangodb::velocypack::Builder& getBuilder() noexcept;

 private:
  bool checkFilter(DocumentProducingExpressionContext& ctx);

  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  InputAqlItemRow const& _inputRow;
  OutputAqlItemRow* _outputRow;
  aql::QueryContext& _query;
  transaction::Methods& _trx;
  PhysicalCollection& _physical;
  Expression* _filter;
  arangodb::aql::Projections const& _projections;
  arangodb::aql::Projections const& _filterProjections;
  uint64_t _numScanned;
  uint64_t _numFiltered;

  std::unique_ptr<DocumentProducingExpressionContext> _expressionContext;

  /// @brief Builder that is reused to generate projection results
  arangodb::velocypack::Builder _objectBuilder;

  /// @brief set of already returned documents. Used to make the result distinct
  containers::FlatHashSet<LocalDocumentId> _alreadyReturned;

  RegisterId const _outputRegister;
  Variable const* _outputVariable;

  ReadOwnWrites const _readOwnWrites;

  // note: it is fine if this counter overflows
  uint_fast16_t _killCheckCounter = 0;

  /// @brief Flag if we need to check for uniqueness
  bool const _checkUniqueness;

  bool const _produceResult;
  bool _allowCoveringIndexOptimization;
  /// @brief Flag if the current index pointer is the last of the list.
  ///        Used in uniqueness checks.
  bool _isLastIndex;
};

namespace DocumentProducingCallbackVariant {
struct WithProjectionsCoveredByIndex {};
struct WithFilterCoveredByIndex {};
struct WithProjectionsNotCoveredByIndex {};
struct DocumentCopy {};
}  // namespace DocumentProducingCallbackVariant

template<bool checkUniqueness, bool skip>
IndexIterator::CoveringCallback getCallback(
    DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
    DocumentProducingFunctionContext& context);

template<bool checkUniqueness, bool skip>
IndexIterator::CoveringCallback getCallback(
    DocumentProducingCallbackVariant::WithFilterCoveredByIndex,
    DocumentProducingFunctionContext& context);

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback getCallback(
    DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
    DocumentProducingFunctionContext& context);

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback getCallback(
    DocumentProducingCallbackVariant::DocumentCopy,
    DocumentProducingFunctionContext& context);

template<bool checkUniqueness>
IndexIterator::LocalDocumentIdCallback getNullCallback(
    DocumentProducingFunctionContext& context);

template<bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback buildDocumentCallback(
    DocumentProducingFunctionContext& context);

}  // namespace aql
}  // namespace arangodb
