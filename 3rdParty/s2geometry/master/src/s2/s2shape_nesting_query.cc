// Copyright 2022 Google Inc. All Rights Reserved.
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


#include "s2/s2shape_nesting_query.h"

#include <algorithm>
#include <climits>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "s2/util/bitmap/bitmap.h"
#include "s2/s2crossing_edge_query.h"
#include "s2/s2latlng.h"
#include "s2/s2predicates.h"

using std::vector;
using util::bitmap::Bitmap64;

// Takes N equally spaced points from the given chain of the shape and finds
// the one closest to the target point, returning its index.
static inline int ClosestOfNPoints(const S2Point& target, const S2Shape& shape,
                                   int chain, int num_points) {
  const int chain_len = shape.chain(chain).length;

  // If chain_len is < num_points, we still want to use whatever points there
  // are in the chain, so max 1 the minimum step size and we'll modulo to get
  // back into bounds for the chain.
  const int step = std::max(1, chain_len / num_points);

  double min_dist2 = std::numeric_limits<double>::infinity();
  int closest_idx = 0;
  for (int i = 0; i < num_points; ++i) {
    int idx = (i * step) % chain_len;
    S2Point point = shape.chain_edge(chain, idx).v0;
    double dist2 = (target - point).Norm2();
    if (dist2 < min_dist2) {
      min_dist2 = dist2;
      closest_idx = idx;
    }
  }
  return closest_idx;
}

// Returns the next edge of a particular chain, handling index wrap around.
static inline S2Shape::Edge NextChainEdge(const S2Shape* shape, int chain,
                                          int edge) {
  return shape->chain_edge(chain, (edge + 1) % shape->chain(chain).length);
}

// Returns the previous edge of a particular chain, handling index wrap around.
static inline S2Shape::Edge PrevChainEdge(const S2Shape* shape, int chain,
                                          int edge) {
  int index = edge - 1;
  if (index < 0) {
    index = shape->chain(chain).length - 1;
  }
  return shape->chain_edge(chain, index);
}

S2ShapeNestingQuery::S2ShapeNestingQuery(const S2ShapeIndex* index,
                                         const Options& options) {
  Init(index, options);
}

void S2ShapeNestingQuery::Init(const S2ShapeIndex* index,
                               const Options& options) {
  index_ = index;
  options_ = options;
}

