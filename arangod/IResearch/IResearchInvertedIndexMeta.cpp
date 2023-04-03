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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchInvertedIndexMeta.h"

#include "frozen/map.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchKludge.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VelocyPackHelper.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>

namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

constexpr auto consistencyTypeMap =
    frozen::map<std::string_view, Consistency, 2>(
        {{"eventual", Consistency::kEventual},
         {"immediate", Consistency::kImmediate}});

constexpr std::string_view kNameFieldName = "name";
constexpr std::string_view kAnalyzerFieldName = "analyzer";
constexpr std::string_view kNestedFieldsFieldName = "nested";
constexpr std::string_view kFeaturesFieldName = "features";
constexpr std::string_view kExpressionFieldName = "expression";
constexpr std::string_view kIsArrayFieldName = "isArray";
constexpr std::string_view kIncludeAllFieldsFieldName = "includeAllFields";
constexpr std::string_view kTrackListPositionsFieldName = "trackListPositions";
constexpr std::string_view kFieldName = "field";
constexpr std::string_view kFieldsFieldName = "fields";
constexpr std::string_view kCompressionFieldName = "compression";
constexpr std::string_view kLocaleFieldName = "locale";
constexpr std::string_view kOverrideFieldName = "override";
constexpr std::string_view kPrimarySortFieldName = "primarySort";
constexpr std::string_view kVersionFieldName = "version";
constexpr std::string_view kStoredValuesFieldName = "storedValues";
constexpr std::string_view kConsistencyFieldName = "consistency";
constexpr std::string_view kAnalyzerDefinitionsFieldName =
    "analyzerDefinitions";
constexpr std::string_view kIsSearchField = "searchField";

#ifdef USE_ENTERPRISE
void gatherNestedNulls(std::string_view parent,
                       arangodb::iresearch::MissingFieldsMap& map,
                       arangodb::iresearch::InvertedIndexField const& field) {
  std::string nestedObjectsPath(field.path());
  std::string self;
  arangodb::iresearch::kludge::mangleNested(self);
  self += parent;
  if (!parent.empty()) {
    arangodb::iresearch::kludge::mangleNested(self);
  }
  map[self].emplace(field.path());
  arangodb::iresearch::kludge::mangleNested(nestedObjectsPath);
  for (auto const& sf : field._fields) {
    if (!sf._fields.empty()) {
      gatherNestedNulls(field.path(), map, sf);
    }
    map[nestedObjectsPath].emplace(sf.path());
  }
}
#endif

arangodb::iresearch::MissingFieldsMap gatherMissingFields(
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext const&
        field) {
  arangodb::iresearch::MissingFieldsMap map;
  for (auto const& f : field._meta->_fields) {
    // always monitor on root level plain fields to track completely missing
    // hierarchies. trackListPositions enabled arrays are excluded as we could
    // never predict if array[12345] will exist so we do not emit such "nulls".
    // It is not supported in general indexes anyway
    if ((!field._trackListPositions || !field._meta->_hasExpansion) &&
        f._fields.empty()) {
      map[""].emplace(f._attribute.back().shouldExpand ? f.attributeString()
                                                       : f.path());
    }
    // but for individual objects in array we always could track expected fields
    // and emit "nulls"
    if (f._hasExpansion && !f._attribute.back().shouldExpand) {
      TRI_ASSERT(f._fields.empty());
      // monitor array subobjects
      map[f.attributeString()].emplace(f.path());
    }
#ifdef USE_ENTERPRISE
    if (!f._fields.empty()) {
      gatherNestedNulls("", map, f);
    }
#endif
  }
  return map;
}
}  // namespace

