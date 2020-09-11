////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_INDEX_HINT_H
#define ARANGOD_AQL_INDEX_HINT_H 1

#include <iosfwd>
#include <string>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

namespace aql {
struct AstNode;

/// @brief container for index hint information
class IndexHint {
 public:
  enum HintType : uint8_t { Illegal, None, Simple };

 public:
  explicit IndexHint();
  explicit IndexHint(AstNode const* node);
  explicit IndexHint(arangodb::velocypack::Slice const& slice);

 public:
  HintType type() const;
  bool isForced() const;
  std::vector<std::string> const& hint() const;

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  std::string typeName() const;
  std::string toString() const;

 private:
  HintType _type;
  bool _forced;

  // actual hint is a recursive structure, with the data type determined by the
  // _type above; in the case of a nested IndexHint, the value of isForced() is
  // inherited
  struct HintData {
    std::vector<std::string> simple;
  } _hint;
};

std::ostream& operator<<(std::ostream& stream, arangodb::aql::IndexHint const& hint);

}  // namespace aql
}  // namespace arangodb

#endif
