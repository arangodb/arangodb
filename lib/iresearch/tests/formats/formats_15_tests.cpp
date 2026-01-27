////////////////////////////////////////////////////////////////////////////////
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include <limits>
#include <random>

#include "formats/formats_10.hpp"
#include "formats/formats_10_attributes.hpp"
#include "formats/wand_writer.hpp"
#include "formats_test_case_base.hpp"
#include "search/score.hpp"
#include "tests_shared.hpp"

namespace {

struct EmptyColumnProvider : irs::ColumnProvider {
  const irs::column_reader* column(irs::field_id) const final {
    return nullptr;
  }
  const irs::column_reader* column(std::string_view) const final {
    return nullptr;
  }
};

struct FreqScorer : irs::ScorerBase<void> {
  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::FREQ;
  }

  irs::ScoreFunction prepare_scorer(
    const irs::ColumnProvider&,
    const std::map<irs::type_info::type_id, irs::field_id>&,
    const irs::byte_type*, const irs::attribute_provider& attrs,
    irs::score_t) const final {
    auto* freq = irs::get<irs::frequency>(attrs);
    EXPECT_NE(nullptr, freq);

    return irs::ScoreFunction{
      reinterpret_cast<irs::score_ctx&>(const_cast<irs::frequency&>(*freq)),
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        *res = reinterpret_cast<irs::frequency*>(ctx)->value;
      }};
  }

  irs::WandWriter::ptr prepare_wand_writer(size_t max_levels) const {
    return std::make_unique<irs::FreqNormWriter<irs::kWandTagMaxScore>>(
      *this, max_levels);
  }

  irs::WandSource::ptr prepare_wand_source() const {
    return std::make_unique<irs::FreqNormSource<irs::kWandTagFreq>>();
  }
};

class FreqThresholdDocIterator : public irs::doc_iterator {
 public:
  FreqThresholdDocIterator(irs::doc_iterator& impl, uint32_t threshold,
                           bool is_strict)
    : impl_{&impl},
      freq_{irs::get<irs::frequency>(impl)},
      threshold_{threshold},
      is_strict_{is_strict} {}

  irs::attribute* get_mutable(irs::type_info::type_id id) final {
    return impl_->get_mutable(id);
  }

  irs::doc_id_t value() const final { return impl_->value(); }

  bool next() final {
    while (impl_->next()) {
      if (freq_ && Less()) {
        continue;
      }

      return true;
    }
    return false;
  }

  irs::doc_id_t seek(irs::doc_id_t target) final {
    if (target <= value()) {
      return value();
    }

    target = impl_->seek(target);

    if (irs::doc_limits::eof(target)) {
      return irs::doc_limits::eof();
    }

    if (freq_ && Less()) {
      next();
    }

    return value();
  }

 private:
  bool Less() {
    if (is_strict_) {
      return freq_->value <= threshold_;
    } else {
      return freq_->value < threshold_;
    }
  }

  irs::doc_iterator* impl_;
  const irs::frequency* freq_;
  uint32_t threshold_;
  bool is_strict_;
};

class SkipList {
 public:
  struct Step {
    irs::doc_id_t key;
    irs::score_t freq;
  };

  struct Level {
    const irs::doc_id_t step;
    std::vector<Step> steps;
  };

  static SkipList Make(irs::doc_iterator& it, irs::doc_id_t skip_0,
                       irs::doc_id_t skip_n, irs::doc_id_t count);

  SkipList() = default;

  size_t Size() const noexcept { return skip_list_.size(); }
  irs::score_t At(size_t level, irs::doc_id_t doc) const noexcept {
    EXPECT_LT(level, skip_list_.size());

    auto& [_, data] = skip_list_[level];
    auto it = std::lower_bound(
      std::begin(data), std::end(data), Step{doc, 0.f},
      [](const auto& lhs, const auto& rhs) { return lhs.key < rhs.key; });

    EXPECT_NE(it, std::end(data));
    return it->freq;
  }

