////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {

using DocumentProducingFunction =
    std::function<void(LocalDocumentId const&, VPackSlice slice)>;

void handleProjections(std::vector<std::string> const& projections,
                       transaction::Methods const* trxPtr, VPackSlice slice,
                       VPackBuilder& b, bool useRawDocumentPointers);

struct DocumentProducingFunctionContext {
 public:
  DocumentProducingFunctionContext(InputAqlItemRow const& inputRow, OutputAqlItemRow* outputRow,
                                   RegisterId outputRegister, bool produceResult,
                                   std::vector<std::string> const& projections,
                                   transaction::Methods* trxPtr,
                                   std::vector<size_t> const& coveringIndexAttributePositions,
                                   bool allowCoveringIndexOptimization,
                                   bool useRawDocumentPointers, bool checkUniqueness);

  DocumentProducingFunctionContext() = delete;

  ~DocumentProducingFunctionContext() = default;

  void setOutputRow(OutputAqlItemRow* outputRow);

  bool getProduceResult() const noexcept;

  std::vector<std::string> const& getProjections() const noexcept;

  transaction::Methods* getTrxPtr() const noexcept;

  std::vector<size_t> const& getCoveringIndexAttributePositions() const noexcept;

  bool getAllowCoveringIndexOptimization() const noexcept;

  bool getUseRawDocumentPointers() const noexcept;

  void setAllowCoveringIndexOptimization(bool allowCoveringIndexOptimization) noexcept;

  void incrScanned() noexcept;

  size_t getAndResetNumScanned() noexcept;

  InputAqlItemRow const& getInputRow() const noexcept;

  OutputAqlItemRow& getOutputRow() const noexcept;

  RegisterId getOutputRegister() const noexcept;

  bool checkUniqueness(LocalDocumentId const& token);

  void reset();

  void setIsLastIndex(bool val);

 private:
  InputAqlItemRow const& _inputRow;
  OutputAqlItemRow* _outputRow;
  transaction::Methods* const _trxPtr;
  std::vector<std::string> const& _projections;
  std::vector<size_t> const& _coveringIndexAttributePositions;
  size_t _numScanned;

  /// @brief set of already returned documents. Used to make the result distinct
  std::unordered_set<TRI_voc_rid_t> _alreadyReturned;

  RegisterId const _outputRegister;
  bool const _produceResult;
  bool const _useRawDocumentPointers;
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
struct DocumentWithRawPointer {};
struct DocumentCopy {};
}  // namespace DocumentProducingCallbackVariant

template <bool checkUniqueness>
DocumentProducingFunction getCallback(DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex,
                                      DocumentProducingFunctionContext& context);

template <bool checkUniqueness>
DocumentProducingFunction getCallback(DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex,
                                      DocumentProducingFunctionContext& context);

template <bool checkUniqueness>
DocumentProducingFunction getCallback(DocumentProducingCallbackVariant::DocumentWithRawPointer,
                                      DocumentProducingFunctionContext& context);

template <bool checkUniqueness>
DocumentProducingFunction getCallback(DocumentProducingCallbackVariant::DocumentCopy,
                                      DocumentProducingFunctionContext& context);

template <bool checkUniqueness>
std::function<void(LocalDocumentId const& token)> getNullCallback(DocumentProducingFunctionContext& context);

template <bool checkUniqueness>
DocumentProducingFunction buildCallback(DocumentProducingFunctionContext& context);

}  // namespace aql
}  // namespace arangodb

#endif
