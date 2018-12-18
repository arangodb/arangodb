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

#include "Index.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"

#ifdef USE_IRESEARCH
  #include "IResearch/IResearchCommon.h"
#endif

#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <iostream>
#include <ostream>

using namespace arangodb;

namespace {

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

  for (auto const& name : VPackArrayIterator(fields)) {
    if (!name.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                      "invalid index description");
    }

    std::vector<arangodb::basics::AttributeName> parsedAttributes;
    TRI_ParseAttributeString(name.copyString(), parsedAttributes,
                              allowExpansion);
    result.emplace_back(std::move(parsedAttributes));
  }
  return result;
}

bool canBeNull(arangodb::aql::AstNode const* op, arangodb::aql::AstNode const* access,
               std::unordered_set<std::string> const& nonNullAttributes) {
  TRI_ASSERT(op != nullptr);
  TRI_ASSERT(access != nullptr);

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

  // for everything else we are unusure
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

} // namespace

// If the Index is on a coordinator instance the index may not access the
// logical collection because it could be gone!

Index::Index(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique,
    bool sparse
)
    : _iid(iid),
      _collection(collection),
      _fields(fields),
      _useExpansion(::hasExpansion(_fields)),
      _unique(unique),
      _sparse(sparse) {
  // note: _collection can be a nullptr in the cluster coordinator case!!
}

Index::Index(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    VPackSlice const& slice
)
    : _iid(iid),
      _collection(collection),
      _fields(::parseFields(slice.get(arangodb::StaticStrings::IndexFields),
                            Index::allowExpansion(Index::type(slice.get(arangodb::StaticStrings::IndexType).copyString())))),
      _useExpansion(::hasExpansion(_fields)),
      _unique(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, arangodb::StaticStrings::IndexUnique, false
      )),
      _sparse(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, arangodb::StaticStrings::IndexSparse, false
      )) {
}

Index::~Index() {}

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
  auto allowExpansion = Index::allowExpansion(
    Index::type(slice.get(arangodb::StaticStrings::IndexType).copyString())
  );

  auto fields = slice.get(arangodb::StaticStrings::IndexFields);

  if (!fields.isArray()) {
    return;
  }

  for (auto const& name : VPackArrayIterator(fields)) {
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
Index::IndexType Index::type(char const* type) {
  if (::strcmp(type, "primary") == 0) {
    return TRI_IDX_TYPE_PRIMARY_INDEX;
  }
  if (::strcmp(type, "edge") == 0) {
    return TRI_IDX_TYPE_EDGE_INDEX;
  }
  if (::strcmp(type, "hash") == 0) {
    return TRI_IDX_TYPE_HASH_INDEX;
  }
  if (::strcmp(type, "skiplist") == 0) {
    return TRI_IDX_TYPE_SKIPLIST_INDEX;
  }
  if (::strcmp(type, "persistent") == 0 || ::strcmp(type, "rocksdb") == 0) {
    return TRI_IDX_TYPE_PERSISTENT_INDEX;
  }
  if (::strcmp(type, "fulltext") == 0) {
    return TRI_IDX_TYPE_FULLTEXT_INDEX;
  }
  if (::strcmp(type, "geo1") == 0) {
    return TRI_IDX_TYPE_GEO1_INDEX;
  }
  if (::strcmp(type, "geo2") == 0) {
    return TRI_IDX_TYPE_GEO2_INDEX;
  }
  if (::strcmp(type, "geo") == 0) {
    return TRI_IDX_TYPE_GEO_INDEX;
  }
#ifdef USE_IRESEARCH
  if (arangodb::iresearch::DATA_SOURCE_TYPE.name() == type) {
    return TRI_IDX_TYPE_IRESEARCH_LINK;
  }
#endif
  if (::strcmp(type, "noaccess") == 0) {
    return TRI_IDX_TYPE_NO_ACCESS_INDEX;
  }

  return TRI_IDX_TYPE_UNKNOWN;
}

Index::IndexType Index::type(std::string const& type) {
  return Index::type(type.c_str());
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
#ifdef USE_IRESEARCH
    case TRI_IDX_TYPE_IRESEARCH_LINK:
      return arangodb::iresearch::DATA_SOURCE_TYPE.name().c_str();
#endif
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

/// @brief validate an index handle (collection name + / + index id)
bool Index::validateHandle(char const* key, size_t* split) {
  char const* p = key;
  char c = *p;

  // extract collection name

  if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
    return false;
  }

  ++p;

  while (1) {
    c = *p;

    if ((c == '_') || (c == '-') || (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      ++p;
      continue;
    }

    if (c == '/') {
      break;
    }

    return false;
  }

  if (static_cast<size_t>(p - key) > TRI_COL_NAME_LENGTH) {
    return false;
  }

  // store split position
  *split = p - key;
  ++p;

  // validate index id
  return validateId(p);
}

/// @brief generate a new index id
TRI_idx_iid_t Index::generateId() { return TRI_NewTickServer(); }

/// @brief index comparator, used by the coordinator to detect if two index
/// contents are the same
bool Index::Compare(VPackSlice const& lhs, VPackSlice const& rhs) {
  auto lhsType = lhs.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(lhsType.isString());

  // type must be identical
  if (arangodb::basics::VelocyPackHelper::compare(
        lhsType, rhs.get(arangodb::StaticStrings::IndexType), false
      ) != 0) {
    return false;
  }

  auto* engine = EngineSelectorFeature::ENGINE;

  return engine
    && engine->indexFactory().factory(lhsType.copyString()).equal(lhs, rhs);
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
  builder.add(
    arangodb::StaticStrings::IndexId,
    arangodb::velocypack::Value(std::to_string(_iid))
  );
  builder.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(oldtypeName(type()))
  );

  builder.add(
    arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields)
  );
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
  builder->openObject();
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
  StringRef typeStr(typeSlice);
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
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
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
                   info, arangodb::StaticStrings::IndexUnique, false
                 )
     ) {
    return false;
  }

  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexSparse, false
                 )
      ) {
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
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                      false)) {
      return false;
    }
  }
  return true;
}

