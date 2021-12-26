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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexFactory.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/FloatingPoint.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Utilities/NameValidator.h"
#include "VocBase/LogicalCollection.h"

#include <limits.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <regex>

namespace {

using namespace arangodb;

struct InvalidIndexFactory : public IndexTypeFactory {
  InvalidIndexFactory(application_features::ApplicationServer& server)
      : IndexTypeFactory(server) {}

  bool equal(velocypack::Slice, velocypack::Slice,
             std::string const&) const override {
    return false;  // invalid definitions are never equal
  }

  std::shared_ptr<Index> instantiate(LogicalCollection&,
                                     velocypack::Slice definition, IndexId,
                                     bool) const override {
    std::string type = basics::VelocyPackHelper::getStringValue(
        definition, StaticStrings::IndexType, "");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid index type '" + type + "'");
  }

  Result normalize(velocypack::Builder&, velocypack::Slice definition, bool,
                   TRI_vocbase_t const&) const override {
    std::string type = basics::VelocyPackHelper::getStringValue(
        definition, StaticStrings::IndexType, "");
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid index type '" + type + "'");
  }
};

}  // namespace

namespace arangodb {

IndexTypeFactory::IndexTypeFactory(
    application_features::ApplicationServer& server)
    : _server(server) {}

bool IndexTypeFactory::equal(Index::IndexType type, velocypack::Slice lhs,
                             velocypack::Slice rhs,
                             bool attributeOrderMatters) const {
  // unique must be identical if present
  auto value = lhs.get(StaticStrings::IndexUnique);

  if (value.isBoolean() &&
      !basics::VelocyPackHelper::equal(
          value, rhs.get(StaticStrings::IndexUnique), false)) {
    return false;
  }

  // sparse must be identical if present
  value = lhs.get(StaticStrings::IndexSparse);

  if (value.isBoolean() &&
      !basics::VelocyPackHelper::equal(
          value, rhs.get(StaticStrings::IndexSparse), false)) {
    return false;
  }

  if (Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX == type ||
      Index::IndexType::TRI_IDX_TYPE_GEO_INDEX == type) {
    // geoJson must be identical if present
    value = lhs.get("geoJson");

    if (value.isBoolean() &&
        !basics::VelocyPackHelper::equal(value, rhs.get("geoJson"), false)) {
      return false;
    }
  } else if (Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX == type) {
    // minLength
    value = lhs.get("minLength");

    if (value.isNumber() &&
        !basics::VelocyPackHelper::equal(value, rhs.get("minLength"), false)) {
      return false;
    }
  } else if (Index::IndexType::TRI_IDX_TYPE_TTL_INDEX == type) {
    value = lhs.get(StaticStrings::IndexExpireAfter);

    if (value.isNumber() &&
        rhs.get(StaticStrings::IndexExpireAfter).isNumber()) {
      double const expireAfter = value.getNumber<double>();
      value = rhs.get(StaticStrings::IndexExpireAfter);

      if (!FloatingPoint<double>{expireAfter}.AlmostEquals(
              FloatingPoint<double>{value.getNumber<double>()})) {
        return false;
      }
    }
  }

  // other index types: fields must be identical if present
  value = lhs.get(StaticStrings::IndexFields);

  if (value.isArray()) {
    if (!attributeOrderMatters) {
      // attributes can be specified in any order
      velocypack::ValueLength const nv = value.length();

      // compare fields in arbitrary order
      auto r = rhs.get(StaticStrings::IndexFields);

      if (!r.isArray() || nv != r.length()) {
        return false;
      }

      for (size_t i = 0; i < nv; ++i) {
        velocypack::Slice const v = value.at(i);

        bool found = false;

        for (VPackSlice vr : VPackArrayIterator(r)) {
          if (basics::VelocyPackHelper::equal(v, vr, false)) {
            found = true;
            break;
          }
        }

        if (!found) {
          return false;
        }
      }
    } else {
      // attribute order matters
      if (!basics::VelocyPackHelper::equal(
              value, rhs.get(StaticStrings::IndexFields), false)) {
        return false;
      }
    }
  }

  return true;
}

IndexFactory::IndexFactory(application_features::ApplicationServer& server)
    : _server(server),
      _factories(),
      _invalid(std::make_unique<InvalidIndexFactory>(server)) {}

void IndexFactory::clear() { _factories.clear(); }

Result IndexFactory::emplace(std::string const& type,
                             IndexTypeFactory const& factory) {
  if (_server.hasFeature<BootstrapFeature>()) {
    auto& feature = _server.getFeature<BootstrapFeature>();
    // ensure new factories are not added at runtime since that would require
    // additional locks
    if (feature.isReady()) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("index factory registration is only "
                                "allowed during server startup"));
    }
  }

  if (!_factories.try_emplace(type, &factory).second) {
    return Result(
        TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
        std::string("index factory previously registered during index factory "
                    "registration for index type '") +
            type + "'");
  }

  return Result();
}

