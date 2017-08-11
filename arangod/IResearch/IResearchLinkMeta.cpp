//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <unordered_set>

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/hash_utils.hpp"
#include "utils/locale_utils.hpp"

#include "ApplicationServerHelper.h"
#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

#include "IResearchLinkMeta.h"

NS_LOCAL

bool equalTokenizers(
  arangodb::iresearch::IResearchLinkMeta::Tokenizers const& lhs,
  arangodb::iresearch::IResearchLinkMeta::Tokenizers const& rhs
) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  std::unordered_multiset<irs::string_ref> expected;

  for (auto& entry: lhs) {
    expected.emplace(entry ? irs::string_ref(entry->name()) : irs::string_ref::nil);
  }

  for (auto& entry: rhs) {
    auto itr = expected.find(entry ? irs::string_ref(entry->name()) : irs::string_ref::nil);

    if (itr == expected.end()) {
      return false; // values do not match
    }

    expected.erase(itr); // ensure same count of duplicates
  }

  return true;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchLinkMeta::Mask::Mask(bool mask /*= false*/) noexcept
  : _boost(mask),
    _fields(mask),
    _includeAllFields(mask),
    _nestListValues(mask),
    _tokenizers(mask) {
}

IResearchLinkMeta::IResearchLinkMeta()
  : _boost(1.0), // no boosting of field preference in view ordering
    //_fields(<empty>), // empty map to match all encounteredfields
    _includeAllFields(false), // true to match all encountered fields, false match only fields in '_fields'
    _nestListValues(false) { // treat '_nestListValues' as SQL-IN
  auto analyzer = IResearchAnalyzerFeature::identity();

  // identity-only tokenization
  if (analyzer) {
    _tokenizers.emplace_back(analyzer);
  }
}

IResearchLinkMeta::IResearchLinkMeta(IResearchLinkMeta const& other) {
  *this = other;
}

IResearchLinkMeta::IResearchLinkMeta(IResearchLinkMeta&& other) noexcept {
  *this = std::move(other);
}

IResearchLinkMeta& IResearchLinkMeta::operator=(IResearchLinkMeta&& other) noexcept {
  if (this != &other) {
    _boost = std::move(other._boost);
    _fields = std::move(other._fields);
    _includeAllFields = std::move(other._includeAllFields);
    _nestListValues = std::move(other._nestListValues);
    _tokenizers = std::move(other._tokenizers);
  }

  return *this;
}

IResearchLinkMeta& IResearchLinkMeta::operator=(IResearchLinkMeta const& other) {
  if (this != &other) {
    _boost = other._boost;
    _fields = other._fields;
    _includeAllFields = other._includeAllFields;
    _nestListValues = other._nestListValues;
    _tokenizers = other._tokenizers;
  }

  return *this;
}

bool IResearchLinkMeta::operator==(
  IResearchLinkMeta const& other
) const noexcept {
  if (_boost != other._boost) {
    return false; // values do not match
  }

  if (_fields.size() != other._fields.size()) {
    return false; // values do not match
  }

  auto itr = other._fields.begin();

  for (auto& entry: _fields) {
    if (itr.key() != entry.key() || itr.value() != entry.value()) {
      return false; // values do not match
    }

    ++itr;
  }

  if (_includeAllFields != other._includeAllFields) {
    return false; // values do not match
  }

  if (_nestListValues != other._nestListValues) {
    return false; // values do not match
  }

  if (!equalTokenizers(_tokenizers, other._tokenizers)) {
    return false; // values do not match
  }

  return true;
}

bool IResearchLinkMeta::operator!=(
  IResearchLinkMeta const& other
) const noexcept {
  return !(*this == other);
}

/*static*/ const IResearchLinkMeta& IResearchLinkMeta::DEFAULT() {
  static const IResearchLinkMeta meta;

  return meta;
}

