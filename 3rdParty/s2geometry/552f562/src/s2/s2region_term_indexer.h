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
// S2RegionTermIndexer is a helper class for adding spatial data to an
// information retrieval system.  Such systems work by converting documents
// into a collection of "index terms" (e.g., representing words or phrases),
// and then building an "inverted index" that maps each term to a list of
// documents (and document positions) where that term occurs.
//
// This class deals with the problem of converting spatial data into index
// terms, which can then be indexed along with the other document information.
//
// Spatial data is represented using the S2Region type.  Useful S2Region
// subtypes include:
//
//   S2Cap
//    - a disc-shaped region
//
//   S2LatLngRect
//    - a rectangle in latitude-longitude coordinates
//
//   S2Polyline
//    - a polyline
//
//   S2Polygon
//    - a polygon, possibly with multiple holes and/or shells
//
//   S2CellUnion
//    - a region approximated as a collection of S2CellIds
//
//   S2ShapeIndexRegion
//    - an arbitrary collection of points, polylines, and polygons
//
//   S2ShapeIndexBufferedRegion
//    - like the above, but expanded by a given radius
//
//   S2RegionUnion, S2RegionIntersection
//    - the union or intersection of arbitrary other regions
//
// So for example, if you want to query documents that are within 500 meters
// of a polyline, you could use an S2ShapeIndexBufferedRegion containing the
// polyline with a radius of 500 meters.
//
// Example usage:
//
//   // This class is intended to be used with an external key-value store,
//   // but for this example will we use an unordered_map.  The key is an
//   // index term, and the value is a set of document ids.
//   std::unordered_map<string, std::vector<int>> index;
//
//   // Create an indexer that uses up to 10 cells to approximate each region.
//   S2RegionTermIndexer::Options options;
//   options.set_max_cells(10);
//   S2RegionTermIndexer indexer(options);
//
//   // For this example, we index a disc-shaped region with a 10km radius.
//   S2LatLng center = S2LatLng::FromDegrees(44.1, -56.235);
//   S1Angle radius = S2Earth::ToAngle(util::units::Kilometers(10.0));
//   S2Cap cap(center.ToPoint(), radius);
//
//   // Add the terms for this disc-shaped region to the index.
//   for (const auto& term : indexer.GetIndexTerms(cap)) {
//     index[term].push_back(kSomeDocumentId);
//   }
//
//   // And now at query time: build a latitude-longitude rectangle.
//   S2LatLngRect rect(S2LatLng::FromDegrees(-12.1, 10.2),
//                     S2LatLng::FromDegrees(-9.2,  120.5));
//
//   // Convert the query region to a set of terms, and compute the union
//   // of the document ids associated with those terms.
//   std::set<int> doc_ids;
//   for (const auto& term : indexer.GetQueryTerms(rect)) {
//     doc_ids.insert(index[term].begin(), index[term].end());
//   }
//
//   // "doc_ids" now contains all documents that intersect the query region,
//   // along with some documents that nearly intersect it.  The results can
//   // be further pruned if desired by retrieving the original regions that
//   // were indexed (i.e., the document contents) and checking for exact
//   // intersection with the query region.

#ifndef S2_S2REGION_TERM_INDEXER_H_
#define S2_S2REGION_TERM_INDEXER_H_

#include <string>
#include <vector>

#include "s2/s2cell_union.h"
#include "s2/s2region.h"
#include "s2/s2region_coverer.h"
#include "s2/third_party/absl/strings/string_view.h"

class S2RegionTermIndexer {
 public:
  // The following parameters control the tradeoffs between index size, query
  // size, and accuracy (see s2region_coverer.h for details).
  //
  // IMPORTANT: You must use the same values for min_level(), max_level(), and
  // level_mod() for both indexing and queries, otherwise queries will return
  // incorrect results.  However, max_cells() can be changed as often as
  // desired -- you can even change this parameter for every region.

  class Options : public S2RegionCoverer::Options {
   public:
    Options();

    ///////////////// Options Inherited From S2RegionCoverer ////////////////

    // max_cells() controls the maximum number of cells when approximating
    // each region.  This parameter value may be changed as often as desired
    // (using mutable_options(), see below), e.g. to approximate some regions
    // more accurately than others.
    //
    // Increasing this value during indexing will make indexes more accurate
    // but larger.  Increasing this value for queries will make queries more
    // accurate but slower.  (See s2region_coverer.h for details on how this
    // parameter affects accuracy.)  For example, if you don't mind large
    // indexes but want fast serving, it might be reasonable to set
    // max_cells() == 100 during indexing and max_cells() == 8 for queries.
    //
    // DEFAULT: 8  (coarse approximations)
    using S2RegionCoverer::Options::max_cells;
    using S2RegionCoverer::Options::set_max_cells;

    // min_level() and max_level() control the minimum and maximum size of the
    // S2Cells used to approximate regions.  Setting these parameters
    // appropriately can reduce the size of the index and speed up queries by
    // reducing the number of terms needed.  For example, if you know that
    // your query regions will rarely be less than 100 meters in width, then
    // you could set max_level() as follows:
    //
    //   options.set_max_level(S2::kAvgEdge.GetClosestLevel(
    //       S2Earth::MetersToRadians(100)));
    //
    // This restricts the index to S2Cells that are approximately 100 meters
    // across or larger.  Similar, if you know that query regions will rarely
    // be larger than 1000km across, then you could set min_level() similarly.
    //
    // If min_level() is set too high, then large regions may generate too
    // many query terms.  If max_level() is set too low, then small query
    // regions will not be able to discriminate which regions they intersect
    // very precisely and may return many more candidates than necessary.
    //
    // If you have no idea about the scale of the regions being queried,
    // it is perfectly fine to set min_level() == 0 and max_level() == 30
    // (== S2::kMaxLevel).  The only drawback is that may result in a larger
    // index and slower queries.
    //
    // The default parameter values are suitable for query regions ranging
    // from about 100 meters to 3000 km across.
    //
    // DEFAULT: 4  (average cell width == 600km)
    using S2RegionCoverer::Options::min_level;
    using S2RegionCoverer::Options::set_min_level;

