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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGOD_IRESEARCH__IRESEARCH_CALCULATION_ANALYZER
#define ARANGOD_IRESEARCH__IRESEARCH_CALCULATION_ANALYZER 1

#include "RestServer/DatabaseFeature.h"
#include "analysis/token_attributes.hpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <string>

namespace arangodb {
namespace iresearch {

class CalculationQueryContext;

class CalculationAnalyzer final : public irs::analysis::analyzer{
 public:
  struct options_t {
    options_t() = default;

    options_t(std::string&& query, bool collapse, bool keep)
      : queryString(query), collapseArrayPositions(collapse),
      keepNull(keep) {}

    /// @brief Query string to be executed for each document.
    /// Field value is set with @param binded parameter.
    std::string queryString;

    /// @brief determines how processed members of array result:
    /// if set to true all members are considered to be at position 0
    /// if set to false each array members is set at positions serially
    bool collapseArrayPositions{ false };

    /// @brief do not emit empty token if query result is NULL
    /// this could be used fo index filtering.
    bool keepNull{ true };
  };

  static bool parse_options(const irs::string_ref& args, options_t& options);

  static constexpr irs::string_ref type_name() noexcept {
    return "calculation";
  }

  static bool normalize(const irs::string_ref& args, std::string& out);
  static irs::analysis::analyzer::ptr make(irs::string_ref const& args);

  CalculationAnalyzer(options_t const& options);

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::increment>::id()) {
      return &_inc;
    }

    return type == irs::type<irs::term_attribute>::id()
      ? &_term : nullptr;
  }

  virtual bool next() override;
  virtual bool reset(irs::string_ref const& field) noexcept override;

  static void initCalculationContext(arangodb::application_features::ApplicationServer& server);

  static void shutdownCalculationContext();
 private:
  irs::term_attribute _term;
  irs::increment _inc;
  bool _has_data{ false };
  std::string _str;
  options_t _options;

  std::unique_ptr<arangodb::aql::Ast> _ast;
  std::unique_ptr<arangodb::aql::ExecutionEngine> _engine;
  std::unique_ptr<CalculationQueryContext> _query;
  std::unique_ptr<arangodb::aql::ExecutionPlan> _plan;
  arangodb::aql::SharedAqlItemBlockPtr _queryResults;
  /// @brief Artificial vocbase for executing calculation queries
  static std::unique_ptr<TRI_vocbase_t> CalculationAnalyzer::_calculationVocbase;
}; // CalculationAnalyzer
}
}

#endif // ARANGOD_IRESEARCH__IRESEARCH_CALCULATION_ANALYZER
