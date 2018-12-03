// Copyright 2005 Google Inc. All Rights Reserved.
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

#ifndef S2_S2CELL_UNION_H_
#define S2_S2CELL_UNION_H_

#include <vector>

#include "s2/base/commandlineflags.h"
#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/s2cell_id.h"
#include "s2/s2region.h"
#include "s2/third_party/absl/base/macros.h"

class Decoder;
class Encoder;
class S1Angle;
class S2Cap;
class S2Cell;
class S2LatLngRect;

DECLARE_int32(s2cell_union_decode_max_num_cells);

// An S2CellUnion is a region consisting of cells of various sizes.  Typically
// a cell union is used to approximate some other shape.  There is a tradeoff
// between the accuracy of the approximation and how many cells are used.
// Unlike polygons, cells have a fixed hierarchical structure.  This makes
// them more suitable for optimizations based on preprocessing.
//
// An S2CellUnion is represented as a vector of sorted, non-overlapping
// S2CellIds.  By default the vector is also "normalized", meaning that groups
// of 4 child cells have been replaced by their parent cell whenever possible.
// S2CellUnions are not required to be normalized, but certain operations will
// return different results if they are not (e.g., Contains(S2CellUnion).)
//
// S2CellUnion is movable and copyable.
class S2CellUnion final : public S2Region {
 public:
  // Creates an empty cell union.
  S2CellUnion() {}

  // Constructs a cell union with the given S2CellIds, then calls Normalize()
  // to sort them, remove duplicates, and merge cells when possible.  (See
  // FromNormalized if your vector is already normalized.)
  //
  // The argument is passed by value, so if you are passing a named variable
  // and have no further use for it, consider using std::move().
  //
  // A cell union containing a single S2CellId may be constructed like this:
  //
  //     S2CellUnion example({cell_id});
  explicit S2CellUnion(std::vector<S2CellId> cell_ids);

  // Convenience constructor that accepts a vector of uint64.  Note that
  // unlike the constructor above, this one makes a copy of "cell_ids".
  explicit S2CellUnion(const std::vector<uint64>& cell_ids);

  // Constructs a cell union from S2CellIds that have already been normalized
  // (typically because they were extracted from another S2CellUnion).
  //
  // The argument is passed by value, so if you are passing a named variable
  // and have no further use for it, consider using std::move().
  //
  // REQUIRES: "cell_ids" satisfies the requirements of IsNormalized().
  static S2CellUnion FromNormalized(std::vector<S2CellId> cell_ids);

  // Constructs a cell union from a vector of sorted, non-overlapping
  // S2CellIds.  Unlike the other constructors, FromVerbatim does not require
  // that groups of 4 child cells have been replaced by their parent cell.  In
  // other words, "cell_ids" must satisfy the requirements of IsValid() but
  // not necessarily IsNormalized().
  //
  // Note that if the cell union is not normalized, certain operations may
  // return different results (e.g., Contains(S2CellUnion)).
  //
  // REQUIRES: "cell_ids" satisfies the requirements of IsValid().
  static S2CellUnion FromVerbatim(std::vector<S2CellId> cell_ids);

  // Constructs a cell union that corresponds to a continuous range of cell
  // ids.  The output is a normalized collection of cell ids that covers the
  // leaf cells between "min_id" and "max_id" inclusive.
  //
  // REQUIRES: min_id.is_leaf(), max_id.is_leaf(), min_id <= max_id.
  static S2CellUnion FromMinMax(S2CellId min_id, S2CellId max_id);

  // Like FromMinMax() except that the union covers the range of leaf cells
  // from "begin" (inclusive) to "end" (exclusive), as with Python ranges or
  // STL iterator ranges.  If (begin == end) the result is empty.
  //
  // REQUIRES: begin.is_leaf(), end.is_leaf(), begin <= end.
  static S2CellUnion FromBeginEnd(S2CellId begin, S2CellId end);

  // Init() methods corresponding to the constructors/factory methods above.
  // TODO(ericv): Consider deprecating these methods in favor of using the
  // constructors and move assignment operator.
  void Init(std::vector<S2CellId> cell_ids);
  void Init(const std::vector<uint64>& cell_ids);
  void InitFromMinMax(S2CellId min_id, S2CellId max_id);
  void InitFromBeginEnd(S2CellId begin, S2CellId end);

  // Clears the contents of the cell union and minimizes memory usage.
  void Clear();

  // Gives ownership of the vector data to the client without copying, and
  // clears the content of the cell union.  The original data in cell_ids
  // is lost if there was any.
  std::vector<S2CellId> Release();

  // Convenience methods for accessing the individual cell ids.
  int num_cells() const { return static_cast<int>(cell_ids_.size()); }
  S2CellId cell_id(int i) const { return cell_ids_[i]; }

  // Vector-like methods for accessing the individual cell ids.
  size_t size() const { return cell_ids_.size(); }
  bool empty() const { return cell_ids_.empty(); }
  S2CellId operator[](int i) const { return cell_ids_[i]; }