Result IndexFactory::enhanceIndexDefinition(  // normalize definition
    velocypack::Slice const definition,       // source definition
    velocypack::Builder& normalized,  // normalized definition (out-param)
    bool isCreation,                  // definition for index creation
    TRI_vocbase_t const& vocbase      // index vocbase
) const {
  auto type = definition.get(StaticStrings::IndexType);

  if (!type.isString()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid index type");
  }

  auto& factory = IndexFactory::factory(type.copyString());

  TRI_ASSERT(normalized.isEmpty());

  try {
    velocypack::ObjectBuilder b(&normalized);
    auto idSlice = definition.get(StaticStrings::IndexId);
    uint64_t id = 0;

    if (idSlice.isNumber()) {
      id = idSlice.getNumericValue<uint64_t>();
    } else if (idSlice.isString()) {
      id = basics::StringUtils::uint64(idSlice.copyString());
    }

    if (id) {
      normalized.add(StaticStrings::IndexId,
                     velocypack::Value(std::to_string(id)));
    }

    auto nameSlice = definition.get(StaticStrings::IndexName);
    std::string name;

    if (nameSlice.isString() && (nameSlice.getStringLength() != 0)) {
      name = nameSlice.copyString();
    } else {
      // we should set the name for special types explicitly elsewhere, but just
      // in case...
      if (Index::type(type.copyString()) ==
          Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        name = StaticStrings::IndexNamePrimary;
      } else if (Index::type(type.copyString()) ==
                 Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        name = StaticStrings::IndexNameEdge;
      } else {
        // generate a name
        name = "idx_" + std::to_string(TRI_HybridLogicalClock());
      }
    }

    bool extendedNames =
        _server.getFeature<DatabaseFeature>().extendedNamesForCollections();
    if (!IndexNameValidator::isAllowedName(extendedNames, name)) {
      return Result(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }

    normalized.add(StaticStrings::IndexName, velocypack::Value(name));

    return factory.normalize(normalized, definition, isCreation, vocbase);
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "unknown exception");
  }
}

const IndexTypeFactory& IndexFactory::factory(
    std::string const& type) const noexcept {
  auto itr = _factories.find(type);
  TRI_ASSERT(
      itr == _factories.end() ||
      false ==
          !(itr->second));  // IndexFactory::emplace(...) inserts non-nullptr

  return itr == _factories.end() ? *_invalid : *(itr->second);
}

std::shared_ptr<Index> IndexFactory::prepareIndexFromSlice(
    velocypack::Slice definition, bool generateKey,
    LogicalCollection& collection, bool isClusterConstructor) const {
  auto id = validateSlice(definition, generateKey, isClusterConstructor);
  auto type = definition.get(StaticStrings::IndexType);

  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid index type definition");
  }

  auto& factory = IndexFactory::factory(type.copyString());
  std::shared_ptr<Index> index =
      factory.instantiate(collection, definition, id, isClusterConstructor);

  if (!index) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "failed to instantiate index, factory returned null instance");
  }

  return index;
}

/// same for both storage engines
std::vector<std::string> IndexFactory::supportedIndexes() const {
  return std::vector<std::string>{"primary",  "edge",     "hash",
                                  "skiplist", "ttl",      "persistent",
                                  "geo",      "fulltext", "zkd"};
}

std::unordered_map<std::string, std::string> IndexFactory::indexAliases()
    const {
  return std::unordered_map<std::string, std::string>();
}

IndexId IndexFactory::validateSlice(velocypack::Slice info, bool generateKey,
                                    bool isClusterConstructor) {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "expecting object for index definition");
  }

  IndexId iid = IndexId::none();
  auto value = info.get(StaticStrings::IndexId);

  if (value.isString()) {
    iid = IndexId{basics::StringUtils::uint64(value.copyString())};
  } else if (value.isNumber()) {
    iid = IndexId{basics::VelocyPackHelper::getNumericValue<IndexId::BaseType>(
        info, StaticStrings::IndexId.c_str(), 0)};
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "cannot restore index without index identifier");
  }

  if (iid.empty() && !isClusterConstructor) {
    // Restore is not allowed to generate an id
    VPackSlice type = info.get(StaticStrings::IndexType);
    // dont generate ids for indexes of type "primary"
    // id 0 is expected for primary indexes
    if (!type.isString() || !type.isEqualString("primary")) {
      TRI_ASSERT(generateKey);
      iid = Index::generateId();
    }
  }

  return iid;
}

