////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "index/index_tests.hpp"
#include "index/norm.hpp"
#include "search/bm25.hpp"
#include "search/boolean_filter.hpp"
#include "search/filter.hpp"
#include "search/score.hpp"
#include "search/term_filter.hpp"
#include "search/tfidf.hpp"
#include "utils/index_utils.hpp"

namespace {

class Scorers {
 public:
  template<typename T, typename... Args>
  const T& PushBack(Args&&... args) {
    auto scorer = std::make_unique<T>(std::forward<Args>(args)...);
    auto& bucket = scorers_.emplace_back();
    bucket = scorer.release();
    return static_cast<T&>(*bucket);
  }

  // Intentionally imlicit
  operator irs::ScorersView() const noexcept { return scorers_; }

  ~Scorers() {
    for (auto* scorer : scorers_) {
      delete scorer;
    }
  }

 private:
  std::vector<irs::Scorer*> scorers_;
};

struct Doc {
  Doc(size_t segment, irs::doc_id_t doc) noexcept
    : segment{segment}, doc{doc} {}

  bool operator==(const Doc&) const = default;

  size_t segment;
  irs::doc_id_t doc;
};

struct ScoredDoc : Doc {
  ScoredDoc(size_t segment, irs::doc_id_t doc, float score) noexcept
    : Doc{segment, doc}, score{score} {}

  bool operator<(const ScoredDoc& rhs) const noexcept {
    if (score > rhs.score) {
      return true;
    }
    if (score < rhs.score) {
      return false;
    }
    if (segment < rhs.segment) {
      return true;
    }
    if (segment > rhs.segment) {
      return false;
    }
    return doc < rhs.doc;
  }

  float score;
};

class WandTestCase : public tests::index_test_base {
 public:
  static irs::IndexWriterOptions GetWriterOptions(irs::ScorersView scorers,
                                                  bool write_norms);

  std::vector<Doc> Collect(const irs::DirectoryReader& index,
                           const irs::filter& filter, irs::ScorersView scorers,
                           irs::byte_type wand_idx, bool can_use_wand,
                           size_t limit);

  void AssertResults(const irs::DirectoryReader& index,
                     const irs::filter& filter, irs::ScorersView scorers,
                     irs::byte_type wand_idx, bool can_use_wand, size_t limit);

  void GenerateSegment(irs::ScorersView scorers, bool write_norms,
                       bool append_data = false);
  void GenerateSegmentMinNorm(irs::ScorersView scorers);
  void ConsolidateAll(irs::ScorersView scorers, bool write_norms);

  void AssertFilters(irs::ScorersView scorers, bool disjunction = true) {
    auto apply = [&](auto assert_filter) {
      ASSERT_FALSE(scorers.empty());
      for (size_t idx = 0; auto* scorer : scorers) {
        std::invoke(assert_filter, *this, scorers, *scorer, idx);
        ++idx;
      }
      // Invalid scorer
      std::invoke(assert_filter, *this, scorers, *scorers[0], scorers.size());
    };
    apply(&WandTestCase::AssertTermFilter);
    apply(&WandTestCase::AssertConjunctionFilter);
    if (disjunction) {
      apply(&WandTestCase::AssertDisjunctionFilter);
    }
  }

  void AssertTermFilter(irs::ScorersView scorers, const irs::Scorer& scorer,
                        irs::byte_type wand_index);

  void AssertConjunctionFilter(irs::ScorersView scorers,
                               const irs::Scorer& scorer,
                               irs::byte_type wand_index);

  void AssertDisjunctionFilter(irs::ScorersView scorers,
                               const irs::Scorer& scorer,
                               irs::byte_type wand_index);

