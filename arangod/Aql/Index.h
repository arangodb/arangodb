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
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include <iosfwd>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace triagens {
namespace aql {
class Ast;
struct AstNode;
class SortCondition;
struct Variable;


struct Index {
  
  Index(Index const&) = delete;
  Index& operator=(Index const&) = delete;

  explicit Index(triagens::arango::Index* idx)
      : id(idx->id()),
        type(idx->type()),
        unique(false),
        sparse(false),
        ownsInternals(false),
        fields(idx->fields()),
        internals(idx) {
    TRI_ASSERT(internals != nullptr);

    if (type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
      unique = true;
    } else if (type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX ||
               type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
      sparse = idx->sparse();
      unique = idx->unique();
    }
  }

  explicit Index(VPackSlice const& slice)
      : id(triagens::basics::StringUtils::uint64(
            triagens::basics::VelocyPackHelper::checkAndGetStringValue(slice,
                                                                       "id"))),
        type(triagens::arango::Index::type(
            triagens::basics::VelocyPackHelper::checkAndGetStringValue(slice,
                                                                       "type")
                .c_str())),
        unique(triagens::basics::VelocyPackHelper::getBooleanValue(
            slice, "unique", false)),
        sparse(triagens::basics::VelocyPackHelper::getBooleanValue(
            slice, "sparse", false)),
        ownsInternals(false),
        fields(),
        internals(nullptr) {
    VPackSlice const f = slice.get("fields");

    if (f.isArray()) {
      size_t const n = static_cast<size_t>(f.length());
      fields.reserve(n);

      for (auto const& name : VPackArrayIterator(f)) {

        if (name.isString()) {
          std::vector<triagens::basics::AttributeName> parsedAttributes;
          TRI_ParseAttributeString(name.copyString(), parsedAttributes);
          fields.emplace_back(parsedAttributes);
        }
      }
    }

    // it is the caller's responsibility to fill the internals attribute with
    // something sensible later!
  }

  ~Index();

  triagens::basics::Json toJson() const {
    triagens::basics::Json json(triagens::basics::Json::Object);

    json("type",
         triagens::basics::Json(triagens::arango::Index::typeName(type)))(
        "id", triagens::basics::Json(triagens::basics::StringUtils::itoa(id)))(
        "unique", triagens::basics::Json(unique))(
        "sparse", triagens::basics::Json(sparse));

    if (hasSelectivityEstimate()) {
      json("selectivityEstimate",
           triagens::basics::Json(selectivityEstimate()));
    }

    triagens::basics::Json f(triagens::basics::Json::Array);
    for (auto const& field : fields) {
      std::string tmp;
      TRI_AttributeNamesToString(field, tmp);
      f.add(triagens::basics::Json(tmp));
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

  triagens::arango::Index* getInternals() const;

  void setInternals(triagens::arango::Index*, bool);

  bool isSorted() const {
    try {
      return getInternals()->isSorted();
    } catch (...) {
      return type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether or not the index supports the filter condition
  /// and calculate the filter costs and number of items
  //////////////////////////////////////////////////////////////////////////////

  bool supportsFilterCondition(triagens::aql::AstNode const*,
                               triagens::aql::Variable const*, size_t, size_t&,
                               double&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether or not the index supports the sort condition
  /// and calculate the sort costs
  //////////////////////////////////////////////////////////////////////////////

  bool supportsSortCondition(triagens::aql::SortCondition const*,
                             triagens::aql::Variable const*, size_t,
                             double&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get an iterator for the index
  //////////////////////////////////////////////////////////////////////////////

  triagens::arango::IndexIterator* getIterator(
      triagens::arango::Transaction*, triagens::arango::IndexIteratorContext*,
      triagens::aql::Ast*, triagens::aql::AstNode const*,
      triagens::aql::Variable const*, bool) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief specialize the condition for the index
  /// this will remove all nodes from the condition that the index cannot
  /// handle
  //////////////////////////////////////////////////////////////////////////////

  triagens::aql::AstNode* specializeCondition(
      triagens::aql::AstNode*, triagens::aql::Variable const*) const;

  
 public:
  TRI_idx_iid_t const id;
  triagens::arango::Index::IndexType type;
  bool unique;
  bool sparse;
  bool ownsInternals;
  std::vector<std::vector<triagens::basics::AttributeName>> fields;

 private:
  triagens::arango::Index* internals;
};
}
}

std::ostream& operator<<(std::ostream&, triagens::aql::Index const*);
std::ostream& operator<<(std::ostream&, triagens::aql::Index const&);

#endif


