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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VelocyPackHelper.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

namespace {
using namespace arangodb;
using namespace arangodb::iresearch;

constexpr auto consistencyTypeMap =
    frozen::make_map<irs::string_ref, Consistency>({
        {"eventual", Consistency::kEventual},
        {"immediate", Consistency::kImmediate}
    });

constexpr std::string_view kNameFieldName("name");
constexpr std::string_view kAnalyzerFieldName("analyzer");
constexpr std::string_view kNestedFieldsFieldName("nested");
constexpr std::string_view kFeaturesFieldName("features");
constexpr std::string_view kExpressionFieldName("expression");

std::optional<IResearchInvertedIndexMeta::FieldRecord> fromVelocyPack(
    VPackSlice slice, bool isNested,
    IResearchInvertedIndexMeta::AnalyzerDefinitions& analyzerDefinitions,
    AnalyzerPool::ptr versionSpecificIdentity, LinkVersion version,
    bool extendedNames, IResearchAnalyzerFeature& analyzers,
    irs::string_ref const defaultVocbase, std::string& errorField) {
  std::optional<Features> features;
  IResearchInvertedIndexMeta::Fields nestedFields;
  AnalyzerPool::ptr analyzer;
  std::string expression;
  std::vector<basics::AttributeName> fieldParts;
  bool isArray{false};
  bool trackListPositions{false};
  bool includeAllFields{false};
  if (slice.isString()) {
    try {
      analyzer = versionSpecificIdentity;
      TRI_ParseAttributeString(slice.stringView(), fieldParts, true);
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
      try {
        TRI_ParseAttributeString(nameSlice.stringRef(), fieldParts, true);
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
          auto shortName = name;
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
        for (auto it = VPackArrayIterator(nestedSlice); it.valid(); ++it) {
          auto nested = fromVelocyPack(
              it.value(), true, analyzerDefinitions, versionSpecificIdentity,
              version, extendedNames, analyzers, defaultVocbase, localError);
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
        auto expressionSlice = slice.get(kExpressionFieldName);
        if (!expressionSlice.isString()) {
          errorField = kExpressionFieldName;
          return std::nullopt;
        }
        expression = expressionSlice.stringView();
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
  return std::make_optional<IResearchInvertedIndexMeta::FieldRecord>(
      std::move(fieldParts), FieldMeta::Analyzer(versionSpecificIdentity),
      std::move(nestedFields), std::move(features), std::move(expression),
      isArray, includeAllFields, trackListPositions);
}

void toVelocyPack(IResearchInvertedIndexMeta::FieldRecord const& field, VPackBuilder& vpack) {
  TRI_ASSERT(vpack.isOpenArray());
   VPackObjectBuilder fieldObject(&vpack);
  vpack.add(kNameFieldName, VPackValue(field.toString()));
  vpack.add(kAnalyzerFieldName, VPackValue(field.analyzerName()));
  if (!field.nested().empty()) {
    VPackBuilder nestedFields;
    VPackArrayBuilder nestedArray(&nestedFields);
    for (auto const nested : field.nested()) {
      toVelocyPack(nested, nestedFields);
    }
    vpack.add(kNestedFieldsFieldName, nestedFields.slice());
  }
}

} // namespace

namespace arangodb::iresearch {

bool IResearchInvertedIndexMeta::init(arangodb::ArangodServer& server,
                                      VPackSlice const& slice,
                                      bool readAnalyzerDefinition,
                                      std::string& errorField,
                                      irs::string_ref const defaultVocbase) {

  // consistency (optional)
  {
    constexpr std::string_view kFieldName("consistency");
    auto consistencySlice = slice.get(kFieldName);
    if (!consistencySlice.isNone()) {
      if (consistencySlice.isString()) {
        auto it = consistencyTypeMap.find(consistencySlice.stringRef());
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
      return false;
    }
  }
  {
    // optional sort compression
    constexpr std::string_view kFieldName("primarySortCompression");
    auto const field = slice.get(kFieldName);

    if (!field.isNone() && ((_sortCompression = columnCompressionFromString(
                                 getStringRef(field))) == nullptr)) {
      return false;
    }
  }
  {
    // optional primarySort
    constexpr std::string_view kFieldName("primarySort");
    auto const field = slice.get(kFieldName);
    if (!field.isNone() && !_sort.fromVelocyPack(field, errorField)) {
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
    if (readAnalyzerDefinition) {
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
  // end of the copied part
  constexpr std::string_view kFieldsFieldName("fields");
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
                                analyzers, defaultVocbase, localError);
    if (field) {
      _fields.push_back(std::move(*field));
    }
    else {
      errorField = std::string{kFieldsFieldName} + "[" +
                   basics::StringUtils::itoa(itr.index()) + "]." + localError;
      return false;
    } 
  }
  return true;
}

bool IResearchInvertedIndexMeta::json(
    arangodb::ArangodServer& server, VPackBuilder& builder,
    bool writeAnalyzerDefinition,
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
       toVelocyPack(entry, fieldsBuilder);
    }
  }
  builder.add(arangodb::StaticStrings::IndexFields, fieldsBuilder.slice());
  return true;
}

bool IResearchInvertedIndexMeta::operator==(
    IResearchInvertedIndexMeta const& other) const noexcept {
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
      if (thisField.namesMatch(otherField)) {
        matched++;
        break;
      }
    }
  }
  return matched == _fields.size();
}

bool IResearchInvertedIndexMeta::matchesFieldsDefinition(
    IResearchInvertedIndexMeta const& meta, VPackSlice other) {
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



IResearchInvertedIndexMeta::FieldRecord::FieldRecord(
    std::vector<basics::AttributeName> const& path, FieldMeta::Analyzer&& a,
    std::vector<FieldRecord>&& nested, std::optional<Features>&& features,
    std::string&& expression, bool isArray, bool includeAllFields, bool trackListPositions)
    : _nested(std::move(nested)),
      _expression(std::move(expression)),
      _analyzer(std::move(a)),
      _features(std::move(features)),
      _isArray(isArray),
      _includeAllFields(includeAllFields),
      _trackListPositions(trackListPositions) {
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
  if (_analyzer._shortName == analyzerName &&
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
}  // namespace arangodb::iresearch
