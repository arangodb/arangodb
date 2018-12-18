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

#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

#include "IResearchLinkMeta.h"
#include "Misc.h"

NS_LOCAL

bool equalAnalyzers(
  arangodb::iresearch::IResearchLinkMeta::Analyzers const& lhs,
  arangodb::iresearch::IResearchLinkMeta::Analyzers const& rhs
) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  std::unordered_multiset<irs::string_ref> expected;

  for (auto& entry: lhs) {
    expected.emplace(entry ? irs::string_ref(entry->name()) : irs::string_ref::NIL);
  }

  for (auto& entry: rhs) {
    auto itr = expected.find(entry ? irs::string_ref(entry->name()) : irs::string_ref::NIL);

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
  : _analyzers(mask),
    _fields(mask),
    _includeAllFields(mask),
    _trackListPositions(mask),
    _storeValues(mask) {
}

IResearchLinkMeta::IResearchLinkMeta()
  : //_fields(<empty>), // no fields to index by default
    _includeAllFields(false), // true to match all encountered fields, false match only fields in '_fields'
    _trackListPositions(false), // treat '_trackListPositions' as SQL-IN
    _storeValues(ValueStorage::NONE) { // do not track values at all
  auto analyzer = IResearchAnalyzerFeature::identity();

  // identity-only tokenization
  if (analyzer) {
    _analyzers.emplace_back(analyzer);
  }
}

IResearchLinkMeta::IResearchLinkMeta(IResearchLinkMeta const& other)
  : _analyzers(other._analyzers),
    _fields(other._fields),
    _includeAllFields(other._includeAllFields),
    _trackListPositions(other._trackListPositions),
    _storeValues(other._storeValues) {
}

IResearchLinkMeta::IResearchLinkMeta(IResearchLinkMeta&& other) noexcept
  : _analyzers(std::move(other._analyzers)),
    _fields(std::move(other._fields)),
    _includeAllFields(other._includeAllFields),
    _trackListPositions(other._trackListPositions),
    _storeValues(other._storeValues) {
}

IResearchLinkMeta& IResearchLinkMeta::operator=(IResearchLinkMeta&& other) noexcept {
  if (this != &other) {
    _analyzers = std::move(other._analyzers);
    _fields = std::move(other._fields);
    _includeAllFields = std::move(other._includeAllFields);
    _trackListPositions = std::move(other._trackListPositions);
    _storeValues = other._storeValues;
  }

  return *this;
}

IResearchLinkMeta& IResearchLinkMeta::operator=(IResearchLinkMeta const& other) {
  if (this != &other) {
    _analyzers = other._analyzers;
    _fields = other._fields;
    _includeAllFields = other._includeAllFields;
    _trackListPositions = other._trackListPositions;
    _storeValues = other._storeValues;
  }

  return *this;
}

bool IResearchLinkMeta::operator==(
  IResearchLinkMeta const& other
) const noexcept {
  if (!equalAnalyzers(_analyzers, other._analyzers)) {
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

  if (_trackListPositions != other._trackListPositions) {
    return false; // values do not match
  }

  if (_storeValues != other._storeValues) {
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
    // optional string list
    static const std::string fieldName("analyzers");

    mask->_analyzers = slice.hasKey(fieldName);

    if (!mask->_analyzers) {
      _analyzers = defaults._analyzers;
    } else {
      auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
        IResearchAnalyzerFeature
      >();
      auto field = slice.get(fieldName);

      if (!analyzers || !field.isArray()) {
        errorField = fieldName;

        return false;
      }

      _analyzers.clear(); // reset to match read values exactly

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
        _analyzers.emplace_back(analyzer);
      }
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
    static const std::string fieldName("trackListPositions");

    mask->_trackListPositions = slice.hasKey(fieldName);

    if (!mask->_trackListPositions) {
      _trackListPositions = defaults._trackListPositions;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isBool()) {
        errorField = fieldName;

        return false;
      }

      _trackListPositions = field.getBool();
    }
  }

  {
    // optional string enum
    static const std::string fieldName("storeValues");

    mask->_storeValues = slice.hasKey(fieldName);

    if (!mask->_storeValues) {
      _storeValues = defaults._storeValues;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isString()) {
        errorField = fieldName;

        return false;
      }

      static const std::unordered_map<std::string, ValueStorage> policies = {
        { "none", ValueStorage::NONE },
        { "id", ValueStorage::ID },
        { "full", ValueStorage::FULL }
      };
      auto name = field.copyString();
      auto itr = policies.find(name);

      if (itr == policies.end()) {
        errorField = fieldName + "=>" + name;

        return false;
      }

      _storeValues = itr->second;
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

  if ((!ignoreEqual || !equalAnalyzers(_analyzers, ignoreEqual->_analyzers))
      && (!mask || mask->_analyzers)) {
    arangodb::velocypack::Builder analyzersBuilder;

    analyzersBuilder.openArray();

    for (auto& entry: _analyzers) {
      if (entry) { // skip null analyzers
        analyzersBuilder.add(arangodb::velocypack::Value(entry->name()));
      }
    }

    analyzersBuilder.close();
    builder.add("analyzers", analyzersBuilder.slice());
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

  if ((!ignoreEqual || _trackListPositions != ignoreEqual->_trackListPositions) && (!mask || mask->_trackListPositions)) {
    builder.add("trackListPositions", arangodb::velocypack::Value(_trackListPositions));
  }

  if ((!ignoreEqual || _storeValues != ignoreEqual->_storeValues) && (!mask || mask->_storeValues)) {
    static_assert(adjacencyChecker<ValueStorage>::checkAdjacency<
      ValueStorage::FULL, ValueStorage::ID, ValueStorage::NONE>(),
      "Values are not adjacent"
    );

    static const std::string policies[] {
      "none", // ValueStorage::NONE
      "id", // ValueStorage::ID
      "full" // ValueStorage::FULL
    };

    auto const policyIdx = static_cast<std::underlying_type<ValueStorage>::type>(_storeValues);

    if (policyIdx >= IRESEARCH_COUNTOF(policies)) {
      return false; // unsupported value storage policy
    }

    builder.add("storeValues", arangodb::velocypack::Value(policies[policyIdx]));
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

size_t IResearchLinkMeta::memory() const noexcept {
  auto size = sizeof(IResearchLinkMeta);

  size += _analyzers.size() * sizeof(decltype(_analyzers)::value_type);
  size += _fields.size() * sizeof(decltype(_fields)::value_type);

  for (auto& entry: _fields) {
    size += entry.key().size();
    size += entry.value()->memory();
  }

  return size;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
