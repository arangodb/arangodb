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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include <date/date.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include "Index.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/datetime.h"
#include "Cluster/ServerState.h"
#include "Containers/HashSet.h"
#include "IResearch/IResearchCommon.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace std::chrono;
using namespace date;

namespace {

/// @brief the _key attribute, which, when used in an index, will implictly make it unique
/// (note that we must not refer to StaticStrings::KeyString here to avoid an init-order-fiasco
std::vector<arangodb::basics::AttributeName> const KeyAttribute{
    arangodb::basics::AttributeName("_key", false)};

bool hasExpansion(std::vector<std::vector<arangodb::basics::AttributeName>> const& fields) {
  for (auto const& it : fields) {
    if (TRI_AttributeNamesHaveExpansion(it)) {
      return true;
    }
  }
  return false;
}

/// @brief set fields from slice
std::vector<std::vector<arangodb::basics::AttributeName>> parseFields(VPackSlice const& fields,
                                                                      bool allowExpansion) {
  std::vector<std::vector<arangodb::basics::AttributeName>> result;
  if (!fields.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                   "invalid index description");
  }

  size_t const n = static_cast<size_t>(fields.length());
  result.reserve(n);

  for (VPackSlice name : VPackArrayIterator(fields)) {
    if (!name.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                     "invalid index description");
    }

    std::vector<arangodb::basics::AttributeName> parsedAttributes;
    TRI_ParseAttributeString(name.copyString(), parsedAttributes, allowExpansion);
    result.emplace_back(std::move(parsedAttributes));
  }
  return result;
}

bool canBeNull(arangodb::aql::AstNode const* op, arangodb::aql::AstNode const* access,
               std::unordered_set<std::string> const& nonNullAttributes) {
  TRI_ASSERT(op != nullptr);
  TRI_ASSERT(access != nullptr);

  if (access->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS &&
      access->getMemberUnchecked(0)->type == arangodb::aql::NODE_TYPE_REFERENCE) {
    // a.b
    // now check if the accessed attribute is _key, _rev or _id.
    // all of these cannot be null
    auto attributeName = access->getStringRef();
    if (attributeName == arangodb::StaticStrings::KeyString ||
        attributeName == arangodb::StaticStrings::IdString ||
        attributeName == arangodb::StaticStrings::RevString) {
      return false;
    }
  }

  if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT ||
      op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE ||
      op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    if (op->getExcludesNull()) {
      // already proven that the attribute cannot become "null"
      return false;
    }
  }

  try {
    if (nonNullAttributes.find(access->toString()) != nonNullAttributes.end()) {
      // found an attribute marked as non-null
      return false;
    }
  } catch (...) {
    // stringification may throw
  }

  // for everything else we are unsure
  return true;
}

void markAsNonNull(arangodb::aql::AstNode const* op, arangodb::aql::AstNode const* access,
                   std::unordered_set<std::string>& nonNullAttributes) {
  TRI_ASSERT(op != nullptr);
  TRI_ASSERT(access != nullptr);

  if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT ||
      op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE ||
      op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // non-null marking currently only supported for these node types
    const_cast<arangodb::aql::AstNode*>(op)->setExcludesNull(true);
  }
  // all other node types will be ignored here

  try {
    nonNullAttributes.emplace(access->toString());
  } catch (...) {
    // stringification may throw
  }
}

bool typeMatch(char const* type, size_t len, char const* expected) {
  return (len == ::strlen(expected)) && (::memcmp(type, expected, len) == 0);
}

