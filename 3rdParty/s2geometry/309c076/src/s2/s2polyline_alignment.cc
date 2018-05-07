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


#include "s2/s2polyline_alignment.h"
#include "s2/s2polyline_alignment_internal.h"

#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "s2/base/logging.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/util/math/mathutil.h"

namespace s2polyline_alignment {

Window::Window(const std::vector<ColumnStride>& strides) {
  S2_DCHECK(!strides.empty()) << "Cannot construct empty window.";
  S2_DCHECK(strides[0].start == 0) << "First element of start_cols is non-zero.";
  strides_ = strides;
  rows_ = strides.size();
  cols_ = strides.back().end;
  S2_DCHECK(this->IsValid()) << "Constructor validity check fail.";
}

Window::Window(const WarpPath& warp_path) {
  S2_DCHECK(!warp_path.empty()) << "Cannot construct window from empty warp path.";
  S2_DCHECK(warp_path.front() == std::make_pair(0, 0)) << "Must start at (0, 0).";
  rows_ = warp_path.back().first + 1;
  S2_DCHECK(rows_ > 0) << "Must have at least one row.";
  cols_ = warp_path.back().second + 1;
  S2_DCHECK(cols_ > 0) << "Must have at least one column.";
  strides_.resize(rows_);

  int prev_row = 0;
  int curr_row = 0;
  int stride_start = 0;
  int stride_stop = 0;
  for (const auto& pair : warp_path) {
    curr_row = pair.first;
    if (curr_row > prev_row) {
      strides_[prev_row] = {stride_start, stride_stop};
      stride_start = pair.second;
      prev_row = curr_row;
    }
    stride_stop = pair.second + 1;
  }
  S2_DCHECK_EQ(curr_row, rows_ - 1);
  strides_[rows_ - 1] = {stride_start, stride_stop};
  S2_DCHECK(this->IsValid()) << "Constructor validity check fail.";
}

Window Window::Upsample(const int new_rows, const int new_cols) const {
  S2_DCHECK(new_rows >= rows_) << "Upsampling: New_rows < current_rows";
  S2_DCHECK(new_cols >= cols_) << "Upsampling: New_cols < current_cols";
  const double row_scale = static_cast<double>(new_rows) / rows_;
  const double col_scale = static_cast<double>(new_cols) / cols_;
  std::vector<ColumnStride> new_strides(new_rows);
  ColumnStride from_stride;
  for (int row = 0; row < new_rows; ++row) {
    from_stride = strides_[static_cast<int>((row + 0.5) / row_scale)];
    new_strides[row] = {static_cast<int>(col_scale * from_stride.start + 0.5),
                        static_cast<int>(col_scale * from_stride.end + 0.5)};
  }
  return Window(new_strides);
}

// This code takes advantage of the fact that the dilation window is square to
// ensure that we can compute the stride for each output row in constant time.
// TODO (mrdmnd): a potential optimization might be to combine this method and
// the Upsample method into a single "Expand" method. For the sake of
// testing, I haven't done that here, but I think it would be fairly
// straightforward to do so. This method generally isn't very expensive so it
// feels unnecessary to combine them.
Window Window::Dilate(const int radius) const {
  S2_DCHECK(radius >= 0) << "Negative dilation radius.";
  std::vector<ColumnStride> new_strides(rows_);
  int prev_row, next_row;
  for (int row = 0; row < rows_; ++row) {
    prev_row = std::max(0, row - radius);
    next_row = std::min(row + radius, rows_ - 1);
    new_strides[row] = {std::max(0, strides_[prev_row].start - radius),
                        std::min(strides_[next_row].end + radius, cols_)};
  }
  return Window(new_strides);
}

// Debug string implemented primarily for testing purposes.
string Window::DebugString() const {
  std::stringstream buffer;
  for (int row = 0; row < rows_; ++row) {
    for (int col = 0; col < cols_; ++col) {
      buffer << (strides_[row].InRange(col) ? " *" : " .");
    }
    buffer << std::endl;
  }
  return buffer.str();
}

// Valid Windows require the following structural conditions to hold:
// 1) All rows must consist of a single contiguous stride of `true` values.
// 2) All strides are greater than zero length (i.e. no empty rows).
// 3) The index of the first `true` column in a row must be at least as
//    large as the index of the first `true` column in the previous row.
// 4) The index of the last `true` column in a row must be at least as large
//    as the index of the last `true` column in the previous row.
// 5) strides[0].start = 0 (the first cell is always filled).
// 6) strides[n_rows-1].end = n_cols (the last cell is filled).
bool Window::IsValid() const {
  if (rows_ <= 0 || cols_ <= 0 || strides_.front().start != 0 ||
      strides_.back().end != cols_) {
    return false;
  }

  ColumnStride prev = {-1, -1};
  for (const auto& curr : strides_) {
    if (curr.end <= curr.start || curr.start < prev.start ||
        curr.end < prev.end) {
      return false;
    }
    prev = curr;
  }
  return true;
}

inline double BoundsCheckedTableCost(const int row, const int col,
                                     const ColumnStride& stride,
                                     const CostTable& table) {
  if (row < 0 && col < 0) {
    return 0.0;
  } else if (row < 0 || col < 0 || !stride.InRange(col)) {
    return DOUBLE_MAX;
  } else {
    return table[row][col];
  }
}

// Perform dynamic timewarping by filling in the DP table on cells that are
// inside our search window. For an exact (all-squares) evaluation, this
// incurs bounds checking overhead - we don't need to ensure that we're inside
// the appropriate cells in the window, because it's guaranteed. Structuring
// the program to reuse code for both the EXACT and WINDOWED cases by
// abstracting EXACT as a window with full-covering strides is done for
// maintainability reasons. One potential optimization here might be to overload
// this function to skip bounds checking when the window is full.
//
// As a note of general interest, the Dynamic Timewarp algorithm as stated here
// prefers shorter warp paths, when two warp paths might be equally costly. This
// is because it favors progressing in the sequences simultaneously due to the
// equal weighting of a diagonal step in the cost table with a horizontal or
// vertical step. This may be counterintuitive, but represents the standard
// implementation of this algorithm. TODO(user) - future implementations could
// allow weights on the lookup costs to mitigate this.
//
// This is the hottest routine in the whole package, please be careful to
// profile any future changes made here.
//
// This method takes time proportional to the number of cells in the window,
// which can range from O(max(a, b)) cells (best) to O(a*b) cells (worst)
VertexAlignment DynamicTimewarp(const S2Polyline& a, const S2Polyline& b,
                                const Window& w) {
  const int rows = a.num_vertices();
  const int cols = b.num_vertices();
  auto costs = CostTable(rows, std::vector<double>(cols));

  ColumnStride curr;
  ColumnStride prev = ColumnStride::All();
  for (int row = 0; row < rows; ++row) {
    curr = w.GetColumnStride(row);
    for (int col = curr.start; col < curr.end; ++col) {
      double d_cost = BoundsCheckedTableCost(row - 1, col - 1, prev, costs);
      double u_cost = BoundsCheckedTableCost(row - 1, col - 0, prev, costs);
      double l_cost = BoundsCheckedTableCost(row - 0, col - 1, curr, costs);
      costs[row][col] = std::min({d_cost, u_cost, l_cost}) +
                        (a.vertex(row) - b.vertex(col)).Norm2();
    }
    prev = curr;
  }

  // Now we walk back through the cost table and build up the warp path.
  // Somewhat surprisingly, it is faster to recover the path this way than it
  // is to save the comparisons from the computation we *already did* to get the
  // direction we came from. The author speculates that this behavior is
  // assignment-cost-related: to persist direction, we have to do extra
  // stores/loads of "directional" information, and the extra assignment cost
  // this incurs is larger than the cost to simply redo the comparisons.
  // It's probably worth revisiting this assumption in the future.
  // As it turns out, the following code ends up effectively free.
  WarpPath warp_path;
  warp_path.reserve(std::max(a.num_vertices(), b.num_vertices()));
  int row = a.num_vertices() - 1;
  int col = b.num_vertices() - 1;
  curr = w.GetCheckedColumnStride(row);
  prev = w.GetCheckedColumnStride(row - 1);
  while (row >= 0 && col >= 0) {
    warp_path.push_back({row, col});
    double d_cost = BoundsCheckedTableCost(row - 1, col - 1, prev, costs);
    double u_cost = BoundsCheckedTableCost(row - 1, col - 0, prev, costs);
    double l_cost = BoundsCheckedTableCost(row - 0, col - 1, curr, costs);
    if (d_cost <= u_cost && d_cost <= l_cost) {
      row -= 1;
      col -= 1;
      curr = w.GetCheckedColumnStride(row);
      prev = w.GetCheckedColumnStride(row - 1);
    } else if (u_cost <= l_cost) {
      row -= 1;
      curr = w.GetCheckedColumnStride(row);
      prev = w.GetCheckedColumnStride(row - 1);
    } else {
      col -= 1;
    }
  }
  std::reverse(warp_path.begin(), warp_path.end());
  return VertexAlignment(costs.back().back(), warp_path);
}

std::unique_ptr<S2Polyline> HalfResolution(const S2Polyline& in) {
  const int n = in.num_vertices();
  std::vector<S2Point> vertices;
  vertices.reserve(n / 2);
  for (int i = 0; i < n; i += 2) {
    vertices.push_back(in.vertex(i));
  }
  return absl::make_unique<S2Polyline>(vertices);
}

// Helper methods for GetMedoidPolyline and GetConsensusPolyline to auto-select
// appropriate cost function / alignment functions.
double CostFn(const S2Polyline& a, const S2Polyline& b, bool approx) {
  return approx ? GetApproxVertexAlignment(a, b).alignment_cost
                : GetExactVertexAlignmentCost(a, b);
}

VertexAlignment AlignmentFn(const S2Polyline& a, const S2Polyline& b,
                            bool approx) {
  return approx ? GetApproxVertexAlignment(a, b)
                : GetExactVertexAlignment(a, b);
}

// PUBLIC API IMPLEMENTATION DETAILS

// This is the constant-space implementation of Dynamic Timewarp that can
// compute the alignment cost, but not the warp path.
double GetExactVertexAlignmentCost(const S2Polyline& a, const S2Polyline& b) {
  const int a_n = a.num_vertices();
  const int b_n = b.num_vertices();
  S2_CHECK(a_n > 0) << "A is empty polyline.";
  S2_CHECK(b_n > 0) << "B is empty polyline.";
  std::vector<double> cost(b_n, DOUBLE_MAX);
  double left_diag_min_cost = 0;
  for (int row = 0; row < a_n; ++row) {
    for (int col = 0; col < b_n; ++col) {
      double up_cost = cost[col];
      cost[col] = std::min(left_diag_min_cost, up_cost) +
                  (a.vertex(row) - b.vertex(col)).Norm2();
      left_diag_min_cost = std::min(cost[col], up_cost);
    }
    left_diag_min_cost = DOUBLE_MAX;
  }
  return cost.back();
}

VertexAlignment GetExactVertexAlignment(const S2Polyline& a,
                                        const S2Polyline& b) {
  const int a_n = a.num_vertices();
  const int b_n = b.num_vertices();
  S2_CHECK(a_n > 0) << "A is empty polyline.";
  S2_CHECK(b_n > 0) << "B is empty polyline.";
  const auto w = Window(std::vector<ColumnStride>(a_n, {0, b_n}));
  return DynamicTimewarp(a, b, w);
}

VertexAlignment GetApproxVertexAlignment(const S2Polyline& a,
                                         const S2Polyline& b,
                                         const int radius) {
  // Determined experimentally, through benchmarking, as about the points at
  // which ExactAlignment is faster than ApproxAlignment, so we use these as
  // our switchover points to exact computation mode.
  const int kSizeSwitchover = 32;
  const double kDensitySwitchover = 0.85;
  const int a_n = a.num_vertices();
  const int b_n = b.num_vertices();
  S2_CHECK(a_n > 0) << "A is empty polyline.";
  S2_CHECK(b_n > 0) << "B is empty polyline.";
  S2_CHECK(radius >= 0) << "Radius is negative.";

  // If we've hit the point where doing a full, direct solve is guaranteed to
  // be faster, then terminate the recursion and do that.
  if (a_n - radius < kSizeSwitchover || b_n - radius < kSizeSwitchover) {
    return GetExactVertexAlignment(a, b);
  }

  // If we've hit the point where the window will be probably be so full that we
  // might as well compute an exact solution, then terminate recursion to do so.
  if (std::max(a_n, b_n) * (2 * radius + 1) > a_n * b_n * kDensitySwitchover) {
    return GetExactVertexAlignment(a, b);
  }

  // Otherwise, shrink the input polylines, recursively compute the vertex
  // alignment using this method, and then compute the final alignment using
  // the projected alignment `proj` on an upsampled, dilated window.
  const auto a_half = HalfResolution(a);
  const auto b_half = HalfResolution(b);
  const auto proj = GetApproxVertexAlignment(*a_half, *b_half, radius);
  const auto w = Window(proj.warp_path).Upsample(a_n, b_n).Dilate(radius);
  return DynamicTimewarp(a, b, w);
}

// This method calls the approx method with a reasonable default for radius.
VertexAlignment GetApproxVertexAlignment(const S2Polyline& a,
                                         const S2Polyline& b) {
  const int max_length = std::max(a.num_vertices(), b.num_vertices());
  const int radius = static_cast<int>(std::pow(max_length, 0.25));
  return GetApproxVertexAlignment(a, b, radius);
}

// We use some of the symmetry of our metric to avoid computing all N^2
// alignments. Specifically, because cost_fn(a, b) = cost_fn(b, a), and
// cost_fn(a, a) = 0, we can compute only the lower triangle of cost matrix
// and then mirror it across the diagonal to save on cost_fn invocations.
int GetMedoidPolyline(const std::vector<std::unique_ptr<S2Polyline>>& polylines,
                      const MedoidOptions options) {
  const int num_polylines = polylines.size();
  const bool approx = options.approx();
  S2_CHECK_GT(num_polylines, 0);

  // costs[i] stores total cost of aligning [i] with all other polylines.
  std::vector<double> costs(num_polylines, 0.0);
  for (int i = 0; i < num_polylines; ++i) {
    for (int j = i + 1; j < num_polylines; ++j) {
      double cost = CostFn(*polylines[i], *polylines[j], approx);
      costs[i] += cost;
      costs[j] += cost;
    }
  }
  return std::min_element(costs.begin(), costs.end()) - costs.begin();
}

// Implements Iterative Dynamic Timewarp Barycenter Averaging algorithm from
//
// https://pdfs.semanticscholar.org/a596/8ca9488199291ffe5473643142862293d69d.pdf
//
// Algorithm:
// Initialize consensus sequence with either the medoid or an arbitrary
// element (chosen here to be the first element in the input collection).
// While the consensus polyline `consensus` hasn't converged and we haven't
// exceeded our iteration cap:
//   For each polyline `p` in the input,
//     Compute vertex alignment from the current consensus to `p`.
//     For each (c_index, p_index) pair in the warp path,
//       Add the S2Point pts->vertex(p_index) to S2Point consensus[c_index]
//   Normalize (compute centroid) of each consensus point.
//   Determine if consensus is converging; if no vertex has moved or we've hit
//   the iteration cap, halt.
//
//  This algorithm takes O(iteration_cap * num_polylines) pairwise alignments.

std::unique_ptr<S2Polyline> GetConsensusPolyline(
    const std::vector<std::unique_ptr<S2Polyline>>& polylines,
    const ConsensusOptions options) {
  const int num_polylines = polylines.size();
  S2_CHECK_GT(num_polylines, 0);
  const bool approx = options.approx();

  // Seed a consensus polyline, either arbitrarily with first element, or with
  // the medoid. If seeding with medoid, inherit approx parameter from options.
  int seed_index = 0;
  if (options.seed_medoid()) {
    MedoidOptions medoid_options;
    medoid_options.set_approx(approx);
    seed_index = GetMedoidPolyline(polylines, medoid_options);
  }
  auto consensus = std::unique_ptr<S2Polyline>(polylines[seed_index]->Clone());
  const int num_consensus_vertices = consensus->num_vertices();
  S2_DCHECK_GT(num_consensus_vertices, 1);

  bool converged = false;
  int iterations = 0;
  while (!converged && iterations < options.iteration_cap()) {
    std::vector<S2Point> points(num_consensus_vertices, S2Point());
    for (const auto& polyline : polylines) {
      const auto alignment = AlignmentFn(*consensus, *polyline, approx);
      for (const auto& pair : alignment.warp_path) {
        points[pair.first] += polyline->vertex(pair.second);
      }
    }
    for (S2Point& p : points) {
      p = p.Normalize();
    }

    ++iterations;
    auto new_consensus = absl::make_unique<S2Polyline>(points);
    converged = new_consensus->ApproxEquals(*consensus);
    consensus = std::move(new_consensus);
  }
  return consensus;
}
}  // namespace s2polyline_alignment
