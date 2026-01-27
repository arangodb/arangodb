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

#include "index/composite_reader_impl.hpp"
#include "index/index_meta.hpp"
#include "index/index_writer.hpp"
#include "tests_shared.hpp"
#include "utils/index_utils.hpp"

namespace {

constexpr double minDeletionPercentage { 0.5 };
constexpr double maxSkewThreshold { 0.4 };

[[maybe_unused]] void PrintConsolidation(
  const irs::IndexReader& reader, const irs::ConsolidationPolicy& policy) {
  struct less_t {
    bool operator()(const irs::SubReader* lhs,
                    const irs::SubReader* rhs) const {
      auto& lhs_meta = lhs->Meta();
      auto& rhs_meta = rhs->Meta();
      return lhs_meta.byte_size == rhs_meta.byte_size
               ? lhs_meta.name < rhs_meta.name
               : lhs_meta.byte_size < rhs_meta.byte_size;
    }
  };
  irs::Consolidation candidates;
  irs::ConsolidatingSegments consolidating_segments;

  size_t i = 0;
  while (true) {
    candidates.clear();
    policy(candidates, reader, consolidating_segments, true);

    if (candidates.empty()) {
      break;
    }

    std::set<const irs::SubReader*, less_t> sorted_candidates(
      candidates.begin(), candidates.end(), less_t());

    std::cerr << "Consolidation " << i++ << ": ";
    for (auto* segment : sorted_candidates) {
      auto& meta = segment->Meta();
      std::cerr << meta.byte_size << " ("
                << double_t(meta.live_docs_count) / meta.docs_count << "), ";
    }
    std::cerr << "\n";

    // register candidates for consolidation
    for (const auto* candidate : candidates) {
      consolidating_segments.emplace(candidate->Meta().name);
    }
  }
}

void AssertCandidates(const irs::IndexReader& reader,
                      const std::vector<size_t>& expected_candidates,
                      const irs::Consolidation& actual_candidates,
                      const std::string& errMsg = "") {
  ASSERT_EQ(expected_candidates.size(), actual_candidates.size()) << errMsg;

  for (const size_t expected_candidate_idx : expected_candidates) {
    const auto& expected_candidate = reader[expected_candidate_idx];
    ASSERT_NE(actual_candidates.end(),
              std::find(actual_candidates.begin(), actual_candidates.end(),
                        &expected_candidate));
  }
}

class SubReaderMock final : public irs::SubReader {
 public:
  explicit SubReaderMock(const irs::SegmentInfo meta) : meta_{meta} {}

  virtual uint64_t CountMappedMemory() const { return 0; }

  const irs::SegmentInfo& Meta() const final { return meta_; }

  const SubReaderMock& operator*() const noexcept { return *this; }

  // Live & deleted docs

  const irs::DocumentMask* docs_mask() const final { return nullptr; }

  // Returns an iterator over live documents in current segment.
  irs::doc_iterator::ptr docs_iterator() const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

  irs::doc_iterator::ptr mask(irs::doc_iterator::ptr&& it) const final {
    EXPECT_FALSE(true);
    return std::move(it);
  }

  // Inverted index

  irs::field_iterator::ptr fields() const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

  // Returns corresponding term_reader by the specified field name.
  const irs::term_reader* field(std::string_view) const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

  // Columnstore

  irs::column_iterator::ptr columns() const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

  const irs::column_reader* column(irs::field_id) const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

  const irs::column_reader* column(std::string_view) const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

  const irs::column_reader* sort() const final {
    EXPECT_FALSE(true);
    return nullptr;
  }

