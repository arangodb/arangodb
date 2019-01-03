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


#ifndef S2_S2POLYLINE_ALIGNMENT_INTERNAL_H_
#define S2_S2POLYLINE_ALIGNMENT_INTERNAL_H_

#include "s2/s2polyline_alignment.h"

#include <limits>
#include <vector>

#include "s2/s2polyline.h"

namespace s2polyline_alignment {

static constexpr double DOUBLE_MAX = std::numeric_limits<double>::max();

// Alias for a 2d Dynamic Programming table.
typedef std::vector<std::vector<double>> CostTable;

// A ColumnStride is a [start, end) range of columns in a search window.
// It enables us to lazily fill up our CostTable structures by providing bounds
// checked access for reads. We also use them to keep track of structured,
// sparse Window matrices by tracking start and end columns for each row.
struct ColumnStride {
  int start;
  int end;
  inline bool InRange(const int index) const {
    return start <= index && index < end;
  }
  // Returns a ColumnStride where InRange evaluates to `true` for all
  // non-negative inputs less than INT_MAX;
  static inline constexpr ColumnStride All() {
    return ColumnStride{-1, std::numeric_limits<int>::max()};
  }
};

// A Window is a sparse binary matrix with specific structural constraints
// on allowed element configurations. It is used in this library to represent
// "search windows" for windowed dynamic timewarping.
//
// Valid Windows require the following structural conditions to hold:
// 1) All rows must consist of a single contiguous stride of `true` values.
// 2) All strides are greater than zero length (i.e. no empty rows).
// 3) The index of the first `true` column in a row must be at least as
//    large as the index of the first `true` column in the previous row.
// 4) The index of the last `true` column in a row must be at least as large
//    as the index of the last `true` column in the previous row.
// 5) strides[0].start = 0 (the first cell is always filled).
// 6) strides[n_rows-1].end = n_cols (the last cell is filled).
//
// Example valid strided_masks (* = filled, . = unfilled)
//   0 1 2 3 4 5
// 0 * * * . . .
// 1 . * * * . .
// 2 . * * * . .
// 3 . . * * * *
// 4 . . * * * *
//   0 1 2 3 4 5
// 0 * * * * . .
// 1 . * * * * .
// 2 . . * * * .
// 3 . . . . * *
// 4 . . . . . *
//   0 1 2 3 4 5
// 0 * * . . . .
// 1 . * . . . .
// 2 . . * * * .
// 3 . . . . . *
// 4 . . . . . *
//
// Example invalid strided_masks:
//   0 1 2 3 4 5
// 0 * * * . * * <-- more than one continuous run
// 1 . * * * . .
// 2 . * * * . .
// 3 . . * * * *
// 4 . . * * * *
//   0 1 2 3 4 5
// 0 * * * . . .
// 1 . * * * . .
// 2 . * * * . .
// 3 * * * * * * <-- start index not monotonically increasing
// 4 . . * * * *
//   0 1 2 3 4 5
// 0 * * * . . .
// 1 . * * * * .
// 2 . * * * . . <-- end index not monotonically increasing
// 3 . . * * * *
// 4 . . * * * *
//   0 1 2 3 4 5
// 0 . * . . . . <-- does not fill upper left corner
// 1 . * . . . .
// 2 . * . . . .
// 3 . * * * . .
// 4 . . * * * *
class Window {
 public:
  // Construct a Window from a non-empty list of column strides.
  explicit Window(const std::vector<ColumnStride>& strides);

  // Construct a Window from a non-empty sequence of warp path index pairs.
  explicit Window(const WarpPath& warp_path);

  // Return the (not-bounds-checked) stride for this row.
  inline ColumnStride GetColumnStride(const int row) const {
    return strides_[row];
  }

  // Return the (bounds-checked) stride for this row.
  // If row < 0, returns ColumnStride::All()
  inline ColumnStride GetCheckedColumnStride(const int row) const {
    return (row > 0) ? strides_[row] : ColumnStride::All();
  }

  // Return a new, larger Window that is an upscaled version of this window
  // Used by ApproximateAlignment window expansion step.
  Window Upsample(const int new_rows, const int new_cols) const;

  // Return a new, equal-size Window by dilating this window with a square
  // structuring element with half-length `radius`. Radius = 1 corresponds to
  // a 3x3 square morphological dilation.
  // Used by ApproximateAlignment window expansion step.
  Window Dilate(const int radius) const;

  // Return a string representation of this window.
  string DebugString() const;

 private:
  int rows_;
  int cols_;
  std::vector<ColumnStride> strides_;
  // Returns true if this window's data represents a valid window.
  bool IsValid() const;
};

// Reduce the number of vertices of polyline `in` by selecting every other
// vertex for inclusion in a new polyline. Specifically, we take even-index
// vertices [0, 2, 4,...]. For an even-length polyline, the last vertex is not
// selected. For an odd-length polyline, the last vertex is selected.
// Constructs and returns a new S2Polyline in linear time.
std::unique_ptr<S2Polyline> HalfResolution(const S2Polyline& in);

}  // namespace s2polyline_alignment
#endif  // S2_S2POLYLINE_ALIGNMENT_INTERNAL_H_
