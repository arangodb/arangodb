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
//
// Indexing Strategy
// -----------------
//
// Given a query region, we want to find all of the document regions that
// intersect it.  The first step is to represent all the regions as S2Cell
// coverings (see S2RegionCoverer).  We then split the problem into two parts,
// namely finding the document regions that are "smaller" than the query
// region and those that are "larger" than the query region.
//
// We do this by defining two terms for each S2CellId: a "covering term" and
// an "ancestor term".  (In the implementation below, covering terms are
// distinguished by prefixing a '$' to them.)  For each document region, we
// insert a covering term for every cell in the region's covering, and we
// insert an ancestor term for these cells *and* all of their ancestors.
//
// Then given a query region, we can look up all the document regions that
// intersect its covering by querying the union of the following terms:
//
// 1. An "ancestor term" for each cell in the query region.  These terms
//    ensure that we find all document regions that are "smaller" than the
//    query region, i.e. where the query region contains a cell that is either
//    a cell of a document region or one of its ancestors.
//
// 2. A "covering term" for every ancestor of the cells in the query region.
//    These terms ensure that we find all the document regions that are
//    "larger" than the query region, i.e. where document region contains a
//    cell that is a (proper) ancestor of a cell in the query region.
//
// Together, these terms find all of the document regions that intersect the
// query region.  Furthermore, the number of terms to be indexed and queried
// are both fairly small, and can be bounded in terms of max_cells() and the
// number of cell levels used.
//
// Optimizations
// -------------
//
// + Cells at the maximum level being indexed (max_level()) have the special
//   property that they will never be an ancestor of a cell in the query
//   region.  Therefore we can safely skip generating "covering terms" for
//   these cells (see query step 2 above).
//
// + If the index will contain only points (rather than general regions), then
//   we can skip all the covering terms mentioned above because there will
//   never be any document regions larger than the query region.  This can
//   significantly reduce the size of queries.
//
// + If it is more important to optimize index size rather than query speed,
//   the number of index terms can be reduced by creating ancestor terms only
//   for the *proper* ancestors of the cells in a document region, and
//   compensating for this by including covering terms for all cells in the
//   query region (in addition to their ancestors).
//
//   Effectively, when the query region and a document region contain exactly
//   the same cell, we have a choice about whether to treat this match as a
//   "covering term" or an "ancestor term".  One choice minimizes query size
//   while the other minimizes index size.

#include "s2/s2region_term_indexer.h"

#include <cctype>

#include "s2/base/logging.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell_id.h"
#include "s2/s2region.h"
#include "s2/third_party/absl/strings/str_cat.h"

using absl::string_view;
using std::vector;

S2RegionTermIndexer::Options::Options() {
  // Override the S2RegionCoverer defaults.
  set_max_cells(8);
  set_min_level(4);
  set_max_level(16);
  set_level_mod(1);
}

void S2RegionTermIndexer::Options::set_marker_character(char ch) {
  S2_DCHECK(!std::isalnum(ch));
  marker_ = string(1, ch);
}

S2RegionTermIndexer::S2RegionTermIndexer(const Options& options)
    : options_(options) {
}

// Defaulted in the implementation to prevent inline bloat.
S2RegionTermIndexer::S2RegionTermIndexer() = default;
S2RegionTermIndexer::~S2RegionTermIndexer() = default;
S2RegionTermIndexer::S2RegionTermIndexer(S2RegionTermIndexer&&) = default;
S2RegionTermIndexer& S2RegionTermIndexer::operator=(S2RegionTermIndexer&&) =
                                                   default;

string S2RegionTermIndexer::GetTerm(TermType term_type, const S2CellId& id,
                                    string_view prefix) const {
  // There are generally more ancestor terms than covering terms, so we add
  // the extra "marker" character to the covering terms to distinguish them.
  if (term_type == TermType::ANCESTOR) {
    return absl::StrCat(prefix, id.ToToken());
  } else {
    return absl::StrCat(prefix, options_.marker(), id.ToToken());
  }
}

vector<string> S2RegionTermIndexer::GetIndexTerms(const S2Point& point,
                                                  string_view prefix) {
  // See the top of this file for an overview of the indexing strategy.
  //
  // The last cell generated by this loop is effectively the covering for
  // the given point.  You might expect that this cell would be indexed as a
  // covering term, but as an optimization we always index these cells as
  // ancestor terms only.  This is possible because query regions will never
  // contain a descendant of such cells.  Note that this is true even when
  // max_level() != true_max_level() (see S2RegionCoverer::Options).

  const S2CellId id(point);
  vector<string> terms;
  for (int level = options_.min_level(); level <= options_.max_level();
       level += options_.level_mod()) {
    terms.push_back(GetTerm(TermType::ANCESTOR, id.parent(level), prefix));
  }
  return terms;
}

vector<string> S2RegionTermIndexer::GetIndexTerms(const S2Region& region,
                                                  string_view prefix) {
  // Note that options may have changed since the last call.
  *coverer_.mutable_options() = options_;
  S2CellUnion covering = coverer_.GetCovering(region);
  return GetIndexTermsForCanonicalCovering(covering, prefix);
}

