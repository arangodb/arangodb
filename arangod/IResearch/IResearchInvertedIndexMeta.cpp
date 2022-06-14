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

namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

constexpr auto consistencyTypeMap =
    frozen::map<irs::string_ref, Consistency, 2>({
        {"eventual", Consistency::kEventual},
        {"immediate", Consistency::kImmediate}
    });

constexpr std::string_view kNameFieldName("name");
constexpr std::string_view kAnalyzerFieldName("analyzer");
constexpr std::string_view kNestedFieldsFieldName("nested");
constexpr std::string_view kFeaturesFieldName("features");
constexpr std::string_view kExpressionFieldName("expression");
constexpr std::string_view kIsArrayFieldName("isArray");
constexpr std::string_view kIncludeAllFieldsFieldName("includeAllFields");
constexpr std::string_view kTrackListPositionsFieldName("trackListPositions");
constexpr std::string_view kDirectionFieldName = "direction";
constexpr std::string_view kAscFieldName = "asc";
constexpr std::string_view kFieldName = "field";
constexpr std::string_view kFieldsFieldName = "fields";
constexpr std::string_view kSortCompressionFieldName = "primarySortCompression";
constexpr std::string_view kLocaleFieldName = "locale";
constexpr std::string_view kOverrideFieldName = "override";

