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

#ifndef S2_S2REGION_COVERER_H_
#define S2_S2REGION_COVERER_H_

#include <queue>
#include <utility>
#include <vector>

#include "s2/third_party/absl/base/macros.h"
#include "s2/_fp_contract_off.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"

#ifdef _WIN32
#pragma warning(disable : 4200)
#endif

class S2Region;

// An S2RegionCoverer is a class that allows arbitrary regions to be
// approximated as unions of cells (S2CellUnion).  This is useful for
// implementing various sorts of search and precomputation operations.
//
// Typical usage:
//
// S2RegionCoverer::Options options;
// options.set_max_cells(5);
// S2RegionCoverer coverer(options);
// S2Cap cap(center, radius);
// S2CellUnion covering = coverer.GetCovering(cap);
//
// This yields a vector of at most 5 cells that is guaranteed to cover the
// given cap (a disc-shaped region on the sphere).
//
// The approximation algorithm is not optimal but does a pretty good job in
// practice.  The output does not always use the maximum number of cells
// allowed, both because this would not always yield a better approximation,
// and because max_cells() is a limit on how much work is done exploring the
// possible covering as well as a limit on the final output size.
//
// Because it is an approximation algorithm, one should not rely on the
// stability of the output.  In particular, the output of the covering algorithm
// may change across different versions of the library.
//
// One can also generate interior coverings, which are sets of cells which
// are entirely contained within a region.  Interior coverings can be
// empty, even for non-empty regions, if there are no cells that satisfy
// the provided constraints and are contained by the region.  Note that for
// performance reasons, it is wise to specify a max_level when computing
// interior coverings - otherwise for regions with small or zero area, the
// algorithm may spend a lot of time subdividing cells all the way to leaf
// level to try to find contained cells.
class S2RegionCoverer {
 public:
#ifndef SWIG
  class Options {
   public:
    // Sets the desired maximum number of cells in the approximation.  Note
    // the following:
    //
    //  - For any setting of max_cells(), up to 6 cells may be returned if
    //    that is the minimum number required (e.g. if the region intersects
    //    all six cube faces).  Even for very tiny regions, up to 3 cells may
    //    be returned if they happen to be located at the intersection of
    //    three cube faces.
    //
    //  - min_level() takes priority over max_cells(), i.e. cells below the
    //    given level will never be used even if this causes a large number of
    //    cells to be returned.
    //
    //  - If max_cells() is less than 4, the area of the covering may be
    //    arbitrarily large compared to the area of the original region even
    //    if the region is convex (e.g. an S2Cap or S2LatLngRect).
    //
    // Accuracy is measured by dividing the area of the covering by the area
    // of the original region.  The following table shows the median and worst
    // case values for this area ratio on a test case consisting of 100,000
    // spherical caps of random size (generated using s2region_coverer_test):
    //
    //   max_cells:        3      4     5     6     8    12    20   100   1000
    //   median ratio:  5.33   3.32  2.73  2.34  1.98  1.66  1.42  1.11   1.01
    //   worst case:  215518  14.41  9.72  5.26  3.91  2.75  1.92  1.20   1.02
    //
    // The default value of 8 gives a reasonable tradeoff between the number
    // of cells used and the accuracy of the approximation.
    //
    // DEFAULT: kDefaultMaxCells
    static constexpr int kDefaultMaxCells = 8;
    int max_cells() const { return max_cells_; }
    void set_max_cells(int max_cells);

    // Sets the minimum and maximum cell levels to be used.  The default is to
    // use all cell levels.
    //
    // To find the cell level corresponding to a given physical distance, use
    // the S2Cell metrics defined in s2metrics.h.  For example, to find the
    // cell level that corresponds to an average edge length of 10km, use:
    //
    //   int level =
    //       S2::kAvgEdge.GetClosestLevel(S2Earth::KmToRadians(length_km));
    //
    // Note that min_level() takes priority over max_cells(), i.e. cells below
    // the given level will never be used even if this causes a large number
    // of cells to be returned.  (This doesn't apply to interior coverings,
    // since interior coverings make no completeness guarantees -- the result
    // is simply a set of cells that covers as much of the interior as
    // possible while satisfying the given restrictions.)
    //
    // REQUIRES: min_level() <= max_level()
    // DEFAULT: 0
    int min_level() const { return min_level_; }
    void set_min_level(int min_level);

