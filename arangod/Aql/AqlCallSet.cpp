////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AqlCallSet.h"

using namespace arangodb;
using namespace arangodb::aql;

auto aql::operator<<(std::ostream& out, AqlCallSet::DepCallPair const& callPair)
    -> std::ostream& {
  return out << callPair.dependency << " => " << callPair.call;
}

auto aql::operator<<(std::ostream& out, AqlCallSet const& callSet) -> std::ostream& {
  out << "[";
  auto first = true;
  for (auto const& it : callSet.calls) {
    if (first) {
      out << " ";
      first = false;
    } else {
      out << ", ";
    }
    out << it;
  }
  out << " ]";
  return out;
}

auto AqlCallSet::empty() const noexcept -> bool { return calls.empty(); }

auto AqlCallSet::size() const noexcept -> size_t { return calls.size(); }