 private:
  explicit SkipList(std::vector<Level>&& skip_list)
    : skip_list_{std::move(skip_list)} {
    for (auto& [_, level] : skip_list) {
      EXPECT_TRUE(std::is_sorted(
        std::begin(level), std::end(level),
        [](const auto& lhs, const auto& rhs) { return lhs.key < rhs.key; }));
    }
  }

  std::vector<Level> skip_list_;
};

SkipList SkipList::Make(irs::doc_iterator& it, irs::doc_id_t skip_0,
                        irs::doc_id_t skip_n, irs::doc_id_t count) {
  size_t num_levels =
    skip_0 < count ? 1 + irs::math::log(count / skip_0, skip_n) : 0;
  EXPECT_GT(num_levels, 0);

  std::vector<Level> skip_list;
  skip_list.reserve(num_levels);

  auto step = static_cast<irs::doc_id_t>(
    skip_0 * static_cast<size_t>(std::pow(skip_n, num_levels - 1)));

  for (; num_levels; --num_levels) {
    skip_list.emplace_back(Level{step, std::vector{Step{0U, 0.f}}});
    step /= skip_n;
  }

  auto add = [&](irs::doc_id_t i, irs::doc_id_t doc, irs::score_t freq) {
    for (auto& [step, level] : skip_list) {
      if (level.size() * step < count) {
        ASSERT_FALSE(level.empty());
        level.back() = {doc, std::max(level.back().freq, freq)};
        if (0 == (i % step)) {
          level.emplace_back(Step{0, 0.f});
        }
      }
    }
  };

  auto* freq = irs::get<irs::frequency>(it);

  if (freq) {
    for (irs::doc_id_t i = 1; it.next(); ++i) {
      add(i, it.value(), freq->value);
    }

    for (auto& [step, level] : skip_list) {
      level.back() = {irs::doc_limits::eof(),
                      std::numeric_limits<irs::score_t>::max()};
    }
  }

  return SkipList{std::move(skip_list)};
}

void AssertSkipList(const SkipList& expected_freqs, irs::doc_id_t doc,
                    irs::score* threshold) {
  if (!threshold) {
    return;
  }
  const auto size = expected_freqs.Size();
  ASSERT_EQ(size, threshold->max.levels.size());
  if (size != 0) {
    ASSERT_EQ(expected_freqs.At(size - 1, doc), threshold->max.leaf);
  }
  for (size_t i = 0; i < size; ++i) {
    const auto expected_freq = expected_freqs.At(i, doc);
    ASSERT_EQ(expected_freq, threshold->max.levels[i]);
    if (expected_freq != std::numeric_limits<irs::score_t>::max()) {
      ASSERT_LE(expected_freq, threshold->max.tail);
    }
  }
}

class Format15TestCase : public tests::format_test_case {
 public:
  static constexpr size_t kVersion10PostingsWriterBlockSize = 128;
  static constexpr auto kNone = irs::IndexFeatures::NONE;
  static constexpr auto kFreq = irs::IndexFeatures::FREQ;
  static constexpr auto kPos =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS;
  static constexpr auto kOffs = irs::IndexFeatures::FREQ |
                                irs::IndexFeatures::POS |
                                irs::IndexFeatures::OFFS;
  static constexpr auto kPay = irs::IndexFeatures::FREQ |
                               irs::IndexFeatures::POS |
                               irs::IndexFeatures::PAY;
  static constexpr auto kAll =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  using Doc = std::pair<irs::doc_id_t, uint32_t>;
  using Docs = std::vector<Doc>;
  using DocsView = std::span<const Doc>;

  Docs GenerateDocs(size_t count, float_t mean, float_t dev, size_t step);

  std::pair<irs::version10::term_meta, irs::postings_reader::ptr> WriteReadMeta(
    irs::directory& dir, DocsView docs, irs::ScorersView scorers,
    irs::IndexFeatures features);

