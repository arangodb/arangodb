////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "IndexFactory.h"
#include "Indexes/Index.h"
#include "RestServer/BootstrapFeature.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

namespace {

struct InvalidIndexFactory : public arangodb::IndexTypeFactory {
  bool equal(arangodb::velocypack::Slice const&,
             arangodb::velocypack::Slice const&) const override {
    return false;  // invalid definitions are never equal
  }

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>&,
                               arangodb::LogicalCollection&,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t, bool) const override {
    std::string type = arangodb::basics::VelocyPackHelper::getStringValue(
        definition, arangodb::StaticStrings::IndexType, "");
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER,
                            "invalid index type '" + type + "'");
  }

  arangodb::Result normalize(          // normalize definition
      arangodb::velocypack::Builder&,  // normalized definition (out-param)
      arangodb::velocypack::Slice definition,  // source definition
      bool,                                    // definition for index creation
      TRI_vocbase_t const&                     // index vocbase
      ) const override {
    std::string type = arangodb::basics::VelocyPackHelper::getStringValue(
        definition, arangodb::StaticStrings::IndexType, "");
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER,
                            "invalid index type '" + type + "'");
  }
};

InvalidIndexFactory const INVALID;

}  // namespace

namespace arangodb {

bool IndexTypeFactory::equal(arangodb::Index::IndexType type,
                             arangodb::velocypack::Slice const& lhs,
                             arangodb::velocypack::Slice const& rhs,
                             bool attributeOrderMatters) const {
  // unique must be identical if present
  auto value = lhs.get(arangodb::StaticStrings::IndexUnique);

  if (value.isBoolean()) {
    if (arangodb::basics::VelocyPackHelper::compare(value, rhs.get(arangodb::StaticStrings::IndexUnique),
                                                    false)) {
      return false;
    }
  }

  // sparse must be identical if present
  value = lhs.get(arangodb::StaticStrings::IndexSparse);

  if (value.isBoolean()) {
    if (arangodb::basics::VelocyPackHelper::compare(value, rhs.get(arangodb::StaticStrings::IndexSparse),
                                                    false)) {
      return false;
    }
  }

  if (arangodb::Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX == type ||
      arangodb::Index::IndexType::TRI_IDX_TYPE_GEO_INDEX == type) {
    // geoJson must be identical if present
    value = lhs.get("geoJson");

    if (value.isBoolean() &&
        arangodb::basics::VelocyPackHelper::compare(value, rhs.get("geoJson"), false)) {
      return false;
    }
  } else if (arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX == type) {
    // minLength
    value = lhs.get("minLength");

    if (value.isNumber() &&
        arangodb::basics::VelocyPackHelper::compare(value, rhs.get("minLength"), false)) {
      return false;
    }
  }

  // other index types: fields must be identical if present
  value = lhs.get(arangodb::StaticStrings::IndexFields);

  if (value.isArray()) {
    if (!attributeOrderMatters) {
      // attributes can be specified in any order
      arangodb::velocypack::ValueLength const nv = value.length();

      // compare fields in arbitrary order
      auto r = rhs.get(arangodb::StaticStrings::IndexFields);

      if (!r.isArray() || nv != r.length()) {
        return false;
      }

      for (size_t i = 0; i < nv; ++i) {
        arangodb::velocypack::Slice const v = value.at(i);

        bool found = false;

        for (auto const& vr : VPackArrayIterator(r)) {
          if (arangodb::basics::VelocyPackHelper::compare(v, vr, false) == 0) {
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
      if (arangodb::basics::VelocyPackHelper::compare(
              value, rhs.get(arangodb::StaticStrings::IndexFields), false) != 0) {
        return false;
      }
    }
  }

  return true;
}

void IndexFactory::clear() { _factories.clear(); }

Result IndexFactory::emplace(std::string const& type, IndexTypeFactory const& factory) {
  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature(
      "Bootstrap");
  auto* bootstrapFeature = dynamic_cast<BootstrapFeature*>(feature);

  // ensure new factories are not added at runtime since that would require
  // additional locks
  if (bootstrapFeature && bootstrapFeature->isReady()) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            std::string("index factory registration is only "
                                        "allowed during server startup"));
  }

  if (!_factories.emplace(type, &factory).second) {
    return arangodb::Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER, std::string("index factory previously registered during index factory "
                                                                               "registration for index type '") +
                                                                       type +
                                                                       "'");
  }

  return arangodb::Result();
}

Result IndexFactory::enhanceIndexDefinition(  // normalizze deefinition
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
                     arangodb::velocypack::Value(std::to_string(id)));
    }

    auto nameSlice = definition.get(StaticStrings::IndexName);
    std::string name;

    if (nameSlice.isString() && (nameSlice.getStringLength() != 0)) {
      name = nameSlice.copyString();
    } else {
      // we should set the name for special types explicitly elsewhere, but just in case...
      if (Index::type(type.copyString()) == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        name = StaticStrings::IndexNamePrimary;
      } else if (Index::type(type.copyString()) == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        name = StaticStrings::IndexNameEdge;
      } else {
        // generate a name
        name = "idx_" + std::to_string(TRI_HybridLogicalClock());
      }
    }

    if (!TRI_vocbase_t::IsAllowedName(false, velocypack::StringRef(name))) {
      return Result(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }

    normalized.add(StaticStrings::IndexName, arangodb::velocypack::Value(name));

    return factory.normalize(normalized, definition, isCreation, vocbase);
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "unknown exception");
  }
}