std::optional<IResearchInvertedIndexMeta::FieldRecord> fromVelocyPack(
    VPackSlice slice, bool isNested,
    IResearchInvertedIndexMeta::AnalyzerDefinitions& analyzerDefinitions,
    AnalyzerPool::ptr versionSpecificIdentity, LinkVersion version,
    bool extendedNames, IResearchAnalyzerFeature& analyzers,
    irs::string_ref const defaultVocbase, std::string& errorField,
    std::string_view parentName) {
  std::optional<Features> features;
  std::vector<IResearchInvertedIndexMeta::FieldRecord> nestedFields;
  AnalyzerPool::ptr analyzer;
  std::string expression;
  std::vector<basics::AttributeName> fieldParts;
  std::string shortName;
  std::string fieldName;
  bool isArray{false};
  bool trackListPositions{false};
  bool includeAllFields{false};
  bool overrideValue{false};
  if (slice.isString()) {
    try {
      analyzer = versionSpecificIdentity;
      shortName = versionSpecificIdentity->name();
      fieldName = slice.copyString();
      TRI_ParseAttributeString(fieldName, fieldParts, true);
      TRI_ASSERT(!fieldParts.empty());
    } catch (arangodb::basics::Exception const& err) {
      LOG_TOPIC("1d04c", ERR, arangodb::iresearch::TOPIC)
          << "Error parsing attribute: " << err.what();
      errorField = slice.stringView();
      return std::nullopt;
    }
  } else if (slice.isObject()) {
    auto nameSlice = slice.get(kNameFieldName);
    if (nameSlice.isString()) {
      fieldName = nameSlice.copyString();
      try {
        TRI_ParseAttributeString(nameSlice.stringView(), fieldParts, true);
        TRI_ASSERT(!fieldParts.empty());
      } catch (arangodb::basics::Exception const& err) {
        LOG_TOPIC("84c20", ERR, arangodb::iresearch::TOPIC)
            << "Error parsing attribute: " << err.what();
        errorField = kNameFieldName;
        return std::nullopt;
      }
      // and now set analyzers to the last one
      if (slice.hasKey(kAnalyzerFieldName)) {
        auto analyzerSlice = slice.get(kAnalyzerFieldName);
        if (analyzerSlice.isString()) {
          auto name = analyzerSlice.copyString();
          shortName = name;
          if (!defaultVocbase.null()) {
            name = IResearchAnalyzerFeature::normalize(name, defaultVocbase);
            shortName = IResearchAnalyzerFeature::normalize(
                name, defaultVocbase, false);
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
            return std::nullopt;
          }
          if (!found) {
            // save in referencedAnalyzers
            analyzerDefinitions.emplace(analyzer);
          }
        } else {
          errorField = kAnalyzerFieldName;
          return std::nullopt;
        }
      } else {
        analyzer = versionSpecificIdentity;
        shortName = versionSpecificIdentity->name();
      }
      {
        if (slice.hasKey(kFeaturesFieldName)) {
          Features tmp;
          auto featuresRes = tmp.fromVelocyPack(slice.get(kFeaturesFieldName));
          if (featuresRes.fail()) {
            errorField = kFeaturesFieldName;
            LOG_TOPIC("2d52d", ERR, arangodb::iresearch::TOPIC)
                << "Error parsing features " << featuresRes.errorMessage();
            return std::nullopt;
          }
          features = std::move(tmp);
        }
      }
      if (slice.hasKey(kNestedFieldsFieldName)) {
        auto nestedSlice = slice.get(kNestedFieldsFieldName);
        if (!nestedSlice.isArray()) {
          errorField = kNestedFieldsFieldName;
          return std::nullopt;
        }
        std::string localError;
        std::string myPath;
        if (!parentName.empty()) {
          myPath += parentName;
          myPath += ".";
        }
        myPath += fieldName;
        kludge::mangleNested(myPath);

        for (auto it = VPackArrayIterator(nestedSlice); it.valid(); ++it) {
          auto nested = fromVelocyPack(
              it.value(), true, analyzerDefinitions, versionSpecificIdentity,
              version, extendedNames, analyzers, defaultVocbase, localError,
              myPath);
          if (nested) {
            nestedFields.push_back(std::move(*nested));
          } else {
            errorField = kNestedFieldsFieldName;
            errorField.append("[")
                .append(std::to_string(it.index()))
                .append("].")
                .append(localError);
            return std::nullopt;
          }
        }
      }
      if (slice.hasKey(kExpressionFieldName)) {
        auto subSlice = slice.get(kExpressionFieldName);
        if (!subSlice.isString()) {
          errorField = kExpressionFieldName;
          return std::nullopt;
        }
        expression = subSlice.stringView();
      }
      if (slice.hasKey(kIsArrayFieldName)) {
        auto subSlice = slice.get(kIsArrayFieldName);
        if (!subSlice.isBool()) {
          errorField = kIsArrayFieldName;
          return std::nullopt;
        }
        isArray = subSlice.getBool();
      }
      if (slice.hasKey(kTrackListPositionsFieldName)) {
        auto subSlice = slice.get(kTrackListPositionsFieldName);
        if (!subSlice.isBool()) {
          errorField = kTrackListPositionsFieldName;
          return std::nullopt;
        }
        trackListPositions = subSlice.getBool();
      }
      if (slice.hasKey(kIncludeAllFieldsFieldName)) {
        auto subSlice = slice.get(kIncludeAllFieldsFieldName);
        if (!subSlice.isBool()) {
          errorField = kIncludeAllFieldsFieldName;
          return std::nullopt;
        }
        includeAllFields = subSlice.getBool();
      }
      if (slice.hasKey(kOverrideFieldName)) {
        auto subSlice = slice.get(kOverrideFieldName);
        if (!subSlice.isBoolean()) {
          errorField = kOverrideFieldName;
          return std::nullopt;
        }
        overrideValue = subSlice.getBoolean();
      }
    } else {
      errorField = kNameFieldName;
      return std::nullopt;
    }
  } else {
    errorField = "<String or object expected>";
    return std::nullopt;
  }
  // we only allow one expansion
  size_t expansionCount{0};
  std::for_each(fieldParts.begin(), fieldParts.end(),
                [&expansionCount](basics::AttributeName const& a) -> void {
                  if (a.shouldExpand) {
                    ++expansionCount;
                  }
                });
  if (expansionCount > 1 && !isNested) {
    LOG_TOPIC("2646b", ERR, arangodb::iresearch::TOPIC)
        << "Error parsing field: '" << kNameFieldName << "'. "
        << "Expansion is allowed only once.";
    errorField = kNameFieldName;
    return std::nullopt;
  } else if (expansionCount > 0 && isNested) {
    LOG_TOPIC("2646b", ERR, arangodb::iresearch::TOPIC)
        << "Error parsing field: '" << kNameFieldName << "'. "
        << "Expansion is not allowed for nested fields.";
    errorField = kNameFieldName;
    return std::nullopt;
  }
  bool const isAnalyzerPrimitive =
      !analyzer->accepts(AnalyzerValueType::Array | AnalyzerValueType::Object);
  return std::make_optional<IResearchInvertedIndexMeta::FieldRecord>(
      std::move(fieldParts), FieldMeta::Analyzer(std::move(analyzer), std::move(shortName)),
      std::move(nestedFields), std::move(features), std::move(expression),
      isArray, includeAllFields, trackListPositions, overrideValue, isAnalyzerPrimitive, parentName);
}

