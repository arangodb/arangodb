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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ComputedValues.h"
#include "Aql/StandaloneCalculation.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace {
constexpr std::string_view docParameter("doc");

std::underlying_type<arangodb::ExecuteOn>::type executeOnValue(
    arangodb::ExecuteOn executeOn) {
  return static_cast<std::underlying_type<arangodb::ExecuteOn>::type>(
      executeOn);
}

}  // namespace

namespace arangodb {

ComputedValues::ComputedValue::ComputedValue(std::string_view name,
                                             std::string_view expression,
                                             ExecuteOn executeOn,
                                             bool doOverride)
    : _name(name),
      _expression(expression),
      _executeOn(executeOn),
      _override(doOverride) {}

ComputedValues::ComputedValue::~ComputedValue() = default;

void ComputedValues::ComputedValue::toVelocyPack(
    velocypack::Builder& result) const {
  result.openObject();
  result.add("name", VPackValue(_name));
  result.add("expression", VPackValue(_expression));
  result.add("executeOn", VPackValue(VPackValueType::Array));
  if (::executeOnValue(_executeOn) & ::executeOnValue(ExecuteOn::kInsert)) {
    result.add(VPackValue("insert"));
  }
  if (::executeOnValue(_executeOn) & ::executeOnValue(ExecuteOn::kUpdate)) {
    result.add(VPackValue("update"));
  }
  if (::executeOnValue(_executeOn) & ::executeOnValue(ExecuteOn::kReplace)) {
    result.add(VPackValue("replace"));
  }
  result.close();  // executeOn
  result.add("override", VPackValue(_override));
  result.close();
}

ComputedValues::ComputedValues(TRI_vocbase_t& vocbase,
                               std::vector<std::string> const& shardKeys,
                               velocypack::Slice params)
    : _executeOn(ExecuteOn::kNever) {
  Result res = buildDefinitions(vocbase, shardKeys, params);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(_executeOn != ExecuteOn::kNever);
}

ComputedValues::~ComputedValues() = default;

bool ComputedValues::mustRunOnInsert() const noexcept {
  return mustRunOn(ExecuteOn::kInsert);
}

bool ComputedValues::mustRunOnUpdate() const noexcept {
  return mustRunOn(ExecuteOn::kUpdate);
}

bool ComputedValues::mustRunOnReplace() const noexcept {
  return mustRunOn(ExecuteOn::kReplace);
}

void ComputedValues::computeAttributes(velocypack::Slice input,
                                       ExecuteOn executeOn,
                                       velocypack::Builder& output) const {
  TRI_ASSERT(mustRunOn(executeOn));
}

bool ComputedValues::mustRunOn(ExecuteOn executeOn) const noexcept {
  return ((::executeOnValue(_executeOn) & ::executeOnValue(executeOn)) != 0);
}

Result ComputedValues::buildDefinitions(
    TRI_vocbase_t& vocbase, std::vector<std::string> const& shardKeys,
    velocypack::Slice params) {
  if (params.isNone() || params.isNull()) {
    return {};
  }

  if (!params.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER, "'computedValues' must be an array"};
  }

  using namespace std::literals::string_literals;

  std::unordered_set<std::string_view> names;
  Result res;

  for (auto it : VPackArrayIterator(params)) {
    VPackSlice name = it.get("name");
    if (!name.isString() || name.getStringLength() == 0) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: invalid attribute name");
    }

    std::string_view n = name.stringView();

    if (n == StaticStrings::IdString || n == StaticStrings::RevString ||
        n == StaticStrings::KeyString) {
      return res.reset(TRI_ERROR_BAD_PARAMETER,
                       "invalid 'computedValues' entry: '"s +
                           name.copyString() +
                           "' attribute cannot be computed");
    }

    // forbid computedValues on shardKeys!
    for (auto const& key : shardKeys) {
      if (key == n) {
        return res.reset(TRI_ERROR_BAD_PARAMETER,
                         "invalid 'computedValues' entry: cannot compute "
                         "values of shard key attributes");
      }
    }

    ExecuteOn executeOn = ExecuteOn::kInsert;

    VPackSlice on = it.get("on");
    if (on.isArray()) {
      executeOn = ExecuteOn::kNever;

      for (auto const& onValue : VPackArrayIterator(on)) {
        if (!onValue.isString()) {
          return res.reset(
              TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid on enumeration value");
        }
        std::string_view ov = onValue.stringView();
        if (ov == "insert") {
          executeOn =
              static_cast<ExecuteOn>(::executeOnValue(executeOn) |
                                     ::executeOnValue(ExecuteOn::kInsert));
        } else if (ov == "update") {
          executeOn =
              static_cast<ExecuteOn>(::executeOnValue(executeOn) |
                                     ::executeOnValue(ExecuteOn::kUpdate));
        } else if (ov == "replace") {
          executeOn =
              static_cast<ExecuteOn>(::executeOnValue(executeOn) |
                                     ::executeOnValue(ExecuteOn::kReplace));
        } else {
          return res.reset(
              TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid on value '"s +
                  onValue.copyString() + "'");
        }
      }

      if (executeOn == ExecuteOn::kNever) {
        return res.reset(TRI_ERROR_BAD_PARAMETER,
                         "invalid 'computedValues' entry: empty on value");
      }
    }

    _executeOn = static_cast<ExecuteOn>(::executeOnValue(_executeOn) |
                                        ::executeOnValue(executeOn));

    VPackSlice expression = it.get("expression");
    if (!expression.isString()) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: invalid computation expression");
    }

    // validate the actual expression
    res.reset(aql::StandaloneCalculation::validateQuery(
        vocbase, expression.stringView(), ::docParameter, ""));
    if (res.fail()) {
      std::string errorMsg = "invalid 'computedValues' entry: ";
      errorMsg.append(res.errorMessage()).append(" in computation expression");
      res.reset(TRI_ERROR_BAD_PARAMETER, std::move(errorMsg));
      break;
    }

    VPackSlice doOverride = it.get("override");
    if (!doOverride.isBoolean()) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: 'override' must be a boolean");
    }

    // check for duplicate names in the array
    if (!names.emplace(name.stringView()).second) {
      using namespace std::literals::string_literals;
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: duplicate attribute name '"s +
              name.copyString() + "'");
    }

    try {
      _values.emplace_back(ComputedValue{name.stringView(),
                                         expression.stringView(), executeOn,
                                         doOverride.getBoolean()});
    } catch (std::exception const& ex) {
      return res.reset(TRI_ERROR_BAD_PARAMETER,
                       "invalid 'computedValues' entry: "s + ex.what());
    }
  }

  return res;
}

void ComputedValues::toVelocyPack(velocypack::Builder& result) const {
  if (_values.empty()) {
    result.add(VPackSlice::emptyArraySlice());
  } else {
    result.openArray();
    for (auto const& it : _values) {
      it.toVelocyPack(result);
    }
    result.close();
  }
}

}  // namespace arangodb
