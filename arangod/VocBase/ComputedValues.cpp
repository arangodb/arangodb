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
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string_view>
#include <unordered_set>

namespace arangodb {

ComputedValues::ComputedValue::ComputedValue(std::string_view name,
                                             std::string_view expression,
                                             bool doOverride)
    : _name(name), _expression(expression), _override(doOverride) {
  // TODO: validate
}

ComputedValues::ComputedValue::~ComputedValue() = default;

void ComputedValues::ComputedValue::toVelocyPack(
    velocypack::Builder& result) const {
  result.openObject();
  result.add("name", VPackValue(_name));
  result.add("expression", VPackValue(_expression));
  result.add("override", VPackValue(_override));
  result.close();
}

ComputedValues::ComputedValues(velocypack::Slice params) {
  Result res = buildDefinitions(params);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

ComputedValues::~ComputedValues() = default;

Result ComputedValues::buildDefinitions(velocypack::Slice params) {
  if (params.isNone() || params.isNull()) {
    return {};
  }

  if (!params.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER, "'computedValues' must be an array"};
  }

  using namespace std::literals::string_literals;

  std::unordered_set<std::string_view> names;
  for (auto it : VPackArrayIterator(params)) {
    VPackSlice name = it.get("name");
    if (!name.isString() || name.getStringLength() == 0) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid attribute name"};
    }

    if (name.stringView() == StaticStrings::IdString ||
        name.stringView() == StaticStrings::RevString ||
        name.stringView() == StaticStrings::KeyString) {
      return {TRI_ERROR_BAD_PARAMETER, "invalid 'computedValues' entry: '"s +
                                           name.copyString() +
                                           "' attribute cannot be computed"};
    }
    // TODO: forbid computedValues on shardKeys!

    VPackSlice expression = it.get("expression");
    if (!expression.isString()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid computation expression"};
    }
    // TODO: validate the actual expression

    VPackSlice doOverride = it.get("override");
    if (!doOverride.isBoolean()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: 'override' must be a boolean"};
    }

    // check for duplicate names in the array
    if (!names.emplace(name.stringView()).second) {
      using namespace std::literals::string_literals;
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: duplicate attribute name '"s +
                  name.copyString() + "'"};
    }

    try {
      _values.emplace_back(ComputedValue{
          name.stringView(), expression.stringView(), doOverride.getBoolean()});
    } catch (std::exception const& ex) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: "s + ex.what()};
    }
  }

  return {};
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

Result ComputedValues::validateDefinitions(velocypack::Slice params) {
  return basics::catchVoidToResult([params]() {
    // will throw upon invalid inputs
    ComputedValues temp(params);
  });
}

}  // namespace arangodb