const IndexTypeFactory& IndexFactory::factory(std::string const& type) const noexcept {
  auto itr = _factories.find(type);
  TRI_ASSERT(itr == _factories.end() || false == !(itr->second));  // IndexFactory::emplace(...) inserts non-nullptr

  return itr == _factories.end() ? INVALID : *(itr->second);
}

std::shared_ptr<Index> IndexFactory::prepareIndexFromSlice(velocypack::Slice definition,
                                                           bool generateKey,
                                                           LogicalCollection& collection,
                                                           bool isClusterConstructor) const {
  auto id = validateSlice(definition, generateKey, isClusterConstructor);
  auto type = definition.get(StaticStrings::IndexType);

  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid index type definition");
  }

  auto& factory = IndexFactory::factory(type.copyString());
  std::shared_ptr<Index> index;
  auto res = factory.instantiate(index, collection, definition, id, isClusterConstructor);

  if (!res.ok()) {
    TRI_set_errno(res.errorNumber());
    LOG_TOPIC("77be6", ERR, arangodb::Logger::ENGINES)
        << "failed to instantiate index, error: " << res.errorNumber() << " "
        << res.errorMessage();

    return nullptr;
  }

  if (!index) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_TOPIC("5384c", ERR, arangodb::Logger::ENGINES)
        << "failed to instantiate index, factory returned null instance";

    return nullptr;
  }

  return index;
}

/// same for both storage engines
std::vector<std::string> IndexFactory::supportedIndexes() const {
  return std::vector<std::string>{"primary", "edge",       "hash", "skiplist",
                                  "ttl",     "persistent", "geo",  "fulltext"};
}

std::unordered_map<std::string, std::string> IndexFactory::indexAliases() const {
  return std::unordered_map<std::string, std::string>();
}

TRI_idx_iid_t IndexFactory::validateSlice(arangodb::velocypack::Slice info,
                                          bool generateKey, bool isClusterConstructor) {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "expecting object for index definition");
  }

  TRI_idx_iid_t iid = 0;
  auto value = info.get(StaticStrings::IndexId);

  if (value.isString()) {
    iid = basics::StringUtils::uint64(value.copyString());
  } else if (value.isNumber()) {
    iid = basics::VelocyPackHelper::getNumericValue<TRI_idx_iid_t>(
        info, StaticStrings::IndexId.c_str(), 0);
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot restore index without index identifier");
  }

  if (iid == 0 && !isClusterConstructor) {
    // Restore is not allowed to generate an id
    VPackSlice type = info.get(StaticStrings::IndexType);
    // dont generate ids for indexes of type "primary"
    // id 0 is expected for primary indexes
    if (!type.isString() || !type.isEqualString("primary")) {
      TRI_ASSERT(generateKey);
      iid = arangodb::Index::generateId();
    }
  }

  return iid;
}

/// @brief process the fields list, deduplicate it, and add it to the json
Result IndexFactory::processIndexFields(VPackSlice definition, VPackBuilder& builder,
                                        size_t minFields, size_t maxField,
                                        bool create, bool allowExpansion) {
  TRI_ASSERT(builder.isOpenObject());
  std::unordered_set<arangodb::velocypack::StringRef> fields;
  auto fieldsSlice = definition.get(arangodb::StaticStrings::IndexFields);

  builder.add(arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields));
  builder.openArray();

  if (fieldsSlice.isArray()) {
    // "fields" is a list of fields
    for (auto const& it : VPackArrayIterator(fieldsSlice)) {
      if (!it.isString()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "index field names must be strings");
      }

      arangodb::velocypack::StringRef f(it);

      if (f.empty() || (create && f == StaticStrings::IdString)) {
        // accessing internal attributes is disallowed
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "_id attribute cannot be indexed");
      }

      if (fields.find(f) != fields.end()) {
        // duplicate attribute name
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "duplicate attribute name in index fields list");
      }

      std::vector<basics::AttributeName> temp;
      TRI_ParseAttributeString(f, temp, allowExpansion);

      fields.insert(f);
      builder.add(it);
    }
  }

  size_t cc = fields.size();
  if (cc == 0 || cc < minFields || cc > maxField) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "invalid number of index attributes");
  }

  builder.close();
  return Result();
}

