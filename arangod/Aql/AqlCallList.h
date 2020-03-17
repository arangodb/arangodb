////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_CALL_LIST_H
#define ARANGOD_AQL_AQL_CALL_LIST_H 1

#include "Aql/AqlCall.h"

#include <optional>
#include <vector>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::aql {

class AqlCallList {
 public:
  friend bool operator==(AqlCallList const& left, AqlCallList const& right);

  friend auto operator<<(std::ostream& out, const arangodb::aql::AqlCallList& list)
      -> std::ostream&;

  explicit AqlCallList(AqlCall const& call);

  AqlCallList(AqlCall const& specificCall, AqlCall const& defaultCall);

  [[nodiscard]] auto popNextCall() -> AqlCall;
  [[nodiscard]] auto peekNextCall() const -> AqlCall const&;
  [[nodiscard]] auto hasMoreCalls() const noexcept -> bool;

  /**
   * @brief Get a reference to the next call.
   *        This is modifiable, but caller will not take
   *        responsibility.
   *
   * @return AqlCall& reference to the call, can be modified.
   */
  [[nodiscard]] auto modifyNextCall() -> AqlCall&;

  static auto fromVelocyPack(velocypack::Slice) -> ResultT<AqlCallList>;
  auto toVelocyPack(velocypack::Builder&) const -> void;
  auto toString() const -> std::string;

 private:
  std::vector<AqlCall> _specificCalls{};
  std::optional<AqlCall> _defaultCall{std::nullopt};
};

auto operator==(AqlCallList const& left, AqlCallList const& right) -> bool;

auto operator<<(std::ostream& out, const arangodb::aql::AqlCallList& list) -> std::ostream&;

}  // namespace arangodb::aql

#endif