std::string defaultIndexName(VPackSlice const& slice) {
  auto type =
      arangodb::Index::type(slice.get(arangodb::StaticStrings::IndexType).copyString());
  if (type == arangodb::Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return arangodb::StaticStrings::IndexNamePrimary;
  } else if (type == arangodb::Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
    if (arangodb::EngineSelectorFeature::isRocksDB()) {
      auto fields = slice.get(arangodb::StaticStrings::IndexFields);
      TRI_ASSERT(fields.isArray());
      auto firstField = fields.at(0);
      TRI_ASSERT(firstField.isString());
      bool isFromIndex = firstField.isEqualString(arangodb::StaticStrings::FromString);
      return isFromIndex ? arangodb::StaticStrings::IndexNameEdgeFrom
                         : arangodb::StaticStrings::IndexNameEdgeTo;
    }
    return arangodb::StaticStrings::IndexNameEdge;
  }

  std::string idString = arangodb::basics::VelocyPackHelper::getStringValue(
      slice, arangodb::StaticStrings::IndexId.c_str(),
      std::to_string(TRI_NewTickServer()));
  return std::string("idx_").append(idString);
}

}  // namespace

namespace arangodb {

Index::FilterCosts Index::FilterCosts::zeroCosts() {
  Index::FilterCosts costs;
  costs.supportsCondition = true;
  costs.coveredAttributes = 0;
  costs.estimatedItems = 0;
  costs.estimatedCosts = 0;
  return costs;
}
    
Index::FilterCosts Index::FilterCosts::defaultCosts(size_t itemsInIndex) {
  Index::FilterCosts costs;
  costs.supportsCondition = false;
  costs.coveredAttributes = 0;
  costs.estimatedItems = itemsInIndex;
  costs.estimatedCosts = static_cast<double>(itemsInIndex);
  return costs;
}

Index::SortCosts Index::SortCosts::zeroCosts(size_t coveredAttributes) {
  Index::SortCosts costs;
  costs.coveredAttributes = coveredAttributes;
  costs.supportsCondition = true;
  costs.estimatedCosts = 0;
  return costs;
}
    
Index::SortCosts Index::SortCosts::defaultCosts(size_t itemsInIndex, bool isPersistent) {
  Index::SortCosts costs;
  TRI_ASSERT(!costs.supportsCondition);
  costs.coveredAttributes = 0;
  costs.estimatedCosts = itemsInIndex > 0 ? (itemsInIndex * std::log2(static_cast<double>(itemsInIndex))) : 0.0;
  if (isPersistent) {
    // slightly penalize this type of index against other indexes which
    // are in memory
    costs.estimatedCosts *= 1.05;
  }
  return costs;
}

// If the Index is on a coordinator instance the index may not access the
// logical collection because it could be gone!

Index::Index(IndexId iid, arangodb::LogicalCollection& collection, std::string const& name,
             std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
             bool unique, bool sparse)
    : _iid(iid),
      _collection(collection),
      _name(name),
      _fields(fields),
      _useExpansion(::hasExpansion(_fields)),
      _unique(unique),
      _sparse(sparse) {
  // note: _collection can be a nullptr in the cluster coordinator case!!
}

Index::Index(IndexId iid, arangodb::LogicalCollection& collection, VPackSlice const& slice)
    : _iid(iid),
      _collection(collection),
      _name(arangodb::basics::VelocyPackHelper::getStringValue(
          slice, arangodb::StaticStrings::IndexName, ::defaultIndexName(slice))),
      _fields(::parseFields(slice.get(arangodb::StaticStrings::IndexFields),
                            Index::allowExpansion(Index::type(
                                slice.get(arangodb::StaticStrings::IndexType).copyString())))),
      _useExpansion(::hasExpansion(_fields)),
      _unique(arangodb::basics::VelocyPackHelper::getBooleanValue(slice, arangodb::StaticStrings::IndexUnique,
                                                                  false)),
      _sparse(arangodb::basics::VelocyPackHelper::getBooleanValue(slice, arangodb::StaticStrings::IndexSparse,
                                                                  false)) {}

Index::~Index() = default;

void Index::name(std::string const& newName) {
  if (_name.empty()) {
    _name = newName;
  }
}

size_t Index::sortWeight(arangodb::aql::AstNode const* node) {
  switch (node->type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      return 1;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
      return 2;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      return 3;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      return 4;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      return 5;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      return 6;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE:
      return 7;
    default:
      return 42; /* OPST_CIRCUS */
  }
}

/// @brief validate fields from slice
void Index::validateFields(VPackSlice const& slice) {
  VPackValueLength len;
  const char* idxStr = slice.get(arangodb::StaticStrings::IndexType).getString(len);
  auto allowExpansion = Index::allowExpansion(Index::type(idxStr, len));

  auto fields = slice.get(arangodb::StaticStrings::IndexFields);
  if (!fields.isArray()) {
    return;
  }

  for (VPackSlice name : VPackArrayIterator(fields)) {
    if (!name.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                     "invalid index description");
    }

    std::vector<arangodb::basics::AttributeName> parsedAttributes;
    TRI_ParseAttributeString(name.copyString(), parsedAttributes, allowExpansion);
  }
}