 private:
  irs::SegmentInfo meta_;
};

class IndexReaderMock final
  : public irs::CompositeReaderImpl<std::vector<SubReaderMock>> {
 public:
  explicit IndexReaderMock(const irs::IndexMeta& meta)
    : IndexReaderMock{Init{meta}} {}

 private:
  struct Init {
    explicit Init(const irs::IndexMeta& meta) {
      readers.reserve(meta.segments.size());

      for (const auto& segment : meta.segments) {
        readers.emplace_back(segment.meta);
        docs_count += segment.meta.docs_count;
        live_docs_count += segment.meta.live_docs_count;
      }
    }

    std::vector<SubReaderMock> readers;
    uint64_t docs_count{};
    uint64_t live_docs_count{};
  };

  explicit IndexReaderMock(Init&& init) noexcept
    : irs::CompositeReaderImpl<std::vector<SubReaderMock>>{
        std::move(init.readers), init.live_docs_count, init.docs_count} {}
};

void AddSegment(irs::IndexMeta& meta, std::string_view name,
                irs::doc_id_t docs_count, irs::doc_id_t live_docs_count,
                size_t size) {
  auto& segment = meta.segments.emplace_back().meta;
  segment.name = name;
  segment.docs_count = docs_count;
  segment.live_docs_count = live_docs_count;
  segment.byte_size = size;
}

}  // namespace

TEST(ConsolidationTierTest, MaxConsolidationSize) {
  irs::IndexMeta meta;
  for (size_t i = 0; i < 22; ++i) {
    AddSegment(meta, std::to_string(i), 1, 1, 1);
  }
  IndexReaderMock reader{meta};

  {
    irs::index_utils::ConsolidateTier options;
    options.max_segments_bytes = 10;

    irs::ConsolidatingSegments consolidating_segments;
    auto policy = irs::index_utils::MakePolicy(options);

    // 1st tier
    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      // register candidates for consolidation
      for (const auto* candidate : candidates) {
        consolidating_segments.emplace(candidate->Meta().name);
      }
      ASSERT_EQ(options.max_segments_bytes, candidates.size());
    }

    // 2nd tier
    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      // register candidates for consolidation
      for (const auto* candidate : candidates) {
        consolidating_segments.emplace(candidate->Meta().name);
      }
      ASSERT_EQ(options.max_segments_bytes, candidates.size());
    }

    // 3rd tier
    // At this point we'll be left with only 2 segments left
    // for consolidation. But the skew is now over the threshold,
    // so we won't consolidate those segments.
    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      // register candidates for consolidation
      for (const auto* candidate : candidates) {
        consolidating_segments.emplace(candidate->Meta().name);
      }
      ASSERT_EQ(consolidating_segments.size(), 2 * options.max_segments_bytes);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // invalid options: maxSegmentsBytes == 0
  {
    irs::index_utils::ConsolidateTier options;
    options.max_segments_bytes = 0;

    irs::ConsolidatingSegments consolidating_segments;
    auto policy = irs::index_utils::MakePolicy(options);

    // all segments are too big
    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      ASSERT_TRUE(candidates.empty());
    }
  }
}

TEST(ConsolidationTierTest, EmptyMeta) {
  irs::IndexMeta meta;
  IndexReaderMock reader{meta};

  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = std::numeric_limits<size_t>::max();

  irs::ConsolidatingSegments consolidating_segments;
  auto policy = irs::index_utils::MakePolicy(options);
  irs::Consolidation candidates;
  policy(candidates, reader, consolidating_segments, true);
  ASSERT_TRUE(candidates.empty());
}

TEST(ConsolidationTierTest, EmptyConsolidatingSegment) {
  irs::IndexMeta meta;
  AddSegment(meta, "empty", 1, 0, 1);
  IndexReaderMock reader{meta};

  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = std::numeric_limits<size_t>::max();

  irs::ConsolidatingSegments consolidating_segments{reader[0].Meta().name};
  auto policy = irs::index_utils::MakePolicy(options);
  irs::Consolidation candidates;
  policy(candidates, reader, consolidating_segments, true);
  ASSERT_TRUE(candidates.empty());  // skip empty consolidating segments
}

