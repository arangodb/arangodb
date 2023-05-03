////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchLinkMeta.h"

#include "frozen/map.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/norm.hpp"
#include "utils/hash_utils.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Containers/FlatHashSet.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "VocBase/vocbase.h"
#include "IResearch/IResearchCommon.h"
#include "Misc.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VelocyPackHelper.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

#include <absl/strings/str_cat.h>

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

bool equalAnalyzers(std::vector<FieldMeta::Analyzer> const& lhs,
                    std::vector<FieldMeta::Analyzer> const& rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  containers::FlatHashMap<std::string_view, size_t> expected;

  for (auto& entry : lhs) {
    ++expected[entry._pool ? std::string_view(entry._pool->name())
                           : std::string_view{}];
  }

  for (auto& entry : rhs) {
    auto itr = expected.find(entry._pool ? std::string_view(entry._pool->name())
                                         : std::string_view{});

    if (itr == expected.end()) {
      return false;  // values do not match
    }

    if (--itr->second == 0) {
      expected.erase(itr);  // ensure same count of duplicates
    }
  }

  return true;
}

bool equalFields(IResearchLinkMeta::Fields const& lhs,
                 IResearchLinkMeta::Fields const& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (auto const notFoundField = rhs.end(); auto& entry : lhs) {
    auto rhsField = rhs.find(entry.first);
    if (rhsField == notFoundField || rhsField->second != entry.second) {
      return false;
    }
  }

  return true;
};

constexpr frozen::map<std::string_view, ValueStorage, 3> kNameToPolicy = {
    {"none", ValueStorage::NONE},
    {"id", ValueStorage::ID},
    {"value", ValueStorage::VALUE}};

constexpr std::array<std::string_view, kNameToPolicy.size()> kPolicyToName{
    "none",    // ValueStorage::NONE
    "id",      // ValueStorage::ID
    "value"};  // ValueStorage::VALUE

}  // namespace

bool operator<(arangodb::iresearch::FieldMeta::Analyzer const& lhs,
               std::string_view rhs) noexcept {
  return lhs._pool->name() < rhs;
}

bool operator<(std::string_view lhs,
               arangodb::iresearch::FieldMeta::Analyzer const& rhs) noexcept {
  return lhs < rhs._pool->name();
}

