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

#include "utils/index_utils.hpp"

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "IResearchCommon.h"
#include "VelocyPackHelper.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "IResearchViewMeta.h"

namespace arangodb {
namespace iresearch {

IResearchViewMeta::Mask::Mask(bool mask /*=false*/) noexcept
    : IResearchDataStoreMeta::Mask(mask),
      _primarySort(mask),
      _storedValues(mask),
      _primarySortCompression(mask) {}

IResearchViewMeta::IResearchViewMeta()
    : _primarySortCompression{getDefaultCompression()} {}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta const& other) {
  storeFull(other);
}

IResearchViewMeta::IResearchViewMeta(FullTag,
                                     IResearchViewMeta&& other) noexcept {
  storeFull(std::move(other));
}

IResearchViewMeta::IResearchViewMeta(PartialTag,
                                     IResearchViewMeta&& other) noexcept {
  storePartial(std::move(other));
}

void IResearchViewMeta::storeFull(IResearchViewMeta const& other) {
  if (this == &other) {
    return;
  }
  _primarySort = other._primarySort;
  _storedValues = other._storedValues;
  _primarySortCompression = other._primarySortCompression;
  IResearchDataStoreMeta::storeFull(other);
}

void IResearchViewMeta::storeFull(IResearchViewMeta&& other) noexcept {
  if (this == &other) {
    return;
  }
  _primarySort = std::move(other._primarySort);
  _storedValues = std::move(other._storedValues);
  _primarySortCompression = other._primarySortCompression;
  IResearchDataStoreMeta::storeFull(std::move(other));
}

bool IResearchViewMeta::operator==(
    IResearchViewMeta const& other) const noexcept {
  if (*static_cast<IResearchDataStoreMeta const*>(this) !=
      static_cast<IResearchDataStoreMeta const&>(other)) {
    return false;
  }
  if (_primarySortCompression != other._primarySortCompression ||
      _primarySort != other._primarySort ||
      _storedValues != other._storedValues) {
    return false;
  }
  return true;
}

bool IResearchViewMeta::operator!=(
    IResearchViewMeta const& other) const noexcept {
  return !(*this == other);
}

/*static*/ const IResearchViewMeta& IResearchViewMeta::DEFAULT() {
  static const IResearchViewMeta meta;

  return meta;
}

bool IResearchViewMeta::init(velocypack::Slice slice, std::string& errorField,
                             IResearchViewMeta const& defaults /*= DEFAULT()*/,
                             Mask* mask /*= nullptr*/) noexcept {
  if (!slice.isObject()) {
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  if (!IResearchDataStoreMeta::init(slice, errorField, defaults, mask)) {
    return false;
  }

  {
    // optional object
    constexpr std::string_view kFieldName{StaticStrings::PrimarySortField};
    std::string errorSubField;

    auto const field = slice.get(kFieldName);
    mask->_primarySort = !field.isNone();

    if (!mask->_primarySort) {
      _primarySort = defaults._primarySort;
    } else if (!_primarySort.fromVelocyPack(field, errorSubField)) {
      errorField = kFieldName;
      if (!errorSubField.empty()) {
        errorField += errorSubField;
      }

      return false;
    }
  }

  {
    // optional object
    constexpr std::string_view kFieldName{StaticStrings::StoredValuesField};
    std::string errorSubField;

    auto const field = slice.get(kFieldName);
    mask->_storedValues = !field.isNone();

    if (!mask->_storedValues) {
      _storedValues = defaults._storedValues;
    } else if (!_storedValues.fromVelocyPack(field, errorSubField)) {
      errorField = kFieldName;
      if (!errorSubField.empty()) {
        errorField += errorSubField;
      }

      return false;
    }
  }
  {
    // optional string (only if primarySort present)
    auto const field = slice.get(StaticStrings::PrimarySortCompressionField);
    mask->_primarySortCompression = !field.isNone();
    if (mask->_primarySortCompression) {
      _primarySortCompression = nullptr;
      if (field.isString()) {
        _primarySortCompression =
            columnCompressionFromString(field.copyString());
      }
      if (ADB_UNLIKELY(nullptr == _primarySortCompression)) {
        errorField += ".primarySortCompression";
        return false;
      }
    }
  }
  return true;
}

bool IResearchViewMeta::json(velocypack::Builder& builder,
                             IResearchViewMeta const* ignoreEqual /*= nullptr*/,
                             Mask const* mask /*= nullptr*/) const {
  if (!IResearchDataStoreMeta::json(builder, ignoreEqual, mask)) {
    return false;
  }

  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _primarySort != ignoreEqual->_primarySort) &&
      (!mask || mask->_primarySort)) {
    velocypack::ArrayBuilder arrayScope(&builder,
                                        StaticStrings::PrimarySortField);
    if (!_primarySort.toVelocyPack(builder)) {
      return false;
    }
  }