Result IndexFactory::validateFieldsDefinition(VPackSlice definition,
                                              size_t minFields,
                                              size_t maxFields,
                                              bool allowSubAttributes) {
  if (basics::VelocyPackHelper::getBooleanValue(definition,
                                                StaticStrings::Error, false)) {
    // We have an error here.
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  std::unordered_set<velocypack::StringRef> fields;
  auto fieldsSlice = definition.get(StaticStrings::IndexFields);

  if (fieldsSlice.isArray()) {
    std::regex const idRegex("^(.+\\.)?" + StaticStrings::IdString + "$",
                             std::regex::ECMAScript);

    // "fields" is a list of fields
    for (VPackSlice it : VPackArrayIterator(fieldsSlice)) {
      if (!it.isString()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "index field names must be non-empty strings");
      }

      velocypack::StringRef f(it);

      if (f.empty()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "index field names must be non-empty strings");
      }

      if (fields.find(f) != fields.end()) {
        // duplicate attribute name
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "duplicate attribute name in index fields list");
      }

      if (!allowSubAttributes && f.find('.') != std::string::npos) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "cannot index a sub-attribute in this type of index");
      }

      if (std::regex_match(f.toString(), idRegex)) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "_id attribute cannot be indexed");
      }

      fields.insert(f);
    }
  }

  size_t cc = fields.size();
  if (cc < minFields || cc > maxFields) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "invalid number of index attributes");
  }

  return Result();
}

/// @brief process the fields list, deduplicate it, and add it to the json
Result IndexFactory::processIndexFields(VPackSlice definition,
                                        VPackBuilder& builder, size_t minFields,
                                        size_t maxFields, bool create,
                                        bool allowExpansion,
                                        bool allowSubAttributes) {
  TRI_ASSERT(builder.isOpenObject());

  Result res = validateFieldsDefinition(definition, minFields, maxFields,
                                        allowSubAttributes);
  if (res.fail()) {
    return res;
  }

  auto fieldsSlice = definition.get(StaticStrings::IndexFields);

  TRI_ASSERT(fieldsSlice.isArray());

  builder.add(velocypack::Value(StaticStrings::IndexFields));
  builder.openArray();

  // "fields" is a list of fields when we have got here
  for (VPackSlice it : VPackArrayIterator(fieldsSlice)) {
    std::vector<basics::AttributeName> temp;
    TRI_ParseAttributeString(it.stringRef(), temp, allowExpansion);

    builder.add(it);
  }

  builder.close();
  return Result();
}

/// @brief process the unique flag and add it to the json
void IndexFactory::processIndexUniqueFlag(VPackSlice definition,
                                          VPackBuilder& builder) {
  bool unique = basics::VelocyPackHelper::getBooleanValue(
      definition, StaticStrings::IndexUnique.c_str(), false);

  builder.add(StaticStrings::IndexUnique, velocypack::Value(unique));
}

/// @brief process the sparse flag and add it to the json
void IndexFactory::processIndexSparseFlag(VPackSlice definition,
                                          VPackBuilder& builder, bool create) {
  if (definition.hasKey(StaticStrings::IndexSparse)) {
    bool sparseBool = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexSparse.c_str(), false);

    builder.add(StaticStrings::IndexSparse, velocypack::Value(sparseBool));
  } else if (create) {
    // not set. now add a default value
    builder.add(StaticStrings::IndexSparse, velocypack::Value(false));
  }
}

/// @brief process the deduplicate flag and add it to the json
void IndexFactory::processIndexDeduplicateFlag(VPackSlice definition,
                                               VPackBuilder& builder) {
  bool dup = basics::VelocyPackHelper::getBooleanValue(definition,
                                                       "deduplicate", true);
  builder.add("deduplicate", VPackValue(dup));
}