namespace arangodb {
namespace iresearch {

FieldMeta::Analyzer const& FieldMeta::identity() {
  static Analyzer kIdentity{IResearchAnalyzerFeature::identity()};
  return kIdentity;
}

bool FieldMeta::operator==(FieldMeta const& rhs) const noexcept {
  if (!equalAnalyzers(_analyzers, rhs._analyzers)) {
    return false;
  }

  if (!equalFields(_fields, rhs._fields)) {
    return false;
  }

  if (_includeAllFields != rhs._includeAllFields ||
      _trackListPositions != rhs._trackListPositions ||
      _storeValues != rhs._storeValues) {
    return false;
  }

#ifdef USE_ENTERPRISE
  if (_cache != rhs._cache) {
    return false;
  }

  if (_hasNested != rhs._hasNested) {
    return false;
  }

  if (!equalFields(_nested, rhs._nested)) {
    return false;
  }
#endif

  return true;
}

bool FieldMeta::init(
    ArangodServer& server, velocypack::Slice const& slice,
    std::string& errorField, std::string_view defaultVocbase,
    LinkVersion version, FieldMeta const& defaults,
    std::set<AnalyzerPool::ptr, AnalyzerComparer>& referencedAnalyzers,
    Mask* mask) {
  if (!slice.isObject()) {
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional string list
    constexpr std::string_view kFieldName("analyzers");

    auto field = slice.get(kFieldName);
    mask->_analyzers = !field.isNone();

    if (!mask->_analyzers) {
      _analyzers = defaults._analyzers;
      _primitiveOffset = defaults._primitiveOffset;
    } else {
      auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();
      bool const extendedNames =
          server.getFeature<DatabaseFeature>().extendedNames();

      if (!field.isArray()) {
        errorField = kFieldName;

        return false;
      }

      _analyzers.clear();  // reset to match read values exactly
      containers::FlatHashSet<std::string_view> uniqueGuard;

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isString()) {
          errorField = absl::StrCat(kFieldName, "[", itr.index(), "]");
          return false;
        }

        std::string name{value.stringView()};
        auto shortName = name;

        if (!irs::IsNull(defaultVocbase)) {
          name = IResearchAnalyzerFeature::normalize(name, defaultVocbase);
          shortName =
              IResearchAnalyzerFeature::normalize(name, defaultVocbase, false);
        }

        AnalyzerPool::ptr analyzer;
        bool found = false;

        auto it = referencedAnalyzers.find(std::string_view(name));

        if (it != referencedAnalyzers.end()) {
          analyzer = *it;
          found = static_cast<bool>(analyzer);

          if (ADB_UNLIKELY(!found)) {
            TRI_ASSERT(false);              // should not happen
            referencedAnalyzers.erase(it);  // remove null analyzer
          }
        }

        if (!found) {
          // For cluster only check cache to avoid ClusterInfo locking issues
          // analyzer should have been populated via 'analyzerDefinitions'
          // above.
          analyzer = analyzers.get(name, QueryAnalyzerRevisions::QUERY_LATEST,
                                   ServerState::instance()->isClusterRole());

          if (!analyzer) {
            errorField = absl::StrCat(kFieldName, ".", value.stringView());
            return false;
          }

          // Remap analyzer features to match version.
          AnalyzerPool::ptr remappedAnalyzer;

          auto const res = IResearchAnalyzerFeature::copyAnalyzerPool(
              remappedAnalyzer, *analyzer, version, extendedNames);

          if (res.fail() || !remappedAnalyzer) {
            errorField = absl::StrCat(kFieldName, ".", value.stringView());
            return false;
          }

          analyzer = remappedAnalyzer;
          referencedAnalyzers.emplace(analyzer);
        }

        // Avoid adding same analyzer twice.
        if (uniqueGuard.emplace(analyzer->name()).second) {
          _analyzers.emplace_back(analyzer, std::move(shortName));
        }

        TRI_ASSERT(referencedAnalyzers.contains(analyzer));
      }

      auto* begin = _analyzers.data();
      auto* end = _analyzers.data() + _analyzers.size();

      std::sort(begin, end, [](auto const& lhs, auto const& rhs) {
        return lhs._pool->inputType() < rhs._pool->inputType();
      });

      // find offset of the first non-primitive analyzer
      auto* primitiveEnd =
          std::find_if(begin, end, [](auto const& analyzer) noexcept {
            return analyzer._pool->accepts(AnalyzerValueType::Array |
                                           AnalyzerValueType::Object);
          });

      _primitiveOffset = std::distance(begin, primitiveEnd);
    }
  }

  {
    // optional bool
    constexpr std::string_view kFieldName{"includeAllFields"};

    auto field = slice.get(kFieldName);
    mask->_includeAllFields = !field.isNone();

    if (!mask->_includeAllFields) {
      _includeAllFields = defaults._includeAllFields;
    } else {
      if (!field.isBool()) {
        errorField = kFieldName;

        return false;
      }

      _includeAllFields = field.getBool();
    }
  }

  {
    // optional bool
    constexpr std::string_view kFieldName{"trackListPositions"};

    auto field = slice.get(kFieldName);
    mask->_trackListPositions = !field.isNone();

    if (!mask->_trackListPositions) {
      _trackListPositions = defaults._trackListPositions;
    } else {
      if (!field.isBool()) {
        errorField = kFieldName;

        return false;
      }

      _trackListPositions = field.getBool();
    }
  }

  {
    // optional string enum
    constexpr std::string_view kFieldName{"storeValues"};

    auto field = slice.get(kFieldName);
    mask->_storeValues = !field.isNone();

    if (!mask->_storeValues) {
      _storeValues = defaults._storeValues;
    } else {
      if (!field.isString()) {
        errorField = kFieldName;

        return false;
      }

      auto name = field.stringView();
      auto itr = kNameToPolicy.find(name);

      if (itr == kNameToPolicy.end()) {
        errorField = absl::StrCat(kFieldName, ".", name);
        return false;
      }

      _storeValues = itr->second;
    }
  }

#ifdef USE_ENTERPRISE
  // optional caching
  {
    auto const field = slice.get(StaticStrings::kCacheField);
    mask->_cache = !field.isNone();
    if (!mask->_cache) {
      _cache = defaults._cache;
    } else {
      if (!field.isBool()) {
        errorField = StaticStrings::kCacheField;
        return false;
      }
      _cache = field.getBool();
    }
  }
#endif