TEST(ConsolidationTierTest, EmptySegment) {
  irs::IndexMeta meta;
  AddSegment(meta, "empty", 0, 0, 1);
  IndexReaderMock reader{meta};

  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = std::numeric_limits<size_t>::max();

  irs::ConsolidatingSegments consolidating_segments{reader[0].Meta().name};
  auto policy = irs::index_utils::MakePolicy(options);
  irs::Consolidation candidates;
  policy(candidates, reader, consolidating_segments, true);
  ASSERT_TRUE(candidates.empty());  // skip empty segments
}

TEST(ConsolidationTierTest, PreferConsolidationOverCleanupWhenDoesntMeetThreshold) {
  irs::IndexMeta meta;
  AddSegment(meta, "0", 10, 10, 10);
  AddSegment(meta, "1", 10, 10, 10);
  AddSegment(meta, "2", 10, 10, 10);
  AddSegment(meta, "3", 10, 10, 10);
  AddSegment(meta, "4", 10, 10, 10);
  AddSegment(meta, "5", 11, 8, 81);
  AddSegment(meta, "6", 10, 9, 91);
  IndexReaderMock reader{meta};

  //  The conslidation threshold defaults are defined
  //  in tier::ConsolidationConfig.

  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = std::numeric_limits<size_t>::max();

  irs::ConsolidatingSegments consolidating_segments;
  auto policy = irs::index_utils::MakePolicy(options);

  irs::Consolidation candidates;
  policy(candidates, reader, consolidating_segments, true);
  ASSERT_EQ(5, candidates.size());
}

TEST(ConsolidationTierTest, PreferCleanupWhenMeetsThreshold) {
  // generate meta
  irs::IndexMeta meta;
  AddSegment(meta, "0", 10, 10, 10);
  AddSegment(meta, "1", 10, 10, 10);
  AddSegment(meta, "2", 10, 10, 10);
  AddSegment(meta, "3", 10, 10, 10);
  AddSegment(meta, "4", 10, 10, 10);
  AddSegment(meta, "5", 11, 5, 11);
  AddSegment(meta, "6", 10, 5, 11);
  IndexReaderMock reader{meta};

  // ensure policy prefers segments with removals
  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = std::numeric_limits<size_t>::max();

  irs::ConsolidatingSegments consolidating_segments;
  auto policy = irs::index_utils::MakePolicy(options);

  irs::Consolidation candidates;
  policy(candidates, reader, consolidating_segments, true);
  ASSERT_EQ(2, candidates.size());
  AssertCandidates(reader, {5, 6}, candidates);
}

TEST(ConsolidationTierTest, Singleton) {
  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = std::numeric_limits<size_t>::max();
  auto policy = irs::index_utils::MakePolicy(options);

  // singleton consolidation without removals
  {
    irs::ConsolidatingSegments consolidating_segments;
    irs::IndexMeta meta;
    AddSegment(meta, "0", 100, 100, 150);
    IndexReaderMock reader{meta};

    // avoid having singletone merges without removals
    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // singleton consolidation with < 50% removals
  {
    irs::ConsolidatingSegments consolidating_segments;
    irs::IndexMeta meta;
    AddSegment(meta, "0", 100, 51, 150);
    IndexReaderMock reader{meta};

    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      ASSERT_TRUE(candidates.empty());
    }
  }

  // singleton consolidation with >= 50% removals
  {
    irs::ConsolidatingSegments consolidating_segments;
    irs::IndexMeta meta;
    AddSegment(meta, "0", 100, 49, 150);
    AddSegment(meta, "1", 100, 59, 150);
    AddSegment(meta, "2", 100, 69, 150);
    AddSegment(meta, "3", 100, 50, 150);
    IndexReaderMock reader{meta};

    {
      irs::Consolidation candidates;
      policy(candidates, reader, consolidating_segments, true);
      AssertCandidates(reader, {0, 3}, candidates);
    }
  }
}