/// @brief process the geojson flag and add it to the json
void IndexFactory::processIndexGeoJsonFlag(VPackSlice definition,
                                           VPackBuilder& builder) {
  auto fieldsSlice = definition.get(StaticStrings::IndexFields);

  if (fieldsSlice.isArray() && fieldsSlice.length() == 1) {
    // only add geoJson for indexes with a single field (with needs to be an
    // array)
    bool geoJson =
        basics::VelocyPackHelper::getBooleanValue(definition, "geoJson", false);

    builder.add("geoJson", VPackValue(geoJson));
  }
}

/// @brief enhances the json of a hash, skiplist or persistent index
Result IndexFactory::enhanceJsonIndexGeneric(VPackSlice definition,
                                             VPackBuilder& builder,
                                             bool create) {
  Result res =
      processIndexFields(definition, builder, 1, INT_MAX, create, true);

  if (res.ok()) {
    processIndexSparseFlag(definition, builder, create);
    processIndexUniqueFlag(definition, builder);
    processIndexDeduplicateFlag(definition, builder);

    bool bck = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexInBackground, false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a ttl index
Result IndexFactory::enhanceJsonIndexTtl(VPackSlice definition,
                                         VPackBuilder& builder, bool create) {
  Result res =
      processIndexFields(definition, builder, 1, 1, create, false, false);

  auto value = definition.get(StaticStrings::IndexUnique);
  if (value.isBoolean() && value.getBoolean()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "a TTL index cannot be unique");
  }

  if (res.ok()) {
    // a TTL index is always non-unique but sparse!
    builder.add(StaticStrings::IndexUnique, velocypack::Value(false));
    builder.add(StaticStrings::IndexSparse, velocypack::Value(true));

    VPackSlice v = definition.get(StaticStrings::IndexExpireAfter);
    if (!v.isNumber()) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "expireAfter attribute must be a number");
    }
    double d = v.getNumericValue<double>();
    if (d < 0.0) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "expireAfter attribute must greater equal to zero");
    }
    builder.add(StaticStrings::IndexExpireAfter, v);

    bool bck = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexInBackground, false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a geo, geo1 or geo2 index
Result IndexFactory::enhanceJsonIndexGeo(VPackSlice definition,
                                         VPackBuilder& builder, bool create,
                                         int minFields, int maxFields) {
  Result res = processIndexFields(definition, builder, minFields, maxFields,
                                  create, false);

  if (res.ok()) {
    builder.add(StaticStrings::IndexSparse, velocypack::Value(true));
    builder.add(StaticStrings::IndexUnique, velocypack::Value(false));
    IndexFactory::processIndexGeoJsonFlag(definition, builder);

    bool bck = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexInBackground, false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a fulltext index
Result IndexFactory::enhanceJsonIndexFulltext(VPackSlice definition,
                                              VPackBuilder& builder,
                                              bool create) {
  Result res = processIndexFields(definition, builder, 1, 1, create, false);

  if (res.ok()) {
    // hard-coded defaults
    builder.add(StaticStrings::IndexSparse, velocypack::Value(true));
    builder.add(StaticStrings::IndexUnique, velocypack::Value(false));

    // handle "minLength" attribute
    int minWordLength = FulltextIndexLimits::minWordLengthDefault;
    VPackSlice minLength = definition.get("minLength");

    if (minLength.isNumber()) {
      minWordLength = minLength.getNumericValue<int>();
    } else if (!minLength.isNull() && !minLength.isNone()) {
      return Result(TRI_ERROR_BAD_PARAMETER);
    }

    if (minWordLength <= 0) {
      minWordLength = 1;
    }

    builder.add("minLength", VPackValue(minWordLength));

    bool bck = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexInBackground, false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a zkd index
Result IndexFactory::enhanceJsonIndexZkd(VPackSlice definition,
                                         VPackBuilder& builder, bool create) {
  if (auto fieldValueTypes = definition.get("fieldValueTypes");
      !fieldValueTypes.isString() || !fieldValueTypes.isEqualString("double")) {
    return Result(
        TRI_ERROR_BAD_PARAMETER,
        "zkd index requires `fieldValueTypes` to be set to `double` - future "
        "releases might lift this requirement");
  }

  builder.add("fieldValueTypes", VPackValue("double"));
  Result res =
      processIndexFields(definition, builder, 1, INT_MAX, create, false);

  if (res.ok()) {
    if (auto isSparse = definition.get(StaticStrings::IndexSparse).isTrue();
        isSparse) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "zkd index does not support sparse property");
    }

    processIndexUniqueFlag(definition, builder);

    bool bck = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexInBackground, false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

}  // namespace arangodb