  // .............................................................................
  // process fields last since children inherit from parent
  // .............................................................................
  {
    // optional string map<name, overrides>
    constexpr std::string_view kFieldName{"fields"};

    auto field = slice.get(kFieldName);
    mask->_fields = !field.isNone();

    if (!mask->_fields) {
      _fields = defaults._fields;
    } else {
      if (!field.isObject()) {
        errorField = kFieldName;

        return false;
      }

      auto subDefaults = *this;

      // do not inherit fields and overrides from this field
      subDefaults._fields.clear();
      _fields.clear();  // reset to match either defaults or read values exactly

      for (velocypack::ObjectIterator itr(field); itr.valid(); ++itr) {
        auto key = itr.key();
        auto value = itr.value();

        if (!key.isString()) {
          errorField = absl::StrCat(kFieldName, "[", itr.index(), "]");
          return false;
        }

        auto name = key.stringView();

        if (!value.isObject()) {
          errorField = absl::StrCat(kFieldName, ".", name);
          return false;
        }

        std::string childErrorField;

        if (!_fields[name].init(server, value, childErrorField, defaultVocbase,
                                version, subDefaults, referencedAnalyzers,
                                nullptr)) {
          errorField =
              absl::StrCat(kFieldName, ".", name, ".", childErrorField);
          return false;
        }
      }
    }
  }

  {
    // optional string map<name, overrides>
    constexpr std::string_view kFieldName{"nested"};

    if (auto const field = slice.get(kFieldName); !field.isNone()) {
#ifndef USE_ENTERPRISE
      errorField =
          absl::StrCat("'", kFieldName,
                       "' is supported in ArangoDB Enterprise Edition only.");
      return false;
#else
      if (!field.isObject()) {
        errorField = kFieldName;

        return false;
      }

      auto subDefaults = *this;

      _nested.clear();  // reset to match either defaults or read values exactly

      for (velocypack::ObjectIterator itr(field); itr.valid(); ++itr) {
        auto key = itr.key();
        auto value = itr.value();

        if (!key.isString()) {
          errorField = absl::StrCat(kFieldName, "[", itr.index(), "]");
          return false;
        }

        auto name = key.stringView();

        if (!value.isObject()) {
          errorField = absl::StrCat(kFieldName, ".", name);
          return false;
        }

        std::string childErrorField;

        if (!_nested[name].init(server, value, childErrorField, defaultVocbase,
                                version, subDefaults, referencedAnalyzers,
                                nullptr)) {
          errorField =
              absl::StrCat(kFieldName, ".", name, ".", childErrorField);
          return false;
        }
      }
#endif
    }
  }

#ifdef USE_ENTERPRISE
  _hasNested = !_nested.empty();
  if (!_hasNested) {
    for (auto const& f : _fields) {
      if (!f.second._nested.empty()) {
        _hasNested = true;
        break;
      }
    }
  }
#endif

  return true;
}