  bool CanUseWand(irs::ScorersView scorers, const irs::Scorer& scorer,
                  irs::byte_type wand_index, const irs::term_reader& field) {
    const auto& field_meta = field.meta();
    const auto index_features = scorer.index_features();
    if (index_features != (index_features & field_meta.index_features)) {
      return false;
    }

    irs::feature_set_t features;
    scorer.get_features(features);

    for (const auto feature : features) {
      const auto it = field_meta.features.find(feature);
      if (it == field_meta.features.end() ||
          !irs::field_limits::valid(it->second)) {
        return false;
      }
    }

    return wand_index < scorers.size();
  }
};

std::vector<Doc> WandTestCase::Collect(const irs::DirectoryReader& index,
                                       const irs::filter& filter,
                                       irs::ScorersView scorers,
                                       irs::byte_type wand_idx,
                                       bool can_use_wand, size_t limit) {
  auto prepared = irs::Scorers::Prepare(std::span(
    const_cast<const irs::Scorer**>(&scorers.front()), scorers.size()));
  EXPECT_FALSE(prepared.empty());
  auto query = filter.prepare({.index = index, .scorers = prepared});
  EXPECT_NE(nullptr, query);

  const irs::WandContext mode{.index = wand_idx};

  std::vector<ScoredDoc> sorted;
  sorted.reserve(limit);

  for (size_t left = limit, segment_id = 0; const auto& segment : index) {
    auto docs = query->execute(irs::ExecutionContext{
      .segment = segment, .scorers = prepared, .wand = mode});
    EXPECT_NE(nullptr, docs);

    const auto* doc = irs::get<irs::document>(*docs);
    EXPECT_NE(nullptr, doc);
    auto* score = irs::get_mutable<irs::score>(docs.get());
    EXPECT_NE(nullptr, score);
    if (wand_idx != irs::WandContext::kDisable && can_use_wand) {
      EXPECT_NE(std::numeric_limits<irs::score_t>::max(), score->max.tail);
    } else {
      EXPECT_EQ(std::numeric_limits<irs::score_t>::max(), score->max.tail);
    }

    if (!left) {
      EXPECT_TRUE(!sorted.empty());
      EXPECT_TRUE(std::is_heap(std::begin(sorted), std::end(sorted)));
      score->Min(sorted.front().score);
    }
    std::vector<irs::score_t> scores(scorers.size());
    auto& score_value = *scores.data();
    while (docs->next()) {
      (*score)(&score_value);

      if (left) {
        sorted.emplace_back(segment_id, doc->value, score_value);

        if (0 == --left) {
          std::make_heap(std::begin(sorted), std::end(sorted));
          score->Min(sorted.front().score);
        }
      } else if (sorted.front().score < score_value) {
        std::pop_heap(std::begin(sorted), std::end(sorted));

        auto& min_doc = sorted.back();
        min_doc.segment = segment_id;
        min_doc.doc = doc->value;
        min_doc.score = score_value;

        std::push_heap(std::begin(sorted), std::end(sorted));

        score->Min(sorted.front().score);
      }
    }

    ++segment_id;
  }

  std::sort(std::begin(sorted), std::end(sorted));

  return {std::begin(sorted), std::end(sorted)};
}

void WandTestCase::AssertResults(const irs::DirectoryReader& index,
                                 const irs::filter& filter,
                                 irs::ScorersView scorers,
                                 irs::byte_type scorer_idx, bool can_use_wand,
                                 size_t limit) {
  auto wand_result =
    Collect(index, filter, scorers, scorer_idx, can_use_wand, limit);
  auto result = Collect(index, filter, scorers, irs::WandContext::kDisable,
                        can_use_wand, limit);
  ASSERT_EQ(result, wand_result);
}

void WandTestCase::ConsolidateAll(irs::ScorersView scorers, bool write_norms) {
  const irs::index_utils::ConsolidateCount consolidate_all;
  auto writer =
    open_writer(irs::OM_APPEND, GetWriterOptions(scorers, write_norms));
  ASSERT_TRUE(
    writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)));
  ASSERT_TRUE(writer->Commit());
  ASSERT_EQ(1, writer->GetSnapshot().size());
}

irs::IndexWriterOptions WandTestCase::GetWriterOptions(irs::ScorersView scorers,
                                                       bool write_norms) {
  EXPECT_TRUE(std::all_of(scorers.begin(), scorers.end(),
                          [](auto* scorer) { return scorer != nullptr; }));

  irs::IndexWriterOptions writer_options;
  writer_options.reader_options.scorers = scorers;
  writer_options.features = [write_norms](irs::type_info::type_id id) {
    if (write_norms && irs::type<irs::Norm2>::id() == id) {
      return std::make_pair(
        irs::ColumnInfo{irs::type<irs::compression::none>::get(), {}, false},
        &irs::Norm2::MakeWriter);
    }

    return std::make_pair(
      irs::ColumnInfo{irs::type<irs::compression::none>::get(), {}, false},
      irs::FeatureWriterFactory{});
  };

  return writer_options;
}

void WandTestCase::GenerateSegment(irs::ScorersView scorers, bool write_norms,
                                   bool append_data) {
  tests::json_doc_generator gen(
    resource("simple_single_column_multi_term.json"),
    [](tests::document& doc, std::string_view name,
       const tests::json_doc_generator::json_value& data) {
      using TextField = tests::text_field<std::string>;

      if (tests::json_doc_generator::ValueType::STRING == data.vt) {
        doc.indexed.push_back(std::make_shared<TextField>(
          std::string{name}, data.str, false,
          std::vector{irs::type<irs::Norm2>::id()}));
      }
    });

  auto open_mode = irs::OM_CREATE;
  if (append_data) {
    open_mode |= irs::OM_APPEND;
  }

  add_segment(gen, open_mode, GetWriterOptions(scorers, write_norms));
}

