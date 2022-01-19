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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "utils/frozen_attributes.hpp"
#include "Aql/Ast.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Containers/SmallVector.h"
#include "StorageEngine/TransactionState.h"
#include "IResearchAnalyzerValueTypeAttribute.h"
#include "IResearchVPackTermAttribute.h"

#include <string>

namespace arangodb {
namespace iresearch {

class AqlAnalyzer final : public irs::analysis::analyzer {
 public:
  struct Options {
    Options() = default;

    Options(std::string&& query, bool collapse, bool keep, uint32_t batch,
            uint32_t limit, AnalyzerValueType retType)
        : queryString(query),
          collapsePositions(collapse),
          keepNull(keep),
          batchSize(batch),
          memoryLimit(limit),
          returnType(retType) {}

    /// @brief Query string to be executed for each document.
    /// Field value is set with @param binded parameter.
    std::string queryString;

    /// @brief determines how processed members of array result:
    /// if set to true all members are considered to be at position 0
    /// if set to false each array members is set at positions serially
    bool collapsePositions{false};

    /// @brief do not emit empty token if query result is NULL
    /// this could be used fo index filtering.
    bool keepNull{true};

    /// @brief  batch size for running query. Set to 10 as most of the cases
    /// we expect just simple query.
    uint32_t batchSize{10};

    /// @brief memory limit for query.  1Mb by default. Could be increased to
    /// 32Mb
    uint32_t memoryLimit{1048576U};

    /// @brief target type to convert query output. Could be
    ///        string, bool, number.
    AnalyzerValueType returnType{AnalyzerValueType::String};
  };

  static constexpr irs::string_ref type_name() noexcept { return "aql"; }

 public:
#ifdef ARANGODB_USE_GOOGLE_TESTS
  bool isOptimized() const;
#endif

  static bool normalize_vpack(const irs::string_ref& args, std::string& out);
  static irs::analysis::analyzer::ptr make_vpack(irs::string_ref const& args);
  static bool normalize_json(const irs::string_ref& args, std::string& out);
  static irs::analysis::analyzer::ptr make_json(irs::string_ref const& args);

  explicit AqlAnalyzer(Options const& options);

  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override final {
    return irs::get_mutable(_attrs, type);
  }

  virtual bool next() override;
  virtual bool reset(irs::string_ref const& field) noexcept override;

 private:
  using ResetImplFunctor = void (*)(AqlAnalyzer* analyzer);

  friend bool tryOptimize(AqlAnalyzer* analyzer);
  friend void resetFromExpression(AqlAnalyzer* analyzer);
  friend void resetFromQuery(AqlAnalyzer* analyzer);

  using attributes = std::tuple<irs::increment, AnalyzerValueTypeAttribute,
                                irs::term_attribute, VPackTermAttribute>;

  Options _options;
  aql::AqlValue _valueBuffer;
  std::unique_ptr<aql::QueryContext> _query;
  containers::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type
      _params_arena;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  aql::AqlItemBlockManager _itemBlockManager;
  aql::ExecutionEngine _engine;
  std::unique_ptr<aql::ExecutionPlan> _plan;

  aql::CalculationNode* _nodeToOptimize{nullptr};
  ResetImplFunctor _resetImpl;
  aql::SharedAqlItemBlockPtr _queryResults;
  std::vector<aql::AstNode*> _bindedNodes;
  aql::ExecutionState _executionState{aql::ExecutionState::DONE};

  aql::RegisterId _engineResultRegister;
  attributes _attrs;
  size_t _resultRowIdx{0};
  uint32_t _nextIncVal{0};
};  // AqlAnalyzer
}  // namespace iresearch
}  // namespace arangodb