bool FieldMeta::json(ArangodServer& server, velocypack::Builder& builder,
                     FieldMeta const* ignoreEqual /*= nullptr*/,
                     TRI_vocbase_t const* defaultVocbase /*= nullptr*/,
                     Mask const* mask /*= nullptr*/) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || !equalAnalyzers(_analyzers, ignoreEqual->_analyzers)) &&
      (!mask || mask->_analyzers)) {
    velocypack::Builder analyzersBuilder;

    analyzersBuilder.openArray();

    for (auto& entry : _analyzers) {
      if (!entry._pool) {
        continue;  // skip null analyzers
      }

      std::string name;

      if (defaultVocbase) {
        // @note: DBServerAgencySync::getLocalCollections(...) generates
        //        'forPersistence' definitions that are then compared in
        //        Maintenance.cpp:compareIndexes(...) via
        //        arangodb::Index::Compare(...) without access to
        //        'defaultVocbase', hence the generated definitions must not
        //        rely on 'defaultVocbase'
        //        hence must use 'expandVocbasePrefix==true' if
        //        'writeAnalyzerDefinition==true' for normalize
        //        for 'writeAnalyzerDefinition==false' must use
        //        'expandVocbasePrefix==false' so that dump/restore can restore
        //        definitions into differently named databases
        name = IResearchAnalyzerFeature::normalize(
            entry._pool->name(), defaultVocbase->name(), false);
      } else {
        name = entry._pool->name();  // verbatim (assume already normalized)
      }

      analyzersBuilder.add(velocypack::Value(std::move(name)));
    }

    analyzersBuilder.close();
    builder.add("analyzers", analyzersBuilder.slice());
  }

  if (!mask || mask->_fields) {  // fields are not inherited from parent
    velocypack::Builder fieldsBuilder;
    Mask fieldMask(true);      // output all non-matching fields
    auto subDefaults = *this;  // make modifable copy

    subDefaults._fields
        .clear();  // do not inherit fields and overrides from this field
    fieldsBuilder.openObject();

    for (auto& entry : _fields) {
      // do not output empty fields on subobjects
      fieldMask._fields = !entry.second._fields.empty();
      // add sub-object
      fieldsBuilder.add(entry.first, VPackValue(velocypack::ValueType::Object));

      if (!entry.second.json(server, fieldsBuilder, &subDefaults,
                             defaultVocbase, &fieldMask)) {
        return false;
      }

      fieldsBuilder.close();
    }
    fieldsBuilder.close();
    builder.add("fields", fieldsBuilder.slice());
  }
#ifdef USE_ENTERPRISE
  if (!_nested.empty()) {
    velocypack::Builder fieldsBuilder;
    Mask fieldMask(true);      // output all non-matching fields
    auto subDefaults = *this;  // make modifable copy
    fieldsBuilder.openObject();

    for (auto& entry : _nested) {
      // do not output empty fields on subobjects
      fieldMask._fields = !entry.second._fields.empty();
      fieldsBuilder.add(entry.first, VPackValue(velocypack::ValueType::Object));

      if (!entry.second.json(server, fieldsBuilder, &subDefaults,
                             defaultVocbase, &fieldMask)) {
        return false;
      }

      fieldsBuilder.close();
    }
    fieldsBuilder.close();
    builder.add("nested", fieldsBuilder.slice());
  }
#endif

  if ((!ignoreEqual || _includeAllFields != ignoreEqual->_includeAllFields) &&
      (!mask || mask->_includeAllFields)) {
    builder.add("includeAllFields", velocypack::Value(_includeAllFields));
  }

  if ((!ignoreEqual ||
       _trackListPositions != ignoreEqual->_trackListPositions) &&
      (!mask || mask->_trackListPositions)) {
    builder.add("trackListPositions", velocypack::Value(_trackListPositions));
  }

  if ((!ignoreEqual || _storeValues != ignoreEqual->_storeValues) &&
      (!mask || mask->_storeValues)) {
    static_assert(
        adjacencyChecker<ValueStorage>::checkAdjacency<
            ValueStorage::VALUE, ValueStorage::ID, ValueStorage::NONE>(),
        "Values are not adjacent");

    auto const policyIdx =
        static_cast<std::underlying_type<ValueStorage>::type>(_storeValues);

    if (policyIdx >= kPolicyToName.size()) {
      return false;  // unsupported value storage policy
    }

    builder.add("storeValues", velocypack::Value(kPolicyToName[policyIdx]));
  }

#ifdef USE_ENTERPRISE
  if (((!ignoreEqual && _cache) ||
       (ignoreEqual && _cache != ignoreEqual->_cache)) &&
      (!mask || mask->_cache)) {
    builder.add(StaticStrings::kCacheField, velocypack::Value(_cache));
  }
#endif

  return true;
}