namespace arangodb::iresearch {

const IResearchInvertedIndexMeta& IResearchInvertedIndexMeta::DEFAULT() {
  static const IResearchInvertedIndexMeta meta{};
  return meta;
}

IResearchInvertedIndexMeta::IResearchInvertedIndexMeta() {
  _analyzers[0] = FieldMeta::identity();
  _primitiveOffset = _analyzers.size();
  _indexingContext =
      std::make_unique<IResearchInvertedIndexMetaIndexingContext>(this, false);
}

// FIXME(Dronplane): make all constexpr defines consistent
bool IResearchInvertedIndexMeta::init(arangodb::ArangodServer& server,
                                      VPackSlice const& slice,
                                      bool readAnalyzerDefinition,
                                      std::string& errorField,
                                      std::string_view const defaultVocbase) {
  if (!IResearchDataStoreMeta::init(slice, errorField, DEFAULT(), nullptr)) {
    return false;
  }

  if (ServerState::instance()->isDBServer()) {
    auto collectionName = slice.get(StaticStrings::CollectionNameField);
    if (collectionName.isString()) {
      _collectionName = collectionName.stringView();
    } else if (!collectionName.isNone()) {
      errorField = StaticStrings::CollectionNameField;
      return false;
    }
  }

  // consistency (optional)
  {
    auto consistencySlice = slice.get(kConsistencyFieldName);
    if (!consistencySlice.isNone()) {
      if (consistencySlice.isString()) {
        auto it = consistencyTypeMap.find(consistencySlice.stringView());
        if (it != consistencyTypeMap.end()) {
          _consistency = it->second;
        } else {
          errorField = kConsistencyFieldName;
          return false;
        }
      } else {
        errorField = kConsistencyFieldName;
        return false;
      }
    }
  }
  {
    // optional stored values
    auto const field = slice.get(kStoredValuesFieldName);
    if (!field.isNone() && !_storedValues.fromVelocyPack(field, errorField)) {
      errorField = kStoredValuesFieldName;
      return false;
    }
  }
  {
    // optional primarySort
    auto const field = slice.get(kPrimarySortFieldName);
    if (!field.isNone() && !_sort.fromVelocyPack(field, errorField)) {
      errorField = kPrimarySortFieldName;
      return false;
    }
  }
  {
    // optional version
    auto const field = slice.get(kVersionFieldName);
    if (field.isNumber()) {
      auto version = field.getNumber<uint32_t>();
      if (version > static_cast<uint32_t>(LinkVersion::MAX)) {
        errorField = kVersionFieldName;
        return false;
      }
      _version = static_cast<uint32_t>(static_cast<LinkVersion>(version));
    } else if (field.isNone()) {
      // not present -> last version
      _version = static_cast<uint32_t>(LinkVersion::MAX);
    } else {
      errorField = kVersionFieldName;
      return false;
    }
  }
  bool const extendedNames =
      server.getFeature<DatabaseFeature>().extendedNamesForAnalyzers();

  auto const& identity = *IResearchAnalyzerFeature::identity();
  AnalyzerPool::ptr versionSpecificIdentity;

  auto const res = IResearchAnalyzerFeature::copyAnalyzerPool(
      versionSpecificIdentity, identity, static_cast<LinkVersion>(_version),
      extendedNames);

  if (res.fail() || !versionSpecificIdentity) {
    TRI_ASSERT(false);
    return false;
  }
  // replace default with version specific default
  _analyzers[0] = std::move(versionSpecificIdentity);

  {
    // clear existing definitions
    _analyzerDefinitions.clear();

    // optional object list
    // load analyzer definitions if requested (used on cluster)
    // @note must load definitions before loading 'analyzers' to ensure presence
    if (velocypack::Slice field;
        readAnalyzerDefinition &&
        !(field = slice.get(kAnalyzerDefinitionsFieldName)).isNone()) {
      if (!field.isArray()) {
        errorField = kAnalyzerDefinitionsFieldName;
        return false;
      }

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isObject()) {
          errorField = absl::StrCat(kAnalyzerDefinitionsFieldName, "[",
                                    itr.index(), "]");
          return false;
        }

        std::string name;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"name"};

          auto nameSlice = value.get(kSubFieldName);
          if (!nameSlice.isString()) {
            errorField = absl::StrCat(kAnalyzerDefinitionsFieldName, "[",
                                      itr.index(), "].", kSubFieldName);
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
            errorField = absl::StrCat(kAnalyzerDefinitionsFieldName, "[",
                                      itr.index(), "].", kSubFieldName);
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
              errorField = absl::StrCat(kAnalyzerDefinitionsFieldName, "[",
                                        itr.index(), "].", kSubFieldName);
              return false;
            }

            properties = subField;
          }
        }
        Features features;
        {
          // optional string list
          constexpr std::string_view kSubFieldName{"features"};

          auto subField = value.get(kSubFieldName);
          if (!subField.isNone()) {
            auto featuresRes = features.fromVelocyPack(subField);
            if (featuresRes.fail()) {
              errorField = absl::StrCat(kAnalyzerDefinitionsFieldName, " (",
                                        featuresRes.errorMessage(), ")");
              return false;
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
        auto const res = IResearchAnalyzerFeature::createAnalyzerPool(
            analyzer, name, type, properties, revision, features,
            static_cast<LinkVersion>(_version), extendedNames);

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
  auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();

#ifdef USE_ENTERPRISE
  {
    auto const pkCacheSlice = slice.get(StaticStrings::kCachePrimaryKeyField);
    if (!pkCacheSlice.isNone()) {
      if (!pkCacheSlice.isBool()) {
        errorField = StaticStrings::kPrimarySortCacheField;
        return false;
      }
      _pkCache = pkCacheSlice.getBool();
    }
  }
  {
    auto optimizeTopKSlice = slice.get(StaticStrings::kOptimizeTopKField);
    if (!optimizeTopKSlice.isNone()) {
      std::string err;
      if (!_optimizeTopK.fromVelocyPack(optimizeTopKSlice, err)) {
        errorField = absl::StrCat(StaticStrings::kOptimizeTopKField, ": ", err);
        return false;
      }
    }
  }
#endif

  if (!InvertedIndexField::init(
          slice, _analyzerDefinitions, static_cast<LinkVersion>(_version),
          extendedNames, analyzers, *this, defaultVocbase, true, errorField)) {
    return false;
  }
  _hasNested = std::find_if(_fields.begin(), _fields.end(),
                            [](InvertedIndexField const& r) {
                              return !r._fields.empty();
                            }) != _fields.end();

  _indexingContext =
      std::make_unique<IResearchInvertedIndexMetaIndexingContext>(this, true);
  // only root level need to have missing fields map
  _indexingContext->_missingFieldsMap = gatherMissingFields(*_indexingContext);
  return true;
}

bool IResearchInvertedIndexMeta::json(
    arangodb::ArangodServer& server, VPackBuilder& builder,
    bool writeAnalyzerDefinition,
    TRI_vocbase_t const* defaultVocbase /*= nullptr*/) const {
  if (!IResearchDataStoreMeta::json(builder)) {
    return false;
  }

  if (!builder.isOpenObject()) {
    return false;
  }

  // output definitions if 'writeAnalyzerDefinition' requested and not maked
  // this should be the case for the default top-most call
  if (writeAnalyzerDefinition) {
    VPackArrayBuilder arrayScope(&builder, kAnalyzerDefinitionsFieldName);

    for (auto& entry : _analyzerDefinitions) {
      TRI_ASSERT(entry);  // ensured by emplace into 'analyzers' above
      entry->toVelocyPack(builder, defaultVocbase);
    }
  }

  {
    VPackObjectBuilder obj(&builder, kPrimarySortFieldName);
    if (!_sort.toVelocyPack(builder)) {
      return false;
    }
  }

  {
    VPackArrayBuilder arr(&builder, kStoredValuesFieldName);
    if (!_storedValues.toVelocyPack(builder)) {
      return false;
    }
  }

  // FIXME: Uncomment once support is done
  // for (auto const& it : consistencyTypeMap) {
  //  if (it.second == _consistency) {
  //    builder.add(kConsistencyFieldName, VPackValue(it.first));
  //  }
  //}

  if (writeAnalyzerDefinition && ServerState::instance()->isDBServer() &&
      !_collectionName.empty()) {
    builder.add(StaticStrings::CollectionNameField,
                velocypack::Value{_collectionName});
  }

#ifdef USE_ENTERPRISE
  if (_pkCache) {
    builder.add(StaticStrings::kCachePrimaryKeyField, _pkCache);
  }
  {
    VPackArrayBuilder arrayScope(&builder, StaticStrings::kOptimizeTopKField);
    _optimizeTopK.toVelocyPack(builder);
  }
#endif

  return InvertedIndexField::json(server, builder, *this, true, defaultVocbase);
}

bool IResearchInvertedIndexMeta::operator==(
    IResearchInvertedIndexMeta const& other) const noexcept {
  return _consistency == other._consistency &&
         (static_cast<IResearchDataStoreMeta const&>(*this) ==
          static_cast<IResearchDataStoreMeta const&>(other)) &&
         (static_cast<InvertedIndexField const&>(*this) ==
          static_cast<InvertedIndexField const&>(other)) &&
         _sort == other._sort && _storedValues == other._storedValues;
}

bool IResearchInvertedIndexMeta::matchesDefinition(
    IResearchInvertedIndexMeta const& meta, VPackSlice other,
    TRI_vocbase_t const& vocbase) {
  auto value = other.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  auto const count = meta._fields.size();
  if (n != count) {
    return false;
  }

  IResearchInvertedIndexMeta otherMeta;
  std::string errorField;
  return otherMeta.init(vocbase.server(), other, true, errorField,
                        vocbase.name()) &&
         meta == otherMeta;
}

bool InvertedIndexField::json(
    arangodb::ArangodServer& server, VPackBuilder& builder,
    InvertedIndexField const& parent, bool rootMode,
    TRI_vocbase_t const* defaultVocbase /*= nullptr*/) const {
  // FIXME: uncomment once parameter is supported
  // if (rootMode || parent._isArray != _isArray) {
  //  builder.add(kIsArrayFieldName, VPackValue(_isArray));
  //}
  if (rootMode || parent._trackListPositions != _trackListPositions) {
    builder.add(kTrackListPositionsFieldName, VPackValue(_trackListPositions));
  }
  if (rootMode || parent._includeAllFields != _includeAllFields) {
    builder.add(kIncludeAllFieldsFieldName, VPackValue(_includeAllFields));
  }
  // FIXME: uncomment once parameter is supported
  // if (rootMode || parent._overrideValue != _overrideValue) {
  //  builder.add(kOverrideFieldName, VPackValue(_overrideValue));
  //}
  if (rootMode || parent._features != _features) {
    VPackBuilder tmp;
    _features.toVelocyPack(tmp);
    builder.add(kFeaturesFieldName, tmp.slice());
  }
  // FIXME: uncomment once parameter is supported
  // if (parent._expression != _expression) {
  //  builder.add(kExpressionFieldName, VPackValue(_expression));
  //}

  if (rootMode || _analyzers.size() != parent._analyzers.size() ||
      _analyzers[0]._pool->name() != parent._analyzers[0]._pool->name()) {
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
      name = IResearchAnalyzerFeature::normalize(_analyzers[0]._pool->name(),
                                                 defaultVocbase->name(), false);
    } else {
      // verbatim (assume already normalized)
      name = _analyzers[0]._pool->name();
    }

    builder.add(kAnalyzerFieldName, VPackValue(name));
  }

  if (!rootMode) {
    builder.add(kNameFieldName, VPackValue(toString()));
  }

  if (rootMode || parent._isSearchField != _isSearchField) {
    builder.add(kIsSearchField, VPackValue(_isSearchField));
  }

#ifdef USE_ENTERPRISE
  if ((rootMode && _cache) || (!rootMode && _cache != parent._cache)) {
    builder.add(StaticStrings::kCacheField, velocypack::Value(_cache));
  }
#endif

  if (!_fields.empty() || rootMode) {
    auto fieldsAttributeName =
        rootMode ? kFieldsFieldName : kNestedFieldsFieldName;
    VPackArrayBuilder nestedArray(&builder, fieldsAttributeName);
    for (auto const& f : _fields) {
      VPackObjectBuilder obj(&builder);
      if (!f.json(server, builder, *this, false, defaultVocbase)) {
        return false;
      }
    }
  }
  return true;
}

bool InvertedIndexField::init(
    VPackSlice slice,
    InvertedIndexField::AnalyzerDefinitions& analyzerDefinitions,
    LinkVersion version, bool extendedNames,
    IResearchAnalyzerFeature& analyzers, InvertedIndexField const& parent,
    std::string_view const defaultVocbase, bool rootMode,
    std::string& errorField) {
  // Fill inherited fields
  if (!rootMode) {
    _includeAllFields = parent._includeAllFields;
    _trackListPositions = parent._trackListPositions;
    _isArray = parent._isArray;
    _overrideValue = parent._overrideValue;
    _expression = parent._expression;
    _isSearchField = parent._isSearchField;
#ifdef USE_ENTERPRISE
    _cache = parent._cache;
#endif
  }
  std::vector<basics::AttributeName> fieldParts;
  if (slice.isString()) {
    TRI_ASSERT(!rootMode);
    try {
      TRI_ASSERT(!parent._analyzers.empty());
      _analyzers[0] = parent.analyzer();
      _features = parent._features;
      TRI_ParseAttributeString(slice.stringView(), fieldParts, !_isSearchField);
      TRI_ASSERT(!fieldParts.empty());
    } catch (arangodb::basics::Exception const& err) {
      LOG_TOPIC("1d04c", ERR, arangodb::iresearch::TOPIC)
          << "Error parsing attribute: " << err.what();
      errorField = slice.stringView();
      return false;
    }
  } else if (slice.isObject()) {
#ifdef USE_ENTERPRISE
    {
      auto cacheSlice = slice.get(StaticStrings::kCacheField);
      if (!cacheSlice.isNone()) {
        if (!cacheSlice.isBool()) {
          errorField = StaticStrings::kCacheField;
          return false;
        }
        _cache = cacheSlice.getBool();
      }
    }
#endif
    if (auto value = slice.get(kIsSearchField); !value.isNone()) {
      if (value.isBoolean()) {
        _isSearchField = value.getBoolean();
      } else {
        LOG_TOPIC("1d04d", ERR, arangodb::iresearch::TOPIC)
            << "Error parsing attribute: " << kIsSearchField;
        errorField = kIsSearchField;
        return false;
      }
    }
    if (!rootMode) {
      // name attribute
      auto nameSlice = slice.get(kNameFieldName);
      if (!nameSlice.isString()) {
        errorField = kNameFieldName;
        return false;
      }
      try {
        TRI_ParseAttributeString(nameSlice.stringView(), fieldParts,
                                 !_isSearchField);
        TRI_ASSERT(!fieldParts.empty());
      } catch (arangodb::basics::Exception const& err) {
        LOG_TOPIC("84c20", ERR, arangodb::iresearch::TOPIC)
            << "Error parsing attribute: " << err.what();
        errorField = kNameFieldName;
        return false;
      }
    }

    if (auto analyzerSlice = slice.get(kAnalyzerFieldName);
        !analyzerSlice.isNone()) {
      AnalyzerPool::ptr analyzer;
      if (analyzerSlice.isString()) {
        std::string name{analyzerSlice.stringView()};
        auto shortName = name;
        if (!irs::IsNull(defaultVocbase)) {
          name = IResearchAnalyzerFeature::normalize(name, defaultVocbase);
          shortName =
              IResearchAnalyzerFeature::normalize(name, defaultVocbase, false);
        }

        bool found = false;
        if (!analyzerDefinitions.empty()) {
          auto it = analyzerDefinitions.find(std::string_view(name));

          if (it != analyzerDefinitions.end()) {
            analyzer = *it;
            found = static_cast<bool>(analyzer);

            if (ADB_UNLIKELY(!found)) {
              TRI_ASSERT(false);              // should not happen
              analyzerDefinitions.erase(it);  // remove null analyzer
            }
          }
        }
        if (!found) {
          // for cluster only check cache to avoid ClusterInfo locking
          // issues analyzer should have been populated via
          // 'analyzerDefinitions' above
          analyzer = analyzers.get(name, QueryAnalyzerRevisions::QUERY_LATEST,
                                   ServerState::instance()->isClusterRole());
          if (analyzer) {
            // Remap analyzer features to match version.
            AnalyzerPool::ptr remappedAnalyzer;

            auto const res = IResearchAnalyzerFeature::copyAnalyzerPool(
                remappedAnalyzer, *analyzer, LinkVersion{version},
                extendedNames);

            LOG_TOPIC_IF("2d81d", ERR, arangodb::iresearch::TOPIC, res.fail())
                << "Error remapping analyzer '" << name
                << "' Error:" << res.errorMessage();
            analyzer = remappedAnalyzer;
          }
        }
        if (!analyzer) {
          errorField = kAnalyzerFieldName;
          LOG_TOPIC("2d79d", ERR, arangodb::iresearch::TOPIC)
              << "Error loading analyzer '" << name << "'";
          return false;
        }
        // save in referencedAnalyzers
        analyzerDefinitions.emplace(analyzer);
        _analyzers[0] =
            FieldMeta::Analyzer(std::move(analyzer), std::move(shortName));
      } else {
        errorField = kAnalyzerFieldName;
        return false;
      }
    } else {
      auto& analyzer = parent.analyzer();
      if (!rootMode) {
        _analyzers[0] = analyzer;
      }
      analyzerDefinitions.emplace(analyzer._pool);
    }
    if (auto v = slice.get(kFeaturesFieldName); !v.isNone()) {
      Features tmp;
      auto featuresRes = tmp.fromVelocyPack(v);
      if (featuresRes.fail()) {
        errorField = kFeaturesFieldName;
        LOG_TOPIC("2d52d", ERR, arangodb::iresearch::TOPIC)
            << "Error parsing features " << featuresRes.errorMessage();
        return false;
      }
      _features = std::move(tmp);
    } else if (rootMode || !slice.get(kAnalyzerFieldName).isNone()) {
      TRI_ASSERT(_analyzers[0]);
      _features = _analyzers[0]._pool->features();
    } else {
      _features = parent._features;
    }
    if (velocypack::Slice subSlice;
        !rootMode && !(subSlice = slice.get(kExpressionFieldName)).isNone()) {
      if (!subSlice.isString()) {
        errorField = kExpressionFieldName;
        return false;
      }
      _expression = subSlice.stringView();
    }
    if (auto subSlice = slice.get(kIsArrayFieldName); !subSlice.isNone()) {
      if (!subSlice.isBool()) {
        errorField = kIsArrayFieldName;
        return false;
      }
      _isArray = subSlice.getBool();
    }
    if (auto subSlice = slice.get(kTrackListPositionsFieldName);
        !subSlice.isNone()) {
      if (!subSlice.isBool()) {
        errorField = kTrackListPositionsFieldName;
        return false;
      }
      _trackListPositions = subSlice.getBool();
    }
    if (auto subSlice = slice.get(kIncludeAllFieldsFieldName);
        !subSlice.isNone()) {
      if (!subSlice.isBool()) {
        errorField = kIncludeAllFieldsFieldName;
        return false;
      }
      _includeAllFields = subSlice.getBool();
    }
    if (auto subSlice = slice.get(kOverrideFieldName); !subSlice.isNone()) {
      if (!subSlice.isBoolean()) {
        errorField = kOverrideFieldName;
        return false;
      }
      _overrideValue = subSlice.getBoolean();
    }
  } else {
    errorField = "<String or object expected>";
    return false;
  }
  _primitiveOffset =
      !analyzer()->accepts(AnalyzerValueType::Array | AnalyzerValueType::Object)
          ? 1
          : 0;

  if (!rootMode) {
    // we only allow one expansion
    size_t expansionCount{0};
    std::for_each(fieldParts.begin(), fieldParts.end(),
                  [&expansionCount](basics::AttributeName const& a) -> void {
                    if (a.shouldExpand) {
                      ++expansionCount;
                    }
                  });
    if (expansionCount > 1 && parent._attribute.empty()) {
      LOG_TOPIC("2646b", ERR, arangodb::iresearch::TOPIC)
          << "Error parsing field: '" << kNameFieldName << "'. "
          << "Expansion is allowed only once.";
      errorField = kNameFieldName;
      return false;
    } else if (expansionCount > 0 && !parent._attribute.empty()) {
      LOG_TOPIC("2646d", ERR, arangodb::iresearch::TOPIC)
          << "Error parsing field: '" << kNameFieldName << "'. "
          << "Expansion is not allowed for nested fields.";
      errorField = kNameFieldName;
      return false;
    }
    _hasExpansion = expansionCount != 0;
    _attribute = std::move(fieldParts);
    bool isFirst = true;
    _attributeName.clear();
    for (auto const& it : _attribute) {
      if (!isFirst) {
        _attributeName += ".";
      }
      isFirst = false;
      _attributeName += it.name;
      if (it.shouldExpand) {
        break;
      }
    }
    _path = parent.path();
#ifdef USE_ENTERPRISE
    if (!_path.empty()) {
      kludge::mangleNested(_path);
      _path += ".";
    }
#endif
    std::string tmp;
    // last expansion is not emitted as field name in the index!
    TRI_AttributeNamesToString(_attribute, tmp, _attribute.back().shouldExpand);
    _path += tmp;
  }

#ifndef USE_ENTERPRISE
  if (!rootMode) {
    if (slice.isObject() && !slice.get(kNestedFieldsFieldName).isNone()) {
      errorField =
          absl::StrCat(kNestedFieldsFieldName,
                       " is supported in ArangoDB Enterprise Edition only.");
      return false;
    }

    return true;
  }
#endif
  auto const fieldsAttributeName =
      rootMode ? kFieldsFieldName : kNestedFieldsFieldName;

  velocypack::Slice nestedSlice;
  if (slice.isObject() &&
      !(nestedSlice = slice.get(fieldsAttributeName)).isNone()) {
    if (!nestedSlice.isArray()) {
      errorField = fieldsAttributeName;
      return false;
    }
    if (!rootMode && _trackListPositions) {
      if (!slice.get(kTrackListPositionsFieldName).isNone()) {
        // explicit track list positions is forbidden
        // if nested fields are present
        errorField = kTrackListPositionsFieldName;
        return false;
      }
      // implicit is just disabled
      _trackListPositions = false;
    }
    if (_hasExpansion) {
      errorField = kNameFieldName;
      return false;
    }
    std::string localError;
    containers::FlatHashSet<std::string> fieldsDeduplicator;
    for (auto it = VPackArrayIterator(nestedSlice); it.valid(); ++it) {
      InvertedIndexField nested;
      if (nested.init(it.value(), analyzerDefinitions, version, extendedNames,
                      analyzers, *this, defaultVocbase, false, localError)) {
        if (!fieldsDeduplicator.emplace(nested.path()).second) {
          errorField = absl::StrCat(fieldsAttributeName, "[", it.index(), "]",
                                    " is duplicated");
          return false;
        }
        _fields.push_back(std::move(nested));
      } else {
        errorField = absl::StrCat(fieldsAttributeName, "[", it.index(), "].",
                                  localError);
        return false;
      }
    }
  }
  if (rootMode && _fields.empty() && !_includeAllFields) {
    errorField = kFieldsFieldName;
    return false;
  }
  return true;
}

std::string InvertedIndexField::toString() const {
  std::string attr;
  TRI_AttributeNamesToString(_attribute, attr, false);
  return attr;
}

std::string_view InvertedIndexField::attributeString() const {
  return _attributeName;
}

std::string_view InvertedIndexField::path() const noexcept { return _path; }

bool InvertedIndexField::operator==(
    InvertedIndexField const& other) const noexcept {
  if (!(analyzerName() == other.analyzerName() &&
        basics::AttributeName::namesMatch(_attribute, other._attribute) &&
        _includeAllFields == other._includeAllFields &&
        _trackListPositions == other._trackListPositions &&
        _features == other._features && _isArray == other._isArray &&
        _overrideValue == other._overrideValue &&
        _expression == other._expression &&
        _isSearchField == other._isSearchField &&
#ifdef USE_ENTERPRISE
        _cache == other._cache &&
#endif
        _fields.size() == other._fields.size())) {
    return false;
  }
  for (auto const& otherField : other._fields) {
    if (std::end(_fields) ==
        std::find(std::begin(_fields), std::end(_fields), otherField)) {
      return false;
    }
  }
  return true;
}

bool IResearchInvertedIndexSort::toVelocyPack(
    velocypack::Builder& builder) const {
  if (!builder.isOpenObject()) {
    return false;
  }
  {
    VPackArrayBuilder arrayScope(&builder, kFieldsFieldName);
    if (!IResearchSortBase::toVelocyPack(builder)) {
      return false;
    }
  }

  {
    auto compression = columnCompressionToString(_sortCompression);
    addStringRef(builder, kCompressionFieldName, compression);
  }

  // FIXME: Uncomment once support is done
  // if (!_locale.isBogus()) {
  //  builder.add(kLocaleFieldName, VPackValue(_locale.getName()));
  //}
#ifdef USE_ENTERPRISE
  if (_cache) {
    builder.add(StaticStrings::kCacheField, _cache);
  }
#endif
  return true;
}

bool IResearchInvertedIndexSort::fromVelocyPack(velocypack::Slice slice,
                                                std::string& error) {
  clear();
  if (!slice.isObject()) {
    return false;
  }

  auto fieldsSlice = slice.get(kFieldsFieldName);
  if (!fieldsSlice.isArray()) {
    error = kFieldsFieldName;
    return false;
  }
  if (!IResearchSortBase::fromVelocyPack(fieldsSlice, error)) {
    return false;
  }

  auto const compression = slice.get(kCompressionFieldName);
  if (!compression.isNone()) {
    if (!compression.isString()) {
      error = kCompressionFieldName;
      return false;
    }
    auto sort = columnCompressionFromString(compression.stringView());
    if (!sort) {
      error = kCompressionFieldName;
      return false;
    }
    _sortCompression = sort;
  }

  auto localeSlice = slice.get(kLocaleFieldName);
  if (!localeSlice.isNone()) {
    if (!localeSlice.isString()) {
      error = kLocaleFieldName;
      return false;
    }
    // intentional string copy here as createCanonical expects null-terminated
    // string and string_view has no such guarantees
    _locale = icu::Locale::createCanonical(
        std::string{localeSlice.stringView()}.c_str());
    if (_locale.isBogus()) {
      error = kLocaleFieldName;
      return false;
    }
  }
#ifdef USE_ENTERPRISE
  auto cacheSlice = slice.get(StaticStrings::kCacheField);
  if (!cacheSlice.isNone()) {
    if (!cacheSlice.isBool()) {
      error = StaticStrings::kCacheField;
      return false;
    }
    _cache = cacheSlice.getBool();
  }
#endif
  return true;
}

IResearchInvertedIndexMetaIndexingContext::
    IResearchInvertedIndexMetaIndexingContext(
        IResearchInvertedIndexMeta const* field, bool add)
    : _analyzers(&field->_analyzers),
      _primitiveOffset(field->_primitiveOffset),
      _meta(field),
      _sort(field->_sort),
      _storedValues(field->_storedValues),
      _hasNested(field->_hasNested),
      _includeAllFields(field->_includeAllFields),
      _trackListPositions(field->_trackListPositions),
#ifdef USE_ENTERPRISE
      _cache(field->_cache),
#endif
      _isSearchField(field->_isSearchField) {
  setFeatures(field->_features);
  if (add) {
    addField(*field, false);
  }
}

void IResearchInvertedIndexMetaIndexingContext::setFeatures(
    Features const& features) {
  _features = features;
  _fieldFeatures =
      _features.fieldFeatures(static_cast<LinkVersion>(_meta->_version));
}

void IResearchInvertedIndexMetaIndexingContext::addField(
    InvertedIndexField const& field, bool nested) {
  for (auto const& f : field._fields) {
    auto current = this;
    for (size_t i = 0; i < f._attribute.size(); ++i) {
      auto const& a = f._attribute[i];
      auto& fieldsContainer =
          nested && i == 0 ? current->_nested : current->_fields;
      auto emplaceRes = fieldsContainer.emplace(
          a.name, IResearchInvertedIndexMetaIndexingContext{_meta, false});

      if (!emplaceRes.second) {
        // first emplaced as nested root then array may come as regular field
        emplaceRes.first->second._isArray |= a.shouldExpand;
        current = &(emplaceRes.first->second);
        current->_hasNested |= !f._fields.empty();
      } else {
        current = &(emplaceRes.first->second);
        current->_isArray = a.shouldExpand;
        current->_hasNested = !f._fields.empty();
      }
      if (i == f._attribute.size() - 1) {
        current->_analyzers = &f._analyzers;
        current->_primitiveOffset = f._primitiveOffset;
        current->_includeAllFields = f._includeAllFields;
        current->_trackListPositions = f._trackListPositions;
        current->_isSearchField = f._isSearchField;
#ifdef USE_ENTERPRISE
        current->_cache = f._cache;
#endif
        current->setFeatures(f._features);
      }
    }
#ifdef USE_ENTERPRISE
    if (!f._fields.empty()) {
      current->_hasNested = true;
      current->addField(f, true);
    }
#endif
  }
}

std::string_view IResearchInvertedIndexMetaIndexingContext::collectionName()
    const noexcept {
  TRI_ASSERT(_meta);
  return _meta->_collectionName;
}

}  // namespace arangodb::iresearch