TEST(ConsolidationTierTest, NoCandidates) {
  irs::index_utils::ConsolidateTier options;
  options.max_segments_bytes = 4294967296;
  auto policy = irs::index_utils::MakePolicy(options);

  irs::ConsolidatingSegments consolidating_segments;
  irs::IndexMeta meta;
  AddSegment(meta, "0", 100, 100, 1);
  AddSegment(meta, "1", 100, 100, 4);
  AddSegment(meta, "2", 100, 100, 7);
  AddSegment(meta, "3", 100, 100, 13);
  AddSegment(meta, "4", 100, 100, 26);
  IndexReaderMock reader{meta};

  //  The candidate sizes are such that the skew for any
  //  combination of contiguous candidates is greater than
  //  ConsolidationConfig::maxMergeScore (default: 0.4)
  irs::Consolidation candidates;
  policy(candidates, reader, consolidating_segments, true);
  ASSERT_TRUE(candidates.empty());
}

TEST(ConsolidationTierTest, SkewedSegments) {
    irs::index_utils::ConsolidateTier options;
    options.max_segments_bytes = 52500;  // max size of the merge

    auto policy = irs::index_utils::MakePolicy(options);

    //  test correct selection of candidates
    {
      irs::ConsolidatingSegments consolidating_segments;
      irs::IndexMeta meta;
      AddSegment(meta, "0", 100, 100, 10);
      AddSegment(meta, "1", 100, 100, 40);
      AddSegment(meta, "2", 100, 100, 60);
      AddSegment(meta, "3", 100, 100, 70);
      AddSegment(meta, "4", 100, 100, 100);
      AddSegment(meta, "5", 100, 100, 150);
      AddSegment(meta, "6", 100, 100, 200);
      AddSegment(meta, "7", 100, 100, 500);
      AddSegment(meta, "8", 100, 100, 750);
      AddSegment(meta, "9", 100, 100, 1100);
      AddSegment(meta, "10", 100, 100, 90);
      AddSegment(meta, "11", 100, 100, 75);
      AddSegment(meta, "12", 100, 100, 1500);
      AddSegment(meta, "13", 100, 100, 10000);
      AddSegment(meta, "14", 100, 100, 5000);
      AddSegment(meta, "15", 100, 100, 1750);
      AddSegment(meta, "16", 100, 100, 690);
      IndexReaderMock reader{meta};

      const std::vector<std::vector<size_t>> expected_tiers{
        {0, 1, 2, 3, 4, 10, 11},
        {5, 6, 7, 8, 9, 12, 15, 16}
      };

      for (size_t i = 0; i < expected_tiers.size(); i++) {
        auto& expected_tier = expected_tiers[i];
        irs::Consolidation candidates;
        policy(candidates, reader, consolidating_segments, true);
        AssertCandidates(reader, expected_tier, candidates, "Line: " + std::to_string(__LINE__) + ", i = " + std::to_string(i));
        candidates.clear();
        policy(candidates, reader, consolidating_segments, true);
        AssertCandidates(reader, expected_tier, candidates, "Line: " + std::to_string(__LINE__) + ", i = " + std::to_string(i));
        // register candidates for consolidation
        for (const auto* candidate : candidates) {
          consolidating_segments.emplace(candidate->Meta().name);
        }
      }
    }

    {
      irs::ConsolidatingSegments consolidating_segments;
      irs::IndexMeta meta;
      AddSegment(meta, "0", 100, 100, 1);
      AddSegment(meta, "1", 100, 100, 1);
      AddSegment(meta, "2", 100, 100, 1);
      AddSegment(meta, "3", 100, 100, 75);
      AddSegment(meta, "4", 100, 100, 90);
      AddSegment(meta, "5", 100, 100, 100);
      AddSegment(meta, "6", 100, 100, 150);
      AddSegment(meta, "7", 100, 100, 200);
      AddSegment(meta, "8", 100, 100, 750);
      AddSegment(meta, "9", 100, 100, 1100);
      AddSegment(meta, "10", 100, 100, 1500);
      AddSegment(meta, "11", 100, 100, 1750);
      AddSegment(meta, "12", 100, 100, 5000);
      AddSegment(meta, "13", 100, 100, 10000);
      AddSegment(meta, "14", 100, 100, 15000);
      AddSegment(meta, "15", 100, 100, 20000);
      IndexReaderMock reader{meta};

      const std::vector<std::vector<size_t>> expected_tiers{
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
        {12, 13, 14, 15}
      };

      for (size_t i = 0; i < expected_tiers.size(); i++) {
        auto& expected_tier = expected_tiers[i];
        irs::Consolidation candidates;
        policy(candidates, reader, consolidating_segments, true);
        AssertCandidates(reader, expected_tier, candidates, "Line: " + std::to_string(__LINE__) + ", i = " + std::to_string(i));
        candidates.clear();

        policy(candidates, reader, consolidating_segments, true);
        AssertCandidates(reader, expected_tier, candidates, "Line: " + std::to_string(__LINE__) + ", i = " + std::to_string(i));
        // register candidates for consolidation
        for (const auto* candidate : candidates) {
          consolidating_segments.emplace(candidate->Meta().name);
        }
      }
    }
}

