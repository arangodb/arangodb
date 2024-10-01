////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Index.h"

#include "Aql/Projections.h"
#include "Aql/Ast.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/datetime.h"
#include "Basics/debugging.h"
#include "Containers/HashSet.h"
#include "IResearch/IResearchCommon.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utilities/NameValidator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <date/date.h>
#include <velocypack/Iterator.h>
#include <velocypack/Utf8Helper.h>

#include <iostream>
#include <limits>

using namespace std::chrono;
using namespace date;

namespace {

bool hasExpansion(
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields) {
  for (auto const& it : fields) {
    if (TRI_AttributeNamesHaveExpansion(it)) {
      return true;
    }
  }
  return false;
}

bool canBeNull(
    arangodb::aql::AstNode const* op, arangodb::aql::AstNode const* access,
    arangodb::containers::FlatHashSet<std::string> const& nonNullAttributes) {
  TRI_ASSERT(op != nullptr);
  TRI_ASSERT(access != nullptr);

  if (access->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS &&
      access->getMemberUnchecked(0)->type ==
          arangodb::aql::NODE_TYPE_REFERENCE) {
    // a.b
    // now check if the accessed attribute is _key, _rev or _id.
    // all of these cannot be null
    auto attributeName = access->getStringView();
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

void markAsNonNull(
    arangodb::aql::AstNode const* op, arangodb::aql::AstNode const* access,
    arangodb::containers::FlatHashSet<std::string>& nonNullAttributes) {
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

std::string defaultIndexName(VPackSlice const& slice) {
  auto type = arangodb::Index::type(
      slice.get(arangodb::StaticStrings::IndexType).stringView());

  if (type == arangodb::Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return arangodb::StaticStrings::IndexNamePrimary;
  }
  if (type == arangodb::Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
    auto fields = slice.get(arangodb::StaticStrings::IndexFields);
    TRI_ASSERT(fields.isArray());
    auto firstField = fields.at(0);
    TRI_ASSERT(firstField.isString());
    bool isFromIndex =
        firstField.isEqualString(arangodb::StaticStrings::FromString);
    return isFromIndex ? arangodb::StaticStrings::IndexNameEdgeFrom
                       : arangodb::StaticStrings::IndexNameEdgeTo;
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

Index::FilterCosts Index::FilterCosts::defaultCosts(size_t itemsInIndex,
                                                    size_t numLookups) {
  Index::FilterCosts costs;
  costs.supportsCondition = false;
  costs.coveredAttributes = 0;
  costs.estimatedItems = itemsInIndex * numLookups;
  costs.estimatedCosts = static_cast<double>(costs.estimatedItems);
  return costs;
}

Index::SortCosts Index::SortCosts::zeroCosts(size_t coveredAttributes) {
  Index::SortCosts costs;
  costs.coveredAttributes = coveredAttributes;
  costs.supportsCondition = true;
  costs.estimatedCosts = 0;
  return costs;
}

Index::SortCosts Index::SortCosts::defaultCosts(size_t itemsInIndex) {
  Index::SortCosts costs;
  TRI_ASSERT(!costs.supportsCondition);
  costs.coveredAttributes = 0;
  costs.estimatedCosts =
      0.01 * itemsInIndex + /*for sort setup, 1 percent of documents covered by
                               index */
      1.05 * (itemsInIndex > 0 ? (static_cast<double>(itemsInIndex) *
                                  std::log2(static_cast<double>(itemsInIndex)))
                               : 0.0);
  return costs;
}

// empty field attributes list
/*static*/ std::vector<std::vector<arangodb::basics::AttributeName>> const
    Index::emptyCoveredFields{};

// If the Index is on a coordinator instance the index may not access the
// logical collection because it could be gone!
Index::Index(
    IndexId iid, arangodb::LogicalCollection& collection,
    std::string const& name,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse)
    : _iid(iid),
      _collection(collection),
      _name(name),
      _fields(fields),
      _progress(-1.),
      _useExpansion(::hasExpansion(_fields)),
      _unique(unique),
      _sparse(sparse) {
  // note: _collection can be a nullptr in the cluster coordinator case!!
}

Index::Index(IndexId iid, arangodb::LogicalCollection& collection,
             VPackSlice slice)
    : _iid(iid),
      _collection(collection),
      _name(arangodb::basics::VelocyPackHelper::getStringValue(
          slice, arangodb::StaticStrings::IndexName,
          ::defaultIndexName(slice))),
      _fields(parseFields(
          slice.get(arangodb::StaticStrings::IndexFields), /*allowEmpty*/ true,
          Index::allowExpansion(Index::type(
              slice.get(arangodb::StaticStrings::IndexType).stringView())))),
      _progress(-1.),
      _useExpansion(::hasExpansion(_fields)),
      _unique(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, arangodb::StaticStrings::IndexUnique, false)),
      _sparse(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, arangodb::StaticStrings::IndexSparse, false)) {}

Index::~Index() = default;

/// @brief return the index name
std::string const& Index::name() const {
  if (_name == StaticStrings::IndexNameEdgeFrom ||
      _name == StaticStrings::IndexNameEdgeTo) {
    return StaticStrings::IndexNameEdge;
  }
  return _name;
}

void Index::name(std::string const& newName) {
  if (_name.empty()) {
    _name = newName;
  }
}

/// @brief return the index fields names
std::vector<std::vector<std::string>> Index::fieldNames() const {
  std::vector<std::vector<std::string>> result;
  result.reserve(_fields.size());

  for (auto const& it : _fields) {
    std::vector<std::string> parts;
    parts.reserve(it.size());
    for (auto const& it2 : it) {
      parts.emplace_back(it2.name);
    }
    result.emplace_back(std::move(parts));
  }
  return result;
}

/// @brief whether or not the ith attribute is expanded (somewhere)
bool Index::isAttributeExpanded(size_t i) const {
  if (i >= _fields.size()) {
    return false;
  }
  return TRI_AttributeNamesHaveExpansion(_fields[i]);
}

/// @brief whether or not any attribute is expanded
bool Index::isAttributeExpanded(
    std::vector<basics::AttributeName> const& attribute) const {
  for (auto const& it : _fields) {
    if (!basics::AttributeName::namesMatch(attribute, it)) {
      continue;
    }
    return TRI_AttributeNamesHaveExpansion(it);
  }
  return false;
}

/// @brief whether or not any attribute is expanded
bool Index::attributeMatches(
    std::vector<basics::AttributeName> const& attribute, bool isPrimary) const {
  for (auto const& it : _fields) {
    if (basics::AttributeName::isIdentical(attribute, it, true)) {
      return true;
    }
  }
  if (isPrimary) {
    static std::vector<basics::AttributeName> const vec_id{
        {StaticStrings::IdString, false}};
    return basics::AttributeName::isIdentical(attribute, vec_id, true);
  }
  return false;
}

/// @brief whether or not the given attribute vector matches any of the index
/// fields In case it does, the position will be returned as well.
std::pair<bool, size_t> Index::attributeMatchesWithPos(
    std::vector<basics::AttributeName> const& attribute, bool isPrimary) const {
  size_t pos = 0;
  for (auto const& it : _fields) {
    if (basics::AttributeName::isIdentical(attribute, it, true)) {
      return {true, pos};
    }
    pos++;
  }
  if (isPrimary) {
    static std::vector<basics::AttributeName> const vec_id{
        {StaticStrings::IdString, false}};
    return basics::AttributeName::isIdentical(attribute, vec_id, true)
               ? std::pair<bool, size_t>(true, pos)
               : std::pair<bool, size_t>(false, -1);
  }
  return {false, -1};
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

/// @brief validate fields that start or end with ":"
void Index::validateFieldsWithSpecialCase(VPackSlice fields) {
  if (!fields.isArray()) {
    return;
  }

  for (VPackSlice name : VPackArrayIterator(fields)) {
    if (name.isObject()) {
      name = name.get("name");
    }
    if (name.stringView().starts_with(":") ||
        name.stringView().ends_with(":")) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                     "field names starting or ending with "
                                     "\":\" are disallowed in indexes");
    }
  }
}

/// @brief validate fields from slice
void Index::validateFields(VPackSlice slice) {
  std::string_view type =
      slice.get(arangodb::StaticStrings::IndexType).stringView();
  auto allowExpansion = Index::allowExpansion(Index::type(type));
  auto const fieldIsObject = TRI_IDX_TYPE_INVERTED_INDEX == Index::type(type);

  auto fields = slice.get(arangodb::StaticStrings::IndexFields);
  if (!fields.isArray()) {
    return;
  }

  for (VPackSlice name : VPackArrayIterator(fields)) {
    if (fieldIsObject) {
      if (!name.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
            "inverted index: field is not an object");
      }
      name = name.get("name");
    }
    if (!name.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                     "invalid index description");
    }
    std::vector<arangodb::basics::AttributeName> parsedAttributes;
    TRI_ParseAttributeString(name.copyString(), parsedAttributes,
                             allowExpansion);
  }
}

/// @brief return the index type based on a type name
Index::IndexType Index::type(std::string_view type) {
  if (type == "primary") {
    return TRI_IDX_TYPE_PRIMARY_INDEX;
  }
  if (type == "edge") {
    return TRI_IDX_TYPE_EDGE_INDEX;
  }
  if (type == "hash") {
    return TRI_IDX_TYPE_HASH_INDEX;
  }
  if (type == "skiplist") {
    return TRI_IDX_TYPE_SKIPLIST_INDEX;
  }
  if (type == "ttl") {
    return TRI_IDX_TYPE_TTL_INDEX;
  }
  if (type == "persistent" || type == "rocksdb") {
    return TRI_IDX_TYPE_PERSISTENT_INDEX;
  }
  if (type == "fulltext") {
    return TRI_IDX_TYPE_FULLTEXT_INDEX;
  }
  if (type == "geo") {
    return TRI_IDX_TYPE_GEO_INDEX;
  }
  if (type == "geo1") {
    return TRI_IDX_TYPE_GEO1_INDEX;
  }
  if (type == "geo2") {
    return TRI_IDX_TYPE_GEO2_INDEX;
  }
  if (type == "mdi") {
    return TRI_IDX_TYPE_MDI_INDEX;
  }
  if (type == "zkd") {
    return TRI_IDX_TYPE_ZKD_INDEX;
  }
  if (type == "mdi-prefixed") {
    return TRI_IDX_TYPE_MDI_PREFIXED_INDEX;
  }
  if (type == iresearch::StaticStrings::ViewArangoSearchType) {
    return TRI_IDX_TYPE_IRESEARCH_LINK;
  }
  if (type == "noaccess") {
    return TRI_IDX_TYPE_NO_ACCESS_INDEX;
  }
  if (type == arangodb::iresearch::IRESEARCH_INVERTED_INDEX_TYPE) {
    return TRI_IDX_TYPE_INVERTED_INDEX;
  }
  return TRI_IDX_TYPE_UNKNOWN;
}

bool Index::onlyHintForced(IndexType type) {
  // inverted index is eventually consistent, so usage must be explicilty
  // permitted by the user
  return type == TRI_IDX_TYPE_INVERTED_INDEX;
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
      return iresearch::StaticStrings::ViewArangoSearchType.data();
    case TRI_IDX_TYPE_NO_ACCESS_INDEX:
      return "noaccess";
    case TRI_IDX_TYPE_ZKD_INDEX:
      return "zkd";
    case TRI_IDX_TYPE_MDI_INDEX:
      return "mdi";
    case TRI_IDX_TYPE_MDI_PREFIXED_INDEX:
      return "mdi-prefixed";
    case TRI_IDX_TYPE_INVERTED_INDEX:
      return arangodb::iresearch::IRESEARCH_INVERTED_INDEX_TYPE.data();
    case TRI_IDX_TYPE_UNKNOWN: {
    }
  }