    // REQUIRES: min_level() <= max_level()
    // DEFAULT: S2CellId::kMaxLevel
    int max_level() const { return max_level_; }
    void set_max_level(int max_level);

    // Convenience function that sets both the maximum and minimum cell levels.
    void set_fixed_level(int level);

    // If specified, then only cells where (level - min_level) is a multiple
    // of "level_mod" will be used (default 1).  This effectively allows the
    // branching factor of the S2CellId hierarchy to be increased.  Currently
    // the only parameter values allowed are 1, 2, or 3, corresponding to
    // branching factors of 4, 16, and 64 respectively.
    //
    // DEFAULT: 1
    int level_mod() const { return level_mod_; }
    void set_level_mod(int level_mod);

    // Convenience function that returns the maximum level such that
    //
    //   (level <= max_level()) && (level - min_level()) % level_mod() == 0.
    //
    // This is the maximum level that will actually be used in coverings.
    int true_max_level() const;

   protected:
    int max_cells_ = kDefaultMaxCells;
    int min_level_ = 0;
    int max_level_ = S2CellId::kMaxLevel;
    int level_mod_ = 1;
  };

  // Constructs an S2RegionCoverer with the given options.
  explicit S2RegionCoverer(const Options& options);

  // S2RegionCoverer is movable but not copyable.
  S2RegionCoverer(const S2RegionCoverer&) = delete;
  S2RegionCoverer& operator=(const S2RegionCoverer&) = delete;
  S2RegionCoverer(S2RegionCoverer&&);
  S2RegionCoverer& operator=(S2RegionCoverer&&);
#endif  // SWIG

  // Default constructor.  Options can be set using mutable_options().
  S2RegionCoverer();
  ~S2RegionCoverer();

  // Returns the current options.  Options can be modifed between calls.
  const Options& options() const { return options_; }
  Options* mutable_options() { return &options_; }

  // Returns an S2CellUnion that covers (GetCovering) or is contained within
  // (GetInteriorCovering) the given region and satisfies the current options.
  //
  // Note that if options().min_level() > 0 or options().level_mod() > 1, the
  // by definition the S2CellUnion may not be normalized, i.e. there may be
  // groups of four child cells that can be replaced by their parent cell.
  S2CellUnion GetCovering(const S2Region& region);
  S2CellUnion GetInteriorCovering(const S2Region& region);

  // Like the methods above, but works directly with a vector of S2CellIds.
  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void GetCovering(const S2Region& region, std::vector<S2CellId>* covering);
  void GetInteriorCovering(const S2Region& region,
                           std::vector<S2CellId>* interior);

  // Like GetCovering(), except that this method is much faster and the
  // coverings are not as tight.  All of the usual parameters are respected
  // (max_cells, min_level, max_level, and level_mod), except that the
  // implementation makes no attempt to take advantage of large values of
  // max_cells().  (A small number of cells will always be returned.)
  //
  // This function is useful as a starting point for algorithms that
  // recursively subdivide cells.
  void GetFastCovering(const S2Region& region, std::vector<S2CellId>* covering);

  // Given a connected region and a starting point on the boundary or inside the
  // region, returns a set of cells at the given level that cover the region.
  // The output cells are returned in arbitrary order.
  //
  // Note that this method is *not* faster than the regular GetCovering()
  // method for most region types, such as S2Cap or S2Polygon, and in fact it
  // can be much slower when the output consists of a large number of cells.
  // Currently it can be faster at generating coverings of long narrow regions
  // such as polylines, but this may change in the future, in which case this
  // method will most likely be removed.
  static void GetSimpleCovering(const S2Region& region, const S2Point& start,
                                int level, std::vector<S2CellId>* output);

  // Like GetSimpleCovering(), but accepts a starting S2CellId rather than a
  // starting point and cell level.  Returns all edge-connected cells at the
  // same level as "start" that intersect "region", in arbitrary order.
  static void FloodFill(const S2Region& region, S2CellId start,
                        std::vector<S2CellId>* output);