    // DEFAULT: 16 (average cell width == 150m)
    using S2RegionCoverer::Options::max_level;
    using S2RegionCoverer::Options::set_max_level;

    // Setting level_mod() to a value greater than 1 increases the effective
    // branching factor of the S2Cell hierarchy by skipping some levels.  For
    // example, if level_mod() == 2 then every second level is skipped (which
    // increases the effective branching factor to 16).  You might want to
    // consider doing this if your query regions are typically very small
    // (e.g., single points) and you don't mind increasing the index size
    // (since skipping levels will reduce the accuracy of cell coverings for a
    // given max_cells() limit).
    //
    // DEFAULT: 1  (don't skip any cell levels)
    using S2RegionCoverer::Options::level_mod;
    using S2RegionCoverer::Options::set_level_mod;

    // If your index will only contain points (rather than regions), be sure
    // to set this flag.  This will generate smaller and faster queries that
    // are specialized for the points-only case.
    //
    // With the default quality settings, this flag reduces the number of
    // query terms by about a factor of two.  (The improvement gets smaller
    // as max_cells() is increased, but there is really no reason not to use
    // this flag if your index consists entirely of points.)
    //
    // DEFAULT: false
    bool index_contains_points_only() const { return points_only_; }
    void set_index_contains_points_only(bool value) { points_only_ = value; }

    // If true, the index will be optimized for space rather than for query
    // time.  With the default quality settings, this flag reduces the number
    // of index terms and increases the number of query terms by the same
    // factor (approximately 1.3).  The factor increases up to a limiting
    // ratio of 2.0 as max_cells() is increased.
    //
    // CAVEAT: This option has no effect if the index contains only points.
    //
    // DEFAULT: false
    bool optimize_for_space() const { return optimize_for_space_; }
    void set_optimize_for_space(bool value) { optimize_for_space_ = value; }

    // A non-alphanumeric character that is used internally to distinguish
    // between two different types of terms (by adding this character).
    //
    // REQUIRES: "ch" is non-alphanumeric.
    // DEFAULT: '$'
    const string& marker() const { return marker_; }
    char marker_character() const { return marker_[0]; }
    void set_marker_character(char ch);

   private:
    bool points_only_ = false;
    bool optimize_for_space_ = false;
    string marker_ = string(1, '$');
  };

  // Default constructor.  Options can be set using mutable_options().
  S2RegionTermIndexer();
  ~S2RegionTermIndexer();

  // Constructs an S2RegionTermIndexer with the given options.
  explicit S2RegionTermIndexer(const Options& options);

  // S2RegionTermIndexer is movable but not copyable.
  S2RegionTermIndexer(const S2RegionTermIndexer&) = delete;
  S2RegionTermIndexer& operator=(const S2RegionTermIndexer&) = delete;
  S2RegionTermIndexer(S2RegionTermIndexer&&);
  S2RegionTermIndexer& operator=(S2RegionTermIndexer&&);

  // Returns the current options.  Options can be modifed between calls.
  const Options& options() const { return options_; }
  Options* mutable_options() { return &options_; }

  // Converts the given region into a set of terms for indexing.  Terms
  // consist of lowercase letters, numbers, '$', and an optional prefix.
  //
  // "prefix" is a unique prefix used to distinguish S2 terms from other terms
  // in the repository.  The prefix may also be used to index documents with
  // multiple types of location information (e.g. store footprint, entrances,
  // parking lots, etc).  The prefix should be kept short since it is
  // prepended to every term.
  std::vector<string> GetIndexTerms(const S2Region& region,
                                    absl::string_view prefix);

  // Converts a given query region into a set of terms.  If you compute the
  // union of all the documents associated with these terms, the result will
  // include all documents whose index region intersects the query region.
  //
  // "prefix" should match the corresponding value used when indexing.
  std::vector<string> GetQueryTerms(const S2Region& region,
                                    absl::string_view prefix);

  // Convenience methods that accept an S2Point rather than S2Region.  (These
  // methods are also faster.)
  //
  // Note that you can index an S2LatLng by converting it to an S2Point first:
  //     auto terms = GetIndexTerms(S2Point(latlng), ...);
  std::vector<string> GetIndexTerms(const S2Point& point,
                                    absl::string_view prefix);
  std::vector<string> GetQueryTerms(const S2Point& point,
                                    absl::string_view prefix);

  // Low-level methods that accept an S2CellUnion covering of the region to be
  // indexed or queried.
  //
  // REQUIRES: "covering" satisfies the S2RegionCoverer::Options for this
  //           class (i.e., max_cells, min_level, max_level, and level_mod).
  //
  // If you have a covering that was computed using different options, then
  // you can either call the regular S2Region methods (since S2CellUnion is a
  // type of S2Region), or "canonicalize" the covering first by calling
  // S2RegionCoverer::CanonicalizeCovering() with the same options.
  std::vector<string> GetIndexTermsForCanonicalCovering(
      const S2CellUnion& covering, absl::string_view prefix);
  std::vector<string> GetQueryTermsForCanonicalCovering(
      const S2CellUnion& covering, absl::string_view prefix);

 private:
  enum TermType { ANCESTOR, COVERING };

  string GetTerm(TermType term_type, const S2CellId& id,
                 absl::string_view prefix) const;

  Options options_;
  S2RegionCoverer coverer_;
};

#endif  // S2_S2REGION_TERM_INDEXER_H_