bool IResearchLinkMeta::init(
  arangodb::velocypack::Slice const& slice,
  std::string& errorField,
  IResearchLinkMeta const& defaults /*= DEFAULT()*/,
  Mask* mask /*= nullptr*/
) noexcept {
  if (!slice.isObject()) {
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional floating point number
    static const std::string fieldName("boost");

    if (!getNumber(_boost, slice, fieldName, mask->_boost, defaults._boost)) {
      errorField = fieldName;

      return false;
    }
  }

  {
    // optional bool
    static const std::string fieldName("includeAllFields");

    mask->_includeAllFields = slice.hasKey(fieldName);

    if (!mask->_includeAllFields) {
      _includeAllFields = defaults._includeAllFields;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isBool()) {
        errorField = fieldName;

        return false;
      }

      _includeAllFields = field.getBool();
    }
  }

  {
    // optional bool
    static const std::string fieldName("nestListValues");

    mask->_nestListValues = slice.hasKey(fieldName);

    if (!mask->_nestListValues) {
      _nestListValues = defaults._nestListValues;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isBool()) {
        errorField = fieldName;

        return false;
      }

      _nestListValues = field.getBool();
    }
  }

  {
    // optional string list
    static const std::string fieldName("tokenizers");

    mask->_tokenizers = slice.hasKey(fieldName);

    if (!mask->_tokenizers) {
      _tokenizers = defaults._tokenizers;
    } else {
      auto* analyzers = getFeature<IResearchAnalyzerFeature>();
      auto field = slice.get(fieldName);

      if (!analyzers || !field.isArray()) {
        errorField = fieldName;

        return false;
      }

      _tokenizers.clear(); // reset to match read values exactly

      for (arangodb::velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto key = *itr;

        if (!key.isString()) {
          errorField = fieldName + "=>[" + arangodb::basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }

        auto name = getStringRef(key);
        auto analyzer = analyzers->ensure(name);

        if (!analyzer) {
          errorField = fieldName + "=>" + std::string(name);

          return false;
        }

        // inserting two identical values for name is a poor-man's boost multiplier
        _tokenizers.emplace_back(analyzer);
      }
    }
  }

  // .............................................................................
  // process fields last since children inherit from parent
  // .............................................................................

  {
    // optional string map<name, overrides>
    static const std::string fieldName("fields");

    mask->_fields = slice.hasKey(fieldName);

    if (!mask->_fields) {
      _fields = defaults._fields;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isObject()) {
        errorField = fieldName;

        return false;
      }

      auto subDefaults = *this;

      subDefaults._fields.clear(); // do not inherit fields and overrides from this field
      _fields.clear(); // reset to match either defaults or read values exactly

      for (arangodb::velocypack::ObjectIterator itr(field); itr.valid(); ++itr) {
        auto key = itr.key();
        auto value = itr.value();

        if (!key.isString()) {
          errorField = fieldName + "=>[" + arangodb::basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }

        auto name = key.copyString();

        if (!value.isObject()) {
          errorField = fieldName + "=>" + name;

          return false;
        }

        std::string childErrorField;

        if (!_fields[name]->init(value, errorField, subDefaults)) {
          errorField = fieldName + "=>" + name + "=>" + childErrorField;

          return false;
        }
      }
    }
  }

  return true;
}

bool IResearchLinkMeta::json(
  arangodb::velocypack::Builder& builder,
  IResearchLinkMeta const* ignoreEqual /*= nullptr*/,
  Mask const* mask /*= nullptr*/
) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _boost != ignoreEqual->_boost) && (!mask || mask->_boost)) {
    builder.add("boost", arangodb::velocypack::Value(_boost));
  }

  if (!mask || mask->_fields) { // fields are not inherited from parent
    arangodb::velocypack::Builder fieldsBuilder;

    {
      arangodb::velocypack::ObjectBuilder fieldsBuilderWrapper(&fieldsBuilder);
      arangodb::velocypack::Builder fieldBuilder;
      Mask mask(true); // output all non-matching fields
      auto subDefaults = *this;

      subDefaults._fields.clear(); // do not inherit fields and overrides overrides from this field

      for(auto& entry: _fields) {
        mask._fields = !entry.value()->_fields.empty(); // do not output empty fields on subobjects

        if (!entry.value()->json(arangodb::velocypack::ObjectBuilder(&fieldBuilder), &subDefaults, &mask)) {
          return false;
        }

        fieldsBuilderWrapper->add(entry.key(), fieldBuilder.slice());
        fieldBuilder.clear();
      }
    }

    builder.add("fields", fieldsBuilder.slice());
  }

  if ((!ignoreEqual || _includeAllFields != ignoreEqual->_includeAllFields) && (!mask || mask->_includeAllFields)) {
    builder.add("includeAllFields", arangodb::velocypack::Value(_includeAllFields));
  }

  if ((!ignoreEqual || _nestListValues != ignoreEqual->_nestListValues) && (!mask || mask->_nestListValues)) {
    builder.add("nestListValues", arangodb::velocypack::Value(_nestListValues));
  }

  if ((!ignoreEqual || !equalTokenizers(_tokenizers, ignoreEqual->_tokenizers)) && (!mask || mask->_tokenizers)) {
    arangodb::velocypack::Builder tokenizersBuilder;

    tokenizersBuilder.openArray();

    for (auto& entry: _tokenizers) {
      if (entry) { // skip null tokenizers
        tokenizersBuilder.add(arangodb::velocypack::Value(entry->name()));
      }
    }

    tokenizersBuilder.close();
    builder.add("tokenizers", tokenizersBuilder.slice());
  }

  return true;
}

bool IResearchLinkMeta::json(
  arangodb::velocypack::ObjectBuilder const& builder,
  IResearchLinkMeta const* ignoreEqual /*= nullptr*/,
  Mask const* mask /*= nullptr*/
) const {
  return builder.builder && json(*(builder.builder), ignoreEqual, mask);
}

size_t IResearchLinkMeta::memory() const {
  auto size = sizeof(IResearchLinkMeta);

  size += _fields.size() * sizeof(decltype(_fields)::value_type);

  for (auto& entry: _fields) {
    size += entry.key().size();
    size += entry.value()->memory();
  }

  size += _tokenizers.size() * sizeof(decltype(_tokenizers)::value_type);

  return size;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------