  // Returns true if the given S2CellId vector represents a valid covering
  // that conforms to the current covering parameters.  In particular:
  //
  //  - All S2CellIds must be valid.
  //
  //  - S2CellIds must be sorted and non-overlapping.
  //
  //  - S2CellId levels must satisfy min_level(), max_level(), and level_mod().
  //
  //  - If covering.size() > max_cells(), there must be no two cells with
  //    a common ancestor at min_level() or higher.
  //
  //  - There must be no sequence of cells that could be replaced by an
  //    ancestor (i.e. with level_mod() == 1, the 4 child cells of a parent).
  bool IsCanonical(const S2CellUnion& covering) const;
  bool IsCanonical(const std::vector<S2CellId>& covering) const;

  // Modify "covering" if necessary so that it conforms to the current
  // covering parameters (max_cells, min_level, max_level, and level_mod).
  // There are no restrictions on the input S2CellIds (they may be unsorted,
  // overlapping, etc).
  S2CellUnion CanonicalizeCovering(const S2CellUnion& covering);
  void CanonicalizeCovering(std::vector<S2CellId>* covering);

 private:
  struct Candidate {
    S2Cell cell;
    bool is_terminal;        // Cell should not be expanded further.
    int num_children;        // Number of children that intersect the region.
    Candidate* children[0];  // Actual size may be 0, 4, 16, or 64 elements.
  };

  // If the cell intersects the given region, return a new candidate with no
  // children, otherwise return nullptr.  Also marks the candidate as "terminal"
  // if it should not be expanded further.
  Candidate* NewCandidate(const S2Cell& cell);

  // Returns the log base 2 of the maximum number of children of a candidate.
  int max_children_shift() const { return 2 * options().level_mod(); }

  // Frees the memory associated with a candidate.
  static void DeleteCandidate(Candidate* candidate, bool delete_children);

  // Processes a candidate by either adding it to the result_ vector or
  // expanding its children and inserting it into the priority queue.
  // Passing an argument of nullptr does nothing.
  void AddCandidate(Candidate* candidate);

  // Populates the children of "candidate" by expanding the given number of
  // levels from the given cell.  Returns the number of children that were
  // marked "terminal".
  int ExpandChildren(Candidate* candidate, const S2Cell& cell, int num_levels);

  // Computes a set of initial candidates that cover the given region.
  void GetInitialCandidates();

  // Generates a covering and stores it in result_.
  void GetCoveringInternal(const S2Region& region);

  // If level > min_level(), then reduces "level" if necessary so that it also
  // satisfies level_mod().  Levels smaller than min_level() are not affected
  // (since cells at these levels are eventually expanded).
  int AdjustLevel(int level) const;

  // Ensures that all cells with level > min_level() also satisfy level_mod(),
  // by replacing them with an ancestor if necessary.  Cell levels smaller
  // than min_level() are not modified (see AdjustLevel).  The output is
  // then normalized to ensure that no redundant cells are present.
  void AdjustCellLevels(std::vector<S2CellId>* cells) const;

  // Returns true if "covering" contains all children of "id" at level
  // (id.level() + options_.level_mod()).
  bool ContainsAllChildren(const std::vector<S2CellId>& covering,
                           S2CellId id) const;

  // Replaces all descendants of "id" in "covering" with "id".
  // REQUIRES: "covering" contains at least one descendant of "id".
  void ReplaceCellsWithAncestor(std::vector<S2CellId>* covering,
                                S2CellId id) const;

  Options options_;

  // We save a temporary copy of the pointer passed to GetCovering() in order
  // to avoid passing this parameter around internally.  It is only used (and
  // only valid) for the duration of a single GetCovering() call.
  const S2Region* region_ = nullptr;

  // The set of S2CellIds that have been added to the covering so far.
  std::vector<S2CellId> result_;

  // We keep the candidates in a priority queue.  We specify a vector to hold
  // the queue entries since for some reason priority_queue<> uses a deque by
  // default.  We define our own own comparison function on QueueEntries in
  // order to make the results deterministic.  (Using the default
  // less<QueueEntry>, entries of equal priority would be sorted according to
  // the memory address of the candidate.)

  typedef std::pair<int, Candidate*> QueueEntry;
  struct CompareQueueEntries {
    bool operator()(const QueueEntry& x, const QueueEntry& y) const {
      return x.first < y.first;
    }
  };
  typedef std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                              CompareQueueEntries> CandidateQueue;
  CandidateQueue pq_;

  // True if we're computing an interior covering.
  bool interior_covering_;

  // Counter of number of candidates created, for performance evaluation.
  int candidates_created_counter_;
};

#endif  // S2_S2REGION_COVERER_H_