  return "";
}

/// @brief validate an index id (i.e. ^[0-9]+$)
bool Index::validateId(std::string_view id) {
  // totally empty id string is not allowed
  return !id.empty() && std::all_of(id.begin(), id.end(), [](char c) {
    return c >= '0' && c <= '9';
  });
}

/// @brief validate an index handle (collection name + / + index id)
bool Index::validateHandle(bool extendedNames,
                           std::string_view handle) noexcept {
  std::size_t pos = handle.find('/');
  if (pos == std::string::npos) {
    // no prefix
    return false;
  }
  // check collection name part
  if (!CollectionNameValidator::validateName(
           /*allowSystem*/ true, extendedNames, handle.substr(0, pos))
           .ok()) {
    return false;
  }
  // check remainder (index id)
  handle = handle.substr(pos + 1);
  return validateId(std::string_view(handle.data(), handle.size()));
}

/// @brief validate an index handle (collection name + / + index name)
bool Index::validateHandleName(bool extendedNames,
                               std::string_view name) noexcept {
  std::size_t pos = name.find('/');
  if (pos == std::string::npos) {
    // no prefix
    return false;
  }
  // check collection name part
  if (!CollectionNameValidator::validateName(
           /*allowSystem*/ true, extendedNames, name.substr(0, pos))
           .ok()) {
    return false;
  }
  // check remainder (index name)
  return IndexNameValidator::validateName(extendedNames, name.substr(pos + 1))
      .ok();
}