/// @brief default implementation for selectivityEstimate
double Index::selectivityEstimate(StringRef const&) const {
  if (_unique) {
    return 1.0;
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief default implementation for implicitlyUnique
bool Index::implicitlyUnique() const {
  // simply return whether the index actually is unique
  // in this base class, we cannot do anything else
  return _unique;
}

void Index::batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  for (auto const& it : documents) {
    Result status = insert(trx, it.first, it.second, OperationMode::normal);
    if (status.errorNumber() != TRI_ERROR_NO_ERROR) {
      queue->setStatus(status.errorNumber());
      break;
    }
  }
}

/// @brief default implementation for drop
Result Index::drop() {
  return Result(); // do nothing
}

/// @brief default implementation for sizeHint
Result Index::sizeHint(transaction::Methods& trx, size_t size) {
  return Result(); // do nothing
}

/// @brief default implementation for hasBatchInsert
bool Index::hasBatchInsert() const { return false; }

/// @brief default implementation for supportsFilterCondition
bool Index::supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const&,
                                    arangodb::aql::AstNode const*,
                                    arangodb::aql::Variable const*,
                                    size_t itemsInIndex, size_t& estimatedItems,
                                    double& estimatedCost) const {
  // by default, no filter conditions are supported
  estimatedItems = itemsInIndex;
  estimatedCost = static_cast<double>(estimatedItems);
  return false;
}

/// @brief default implementation for supportsSortCondition
bool Index::supportsSortCondition(arangodb::aql::SortCondition const*,
                                  arangodb::aql::Variable const*,
                                  size_t itemsInIndex, double& estimatedCost,
                                  size_t& coveredAttributes) const {
  // by default, no sort conditions are supported
  coveredAttributes = 0;
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(itemsInIndex);
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* Index::specializeCondition(
    arangodb::aql::AstNode* node, arangodb::aql::Variable const*) const {
  return node;
}

/// @brief perform some base checks for an index condition part
bool Index::canUseConditionPart(
    arangodb::aql::AstNode const* access, arangodb::aql::AstNode const* other,
    arangodb::aql::AstNode const* op, arangodb::aql::Variable const* reference,
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
        // > anything also excludes "null". now note that this attribute cannot become null
        // range definitely exludes the "null" value
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
        if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE &&
            other->isNullValue()) {
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
  std::unordered_set<aql::Variable const*> variables;
  if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN &&
      (other->type == arangodb::aql::NODE_TYPE_EXPANSION ||
       other->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS)) {
    // value IN a.b  OR  value IN a.b[*]
    arangodb::aql::Ast::getReferencedVariables(access, variables);
    if (other->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS &&
        variables.find(reference) != variables.end()) {
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
void Index::expandInSearchValues(VPackSlice const base,
                                 VPackBuilder& result) const {
  TRI_ASSERT(base.isArray());

  VPackArrayBuilder baseGuard(&result);
  for (auto const& oneLookup : VPackArrayIterator(base)) {
    TRI_ASSERT(oneLookup.isArray());

    bool usesIn = false;
    for (auto const& it : VPackArrayIterator(oneLookup)) {
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

        std::unordered_set<VPackSlice,
                           arangodb::basics::VelocyPackHelper::VPackHash,
                           arangodb::basics::VelocyPackHelper::VPackEqual>
            tmp(static_cast<size_t>(nList),
                arangodb::basics::VelocyPackHelper::VPackHash(),
                arangodb::basics::VelocyPackHelper::VPackEqual());

        for (auto const& el : VPackArrayIterator(inList)) {
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
        if (list != elements.end() &&
            ++positions[level] < list->second.size()) {
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
  size_t i = 0;
  for (size_t j = 0; j < _fields.size(); ++j) {
    bool found = false;
    result.clear();
    TRI_AttributeNamesToString(_fields[j], result, false);
    for (auto const& it : attributes) {
      if (result == it) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
    ++i;
  }
  return true;
}

void Index::warmup(arangodb::transaction::Methods*,
                   std::shared_ptr<basics::LocalTaskQueue>) {
  // Do nothing. If an index needs some warmup
  // it has to explicitly implement it.
}

/// @brief generate error message
/// @param key the conflicting key
Result& Index::addErrorMsg(Result& r, std::string const& key) {
  // now provide more context based on index
  r.appendErrorMessage(" - in index ");
  r.appendErrorMessage(std::to_string(_iid));
  r.appendErrorMessage(" of type ");
  r.appendErrorMessage(typeName());
  
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------