/// @brief return the index type based on a type name
Index::IndexType Index::type(char const* type, size_t len) {
  if (::typeMatch(type, len, "primary")) {
    return TRI_IDX_TYPE_PRIMARY_INDEX;
  }
  if (::typeMatch(type, len, "edge")) {
    return TRI_IDX_TYPE_EDGE_INDEX;
  }
  if (::typeMatch(type, len, "hash")) {
    return TRI_IDX_TYPE_HASH_INDEX;
  }
  if (::typeMatch(type, len, "skiplist")) {
    return TRI_IDX_TYPE_SKIPLIST_INDEX;
  }
  if (::typeMatch(type, len, "ttl")) {
    return TRI_IDX_TYPE_TTL_INDEX;
  }
  if (::typeMatch(type, len, "persistent") ||
      ::typeMatch(type, len, "rocksdb")) {
    return TRI_IDX_TYPE_PERSISTENT_INDEX;
  }
  if (::typeMatch(type, len, "fulltext")) {
    return TRI_IDX_TYPE_FULLTEXT_INDEX;
  }
  if (::typeMatch(type, len, "geo")) {
    return TRI_IDX_TYPE_GEO_INDEX;
  }
  if (::typeMatch(type, len, "geo1")) {
    return TRI_IDX_TYPE_GEO1_INDEX;
  }
  if (::typeMatch(type, len, "geo2")) {
    return TRI_IDX_TYPE_GEO2_INDEX;
  }
  std::string const& tmp = arangodb::iresearch::DATA_SOURCE_TYPE.name();
  if (::typeMatch(type, len, tmp.c_str())) {
    return TRI_IDX_TYPE_IRESEARCH_LINK;
  }
  if (::typeMatch(type, len, "noaccess")) {
    return TRI_IDX_TYPE_NO_ACCESS_INDEX;
  }

  return TRI_IDX_TYPE_UNKNOWN;
}

Index::IndexType Index::type(std::string const& type) {
  return Index::type(type.c_str(), type.size());
}

/// @brief return the name of an index type
char const* Index::oldtypeName(Index::IndexType type) {
  switch (type) {
    case TRI_IDX_TYPE_PRIMARY_INDEX:
      return "primary";
    case TRI_IDX_TYPE_EDGE_INDEX:
      return "edge";
    case TRI_IDX_TYPE_HASH_INDEX:
      return "hash";
    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      return "skiplist";
    case TRI_IDX_TYPE_TTL_INDEX:
      return "ttl";
    case TRI_IDX_TYPE_PERSISTENT_INDEX:
      return "persistent";
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
      return "fulltext";
    case TRI_IDX_TYPE_GEO1_INDEX:
      return "geo1";
    case TRI_IDX_TYPE_GEO2_INDEX:
      return "geo2";
    case TRI_IDX_TYPE_GEO_INDEX:
      return "geo";
    case TRI_IDX_TYPE_IRESEARCH_LINK:
      return arangodb::iresearch::DATA_SOURCE_TYPE.name().c_str();
    case TRI_IDX_TYPE_NO_ACCESS_INDEX:
      return "noaccess";
    case TRI_IDX_TYPE_UNKNOWN: {
    }
  }

  return "";
}