/// @brief generate a new index id
IndexId Index::generateId() { return IndexId{TRI_NewTickServer()}; }

/// @brief check if two index definitions share any identifiers (_id, name)
bool Index::compareIdentifiers(velocypack::Slice const& lhs,
                               velocypack::Slice const& rhs) {
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
bool Index::compare(StorageEngine& engine, VPackSlice const& lhs,
                    VPackSlice const& rhs, std::string const& dbname) {
  auto normalizeType = [](VPackSlice s) -> std::string_view {
    TRI_ASSERT(s.isString());
    // "zkd" is the old naming for "mdi", so we have to treat these
    // two type names as identical.
    if (s.stringView() == "zkd") {
      return "mdi";
    }
    return s.stringView();
  };

  auto lhsType = normalizeType(lhs.get(arangodb::StaticStrings::IndexType));
  auto rhsType = normalizeType(rhs.get(arangodb::StaticStrings::IndexType));

  if (lhsType != rhsType) {
    return false;
  }

  return engine.indexFactory()
      .factory(std::string{lhsType})
      .equal(lhs, rhs, dbname);
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
std::shared_ptr<VPackBuilder> Index::toVelocyPack(
    std::underlying_type<Index::Serialize>::type flags) const {
  auto builder = std::make_shared<VPackBuilder>();
  toVelocyPack(*builder, flags);
  return builder;
}

/// @brief create a VelocyPack representation of the index
/// base functionality (called from derived classes)
/// note: needs an already-opened object as its input!
void Index::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(arangodb::StaticStrings::IndexId,
              arangodb::velocypack::Value(std::to_string(_iid.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              arangodb::velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName,
              arangodb::velocypack::Value(name()));

  builder.add(
      arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields));
  builder.openArray();

  for (auto const& field : fields()) {
    std::string fieldString;
    TRI_AttributeNamesToString(field, fieldString);
    builder.add(VPackValue(fieldString));
  }

  builder.close();

  if (hasSelectivityEstimate() &&
      Index::hasFlag(flags, Index::Serialize::Estimates)) {
    builder.add("selectivityEstimate", VPackValue(selectivityEstimate()));
  }

  auto const progress = _progress.load(std::memory_order_relaxed);
  if (progress > -1 && progress < 100) {
    builder.add("progress", VPackValue(progress));
    builder.add(StaticStrings::IndexIsBuilding, VPackValue(true));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }
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
  TRI_ASSERT(typeSlice.stringView() == oldtypeName());
#endif
  auto value = info.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    return value.stringView() == std::to_string(_iid.id());
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
    TRI_ParseAttributeString(f.stringView(), translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                      false)) {
      return false;
    }
  }
  return true;
}

