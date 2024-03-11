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

#include <string>
#include <vector>

namespace arangodb::aql {
struct Variable;

/// @brief sort element, consisting of variable, sort direction, and a possible
/// attribute path to dig into the document
struct SortElement {
  Variable const* var;
  bool ascending;
  std::vector<std::string> attributePath;

  SortElement(Variable const* v, bool asc);

  SortElement(Variable const* v, bool asc, std::vector<std::string> path);

  /// @brief stringify a sort element. note: the output of this should match the
  /// stringification output of an AstNode for an attribute access
  /// (e.g. foo.bar => $0.bar)
  std::string toString() const;
};

using SortElementVector = std::vector<SortElement>;

}  // namespace arangodb::aql
