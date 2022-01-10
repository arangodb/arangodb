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

#include <unordered_map>
#include <unordered_set>

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/norm.hpp"
#include "utils/hash_utils.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
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

bool equalAnalyzers(
    std::vector<arangodb::iresearch::FieldMeta::Analyzer> const& lhs,
    std::vector<arangodb::iresearch::FieldMeta::Analyzer> const& rhs) noexcept {
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

}  // namespace

bool operator<(arangodb::iresearch::FieldMeta::Analyzer const& lhs,
               irs::string_ref const& rhs) noexcept {
  return lhs._pool->name() < rhs;
}

bool operator<(irs::string_ref const& lhs,
               arangodb::iresearch::FieldMeta::Analyzer const& rhs) noexcept {
  return lhs < rhs._pool->name();
}

namespace arangodb {
namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                               FieldMeta::Analyzer
// -----------------------------------------------------------------------------

FieldMeta::Analyzer::Analyzer()
    : _pool(IResearchAnalyzerFeature::identity()),
      _shortName(_pool ? _pool->name() : arangodb::StaticStrings::Empty) {}

// -----------------------------------------------------------------------------
// --SECTION--                                      IResearchLinkMeta::FieldMeta
// -----------------------------------------------------------------------------

/*static*/ const FieldMeta& FieldMeta::DEFAULT() {
  static const FieldMeta meta;

  return meta;
}

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
                     irs::string_ref const defaultVocbase,
                     FieldMeta const& defaults /*= DEFAULT()*/,
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
    static const std::string fieldName("analyzers");

    mask->_analyzers = slice.hasKey(fieldName);

    if (!mask->_analyzers) {
      _analyzers = defaults._analyzers;
      _primitiveOffset = defaults._primitiveOffset;
    } else {
      auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();

      auto field = slice.get(fieldName);

      if (!field.isArray()) {
        errorField = fieldName;

        return false;
      }

      _analyzers.clear();  // reset to match read values exactly
      std::unordered_set<irs::string_ref> uniqueGuard;  // deduplicate analyzers

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isString()) {
          errorField = fieldName + "[" + std::to_string(itr.index()) + "]";

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
          errorField =
              fieldName + "." +
              value.copyString();  // original (non-normalized) 'name' value

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
          {"none", ValueStorage::NONE},
          {"id", ValueStorage::ID},
          {"value", ValueStorage::VALUE}};

      auto name = field.copyString();
      auto itr = policies.find(name);

      if (itr == policies.end()) {
        errorField = fieldName + "." + name;

        return false;
      }