//  The skew is over threshold for each of the combinations of segments
//  but when finally a segment is found that brings the skew within threshold
//  we're over max_segment_bytes and that is why we don't consolidate

TEST(ConsolidationTierTest, SkewedSegmentsAndMaxConsolidationSize) {
  using SEGMENT_SIZE = int;
  size_t maxSegmentsBytes { 140 };

  /*
      The default threshold value is 0.4.
      In the below table, we can see the skew is over 0.4 until the last segment.
      However, at that time we're already over the maxSegmentBytes of 132. So, we
      shouldn't consolidate this candidate.

  segment size:         1       2       4       8      16      32      43      49
  cumulative size:      1       3       7      15      31      63     106     155
  skew:             1.000   0.667   0.571   0.533   0.516   0.508   0.406   0.316

      Instead, we would start removing segments from the candidate from the left until
      we're back within the maxSegmentBytes limit which happens when we get here.

  segment size:         16      32      43      49
  cumulative size:      16      48      91     140
  skew:              1.000   0.667   0.473   0.350

      At this point we will consolidate the above listed segments

  */
  std::vector<SEGMENT_SIZE> segmentSizes
        { 1, 2, 4, 8, 16, 32, 43, 49 };

  auto getAttributes = [](
    int segment,
    tier::SegmentAttributes& attrs) {
      attrs.byteSize = segment;
  };

  std::vector<SEGMENT_SIZE> expected { 16, 32, 43, 49 };

  tier::ConsolidationCandidate<SEGMENT_SIZE> best;
  auto result = tier::findBestConsolidationCandidate<SEGMENT_SIZE>(segmentSizes, maxSegmentsBytes, maxSkewThreshold, getAttributes, best);
  ASSERT_TRUE(result);

  //  [first, last] is inclusive of bounds.
  ASSERT_EQ(std::distance(best.first(), best.last() + 1), expected.size());

  size_t index = 0;
  for (auto itr = best.first(); itr != best.last() + 1; itr++) {
    ASSERT_EQ(expected[index++], *itr);
  }
}