size_t FieldMeta::memory() const noexcept {
  auto size = sizeof(FieldMeta);

  size += _analyzers.size() * sizeof(decltype(_analyzers)::value_type);
  size += _fields.size() * sizeof(decltype(_fields)::value_type);

  for (auto& entry : _fields) {
    size += entry.first.size();
    size += entry.second.memory();
  }

  return size;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 IResearchLinkMeta
// -----------------------------------------------------------------------------

IResearchLinkMeta::IResearchLinkMeta()
    : _version{static_cast<uint32_t>(LinkVersion::MIN)} {
  _analyzers.emplace_back(FieldMeta::identity());
  _primitiveOffset = _analyzers.size();
}

bool IResearchLinkMeta::operator==(
    IResearchLinkMeta const& other) const noexcept {
  if (FieldMeta::operator!=(other)) {
    return false;
  }

  if (_sort != other._sort || _storedValues != other._storedValues ||
      _sortCompression != other._sortCompression ||
      _version != other._version) {
    return false;
  }

#ifdef USE_ENTERPRISE
  if (_pkCache != other._pkCache || _sortCache != other._sortCache ||
      _optimizeTopK != other._optimizeTopK) {
    return false;
  }
#endif

  // Intentionally do not compare _collectioName here.
  // It should be filled equally during upgrade/creation
  // And during upgrade difference in name (filled/not filled) should not
  // trigger link recreation!

  return true;
}

bool IResearchLinkMeta::init(
    ArangodServer& server, VPackSlice slice, std::string& errorField,
    std::string_view defaultVocbase /*= std::string_view{}*/,
    LinkVersion defaultVersion /* = LinkVersion::MIN*/,
    Mask* mask /*= nullptr*/) {
  if (!slice.isObject()) {
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional sort
    auto const field = slice.get(StaticStrings::PrimarySortField);
    mask->_sort = field.isArray();

    if (mask->_sort && !_sort.fromVelocyPack(field, errorField)) {
      return false;
    }
  }

  {
    // optional stored values
    auto const field = slice.get(StaticStrings::StoredValuesField);
    mask->_storedValues = field.isArray();

    if (mask->_storedValues &&
        !_storedValues.fromVelocyPack(field, errorField)) {
      return false;
    }
  }
  {
    // optional sort compression
    auto const field = slice.get(StaticStrings::PrimarySortCompressionField);
    mask->_sortCompression = field.isString();

    if (mask->_sortCompression &&
        (_sortCompression = columnCompressionFromString(field.stringView())) ==
            nullptr) {
      return false;
    }
  }

#ifdef USE_ENTERPRISE
  {
    auto const field = slice.get(StaticStrings::kPrimarySortCacheField);
    mask->_sortCache = !field.isNone();
    if (mask->_sortCache) {
      if (!field.isBool()) {
        errorField = StaticStrings::kPrimarySortCacheField;
        return false;
      }
      _sortCache = field.getBoolean();
    }
  }
  {
    auto const field = slice.get(StaticStrings::kCachePrimaryKeyField);
    mask->_pkCache = !field.isNone();
    if (mask->_pkCache) {
      if (!field.isBool()) {
        errorField = StaticStrings::kCachePrimaryKeyField;
        return false;
      }
      _pkCache = field.getBool();
    }
  }
  {
    auto const field = slice.get(StaticStrings::kOptimizeTopKField);
    mask->_optimizeTopK = !field.isNone();
    std::string err;
    if (mask->_optimizeTopK) {
      if (!_optimizeTopK.fromVelocyPack(field, err)) {
        errorField = absl::StrCat(StaticStrings::kOptimizeTopKField, ": ", err);
        return false;
      }
    }
  }
#endif

  {
    // Optional version
    constexpr std::string_view kFieldName{StaticStrings::VersionField};

    auto const field = slice.get(kFieldName);
    mask->_version = field.isNumber<uint32_t>();

    if (mask->_version) {
      _version = field.getNumber<uint32_t>();
      if (_version > static_cast<uint32_t>(LinkVersion::MAX)) {
        errorField = kFieldName;
        return false;
      }
    } else if (field.isNone()) {
      // Not present -> default version.
      _version = static_cast<uint32_t>(defaultVersion);
    } else {
      errorField = kFieldName;
      return false;
    }
  }

  bool const extendedNames =
      server.getFeature<DatabaseFeature>().extendedNames();

  {
    _analyzerDefinitions.clear();

    // optional object list
    constexpr std::string_view kFieldName{
        StaticStrings::AnalyzerDefinitionsField};

    auto field = slice.get(kFieldName);
    mask->_analyzerDefinitions = !field.isNone();

    // load analyzer definitions if requested (used on cluster)
    // @note must load definitions before loading 'analyzers' to ensure presence
    if (mask->_analyzerDefinitions) {
      if (!field.isArray()) {
        errorField = kFieldName;

        return false;
      }

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isObject()) {
          errorField = absl::StrCat(kFieldName, "[", itr.index(), "]");
          return false;
        }

        std::string name;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"name"};

          auto nameSlice = value.get(kSubFieldName);
          if (!nameSlice.isString()) {
            errorField =
                absl::StrCat(kFieldName, "[", itr.index(), "].", kSubFieldName);

            return false;
          }

          name = nameSlice.stringView();
          if (!irs::IsNull(defaultVocbase)) {
            name =
                IResearchAnalyzerFeature::normalize(name, defaultVocbase, true);
          }
        }
        std::string_view type;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"type"};

          auto typeSlice = value.get(kSubFieldName);
          if (!typeSlice.isString()) {
            errorField =
                absl::StrCat(kFieldName, "[", itr.index(), "].", kSubFieldName);
            return false;
          }

          type = typeSlice.stringView();
        }

        VPackSlice properties;

        {
          // optional string value
          constexpr std::string_view kSubFieldName{"properties"};

          auto subField = value.get(kSubFieldName);
          if (!subField.isNone()) {
            if (!subField.isObject() && !subField.isNull()) {
              errorField = absl::StrCat(kFieldName, "[", itr.index(), "].",
                                        kSubFieldName);
              return false;
            }

            properties = subField;
          }
        }

        Features features;

        {
          // optional string list
          constexpr std::string_view kSubFieldName("features");

          auto subField = value.get(kSubFieldName);
          if (!subField.isNone()) {
            auto featuresRes = features.fromVelocyPack(subField);
            if (featuresRes.fail()) {
              errorField = absl::StrCat(kFieldName, " (",
                                        featuresRes.errorMessage(), ")");
              return false;
            }
          }
        }

        AnalyzersRevision::Revision revision{AnalyzersRevision::MIN};
        auto const revisionSlice =
            value.get(arangodb::StaticStrings::AnalyzersRevision);
        if (!revisionSlice.isNone()) {
          if (revisionSlice.isNumber()) {
            revision = revisionSlice.getNumber<AnalyzersRevision::Revision>();
          } else {
            errorField = arangodb::StaticStrings::AnalyzersRevision;
            return false;
          }
        }

        AnalyzerPool::ptr analyzer;
        auto const res = IResearchAnalyzerFeature::createAnalyzerPool(
            analyzer, name, type, properties, revision, features,
            LinkVersion{_version}, extendedNames);

        if (res.fail() || !analyzer) {
          errorField = absl::StrCat(kFieldName, "[", itr.index(), "]");
          if (res.fail()) {
            absl::StrAppend(&errorField, ": ", res.errorMessage());
          }
          return false;
        }

        _analyzerDefinitions.emplace(analyzer);
      }
    }
  }

  if (ServerState::instance()->isClusterRole()) {
    auto const field = slice.get(StaticStrings::CollectionNameField);
    if (field.isString()) {
      _collectionName = field.stringView();
    } else if (!field.isNone()) {
      return false;
    }
  }

  // Initialize version specific defaults
  FieldMeta defaults;
  {
    auto const& identity = *IResearchAnalyzerFeature::identity();
    AnalyzerPool::ptr analyzer;

    auto const res = IResearchAnalyzerFeature::copyAnalyzerPool(
        analyzer, identity, LinkVersion{_version}, extendedNames);

    if (res.fail() || !analyzer) {
      TRI_ASSERT(false);
      return false;
    }

    defaults._analyzers.emplace_back(analyzer);
    defaults._primitiveOffset = defaults._analyzers.size();

    _analyzers.clear();
    _analyzers.emplace_back(analyzer);
    _primitiveOffset = _analyzers.size();
  }

  return FieldMeta::init(server, slice, errorField, defaultVocbase,
                         LinkVersion{_version}, defaults, _analyzerDefinitions,
                         mask);
}