/// @brief process the unique flag and add it to the json
void IndexFactory::processIndexUniqueFlag(VPackSlice definition, VPackBuilder& builder) {
  bool unique = basics::VelocyPackHelper::getBooleanValue(
      definition, arangodb::StaticStrings::IndexUnique.c_str(), false);

  builder.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(unique));
}

/// @brief process the sparse flag and add it to the json
void IndexFactory::processIndexSparseFlag(VPackSlice definition,
                                          VPackBuilder& builder, bool create) {
  if (definition.hasKey(arangodb::StaticStrings::IndexSparse)) {
    bool sparseBool = basics::VelocyPackHelper::getBooleanValue(
        definition, arangodb::StaticStrings::IndexSparse.c_str(), false);

    builder.add(arangodb::StaticStrings::IndexSparse,
                arangodb::velocypack::Value(sparseBool));
  } else if (create) {
    // not set. now add a default value
    builder.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(false));
  }
}

/// @brief process the deduplicate flag and add it to the json
void IndexFactory::processIndexDeduplicateFlag(VPackSlice definition, VPackBuilder& builder) {
  bool dup = basics::VelocyPackHelper::getBooleanValue(definition, "deduplicate", true);
  builder.add("deduplicate", VPackValue(dup));
}

/// @brief process the geojson flag and add it to the json
void IndexFactory::processIndexGeoJsonFlag(VPackSlice definition, VPackBuilder& builder) {
  auto fieldsSlice = definition.get(arangodb::StaticStrings::IndexFields);

  if (fieldsSlice.isArray() && fieldsSlice.length() == 1) {
    // only add geoJson for indexes with a single field (with needs to be an array)
    bool geoJson =
        basics::VelocyPackHelper::getBooleanValue(definition, "geoJson", false);

    builder.add("geoJson", VPackValue(geoJson));
  }
}

/// @brief enhances the json of a hash, skiplist or persistent index
Result IndexFactory::enhanceJsonIndexGeneric(VPackSlice definition,
                                             VPackBuilder& builder, bool create) {
  Result res = processIndexFields(definition, builder, 1, INT_MAX, create, true);

  if (res.ok()) {
    processIndexSparseFlag(definition, builder, create);
    processIndexUniqueFlag(definition, builder);
    processIndexDeduplicateFlag(definition, builder);

    bool bck = basics::VelocyPackHelper::getBooleanValue(definition, StaticStrings::IndexInBackground,
                                                         false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a ttl index
Result IndexFactory::enhanceJsonIndexTtl(VPackSlice definition,
                                         VPackBuilder& builder, bool create) {
  Result res = processIndexFields(definition, builder, 1, 1, create, false);

  if (res.ok()) {
    // a TTL index is always non-unique but sparse!
    builder.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(false));
    builder.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(true));

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
    builder.add(arangodb::StaticStrings::IndexExpireAfter, v);

    bool bck = basics::VelocyPackHelper::getBooleanValue(definition, StaticStrings::IndexInBackground,
                                                         false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a geo, geo1 or geo2 index
Result IndexFactory::enhanceJsonIndexGeo(VPackSlice definition, VPackBuilder& builder,
                                         bool create, int minFields, int maxFields) {
  Result res = processIndexFields(definition, builder, minFields, maxFields, create, false);

  if (res.ok()) {
    builder.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(true));
    builder.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(false));
    IndexFactory::processIndexGeoJsonFlag(definition, builder);

    bool bck = basics::VelocyPackHelper::getBooleanValue(definition, StaticStrings::IndexInBackground,
                                                         false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

/// @brief enhances the json of a fulltext index
Result IndexFactory::enhanceJsonIndexFulltext(VPackSlice definition,
                                              VPackBuilder& builder, bool create) {
  Result res = processIndexFields(definition, builder, 1, 1, create, false);

  if (res.ok()) {
    // hard-coded defaults
    builder.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(true));
    builder.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(false));

    // handle "minLength" attribute
    int minWordLength = FulltextIndexLimits::minWordLengthDefault;
    VPackSlice minLength = definition.get("minLength");

    if (minLength.isNumber()) {
      minWordLength = minLength.getNumericValue<int>();
    } else if (!minLength.isNull() && !minLength.isNone()) {
      return Result(TRI_ERROR_BAD_PARAMETER);
    }

    builder.add("minLength", VPackValue(minWordLength));

    bool bck = basics::VelocyPackHelper::getBooleanValue(definition, StaticStrings::IndexInBackground,
                                                         false);
    builder.add(StaticStrings::IndexInBackground, VPackValue(bck));
  }

  return res;
}

}  // namespace arangodb