void WandTestCase::GenerateSegmentMinNorm(irs::ScorersView scorers) {
  tests::json_doc_generator gen(
    resource("simple_single_column_multi_term_norm.json"),
    [](tests::document& doc, std::string_view name,
       const tests::json_doc_generator::json_value& data) {
      using TextField = tests::text_field<std::string>;

      if (tests::json_doc_generator::ValueType::STRING == data.vt) {
        doc.indexed.push_back(std::make_shared<TextField>(
          std::string{name}, data.str, false,
          std::vector{irs::type<irs::Norm2>::id()}));
      }
    });

  auto open_mode = irs::OM_CREATE;
  // if (append_data) {
  //   open_mode |= irs::OM_APPEND;
  // }

  add_segment(gen, open_mode, GetWriterOptions(scorers, true));
}

void WandTestCase::AssertTermFilter(irs::ScorersView scorers,
                                    const irs::Scorer& scorer,
                                    irs::byte_type wand_index) {
  static constexpr std::string_view kFieldName = "name";

  irs::by_term filter;
  *filter.mutable_field() = kFieldName;

  auto reader = irs::DirectoryReader{
    dir(), codec(), irs::IndexReaderOptions{.scorers = scorers}};
  ASSERT_NE(nullptr, reader);

  for (const auto& segment : reader) {
    const auto* field = segment.field(kFieldName);
    ASSERT_NE(nullptr, field);

    const auto can_use_wand = CanUseWand(scorers, scorer, wand_index, *field);
    ASSERT_EQ(can_use_wand, field->has_scorer(wand_index));

    for (auto terms = field->iterator(irs::SeekMode::NORMAL); terms->next();) {
      filter.mutable_options()->term = terms->value();

      AssertResults(reader, filter, scorers, wand_index, can_use_wand, 10);
      AssertResults(reader, filter, scorers, wand_index, can_use_wand, 100);
    }
  }
}

void WandTestCase::AssertConjunctionFilter(irs::ScorersView scorers,
                                           const irs::Scorer& scorer,
                                           irs::byte_type wand_index) {
  static constexpr std::string_view kFieldName = "name";

  irs::And conjunction;
  irs::by_term& filter1 = conjunction.add<irs::by_term>();
  *filter1.mutable_field() = kFieldName;
  irs::by_term& filter2 = conjunction.add<irs::by_term>();
  *filter2.mutable_field() = kFieldName;

  auto reader = irs::DirectoryReader{
    dir(), codec(), irs::IndexReaderOptions{.scorers = scorers}};
  ASSERT_NE(nullptr, reader);

  for (const auto& segment : reader) {
    const auto* field = segment.field(kFieldName);
    ASSERT_NE(nullptr, field);

    const auto can_use_wand = CanUseWand(scorers, scorer, wand_index, *field);
    ASSERT_EQ(can_use_wand, field->has_scorer(wand_index));

    auto terms = field->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(terms->next());
    filter1.mutable_options()->term = terms->value();
    ASSERT_TRUE(terms->next());
    filter2.mutable_options()->term = terms->value();

    AssertResults(reader, conjunction, scorers, wand_index, can_use_wand, 10);
    AssertResults(reader, conjunction, scorers, wand_index, can_use_wand, 100);
  }
}

void WandTestCase::AssertDisjunctionFilter(irs::ScorersView scorers,
                                           const irs::Scorer& scorer,
                                           irs::byte_type wand_index) {
  static constexpr std::string_view kFieldName = "name";

  irs::Or disjunction;
  irs::by_term& filter1 = disjunction.add<irs::by_term>();
  *filter1.mutable_field() = kFieldName;
  irs::by_term& filter2 = disjunction.add<irs::by_term>();
  *filter2.mutable_field() = kFieldName;
  irs::by_term& filter3 = disjunction.add<irs::by_term>();
  *filter3.mutable_field() = kFieldName;

  auto reader = irs::DirectoryReader{
    dir(), codec(), irs::IndexReaderOptions{.scorers = scorers}};
  ASSERT_NE(nullptr, reader);

  for (const auto& segment : reader) {
    const auto* field = segment.field(kFieldName);
    ASSERT_NE(nullptr, field);

    const auto can_use_wand = CanUseWand(scorers, scorer, wand_index, *field);
    ASSERT_EQ(can_use_wand, field->has_scorer(wand_index));

    auto terms = field->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(terms->next());
    filter1.mutable_options()->term = terms->value();
    ASSERT_TRUE(terms->next());
    filter2.mutable_options()->term = terms->value();
    ASSERT_TRUE(terms->next());
    filter3.mutable_options()->term = terms->value();

    AssertResults(reader, disjunction, scorers, wand_index, can_use_wand, 10);
    AssertResults(reader, disjunction, scorers, wand_index, can_use_wand, 100);
  }
}

