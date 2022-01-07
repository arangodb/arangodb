////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <unordered_map>
#include <unordered_set>

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/norm.hpp"
#include "utils/hash_utils.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Containers/FlatHashSet.h"
#include "Basics/StringUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "VocBase/vocbase.h"
#include "IResearch/IResearchCommon.h"
#include "IResearchLinkMeta.h"
#include "Misc.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VelocyPackHelper.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

namespace {

using namespace arangodb::iresearch;

[[maybe_unused]] void assertAnaylzerFeatures(auto const& analyzers,
                                             LinkVersion version) noexcept {
  for (auto& analyzer : analyzers) {
    irs::type_info::type_id invalidNorm;
    if (version < LinkVersion::MAX) {
      invalidNorm = irs::type<irs::Norm2>::id();
    } else {
      invalidNorm = irs::type<irs::Norm>::id();
    }
    const auto features = analyzer->fieldFeatures();
    TRI_ASSERT(std::end(features) == std::find(std::begin(features),
                                               std::end(features),
                                               invalidNorm));
  }
}

bool equalAnalyzers(std::vector<FieldMeta::Analyzer> const& lhs,
                    std::vector<FieldMeta::Analyzer> const& rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  std::unordered_multiset<irs::string_ref> expected;

  for (auto& entry : lhs) {
    expected.emplace(entry._pool ? irs::string_ref(entry._pool->name())
                                 : irs::string_ref::NIL);
  }

  for (auto& entry : rhs) {
    auto itr = expected.find(entry._pool ? irs::string_ref(entry._pool->name())
                                         : irs::string_ref::NIL);

    if (itr == expected.end()) {
      return false;  // values do not match
    }

    expected.erase(itr);  // ensure same count of duplicates
  }

  return true;
}

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
               irs::string_ref rhs) noexcept {
  return lhs._pool->name() < rhs;
}

bool operator<(irs::string_ref lhs,
               arangodb::iresearch::FieldMeta::Analyzer const& rhs) noexcept {
  return lhs < rhs._pool->name();
}