  if ((!ignoreEqual || _storedValues != ignoreEqual->_storedValues) &&
      (!mask || mask->_storedValues)) {
    velocypack::ArrayBuilder arrayScope(&builder,
                                        StaticStrings::StoredValuesField);
    if (!_storedValues.toVelocyPack(builder)) {
      return false;
    }
  }

  if ((!ignoreEqual ||
       _primarySortCompression != ignoreEqual->_primarySortCompression) &&
      (!mask || mask->_primarySortCompression)) {
    auto compression = columnCompressionToString(_primarySortCompression);
    addStringRef(builder, StaticStrings::PrimarySortCompressionField,
                 compression);
  }

  return true;
}

size_t IResearchViewMeta::memory() const {
  auto size = sizeof(IResearchViewMeta);

  return size;
}

IResearchViewMetaState::Mask::Mask(bool mask /*=false*/) noexcept
    : _collections(mask) {}

IResearchViewMetaState::IResearchViewMetaState(
    IResearchViewMetaState const& defaults) {
  *this = defaults;
}

IResearchViewMetaState::IResearchViewMetaState(
    IResearchViewMetaState&& other) noexcept {
  *this = std::move(other);
}

IResearchViewMetaState& IResearchViewMetaState::operator=(
    IResearchViewMetaState&& other) noexcept {
  if (this != &other) {
    _collections = std::move(other._collections);
  }

  return *this;
}

IResearchViewMetaState& IResearchViewMetaState::operator=(
    IResearchViewMetaState const& other) {
  if (this != &other) {
    _collections = other._collections;
  }

  return *this;
}

bool IResearchViewMetaState::operator==(
    IResearchViewMetaState const& other) const noexcept {
  if (_collections != other._collections) {
    return false;  // values do not match
  }

  return true;
}

bool IResearchViewMetaState::operator!=(
    IResearchViewMetaState const& other) const noexcept {
  return !(*this == other);
}

bool IResearchViewMetaState::init(VPackSlice slice, std::string& errorField,
                                  Mask* mask /*= nullptr*/) {
  if (!slice.isObject()) {
    errorField = "not an object";
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional uint64 list
    constexpr std::string_view kFieldName{"collections"};

    mask->_collections = slice.hasKey(kFieldName);

    if (!mask->_collections) {
      _collections.clear();
    } else {
      auto field = slice.get(kFieldName);

      if (!field.isArray()) {
        errorField = kFieldName;

        return false;
      }

      _collections.clear();  // reset to match read values exactly

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        decltype(_collections)::key_type value;

        DataSourceId::BaseType tmp;
        if (!getNumber(
                tmp,
                itr.value())) {  // [ <collectionId 1> ... <collectionId N> ]
          errorField = std::string{kFieldName} + "[" +
                       basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }
        value = DataSourceId{tmp};

        _collections.emplace(value);
      }
    }
  }

  return true;
}

bool IResearchViewMetaState::json(
    VPackBuilder& builder,
    IResearchViewMetaState const* ignoreEqual /*= nullptr*/,
    Mask const* mask /*= nullptr*/) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _collections != ignoreEqual->_collections) &&
      (!mask || mask->_collections)) {
    velocypack::Builder subBuilder;

    {
      velocypack::ArrayBuilder subBuilderWrapper(&subBuilder);

      for (auto& cid : _collections) {
        subBuilderWrapper->add(velocypack::Value(cid.id()));
      }
    }

    builder.add("collections", subBuilder.slice());
  }

  return true;
}

size_t IResearchViewMetaState::memory() const {
  auto size = sizeof(IResearchViewMetaState);

  size += sizeof(DataSourceId) * _collections.size();

  return size;
}

}  // namespace iresearch
}  // namespace arangodb
