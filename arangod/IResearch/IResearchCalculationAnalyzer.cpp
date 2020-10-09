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

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "analysis/analyzers.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExpressionContext.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/FunctionUtils.h"
#include "IResearchCommon.h"
#include "IResearchCalculationAnalyzer.h"
#include "Logger/LogMacros.h"
#include "Transaction/StandaloneContext.h"
#include "VelocyPackHelper.h"
#include "VocBase/vocbase.h"

#include <Containers/HashSet.h>
#include "VPackDeserializer/deserializer.h"

namespace {
using namespace arangodb::velocypack::deserializer;

constexpr const char QUERY_STRING_PARAM_NAME[] = "queryString";
constexpr const char COLLAPSE_ARRAY_POSITIONS_PARAM_NAME[] = "collapseArrayPos";
constexpr const char DISCARD_NULLS_PARAM_NAME[] = "discardNulls";
using Options = arangodb::iresearch::CalculationAnalyzer::options_t;

struct OptionsValidator {
  std::optional<deserialize_error> operator()(Options const& opts) const {
    if (opts.queryString.empty()) {
      return deserialize_error{std::string("Value of '").append(QUERY_STRING_PARAM_NAME).append("' should be non empty string")};
    }
    return {};
  }
};


using non_empty_string = validate<values::value_deserializer<std::string>, utilities::not_empty_validator>;

using OptionsDeserializer = utilities::constructing_deserializer<Options, parameter_list<
  factory_deserialized_parameter<QUERY_STRING_PARAM_NAME, values::value_deserializer<std::string>, true>,
  factory_simple_parameter<COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, bool, false, values::numeric_value<bool, false>>,
  factory_simple_parameter<DISCARD_NULLS_PARAM_NAME, bool, false, values::numeric_value<bool, true>>
  >>;

using ValidatingOptionsDeserializer = validate<OptionsDeserializer, OptionsValidator>;

}

namespace arangodb {
namespace iresearch {

arangodb::DatabaseFeature* CalculationAnalyzer::DB_FEATURE;

bool CalculationAnalyzer::parse_options(const irs::string_ref& args, options_t& options) {
  auto const slice = arangodb::iresearch::slice(args);
  auto const res = deserialize<ValidatingOptionsDeserializer>(slice);
  if (!res.ok()) {
    LOG_TOPIC("4349c", WARN, arangodb::iresearch::TOPIC) <<
      "Failed to deserialize options from JSON while constructing '"
      << type_name() << "' analyzer, error: '"
      << res.error().message << "'";
    return false;
  }
  options = res.get();
  return true;
}

/*static*/ bool CalculationAnalyzer::normalize(const irs::string_ref& args, std::string& out) {
  options_t options;
  if (parse_options(args, options)) {
    VPackBuilder builder;
    {
      VPackObjectBuilder root(&builder);
      builder.add(QUERY_STRING_PARAM_NAME, VPackValue(options.queryString));
      builder.add(COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, VPackValue(options.collapseArrayPositions));
      builder.add(DISCARD_NULLS_PARAM_NAME, VPackValue(options.discardNulls));
    }
    out.resize(builder.slice().byteSize());
    std::memcpy(&out[0], builder.slice().begin(), out.size());
    return true;
  }
  return false;
}

/*static*/ irs::analysis::analyzer::ptr CalculationAnalyzer::make(irs::string_ref const& args) {
  options_t options;
  if (parse_options(args, options)) {
    return std::make_shared<CalculationAnalyzer>(options);
  }
  return nullptr;
}

inline bool CalculationAnalyzer::next() {
  if (_queryResult.ok() && _has_data) {
    _has_data = false;
    if (_queryResult.data->slice().isArray()) {
      auto value = _queryResult.data->slice().at(0);
      _str = value.copyString(); // TODO: maybe just process all with copyBinary?
      _term.value = irs::ref_cast<irs::byte_type>(irs::string_ref(_str));
      return true;
    }
    else { // TODO: remove me!!!
      _str = _queryResult.data->slice().typeName();
      _term.value = irs::ref_cast<irs::byte_type>(irs::string_ref(_str));
      return true;
    }
  }
  return false;
}

inline bool CalculationAnalyzer::reset(irs::string_ref const& field) noexcept {
  // FIXME: For now it is only strings. So make VPACK and set parameter
  // FIXME: use VPAck buffer and never reallocate between queries
  auto vPackArgs = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackObjectBuilder o(vPackArgs.get());
    vPackArgs->add("field", VPackValue(field));
  }
  // TODO: check query somehow
  // TODO: Forbid to use TOKENS DOCUMENT FULLTEXT ,V8 related inside query -> problems on recovery as analyzers are not available for querying!
  // TODO: Position calculation as parameter
  // TODO: Filtering results
  // TODO: SetQuery quit option

  _query = std::make_unique<arangodb::aql::Query>(
    std::make_shared<arangodb::transaction::StandaloneContext>(DB_FEATURE->getExpressionVocbase()),
    arangodb::aql::QueryString(_options.queryString),
    vPackArgs, nullptr);
  _query->prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);
  _query->execute(_queryResult);
  _has_data = true;
  return _queryResult.ok();
}
} // iresearch
} // arangodb
