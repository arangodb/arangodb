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

namespace arangodb {
namespace aql {
class Ast;
struct AstNode;
struct Variable;
}

class Index;

class SimpleAttributeEqualityMatcher {
 public:
  explicit SimpleAttributeEqualityMatcher(
      std::vector<std::vector<arangodb::basics::AttributeName>> const&);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief match a single of the attributes
  /// this is used for the primary index and the edge index
  //////////////////////////////////////////////////////////////////////////////

  bool matchOne(arangodb::Index const*, arangodb::aql::AstNode const*,
                arangodb::aql::Variable const*, size_t, size_t&, double&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief match all of the attributes, in any order
  /// this is used for the hash index
  //////////////////////////////////////////////////////////////////////////////

  bool matchAll(arangodb::Index const*, arangodb::aql::AstNode const*,
                arangodb::aql::Variable const*, size_t, size_t&, double&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the condition parts that the index is responsible for
  /// this is used for the primary index and the edge index
  /// requires that a previous matchOne() returned true
  /// the caller must not free the returned AstNode*, as it belongs to the ast
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::AstNode* getOne(arangodb::aql::Ast*, arangodb::Index const*,
                                 arangodb::aql::AstNode const*,
                                 arangodb::aql::Variable const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief specialize the condition for the index
  /// this is used for the primary index and the edge index
  /// requires that a previous matchOne() returned true
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::AstNode* specializeOne(arangodb::Index const*,
                                        arangodb::aql::AstNode*,
                                        arangodb::aql::Variable const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief specialize the condition for the index
  /// this is used for the hash index
  /// requires that a previous matchAll() returned true
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::AstNode* specializeAll(arangodb::Index const*,
                                        arangodb::aql::AstNode*,
                                        arangodb::aql::Variable const*);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief determine the costs of using this index and the number of items
  /// that will return in average
  /// cost values have no special meaning, except that multiple cost values are
  /// comparable, and lower values mean lower costs
  //////////////////////////////////////////////////////////////////////////////

  void calculateIndexCosts(arangodb::Index const*, size_t, size_t&,
                           double&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the access fits
  //////////////////////////////////////////////////////////////////////////////

  bool accessFitsIndex(arangodb::Index const*, arangodb::aql::AstNode const*,
                       arangodb::aql::AstNode const*,
                       arangodb::aql::AstNode const*,
                       arangodb::aql::Variable const*, bool);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief array of attributes used for comparisons
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::vector<arangodb::basics::AttributeName>> const& _attributes;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief an internal map to mark which condition parts were useful and
  /// covered by the index. Also contains the matching Node
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<size_t, arangodb::aql::AstNode const*> _found;
};
}

#endif
