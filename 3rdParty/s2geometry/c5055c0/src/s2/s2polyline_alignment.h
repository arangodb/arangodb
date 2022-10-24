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


#ifndef S2_S2POLYLINE_ALIGNMENT_H_
#define S2_S2POLYLINE_ALIGNMENT_H_

#include <memory>
#include <vector>

#include "s2/s2polyline.h"

// This library provides code to compute vertex alignments between S2Polylines.
//
// A vertex "alignment" or "warp" between two polylines is a matching between
// pairs of their vertices. Users can imagine pairing each vertex from
// S2Polyline `a` with at least one other vertex in S2Polyline `b`. The "cost"
// of an arbitrary alignment is defined as the summed value of the squared
// chordal distance between each pair of points in the warp path. An "optimal
// alignment" for a pair of polylines is defined as the alignment with least
// cost. Note: optimal alignments are not necessarily unique. The standard way
// of computing an optimal alignment between two sequences is the use of the
// `Dynamic Timewarp` algorithm.
//
// We provide three methods for computing (via Dynamic Timewarp) the optimal
// alignment between two S2Polylines. These methods are performance-sensitive,
// and have been reasonably optimized for space- and time- usage. On modern
// hardware, it is possible to compute exact alignments between 4096x4096
// element polylines in ~70ms, and approximate alignments much more quickly.
//
// The results of an alignment operation are captured in a VertexAlignment
// object. In particular, a VertexAlignment keeps track of the total cost of
// alignment, as well as the warp path (a sequence of pairs of indices into each
// polyline whose vertices are linked together in the optimal alignment)
//
// For a worked example, consider the polylines
//
// a = [(1, 0), (5, 0), (6, 0), (9, 0)] and
// b = [(2, 0), (7, 0), (8, 0)].
//
// The "cost matrix" between these two polylines (using squared chordal
// distance, .Norm2(), as our distance function) looks like this:
//
//        (2, 0)  (7, 0)  (8, 0)
// (1, 0)     1      36      49
// (5, 0)     9       4       9
// (6, 0)    16       1       4
// (9, 0)    49       4       1
//
// The Dynamic Timewarp DP table for this cost matrix has cells defined by
//
// table[i][j] = cost(i,j) + min(table[i-1][j-1], table[i][j-1], table[i-1, j])
//
//        (2, 0)  (7, 0)  (8, 0)
// (1, 0)     1      37      86
// (5, 0)    10       5      14
// (6, 0)    26       6       9
// (9, 0)    75      10       7
//
// Starting at the bottom right corner of the DP table, we can work our way
// backwards to the upper left corner  to recover the reverse of the warp path:
// (3, 2) -> (2, 1) -> (1, 1) -> (0, 0). The VertexAlignment produced containing
// this has alignment_cost = 7 and warp_path = {(0, 0), (1, 1), (2, 1), (3, 2)}.
//
// We also provide methods for performing alignment of multiple sequences. These
// methods return a single, representative polyline from a non-empty collection
// of polylines, for various definitions of "representative."
//
// GetMedoidPolyline() returns a new polyline (point-for-point-equal to some
// existing polyline from the collection) that minimizes the summed vertex
// alignment cost to all other polylines in the collection.
//
// GetConsensusPolyline() returns a new polyline (unlikely to be present in the
// input collection) that represents a "weighted consensus" polyline. This
// polyline is constructed iteratively using the Dynamic Timewarp Barycenter
// Averaging algorithm of F. Petitjean, A. Ketterlin, and P. Gancarski, which
// can be found here:
// https://pdfs.semanticscholar.org/a596/8ca9488199291ffe5473643142862293d69d.pdf

namespace s2polyline_alignment {

typedef std::vector<std::pair<int, int>> WarpPath;

struct VertexAlignment {
  // `alignment_cost` represents the sum of the squared chordal distances
  // between each pair of vertices in the warp path. Specifically,
  // cost = sum_{(i, j) \in path} (a.vertex(i) - b.vertex(j)).Norm2();
  // This means that the units of alignment_cost are "squared distance". This is
  // an optimization to avoid the (expensive) atan computation of the true
  // spherical angular distance between the points, as well as an unnecessary
  // square root. All we need to compute vertex alignment is a metric that
  // satisifies the triangle inequality, and squared chordal distance works as
  // well as spherical S1Angle distance for this purpose.
  double alignment_cost;

  // Each entry (i, j) of `warp_path` represents a pairing between vertex
  // a.vertex(i) and vertex b.vertex(j) in the optimal alignment.
  // The warp_path is defined in forward order, such that the result of
  // aligning polylines `a` and `b` is always a warp_path with warp_path.front()
  // = {0,0} and warp_path.back() = {a.num_vertices() - 1, b.num_vertices() - 1}
  // Note that this DOES NOT define an alignment from a point sequence to an
  // edge sequence. That functionality may come at a later date.
  WarpPath warp_path;