namespace arangodb {
namespace iresearch {

FieldMeta::Analyzer::Analyzer()
    : _pool(IResearchAnalyzerFeature::identity()),
      _shortName(_pool ? _pool->name() : arangodb::StaticStrings::Empty) {}

FieldMeta::FieldMeta()
    : _analyzers{Analyzer()},  // identity analyzer
      _primitiveOffset{1} {}

bool FieldMeta::operator==(FieldMeta const& rhs) const noexcept {
  if (!equalAnalyzers(_analyzers, rhs._analyzers)) {
    return false;  // values do not match
  }

  if (_fields.size() != rhs._fields.size()) {
    return false;  // values do not match
  }

  auto const notFoundField = rhs._fields.end();

  for (auto& entry : _fields) {
    auto rhsField = rhs._fields.find(entry.key());
    if (rhsField == notFoundField || rhsField.value() != entry.value()) {
      return false;
    }
  }

  if (_includeAllFields != rhs._includeAllFields) {
    return false;  // values do not match
  }

  if (_trackListPositions != rhs._trackListPositions) {
    return false;  // values do not match
  }

  if (_storeValues != rhs._storeValues) {
    return false;  // values do not match
  }

  return true;
}

bool FieldMeta::init(application_features::ApplicationServer& server,
                     velocypack::Slice const& slice, std::string& errorField,
                     irs::string_ref defaultVocbase, FieldMeta const& defaults,
                     Mask* mask /*= nullptr*/,
                     std::set<AnalyzerPool::ptr, AnalyzerComparer>*
                         referencedAnalyzers /*= nullptr*/) {
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

    mask->_analyzers = slice.hasKey(kFieldName);

    if (!mask->_analyzers) {
      _analyzers = defaults._analyzers;
      _primitiveOffset = defaults._primitiveOffset;
    } else {
      auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();

      auto field = slice.get(kFieldName);

      if (!field.isArray()) {
        errorField = kFieldName;

        return false;
      }

      _analyzers.clear();  // reset to match read values exactly
      containers::FlatHashSet<std::string_view> uniqueGuard;

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isString()) {
          errorField =
              std::string{kFieldName} + "[" + std::to_string(itr.index()) + "]";

          return false;
        }

        auto name = value.copyString();
        auto shortName = name;

        if (!defaultVocbase.null()) {
          name = IResearchAnalyzerFeature::normalize(name, defaultVocbase);
          shortName =
              IResearchAnalyzerFeature::normalize(name, defaultVocbase, false);
        }

        AnalyzerPool::ptr analyzer;
        bool found = false;

        if (referencedAnalyzers) {
          auto it = referencedAnalyzers->find(irs::string_ref(name));

          if (it != referencedAnalyzers->end()) {
            analyzer = *it;
            found = static_cast<bool>(analyzer);

            if (ADB_UNLIKELY(!found)) {
              TRI_ASSERT(false);               // should not happen
              referencedAnalyzers->erase(it);  // remove null analyzer
            }
          }
        }

        if (!found) {
          // for cluster only check cache to avoid ClusterInfo locking issues
          // analyzer should have been populated via 'analyzerDefinitions' above
          analyzer = analyzers.get(name, QueryAnalyzerRevisions::QUERY_LATEST,
                                   ServerState::instance()->isClusterRole());
        }

        if (!analyzer) {
          errorField = std::string{kFieldName} + "." + value.copyString();

          return false;
        }

        if (!found && referencedAnalyzers) {
          // save in referencedAnalyzers

          // FIXME translate features according to the current link version
          referencedAnalyzers->emplace(analyzer);
        }

        // avoid adding same analyzer twice
        if (uniqueGuard.emplace(analyzer->name()).second) {
          _analyzers.emplace_back(analyzer, std::move(shortName));
        }
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

    mask->_includeAllFields = slice.hasKey(kFieldName);

    if (!mask->_includeAllFields) {
      _includeAllFields = defaults._includeAllFields;
    } else {
      auto field = slice.get(kFieldName);

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

    mask->_trackListPositions = slice.hasKey(kFieldName);

    if (!mask->_trackListPositions) {
      _trackListPositions = defaults._trackListPositions;
    } else {
      auto field = slice.get(kFieldName);

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

    mask->_storeValues = slice.hasKey(kFieldName);

    if (!mask->_storeValues) {
      _storeValues = defaults._storeValues;
    } else {
      auto field = slice.get(kFieldName);

      if (!field.isString()) {
        errorField = kFieldName;

        return false;
      }

      auto name = field.copyString();
      auto itr = kNameToPolicy.find(name);

      if (itr == kNameToPolicy.end()) {
        errorField = std::string{kFieldName} + "." + name;

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
    constexpr std::string_view kFieldName{"fields"};

    mask->_fields = slice.hasKey(kFieldName);

    if (!mask->_fields) {
      _fields = defaults._fields;
    } else {
      auto field = slice.get(kFieldName);

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
          errorField = std::string{kFieldName} + "[" +
                       basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }

        auto name = key.copyString();

        if (!value.isObject()) {
          errorField = std::string{kFieldName} + "." + name;

          return false;
        }

        std::string childErrorField;

        if (!_fields[name]->init(server, value, childErrorField, defaultVocbase,
                                 subDefaults, nullptr, referencedAnalyzers)) {
          errorField =
              std::string{kFieldName} + "." + name + "." + childErrorField;

          return false;
        }
      }
    }
  }

  return true;
}

bool FieldMeta::json(application_features::ApplicationServer& server,
                     velocypack::Builder& builder,
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
      fieldMask._fields =
          !entry.value()
               ->_fields.empty();  // do not output empty fields on subobjects
      fieldsBuilder.add(           // add sub-object
          std::string_view(entry.key().c_str(),
                           entry.key().size()),  // field name
          VPackValue(velocypack::ValueType::Object));

      if (!entry.value()->json(server, fieldsBuilder, &subDefaults,
                               defaultVocbase, &fieldMask)) {
        return false;
      }

      fieldsBuilder.close();
    }

    fieldsBuilder.close();
    builder.add("fields", fieldsBuilder.slice());
  }

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