//  When the skew of all the candidates is over the default threshold,
//  we don't consolidate.
TEST(ConsolidationTierTest, SkewOverThresholdDontConsolidate) {

  using SEGMENT_SIZE = int;
  size_t maxSegmentsBytes { 5000 };

  //  The default threshold value is 0.4. In a list of segments sorted
  //  by the segment size, if the size of a segment is >= the
  //  sum of sizes of segments on its left, then the skew will be >= 0.5
  //  which is over the default threshold.
  std::vector<
    std::vector<SEGMENT_SIZE>
      > testcases{
        { 50, 100, 2000, 4000 },
        { 1, 2, 4, 8, 16, 32, 64 },
        { 1, 1, 12 },
        { 12, 12 },
        { 12 },
        { 2, 2, 3}
    };

  auto getAttributes = [](
    int segment,
    tier::SegmentAttributes& attrs) {
      attrs.byteSize = segment;
  };

  for (size_t i = 0; i < testcases.size(); i++) {

    auto& segmentSizes = testcases[i];
    //  No cleanup candidates should be selected here.
    tier::ConsolidationCandidate<SEGMENT_SIZE> best;
    auto result = tier::findBestConsolidationCandidate<SEGMENT_SIZE>(segmentSizes, maxSegmentsBytes, maxSkewThreshold, getAttributes, best);
    ASSERT_FALSE(result);
  }
}

TEST(ConsolidationTierTest, NewSegmentAdditions) {
  using SEGMENT_SIZE = int;
  size_t maxSegmentsBytes { 5000 };

  //  The default threshold value is 0.4. In a list of segments sorted
  //  by the segment size, if the size of a segment is >= the
  //  sum of sizes of segments on its left, then the skew will be >= 0.5
  //  which is over the default threshold.
  std::vector<SEGMENT_SIZE> segmentSizes
        { 1, 2, 4, 8, 16, 32, 64 };

  auto getAttributes = [](
    int segment,
    tier::SegmentAttributes& attrs) {
      attrs.byteSize = segment;
  };

  tier::ConsolidationCandidate<SEGMENT_SIZE> best;
  auto result = tier::findBestConsolidationCandidate<SEGMENT_SIZE>(segmentSizes, maxSegmentsBytes, maxSkewThreshold, getAttributes, best);
  ASSERT_FALSE(result);

  //  Adding 2 segments of size 64 each will lower the
  //  skew and bring it under the default skew threshold
  //  of 0.4 thereby allowing consolidation.
  segmentSizes.emplace_back(64);
  segmentSizes.emplace_back(64);
  result = tier::findBestConsolidationCandidate<SEGMENT_SIZE>(segmentSizes, maxSegmentsBytes, maxSkewThreshold, getAttributes, best);
  ASSERT_TRUE(result);

  //  [first, last] is inclusive of bounds.
  ASSERT_EQ(std::distance(best.first(), best.last() + 1), segmentSizes.size());
}

//  Total segment size goes over max_segment_bytes
//  and we have to remove a few segments from the beginning.
TEST(ConsolidationTierTest, PopLeftTest) {

  //  Segments with individual- and combined- live % over 50%
  size_t maxSegmentsBytes { 9 };
  std::vector<int> segmentSizes{
    1, 1, 2, 3, 3, 4
  };

  auto getAttributes = [](
    int segment,
    tier::SegmentAttributes& attrs) {
      attrs.byteSize = segment;
  };

  //  No cleanup candidates should be selected here.
  tier::ConsolidationCandidate<int> best;
  auto result = tier::findBestConsolidationCandidate<int>(segmentSizes, maxSegmentsBytes, maxSkewThreshold, getAttributes, best);
  ASSERT_TRUE(result);

  ASSERT_EQ(std::distance(best.first(), best.last()), 3);
  ASSERT_EQ(best.first(), segmentSizes.cbegin() + 1);
  ASSERT_EQ(best.last(), segmentSizes.cend() - 2);

  for (auto res_itr = best.first(), src_itr = segmentSizes.cbegin() + 1;
       res_itr != best.last() + 1;
       res_itr++, src_itr++) {
    ASSERT_EQ(*res_itr, *src_itr);
  }
}

