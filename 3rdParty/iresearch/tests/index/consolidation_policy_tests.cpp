////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include "index/index_writer.hpp"
#include "store/memory_directory.hpp"
#include "utils/index_utils.hpp"

NS_LOCAL

void assert_candidates(
    const irs::index_meta& meta,
    const std::vector<size_t>& expected_candidates,
    const std::set<const irs::segment_meta*>& actual_candidates
) {
  ASSERT_EQ(expected_candidates.size(), actual_candidates.size());

  for (const size_t expected_candidate_idx : expected_candidates) {
    const auto& expected_candidate = meta[expected_candidate_idx];
    ASSERT_NE(actual_candidates.end(), actual_candidates.find(&expected_candidate.meta));
  }
}

NS_END

TEST(consolidation_test_tier, test_defaults) {
  irs::index_utils::consolidate_tier options;
  auto policy = irs::index_utils::consolidation_policy(options);

  irs::memory_directory dir;

  {
    irs::index_writer::consolidating_segments_t consolidating_segments;

    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100,  false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 2, 3, 4 }, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 2, 3, 4 }, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("10", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(consolidation_test_tier, test_skewed_segments) {
  irs::memory_directory dir;

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 1;         // min number of segments per tier to merge at once
    options.max_segments = 10;        // max number of segments per tier to merge at once
    options.max_segments_bytes= 2500; // max size of the merge
    options.floor_segment_bytes = 50; // smaller segments will be treated as equal to this value
    options.lookahead = irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 40));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 60));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 70));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 200));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 750));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1100));
    meta.add(irs::segment_meta("10", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 90));
    meta.add(irs::segment_meta("11", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 75));
    meta.add(irs::segment_meta("12", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1500));
    meta.add(irs::segment_meta("13", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10000));
    meta.add(irs::segment_meta("14", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 5000));
    meta.add(irs::segment_meta("15", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1750));
    meta.add(irs::segment_meta("16", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 690));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 2, 3, 4, 10, 11 }, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 2, 3, 4, 10, 11 }, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {7, 8, 9, 16}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {7, 8, 9, 16}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 3rd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {5, 6}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {5, 6}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 1;            // min number of segments per tier to merge at once
    options.max_segments = 10;           // max number of segments per tier to merge at once
    options.max_segments_bytes = 250000; // max size of the merge
    options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
    options.lookahead =  irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1000));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2000));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 4000));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 12000));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 30000));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 50000));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100000));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {2, 3}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {2, 3}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

  }

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 3;            // min number of segments per tier to merge at once
    options.max_segments = 10;           // max number of segments per tier to merge at once
    options.max_segments_bytes = 250000; // max size of the merge
    options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
    options.lookahead = irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1000));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2000));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 4000));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 12000));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 30000));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 50000));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100000));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {2, 3, 4}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {2, 3, 4}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {7, 8, 9}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {7, 8, 9}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 3rd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 5}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 1, 5}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }
  }

  {
    irs::index_utils::consolidate_tier options;
    options.min_segments = 1;            // min number of segments per tier to merge at once
    options.max_segments = 10;           // max number of segments per tier to merge at once
    options.max_segments_bytes = 250000; // max size of the merge
    options.floor_segment_bytes = 50;    // smaller segments will be treated as equal to this value
    options.lookahead = irs::integer_traits<size_t>::const_max;
    auto policy = irs::index_utils::consolidation_policy(options);

    irs::index_writer::consolidating_segments_t consolidating_segments;
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 10));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 500));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1000));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2000));
    meta.add(irs::segment_meta("5", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 4000));
    meta.add(irs::segment_meta("6", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 12000));
    meta.add(irs::segment_meta("7", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 30000));
    meta.add(irs::segment_meta("8", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 50000));
    meta.add(irs::segment_meta("9", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100000));
    meta.add(irs::segment_meta("10", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 51));
    meta.add(irs::segment_meta("11", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 151));
    meta.add(irs::segment_meta("12", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 637));
    meta.add(irs::segment_meta("13", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 351));
    meta.add(irs::segment_meta("14", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 2351));
    meta.add(irs::segment_meta("15", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1351));
    meta.add(irs::segment_meta("16", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 1351));
    meta.add(irs::segment_meta("17", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 20));

    // 1st tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 10, 17}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {0, 10, 17}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 2nd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {3, 4, 14, 15, 16}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {3, 4, 14, 15, 16}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 3rd tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {2, 12, 13}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {2, 12, 13}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 4th tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {1, 11}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {1, 11}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 5th tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {7, 8}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {7, 8}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 6th tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {5, 6}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {5, 6}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // 7th tier
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {9}, candidates);
      candidates.clear();
      policy(candidates, dir, meta, consolidating_segments);
      assert_candidates(meta, {9}, candidates);
      consolidating_segments.insert(candidates.begin(), candidates.end()); // register candidates for consolidation
    }

    // no more segments to consolidate
    {
      std::set<const irs::segment_meta*> candidates;
      policy(candidates, dir, meta, consolidating_segments);
      ASSERT_TRUE(candidates.empty());
    }

  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------