bool IResearchLinkMeta::json(ArangodServer& server,
                             velocypack::Builder& builder,
                             bool writeAnalyzerDefinition,
                             IResearchLinkMeta const* ignoreEqual /*= nullptr*/,
                             TRI_vocbase_t const* defaultVocbase /*= nullptr*/,
                             Mask const* mask /*= nullptr*/) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if (writeAnalyzerDefinition &&
      (!ignoreEqual || _sort != ignoreEqual->_sort) && (!mask || mask->_sort)) {
    velocypack::ArrayBuilder arrayScope(&builder,
                                        StaticStrings::PrimarySortField);
    if (!_sort.toVelocyPack(builder)) {
      return false;
    }
  }

  if (writeAnalyzerDefinition && (!mask || mask->_storedValues)) {
    velocypack::ArrayBuilder arrayScope(&builder,
                                        StaticStrings::StoredValuesField);
    if (!_storedValues.toVelocyPack(builder)) {
      return false;
    }
  }

  if (writeAnalyzerDefinition && (!mask || mask->_sortCompression) &&
      _sortCompression &&
      (!ignoreEqual || _sortCompression != ignoreEqual->_sortCompression)) {
    addStringRef(builder, StaticStrings::PrimarySortCompressionField,
                 columnCompressionToString(_sortCompression));
  }

