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

#ifndef ARANGOD_INDEXES_SIMPLE_ATTRIBUTE_EQUALITY_MATCHER_H
#define ARANGOD_INDEXES_SIMPLE_ATTRIBUTE_EQUALITY_MATCHER_H 1

#include "Basics/Common.h"
#include "Basics/AttributeNameParser.h"

namespace triagens {
namespace aql {
class Ast;
struct AstNode;
struct Variable;
}

namespace arango {
class Index;


class SimpleAttributeEqualityMatcher {
  
 public:
  explicit SimpleAttributeEqualityMatcher(
      std::vector<std::vector<triagens::basics::AttributeName>> const&);

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief match a single of the attributes
  /// this is used for the primary index and the edge index
  //////////////////////////////////////////////////////////////////////////////

  bool matchOne(triagens::arango::Index const*, triagens::aql::AstNode const*,
                triagens::aql::Variable const*, size_t, size_t&, double&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief match all of the attributes, in any order
  /// this is used for the hash index
  //////////////////////////////////////////////////////////////////////////////

  bool matchAll(triagens::arango::Index const*, triagens::aql::AstNode const*,
                triagens::aql::Variable const*, size_t, size_t&, double&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the condition parts that the index is responsible for
  /// this is used for the primary index and the edge index
  /// requires that a previous matchOne() returned true
  /// the caller must not free the returned AstNode*, as it belongs to the ast
  //////////////////////////////////////////////////////////////////////////////

  triagens::aql::AstNode* getOne(triagens::aql::Ast*,
                                 triagens::arango::Index const*,
                                 triagens::aql::AstNode const*,
                                 triagens::aql::Variable const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief specialize the condition for the index
  /// this is used for the primary index and the edge index
  /// requires that a previous matchOne() returned true
  //////////////////////////////////////////////////////////////////////////////

  triagens::aql::AstNode* specializeOne(triagens::arango::Index const*,
                                        triagens::aql::AstNode*,
                                        triagens::aql::Variable const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief specialize the condition for the index
  /// this is used for the hash index
  /// requires that a previous matchAll() returned true
  //////////////////////////////////////////////////////////////////////////////

  triagens::aql::AstNode* specializeAll(triagens::arango::Index const*,
                                        triagens::aql::AstNode*,
                                        triagens::aql::Variable const*);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief determine the costs of using this index and the number of items
  /// that will return in average
  /// cost values have no special meaning, except that multiple cost values are
  /// comparable, and lower values mean lower costs
  //////////////////////////////////////////////////////////////////////////////

  void calculateIndexCosts(triagens::arango::Index const*, size_t, size_t&,
                           double&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the access fits
  //////////////////////////////////////////////////////////////////////////////

  bool accessFitsIndex(triagens::arango::Index const*,
                       triagens::aql::AstNode const*,
                       triagens::aql::AstNode const*,
                       triagens::aql::AstNode const*,
                       triagens::aql::Variable const*, bool);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief array of attributes used for comparisons
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::vector<triagens::basics::AttributeName>> const _attributes;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief an internal map to mark which condition parts were useful and
  /// covered by the index. Also contains the matching Node
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<size_t, triagens::aql::AstNode const*> _found;
};
}
}

#endif


