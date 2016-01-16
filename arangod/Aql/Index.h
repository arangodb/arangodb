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
/// @author not James
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_INDEX_H
#define ARANGOD_AQL_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include <iosfwd>

namespace arangodb {
namespace aql {
class Ast;
struct AstNode;
class SortCondition;
struct Variable;


struct Index {
  
  Index(Index const&) = delete;
  Index& operator=(Index const&) = delete;

  explicit Index(arangodb::arango::Index* idx)
      : id(idx->id()),
        type(idx->type()),
        unique(false),
        sparse(false),
        ownsInternals(false),
        fields(idx->fields()),
        internals(idx) {
    TRI_ASSERT(internals != nullptr);

    if (type == arangodb::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
      unique = true;
    } else if (type == arangodb::arango::Index::TRI_IDX_TYPE_HASH_INDEX ||
               type == arangodb::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
      sparse = idx->sparse();
      unique = idx->unique();
    }
  }

  explicit Index(TRI_json_t const* json)
      : id(arangodb::basics::StringUtils::uint64(
            arangodb::basics::JsonHelper::checkAndGetStringValue(json, "id"))),
        type(arangodb::arango::Index::type(
            arangodb::basics::JsonHelper::checkAndGetStringValue(json, "type")
                .c_str())),
        unique(arangodb::basics::JsonHelper::getBooleanValue(json, "unique",
                                                             false)),
        sparse(arangodb::basics::JsonHelper::getBooleanValue(json, "sparse",
                                                             false)),
        ownsInternals(false),
        fields(),
        internals(nullptr) {
    TRI_json_t const* f = TRI_LookupObjectJson(json, "fields");

    if (TRI_IsArrayJson(f)) {
      size_t const n = TRI_LengthArrayJson(f);
      fields.reserve(n);

      for (size_t i = 0; i < n; ++i) {
        auto* name = static_cast<TRI_json_t const*>(
            TRI_AtVector(&f->_value._objects, i));

        if (TRI_IsStringJson(name)) {
          std::vector<arangodb::basics::AttributeName> parsedAttributes;
          TRI_ParseAttributeString(std::string(name->_value._string.data,
                                               name->_value._string.length - 1),
                                   parsedAttributes);
          fields.emplace_back(parsedAttributes);
        }
      }
    }

    // it is the caller's responsibility to fill the internals attribute with
    // something sensible later!
  }

  ~Index();

  arangodb::basics::Json toJson() const {
    arangodb::basics::Json json(arangodb::basics::Json::Object);

    json("type",
         arangodb::basics::Json(arangodb::arango::Index::typeName(type)))(
        "id", arangodb::basics::Json(arangodb::basics::StringUtils::itoa(id)))(
        "unique", arangodb::basics::Json(unique))(
        "sparse", arangodb::basics::Json(sparse));

    if (hasSelectivityEstimate()) {
      json("selectivityEstimate",
           arangodb::basics::Json(selectivityEstimate()));
    }

    arangodb::basics::Json f(arangodb::basics::Json::Array);
    for (auto const& field : fields) {
      std::string tmp;
      TRI_AttributeNamesToString(field, tmp);
      f.add(arangodb::basics::Json(tmp));
    }

    json("fields", f);
    return json;
  }

  bool hasSelectivityEstimate() const {
    if (!hasInternals()) {
      return false;
    }

    return getInternals()->hasSelectivityEstimate();
  }

  double selectivityEstimate() const {
    auto internals = getInternals();

    TRI_ASSERT(internals->hasSelectivityEstimate());

    return internals->selectivityEstimate();
  }

  inline bool hasInternals() const { return (internals != nullptr); }

  arangodb::arango::Index* getInternals() const;

  void setInternals(arangodb::arango::Index*, bool);

  bool isSorted() const {
    try {
      return getInternals()->isSorted();
    } catch (...) {
      return type == arangodb::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether or not the index supports the filter condition
  /// and calculate the filter costs and number of items
  //////////////////////////////////////////////////////////////////////////////

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether or not the index supports the sort condition
  /// and calculate the sort costs
  //////////////////////////////////////////////////////////////////////////////

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t,
                             double&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get an iterator for the index
  //////////////////////////////////////////////////////////////////////////////

  arangodb::arango::IndexIterator* getIterator(
      arangodb::arango::Transaction*, arangodb::arango::IndexIteratorContext*,
      arangodb::aql::Ast*, arangodb::aql::AstNode const*,
      arangodb::aql::Variable const*, bool) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief specialize the condition for the index
  /// this will remove all nodes from the condition that the index cannot
  /// handle
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const;

  
 public:
  TRI_idx_iid_t const id;
  arangodb::arango::Index::IndexType type;
  bool unique;
  bool sparse;
  bool ownsInternals;
  std::vector<std::vector<arangodb::basics::AttributeName>> fields;

 private:
  arangodb::arango::Index* internals;
};
}
}

std::ostream& operator<<(std::ostream&, arangodb::aql::Index const*);
std::ostream& operator<<(std::ostream&, arangodb::aql::Index const&);

#endif


