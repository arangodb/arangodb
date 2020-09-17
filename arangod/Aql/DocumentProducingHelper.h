////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_DOCUMENT_PRODUCING_HELPER_H
#define ARANGOD_AQL_DOCUMENT_PRODUCING_HELPER_H 1

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include <velocypack/Builder.h>

#include "Aql/types.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Projections.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace transaction {
class Methods;
}
namespace velocypack {
class Builder;
class Slice;
}
namespace aql {
struct AqlValue;
class Expression;
class ExpressionContext;
class InputAqlItemRow;
class OutputAqlItemRow;
class QueryContext;
  
struct DocumentProducingFunctionContext {
 public:
  DocumentProducingFunctionContext(InputAqlItemRow const& inputRow, OutputAqlItemRow* outputRow,
                                   RegisterId outputRegister, bool produceResult,
                                   aql::QueryContext& query,
                                   transaction::Methods& trx, Expression* filter,
                                   arangodb::aql::Projections const& projections,
                                   bool allowCoveringIndexOptimization,
                                   bool checkUniqueness);

  DocumentProducingFunctionContext() = delete;

  ~DocumentProducingFunctionContext() = default;

  void setOutputRow(OutputAqlItemRow* outputRow);

  bool getProduceResult() const noexcept;

  arangodb::aql::Projections const& getProjections() const noexcept;

  transaction::Methods* getTrxPtr() const noexcept;

  std::vector<size_t> const& getCoveringIndexAttributePositions() const noexcept;

  bool getAllowCoveringIndexOptimization() const noexcept;

  void setAllowCoveringIndexOptimization(bool allowCoveringIndexOptimization) noexcept;

  void incrScanned() noexcept;

  void incrFiltered() noexcept;

  size_t getAndResetNumScanned() noexcept;
  
  size_t getAndResetNumFiltered() noexcept;
  
  InputAqlItemRow const& getInputRow() const noexcept;

  OutputAqlItemRow& getOutputRow() const noexcept;

  RegisterId getOutputRegister() const noexcept;

  bool checkUniqueness(LocalDocumentId const& token);

  bool checkFilter(velocypack::Slice slice);

  bool checkFilter(AqlValue (*getValue)(void const* ctx, Variable const* var, bool doCopy),
                   void const* filterContext);

  void reset();

  void setIsLastIndex(bool val);
  
  bool hasFilter() const noexcept;
  
  aql::AqlFunctionsInternalCache& aqlFunctionsInternalCache() { return _aqlFunctionsInternalCache; }

  arangodb::velocypack::Builder& getBuilder() noexcept;

 private:
  bool checkFilter(ExpressionContext& ctx);

  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  InputAqlItemRow const& _inputRow;
  OutputAqlItemRow* _outputRow;
  aql::QueryContext& _query;
  transaction::Methods& _trx;
  Expression* _filter;
  arangodb::aql::Projections const& _projections;
  size_t _numScanned;
  size_t _numFiltered;

  /// @brief Builder that is reused to generate projection results 
  arangodb::velocypack::Builder _objectBuilder;

  /// @brief set of already returned documents. Used to make the result distinct
  std::unordered_set<LocalDocumentId> _alreadyReturned;

  RegisterId const _outputRegister;
  bool const _produceResult;
  bool _allowCoveringIndexOptimization;
  /// @brief Flag if the current index pointer is the last of the list.
  ///        Used in uniqueness checks.
  bool _isLastIndex;

  /// @brief Flag if we need to check for uniqueness
  bool _checkUniqueness;
};

namespace DocumentProducingCallbackVariant {
struct WithProjectionsCoveredByIndex {};
struct WithProjectionsNotCoveredByIndex {};
struct DocumentCopy {};
}  // namespace DocumentProducingCallbackVariant

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback getCallback(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
                                            DocumentProducingFunctionContext& context);

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback getCallback(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
                                            DocumentProducingFunctionContext& context);

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback getCallback(DocumentProducingCallbackVariant::DocumentCopy,
                                            DocumentProducingFunctionContext& context);

template <bool checkUniqueness>
IndexIterator::LocalDocumentIdCallback getNullCallback(DocumentProducingFunctionContext& context);

template <bool checkUniqueness, bool skip>
IndexIterator::DocumentCallback buildDocumentCallback(DocumentProducingFunctionContext& context);

}  // namespace aql
}  // namespace arangodb

#endif