  void AssertWanderator(irs::doc_iterator::ptr& actual,
                        irs::IndexFeatures features, DocsView docs);
  void AssertBackwardsNext(irs::postings_reader& reader, irs::Scorer& scorer,
                           DocsView docs, irs::IndexFeatures field_features,
                           irs::IndexFeatures features,
                           const irs::term_meta& meta, uint32_t threshold,
                           bool strict);
  void AssertDocsSeq(irs::postings_reader& reader, irs::Scorer& scorer,
                     DocsView docs, irs::IndexFeatures field_features,
                     irs::IndexFeatures features, const irs::term_meta& meta,
                     uint32_t threshold, bool strict);
  void AssertDocsRandom(irs::postings_reader& reader, irs::Scorer& scorer,
                        DocsView docs, irs::IndexFeatures field_features,
                        irs::IndexFeatures features, const irs::term_meta& meta,
                        uint32_t threshold, bool strict, size_t seed,
                        size_t inc);
  void AssertCornerCases(irs::postings_reader& reader, irs::Scorer& scorer,
                         DocsView docs, irs::IndexFeatures field_features,
                         irs::IndexFeatures features,
                         const irs::term_meta& meta, bool strict);
  void AssertPostings(DocsView docs, irs::IndexFeatures field_features,
                      irs::IndexFeatures features, uint32_t threshold,
                      bool strict);
  void AssertPostings(DocsView docs, uint32_t threshold, bool strict);
  void AssertPostings(DocsView docs, uint32_t threshold);

 private:
  irs::doc_iterator::ptr GetWanderator(irs::postings_reader& reader,
                                       irs::Scorer& scorer,
                                       irs::IndexFeatures field_features,
                                       irs::IndexFeatures features,
                                       const irs::term_meta& meta,
                                       uint32_t threshold, bool strict);
};

std::pair<irs::version10::term_meta, irs::postings_reader::ptr>
Format15TestCase::WriteReadMeta(irs::directory& dir, DocsView docs,
                                irs::ScorersView scorers,
                                irs::IndexFeatures features) {
  // If this assertion breaks and you really need to test wanderators
  // with different number of buckets you should adjust GetWanderator
  // and set it proper count of scorers as it currently expect only one.
  EXPECT_EQ(scorers.size(), 1);
  auto codec =
    std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
  EXPECT_NE(nullptr, codec);
  auto writer = codec->get_postings_writer(false, irs::IResourceManager::kNoop);
  EXPECT_NE(nullptr, writer);
  irs::version10::term_meta term_meta;

  {
    const EmptyColumnProvider provider;
    const irs::flush_state state{.dir = &dir,
                                 .columns = &provider,
                                 .name = "segment_name",
                                 .scorers = scorers,
                                 .doc_count = docs.back().first + 1,
                                 .index_features = features};

    auto out = dir.create("attributes");
    EXPECT_FALSE(!out);
    irs::write_string(*out, std::string_view("file_header"));

    writer->prepare(*out, state);
    writer->begin_field(features, irs::feature_map_t{});

    postings it{docs, features};
    writer->write(it, term_meta);
    const auto stats = writer->end_field();
    EXPECT_EQ(docs.size(), stats.docs_count);
    const uint64_t expected_wand_mask{irs::IndexFeatures::NONE !=
                                      (features & irs::IndexFeatures::FREQ)};
    EXPECT_EQ(expected_wand_mask, stats.wand_mask);
    writer->encode(*out, term_meta);
    writer->end();
  }

  irs::SegmentMeta meta;
  meta.name = "segment_name";

  const irs::ReaderState state{.dir = &dir, .meta = &meta, .scorers = scorers};

  auto in = dir.open("attributes", irs::IOAdvice::NORMAL);
  EXPECT_FALSE(!in);
  [[maybe_unused]] const auto tmp = irs::read_string<std::string>(*in);

  auto reader = codec->get_postings_reader();
  EXPECT_NE(nullptr, reader);
  reader->prepare(*in, state, features);

  irs::bstring in_data(in->length() - in->file_pointer(), 0);
  in->read_bytes(&in_data[0], in_data.size());
  const auto* begin = in_data.c_str();

  irs::version10::term_meta read_meta;
  begin += reader->decode(begin, features, read_meta);

  {
    EXPECT_EQ(term_meta.docs_count, read_meta.docs_count);
    EXPECT_EQ(term_meta.doc_start, read_meta.doc_start);
    EXPECT_EQ(term_meta.pos_start, read_meta.pos_start);
    EXPECT_EQ(term_meta.pos_end, read_meta.pos_end);
    EXPECT_EQ(term_meta.pay_start, read_meta.pay_start);
    EXPECT_EQ(term_meta.e_single_doc, read_meta.e_single_doc);
    EXPECT_EQ(term_meta.e_skip_start, read_meta.e_skip_start);
  }

  EXPECT_EQ(begin, in_data.data() + in_data.size());

  return std::make_pair(read_meta, std::move(reader));
}