/// @brief validate an index id
bool Index::validateId(char const* key) {
  char const* p = key;

  while (1) {
    char const c = *p;

    if (c == '\0') {
      return (p - key) > 0;
    }
    if (c >= '0' && c <= '9') {
      ++p;
      continue;
    }

    return false;
  }
}

/// @brief validate an index name
bool Index::validateName(char const* key) {
  return TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(key, strlen(key)));
}

namespace {
bool validatePrefix(char const* key, size_t* split) {
  char const* p = key;

  // find divider
  while (1) {
    char c = *p;

    if (c == '\0') {
      return false;
    }

    if (c == '/') {
      break;
    }

    p++;
  }

  // store split position
  *split = p - key;

  return TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef(key, *split));
}
}  // namespace

/// @brief validate an index handle (collection name + / + index id)
bool Index::validateHandle(char const* key, size_t* split) {
  bool ok = validatePrefix(key, split);
  // validate index id
  return ok && validateId(key + *split + 1);
}

/// @brief validate an index handle (collection name + / + index name)
bool Index::validateHandleName(char const* key, size_t* split) {
  bool ok = validatePrefix(key, split);
  // validate index id
  return ok && validateName(key + *split + 1);
}

/// @brief generate a new index id
IndexId Index::generateId() { return IndexId{TRI_NewTickServer()}; }

/// @brief check if two index definitions share any identifiers (_id, name)
bool Index::CompareIdentifiers(velocypack::Slice const& lhs, velocypack::Slice const& rhs) {
  VPackSlice lhsId = lhs.get(arangodb::StaticStrings::IndexId);
  VPackSlice rhsId = rhs.get(arangodb::StaticStrings::IndexId);
  if (lhsId.isString() && rhsId.isString() &&
      arangodb::basics::VelocyPackHelper::equal(lhsId, rhsId, true)) {
    return true;
  }

  VPackSlice lhsName = lhs.get(arangodb::StaticStrings::IndexName);
  VPackSlice rhsName = rhs.get(arangodb::StaticStrings::IndexName);
  if (lhsName.isString() && rhsName.isString() &&
      arangodb::basics::VelocyPackHelper::equal(lhsName, rhsName, true)) {
    return true;
  }

  return false;
}

/// @brief index comparator, used by the coordinator to detect if two index
/// contents are the same
bool Index::Compare(VPackSlice const& lhs, VPackSlice const& rhs,
                    std::string const& dbname) {
  auto lhsType = lhs.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(lhsType.isString());

  // type must be identical
  if (!arangodb::basics::VelocyPackHelper::equal(lhsType, rhs.get(arangodb::StaticStrings::IndexType), false)) {
    return false;
  }

  auto* engine = EngineSelectorFeature::ENGINE;

  return engine && engine->indexFactory().factory(lhsType.copyString()).equal(lhs, rhs, dbname);
}

/// @brief return a contextual string for logging
std::string Index::context() const {
  std::ostringstream result;

  result << "index { id: " << id() << ", type: " << oldtypeName()
         << ", collection: " << _collection.vocbase().name() << "/"
         << _collection.name() << ", unique: " << (_unique ? "true" : "false")
         << ", fields: ";
  result << "[";

  for (size_t i = 0; i < _fields.size(); ++i) {
    if (i > 0) {
      result << ", ";
    }

    result << _fields[i];
  }

  result << "] }";

  return result.str();
}

/// @brief create a VelocyPack representation of the index
/// base functionality (called from derived classes)
std::shared_ptr<VPackBuilder> Index::toVelocyPack(std::underlying_type<Index::Serialize>::type flags) const {
  auto builder = std::make_shared<VPackBuilder>();
  toVelocyPack(*builder, flags);
  return builder;
}

