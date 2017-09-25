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
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <iostream>
#include <ostream>

using namespace arangodb;

// If the Index is on a coordinator instance the index may not access the
// logical collection because it could be gone!

Index::Index(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse)
    : _iid(iid),
      _collection(collection),
      _fields(fields),
      _unique(unique),
      _sparse(sparse),
      _clusterSelectivity(0.1) {
  // note: _collection can be a nullptr in the cluster coordinator case!!
}

Index::Index(TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
             VPackSlice const& slice)
    : _iid(iid),
      _collection(collection),
      _fields(),
      _unique(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "unique", false)),
      _sparse(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "sparse", false)),
      _clusterSelectivity(0.1) {
  VPackSlice const fields = slice.get("fields");
  setFields(fields,
            Index::allowExpansion(Index::type(slice.get("type").copyString())));
}

/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
Index::Index(VPackSlice const& slice)
    : _iid(arangodb::basics::StringUtils::uint64(
          arangodb::basics::VelocyPackHelper::checkAndGetStringValue(slice,
                                                                     "id"))),
      _collection(nullptr),
      _fields(),
      _unique(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "unique", false)),
      _sparse(arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "sparse", false)) {
  VPackSlice const fields = slice.get("fields");
  setFields(fields,
            Index::allowExpansion(Index::type(slice.get("type").copyString())));
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
    default:
      return 42; /* OPST_CIRCUS */
  }
}

/// @brief set fields from slice
void Index::setFields(VPackSlice const& fields, bool allowExpansion) {
  if (!fields.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                   "invalid index description");
  }

  size_t const n = static_cast<size_t>(fields.length());
  _fields.reserve(n);

  for (auto const& name : VPackArrayIterator(fields)) {
    if (!name.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                     "invalid index description");
    }

    std::vector<arangodb::basics::AttributeName> parsedAttributes;
    TRI_ParseAttributeString(name.copyString(), parsedAttributes,
                             allowExpansion);
    _fields.emplace_back(std::move(parsedAttributes));
  }
}