  return true;
}

size_t FieldMeta::memory() const noexcept {
  auto size = sizeof(FieldMeta);

  size += _analyzers.size() * sizeof(decltype(_analyzers)::value_type);
  size += _fields.size() * sizeof(decltype(_fields)::value_type);

  for (auto& entry : _fields) {
    size += entry.key().size();
    size += entry.value()->memory();
  }

  return size;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 IResearchLinkMeta
// -----------------------------------------------------------------------------

IResearchLinkMeta::IResearchLinkMeta()
    : _version{static_cast<uint32_t>(LinkVersion::MAX)} {
  // add default analyzers
  for (auto& analyzer : _analyzers) {
    _analyzerDefinitions.emplace(analyzer._pool);
  }
}

bool IResearchLinkMeta::operator==(
    IResearchLinkMeta const& other) const noexcept {
  if (FieldMeta::operator!=(other)) {
    return false;
  }

  if (_sort != other._sort) {
    return false;
  }

  if (_storedValues != other._storedValues) {
    return false;
  }

  if (_sortCompression != other._sortCompression) {
    return false;
  }

  if (_version != other._version) {
    return false;
  }

  // Intentionally do not compare _collectioName here.
  // It should be filled equally during upgrade/creation
  // And during upgrade difference in name (filled/not filled) should not
  // trigger link recreation!

  return true;
}

bool IResearchLinkMeta::init(
    application_features::ApplicationServer& server, VPackSlice slice,
    bool readAnalyzerDefinition, std::string& errorField,
    irs::string_ref defaultVocbase /*= irs::string_ref::NIL*/,
    Mask* mask /*= nullptr*/) {
  // FIXME(gnusi) defaults based on version
  static const IResearchLinkMeta defaults;

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

    if (readAnalyzerDefinition && mask->_sort &&
        !_sort.fromVelocyPack(field, errorField)) {
      return false;
    }
  }

  {
    // optional stored values
    auto const field = slice.get(StaticStrings::StoredValuesField);
    mask->_storedValues = field.isArray();

    if (readAnalyzerDefinition && mask->_storedValues &&
        !_storedValues.fromVelocyPack(field, errorField)) {
      return false;
    }
  }
  {
    // optional sort compression
    auto const field = slice.get(StaticStrings::PrimarySortCompressionField);
    mask->_sortCompression = field.isString();

    if (readAnalyzerDefinition && mask->_sortCompression &&
        (_sortCompression = columnCompressionFromString(getStringRef(field))) ==
            nullptr) {
      return false;
    }
  }

  {
    // optional version
    auto& fieldName = StaticStrings::VersionField;

    auto const field = slice.get(fieldName);
    mask->_version = field.isNumber<uint32_t>();

    if (readAnalyzerDefinition) {
      if (mask->_version) {
        _version = field.getNumber<uint32_t>();
        if (_version > static_cast<uint32_t>(LinkVersion::MAX)) {
          errorField = fieldName;
          return false;
        }
      } else if (field.isNone()) {
        // not present -> old version
        _version = static_cast<uint32_t>(LinkVersion::MIN);
      } else {
        errorField = fieldName;
        return false;
      }
    }
  }

  {
    // clear existing definitions
    _analyzerDefinitions.clear();

    //    {
    //      auto id = IResearchAnalyzerFeature::identity();
    //
    //      bool extendedNames =
    //          server.getFeature<DatabaseFeature>().extendedNamesForAnalyzers();
    //      AnalyzerPool::ptr analyzer;
    //      auto const res = IResearchAnalyzerFeature::createAnalyzerPool(
    //          analyzer, id->name(), id->type(), id->properties(),
    //          AnalyzersRevision::Revision{AnalyzersRevision::LATEST},
    //          id->features(), LinkVersion{_version}, extendedNames);
    //      _analyzerDefinitions.emplace(analyzer);
    //    }

    // optional object list
    constexpr std::string_view kFieldName{
        StaticStrings::AnalyzerDefinitionsField};

    mask->_analyzerDefinitions = slice.hasKey(kFieldName);

    // load analyzer definitions if requested (used on cluster)
    // @note must load definitions before loading 'analyzers' to ensure presence
    if (readAnalyzerDefinition && mask->_analyzerDefinitions) {
      auto field = slice.get(kFieldName);

      if (!field.isArray()) {
        errorField = kFieldName;

        return false;
      }

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isObject()) {
          errorField =
              std::string{kFieldName} + "[" + std::to_string(itr.index()) + "]";

          return false;
        }

        std::string name;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"name"};

          if (!value.hasKey(kSubFieldName)  // missing required filed
              || !value.get(kSubFieldName).isString()) {
            errorField = std::string{kFieldName} + "[" +
                         std::to_string(itr.index()) + "]." +
                         std::string{kSubFieldName};

            return false;
          }

          name = value.get(kSubFieldName).copyString();
          if (!defaultVocbase.null()) {
            name =
                IResearchAnalyzerFeature::normalize(name, defaultVocbase, true);
          }
        }
        irs::string_ref type;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"type"};

          if (!value.hasKey(kSubFieldName)  // missing required filed
              || !value.get(kSubFieldName).isString()) {
            errorField = std::string{kFieldName} + "[" +
                         std::to_string(itr.index()) + "]." +
                         std::string{kSubFieldName};

            return false;
          }

          type = getStringRef(value.get(kSubFieldName));
        }

        VPackSlice properties;

        {
          // optional string value
          constexpr std::string_view kSubFieldName{"properties"};

          if (value.hasKey(kSubFieldName)) {
            auto subField = value.get(kSubFieldName);

            if (!subField.isObject() && !subField.isNull()) {
              errorField = std::string{kFieldName} + "[" +
                           std::to_string(itr.index()) + "]." +
                           std::string{kSubFieldName};

              return false;
            }

            properties = subField;
          }
        }

        Features features;

        {
          // optional string list
          constexpr std::string_view kSubFieldName("features");

          if (value.hasKey(kSubFieldName)) {
            auto subField = value.get(kSubFieldName);

            if (!subField.isArray()) {
              errorField = std::string{kFieldName} + "[" +
                           std::to_string(itr.index()) + "]." +
                           std::string{kSubFieldName};

              return false;
            }

            for (velocypack::ArrayIterator subItr(subField); subItr.valid();
                 ++subItr) {
              auto subValue = *subItr;

              if (!subValue.isString() && !subValue.isNull()) {
                errorField = std::string{kFieldName} + "[" +
                             std::to_string(itr.index()) + "]." +
                             std::string{kSubFieldName} + "[" +
                             std::to_string(subItr.index()) + +"]";

                return false;
              }

              const auto featureName = getStringRef(subValue);
              if (!features.add(featureName)) {
                errorField = std::string{kFieldName} + "[" +
                             std::to_string(itr.index()) + "]." +
                             std::string{kSubFieldName} + "." +
                             std::string{featureName};

                return false;
              }
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

        bool extendedNames =
            server.getFeature<DatabaseFeature>().extendedNamesForAnalyzers();
        AnalyzerPool::ptr analyzer;
        auto const res = IResearchAnalyzerFeature::createAnalyzerPool(
            analyzer, name, type, properties, revision, features,
            LinkVersion{_version}, extendedNames);

        if (res.fail() || !analyzer) {
          errorField =
              std::string{kFieldName} + "[" + std::to_string(itr.index()) + "]";

          if (res.fail()) {
            errorField.append(": ").append(res.errorMessage());
          }

          return false;
        }

        _analyzerDefinitions.emplace(analyzer);
      }
    }
  }

  if (slice.hasKey(StaticStrings::CollectionNameField) &&
      ServerState::instance()->isClusterRole()) {
    auto const field = slice.get(StaticStrings::CollectionNameField);
    if (!field.isString()) {
      return false;
    }
    _collectionName = field.copyString();
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto checkFeatures = scopeGuard([this]() noexcept {
    ::assertAnaylzerFeatures(_analyzerDefinitions, LinkVersion{_version});
  });
#endif

  return FieldMeta::init(server, slice, errorField, defaultVocbase, defaults,
                         mask, &_analyzerDefinitions);
}

bool IResearchLinkMeta::json(application_features::ApplicationServer& server,
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

  if (writeAnalyzerDefinition && (!mask || mask->_version)) {
    builder.add(StaticStrings::VersionField, VPackValue(_version));
  }

  // output definitions if 'writeAnalyzerDefinition' requested and not maked
  // this should be the case for the default top-most call
  if (writeAnalyzerDefinition && (!mask || mask->_analyzerDefinitions)) {
    // FIXME(gnusi) Why VPackArrayBuilder doesn't accept string_view?
    VPackArrayBuilder arrayScope(
        &builder, std::string{StaticStrings::AnalyzerDefinitionsField});

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

  return size;
}

}  // namespace iresearch
}  // namespace arangodb
