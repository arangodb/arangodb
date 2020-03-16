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

#include "AqlCallList.h"

using namespace arangodb::aql;

AqlCallList::AqlCallList(AqlCall const& call) : _specificCalls{call} {}

AqlCallList::AqlCallList(AqlCall const& specificCall, AqlCall const& defaultCall)
    : _specificCalls{specificCall}, _defaultCall{defaultCall} {}

[[nodiscard]] auto AqlCallList::popNextCall() -> AqlCall {
  TRI_ASSERT(hasMoreCalls());
  if (!_specificCalls.empty()) {
    // We only implemented for a single given call.
    TRI_ASSERT(_specificCalls.size() == 1);
    auto res = _specificCalls.back();
    _specificCalls.pop_back();
    return res;
  }
  TRI_ASSERT(_defaultCall.has_value());
  return _defaultCall.value();
}

[[nodiscard]] auto AqlCallList::hasMoreCalls() const noexcept -> bool {
  return !_specificCalls.empty() || _defaultCall.has_value();
}