/// @brief validate fields from slice
void Index::validateFields(VPackSlice const& slice) {
  bool const allowExpansion =
      Index::allowExpansion(Index::type(slice.get("type").copyString()));

  VPackSlice fields = slice.get("fields");

  if (!fields.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                   "invalid index description");
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
  VPackSlice lhsType = lhs.get("type");
  TRI_ASSERT(lhsType.isString());

  // type must be identical
  if (arangodb::basics::VelocyPackHelper::compare(lhsType, rhs.get("type"),
                                                  false) != 0) {
    return false;
  }

  auto type = Index::type(lhsType.copyString());

  // unique must be identical if present
  VPackSlice value = lhs.get("unique");
  if (value.isBoolean()) {
    if (arangodb::basics::VelocyPackHelper::compare(value, rhs.get("unique"),
                                                    false) != 0) {
      return false;
    }
  }

  // sparse must be identical if present
  value = lhs.get("sparse");
  if (value.isBoolean()) {
    if (arangodb::basics::VelocyPackHelper::compare(value, rhs.get("sparse"),
                                                    false) != 0) {
      return false;
    }
  }

  if (type == IndexType::TRI_IDX_TYPE_GEO1_INDEX) {
    // geoJson must be identical if present
    value = lhs.get("geoJson");
    if (value.isBoolean()) {
      if (arangodb::basics::VelocyPackHelper::compare(value, rhs.get("geoJson"),
                                                      false) != 0) {
        return false;
      }
    }
  } else if (type == IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    // minLength
    value = lhs.get("minLength");
    if (value.isNumber()) {
      if (arangodb::basics::VelocyPackHelper::compare(
              value, rhs.get("minLength"), false) != 0) {
        return false;
      }
    }
  }

  // other index types: fields must be identical if present
  value = lhs.get("fields");

  if (value.isArray()) {
    if (type == IndexType::TRI_IDX_TYPE_HASH_INDEX) {
      VPackValueLength const nv = value.length();

      // compare fields in arbitrary order
      VPackSlice const r = rhs.get("fields");

      if (!r.isArray() || nv != r.length()) {
        return false;
      }

      for (size_t i = 0; i < nv; ++i) {
        VPackSlice const v = value.at(i);

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
      if (arangodb::basics::VelocyPackHelper::compare(value, rhs.get("fields"),
                                                      false) != 0) {
        return false;
      }
    }
  }

  return true;
}

/// @brief return a contextual string for logging
std::string Index::context() const {
  std::ostringstream result;

  result << "index { id: " << id() << ", type: " << oldtypeName()
         << ", collection: " << _collection->dbName() << "/"
         << _collection->name() << ", unique: " << (_unique ? "true" : "false")
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
std::shared_ptr<VPackBuilder> Index::toVelocyPack(bool withFigures, bool forPersistence) const {
  auto builder = std::make_shared<VPackBuilder>();
  toVelocyPack(*builder, withFigures, forPersistence);
  return builder;
}

/// @brief create a VelocyPack representation of the index
/// base functionality (called from derived classes)
/// note: needs an already-opened object as its input!
void Index::toVelocyPack(VPackBuilder& builder, bool withFigures, bool) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("id", VPackValue(std::to_string(_iid)));
  builder.add("type", VPackValue(oldtypeName()));

  builder.add(VPackValue("fields"));
  builder.openArray();
  for (auto const& field : fields()) {
    std::string fieldString;
    TRI_AttributeNamesToString(field, fieldString);
    builder.add(VPackValue(fieldString));
  }
  builder.close();

  if (hasSelectivityEstimate() && !ServerState::instance()->isCoordinator()) {
    builder.add("selectivityEstimate", VPackValue(selectivityEstimate()));
  }

  if (withFigures) {
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
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get("id");
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
  value = info.get("fields");
  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", false)) {
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
double Index::selectivityEstimate(StringRef const* extra) const {
  if (_unique) {
    return 1.0;
  }

  double estimate = 0.1; //default
  if(!ServerState::instance()->isCoordinator()){
    estimate = selectivityEstimateLocal(extra);
  } else {
    // getClusterEstimate can not be called from within the index
    // as _collection is not always vaild

    //estimate = getClusterEstimate(estimate /*as default*/).second;
    estimate=_clusterSelectivity;
  }

  TRI_ASSERT(estimate >= 0.0 &&
             estimate <= 1.00001);  // floating-point tolerance
  return estimate;
}

double Index::selectivityEstimateLocal(StringRef const* extra) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief default implementation for implicitlyUnique
bool Index::implicitlyUnique() const {
  // simply return whether the index actually is unique
  // in this base class, we cannot do anything else
  return _unique;
}

void Index::batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&
        documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  for (auto const& it : documents) {
    Result status = insert(trx, it.first, it.second, false);
    if (status.errorNumber() != TRI_ERROR_NO_ERROR) {
      queue->setStatus(status.errorNumber());
      break;
    }
  }
}

/// @brief default implementation for drop
int Index::drop() {
  // do nothing
  return TRI_ERROR_NO_ERROR;
}

/// @brief default implementation for sizeHint
int Index::sizeHint(transaction::Methods*, size_t) {
  // do nothing
  return TRI_ERROR_NO_ERROR;
}

/// @brief default implementation for hasBatchInsert
bool Index::hasBatchInsert() const { return false; }

/// @brief default implementation for supportsFilterCondition
bool Index::supportsFilterCondition(arangodb::aql::AstNode const*,
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

/// @brief default iterator factory method. does not create an iterator
IndexIterator* Index::iteratorForCondition(transaction::Methods*,
                                           ManagedDocumentResult*,
                                           arangodb::aql::AstNode const*,
                                           arangodb::aql::Variable const*,
                                           bool) {
  // the super class index cannot create an iterator
  // the derived index classes have to manage this.
  return nullptr;
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
      if (!other->isConstant()) {
        return false;
      }

      if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE &&
          other->isNullValue()) {
        // != null. now note that a certain attribute cannot become null
        try {
          nonNullAttributes.emplace(access->toString());
        } catch (...) {
        }
      } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT) {
        // > null. now note that a certain attribute cannot become null
        try {
          nonNullAttributes.emplace(access->toString());
        } catch (...) {
        }
      } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE &&
                 !other->isNullValue()) {
        // >= non-null. now note that a certain attribute cannot become null
        try {
          nonNullAttributes.emplace(access->toString());
        } catch (...) {
        }
      }

      if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT ||
          op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE) {
        // <  and  <= are not supported with sparse indexes as this may include
        // null values
        try {
          // check if we've marked this attribute as being non-null already
          if (nonNullAttributes.find(access->toString()) ==
              nonNullAttributes.end()) {
            return false;
          }
        } catch (...) {
          return false;
        }
      }

      if (other->isNullValue() &&
          (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
           op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE)) {
        // ==  and  >= null are not supported with sparse indexes for the same
        // reason
        try {
          // check if we've marked this attribute as being non-null already
          if (nonNullAttributes.find(access->toString()) ==
              nonNullAttributes.end()) {
            return false;
          }
        } catch (...) {
          return false;
        }
      }

      if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN &&
          other->type == arangodb::aql::NODE_TYPE_ARRAY) {
        size_t const n = other->numMembers();

        for (size_t i = 0; i < n; ++i) {
          if (other->getMemberUnchecked(i)->isNullValue()) {
            return false;
          }
        }
      }
    }
  }

  if (isExecution) {
    // in execution phase, we do not need to check the variable usage again
    return true;
  }

  // test if the reference variable is contained on both side of the expression
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
    arangodb::aql::Ast::getReferencedVariables(other, variables);
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

void Index::warmup(arangodb::transaction::Methods*,
                   std::shared_ptr<basics::LocalTaskQueue>) {
  // Do nothing. If an index needs some warmup
  // it has to explicitly implement it.
}

std::pair<bool,double> Index::updateClusterEstimate(double defaultValue) {
  // try to receive an selectivity estimate for the index
  // from indexEstimates stored in the logical collection.
  // the caller has to guarantee that the _collection is valid.
  // on the coordinator _collection is not always vaild!

  std::pair<bool,double> rv(false,defaultValue);

  auto estimates = _collection->clusterIndexEstimates();
  auto found = estimates.find(std::to_string(_iid));

  if( found != estimates.end()){
    rv.first = true;
    rv.second = found->second;
    _clusterSelectivity = rv.second;
  }
  return rv;
};

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
