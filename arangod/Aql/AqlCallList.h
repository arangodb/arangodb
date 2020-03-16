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

namespace arangodb::aql {

class AqlCallList {
 public:
  explicit AqlCallList(AqlCall const& call);

  explicit AqlCallList(AqlCall const& specificCall, AqlCall const& defaultCall);

  [[nodiscard]] auto popNextCall() -> AqlCall;
  [[nodiscard]] auto hasMoreCalls() const noexcept -> bool;

 private:
  std::vector<AqlCall> _specificCalls{};
  std::optional<AqlCall> _defaultCall{std::nullopt};
};
}  // namespace arangodb::aql

#endif