void Format15TestCase::AssertWanderator(irs::doc_iterator::ptr& actual,
                                        irs::IndexFeatures features,
                                        DocsView /*docs*/) {
  ASSERT_NE(nullptr, actual);

  auto* threshold_value = irs::get_mutable<irs::score>(actual.get());
  ASSERT_NE(threshold_value, nullptr);
  if (irs::IndexFeatures::NONE == (features & irs::IndexFeatures::FREQ)) {
    ASSERT_EQ(std::numeric_limits<irs::score_t>::max(),
              threshold_value->max.tail);
  } else {
    ASSERT_NE(std::numeric_limits<irs::score_t>::max(),
              threshold_value->max.tail);
  }
}

irs::doc_iterator::ptr Format15TestCase::GetWanderator(
  irs::postings_reader& reader, irs::Scorer& scorer,
  irs::IndexFeatures field_features, irs::IndexFeatures features,
  const irs::term_meta& meta, uint32_t threshold, bool strict) {
  const irs::WanderatorOptions options{
    .factory = [&](const irs::attribute_provider& attrs) {
      return scorer.prepare_scorer(EmptyColumnProvider{}, irs::feature_map_t{},
                                   nullptr, attrs, irs::kNoBoost);
    }};

  const bool iterator_has_freq =
    irs::IndexFeatures::NONE != (features & irs::IndexFeatures::FREQ);
  const bool field_has_freq =
    irs::IndexFeatures::NONE != (field_features & irs::IndexFeatures::FREQ);
  irs::WandContext ctx{};
  irs::WandInfo info{};
  EXPECT_EQ((field_features & features), features);
  if (field_has_freq) {
    info.count = 1;
  }
  if (iterator_has_freq) {
    ctx.index = 0;
    ctx.strict = strict;
    info.mapped_index = 0;
  }

  auto actual =
    reader.wanderator(field_features, features, meta, options, ctx, info);
  EXPECT_NE(nullptr, actual);

  auto* score = irs::get_mutable<irs::score>(actual.get());
  score->Min(threshold);

  return actual;
}

void Format15TestCase::AssertBackwardsNext(irs::postings_reader& reader,
                                           irs::Scorer& scorer, DocsView docs,
                                           irs::IndexFeatures field_features,
                                           irs::IndexFeatures features,
                                           const irs::term_meta& meta,
                                           uint32_t threshold, bool strict) {
  auto is_less = [&](auto lhs, auto rhs) {
    if (strict) {
      return lhs <= rhs;
    } else {
      return lhs < rhs;
    }
  };

  for (auto doc = docs.rbegin(), end = docs.rend(); doc != end; ++doc) {
    if (is_less(doc->second, threshold)) {
      continue;
    }

    postings expected_postings{docs, features};
    FreqThresholdDocIterator expected{expected_postings, threshold, strict};

    auto actual = GetWanderator(reader, scorer, field_features, features, meta,
                                threshold, strict);
    AssertWanderator(actual, features, docs);

    ASSERT_FALSE(irs::doc_limits::valid(actual->value()));
    ASSERT_EQ(doc->first, actual->seek(doc->first));

    ASSERT_EQ(doc->first, expected.seek(doc->first));
    AssertFrequencyAndPositions(expected, *actual);

    while (expected.next()) {
      ASSERT_TRUE(actual->next());
      AssertFrequencyAndPositions(expected, *actual);
    }
    ASSERT_FALSE(actual->next());
    ASSERT_TRUE(irs::doc_limits::eof(actual->value()));
  }
}