/// @brief create a VelocyPack representation of the index
/// base functionality (called from derived classes)
/// note: needs an already-opened object as its input!
void Index::toVelocyPack(VPackBuilder& builder,
                         std::underlying_type<Index::Serialize>::type flags) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(arangodb::StaticStrings::IndexId,
              arangodb::velocypack::Value(std::to_string(_iid.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              arangodb::velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName, arangodb::velocypack::Value(name()));

  builder.add(arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields));
  builder.openArray();

  for (auto const& field : fields()) {
    std::string fieldString;
    TRI_AttributeNamesToString(field, fieldString);
    builder.add(VPackValue(fieldString));
  }

  builder.close();

  if (hasSelectivityEstimate() && Index::hasFlag(flags, Index::Serialize::Estimates)) {
    builder.add("selectivityEstimate", VPackValue(selectivityEstimate()));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }
}

/// @brief create a VelocyPack representation of the index figures
/// base functionality (called from derived classes)
std::shared_ptr<VPackBuilder> Index::toVelocyPackFigures() const {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject(/*unindexed*/ true);
  toVelocyPackFigures(*builder);
  builder->close();
  return builder;
}

/// @brief create a VelocyPack representation of the index figures
/// base functionality (called from derived classes)
void Index::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("memory", VPackValue(memory()));
}

/// @brief default implementation for matchesDefinition
bool Index::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  arangodb::velocypack::StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    arangodb::velocypack::StringRef idRef(value);
    return idRef == std::to_string(_iid.id());
  }

  value = info.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }

  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, arangodb::StaticStrings::IndexUnique, false)) {
    return false;
  }

  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, arangodb::StaticStrings::IndexSparse, false)) {
    return false;
  }

  // This check takes ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (size_t i = 0; i < n; ++i) {
    translate.clear();
    VPackSlice f = value.at(i);
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    arangodb::velocypack::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate, false)) {
      return false;
    }
  }
  return true;
}

/// @brief default implementation for selectivityEstimate
double Index::selectivityEstimate(arangodb::velocypack::StringRef const&) const {
  if (_unique) {
    return 1.0;
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief whether or not the index is implicitly unique
/// this can be the case if the index is not declared as unique, but contains a
/// unique attribute such as _key
bool Index::implicitlyUnique() const {
  if (_unique) {
    // a unique index is always unique
    return true;
  }
  if (_useExpansion) {
    // when an expansion such as a[*] is used, the index may not be unique, even
    // if it contains attributes that are guaranteed to be unique
    return false;
  }

  for (auto const& it : _fields) {
    // if _key is contained in the index fields definition, then the index is
    // implicitly unique
    if (it == KeyAttribute) {
      return true;
    }
  }

  // _key not contained
  return false;
}

/// @brief default implementation for drop
Result Index::drop() {
  return Result();  // do nothing
}

/// @brief default implementation for supportsFilterCondition
Index::FilterCosts Index::supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const&,
                                                 arangodb::aql::AstNode const* /* node */,
                                                 arangodb::aql::Variable const* /* reference */, 
                                                 size_t itemsInIndex) const {
  // by default no filter conditions are supported
  return Index::FilterCosts::defaultCosts(itemsInIndex);
}

/// @brief default implementation for supportsSortCondition
Index::SortCosts Index::supportsSortCondition(arangodb::aql::SortCondition const* /* sortCondition */,
                                              arangodb::aql::Variable const* /* node */, 
                                              size_t itemsInIndex) const {
  // by default no sort conditions are supported
  return Index::SortCosts::defaultCosts(itemsInIndex, this->isPersistent());
}
  
arangodb::aql::AstNode* Index::specializeCondition(arangodb::aql::AstNode* /* node */,
                                                   arangodb::aql::Variable const* /* reference */) const {
  // the default implementation should never be called
  TRI_ASSERT(false); 
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("no default implementation for specializeCondition. index type: ") + typeName());
}

