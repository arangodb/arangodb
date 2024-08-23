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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <limits>
#include <numeric>
#include <ostream>

namespace arangodb {
namespace graph {

class TraceEntry {
 public:
  TraceEntry();
  ~TraceEntry();

  void addTiming(double timeTaken) noexcept;

  friend auto operator<<(std::ostream& out, TraceEntry const& entry)
      -> std::ostream&;

 private:
  double _min{std::numeric_limits<double>::max()};
  double _max{0};
  double _total{0};
  uint64_t _count{0};
};

}  // namespace graph
}  // namespace arangodb
