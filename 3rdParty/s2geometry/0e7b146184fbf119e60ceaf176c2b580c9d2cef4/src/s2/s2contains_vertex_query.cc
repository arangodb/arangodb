// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2contains_vertex_query.h"

#include <cmath>
#include <utility>
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

using std::abs;

int S2ContainsVertexQuery::ContainsSign() {
  // Find the unmatched edge that is immediately clockwise from S2::RefDir(P)
  // but not equal to it.  The result is +1 iff this edge is outgoing.
  //
  // A loop with consecutive vertices A,B,C contains vertex B if and only if
  // the fixed vector R = S2::RefDir(B) is contained by the wedge ABC.  The
  // wedge is closed at A and open at C, i.e. the point B is inside the loop
  // if A = R but not if C = R.  This convention is required for compatibility
  // with S2::VertexCrossing.
  S2Point reference_dir = S2::RefDir(target_);
  std::pair<S2Point, int> best(reference_dir, 0);
  for (const auto& e : edge_map_) {
    S2_DCHECK_LE(abs(e.second), 1);
    if (e.second == 0) continue;  // This is a "matched" edge.
    if (s2pred::OrderedCCW(reference_dir, best.first, e.first, target_)) {
      best = e;
    }
  }
  return best.second;
}