/// @brief default implementation for selectivityEstimate
double Index::selectivityEstimate(std::string_view) const {
  if (_unique) {
    return 1.0;
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void Index::updateClusterSelectivityEstimate(double /*estimate*/) {
  TRI_ASSERT(false);  // should never be called except on Coordinator
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
    if (it.size() == 1 && it[0] == arangodb::basics::AttributeName(
                                       StaticStrings::KeyString, false)) {
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
Index::FilterCosts Index::supportsFilterCondition(
    transaction::Methods& /*trx*/, std::vector<std::shared_ptr<Index>> const&,
    aql::AstNode const* /* node */, aql::Variable const* /* reference */,
    size_t itemsInIndex) const {
  // by default no filter conditions are supported
  return Index::FilterCosts::defaultCosts(itemsInIndex);
}

/// @brief default implementation for supportsSortCondition
Index::SortCosts Index::supportsSortCondition(
    aql::SortCondition const* /* sortCondition */,
    aql::Variable const* /* node */, size_t itemsInIndex) const {
  // by default no sort conditions are supported
  return Index::SortCosts::defaultCosts(itemsInIndex);
}

aql::AstNode* Index::specializeCondition(
    transaction::Methods& /*trx*/, aql::AstNode* /* node */,
    aql::Variable const* /* reference */) const {
  // the default implementation should never be called
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string(
          "no default implementation for specializeCondition. index type: ") +
          typeName());
}

std::unique_ptr<IndexIterator> Index::iteratorForCondition(
    ResourceMonitor& /*monitor*/, transaction::Methods* /* trx */,
    aql::AstNode const* /* node */, aql::Variable const* /* reference */,
    IndexIteratorOptions const& /* opts */, ReadOwnWrites /* readOwnWrites */,
    int /*mutableConditionIdx*/) {
  // the default implementation should never be called
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string(
          "no default implementation for iteratorForCondition. index type: ") +
          typeName());
}

/// @brief perform some base checks for an index condition part
bool Index::canUseConditionPart(
    aql::AstNode const* access, aql::AstNode const* other,
    aql::AstNode const* op, aql::Variable const* reference,
    containers::FlatHashSet<std::string>& nonNullAttributes,
    bool isExecution) const {
  if (_sparse) {
    if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_IN &&
        (other->type == aql::NODE_TYPE_EXPANSION ||
         other->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS)) {
      // value IN a.b  OR  value IN a.b[*]
      if (!access->isConstant()) {
        return false;
      }

      /* A sparse index will store null in Array
      if (access->isNullValue()) {
        return false;
      }
      */
    } else if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_IN &&
               access->type == aql::NODE_TYPE_EXPANSION) {
      // value[*] IN a.b
      if (!other->isConstant()) {
        return false;
      }

      /* A sparse index will store null in Array
      if (other->isNullValue()) {
        return false;
      }
      */
    } else if (access->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      // a.b == value  OR  a.b IN values

      if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_GT) {
        // > anything also excludes "null". now note that this attribute cannot
        // become null range definitely exludes the "null" value
        ::markAsNonNull(op, access, nonNullAttributes);
      } else if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_LT ||
                 op->type == aql::NODE_TYPE_OPERATOR_BINARY_LE) {
        // <  and  <= are not supported with sparse indexes as this may include
        // null values
        if (::canBeNull(op, access, nonNullAttributes)) {
          return false;
        }

        // range definitely exludes the "null" value
        ::markAsNonNull(op, access, nonNullAttributes);
      }

      if (other->isConstant()) {
        if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_NE &&
            other->isNullValue()) {
          // != null. now note that a certain attribute cannot become null
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        } else if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_GE &&
                   !other->isNullValue()) {
          // >= non-null. now note that a certain attribute cannot become null
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        }

        if (other->isNullValue() &&
            (op->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
             op->type == aql::NODE_TYPE_OPERATOR_BINARY_GE)) {
          // ==  and  >= null are not supported with sparse indexes for the same
          // reason
          if (::canBeNull(op, access, nonNullAttributes)) {
            return false;
          }
          ::markAsNonNull(op, access, nonNullAttributes);
          return true;
        }

        if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_IN &&
            other->type == aql::NODE_TYPE_ARRAY) {
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

  if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_NE) {
    // none of the indexes can use !=, so we can exit here
    // note that this function may have been called for operator !=. this is
    // necessary to track the non-null attributes, e.g. attr != null, so we can
    // note which attributes cannot be null and still use sparse indexes for
    // these attributes
    return false;
  }

  // test if the reference variable is contained on both sides of the expression
  aql::VarSet variables;
  if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_IN &&
      (other->type == aql::NODE_TYPE_EXPANSION ||
       other->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS)) {
    // value IN a.b  OR  value IN a.b[*]
    aql::Ast::getReferencedVariables(access, variables);
    if (variables.find(reference) != variables.end()) {
      variables.clear();
      aql::Ast::getReferencedVariables(other, variables);
    }
  } else {
    // a.b == value  OR  a.b IN values
    if (!other->isConstant()) {
      // don't look for referenced variables if we only access a
      // constant value (there will be no variables then...)
      aql::Ast::getReferencedVariables(other, variables);
    }
  }

  if (variables.find(reference) != variables.end()) {
    // yes. then we cannot use an index here
    return false;
  }

  return true;
}