  // Standard begin/end methods, to allow range-based for loops:
  //
  //  for (S2CellId id : cell_union) { ... }
  std::vector<S2CellId>::const_iterator begin() const;
  std::vector<S2CellId>::const_iterator end() const;

  // Direct access to the underlying vector for STL algorithms.
  const std::vector<S2CellId>& cell_ids() const { return cell_ids_; }

  // Returns true if the cell union is valid, meaning that the S2CellIds are
  // valid, non-overlapping, and sorted in increasing order.
  bool IsValid() const;

  // Returns true if the cell union is normalized, meaning that it is
  // satisfies IsValid() and that no four cells have a common parent.
  // Certain operations such as Contains(S2CellUnion) will return a different
  // result if the cell union is not normalized.
  bool IsNormalized() const;

  // Normalizes the cell union by discarding cells that are contained by other
  // cells, replacing groups of 4 child cells by their parent cell whenever
  // possible, and sorting all the cell ids in increasing order.
  //
  // Returns true if the number of cells was reduced.
  // TODO(ericv): Change this method to return void.
  bool Normalize();

  // Replaces "output" with an expanded version of the cell union where any
  // cells whose level is less than "min_level" or where (level - min_level)
  // is not a multiple of "level_mod" are replaced by their children, until
  // either both of these conditions are satisfied or the maximum level is
  // reached.
  //
  // This method allows a covering generated by S2RegionCoverer using
  // min_level() or level_mod() constraints to be stored as a normalized cell
  // union (which allows various geometric computations to be done) and then
  // converted back to the original list of cell ids that satisfies the
  // desired constraints.
  void Denormalize(int min_level, int level_mod,
                   std::vector<S2CellId>* output) const;

  // If there are more than "excess" elements of the cell_ids() vector that
  // are allocated but unused, reallocates the array to eliminate the excess
  // space.  This reduces memory usage when many cell unions need to be held
  // in memory at once.
  void Pack(int excess = 0);

  // Returns true if the cell union contains the given cell id.  Containment
  // is defined with respect to regions, e.g. a cell contains its 4 children.
  // This is a fast operation (logarithmic in the size of the cell union).
  //
  // CAVEAT: If you have constructed a non-normalized S2CellUnion using
  // FromVerbatim, note that groups of 4 child cells are *not* considered to
  // contain their parent cell.  To get this behavior you must use one of the
  // other constructors or call Normalize() explicitly.
  bool Contains(S2CellId id) const;

  // Returns true if the cell union intersects the given cell id.
  // This is a fast operation (logarithmic in the size of the cell union).
  bool Intersects(S2CellId id) const;

  // Returns true if this cell union contains the given other cell union.
  //
  // CAVEAT: If you have constructed a non-normalized S2CellUnion using
  // FromVerbatim, note that groups of 4 child cells are *not* considered to
  // contain their parent cell.  To get this behavior you must use one of the
  // other constructors or call Normalize() explicitly.
  bool Contains(const S2CellUnion& y) const;

  // Returns true if this cell union intersects the given other cell union.
  bool Intersects(const S2CellUnion& y) const;

  // Returns the union of the two given cell unions.
  S2CellUnion Union(const S2CellUnion& y) const;

  // Returns the intersection of the two given cell unions.
  S2CellUnion Intersection(const S2CellUnion& y) const;

  // Specialized version of GetIntersection() that returns the intersection of
  // a cell union with an S2CellId.  This can be useful for splitting a cell
  // union into pieces.
  S2CellUnion Intersection(S2CellId id) const;

  // Returns the difference of the two given cell unions.
  S2CellUnion Difference(const S2CellUnion& y) const;

  // Expands the cell union by adding a buffer of cells at "expand_level"
  // around the union boundary.
  //
  // For each cell "c" in the union, we add all neighboring cells at level
  // "expand_level" that are adjacent to "c".  Note that there can be many
  // such cells if "c" is large compared to "expand_level".  If "c" is smaller
  // than "expand_level", we first add the parent of "c" at "expand_level" and
  // then add all the neighbors of that cell.
  //
  // Note that the size of the output is exponential in "expand_level".  For
  // example, if expand_level == 20 and the input has a cell at level 10,
  // there will be on the order of 4000 adjacent cells in the output.  For
  // most applications the Expand(min_radius, max_level_diff) method below is
  // easier to use.
  void Expand(int expand_level);

  // Expands the cell union such that it contains all points whose distance to
  // the cell union is at most "min_radius", but do not use cells that are
  // more than "max_level_diff" levels higher than the largest cell in the
  // input.  The second parameter controls the tradeoff between accuracy and
  // output size when a large region is being expanded by a small amount
  // (e.g. expanding Canada by 1km).  For example, if max_level_diff == 4 the
  // region will always be expanded by approximately 1/16 the width of its
  // largest cell.  Note that in the worst case, the number of cells in the
  // output can be up to 4 * (1 + 2 ** max_level_diff) times larger than the
  // number of cells in the input.
  void Expand(S1Angle min_radius, int max_level_diff);

  // The number of leaf cells covered by the union.
  // This will be no more than 6*2^60 for the whole sphere.
  uint64 LeafCellsCovered() const;

