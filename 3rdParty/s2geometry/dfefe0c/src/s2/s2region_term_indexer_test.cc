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

#include "s2/s2region_term_indexer.h"

#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include <gtest/gtest.h>

#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2testing.h"

using std::vector;

DEFINE_int32(iters, 400, "number of iterations for testing");

namespace {

enum QueryType { POINT, CAP };

void TestRandomCaps(const S2RegionTermIndexer::Options& options,
                    QueryType query_type) {
  // This function creates an index consisting either of points (if
  // options.index_contains_points_only() is true) or S2Caps of random size.
  // It then executes queries consisting of points (if query_type == POINT)
  // or S2Caps of random size (if query_type == CAP).
  S2RegionTermIndexer indexer(options);
  S2RegionCoverer coverer(options);
  vector<S2Cap> caps;
  vector<S2CellUnion> coverings;
  std::unordered_map<string, vector<int>> index;
  int index_terms = 0, query_terms = 0;
  for (int i = 0; i < FLAGS_iters; ++i) {
    // Choose the region to be indexed: either a single point or a cap
    // of random size (up to a full sphere).
    S2Cap cap;
    vector<string> terms;
    if (options.index_contains_points_only()) {
      cap = S2Cap::FromPoint(S2Testing::RandomPoint());
      terms = indexer.GetIndexTerms(cap.center(), "");
    } else {
      cap = S2Testing::GetRandomCap(
          0.3 * S2Cell::AverageArea(options.max_level()),
          4.0 * S2Cell::AverageArea(options.min_level()));
      terms = indexer.GetIndexTerms(cap, "");
    }
    caps.push_back(cap);
    coverings.push_back(coverer.GetCovering(cap));
    for (const string& term : terms) {
      index[term].push_back(i);
    }
    index_terms += terms.size();
  }
  for (int i = 0; i < FLAGS_iters; ++i) {
    // Choose the region to be queried: either a random point or a cap of
    // random size.
    S2Cap cap;
    vector<string> terms;
    if (query_type == QueryType::CAP) {
      cap = S2Cap::FromPoint(S2Testing::RandomPoint());
      terms = indexer.GetQueryTerms(cap.center(), "");
    } else {
      cap = S2Testing::GetRandomCap(
          0.3 * S2Cell::AverageArea(options.max_level()),
          4.0 * S2Cell::AverageArea(options.min_level()));
      terms = indexer.GetQueryTerms(cap, "");
    }
    // Compute the expected results of the S2Cell query by brute force.
    S2CellUnion covering = coverer.GetCovering(cap);
    std::set<int> expected, actual;
    for (int j = 0; j < caps.size(); ++j) {
      if (covering.Intersects(coverings[j])) {
        expected.insert(j);
      }
    }
    for (const string& term : terms) {
      actual.insert(index[term].begin(), index[term].end());
    }
    EXPECT_EQ(expected, actual);
    query_terms += terms.size();
  }
  printf("Index terms/doc: %.2f,  Query terms/doc: %.2f\n",
         static_cast<double>(index_terms) / FLAGS_iters,
         static_cast<double>(query_terms) / FLAGS_iters);
}

// We run one test case for each combination of space vs. time optimization,
// and indexing regions vs. only points.

TEST(S2RegionTermIndexer, IndexRegionsQueryRegionsOptimizeTime) {
  S2RegionTermIndexer::Options options;
  options.set_optimize_for_space(false);       // Optimize for time.
  options.set_min_level(0);                    // Use face cells.
  options.set_max_level(16);
  options.set_max_cells(20);
  TestRandomCaps(options, QueryType::CAP);
}

TEST(S2RegionTermIndexer, IndexRegionsQueryPointsOptimizeTime) {
  S2RegionTermIndexer::Options options;
  options.set_optimize_for_space(false);       // Optimize for time.
  options.set_min_level(0);                    // Use face cells.
  options.set_max_level(16);
  options.set_max_cells(20);
  TestRandomCaps(options, QueryType::POINT);
}

TEST(S2RegionTermIndexer, IndexRegionsQueryRegionsOptimizeTimeWithLevelMod) {
  S2RegionTermIndexer::Options options;
  options.set_optimize_for_space(false);       // Optimize for time.
  options.set_min_level(6);                    // Constrain min/max levels.
  options.set_max_level(12);
  options.set_level_mod(3);
  TestRandomCaps(options, QueryType::CAP);
}

TEST(S2RegionTermIndexer, IndexRegionsQueryRegionsOptimizeSpace) {
  S2RegionTermIndexer::Options options;
  options.set_optimize_for_space(true);        // Optimize for space.
  options.set_min_level(4);
  options.set_max_level(S2CellId::kMaxLevel);  // Use leaf cells.
  options.set_max_cells(8);
  TestRandomCaps(options, QueryType::CAP);
}

TEST(S2RegionTermIndexer, IndexPointsQueryRegionsOptimizeTime) {
  S2RegionTermIndexer::Options options;
  options.set_optimize_for_space(false);       // Optimize for time.
  options.set_min_level(0);                    // Use face cells.
  options.set_max_level(S2CellId::kMaxLevel);
  options.set_level_mod(2);
  options.set_max_cells(20);
  options.set_index_contains_points_only(true);
  TestRandomCaps(options, QueryType::CAP);
}

TEST(S2RegionTermIndexer, IndexPointsQueryRegionsOptimizeSpace) {
  S2RegionTermIndexer::Options options;
  options.set_optimize_for_space(true);        // Optimize for space.
  options.set_index_contains_points_only(true);
  // Use default parameter values.
  TestRandomCaps(options, QueryType::CAP);
}

TEST(S2RegionTermIndexer, MaxLevelSetLoosely) {
  // Test that correct terms are generated even when (max_level - min_level)
  // is not a multiple of level_mod.
  S2RegionTermIndexer::Options options;
  options.set_min_level(1);
  options.set_level_mod(2);
  options.set_max_level(19);
  S2RegionTermIndexer indexer1(options);
  options.set_max_level(20);
  S2RegionTermIndexer indexer2(options);

  S2Point point = S2Testing::RandomPoint();
  EXPECT_EQ(indexer1.GetIndexTerms(point, ""),
            indexer2.GetIndexTerms(point, ""));
  EXPECT_EQ(indexer1.GetQueryTerms(point, ""),
            indexer2.GetQueryTerms(point, ""));

  S2Cap cap = S2Testing::GetRandomCap(0.0, 1.0);  // Area range.
  EXPECT_EQ(indexer1.GetIndexTerms(cap, ""),
            indexer2.GetIndexTerms(cap, ""));
  EXPECT_EQ(indexer1.GetQueryTerms(cap, ""),
            indexer2.GetQueryTerms(cap, ""));
}

TEST(S2RegionTermIndexer, MoveConstructor) {
  S2RegionTermIndexer x;
  x.mutable_options()->set_max_cells(12345);
  S2RegionTermIndexer y = std::move(x);
  EXPECT_EQ(12345, y.options().max_cells());
}

TEST(S2RegionTermIndexer, MoveAssignmentOperator) {
  S2RegionTermIndexer x;
  x.mutable_options()->set_max_cells(12345);
  S2RegionTermIndexer y;
  y.mutable_options()->set_max_cells(0);
  y = std::move(x);
  EXPECT_EQ(12345, y.options().max_cells());
}

}  // namespace