/// @brief whether or not the index covers all the attributes passed in.
/// the function may modify the projections by setting the coveringIndexPosition
/// value in it.
bool Index::covers(aql::Projections& projections) const {
  size_t const n = projections.size();

  if (n == 0) {
    return false;
  }

  // check if we can use covering indexes
  auto const& covered = coveredFields();

  for (size_t i = 0; i < n; ++i) {
    bool found = false;
    for (size_t j = 0; j < covered.size(); ++j) {
      auto const& field = covered[j];
      size_t k = 0;
      for (auto const& part : field) {
        if (part.shouldExpand) {
          k = std::numeric_limits<size_t>::max();
          break;
        }
        if (k >= projections[i].path.size() ||
            part.name != projections[i].path[k]) {
          break;
        }
        ++k;
      }

      // if the index can only satisfy a prefix of the projection, that is still
      // better than nothing, e.g. an index on  a.b  can be used to satisfy a
      // projection on  a.b.c
      if (k >= field.size() && k != std::numeric_limits<size_t>::max()) {
        TRI_ASSERT(k > 0);
        TRI_ASSERT(k < aql::Projections::kNoCoveringIndexPosition);
        projections[i].coveringIndexPosition = static_cast<uint16_t>(j);
        projections[i].coveringIndexCutoff = static_cast<uint16_t>(k);
        found = true;
        break;
      }
    }
    if (!found) {
      // stop on the first attribute that we cannot support
      return false;
    }
  }

  return true;
}