TEST(ConsolidationTierTest, CleanupSmokeTest) {

  using LIVE_DOCS_COUNT = int;
  using DOCS_COUNT = int;
  using SegmentType = std::pair<LIVE_DOCS_COUNT, DOCS_COUNT>;

  std::vector<
    std::vector<SegmentType>
        > testcases {
          {
            { 100, 100 }, //  100% live
            { 55, 100 },  //  55% live
            { 66, 100 },  //  66%
            { 49, 100 },  //  49% - under the default threshold of 50%
            { 50, 100 }   //  50% - equal to the default threshold
          },

          //  no qualifying segments here
          {
            { 90, 100 },
            { 80, 100 },
            { 70, 100 },
            { 60, 100 },
            { 51, 100 }
          }
  };

  std::vector<
    std::vector<
      SegmentType>
        > expectedCandidates {
          {
            { 49, 100 },  //  49% - under the default threshold of 50%
            { 50, 100 }
          },
          {
          }
  };

  auto getSegmentAttributes = [](
    const SegmentType& seg,
    tier::SegmentAttributes& attrs) {
      attrs.liveDocsCount = seg.first;
      attrs.docsCount = seg.second;
    };

  for (size_t i = 0; i < testcases.size(); i++) {
    auto& testcase = testcases[i];
    const auto& expectedCandidate = expectedCandidates[i];

    tier::ConsolidationCandidate<SegmentType> best;

    //  returns true if a cleanup candidate is found, false otherwise.
    auto result = tier::findBestCleanupCandidate<SegmentType>(testcase, minDeletionPercentage, getSegmentAttributes, best);

    ASSERT_EQ(result, expectedCandidate.size() > 0);

    if (result) {
      ASSERT_EQ(std::distance(best.first(), best.last() + 1), expectedCandidate.size());

      //  Compare individual elements of candidate
      auto itr = best.first();
      size_t j = 0;
      while (j < expectedCandidate.size()) {
        ASSERT_EQ(expectedCandidate[j++], *itr++);
      }
    }
  }
}

//  Allow single segment cleanup
TEST(ConsolidationTierTest, SingleSegmentCleanup) {

  using LIVE_DOCS_COUNT = int;
  using DOCS_COUNT = int;
  using SegmentType = std::pair<LIVE_DOCS_COUNT, DOCS_COUNT>;

    std::vector<SegmentType> testcase {
            { 44, 100 }
          };

  auto getSegmentAttributes = [](
    const SegmentType& seg,
    tier::SegmentAttributes& attrs) {
      attrs.liveDocsCount = seg.first;
      attrs.docsCount = seg.second;
    };

    tier::ConsolidationCandidate<SegmentType> best;
    auto result = tier::findBestCleanupCandidate<SegmentType>(testcase, minDeletionPercentage, getSegmentAttributes, best);

    ASSERT_TRUE(result);
    ASSERT_EQ(best.first()->first, 44);
    ASSERT_EQ(best.last()->second, 100);
}

//  Cleanup before consolidation
TEST(ConsolidationTierTest, CleanupBeforeConsolidation) {
    irs::index_utils::ConsolidateTier options;
    options.max_segments_bytes = 100000;  // max size of the merge

    auto policy = irs::index_utils::MakePolicy(options);

    //  test correct selection of candidates
    {
      irs::ConsolidatingSegments consolidating_segments;
      irs::IndexMeta meta;
      AddSegment(meta, "0", 100, 100, 10);
      AddSegment(meta, "1", 100, 100, 40);
      AddSegment(meta, "2", 100, 100, 60);
      AddSegment(meta, "3", 100, 100, 70);
      AddSegment(meta, "4", 100, 50, 300);
      IndexReaderMock reader{meta};

      const std::vector<std::vector<size_t>> expected_tiers{
        {4},
        {0, 1, 2, 3}
      };

      //  We're favoring cleanup when we only have 1 cleanup candidate.
      //  On the 2nd iteration we should perform consolidation after
      //  having not found a cleanup candidate.
      for (size_t i = 0; i < expected_tiers.size(); i++) {
        auto& expected_tier = expected_tiers[i];
        irs::Consolidation candidates;
        policy(candidates, reader, consolidating_segments, true);
        AssertCandidates(reader, expected_tier, candidates, "Line: " + std::to_string(__LINE__) + ", i = " + std::to_string(i));
        candidates.clear();
        policy(candidates, reader, consolidating_segments, true);
        AssertCandidates(reader, expected_tier, candidates, "Line: " + std::to_string(__LINE__) + ", i = " + std::to_string(i));
        // register candidates for consolidation
        for (const auto* candidate : candidates) {
          consolidating_segments.emplace(candidate->Meta().name);
        }
      }
    }
}