void Format15TestCase::AssertDocsRandom(irs::postings_reader& reader,
                                        irs::Scorer& scorer, DocsView docs,
                                        irs::IndexFeatures field_features,
                                        irs::IndexFeatures features,
                                        const irs::term_meta& meta,
                                        uint32_t threshold, bool strict,
                                        size_t seed, size_t inc) {
  postings expected_postings{docs, features};
  FreqThresholdDocIterator expected{expected_postings, threshold, strict};

  auto actual = GetWanderator(reader, scorer, field_features, features, meta,
                              threshold, strict);
  AssertWanderator(actual, features, docs);

  ASSERT_FALSE(irs::doc_limits::valid(actual->value()));

  for (size_t i = seed, size = docs.size(); i < size; i += inc) {
    const auto& doc = docs[i];
    const auto expected_doc_id = expected.seek(doc.first);
    ASSERT_EQ(expected_doc_id, actual->seek(expected_doc_id));
    // Seek to the same doc
    ASSERT_EQ(expected_doc_id, actual->seek(expected_doc_id));
    // Seek to the smaller doc
    ASSERT_EQ(expected_doc_id, actual->seek(irs::doc_limits::invalid()));

    AssertFrequencyAndPositions(expected, *actual);
  }

  if (inc == 1) {
    ASSERT_FALSE(actual->next());
    ASSERT_TRUE(irs::doc_limits::eof(actual->value()));

    // Seek after the existing documents
    ASSERT_TRUE(irs::doc_limits::eof(actual->seek(docs.back().first + 42)));
  }
}

void Format15TestCase::AssertDocsSeq(irs::postings_reader& reader,
                                     irs::Scorer& scorer, DocsView docs,
                                     irs::IndexFeatures field_features,
                                     irs::IndexFeatures features,
                                     const irs::term_meta& meta,
                                     uint32_t threshold, bool strict) {
  postings expected_postings{docs, features};
  FreqThresholdDocIterator expected{expected_postings, threshold, strict};
  SkipList skip_list;

  auto actual = GetWanderator(reader, scorer, field_features, features, meta,
                              threshold, strict);
  AssertWanderator(actual, features, docs);

  auto* threshold_value = irs::get_mutable<irs::score>(actual.get());

  if (!threshold_value->max.levels.empty()) {
    postings tmp{docs, field_features};
    skip_list = SkipList::Make(tmp, kVersion10PostingsWriterBlockSize, 8,
                               irs::doc_id_t(docs.size()));
  }

  ASSERT_FALSE(irs::doc_limits::valid(actual->value()));

  while (expected.next()) {
    const auto expected_doc_id = expected.value();
    ASSERT_TRUE(actual->next());

    ASSERT_EQ(expected_doc_id, actual->value());
    ASSERT_EQ(expected_doc_id, actual->seek(expected_doc_id));
    // seek to the same doc
    ASSERT_EQ(expected_doc_id, actual->seek(expected_doc_id));
    // seek to the smaller doc
    ASSERT_EQ(expected_doc_id, actual->seek(irs::doc_limits::invalid()));

    AssertSkipList(skip_list, expected_doc_id, threshold_value);
    AssertFrequencyAndPositions(expected, *actual);
  }

  ASSERT_FALSE(actual->next());
  ASSERT_TRUE(irs::doc_limits::eof(actual->value()));

  // seek after the existing documents
  ASSERT_TRUE(irs::doc_limits::eof(actual->seek(docs.back().first + 42)));
}

