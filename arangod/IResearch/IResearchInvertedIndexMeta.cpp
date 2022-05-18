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

namespace arangodb::iresearch {

bool IResearchInvertedIndexMeta::init(arangodb::ArangodServer& server,
                                      VPackSlice const& slice,
                                      bool readAnalyzerDefinition,
                                      std::string& errorField,
                                      irs::string_ref const defaultVocbase) {
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
    auto val = itr.value();
    if (val.isString()) {
      try {
        std::vector<basics::AttributeName> fieldParts;
        TRI_ParseAttributeString(val.stringView(), fieldParts, true);
        TRI_ASSERT(!fieldParts.empty());
        _fields.emplace_back(std::move(fieldParts),
                             FieldMeta::Analyzer(versionSpecificIdentity));
      } catch (arangodb::basics::Exception const& err) {
        LOG_TOPIC("1d04c", ERR, iresearch::TOPIC)
            << "Error parsing attribute: " << err.what();
        errorField = std::string{kFieldsFieldName} + "[" +
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
            errorField = std::string{kFieldsFieldName} + "[" +
                         basics::StringUtils::itoa(itr.index()) + "]";
            return false;
          }
        } catch (arangodb::basics::Exception const& err) {
          LOG_TOPIC("84c20", ERR, iresearch::TOPIC)
              << "Error parsing attribute: " << err.what();
          errorField = std::string{kFieldsFieldName} + "[" +
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
              if (analyzer) {
                // Remap analyzer features to match version.
                AnalyzerPool::ptr remappedAnalyzer;

                auto const res = IResearchAnalyzerFeature::copyAnalyzerPool(
                    remappedAnalyzer, *analyzer, LinkVersion{_version},
                    extendedNames);

                LOG_TOPIC_IF("2d81d", ERR, iresearch::TOPIC, res.fail())
                    << "Error remapping analyzer '" << name
                    << "' Error:" << res.errorMessage();
                analyzer = remappedAnalyzer;
              }
            }
            if (!analyzer) {
              errorField = std::string{kFieldsFieldName} + "[" +
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
            errorField = std::string{kFieldsFieldName} + "[" +
                         basics::StringUtils::itoa(itr.index()) + "]" +
                         ".analyzer";
            return false;
          }
        } else {
          _fields.emplace_back(std::move(fieldParts),
                               FieldMeta::Analyzer(versionSpecificIdentity));
        }
      } else {
        errorField = std::string{kFieldsFieldName} + "[" +
                     basics::StringUtils::itoa(itr.index()) + "]";
        return false;
      }
    } else {
      errorField = std::string{kFieldsFieldName} + "[" +
                   basics::StringUtils::itoa(itr.index()) + "]";
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
      VPackObjectBuilder fieldObject(&fieldsBuilder);
      fieldsBuilder.add("name", VPackValue(entry.toString()));
      fieldsBuilder.add("analyzer", VPackValue(entry.analyzer._pool->name()));
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
    std::vector<basics::AttributeName> const& path, FieldMeta::Analyzer&& a)
    : _analyzer(std::move(a)) {
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
  if (!expansion.empty()) {
    attr += ".";
    std::string exp;
    TRI_AttributeNamesToString(_expansion, exp, true);
    attr += exp;
  }
  return attr;
}

bool IResearchInvertedIndexMeta::FieldRecord::isIdentical(
    std::vector<basics::AttributeName> const& path,
    irs::string_ref analyzerName) const noexcept {
  if (analyzer._shortName == analyzerName &&
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