      _storeValues = itr->second;
    }
  }

  static const std::string fieldsFieldName("fields");
  mask->_fields = slice.hasKey(fieldsFieldName);
  // .............................................................................
  // process fields last since children inherit from parent
  // .............................................................................

  if (!mask->_fields) {
    _fields = defaults._fields;
  } else {
    auto field = slice.get(fieldsFieldName);

    if (!field.isObject()) {
      errorField = fieldsFieldName;

      return false;
    }

    auto subDefaults = *this;

    subDefaults._fields
        .clear();     // do not inherit fields and overrides from this field
    _fields.clear();  // reset to match either defaults or read values exactly

    for (velocypack::ObjectIterator itr(field); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();

      if (!key.isString()) {
        errorField = fieldsFieldName + "[" +
                     basics::StringUtils::itoa(itr.index()) + "]";

        return false;
      }

      auto name = key.copyString();

      if (!value.isObject()) {
        errorField = fieldsFieldName + "." + name;

        return false;
      }

      std::string childErrorField;

      if (!_fields[name]->init(server, value, childErrorField, defaultVocbase,
                               subDefaults, nullptr, referencedAnalyzers)) {
        errorField = fieldsFieldName + "." + name + "." + childErrorField;

        return false;
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

  std::map<std::string, AnalyzerPool::ptr> analyzers;

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

      analyzers.emplace(name, entry._pool);
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

    static const std::string policies[]{
        "none",  // ValueStorage::NONE
        "id",    // ValueStorage::ID
        "value"  // ValueStorage::VALUE
    };

    auto const policyIdx =
        static_cast<std::underlying_type<ValueStorage>::type>(_storeValues);

    if (policyIdx >= IRESEARCH_COUNTOF(policies)) {
      return false;  // unsupported value storage policy
    }

    builder.add("storeValues", velocypack::Value(policies[policyIdx]));
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

/*static*/ const IResearchLinkMeta& IResearchLinkMeta::DEFAULT() {
  static const IResearchLinkMeta meta;

  return meta;
}

bool IResearchLinkMeta::init(
    application_features::ApplicationServer& server, VPackSlice slice,
    bool readAnalyzerDefinition, std::string& errorField,
    irs::string_ref const defaultVocbase /*= irs::string_ref::NIL*/,
    IResearchLinkMeta const& defaults /*= DEFAULT()*/,
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
        _version = static_cast<uint32_t>(
            LinkVersion::MIN);  // not present -> old version
      } else {
        errorField = fieldName;
        return false;
      }
    }
  }

  {
    // clear existing definitions
    _analyzerDefinitions.clear();

    // optional object list
    auto& fieldName = StaticStrings::AnalyzerDefinitionsField;

    mask->_analyzerDefinitions = slice.hasKey(fieldName);

    // load analyzer definitions if requested (used on cluster)
    // @note must load definitions before loading 'analyzers' to ensure presence
    if (readAnalyzerDefinition && mask->_analyzerDefinitions) {
      auto field = slice.get(fieldName);

      if (!field.isArray()) {
        errorField = fieldName;

        return false;
      }

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isObject()) {
          errorField = fieldName + "[" + std::to_string(itr.index()) + "]";

          return false;
        }

        std::string name;

        {
          // required string value
          static const std::string subFieldName("name");

          if (!value.hasKey(subFieldName)  // missing required filed
              || !value.get(subFieldName).isString()) {
            errorField = fieldName + "[" + std::to_string(itr.index()) + "]." +
                         subFieldName;

            return false;
          }

          name = value.get(subFieldName).copyString();
          if (!defaultVocbase.null()) {
            name =
                IResearchAnalyzerFeature::normalize(name, defaultVocbase, true);
          }
        }
        irs::string_ref type;

        {
          // required string value
          static const std::string subFieldName("type");

          if (!value.hasKey(subFieldName)  // missing required filed
              || !value.get(subFieldName).isString()) {
            errorField = fieldName + "[" + std::to_string(itr.index()) + "]." +
                         subFieldName;

            return false;
          }

          type = getStringRef(value.get(subFieldName));
        }

        VPackSlice properties;

        {
          // optional string value
          static const std::string subFieldName("properties");

          if (value.hasKey(subFieldName)) {
            auto subField = value.get(subFieldName);

            if (!subField.isObject() && !subField.isNull()) {
              errorField = fieldName + "[" + std::to_string(itr.index()) +
                           "]." + subFieldName;

              return false;
            }

            properties = subField;
          }
        }

        Features features;

        {
          // optional string list
          static const std::string subFieldName("features");

          if (value.hasKey(subFieldName)) {
            auto subField = value.get(subFieldName);

            if (!subField.isArray()) {
              errorField = fieldName + "[" + std::to_string(itr.index()) +
                           "]." + subFieldName;

              return false;
            }

            for (velocypack::ArrayIterator subItr(subField); subItr.valid();
                 ++subItr) {
              auto subValue = *subItr;

              if (!subValue.isString() && !subValue.isNull()) {
                errorField = fieldName + "[" + std::to_string(itr.index()) +
                             "]." + subFieldName + "[" +
                             std::to_string(subItr.index()) + +"]";

                return false;
              }

              const auto featureName = getStringRef(subValue);
              if (!features.add(featureName)) {
                errorField = fieldName + "[" + std::to_string(itr.index()) +
                             "]." + subFieldName + "." +
                             std::string(featureName);

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
          errorField = fieldName + "[" + std::to_string(itr.index()) + "]";

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

bool InvertedIndexFieldMeta::init(
    arangodb::application_features::ApplicationServer& server,
    velocypack::Slice const& slice, bool readAnalyzerDefinition,
    std::string& errorField, irs::string_ref const defaultVocbase) {
  // copied part begin FIXME: verify and move to common base class if possible
  {
    // optional stored values
    static VPackStringRef const fieldName("storedValues");

    auto const field = slice.get(fieldName);
    if (!field.isNone() && !_storedValues.fromVelocyPack(field, errorField)) {
      return false;
    }
  }
  {
    // optional sort compression
    static VPackStringRef const fieldName("primarySortCompression");
    auto const field = slice.get(fieldName);

    if (!field.isNone() && ((_sortCompression = columnCompressionFromString(
                                 getStringRef(field))) == nullptr)) {
      return false;
    }
  }
  {
    // optional primarySort
    static VPackStringRef const fieldName("primarySort");
    auto const field = slice.get(fieldName);
    if (!field.isNone() && !_sort.fromVelocyPack(field, errorField)) {
      return false;
    }
  }
  {
    // optional version
    VPackStringRef constexpr fieldName("version");
    auto const field = slice.get(fieldName);
    if (field.isNumber()) {
      _version = field.getNumber<uint32_t>();
      if (_version > static_cast<uint32_t>(LinkVersion::MAX)) {
        errorField = fieldName;
        return false;
      }
    } else if (field.isNone()) {
      _version = static_cast<uint32_t>(
          LinkVersion::MAX);  // not present -> last version
    } else {
      errorField = fieldName;
      return false;
    }
  }
  {
    // clear existing definitions
    _analyzerDefinitions.clear();

    // optional object list
    static const std::string fieldName("analyzerDefinitions");

    // load analyzer definitions if requested (used on cluster)
    // @note must load definitions before loading 'analyzers' to ensure presence
    if (readAnalyzerDefinition) {
      auto field = slice.get(fieldName);

      if (!field.isArray()) {
        errorField = fieldName;

        return false;
      }

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isObject()) {
          errorField = fieldName + "[" + std::to_string(itr.index()) + "]";

          return false;
        }

        std::string name;

        {
          // required string value
          static const std::string subFieldName("name");

          if (!value.hasKey(subFieldName)  // missing required filed
              || !value.get(subFieldName).isString()) {
            errorField = fieldName + "[" + std::to_string(itr.index()) + "]." +
                         subFieldName;

            return false;
          }

          name = value.get(subFieldName).copyString();
          if (!defaultVocbase.null()) {
            name =
                IResearchAnalyzerFeature::normalize(name, defaultVocbase, true);
          }
        }
        irs::string_ref type;

        {
          // required string value
          static const std::string subFieldName("type");

          if (!value.hasKey(subFieldName)  // missing required filed
              || !value.get(subFieldName).isString()) {
            errorField = fieldName + "[" + std::to_string(itr.index()) + "]." +
                         subFieldName;

            return false;
          }

          type = getStringRef(value.get(subFieldName));
        }

        VPackSlice properties;

        {
          // optional string value
          static const std::string subFieldName("properties");

          if (value.hasKey(subFieldName)) {
            auto subField = value.get(subFieldName);

            if (!subField.isObject() && !subField.isNull()) {
              errorField = fieldName + "[" + std::to_string(itr.index()) +
                           "]." + subFieldName;

              return false;
            }

            properties = subField;
          }
        }
        Features features;
        {
          // optional string list
          static const std::string subFieldName("features");

          if (value.hasKey(subFieldName)) {
            auto subField = value.get(subFieldName);

            if (!subField.isArray()) {
              errorField = fieldName + "[" + std::to_string(itr.index()) +
                           "]." + subFieldName;

              return false;
            }

            for (velocypack::ArrayIterator subItr(subField); subItr.valid();
                 ++subItr) {
              auto subValue = *subItr;

              if (!subValue.isString() && !subValue.isNull()) {
                errorField = fieldName + "[" + std::to_string(itr.index()) +
                             "]." + subFieldName + "[" +
                             std::to_string(subItr.index()) + +"]";

                return false;
              }

              const auto featureName = getStringRef(subValue);
              if (!features.add(featureName)) {
                errorField = fieldName + "[" + std::to_string(itr.index()) +
                             "]." + subFieldName + "." +
                             std::string(featureName);

                return false;
              }
            }
          }
        }

        arangodb::AnalyzersRevision::Revision revision{
            arangodb::AnalyzersRevision::MIN};
        auto const revisionSlice =
            value.get(arangodb::StaticStrings::AnalyzersRevision);
        if (!revisionSlice.isNone()) {
          if (revisionSlice.isNumber()) {
            revision = revisionSlice
                           .getNumber<arangodb::AnalyzersRevision::Revision>();
          } else {
            errorField = arangodb::StaticStrings::AnalyzersRevision;
            return false;
          }
        }

        AnalyzerPool::ptr analyzer;
        bool extendedNames =
            server.getFeature<DatabaseFeature>().extendedNamesForAnalyzers();
        auto const res = IResearchAnalyzerFeature::createAnalyzerPool(
            analyzer, name, type, properties, revision, features,
            LinkVersion{_version}, extendedNames);

        if (res.fail() || !analyzer) {
          errorField = fieldName + "[" + std::to_string(itr.index()) + "]";
          if (res.fail()) {
            errorField.append(": ").append(res.errorMessage());
          }
          return false;
        }
        _analyzerDefinitions.emplace(analyzer);
      }
    }
  }
  // end of the copied part
  static const std::string fieldsFieldName("fields");
  // for index there is no recursive struct and fields array is mandatory
  auto field = slice.get(fieldsFieldName);
  if (!field.isArray() || field.isEmptyArray()) {
    errorField = fieldsFieldName;
    return false;
  }
  auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();
  for (VPackArrayIterator itr(field); itr.valid(); itr.next()) {
    auto val = itr.value();
    if (val.isString()) {
      try {
        std::vector<basics::AttributeName> fieldParts;
        TRI_ParseAttributeString(val.stringView(), fieldParts, true);
        TRI_ASSERT(!fieldParts.empty());
        // we only allow expansion of the last field
        if (fieldParts.rend() !=
            std::find_if(++fieldParts.rbegin(), fieldParts.rend(),
                         [](basics::AttributeName const& a) {
                           return a.shouldExpand;
                         })) {
          LOG_TOPIC("43bda", ERR, iresearch::TOPIC)
              << "Error parsing field: '" << val.stringView() << "'. "
              << "Expansion is allowed only for the last path part.";
          errorField = fieldsFieldName + "[" +
                       basics::StringUtils::itoa(itr.index()) + "]";
          return false;
        }
        _fields.emplace_back(
            std::move(fieldParts),
            FieldMeta::Analyzer(
                IResearchAnalyzerFeature::identity(),
                std::string(IResearchAnalyzerFeature::identity()
                                ->name())));  // FIXME: deduplicate
      } catch (arangodb::basics::Exception const& err) {
        LOG_TOPIC("1d04c", ERR, iresearch::TOPIC)
            << "Error parsing attribute: " << err.what();
        errorField = fieldsFieldName + "[" +
                     basics::StringUtils::itoa(itr.index()) + "]";
        return false;
      }
    } else if (val.isObject()) {
      auto nameSlice = val.get("name");
      if (nameSlice.isString()) {
        std::vector<basics::AttributeName> fieldParts;
        try {
          TRI_ParseAttributeString(nameSlice.stringRef(), fieldParts, true);
          TRI_ASSERT(!fieldParts.empty());
          // we only allow one expansion
          size_t expansionCount{0};
          std::for_each(
              fieldParts.begin(), fieldParts.end(),
              [&expansionCount](basics::AttributeName const& a) -> void {
                if (a.shouldExpand) {
                  ++expansionCount;
                }
              });
          if (expansionCount > 1) {
            LOG_TOPIC("2646b", ERR, iresearch::TOPIC)
                << "Error parsing field: '" << nameSlice.stringView() << "'. "
                << "Expansion is allowed only once.";
            errorField = fieldsFieldName + "[" +
                         basics::StringUtils::itoa(itr.index()) + "]";
            return false;
          }
        } catch (arangodb::basics::Exception const& err) {
          LOG_TOPIC("84c20", ERR, iresearch::TOPIC)
              << "Error parsing attribute: " << err.what();
          errorField = fieldsFieldName + "[" +
                       basics::StringUtils::itoa(itr.index()) + "]";
          return false;
        }
        // and now set analyzers to the last one
        if (val.hasKey("analyzer")) {
          auto analyzerSlice = val.get("analyzer");
          if (analyzerSlice.isString()) {
            auto name = analyzerSlice.copyString();
            auto shortName = name;
            if (!defaultVocbase.null()) {
              name = IResearchAnalyzerFeature::normalize(name, defaultVocbase);
              shortName = IResearchAnalyzerFeature::normalize(
                  name, defaultVocbase, false);
            }
            AnalyzerPool::ptr analyzer;
            bool found = false;
            if (!_analyzerDefinitions.empty()) {
              auto it = _analyzerDefinitions.find(irs::string_ref(name));

              if (it != _analyzerDefinitions.end()) {
                analyzer = *it;
                found = static_cast<bool>(analyzer);

                if (ADB_UNLIKELY(!found)) {
                  TRI_ASSERT(false);               // should not happen
                  _analyzerDefinitions.erase(it);  // remove null analyzer
                }
              }
            }
            if (!found) {
              // for cluster only check cache to avoid ClusterInfo locking
              // issues analyzer should have been populated via
              // 'analyzerDefinitions' above
              analyzer =
                  analyzers.get(name, QueryAnalyzerRevisions::QUERY_LATEST,
                                ServerState::instance()->isClusterRole());
            }
            if (!analyzer) {
              errorField = fieldsFieldName + "[" +
                           basics::StringUtils::itoa(itr.index()) + "]" +
                           ".analyzer";
              LOG_TOPIC("2d79d", ERR, iresearch::TOPIC)
                  << "Error loading analyzer '" << name << "' requested in "
                  << errorField;
              return false;
            }
            if (!found) {
              // save in referencedAnalyzers
              _analyzerDefinitions.emplace(analyzer);
            }
            _fields.emplace_back(
                std::move(fieldParts),
                FieldMeta::Analyzer(analyzer, std::move(shortName)));
          } else {
            errorField = fieldsFieldName + "[" +
                         basics::StringUtils::itoa(itr.index()) + "]" +
                         ".analyzer";
            return false;
          }
        } else {
          _fields.emplace_back(
              std::move(fieldParts),
              FieldMeta::Analyzer(
                  IResearchAnalyzerFeature::identity(),
                  std::string(IResearchAnalyzerFeature::identity()->name())));
        }
      } else {
        errorField = fieldsFieldName + "[" +
                     basics::StringUtils::itoa(itr.index()) + "]";
        return false;
      }
    } else {
      errorField =
          fieldsFieldName + "[" + basics::StringUtils::itoa(itr.index()) + "]";
      return false;
    }
  }
  return true;
}

bool InvertedIndexFieldMeta::json(
    arangodb::application_features::ApplicationServer& server,
    arangodb::velocypack::Builder& builder, bool writeAnalyzerDefinition,
    TRI_vocbase_t const* defaultVocbase /*= nullptr*/) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  // output definitions if 'writeAnalyzerDefinition' requested and not maked
  // this should be the case for the default top-most call
  if (writeAnalyzerDefinition) {
    VPackArrayBuilder arrayScope(&builder, "analyzerDefinitions");

    for (auto& entry : _analyzerDefinitions) {
      TRI_ASSERT(entry);  // ensured by emplace into 'analyzers' above
      entry->toVelocyPack(builder, defaultVocbase);
    }
  }
  velocypack::Builder fieldsBuilder;
  {
    VPackArrayBuilder fieldsArray(&fieldsBuilder);
    TRI_ASSERT(!_fields.empty());
    for (auto& entry : _fields) {
      VPackObjectBuilder fieldObject(&fieldsBuilder);
      fieldsBuilder.add("name", VPackValue(entry.toString()));
      fieldsBuilder.add("analyzer", VPackValue(entry.analyzer._pool->name()));
    }
  }
  builder.add(arangodb::StaticStrings::IndexFields, fieldsBuilder.slice());
  return true;
}

bool InvertedIndexFieldMeta::hasExtra() const noexcept {
  return std::numeric_limits<size_t>::max() != extraFieldsIdx();
}

size_t InvertedIndexFieldMeta::extraFieldsIdx() const noexcept {
  if (!_storedValues.empty()) {
    size_t idx{0};
    for (auto const& f : _storedValues.columns()) {
      bool haveFrom{false};
      bool haveTo{false};
      for (auto const& s : f.fields) {
        haveFrom |= s.first == "_from";
        haveTo |= s.first == "_to";
        if (haveFrom && haveTo) {
          return idx;
        }
      }
      ++idx;
    }
  }
  return std::numeric_limits<size_t>::max();
}

bool InvertedIndexFieldMeta::operator==(
    InvertedIndexFieldMeta const& other) const noexcept {
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

  if (_fields.size() != other._fields.size()) {
    return false;
  }

  size_t matched{0};
  for (auto const& thisField : _fields) {
    for (auto const& otherField : _fields) {
      TRI_ASSERT(thisField.analyzer._pool);
      TRI_ASSERT(otherField.analyzer._pool);
      if (thisField.analyzer._pool->name() ==
              otherField.analyzer._pool->name() &&
          basics::AttributeName::namesMatch(thisField.attribute,
                                            otherField.attribute) &&
          basics::AttributeName::namesMatch(thisField.expansion,
                                            otherField.expansion)) {
        matched++;
        break;
      }
    }
  }
  return matched == _fields.size();
}

bool InvertedIndexFieldMeta::matchesFieldsDefinition(
    InvertedIndexFieldMeta const& meta, VPackSlice other) {
  auto value = other.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  auto const count = meta._fields.size();
  if (n != count) {
    return false;
  }

  // Order of fields does not matter
  std::vector<arangodb::basics::AttributeName> translate;
  size_t matched{0};
  for (auto fieldSlice : VPackArrayIterator(value)) {
    TRI_ASSERT(fieldSlice.isObject());  // We expect only normalized definitions
                                        // here. Otherwise we will need vocbase
                                        // to properly match analyzers.
    if (ADB_UNLIKELY(!fieldSlice.isObject())) {
      return false;
    }

    auto name = fieldSlice.get("name");
    auto analyzer = fieldSlice.get("analyzer");
    TRI_ASSERT(
        name.isString() &&     // We expect only normalized definitions here.
        analyzer.isString());  // Otherwise we will need vocbase to properly
                               // match analyzers.
    if (ADB_UNLIKELY(!name.isString() || !analyzer.isString())) {
      return false;
    }

    auto in = name.stringRef();
    irs::string_ref analyzerName = analyzer.stringView();
    TRI_ParseAttributeString(in, translate, true);
    for (auto const& f : meta._fields) {
      if (f.isIdentical(translate, analyzerName)) {
        matched++;
        break;
      }
    }
    translate.clear();
  }
  return matched == count;
}

InvertedIndexFieldMeta::FieldRecord::FieldRecord(
    std::vector<basics::AttributeName> const& path, FieldMeta::Analyzer&& a)
    : analyzer(std::move(a)) {
  auto it = path.begin();
  while (it != path.end()) {
    attribute.push_back(*it);
    if (it->shouldExpand) {
      ++it;
      break;
    }
    ++it;
  }
  while (it != path.end()) {
    TRI_ASSERT(
        !it->shouldExpand);  // only one expansion per attribute is supported!
    expansion.push_back(*it);
    ++it;
  }
}

std::string InvertedIndexFieldMeta::FieldRecord::toString() const {
  std::string attr;
  TRI_AttributeNamesToString(attribute, attr, false);
  if (!expansion.empty()) {
    attr += ".";
    std::string exp;
    TRI_AttributeNamesToString(expansion, exp, true);
    attr += exp;
  }
  return attr;
}

bool InvertedIndexFieldMeta::FieldRecord::isIdentical(
    std::vector<basics::AttributeName> const& path,
    irs::string_ref analyzerName) const noexcept {
  if (analyzer._shortName == analyzerName &&
      path.size() == (attribute.size() + expansion.size())) {
    auto it = path.begin();
    auto atr = attribute.begin();
    while (it != path.end() && atr != attribute.end()) {
      if (it->name != atr->name || it->shouldExpand != atr->shouldExpand) {
        return false;
      }
      ++atr;
      ++it;
    }
    auto exp = expansion.begin();
    while (it != path.end() && exp != expansion.end()) {
      if (it->name != exp->name || it->shouldExpand != exp->shouldExpand) {
        return false;
      }
      ++exp;
      ++it;
    }
  }
  return false;
}
}  // namespace iresearch
}  // namespace arangodb