bool Index::canWarmup() const noexcept { return false; }

Result Index::warmup() {
  // we should never be called in the base class.
  TRI_ASSERT(!canWarmup());
  TRI_ASSERT(false);
  // Do nothing. If an index needs some warmup
  // it has to explicitly implement it.
  return {};
}

/// @brief generate error result
/// @param code the error key
/// @param key the conflicting key
Result& Index::addErrorMsg(Result& r, ErrorCode code,
                           std::string_view key) const {
  if (code != TRI_ERROR_NO_ERROR) {
    r.reset(code);
    return addErrorMsg(r, key);
  }
  return r;
}

/// @brief generate error message
/// @param key the conflicting key
Result& Index::addErrorMsg(Result& r, std::string_view key) const {
  return r.withError(
      [this, &key](result::Error& err) { addErrorMsg(err, key); });
}

void Index::addErrorMsg(result::Error& r, std::string_view key) const {
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
}

double Index::getTimestamp(velocypack::Slice const& doc,
                           std::string const& attributeName) const {
  VPackSlice value = doc.get(attributeName);

  if (value.isString()) {
    // string value. we expect it to be YYYY-MM-DD etc.
    tp_sys_clock_ms tp;
    if (basics::parseDateTime(value.stringView(), tp)) {
      return static_cast<double>(
          std::chrono::duration_cast<std::chrono::seconds>(
              tp.time_since_epoch())
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

AttributeAccessParts::AttributeAccessParts(aql::AstNode const* comparison,
                                           aql::Variable const* variable)
    : comparison(comparison),
      attribute(nullptr),
      value(nullptr),
      opType(aql::NODE_TYPE_NOP) {
  // first assume a.b == value
  attribute = comparison->getMember(0);
  value = comparison->getMember(1);
  opType = comparison->type;

  if (attribute->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // got value == a.b  ->  flip the two sides
    attribute = comparison->getMember(1);
    value = comparison->getMember(0);
    opType = aql::Ast::reverseOperator(opType);
  }

  TRI_ASSERT(attribute->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  TRI_ASSERT(attribute->isAttributeAccessForVariable(variable, true));
}

void Index::normalizeFilterCosts(Index::FilterCosts& costs, Index const* idx,
                                 size_t itemsInIndex, size_t invocations) {
  // number of fields to consider for this index. this is normally
  // equivalent to the number of fields of the index, but also adds the
  // number of stored values if there exist any.
  size_t numFieldsToConsider = idx->numFieldsToConsiderInIndexSelection();
  TRI_ASSERT(numFieldsToConsider > 0);

  // costs.estimatedItems is always set here, make it at least 1
  costs.estimatedItems = std::max(size_t(1), costs.estimatedItems);

  // seek cost is O(log(n)) for RocksDB
  costs.estimatedCosts =
      std::max(double(1.0), std::log2(double(itemsInIndex)) * invocations);
  // add per-document processing cost
  costs.estimatedCosts += costs.estimatedItems * 0.05;
  // slightly prefer indexes that cover more attributes
  costs.estimatedCosts -= (numFieldsToConsider - 1) * 0.02;

  // cost is already low... now slightly prioritize unique indexes
  if (idx->unique() || idx->implicitlyUnique()) {
    costs.estimatedCosts *= 0.995 - 0.05 * (numFieldsToConsider - 1);
  }

  if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
      idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
    // primary and edge index have faster lookups due to very fast
    // comparators
    costs.estimatedCosts *= 0.9;
  }

  // box the estimated costs to [0 - inf)
  costs.estimatedCosts = std::max(double(0.0), costs.estimatedCosts);
}

/// @brief set fields from slice
std::vector<std::vector<basics::AttributeName>> Index::parseFields(
    VPackSlice fields, bool allowEmpty, bool allowExpansion) {
  std::vector<std::vector<basics::AttributeName>> result;

  if (fields.isArray()) {
    size_t const n = static_cast<size_t>(fields.length());
    result.reserve(n);

    for (VPackSlice name : VPackArrayIterator(fields)) {
      if (!name.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                       "invalid index description");
      }

      std::vector<basics::AttributeName> parsedAttributes;
      TRI_ParseAttributeString(name.copyString(), parsedAttributes,
                               allowExpansion);
      result.emplace_back(std::move(parsedAttributes));
    }
  }

  if (result.empty() && !allowEmpty) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                   "invalid index description");
  }
  return result;
}

std::vector<std::vector<basics::AttributeName>> Index::mergeFields(
    std::vector<std::vector<basics::AttributeName>> const& fields1,
    std::vector<std::vector<basics::AttributeName>> const& fields2) {
  std::vector<std::vector<basics::AttributeName>> result;
  result.reserve(fields1.size() + fields2.size());

  result.insert(result.end(), fields1.begin(), fields1.end());
  result.insert(result.end(), fields2.begin(), fields2.end());
  return result;
}

std::unique_ptr<AqlIndexStreamIterator> Index::streamForCondition(
    transaction::Methods* trx, IndexStreamOptions const&) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string(
          "no default implementation for streamForCondition. index type: ") +
          typeName());
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
