////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iosfwd>
#include <string>
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
  // there is an important distinction between None and Disabled here:
  //   None = no index hint set
  //   Disabled = no index must be used!
  enum HintType : uint8_t { Illegal, None, Simple, Disabled };

 public:
  explicit IndexHint();
  explicit IndexHint(QueryContext& query, AstNode const* node);
  explicit IndexHint(arangodb::velocypack::Slice slice);

  HintType type() const noexcept;
  bool isForced() const noexcept;
  std::vector<std::string> const& hint() const noexcept;

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  std::string typeName() const;
  std::string toString() const;

  size_t getLookahead() const noexcept { return _lookahead; }

 private:
  HintType _type;
  bool _forced;
  size_t _lookahead = 1;

  // actual hint is a recursive structure, with the data type determined by the
  // _type above; in the case of a nested IndexHint, the value of isForced() is
  // inherited
  struct HintData {
    std::vector<std::string> simple;
  } _hint;
};

std::ostream& operator<<(std::ostream& stream,
                         arangodb::aql::IndexHint const& hint);

}  // namespace aql
}  // namespace arangodb