//  Combined live percentage within threshold
TEST(ConsolidationTierTest, CombinedLivePercentageWithinThreshold) {

  using LIVE_DOCS_COUNT = int;
  using DOCS_COUNT = int;
  using SegmentType = std::pair<LIVE_DOCS_COUNT, DOCS_COUNT>;

  //  Segments with individual- and combined- live % over 50%
  std::vector<SegmentType> segments {
        { 50, 90 },
        { 60, 100 }
  };

  auto getSegmentAttributes = [](
    const SegmentType& seg,
    tier::SegmentAttributes& attrs) {
      attrs.liveDocsCount = seg.first;
      attrs.docsCount = seg.second;
    };

  //  No cleanup candidates should be selected here.
  tier::ConsolidationCandidate<SegmentType> best;
  auto result = tier::findBestCleanupCandidate<SegmentType>(segments, minDeletionPercentage, getSegmentAttributes, best);

  ASSERT_FALSE(result);

  //  Add a segment with a very low live %.
  //  The combined live % of all segments should now be
  //  within the threshold.
  segments.emplace_back(20, 100);
  result = tier::findBestCleanupCandidate<SegmentType>(segments, minDeletionPercentage, getSegmentAttributes, best);
  ASSERT_TRUE(result);

  //  findBestCleanupCandidate sorts the candidates in
  //  the incresing order of their live %.
  std::vector<SegmentType> expected {
        { 20, 100 },
        { 50, 90 },
        { 60, 100 },
  };

  //  [first, last] is inclusive of bounds.
  ASSERT_EQ(std::distance(best.first(), best.last() + 1), expected.size());

  //  Compare individual elements of candidate
  auto itr = best.first();
  size_t j = 0;
  while (j < expected.size()) {
    ASSERT_EQ(expected[j++], *itr++);
  }
}

TEST(ConsolidationTierTest, NullChecks) {

  using LIVE_DOCS_COUNT = int;
  using DOCS_COUNT = int;
  using SegmentType = std::pair<LIVE_DOCS_COUNT, DOCS_COUNT>;

  size_t maxSegmentsBytes { 5000 };
  auto getSegmentAttributes = [](
    const SegmentType& seg,
    tier::SegmentAttributes& attrs) {
      attrs.liveDocsCount = seg.first;
      attrs.docsCount = seg.second;
    };

  {
    //  Zero live docs count
    std::vector<SegmentType> segments {
      { 0, 90 },
      { 0, 100 }
    };

    //  No cleanup candidates should be selected here.
    tier::ConsolidationCandidate<SegmentType> best;
    tier::findBestCleanupCandidate<SegmentType>(segments, minDeletionPercentage, getSegmentAttributes, best);
    tier::findBestConsolidationCandidate<SegmentType>(segments, maxSegmentsBytes, maxSkewThreshold, getSegmentAttributes, best);
  }

  //  Zero docs count
  {
    std::vector<SegmentType> segments {
      { 0, 0 },
      { 0, 0 }
    };

    //  No cleanup candidates should be selected here.
    tier::ConsolidationCandidate<SegmentType> best;
    tier::findBestCleanupCandidate<SegmentType>(segments, minDeletionPercentage, getSegmentAttributes, best);
    tier::findBestConsolidationCandidate<SegmentType>(segments, maxSegmentsBytes, maxSkewThreshold, getSegmentAttributes, best);
  }
}
