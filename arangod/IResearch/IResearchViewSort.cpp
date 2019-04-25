////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewSort.h"
#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

namespace {

bool parseDirection(arangodb::velocypack::Slice slice, bool& direction) {
  if (slice.isString()) {
    std::string value = arangodb::iresearch::getStringRef(slice);
    arangodb::basics::StringUtils::tolowerInPlace(&value);

    if (value == "asc") {
      direction = true;
      return true;
    }

    if (value == "desc") {
      direction = false;
      return true;
    }

    return false;
  }

  if (slice.isBool()) {
    // true - asc
    // false - desc
    direction = slice.getBool();
    return true;
  }

  // unsupported value type
  return false;
}

}

namespace arangodb {
namespace iresearch {

void IResearchViewSort::toVelocyPack(velocypack::Builder& builder) const {
  arangodb::velocypack::ObjectBuilder sortBuilder(&builder, "sort");
  arangodb::velocypack::ArrayBuilder primarySortBulder(&builder, "primary");

  std::string fieldName;
  auto visitor = [&builder, &fieldName](std::vector<basics::AttributeName> const& field, bool direction) {
    fieldName.clear();
    basics::TRI_AttributeNamesToString(field, fieldName, true);

    arangodb::velocypack::ObjectBuilder sortEntryBuilder(&builder);
    builder.add("field", VPackValue(fieldName));
    builder.add("direction", VPackValue(direction));

    return true;
  };

  visit(visitor);
}

bool IResearchViewSort::fromVelocyPack(
    velocypack::Slice slice,
    std::string& error) {
  static std::string const sortFieldName = "sort";
  static std::string const primarySortFieldName = "primary";
  static std::string const directionFieldName = "direction";
  static std::string const fieldName = "field";

  if (!slice.isObject()) {
    error = sortFieldName;
    return false;
  }

  slice = slice.get(primarySortFieldName);

  if (!slice.isArray()) {
    error = sortFieldName + "=>" + primarySortFieldName;
    return false;
  }

  clear();
  reserve(slice.length());

  for (auto sortSlice : velocypack::ArrayIterator(slice)) {
    if (!sortSlice.isObject()) {
      error = sortFieldName + "=>" + primarySortFieldName + "[" + std::to_string(size()) + "]";
      return false;
    }

    bool direction;

    if (!parseDirection(sortSlice.get(directionFieldName), direction)) {
      error = sortFieldName + "=>" + primarySortFieldName + "[" + std::to_string(size()) + "]=>" + directionFieldName;
      return false;
    }

    auto const fieldSlice = sortSlice.get(fieldName);

    if (!fieldSlice.isString()) {
      error = sortFieldName + "=>" + primarySortFieldName + "[" + std::to_string(size()) + "]=>" + fieldName;
      return false;
    }

    std::vector<arangodb::basics::AttributeName> field;

    try {
      arangodb::basics::TRI_ParseAttributeString(
        arangodb::iresearch::getStringRef(fieldSlice), field,  false
      );
    } catch (...) {
      // FIXME why doesn't 'TRI_ParseAttributeString' return bool?
      error = sortFieldName + "=>" + primarySortFieldName + "[" + std::to_string(size()) + "]=>" + fieldName;
      return false;
    }

    emplace_back(std::move(field), direction);
  }

  return true;
}

} // iresearch
} // arangodb