Format15TestCase::Docs Format15TestCase::GenerateDocs(size_t count,
                                                      float_t mean, float_t dev,
                                                      size_t step) {
  std::vector<std::pair<irs::doc_id_t, uint32_t>> docs;
  docs.reserve(count);
  std::generate_n(
    std::back_inserter(docs), count,
    [i = (irs::doc_limits::min)(), gen = std::mt19937{},
     distr = std::normal_distribution<float_t>{mean, dev}, step]() mutable {
      const irs::doc_id_t doc = i;
      const auto freq = static_cast<uint32_t>(std::roundf(distr(gen)));
      i += step;

      return std::make_pair(doc, freq);
    });

  auto check_docs = [](const auto& docs) {
    return std::is_sorted(
             std::begin(docs), std::end(docs),
             [](auto& lhs, auto& rhs) { return lhs.first < rhs.first; }) &&
           std::all_of(std::begin(docs), std::end(docs), [](auto& v) {
             return static_cast<int32_t>(v.second) > 0;
           });
  };
  EXPECT_TRUE(check_docs(docs));

  return docs;
}

void Format15TestCase::AssertCornerCases(irs::postings_reader& reader,
                                         irs::Scorer& scorer, DocsView docs,
                                         irs::IndexFeatures field_features,
                                         irs::IndexFeatures features,
                                         const irs::term_meta& meta,
                                         bool strict) {
  // next + seek to eof
  {
    auto it =
      GetWanderator(reader, scorer, field_features, features, meta, 0, strict);
    ASSERT_FALSE(irs::doc_limits::valid(it->value()));
    ASSERT_TRUE(it->next());
    ASSERT_EQ(docs.front().first, it->value());
    ASSERT_TRUE(irs::doc_limits::eof(it->seek(docs.back().first + 42)));
  }

  // Seek to irs::doc_limits::invalid()
  {
    auto it =
      GetWanderator(reader, scorer, field_features, features, meta, 0, strict);
    ASSERT_FALSE(irs::doc_limits::valid(it->value()));
    ASSERT_FALSE(irs::doc_limits::valid(it->seek(irs::doc_limits::invalid())));
    ASSERT_TRUE(it->next());
    ASSERT_EQ(docs.front().first, it->value());
  }

  // Seek to irs::doc_limits::eof()
  {
    auto it =
      GetWanderator(reader, scorer, field_features, features, meta, 0, strict);
    ASSERT_FALSE(irs::doc_limits::valid(it->value()));
    ASSERT_TRUE(irs::doc_limits::eof(it->seek(irs::doc_limits::eof())));
    ASSERT_FALSE(it->next());
    ASSERT_TRUE(irs::doc_limits::eof(it->value()));
  }
}

void Format15TestCase::AssertPostings(DocsView docs,
                                      irs::IndexFeatures field_features,
                                      irs::IndexFeatures features,
                                      uint32_t threshold, bool strict) {
  FreqScorer scorer;
  const irs::Scorer* scorer_ptr = &scorer;

  auto dir = get_directory(*this);
  ASSERT_NE(nullptr, dir);
  auto [meta, reader] =
    WriteReadMeta(*dir, docs, std::span{&scorer_ptr, 1}, field_features);
  ASSERT_NE(nullptr, reader);

  AssertCornerCases(*reader, scorer, docs, field_features, features, meta,
                    strict);

  AssertDocsSeq(*reader, scorer, docs, field_features, features, meta,
                threshold, strict);

  // Seek to every document 127th document in a block
  AssertDocsRandom(*reader, scorer, docs, field_features, features, meta,
                   threshold, strict, kVersion10PostingsWriterBlockSize - 1,
                   kVersion10PostingsWriterBlockSize);

  // Seek to every 128th document in a block
  AssertDocsRandom(*reader, scorer, docs, field_features, features, meta,
                   threshold, strict, kVersion10PostingsWriterBlockSize,
                   kVersion10PostingsWriterBlockSize);

  // Seek to every document
  AssertDocsRandom(*reader, scorer, docs, field_features, features, meta,
                   threshold, strict, 0, 1);

  // Seek to every 5th document
  AssertDocsRandom(*reader, scorer, docs, field_features, features, meta,
                   threshold, strict, 0, 5);

  // Seek backwards && next
  AssertBackwardsNext(*reader, scorer, docs, field_features, features, meta,
                      threshold, strict);
}

