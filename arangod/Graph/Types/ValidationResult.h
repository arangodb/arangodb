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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iosfwd>

namespace arangodb {
namespace graph {

class ValidationResult {
 public:
  friend std::ostream& operator<<(std::ostream& stream,
                                  ValidationResult const& res);

  enum class Type { UNKNOWN, TAKE, PRUNE, FILTER, FILTER_AND_PRUNE };

  explicit ValidationResult(Type type) : _type(type) {}

  bool isPruned() const noexcept;
  bool isFiltered() const noexcept;
  bool isUnknown() const noexcept;

  void combine(Type t) noexcept;

 private:
  Type _type = Type::UNKNOWN;
};

std::ostream& operator<<(std::ostream& stream, ValidationResult const& res);

}  // namespace graph
}  // namespace arangodb