TEST_P(WandTestCase, TermFilterMultipleScorersDense) {
  Scorers scorers;
  scorers.PushBack<irs::TFIDF>(false);
  scorers.PushBack<irs::TFIDF>(true);
  const auto& bm25 = scorers.PushBack<irs::BM25>();
  ASSERT_FALSE(bm25.IsBM15() || bm25.IsBM11());
  const auto& bm15 = scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.f);
  ASSERT_TRUE(bm15.IsBM15());
  const auto& bm11 = scorers.PushBack<irs::BM25>(irs::BM25::K(), 1.f);
  ASSERT_TRUE(bm11.IsBM11());

  GenerateSegment(scorers, true);
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  ConsolidateAll(scorers, true);
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  AssertFilters(scorers);

  ConsolidateAll(scorers, true);
  AssertFilters(scorers);
}

TEST_P(WandTestCase, TermFilterManyScorersDense) {
  Scorers scorers;
  scorers.PushBack<irs::TFIDF>(false);
  scorers.PushBack<irs::TFIDF>(true);
  scorers.PushBack<irs::BM25>();
  scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.0f);
  scorers.PushBack<irs::BM25>(irs::BM25::K(), 1.0f);
  scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.4f);
  scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.2f);
  scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.1f);

  GenerateSegment(scorers, true);
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  ConsolidateAll(scorers, true);
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  AssertFilters(scorers);

  GenerateSegment(scorers, true, true);  // Add another segment
  AssertFilters(scorers);

  ConsolidateAll(scorers, true);
  AssertFilters(scorers);
}

TEST_P(WandTestCase, TermFilterMultipleScorersSparse) {
  Scorers scorers;
  scorers.PushBack<irs::TFIDF>(false);
  scorers.PushBack<irs::TFIDF>(true);
  const auto& bm25 = scorers.PushBack<irs::BM25>();
  ASSERT_FALSE(bm25.IsBM15() || bm25.IsBM11());
  const auto& bm15 = scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.f);
  ASSERT_TRUE(bm15.IsBM15());
  const auto& bm11 = scorers.PushBack<irs::BM25>(irs::BM25::K(), 1.f);
  ASSERT_TRUE(bm11.IsBM11());

  GenerateSegment(scorers, false);
  AssertFilters(scorers);

  GenerateSegment(scorers, false, true);  // Add another segment
  AssertFilters(scorers);

  ConsolidateAll(scorers, false);
  AssertFilters(scorers);
}

TEST_P(WandTestCase, TermFilterTFIDF) {
  Scorers scorers;
  scorers.PushBack<irs::TFIDF>(false);

  GenerateSegment(scorers, true);
  AssertFilters(scorers);
}

TEST_P(WandTestCase, TermFilterTFIDFWithNorms) {
  Scorers scorers;
  scorers.PushBack<irs::TFIDF>(true);

  GenerateSegment(scorers, true);
  AssertFilters(scorers);
}

TEST_P(WandTestCase, TermFilterBM25) {
  Scorers scorers;
  auto& scorer = scorers.PushBack<irs::BM25>();
  ASSERT_FALSE(scorer.IsBM15());
  ASSERT_FALSE(scorer.IsBM11());

  GenerateSegment(scorers, true);
  AssertFilters(scorers);

  GenerateSegmentMinNorm(scorers);
  AssertFilters(scorers, false);
}

TEST_P(WandTestCase, TermFilterBM15) {
  Scorers scorers;
  auto& scorer = scorers.PushBack<irs::BM25>(irs::BM25::K(), 0.f);
  ASSERT_TRUE(scorer.IsBM15());

  GenerateSegment(scorers, true);
  AssertFilters(scorers);
}

TEST_P(WandTestCase, TermFilterBM11) {
  Scorers scorers;
  auto& scorer = scorers.PushBack<irs::BM25>(irs::BM25::K(), 1.f);
  ASSERT_TRUE(scorer.IsBM11());

  GenerateSegment(scorers, true);
  AssertFilters(scorers);
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

static const auto kTestValues =
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_5", "1_0"},
                                       tests::format_info{"1_5simd", "1_0"}));

INSTANTIATE_TEST_SUITE_P(WandTest, WandTestCase, kTestValues,
                         WandTestCase::to_string);

}  // namespace