void Format15TestCase::AssertPostings(DocsView docs, uint32_t threshold,
                                      bool strict) {
  AssertPostings(docs, kNone, kNone, threshold, strict);
  AssertPostings(docs, kAll, kNone, threshold, strict);
  AssertPostings(docs, kFreq, kFreq, threshold, strict);
  AssertPostings(docs, kPos, kPos, threshold, strict);
  AssertPostings(docs, kOffs, kOffs, threshold, strict);
  AssertPostings(docs, kPay, kPay, threshold, strict);
  AssertPostings(docs, kAll, kAll, threshold, strict);
}

void Format15TestCase::AssertPostings(DocsView docs, uint32_t threshold) {
  AssertPostings(docs, threshold, true);
  AssertPostings(docs, threshold, false);
}

static const auto kTestFormats = ::testing::Values(
  tests::format_info{"1_5", "1_0"}, tests::format_info{"1_5simd", "1_0"});

static constexpr auto kTestDirs =
  tests::getDirectories<tests::kTypesDefault | tests::kTypesRot13_16 |
                        tests::kTypesRot13_7>();

static const auto kTestValues =
  ::testing::Combine(::testing::ValuesIn(kTestDirs), kTestFormats);

// Generic tests
using tests::format_test_case;

INSTANTIATE_TEST_SUITE_P(Format15Test, format_test_case, kTestValues,
                         format_test_case::to_string);

// 1.5 specific tests

TEST_P(Format15TestCase, SingletonPostingsThreshold0) {
  static constexpr size_t kCount = 1;
  static constexpr uint32_t kThreshold = 0;
  static_assert(kCount < kVersion10PostingsWriterBlockSize);

  const auto docs = GenerateDocs(kCount, 50.f, 14.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, ShortPostingsThreshold0) {
  static constexpr size_t kCount = 117;  // < postings_writer::BLOCK_SIZE
  static constexpr uint32_t kThreshold = 0;
  static_assert(kCount < kVersion10PostingsWriterBlockSize);

  const auto docs = GenerateDocs(kCount, 50.f, 14.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, BlockPostingsThreshold0) {
  static constexpr size_t kCount = kVersion10PostingsWriterBlockSize;
  static constexpr uint32_t kThreshold = 0;
  const auto docs = GenerateDocs(kCount, 50.f, 14.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, LongPostingsThreshold60) {
  static constexpr size_t kCount = 10000;
  static constexpr uint32_t kThreshold = 60;
  const auto docs = GenerateDocs(kCount, 50.f, 13.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, LongPostingsThreshold100) {
  static constexpr size_t kCount = 10000;
  static constexpr uint32_t kThreshold = 100;
  const auto docs = GenerateDocs(kCount, 50.f, 13.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, MediumPostingsThreshold0) {
  static constexpr size_t kCount = 319;
  static_assert(kCount > kVersion10PostingsWriterBlockSize);
  static constexpr uint32_t kThreshold = 0;
  const auto docs = GenerateDocs(kCount, 50.f, 13.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, LongPostingsThreshold0) {
  GTEST_SKIP() << "too long for our CI";
  static constexpr size_t kCount = 10000;
  static constexpr uint32_t kThreshold = 0;
  const auto docs = GenerateDocs(kCount, 50.f, 13.f, 1);

  AssertPostings(docs, kThreshold);
}

TEST_P(Format15TestCase, VeryLongPostingsThreshold0) {
  GTEST_SKIP() << "too long for our CI";
  static constexpr size_t kCount = size_t{1} << 15;
  static constexpr uint32_t kThreshold = 0;
  const auto docs = GenerateDocs(kCount, 1000.f, 20.f, 2);

  AssertPostings(docs, kThreshold);
}

static constexpr auto kTestDirs15 =
  tests::getDirectories<tests::kTypesDefault>();

static const auto kTestValues15 =
  ::testing::Combine(::testing::ValuesIn(kTestDirs15), kTestFormats);

INSTANTIATE_TEST_SUITE_P(Format15Test, Format15TestCase, kTestValues15,
                         Format15TestCase::to_string);

}  // namespace