std::unique_ptr<IndexIterator> Index::iteratorForCondition(transaction::Methods* /* trx */,
                                                           aql::AstNode const* /* node */,
                                                           aql::Variable const* /* reference */,
                                                           IndexIteratorOptions const& /* opts */) {
  // the default implementation should never be called
  TRI_ASSERT(false); 
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("no default implementation for iteratorForCondition. index type: ") + typeName());
}

/// @brief perform some base checks for an index condition part
bool Index::canUseConditionPart(arangodb::aql::AstNode const* access,
                                arangodb::aql::AstNode const* other,
                                arangodb::aql::AstNode const* op,
                                arangodb::aql::Variable const* reference,
                                std::unordered_set<std::string>& nonNullAttributes,
                                bool isExecution) const {
  if (_sparse) {
    if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN &&
        (other->type == arangodb::aql::NODE_TYPE_EXPANSION ||
         other->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS)) {
      // value IN a.b  OR  value IN a.b[*]
      if (!access->isConstant()) {
        return false;
      }

      /* A sparse index will store null in Array
      if (access->isNullValue()) {
        return false;
      }
      */
    } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN &&
               access->type == arangodb::aql::NODE_TYPE_EXPANSION) {
      // value[*] IN a.b
      if (!other->isConstant()) {
        return false;
      }

      /* A sparse index will store null in Array
      if (other->isNullValue()) {
        return false;
      }
      */
    } else if (access->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      // a.b == value  OR  a.b IN values

      if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT) {
        // > anything also excludes "null". now note that this attribute cannot
        // become null range definitely exludes the "null" value
        ::markAsNonNull(op, access, nonNullAttributes);
      } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT ||
                 op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE) {
        // <  and  <= are not supported with sparse indexes as this may include
        // null values
        if (::canBeNull(op, access, nonNullAttributes)) {
          return false;
        }

        // range definitely exludes the "null" value
        ::markAsNonNull(op, access, nonNullAttributes);
      }

      if (other->isConstant()) {
        if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE && other->isNullValue()) {
          // != null. now note that a certain attribute cannot become null
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE &&
                   !other->isNullValue()) {
          // >= non-null. now note that a certain attribute cannot become null
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        }

        if (other->isNullValue() &&
            (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
             op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE)) {
          // ==  and  >= null are not supported with sparse indexes for the same
          // reason
          if (::canBeNull(op, access, nonNullAttributes)) {
            return false;
          }
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        }

        if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN &&
            other->type == arangodb::aql::NODE_TYPE_ARRAY) {
          size_t const n = other->numMembers();

          for (size_t i = 0; i < n; ++i) {
            if (other->getMemberUnchecked(i)->isNullValue()) {
              return false;
            }
          }
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        }
      } else {
        // !other->isConstant()
        if (::canBeNull(op, access, nonNullAttributes)) {
          return false;
        }

        // range definitely exludes the "null" value
        ::markAsNonNull(op, access, nonNullAttributes);
      }
    }
  }

  if (isExecution) {
    // in execution phase, we do not need to check the variable usage again
    return true;
  }

  if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE) {
    // none of the indexes can use !=, so we can exit here
    // note that this function may have been called for operator !=. this is
    // necessary to track the non-null attributes, e.g. attr != null, so we can
    // note which attributes cannot be null and still use sparse indexes for
    // these attributes
    return false;
  }

  // test if the reference variable is contained on both sides of the expression
  arangodb::aql::VarSet variables;
  if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN &&
      (other->type == arangodb::aql::NODE_TYPE_EXPANSION ||
       other->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS)) {
    // value IN a.b  OR  value IN a.b[*]
    arangodb::aql::Ast::getReferencedVariables(access, variables);
    if (variables.find(reference) != variables.end()) {
      variables.clear();
      arangodb::aql::Ast::getReferencedVariables(other, variables);
    }
  } else {
    // a.b == value  OR  a.b IN values
    if (!other->isConstant()) {
      // don't look for referenced variables if we only access a
      // constant value (there will be no variables then...)
      arangodb::aql::Ast::getReferencedVariables(other, variables);
    }
  }

  if (variables.find(reference) != variables.end()) {
    // yes. then we cannot use an index here
    return false;
  }

  return true;
}