  // Approximates this cell union's area in steradians by summing the average
  // area of each contained cell's average area, using the AverageArea method
  // from the S2Cell class.  This is equivalent to the number of leaves covered,
  // multiplied by the average area of a leaf.  Note that AverageArea does not
  // take into account distortion of cell, and thus may be off by up to a
  // factor of up to 1.7.
  //
  // NOTE: Since this is proportional to LeafCellsCovered(), it is
  // always better to use that function if all you care about is
  // the relative average area between objects.
  double AverageBasedArea() const;

  // Calculates this cell union's area in steradians by summing the approximate
  // area for each contained cell, using the ApproxArea method from the S2Cell
  // class.
  double ApproxArea() const;

  // Calculates this cell union's area in steradians by summing the exact area
  // for each contained cell, using the Exact method from the S2Cell class.
  double ExactArea() const;

  // Return true if two cell unions are identical.
  friend bool operator==(const S2CellUnion& x, const S2CellUnion& y);

  // Return true if two cell unions are different.
  friend bool operator!=(const S2CellUnion& x, const S2CellUnion& y);

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2CellUnion* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;

  // This is a fast operation (logarithmic in the size of the cell union).
  bool Contains(const S2Cell& cell) const override;

  // This is a fast operation (logarithmic in the size of the cell union).
  bool MayIntersect(const S2Cell& cell) const override;

  // The point 'p' does not need to be normalized.
  // This is a fast operation (logarithmic in the size of the cell union).
  bool Contains(const S2Point& p) const override;

  // Appends a serialized representation of the S2CellUnion to "encoder".
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* const encoder) const;

  // Decodes an S2CellUnion encoded with Encode().  Returns true on success.
  bool Decode(Decoder* const decoder);

  ////////////////////////////////////////////////////////////////////////
  // Static methods intended for high-performance clients that prefer to
  // manage their own storage.

  // Like Normalize(), but works with a vector of S2CellIds.
  // Equivalent to:
  //   *cell_ids = S2CellUnion(std::move(*cell_ids)).Release();
  static bool Normalize(std::vector<S2CellId>* cell_ids);

  // Like Denormalize(), but works with a vector of S2CellIds.
  // REQUIRES: out != &in
  static void Denormalize(const std::vector<S2CellId>& in,
                          int min_level, int level_mod,
                          std::vector<S2CellId>* out);

  // Like GetIntersection(), but works directly with vectors of S2CellIds,
  // Equivalent to:
  //
  //    *out = S2CellUnion(x).Intersection(S2CellUnion(y)).Release()
  //
  // except that this method has slightly more relaxed normalization
  // requirements: the input vectors may contain groups of 4 child cells that
  // all have the same parent.  (In a normalized S2CellUnion, such groups are
  // always replaced by the parent cell.)
  static void GetIntersection(const std::vector<S2CellId>& x,
                              const std::vector<S2CellId>& y,
                              std::vector<S2CellId>* out);

 private:
  friend class S2CellUnionTestPeer;  // For creating invalid S2CellUnions.

  // Internal constructor that does not check "cell_ids" for validity.
  enum VerbatimFlag { VERBATIM };
  S2CellUnion(std::vector<S2CellId> cell_ids, VerbatimFlag verbatim)
      : cell_ids_(std::move(cell_ids)) {}

  // Converts a vector of uint64 to a vector of S2CellIds.
  static std::vector<S2CellId> ToS2CellIds(const std::vector<uint64>& ids);

  std::vector<S2CellId> cell_ids_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2CellUnion::S2CellUnion(std::vector<S2CellId> cell_ids)
    : cell_ids_(std::move(cell_ids)) {
  Normalize();
}

inline S2CellUnion S2CellUnion::FromNormalized(std::vector<S2CellId> cell_ids) {
  S2CellUnion result(std::move(cell_ids), VERBATIM);
  S2_DCHECK(result.IsNormalized());
  return result;
}

inline S2CellUnion S2CellUnion::FromVerbatim(std::vector<S2CellId> cell_ids) {
  S2CellUnion result(std::move(cell_ids), VERBATIM);
  S2_DCHECK(result.IsValid());
  return result;
}

inline void S2CellUnion::Init(std::vector<S2CellId> cell_ids) {
  cell_ids_ = std::move(cell_ids);
  Normalize();
}

inline void S2CellUnion::Clear() {
  // swap() guarantees to reduce the RHS vector's size and capacity
  // to zero (as opposed to clear(), shrink_to_fit() sequence).
  std::vector<S2CellId>().swap(cell_ids_);
}

inline std::vector<S2CellId> S2CellUnion::Release() {
  // vector's rvalue reference constructor does not necessarily leave
  // moved-from value in empty state, so swap instead.
  std::vector<S2CellId> cell_ids;
  cell_ids_.swap(cell_ids);
  return cell_ids;
}

inline std::vector<S2CellId>::const_iterator S2CellUnion::begin() const {
  return cell_ids_.begin();
}

inline std::vector<S2CellId>::const_iterator S2CellUnion::end() const {
  return cell_ids_.end();
}

#endif  // S2_S2CELL_UNION_H_