// Finds three consecutive vertices that aren't degenerate and stores them in
// the given array.  Returns false if there aren't three consecutive
// non-degenerate points.
vector<S2ShapeNestingQuery::ChainRelation>
S2ShapeNestingQuery::ComputeShapeNesting(int shape_id) {
  const S2Shape* shape = index_->shape(shape_id);
  if (shape == nullptr || shape->num_chains() == 0) {
    return {};
  }
  S2_DCHECK_EQ(shape->dimension(), 2);

  const int num_chains = shape->num_chains();

  // A single chain is always a shell, with no holes.
  if (num_chains == 1) {
    return {ChainRelation::MakeShell()};
  }

  // Sets to track possible parents and possible children for each chain.
  absl::FixedArray<Bitmap64> parents(num_chains, Bitmap64(num_chains, false));
  absl::FixedArray<Bitmap64> children(num_chains, Bitmap64(num_chains, false));

  // We'll compute edge crossings along a line segment from the datum shell to a
  // random point on the other chains.  This choice is arbitrary, so we'll use
  // the first vertex of edge 1 so we can easily get the next and previous
  // points to check for orientation.
  int32 datum_shell = options().datum_strategy()(shape);
  const S2Point vertices[3] = {
      shape->chain_edge(datum_shell, 0).v0,
      shape->chain_edge(datum_shell, 1).v0,
      shape->chain_edge(datum_shell, 2).v0,
  };
  const S2Point start_point = vertices[1];

  S2CrossingEdgeQuery crossing_query(index_);
  vector<s2shapeutil::ShapeEdge> edges;
  for (int chain = 0; chain < num_chains; ++chain) {
    if (chain == datum_shell) {
      continue;
    }
    S2_VLOG(1) << "Processing chain " << chain;

    // Find a close point on the target chain out of 4 equally spaced ones.
    int end_idx = ClosestOfNPoints(start_point, *shape, chain, 4);
    S2Point end_point = shape->chain_edge(chain, end_idx).v0;

    // We need to know whether we're inside the datum shell at the end, so we
    // need to properly seed its starting state.  If we start by entering the
    // datum shell's interior _and_ end by arriving from the target chain's
    // interior, we set it to true.
    //
    // As we cross edges from the datum to the target chain the total number of
    // datum shell _or_ target chain edges we'll cross is either even or odd.
    // Each of these edges toggles our "insideness" relative to the datum shell,
    if (s2pred::OrderedCCW(vertices[2], end_point, vertices[0], start_point)) {
      S2_VLOG(1) << "  Edge starts into interior of datum chain";
      parents[chain].Set(datum_shell, true);
      children[datum_shell].Set(chain, true);
    }

    // Arriving from the interior of the target chain?
    S2Point next = NextChainEdge(shape, chain, end_idx).v0;
    S2Point prev = PrevChainEdge(shape, chain, end_idx).v0;
    if (s2pred::OrderedCCW(next, start_point, prev, end_point)) {
      S2_VLOG(1) << "  Edge ends from interior of target chain";
      parents[chain].Set(chain, true);
    }
    S2_VLOG(2) << "    Initial set: " << parents[chain].ToString(8);

    // Query all the edges crossed by the line from the datum shell to a point
    // on this chain.  Only look at edges that belong to the requested shape.
    // Using INTERIOR here will avoid returning the two edges on the datum and
    // target shells that are touched by the endpoints of our line segment.
    crossing_query.GetCrossingEdges(start_point, end_point, *shape,
                                    s2shapeutil::CrossingType::INTERIOR,
                                    &edges);

    // Walk through the intersected chains and toggle corresponding bits.
    for (const auto& edge : edges) {
      int32 other_chain = shape->chain_position(edge.id().edge_id).chain_id;

      parents[chain].Toggle(other_chain);
      if (other_chain != chain) {
        children[other_chain].Toggle(chain);
      }
      S2_VLOG(1) << "  Crosses chain " << other_chain;
      S2_VLOG(2) << "    Parent set: " << parents[chain].ToString(8);
    }

    // Now set the final state.  Remove the target chain from its own parent set
    // to make following logic simpler.  The datum shell is a potential parent
    // if both parent and target chain bits are set.
    parents[chain].Set(datum_shell, parents[chain].Get(datum_shell) &&
                                        parents[chain].Get(chain));
    parents[chain].Set(chain, false);
  }

  if (S2_VLOG_IS_ON(2)) {
    S2_LOG(INFO) << "Current parent set";
    for (int chain = 0; chain < num_chains; ++chain) {
      S2_LOG(INFO) << "  " << absl::StrFormat("%2d", chain) << ": "
                << parents[chain].ToString(8);
    }
  }

  // Look at each chain with a single parent and remove the parent from any of
  // its child chains.  This enforces the constraint that if A is a parent of B
  // and B is a parent of C, then A shouldn't directly be a parent of C.
  for (int current_chain = 0; current_chain < num_chains; ++current_chain) {
    if (parents[current_chain].GetOnesCount() != 1) {
      continue;
    }

    Bitmap64::size_type parent_chain;
    parents[current_chain].FindFirstSetBit(&parent_chain);

    int next_chain = current_chain;
    Bitmap64::size_type child = 0;
    for (; children[current_chain].FindNextSetBit(&child); child++) {
      if (parents[child].Get(parent_chain)) {
        parents[child].Set(parent_chain, false);

        // If this chain has a single parent now, we have to process it as well,
        // so if we've already passed it in the outer loop, we have to back up.
        if (parents[child].GetOnesCount() == 1 && child < next_chain) {
          next_chain = child;
        }
      }
    }

    S2_VLOG(1) << "Chain " << current_chain << " has one parent";
    if (S2_VLOG_IS_ON(2)) {
      S2_LOG(INFO) << "  Parent set now:";
      for (int chain = 0; chain < num_chains; ++chain) {
        S2_LOG(INFO) << "  " << absl::StrFormat("%2d", chain) << ": "
                  << parents[chain].ToString(8);
      }
    }

    // Backup current chain so next loop increment sets it properly
    if (next_chain != current_chain) {
      current_chain = next_chain - 1;
    }
  }

  // Each chain now points to its immediate parent.  Scan through and set child
  // to point to parent and vice-versa.
  vector<ChainRelation> relations(num_chains);
  for (int chain = 0; chain < num_chains; ++chain) {
    S2_DCHECK_LE(parents[chain].GetOnesCount(), 1);

    Bitmap64::size_type parent;
    if (parents[chain].FindFirstSetBit(&parent)) {
      relations[chain].SetParent(parent);
      relations[parent].AddHole(chain);
    }
  }

  // Detach any chains that are even depth from their parent and make them
  // shells.  This is effectively implementing the even/odd rule.
  for (int chain = 0; chain < num_chains; ++chain) {
    int depth = -1, current = chain;
    do {
      ++depth;
      current = relations[current].parent_id();
    } while (current >= 0 && depth < num_chains);
    S2_DCHECK_LT(depth, num_chains);

    if (depth && (depth % 2 == 0)) {
      relations[chain].ClearParent();
    }
  }

  return relations;
}