/// @brief Transform the list of search slices to search values.
///        Always expects a list of lists as input.
///        Outer list represents the single lookups, inner list represents the
///        index field values.
///        This will multiply all IN entries and simply return all other
///        entries.
///        Example: Index on (a, b)
///        Input: [ [{=: 1}, {in: 2,3}], [{=:2}, {=:3}]
///        Result: [ [{=: 1}, {=: 2}],[{=:1}, {=:3}], [{=:2}, {=:3}]]
void Index::expandInSearchValues(VPackSlice const base, VPackBuilder& result) const {
  TRI_ASSERT(base.isArray());

  VPackArrayBuilder baseGuard(&result);
  for (VPackSlice oneLookup : VPackArrayIterator(base)) {
    TRI_ASSERT(oneLookup.isArray());

    bool usesIn = false;
    for (VPackSlice it : VPackArrayIterator(oneLookup)) {
      if (it.hasKey(StaticStrings::IndexIn)) {
        usesIn = true;
        break;
      }
    }
    if (!usesIn) {
      // Shortcut, no multiply
      // Just copy over base
      result.add(oneLookup);
      return;
    }

    std::unordered_map<size_t, std::vector<VPackSlice>> elements;
    arangodb::basics::VelocyPackHelper::VPackLess<true> sorter;
    size_t n = static_cast<size_t>(oneLookup.length());
    for (VPackValueLength i = 0; i < n; ++i) {
      VPackSlice current = oneLookup.at(i);
      if (current.hasKey(StaticStrings::IndexIn)) {
        VPackSlice inList = current.get(StaticStrings::IndexIn);
        if (!inList.isArray()) {
          // IN value is a non-array
          result.clear();
          result.openArray();
          return;
        }

        TRI_ASSERT(inList.isArray());
        VPackValueLength nList = inList.length();

        if (nList == 0) {
          // Empty Array. short circuit, no matches possible
          result.clear();
          result.openArray();
          return;
        }

        std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash, arangodb::basics::VelocyPackHelper::VPackEqual>
            tmp(static_cast<size_t>(nList),
                arangodb::basics::VelocyPackHelper::VPackHash(),
                arangodb::basics::VelocyPackHelper::VPackEqual());

        for (VPackSlice el : VPackArrayIterator(inList)) {
          tmp.emplace(el);
        }
        auto& vector = elements[i];
        vector.insert(vector.end(), tmp.begin(), tmp.end());
        std::sort(vector.begin(), vector.end(), sorter);
      }
    }
    // If there is an entry in elements for one depth it was an in,
    // all of them are now unique so we simply have to multiply

    size_t level = n - 1;
    std::vector<size_t> positions(n, 0);
    bool done = false;
    while (!done) {
      TRI_IF_FAILURE("Index::permutationIN") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      VPackArrayBuilder guard(&result);
      for (size_t i = 0; i < n; ++i) {
        auto list = elements.find(i);
        if (list == elements.end()) {
          // Insert
          result.add(oneLookup.at(i));
        } else {
          VPackObjectBuilder objGuard(&result);
          result.add(StaticStrings::IndexEq, list->second.at(positions[i]));
        }
      }
      while (true) {
        auto list = elements.find(level);
        if (list != elements.end() && ++positions[level] < list->second.size()) {
          level = n - 1;
          // abort inner iteration
          break;
        }
        positions[level] = 0;
        if (level == 0) {
          done = true;
          break;
        }
        --level;
      }
    }
  }
}