#ifdef USE_ENTERPRISE
  if (writeAnalyzerDefinition && (!mask || mask->_sortCache) &&
      ((!ignoreEqual && _sortCache) ||
       (ignoreEqual && _sortCache != ignoreEqual->_sortCache))) {
    builder.add(StaticStrings::kPrimarySortCacheField, VPackValue(_sortCache));
  }
  if (writeAnalyzerDefinition && (!mask || mask->_optimizeTopK) &&
      (!ignoreEqual || _optimizeTopK != ignoreEqual->_optimizeTopK)) {
    velocypack::ArrayBuilder arrayScope(&builder,
                                        StaticStrings::kOptimizeTopKField);
    if (!_optimizeTopK.toVelocyPack(builder)) {
      return false;
    }
  }
#endif

  if (writeAnalyzerDefinition && (!mask || mask->_version)) {
    builder.add(StaticStrings::VersionField, VPackValue(_version));
  }

  // output definitions if 'writeAnalyzerDefinition' requested and not maked
  // this should be the case for the default top-most call
  if (writeAnalyzerDefinition && (!mask || mask->_analyzerDefinitions)) {
    VPackArrayBuilder arrayScope(&builder,
                                 StaticStrings::AnalyzerDefinitionsField);

    for (auto& entry : _analyzerDefinitions) {
      TRI_ASSERT(entry);  // ensured by emplace into 'analyzers' above
      entry->toVelocyPack(builder, defaultVocbase);
    }
  }

  if (writeAnalyzerDefinition && ServerState::instance()->isClusterRole() &&
      (!mask || mask->_collectionName) &&
      !_collectionName.empty()) {  // for old-style link meta do not emit empty
                                   // value to match stored definition
    addStringRef(builder, StaticStrings::CollectionNameField, _collectionName);
  }

#ifdef USE_ENTERPRISE
  if (writeAnalyzerDefinition && (!mask || mask->_pkCache) &&
      ((!ignoreEqual && _pkCache) ||
       (ignoreEqual && _pkCache != ignoreEqual->_pkCache))) {
    builder.add(StaticStrings::kCachePrimaryKeyField, VPackValue(_pkCache));
  }
#endif

  return FieldMeta::json(server, builder, ignoreEqual, defaultVocbase, mask);
}

size_t IResearchLinkMeta::memory() const noexcept {
  auto size = sizeof(IResearchLinkMeta);

  size += _analyzers.size() * sizeof(decltype(_analyzers)::value_type);
  size += _sort.memory();
  size += _storedValues.memory();
  size += _collectionName.size();
  size += sizeof(_version);
  size += FieldMeta::memory();
#ifdef USE_ENTERPRISE
  size += _optimizeTopK.memory();
#endif

  return size;
}

bool IResearchLinkMeta::willIndexIdAttribute() const noexcept {
  return _includeAllFields ||
         _fields.find(arangodb::StaticStrings::IdString) != _fields.end();
}

}  // namespace iresearch
}  // namespace arangodb