void toVelocyPack(IResearchInvertedIndexMeta::FieldRecord const& field, VPackBuilder& vpack) {
  TRI_ASSERT(vpack.isOpenArray());
   VPackObjectBuilder fieldObject(&vpack);
  vpack.add(kNameFieldName, VPackValue(field.toString()));
  vpack.add(kAnalyzerFieldName, VPackValue(field.analyzerName()));
  vpack.add(kIsArrayFieldName, VPackValue(field.isArray()));
  vpack.add(kTrackListPositionsFieldName, VPackValue(field.trackListPositions()));
  vpack.add(kIncludeAllFieldsFieldName, VPackValue(field.includeAllFields()));
  vpack.add(kOverrideFieldName, VPackValue(field.overrideValue()));
  if (field.features()) {
    VPackBuilder tmp;
    field.features()->toVelocyPack(tmp);
    vpack.add(kFeaturesFieldName, tmp.slice());
  }
  if (!field.nested().empty()) {
    VPackBuilder nestedFields;
    {
      VPackArrayBuilder nestedArray(&nestedFields);
      for (auto const nested : field.nested()) {
        toVelocyPack(nested, nestedFields);
      }
    }
    vpack.add(kNestedFieldsFieldName, nestedFields.slice());
  }
}

} // namespace

