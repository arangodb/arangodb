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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"

#include "utils/math_utils.hpp"

namespace {

bool parseDirectionBool(arangodb::velocypack::Slice slice, bool& direction) {
  if (slice.isBool()) {
    // true - asc
    // false - desc
    direction = slice.getBool();
    return true;
  }

  // unsupported value type
  return false;
}

bool parseDirectionString(arangodb::velocypack::Slice slice, bool& direction) {
  if (slice.isString()) {
    std::string value = arangodb::iresearch::getStringRef(slice);
    arangodb::basics::StringUtils::tolowerInPlace(value);

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

  // unsupported value type
  return false;
}

}

namespace arangodb {
namespace iresearch {

bool IResearchViewSort::toVelocyPack(velocypack::Builder& builder) const {
  if (!builder.isOpenArray()) {
    return false;
  }

  std::string fieldName;
  auto visitor = [&builder, &fieldName](std::vector<basics::AttributeName> const& field, bool direction) {
    fieldName.clear();
    basics::TRI_AttributeNamesToString(field, fieldName, true);

    arangodb::velocypack::ObjectBuilder sortEntryBuilder(&builder);
    builder.add("field", VPackValue(fieldName));
    builder.add("asc", VPackValue(direction));

    return true;
  };

  return visit(visitor);
}

bool IResearchViewSort::fromVelocyPack(
    velocypack::Slice slice,
    std::string& error) {
  static std::string const directionFieldName = "direction";
  static std::string const ascFieldName = "asc";
  static std::string const fieldName = "field";

  clear();

  if (!slice.isArray()) {
    return false;
  }

  _fields.reserve(slice.length());
  _directions.reserve(slice.length());

  for (auto sortSlice : velocypack::ArrayIterator(slice)) {
    if (!sortSlice.isObject() || sortSlice.length() != 2) {
      error = "[" + std::to_string(size()) + "]";
      return false;
    }

    bool direction;

    auto const directionSlice = sortSlice.get(directionFieldName);
    if (!directionSlice.isNone()) {
      if (!parseDirectionString(directionSlice, direction)) {
        error = "[" + std::to_string(size()) + "]." + directionFieldName;
        return false;
      }
    } else if (!parseDirectionBool(sortSlice.get(ascFieldName), direction)) {
      error = "[" + std::to_string(size()) + "]." + ascFieldName;
      return false;
    }

    auto const fieldSlice = sortSlice.get(fieldName);

    if (!fieldSlice.isString()) {
      error = "[" + std::to_string(size()) + "]." + fieldName;
      return false;
    }

    std::vector<arangodb::basics::AttributeName> field;

    try {
      arangodb::basics::TRI_ParseAttributeString(
        arangodb::iresearch::getStringRef(fieldSlice), field,  false
      );
    } catch (...) {
      // FIXME why doesn't 'TRI_ParseAttributeString' return bool?
      error = "[" + std::to_string(size()) + "]." + fieldName;
      return false;
    }

    emplace_back(std::move(field), direction);
  }

  return true;
}

size_t IResearchViewSort::memory() const noexcept {
  size_t size = sizeof(IResearchViewSort);

  for (auto& field : _fields) {
    size += sizeof(basics::AttributeName)*field.size();
    for (auto& entry : field) {
      size += entry.name.size();
    }
  }

  size += irs::math::math_traits<size_t>::div_ceil(_directions.size(), 8);

  return size;
}

} // iresearch
} // arangodb
