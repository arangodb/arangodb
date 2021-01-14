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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "TraceEntry.h"

#include <iomanip>

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace graph {

TraceEntry::TraceEntry() {}
TraceEntry::~TraceEntry() = default;
void TraceEntry::addTiming(double timeTaken) {
  _count++;
  _total += timeTaken;
  if (_min > timeTaken) {
    _min = timeTaken;
  }
  if (_max < timeTaken) {
    _max = timeTaken;
  }
}

auto operator<<(std::ostream& out, TraceEntry const& entry) -> std::ostream& {
  if (entry._count == 0) {
    out << "not called";
  } else {
    out << "calls: " << entry._count << " min: " << std::setprecision(2)
        << std::fixed << entry._min * 1000.0 << "ms max: " << entry._max * 1000.0
        << "ms avg: " << entry._total / entry._count * 1000.0
        << "ms total: " << entry._total * 1000.0 << "ms";
  }
  return out;
}

}  // namespace graph
}  // namespace arangodb
