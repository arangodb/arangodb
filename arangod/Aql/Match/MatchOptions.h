////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb::aql {
struct AstNode;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief Typed MATCH statement OPTIONS.
/// rejected-unknown-field behavior is enforced via Inspector.
struct MatchOptions {
  /// @brief Parses MATCH OPTIONS from the AST node.
  /// @param node nullptr or NODE_TYPE_OBJECT accepted.
  /// Throws TRI_ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE on unknown fields.
  static MatchOptions fromAstNode(AstNode const* node);
};

template<class Inspector>
auto inspect(Inspector& f, MatchOptions& options) {
  return f.object(options).fields();
}

}  // namespace arangodb::aql::match
