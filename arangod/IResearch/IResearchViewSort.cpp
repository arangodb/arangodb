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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewSort.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <absl/strings/str_cat.h>

#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"

#include "utils/math_utils.hpp"

namespace arangodb::iresearch {
namespace {

constexpr std::string_view kFieldName = "field";
constexpr std::string_view kAscFieldName = "asc";

}  // namespace

bool IResearchSortBase::toVelocyPack(velocypack::Builder& builder) const {
  if (!builder.isOpenArray()) {
    return false;
  }

  std::string fieldName;
  auto visitor = [&builder, &fieldName](
                     std::vector<basics::AttributeName> const& field,
                     bool direction) {
    fieldName.clear();
    basics::TRI_AttributeNamesToString(field, fieldName, true);

    arangodb::velocypack::ObjectBuilder sortEntryBuilder{&builder};
    builder.add(kFieldName, VPackValue{fieldName});
    builder.add(kAscFieldName, VPackValue{direction});

    return true;
  };

  return visit(visitor);
}

bool IResearchSortBase::fromVelocyPack(velocypack::Slice slice,
                                       std::string& error) {
  constexpr std::string_view kDirectionFieldName = "direction";
  clear();

  if (!slice.isArray()) {
    return false;
  }
  velocypack::ArrayIterator it{slice};
  _fields.reserve(it.size());
  _directions.reserve(it.size());

  for (; it.valid(); ++it) {
    auto sortSlice = *it;
    if (!sortSlice.isObject() || sortSlice.length() != 2) {
      error = absl::StrCat("[", size(), "]");
      return false;
    }

    bool direction;

    auto const directionSlice = sortSlice.get(kDirectionFieldName);
    if (!directionSlice.isNone()) {
      if (!parseDirectionString(directionSlice, direction)) {
        error = absl::StrCat("[", size(), "].", kDirectionFieldName);
        return false;
      }
    } else if (!parseDirectionBool(sortSlice.get(kAscFieldName), direction)) {
      error = absl::StrCat("[", size(), "].", kAscFieldName);
      return false;
    }

    auto const fieldSlice = sortSlice.get(kFieldName);

    if (!fieldSlice.isString()) {
      error = absl::StrCat("[", size(), "].", kFieldName);
      return false;
    }

    std::vector<arangodb::basics::AttributeName> field;

    try {
      basics::TRI_ParseAttributeString(getStringRef(fieldSlice), field, false);
    } catch (...) {
      // FIXME why doesn't 'TRI_ParseAttributeString' return bool?
      error = absl::StrCat("[", size(), "].", kFieldName);
      return false;
    }

    emplace_back(std::move(field), direction);
  }

  return true;
}

size_t IResearchSortBase::memory() const noexcept {
  size_t size = 0;
  for (auto& field : _fields) {
    size += sizeof(basics::AttributeName) * field.size();
    for (auto& entry : field) {
      size += entry.name.size();
    }
  }
  size += irs::math::math_traits<size_t>::div_ceil(_directions.size(), 8);
  return size;
}

}  // namespace arangodb::iresearch