  VertexAlignment(const double cost, const WarpPath& path)
      : alignment_cost(cost), warp_path(path) {}
};

// GetExactVertexAlignment takes two non-empty polylines as input, and returns
// the VertexAlignment corresponding to the optimal alignment between them. This
// method is quadratic O(A*B) in both space and time complexity.
VertexAlignment GetExactVertexAlignment(const S2Polyline& a,
                                        const S2Polyline& b);

// GetExactVertexAlignmentCost takes two non-empty polylines as input, and
// returns the *cost* of their optimal alignment. A standard, traditional
// dynamic timewarp algorithm can output both a warp path and a cost, but
// requires quadratic space to reconstruct the path by walking back through the
// Dynamic Programming cost table. If all you really need is the warp cost (i.e.
// you're inducing a similarity metric between S2Polylines, or something
// equivalent), you can overwrite the DP table and use constant space -
// O(max(A,B)). This method provides that space-efficiency optimization.
double GetExactVertexAlignmentCost(const S2Polyline& a, const S2Polyline& b);

// GetApproxVertexAlignment takes two non-empty polylines `a` and `b` as input,
// and a `radius` paramater GetApproxVertexAlignment (quickly) computes an
// approximately optimal vertex alignment of points between polylines `a` and
// `b` by implementing the algorithm described in `FastDTW: Toward Accurate
// Dynamic Time Warping in Linear Time and Space` by Stan Salvador and Philip
// Chan. Details can be found below:
//
// https://pdfs.semanticscholar.org/05a2/0cde15e172fc82f32774dd0cf4fe5827cad2.pdf
//
// The `radius` parameter controls the distance we search outside of the
// projected warp path during the refining step. Smaller values of `radius`
// correspond to a smaller search window, and therefore distance computation on
// fewer cells, which leads to a faster (but worse) approximation.
// This method is O(max(A, B)) in both space and time complexity.
VertexAlignment GetApproxVertexAlignment(const S2Polyline& a,
                                         const S2Polyline& b, const int radius);

// A convience overload for GetApproxVertexAlignment which computes and uses
// suggested default parameter of radius = max(a.size(), b.size())^0.25
VertexAlignment GetApproxVertexAlignment(const S2Polyline& a,
                                         const S2Polyline& b);

// GetMedoidPolyline returns the index `p` of a "medoid" polyline from a
// non-empty collection of `polylines` such that
//
// sum_{all j in `polylines`} VertexAlignmentCost(p, j) is minimized.
//
// In the case of a tie for minimal summed alignment cost, we return the lowest
// index - this tie is guaranteed to happen in the two-polyline-input case.
//
// ASYMPTOTIC BEHAVIOR:
// Computation may require up to (N^2 - N) / 2 alignment cost function
// evaluations, for N input polylines. For polylines of length U, V, the
// alignment cost function evaluation is O(U+V) if options.approx = true and
// O(U*V) if options.approx = false.

class MedoidOptions {
 public:
  // If options.approx = false, we compute vertex alignment costs exactly.
  // If options.approx = true, we use approximate vertex alignment
  // computation, called with the default radius parameter.
  bool approx() const { return approx_; }
  void set_approx(bool approx) { approx_ = approx; }

 private:
  bool approx_ = true;
};

int GetMedoidPolyline(const std::vector<std::unique_ptr<S2Polyline>>& polylines,
                      const MedoidOptions options);

// GetConsensusPolyline allocates and returns a new "consensus" polyline from a
// non-empty collection of polylines. We iteratively apply Dynamic Timewarp
// Barycenter Averaging to an initial `seed` polyline, which improves the
// consensus alignment quality each iteration. For implementation details, see
//
// https://pdfs.semanticscholar.org/a596/8ca9488199291ffe5473643142862293d69d.pdf
//
// The returned polyline from this method is unlikely to be point-for-point
// equal to an input polyline, whereas a polyline returned from
// GetMedoidPolyline() is guaranteed to match an input polyline point-for-point.
// NOTE: the number of points in our returned consensus polyline is always equal
// to the number of points in the initial seed, which is implementation-defined.
// If the collection of polylines has a large resolution distribution, it might
// be a good idea to reinterpolate them to have about the same number of points.
// In practice, this doesn't seem to matter, but is probably worth noting.
//
// ASYMPTOTIC BEHAVIOR:
// Seeding this algorithm requires O(1) vertex alignments if seed_medoid =
// false, and O(N^2) vertex alignments if seed_medoid = true. Once the seed
// polyline is chosen, computing the consensus polyline requires at most
// (iteration_cap)*N vertex alignments. For polylines of length U, V, the
// alignment cost function evaluation is O(U+V) if options.approx = true, and
// O(U*V) if options.approx = false.

class ConsensusOptions {
 public:
  // If options.approx = false, vertex alignments are computed with
  // GetExactVertexAlignment. If options.approx = true, vertex alignments are
  // computed with GetApproxVertexAlignment, called with default radius
  // parameter.
  bool approx() const { return approx_; }
  void set_approx(bool approx) { approx_ = approx; }

  // If options.seed_medoid = true, we seed the consensus polyline with the
  // medoid of the collection. This is a more expensive approach, but may result
  // in higher quality consensus sequences by avoiding bad arbitrary initial
  // seeds. Seeding with the medoid will incur up to (N^2 - N) / 2 evaluations
  // of the vertex alignment function. If options.seed_medoid = false, we seed
  // the consensus polyline by taking an arbitrary element from the collection.
  bool seed_medoid() const { return seed_medoid_; }
  void set_seed_medoid(bool seed_medoid) { seed_medoid_ = seed_medoid; }

  // options.iteration_cap controls the maximum number of DBA refining steps we
  // apply to the initial seed.
  int iteration_cap() const { return iteration_cap_; }
  void set_iteration_cap(int iteration_cap) { iteration_cap_ = iteration_cap; }

 private:
  bool approx_ = true;
  bool seed_medoid_ = false;
  int iteration_cap_ = 5;
};

std::unique_ptr<S2Polyline> GetConsensusPolyline(
    const std::vector<std::unique_ptr<S2Polyline>>& polylines,
    const ConsensusOptions options);
}  // namespace s2polyline_alignment
#endif  // S2_S2POLYLINE_ALIGNMENT_H_