namespace arangodb::iresearch {

const IResearchInvertedIndexMeta& IResearchInvertedIndexMeta::DEFAULT() {
  static const IResearchInvertedIndexMeta meta{};
  return meta;
}


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
    constexpr std::string_view kFieldName("consistency");
    auto consistencySlice = slice.get(kFieldName);
    if (!consistencySlice.isNone()) {
      if (consistencySlice.isString()) {
        auto it = consistencyTypeMap.find(consistencySlice.stringView());
        if (it != consistencyTypeMap.end()) {
          _consistency = it->second;
        } else {
          errorField = kFieldName;
          return false;
        }
      } else {
        errorField = kFieldName;
        return false;
      }
    }
   
  }
  // copied part begin FIXME: verify and move to common base class if possible
  {
    // optional stored values
    constexpr std::string_view kFieldName("storedValues");

    auto const field = slice.get(kFieldName);
    if (!field.isNone() && !_storedValues.fromVelocyPack(field, errorField)) {
      errorField = kFieldName;
      return false;
    }
  }
  {
    // optional primarySort
    constexpr std::string_view kFieldName("primarySort");
    auto const field = slice.get(kFieldName);
    if (!field.isNone() && !_sort.fromVelocyPack(field, errorField)) {
      errorField = kFieldName;
      return false;
    }
  }
  {
    // optional version
    constexpr std::string_view kFieldName("version");
    auto const field = slice.get(kFieldName);
    if (field.isNumber()) {
      _version = field.getNumber<uint32_t>();
      if (_version > static_cast<uint32_t>(LinkVersion::MAX)) {
        errorField = kFieldName;
        return false;
      }
    } else if (field.isNone()) {
      _version = static_cast<uint32_t>(
          LinkVersion::MAX);  // not present -> last version
    } else {
      errorField = kFieldName;
      return false;
    }
  }
  bool const extendedNames =
      server.getFeature<DatabaseFeature>().extendedNamesForAnalyzers();

  auto const& identity = *IResearchAnalyzerFeature::identity();
  AnalyzerPool::ptr versionSpecificIdentity;

  auto const res = IResearchAnalyzerFeature::copyAnalyzerPool(
      versionSpecificIdentity, identity, LinkVersion{_version}, extendedNames);

  if (res.fail() || !versionSpecificIdentity) {
    TRI_ASSERT(false);
    return false;
  }

  {
    // clear existing definitions
    _analyzerDefinitions.clear();

    // optional object list
    constexpr std::string_view kFieldName{"analyzerDefinitions"};

    // load analyzer definitions if requested (used on cluster)
    // @note must load definitions before loading 'analyzers' to ensure presence
    if (readAnalyzerDefinition && slice.hasKey(kFieldName)) {
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
        std::string_view type;

        {
          // required string value
          constexpr std::string_view kSubFieldName{"type"};
          auto typeSlice = value.get(kSubFieldName);

          if (!typeSlice.isString()) {
            errorField = std::string{kFieldName} + "[" +
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
          constexpr std::string_view kSubFieldName{"features"};

          if (value.hasKey(kSubFieldName)) {
            auto subField = value.get(kSubFieldName);
            auto featuresRes = features.fromVelocyPack(subField);
            if (featuresRes.fail()) {
              errorField = std::string{kFieldName}
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
  // for index there is no recursive struct and fields array is mandatory
  auto field = slice.get(kFieldsFieldName);
  if (!field.isArray() || field.isEmptyArray()) {
    errorField = kFieldsFieldName;
    return false;
  }
  auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();
  for (VPackArrayIterator itr(field); itr.valid(); itr.next()) {
    std::string localError;
    auto field = fromVelocyPack(itr.value(), false, _analyzerDefinitions,
                                versionSpecificIdentity, LinkVersion{_version}, extendedNames,
                                analyzers, defaultVocbase, localError, "");
    if (field) {
      _fields._fields.push_back(std::move(*field));
    }
    else {
      errorField = std::string{kFieldsFieldName} + "[" +
                   basics::StringUtils::itoa(itr.index()) + "]." + localError;
      return false;
    } 
  }

  if (slice.hasKey(kTrackListPositionsFieldName)) {
    auto subSlice = slice.get(kTrackListPositionsFieldName);
    if (!subSlice.isBool()) {
      errorField = kTrackListPositionsFieldName;
      return false;
    }
    _fields._trackListPositions = subSlice.getBool();
  }
  if (slice.hasKey(kIncludeAllFieldsFieldName)) {
    auto subSlice = slice.get(kIncludeAllFieldsFieldName);
    if (!subSlice.isBool()) {
      errorField = kIncludeAllFieldsFieldName;
      return false;
    }
    _fields._includeAllFields = subSlice.getBool();
  }

  if (slice.hasKey(kFeaturesFieldName)) {
    Features tmp;
    auto featuresRes = tmp.fromVelocyPack(slice.get(kFeaturesFieldName));
    if (featuresRes.fail()) {
      errorField = kFeaturesFieldName;
      LOG_TOPIC("2d51a", ERR, arangodb::iresearch::TOPIC)
          << "Error parsing features " << featuresRes.errorMessage();
      return false;
    }
    _features = std::move(tmp);
  }
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
    VPackArrayBuilder arrayScope(&builder, "analyzerDefinitions");

    for (auto& entry : _analyzerDefinitions) {
      TRI_ASSERT(entry);  // ensured by emplace into 'analyzers' above
      entry->toVelocyPack(builder, defaultVocbase);
    }
  }
  velocypack::Builder fieldsBuilder;
  {
    VPackArrayBuilder fieldsArray(&fieldsBuilder);
    TRI_ASSERT(!_fields._fields.empty());
    for (auto& entry : _fields._fields) {
       toVelocyPack(entry, fieldsBuilder);
    }
  }
  builder.add(arangodb::StaticStrings::IndexFields, fieldsBuilder.slice());

  if (_features) { 
    VPackBuilder tmp;
    _features->toVelocyPack(tmp);
    builder.add(kFeaturesFieldName, tmp.slice());
  }

  builder.add(kTrackListPositionsFieldName, VPackValue(_fields._trackListPositions));
  builder.add(kIncludeAllFieldsFieldName, VPackValue(_fields._includeAllFields));

  return true;
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

  if (_storedValues != other._storedValues) {
    return false;
  }

  if (_version != other._version) {
    return false;
  }

  if (_fields._fields.size() != other._fields._fields.size()) {
    return false;
  }

  size_t matched{0};
  for (auto const& thisField : _fields._fields) {
    for (auto const& otherField : _fields._fields) {
      if (thisField.namesMatch(otherField)) {
        matched++;
        break;
      }
    }
  }
  return matched == _fields._fields.size();
}

bool IResearchInvertedIndexMeta::matchesFieldsDefinition(
    IResearchInvertedIndexMeta const& meta, VPackSlice other) {
  auto value = other.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  auto const count = meta._fields._fields.size();
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

    auto in = name.stringView();
    irs::string_ref analyzerName = analyzer.stringView();
    TRI_ParseAttributeString(in, translate, true);
    for (auto const& f : meta._fields._fields) {
      if (f.isIdentical(translate, analyzerName)) {
        matched++;
        break;
      }
    }
    translate.clear();
  }
  return matched == count;
}



IResearchInvertedIndexMeta::FieldRecord::FieldRecord(
    std::vector<basics::AttributeName> const& path, FieldMeta::Analyzer&& a,
    std::vector<FieldRecord>&& nested, std::optional<Features>&& features,
    std::string&& expression, bool isArray, bool includeAllFields, bool trackListPositions,
    bool overrideValue, bool isPrimitiveAnalyzer, std::string_view parentName)
    : _fields(std::move(nested)),
      _expression(std::move(expression)),
      _analyzers{std::move(a)},
      _features(std::move(features)),
      _primitiveOffset(isPrimitiveAnalyzer ? 1 : 0),
      _isArray(isArray),
      _includeAllFields(includeAllFields),
      _trackListPositions(trackListPositions),
      _overrideValue(overrideValue) {
  TRI_ASSERT(_attribute.empty());
  TRI_ASSERT(_expansion.empty());
  auto it = path.begin();
  while (it != path.end()) {
    _attribute.push_back(*it);
    if (it->shouldExpand) {
      ++it;
      break;
    }
    ++it;
  }
  while (it != path.end()) {
    TRI_ASSERT(
        !it->shouldExpand);  // only one expansion per attribute is supported!
    _expansion.push_back(*it);
    ++it;
  }
  _path = parentName;
#ifdef USE_ENTERPRISE
  if (!parentName.empty()) {
    _path += ".";
  }
#endif
  {
    std::string tmp;
    TRI_AttributeNamesToString(_attribute, tmp, _expansion.empty());
    _path += tmp;
    if (!_expansion.empty()) {
      _path += ".";
      std::string exp;
      TRI_AttributeNamesToString(_expansion, exp, true);
      _path += exp;
    }
  }
}



std::string IResearchInvertedIndexMeta::FieldRecord::toString() const {
  std::string attr;
  TRI_AttributeNamesToString(_attribute, attr, false);
  if (!_expansion.empty()) {
    attr += ".";
    std::string exp;
    TRI_AttributeNamesToString(_expansion, exp, true);
    attr += exp;
  }
  return attr;
}

std::string IResearchInvertedIndexMeta::FieldRecord::attributeString() const {
  std::string attr;
  TRI_AttributeNamesToString(_attribute, attr, true);
  return attr;
}

std::string_view IResearchInvertedIndexMeta::FieldRecord::path() const noexcept {
   return _path;
}

std::vector<arangodb::basics::AttributeName>
IResearchInvertedIndexMeta::FieldRecord::combinedName() const {
  std::vector<arangodb::basics::AttributeName> combined;
  combined.reserve(_attribute.size() + _expansion.size());
  combined.insert(combined.end(), std::begin(_attribute), std::end(_attribute));
  combined.insert(combined.end(), std::begin(_expansion), std::end(_expansion));
  return combined;
}

bool IResearchInvertedIndexMeta::FieldRecord::namesMatch(
    FieldRecord const& other) const noexcept {
  return analyzerName() == other.analyzerName() &&
         basics::AttributeName::namesMatch(_attribute, other._attribute) &&
         basics::AttributeName::namesMatch(_expansion, other._expansion);
}

bool IResearchInvertedIndexMeta::FieldRecord::isIdentical(
    std::vector<basics::AttributeName> const& path,
    irs::string_ref analyzerName) const noexcept {
  if (_analyzers.front()._shortName == analyzerName &&
      path.size() == (_attribute.size() + _expansion.size())) {
    auto it = path.begin();
    auto atr = _attribute.begin();
    while (it != path.end() && atr != _attribute.end()) {
      if (it->name != atr->name || it->shouldExpand != atr->shouldExpand) {
        return false;
      }
      ++atr;
      ++it;
    }
    auto exp = _expansion.begin();
    while (it != path.end() && exp != _expansion.end()) {
      if (it->name != exp->name || it->shouldExpand != exp->shouldExpand) {
        return false;
      }
      ++exp;
      ++it;
    }
  }
  return false;
}

size_t IResearchInvertedIndexSort::memory() const noexcept {
  size_t size = sizeof(*this);

  for (auto& field : _fields) {
    size += sizeof(basics::AttributeName) * field.size();
    for (auto& entry : field) {
      size += entry.name.size();
    }
  }

  size += irs::math::math_traits<size_t>::div_ceil(_directions.size(), 8);

  return size;
}
bool IResearchInvertedIndexSort::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder objectScope(&builder);
  {
    VPackBuilder fields;
    VPackArrayBuilder arrayScope(&fields);
    std::string fieldName;
    auto visitor = [&fields, &fieldName](
                       std::vector<basics::AttributeName> const& field,
                       bool direction) {
      fieldName.clear();
      basics::TRI_AttributeNamesToString(field, fieldName, true);

      arangodb::velocypack::ObjectBuilder sortEntryBuilder(&fields);
      fields.add(kFieldName, VPackValue(fieldName));
      fields.add(kAscFieldName, VPackValue(direction));

      return true;
    };

    if (!visit(visitor)) {
      return false;
    }
    builder.add(kFieldsFieldName, fields.slice());
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

  _fields.reserve(fieldsSlice.length());
  _directions.reserve(fieldsSlice.length());

  for (auto sortSlice : velocypack::ArrayIterator(fieldsSlice)) {
    if (!sortSlice.isObject() || sortSlice.length() != 2) {
      error = "[" + std::to_string(size()) + "]";
      return false;
    }

    bool direction;

    auto const directionSlice = sortSlice.get(kDirectionFieldName);
    if (!directionSlice.isNone()) {
      if (!parseDirectionString(directionSlice, direction)) {
        error = "[" + std::to_string(size()) + "]." + std::string(kDirectionFieldName);
        return false;
      }
    } else if (!parseDirectionBool(sortSlice.get(kAscFieldName), direction)) {
      error = "[" + std::to_string(size()) + "]." + std::string(kAscFieldName);
      return false;
    }

    auto const fieldSlice = sortSlice.get(kFieldName);

    if (!fieldSlice.isString()) {
      error = "[" + std::to_string(size()) + "]." + std::string(kFieldName);
      return false;
    }

    std::vector<arangodb::basics::AttributeName> field;

    try {
      arangodb::basics::TRI_ParseAttributeString(
          arangodb::iresearch::getStringRef(fieldSlice), field, false);
    } catch (...) {
      error = "[" + std::to_string(size()) + "]." + std::string(kFieldName);
      return false;
    }

    emplace_back(std::move(field), direction);
  }

  if (slice.hasKey(kSortCompressionFieldName)) {
    auto const field = slice.get(kSortCompressionFieldName);

    if (!field.isNone() && ((_sortCompression = columnCompressionFromString(
                                 getStringRef(field))) == nullptr)) {
      error = kSortCompressionFieldName;
      return false;
    }
  } 

  if (slice.hasKey(kLocaleFieldName)) {
    auto localeSlice = slice.get(kLocaleFieldName);
    if (!localeSlice.isString()) {
      error = kLocaleFieldName;
      return false;
    }
    // intentional string copy here as createCanonical expects null-ternibated string
    // and string_view has no such guarantees
    _locale = icu::Locale::createCanonical(localeSlice.copyString().c_str());
    if (_locale.isBogus()) {
      error = kLocaleFieldName;
      return false;
    }
  }

  return true;
}
}  // namespace arangodb::iresearch