vector<string> S2RegionTermIndexer::GetIndexTermsForCanonicalCovering(
    const S2CellUnion& covering, string_view prefix) {
  // See the top of this file for an overview of the indexing strategy.
  //
  // Cells in the covering are normally indexed as covering terms.  If we are
  // optimizing for query time rather than index space, they are also indexed
  // as ancestor terms (since this lets us reduce the number of terms in the
  // query).  Finally, as an optimization we always index true_max_level()
  // cells as ancestor cells only, since these cells have the special property
  // that query regions will never contain a descendant of these cells.

  S2_CHECK(!options_.index_contains_points_only());
  if (google::DEBUG_MODE) {
    *coverer_.mutable_options() = options_;
    S2_CHECK(coverer_.IsCanonical(covering));
  }
  vector<string> terms;
  S2CellId prev_id = S2CellId::None();
  int true_max_level = options_.true_max_level();
  for (S2CellId id : covering) {
    // IsCanonical() already checks the following conditions, but we repeat
    // them here for documentation purposes.
    int level = id.level();
    S2_DCHECK_GE(level, options_.min_level());
    S2_DCHECK_LE(level, options_.max_level());
    S2_DCHECK_EQ(0, (level - options_.min_level()) % options_.level_mod());

    if (level < true_max_level) {
      // Add a covering term for this cell.
      terms.push_back(GetTerm(TermType::COVERING, id, prefix));
    }
    if (level == true_max_level || !options_.optimize_for_space()) {
      // Add an ancestor term for this cell at the constrained level.
      terms.push_back(GetTerm(TermType::ANCESTOR, id.parent(level), prefix));
    }
    // Finally, add ancestor terms for all the ancestors of this cell.
    while ((level -= options_.level_mod()) >= options_.min_level()) {
      S2CellId ancestor_id = id.parent(level);
      if (prev_id != S2CellId::None() && prev_id.level() > level &&
          prev_id.parent(level) == ancestor_id) {
        break;  // We have already processed this cell and its ancestors.
      }
      terms.push_back(GetTerm(TermType::ANCESTOR, ancestor_id, prefix));
    }
    prev_id = id;
  }
  return terms;
}

vector<string> S2RegionTermIndexer::GetQueryTerms(const S2Point& point,
                                                  string_view prefix) {
  // See the top of this file for an overview of the indexing strategy.

  const S2CellId id(point);
  vector<string> terms;
  // Recall that all true_max_level() cells are indexed only as ancestor terms.
  int level = options_.true_max_level();
  terms.push_back(GetTerm(TermType::ANCESTOR, id.parent(level), prefix));
  if (options_.index_contains_points_only()) return terms;

  // Add covering terms for all the ancestor cells.
  for (; level >= options_.min_level(); level -= options_.level_mod()) {
    terms.push_back(GetTerm(TermType::COVERING, id.parent(level), prefix));
  }
  return terms;
}

vector<string> S2RegionTermIndexer::GetQueryTerms(const S2Region& region,
                                                  string_view prefix) {
  // Note that options may have changed since the last call.
  *coverer_.mutable_options() = options_;
  S2CellUnion covering = coverer_.GetCovering(region);
  return GetQueryTermsForCanonicalCovering(covering, prefix);
}

vector<string> S2RegionTermIndexer::GetQueryTermsForCanonicalCovering(
    const S2CellUnion& covering, string_view prefix) {
  // See the top of this file for an overview of the indexing strategy.

  if (google::DEBUG_MODE) {
    *coverer_.mutable_options() = options_;
    S2_CHECK(coverer_.IsCanonical(covering));
  }
  vector<string> terms;
  S2CellId prev_id = S2CellId::None();
  int true_max_level = options_.true_max_level();
  for (S2CellId id : covering) {
    // IsCanonical() already checks the following conditions, but we repeat
    // them here for documentation purposes.
    int level = id.level();
    S2_DCHECK_GE(level, options_.min_level());
    S2_DCHECK_LE(level, options_.max_level());
    S2_DCHECK_EQ(0, (level - options_.min_level()) % options_.level_mod());

    // Cells in the covering are always queried as ancestor terms.
    terms.push_back(GetTerm(TermType::ANCESTOR, id, prefix));

    // If the index only contains points, there are no covering terms.
    if (options_.index_contains_points_only()) continue;

    // If we are optimizing for index space rather than query time, cells are
    // also queried as covering terms (except for true_max_level() cells,
    // which are indexed and queried as ancestor cells only).
    if (options_.optimize_for_space() && level < true_max_level) {
      terms.push_back(GetTerm(TermType::COVERING, id, prefix));
    }
    // Finally, add covering terms for all the ancestors of this cell.
    while ((level -= options_.level_mod()) >= options_.min_level()) {
      S2CellId ancestor_id = id.parent(level);
      if (prev_id != S2CellId::None() && prev_id.level() > level &&
          prev_id.parent(level) == ancestor_id) {
        break;  // We have already processed this cell and its ancestors.
      }
      terms.push_back(GetTerm(TermType::COVERING, ancestor_id, prefix));
    }
    prev_id = id;
  }
  return terms;
}
