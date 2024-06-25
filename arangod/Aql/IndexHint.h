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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/FlatHashMap.h"

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct AstNode;
class QueryContext;

/// @brief container for index hint information
class IndexHint {
 public:
  struct FromCollectionOperation {};
  struct FromGraphOperation {};

  // there is an important distinction between None and Disabled here:
  //   None = no index hint set
  //   Disabled = no index must be used!
  enum HintType : uint8_t { kIllegal, kNone, kSimple, kNested, kDisabled };
  using DepthType = uint64_t;
  static constexpr DepthType BaseDepth = std::numeric_limits<DepthType>::max();

  using PossibleIndexes = std::vector<std::string>;

  // simple index hint, either "index-id" or ["index-id1", "index-id2", ...]
  using SimpleContents = PossibleIndexes;
  // nested index hint, used for traversals. the structure is:
  // { collection name => { direction => { level => index hint(s) } } }
  // e.g.
  // { "edges": { "outbound": { "base": "index1", "1": ["index2", "index3"] } }
  // } all levels must be numeric except the special value "base" (in
  // lowercase). allowed directions are "inbound" and "outbound" (both in lower
  // case only).
  using NestedContents = containers::FlatHashMap<
      /*collection*/ std::string,
      containers::FlatHashMap<
          /*direction*/ std::string,
          containers::FlatHashMap<DepthType, PossibleIndexes>>>;

  IndexHint() = default;
  explicit IndexHint(QueryContext& query, AstNode const* node,
                     FromCollectionOperation);
  explicit IndexHint(QueryContext& query, AstNode const* node,
                     FromGraphOperation);
  explicit IndexHint(velocypack::Slice slice);

  IndexHint(IndexHint&&) = default;
  IndexHint& operator=(IndexHint&&) = default;
  IndexHint(IndexHint const&) = default;
  IndexHint& operator=(IndexHint const&) = default;

  HintType type() const noexcept { return _type; }
  bool isForced() const noexcept { return _forced; }

  // returns true for hint types kSimple and kNested
  bool isSet() const noexcept;

  std::vector<std::string> const& candidateIndexes() const noexcept;

  void clear();
  void toVelocyPack(velocypack::Builder& builder) const;
  std::string_view typeName() const noexcept;
  std::string toString() const;

  size_t getLookahead() const noexcept { return _lookahead; }
  bool waitForSync() const noexcept { return _waitForSync; }

  IndexHint getFromNested(std::string_view direction,
                          std::string_view collection,
                          IndexHint::DepthType depth) const;

 private:
  bool empty() const noexcept;

  HintType _type{kNone};
  bool _forced{false};
  bool _waitForSync{false};
  size_t _lookahead{1};

  // actual hint is a recursive structure, with the data type determined by the
  // _type above; in the case of a nested IndexHint, the value of isForced() is
  // inherited
  std::variant<SimpleContents, NestedContents> _hint;
};

std::ostream& operator<<(std::ostream& stream,
                         arangodb::aql::IndexHint const& hint);

}  // namespace aql
}  // namespace arangodb
