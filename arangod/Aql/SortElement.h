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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Variable.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::aql {
class Ast;
struct Variable;

/// @brief sort element, consisting of variable, sort direction, and a possible
/// attribute path to dig into the document
struct SortElement {
 private:
  SortElement(Variable const* v, bool asc);

  SortElement(Variable const* v, bool asc, std::vector<std::string> path);

 public:
  static SortElement create(Variable const* v, bool asc);
  static SortElement createWithPath(Variable const* v, bool asc,
                                    std::vector<std::string> path);

  /// @brief stringify a sort element. note: the output of this should match the
  /// stringification output of an AstNode for an attribute access
  /// (e.g. foo.bar => $0.bar)
  std::string toString() const;

  /// @brief resets variable to v, and clears the attributePath
  void resetTo(Variable const* v);

  void replaceVariables(
      std::unordered_map<VariableId, Variable const*> const& replacements);

  void replaceAttributeAccess(Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable);

  void toVelocyPack(velocypack::Builder& builder) const;

  static SortElement fromVelocyPack(Ast* ast, velocypack::Slice info);

  // variable to sort by
  Variable const* var;

  // sort direction: true => ascending, false => descending
  bool ascending;

  // an extra attribute path to sort by, used by GatherNode in cluster for
  // merge-sorting
  std::vector<std::string> attributePath;
};

using SortElementVector = std::vector<SortElement>;

}  // namespace arangodb::aql
