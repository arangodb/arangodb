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

namespace {

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

}

TEST(consolidation_test_tier, test_defaults) {
  auto policy = irs::index_utils::consolidate_tier();

  irs::memory_directory dir;
  irs::index_writer::consolidating_segments_t consolidating_segments;

  {
    irs::index_meta meta;
    meta.add(irs::segment_meta("0", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 150));
    meta.add(irs::segment_meta("1", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("2", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("3", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));
    meta.add(irs::segment_meta("4", nullptr, 100, 100, false, irs::segment_meta::file_set{}, 100));

    std::set<const irs::segment_meta*> candidates;
    policy(candidates, dir, meta, consolidating_segments);

    assert_candidates(meta, {0, 1, 2, 3, 4 }, candidates);
  }

  {
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

    std::set<const irs::segment_meta*> candidates;
    policy(candidates, dir, meta, consolidating_segments);

    assert_candidates(meta, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, candidates);
  }
}