bool Index::covers(std::unordered_set<std::string> const& attributes) const {
  // check if we can use covering indexes
  if (_fields.size() < attributes.size()) {
    // we will not be able to satisfy all requested projections with this index
    return false;
  }

  std::string result;
  for (size_t i = 0; i < _fields.size(); ++i) {
    result.clear();
    TRI_AttributeNamesToString(_fields[i], result, false);
    if (std::find(attributes.begin(), attributes.end(), result) == attributes.end()) {
      return false;
    }
  }
  return true;
}

void Index::warmup(arangodb::transaction::Methods*, std::shared_ptr<basics::LocalTaskQueue>) {
  // Do nothing. If an index needs some warmup
  // it has to explicitly implement it.
}

/// @brief generate error message
/// @param key the conflicting key
Result& Index::addErrorMsg(Result& r, std::string const& key) {
  // now provide more context based on index
  r.appendErrorMessage(" - in index ");
  r.appendErrorMessage(name());
  r.appendErrorMessage(" of type ");
  r.appendErrorMessage(oldtypeName());

  // build fields string
  r.appendErrorMessage(" over '");

  for (size_t i = 0; i < _fields.size(); i++) {
    std::string msg;
    TRI_AttributeNamesToString(_fields[i], msg);
    r.appendErrorMessage(msg);
    if (i != _fields.size() - 1) {
      r.appendErrorMessage(", ");
    }
  }
  r.appendErrorMessage("'");

  // provide conflicting key
  if (!key.empty()) {
    r.appendErrorMessage("; conflicting key: ");
    r.appendErrorMessage(key);
  }
  return r;
}

double Index::getTimestamp(arangodb::velocypack::Slice const& doc,
                           std::string const& attributeName) const {
  VPackSlice value = doc.get(attributeName);

  if (value.isString()) {
    // string value. we expect it to be YYYY-MM-DD etc.
    tp_sys_clock_ms tp;
    if (basics::parseDateTime(value.stringRef(), tp)) {
      return static_cast<double>(
          std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
              .count());
    }
    // invalid date format
    // fall-through intentional
  } else if (value.isNumber()) {
    // numeric value. we take it as it is
    return value.getNumericValue<double>();
  }

  // attribute not found in document, or invalid type
  return -1.0;
}

/// @brief return the name of the (sole) index attribute
/// it is only allowed to call this method if the index contains a
/// single attribute
std::string const& Index::getAttribute() const {
  TRI_ASSERT(_fields.size() == 1);
  auto const& fields = _fields[0];
  TRI_ASSERT(fields.size() == 1);
  auto const& field = fields[0];
  TRI_ASSERT(!field.shouldExpand);
  return field.name;
}

AttributeAccessParts::AttributeAccessParts(arangodb::aql::AstNode const* comparison,
                                           arangodb::aql::Variable const* variable)
    : comparison(comparison),
      attribute(nullptr),
      value(nullptr),
      opType(arangodb::aql::NODE_TYPE_NOP) {
  // first assume a.b == value
  attribute = comparison->getMember(0);
  value = comparison->getMember(1);
  opType = comparison->type;

  if (attribute->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // got value == a.b  ->  flip the two sides
    attribute = comparison->getMember(1);
    value = comparison->getMember(0);
    opType = aql::Ast::ReverseOperator(opType);
  }

  TRI_ASSERT(attribute->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  TRI_ASSERT(attribute->isAttributeAccessForVariable(variable, true));
}

}  // namespace arangodb

/// @brief append the index description to an output stream
std::ostream& operator<<(std::ostream& stream, arangodb::Index const* index) {
  stream << index->context();
  return stream;
}

/// @brief append the index description to an output stream
std::ostream& operator<<(std::ostream& stream, arangodb::Index const& index) {
  stream << index.context();
  return stream;
}
