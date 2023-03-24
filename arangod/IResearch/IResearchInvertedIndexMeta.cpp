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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchInvertedIndexMeta.h"

#include "frozen/map.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchKludge.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VelocyPackHelper.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "VocBase/vocbase.h"

namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

constexpr auto consistencyTypeMap =
    frozen::map<irs::string_ref, Consistency, 2>(
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
constexpr std::string_view kSortCompressionFieldName = "primarySortCompression";
constexpr std::string_view kLocaleFieldName = "locale";
constexpr std::string_view kOverrideFieldName = "override";
constexpr std::string_view kPrimarySortFieldName = "primarySort";
constexpr std::string_view kVersionFieldName = "version";
constexpr std::string_view kStoredValuesFieldName = "storedValues";
constexpr std::string_view kConsistencyFieldName = "consistency";
constexpr std::string_view kAnalyzerDefinitionsFieldName =
    "analyzerDefinitions";

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
  _analyzers[0] = IResearchAnalyzerFeature::identity();
  _primitiveOffset = 1;
  _indexingContext =
      std::make_unique<IResearchInvertedIndexMetaIndexingContext>(this, false);
}

// FIXME(Dronplane): make all constexpr defines consistent
bool IResearchInvertedIndexMeta::init(arangodb::ArangodServer& server,
                                      VPackSlice const& slice,
                                      bool readAnalyzerDefinition,
                                      std::string& errorField,
                                      irs::string_ref const defaultVocbase) {
  if (!IResearchDataStoreMeta::init(slice, errorField, DEFAULT(), nullptr)) {
    return false;
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
      _version = static_cast<LinkVersion>(version);
    } else if (field.isNone()) {
      _version = LinkVersion::MAX;  // not present -> last version
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
      versionSpecificIdentity, identity, _version, extendedNames);

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
    if (readAnalyzerDefinition && slice.hasKey(kAnalyzerDefinitionsFieldName)) {
      auto field = slice.get(kAnalyzerDefinitionsFieldName);

      if (!field.isArray()) {
        errorField = kAnalyzerDefinitionsFieldName;
        return false;
      }

      for (velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto value = *itr;

        if (!value.isObject()) {
          errorField = std::string{kAnalyzerDefinitionsFieldName} + "[" +
                       std::to_string(itr.index()) + "]";

          return false;
        }

        std::string name;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"name"};

          if (!value.hasKey(kSubFieldName)  // missing required filed
              || !value.get(kSubFieldName).isString()) {
            errorField = std::string{kAnalyzerDefinitionsFieldName} + "[" +
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
        std::string_view type;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"type"};
          auto typeSlice = value.get(kSubFieldName);

          if (!typeSlice.isString()) {
            errorField = std::string{kAnalyzerDefinitionsFieldName} + "[" +
                         std::to_string(itr.index()) + "]." +
                         std::string{kSubFieldName};
            return false;
          }

          type = typeSlice.stringView();
        }

        VPackSlice properties;

        {
          // optional string value
          constexpr std::string_view kSubFieldName{"properties"};

          if (value.hasKey(kSubFieldName)) {
            auto subField = value.get(kSubFieldName);

            if (!subField.isObject() && !subField.isNull()) {
              errorField = std::string{kAnalyzerDefinitionsFieldName} + "[" +
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
          constexpr std::string_view kSubFieldName{"features"};

          if (value.hasKey(kSubFieldName)) {
            auto subField = value.get(kSubFieldName);
            auto featuresRes = features.fromVelocyPack(subField);
            if (featuresRes.fail()) {
              errorField = std::string{kAnalyzerDefinitionsFieldName}
                               .append(" (")
                               .append(featuresRes.errorMessage())
                               .append(")");
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
            analyzer, name, type, properties, revision, features, _version,
            extendedNames);

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
  auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();

  if (!InvertedIndexField::init(slice, _analyzerDefinitions, _version,
                                extendedNames, analyzers, *this, defaultVocbase,
                                true, errorField)) {
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

  for (auto const& it : consistencyTypeMap) {
    if (it.second == _consistency) {
      builder.add(kConsistencyFieldName, VPackValue(it.first));
    }
  }

  builder.add(kVersionFieldName, VPackValue(static_cast<uint32_t>(_version)));

  return InvertedIndexField::json(server, builder, *this, true, defaultVocbase);
}

bool IResearchInvertedIndexMeta::operator==(
    IResearchInvertedIndexMeta const& other) const noexcept {
  if (*static_cast<IResearchDataStoreMeta const*>(this) !=
      static_cast<IResearchDataStoreMeta const&>(other)) {
    return false;
  }

  if (_sort != other._sort) {
    return false;
  }

  if (_consistency != other._consistency) {
    return false;
  }

  if (_storedValues != other._storedValues) {
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
      if (thisField.namesMatch(otherField)) {
        matched++;
        break;
      }
    }
  }
  return matched == _fields.size();
}

bool IResearchInvertedIndexMeta::matchesFieldsDefinition(
    IResearchInvertedIndexMeta const& meta, VPackSlice other,
    LogicalCollection const& collection) {
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
  auto& vocbase = collection.vocbase();
  std::string errorField;
  return otherMeta.init(vocbase.server(), other, true, errorField,
                        vocbase.name()) &&
         meta == otherMeta;
}

bool InvertedIndexField::json(
    arangodb::ArangodServer& server, VPackBuilder& builder,
    InvertedIndexField const& parent, bool rootMode,
    TRI_vocbase_t const* defaultVocbase /*= nullptr*/) const {
  if (rootMode || parent._isArray != _isArray) {
    builder.add(kIsArrayFieldName, VPackValue(_isArray));
  }
  if (rootMode || parent._trackListPositions != _trackListPositions) {
    builder.add(kTrackListPositionsFieldName, VPackValue(_trackListPositions));
  }
  if (rootMode || parent._includeAllFields != _includeAllFields) {
    builder.add(kIncludeAllFieldsFieldName, VPackValue(_includeAllFields));
  }
  if (rootMode || parent._overrideValue != _overrideValue) {
    builder.add(kOverrideFieldName, VPackValue(_overrideValue));
  }
  if (rootMode || parent._features != _features) {
    VPackBuilder tmp;
    _features.toVelocyPack(tmp);
    builder.add(kFeaturesFieldName, tmp.slice());
  }
  if (parent._expression != _expression) {
    builder.add(kExpressionFieldName, VPackValue(_expression));
  }

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
      name =
          _analyzers[0]._pool->name();  // verbatim (assume already normalized)
    }

    builder.add(kAnalyzerFieldName, VPackValue(name));
  }

  if (!rootMode) {
    builder.add(kNameFieldName, VPackValue(toString()));
  }

  if (!_fields.empty()) {
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
    irs::string_ref const defaultVocbase, bool rootMode,
    std::string& errorField) {
  // Fill inherited fields
  if (!rootMode) {
    _features = parent._features;
    _includeAllFields = parent._includeAllFields;
    _trackListPositions = parent._trackListPositions;
    _isArray = parent._isArray;
    _overrideValue = parent._overrideValue;
    _expression = parent._expression;
  }
  std::vector<basics::AttributeName> fieldParts;
  if (slice.isString()) {
    TRI_ASSERT(!rootMode);
    try {
      TRI_ASSERT(!parent._analyzers.empty());
      _analyzers[0] = parent.analyzer();
      TRI_ParseAttributeString(slice.stringView(), fieldParts, true);
      TRI_ASSERT(!fieldParts.empty());
    } catch (arangodb::basics::Exception const& err) {
      LOG_TOPIC("1d04c", ERR, arangodb::iresearch::TOPIC)
          << "Error parsing attribute: " << err.what();
      errorField = slice.stringView();
      return false;
    }
  } else if (slice.isObject()) {
    if (!rootMode) {
      // name attribute
      auto nameSlice = slice.get(kNameFieldName);
      if (!nameSlice.isString()) {
        errorField = kNameFieldName;
        return false;
      }
      try {
        TRI_ParseAttributeString(nameSlice.stringView(), fieldParts, true);
        TRI_ASSERT(!fieldParts.empty());
      } catch (arangodb::basics::Exception const& err) {
        LOG_TOPIC("84c20", ERR, arangodb::iresearch::TOPIC)
            << "Error parsing attribute: " << err.what();
        errorField = kNameFieldName;
        return false;
      }
    }

    if (slice.hasKey(kAnalyzerFieldName)) {
      AnalyzerPool::ptr analyzer;
      auto analyzerSlice = slice.get(kAnalyzerFieldName);
      if (analyzerSlice.isString()) {
        auto name = analyzerSlice.copyString();
        auto shortName = name;
        if (!defaultVocbase.null()) {
          name = IResearchAnalyzerFeature::normalize(name, defaultVocbase);
          shortName =
              IResearchAnalyzerFeature::normalize(name, defaultVocbase, false);
        }

        bool found = false;
        if (!analyzerDefinitions.empty()) {
          auto it = analyzerDefinitions.find(irs::string_ref(name));

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
        if (!found) {
          // save in referencedAnalyzers
          analyzerDefinitions.emplace(analyzer);
        }
        _analyzers[0] =
            FieldMeta::Analyzer(std::move(analyzer), std::move(shortName));
      } else {
        errorField = kAnalyzerFieldName;
        return false;
      }
    } else if (!rootMode) {
      _analyzers[0] = parent.analyzer();
    }
    if (slice.hasKey(kFeaturesFieldName)) {
      Features tmp;
      auto featuresRes = tmp.fromVelocyPack(slice.get(kFeaturesFieldName));
      if (featuresRes.fail()) {
        errorField = kFeaturesFieldName;
        LOG_TOPIC("2d52d", ERR, arangodb::iresearch::TOPIC)
            << "Error parsing features " << featuresRes.errorMessage();
        return false;
      }
      _features = std::move(tmp);
    }
    if (!rootMode && slice.hasKey(kExpressionFieldName)) {
      auto subSlice = slice.get(kExpressionFieldName);
      if (!subSlice.isString()) {
        errorField = kExpressionFieldName;
        return false;
      }
      _expression = subSlice.stringView();
    }
    if (slice.hasKey(kIsArrayFieldName)) {
      auto subSlice = slice.get(kIsArrayFieldName);
      if (!subSlice.isBool()) {
        errorField = kIsArrayFieldName;
        return false;
      }
      _isArray = subSlice.getBool();
    }
    if (slice.hasKey(kTrackListPositionsFieldName)) {
      auto subSlice = slice.get(kTrackListPositionsFieldName);
      if (!subSlice.isBool()) {
        errorField = kTrackListPositionsFieldName;
        return false;
      }
      _trackListPositions = subSlice.getBool();
    }
    if (slice.hasKey(kIncludeAllFieldsFieldName)) {
      auto subSlice = slice.get(kIncludeAllFieldsFieldName);
      if (!subSlice.isBool()) {
        errorField = kIncludeAllFieldsFieldName;
        return false;
      }
      _includeAllFields = subSlice.getBool();
    }
    if (slice.hasKey(kOverrideFieldName)) {
      auto subSlice = slice.get(kOverrideFieldName);
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

  auto fieldsAttributeName =
      rootMode ? kFieldsFieldName : kNestedFieldsFieldName;
#ifndef USE_ENTERPRISE
  if (!rootMode) {
    return true;
  }
#endif
  if (slice.isObject() && slice.hasKey(fieldsAttributeName)) {
    auto nestedSlice = slice.get(fieldsAttributeName);
    if (!nestedSlice.isArray()) {
      errorField = fieldsAttributeName;
      return false;
    }
    if (!rootMode && _trackListPositions) {
      if (slice.hasKey(kTrackListPositionsFieldName)) {
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
          errorField = fieldsAttributeName;
          errorField.append("[")
              .append(std::to_string(it.index()))
              .append("]")
              .append(" is duplicated");
          return false;
        }
        _fields.push_back(std::move(nested));
      } else {
        errorField = fieldsAttributeName;
        errorField.append("[")
            .append(std::to_string(it.index()))
            .append("].")
            .append(localError);
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

bool InvertedIndexField::namesMatch(
    InvertedIndexField const& other) const noexcept {
  return analyzerName() == other.analyzerName() &&
         basics::AttributeName::namesMatch(_attribute, other._attribute);
}

bool InvertedIndexField::isIdentical(
    std::vector<basics::AttributeName> const& path,
    irs::string_ref analyzerName) const noexcept {
  if (_analyzers.front()._shortName == analyzerName &&
      path.size() == _attribute.size()) {
    auto it = path.begin();
    auto atr = _attribute.begin();
    while (it != path.end() && atr != _attribute.end()) {
      if (it->name != atr->name || it->shouldExpand != atr->shouldExpand) {
        return false;
      }
      ++atr;
      ++it;
    }
    return true;
  }
  return false;
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
    addStringRef(builder, kSortCompressionFieldName, compression);
  }

  if (!_locale.isBogus()) {
    builder.add(kLocaleFieldName, VPackValue(_locale.getName()));
  }
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

  auto const compression = slice.get(kSortCompressionFieldName);
  if (!compression.isNone()) {
    if (!compression.isString()) {
      error = kSortCompressionFieldName;
      return false;
    }
    auto sort = columnCompressionFromString(compression.stringView());
    if (!sort) {
      error = kSortCompressionFieldName;
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
    _locale = icu::Locale::createCanonical(localeSlice.copyString().c_str());
    if (_locale.isBogus()) {
      error = kLocaleFieldName;
      return false;
    }
  }

  return true;
}

IResearchInvertedIndexMetaIndexingContext::
    IResearchInvertedIndexMetaIndexingContext(
        IResearchInvertedIndexMeta const* field, bool add)
    : _analyzers(&field->_analyzers),
      _primitiveOffset(field->_primitiveOffset),
      _meta(field),
      _hasNested(field->_hasNested),
      _includeAllFields(field->_includeAllFields),
      _trackListPositions(field->_trackListPositions),
      _sort(field->_sort),
      _storedValues(field->_storedValues) {
  if (add) {
    addField(*field);
  }
}

void IResearchInvertedIndexMetaIndexingContext::addField(
    InvertedIndexField const& field) {
  for (auto const& f : field._fields) {
    auto current = this;
    for (size_t i = 0; i < f._attribute.size(); ++i) {
      auto const& a = f._attribute[i];
      auto emplaceRes = current->_subFields.emplace(
          a.name, IResearchInvertedIndexMetaIndexingContext{_meta, false});

      if (!emplaceRes.second) {
        TRI_ASSERT(emplaceRes.first->second._isArray == a.shouldExpand);
        current = &(emplaceRes.first->second);
      } else {
        current = &(emplaceRes.first->second);
        current->_isArray = a.shouldExpand;
        current->_hasNested = false;
      }
      if (i == f._attribute.size() - 1) {
        current->_analyzers = &f._analyzers;
        current->_primitiveOffset = f._primitiveOffset;
        current->_includeAllFields = f._includeAllFields;
        current->_trackListPositions = f._trackListPositions;
      }
    }
#ifdef USE_ENTERPRISE
    if (!f._fields.empty()) {
      current->_hasNested = true;
      current->addField(f);
    }
#endif
  }
}
}  // namespace arangodb::iresearch
