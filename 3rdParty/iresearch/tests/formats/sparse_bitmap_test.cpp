////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#include "tests_shared.hpp"
#include "tests_param.hpp"

#include "formats/sparse_bitmap.hpp"

class sparse_bitmap_test_case : public tests::directory_test_case_base<> {
 protected:
  // min, max
  using range_type = std::pair<irs::doc_id_t, irs::doc_id_t>;
  // position, expected, index
  using seek_type = std::tuple<irs::doc_id_t, irs::doc_id_t, irs::doc_id_t>;

  static constexpr range_type MIXED[] {
    { 1, 32 }, { 160, 1184, }, { 1201, 1734 }, { 60000, 64500 },
    { 196608, 262144 },
    { 328007, 328284 }, { 328412, 329489 }, { 329490, 333586 },
    { 458757, 458758 }, { 458777, 460563 }
  };

  static constexpr range_type DENSE[] {
    { 1, 32 }, { 160, 1184, }, { 1201, 1734 }, { 60000, 64500 },
    { 328007, 328284 }, { 328412, 329489 }, { 329490, 333586 }
  };

  static constexpr range_type SPARSE[] {
    { 1, 32 }, { 160, 1184, }, { 1201, 1734 },
    { 328007, 328284 }, { 328412, 329489 }
  };

  static constexpr std::pair<irs::doc_id_t, irs::doc_id_t> ALL[] {
    { 65536, 131072 },
    { 196608, 262144 }
  };

  template<size_t N>
  void test_rw_next(const range_type (&ranges)[N]);

  template<size_t N>
  void test_rw_seek(const range_type (&ranges)[N]);

  template<size_t N>
  void test_rw_seek_next(const range_type (&ranges)[N]);

  template<size_t N, size_t K>
  void test_rw_seek_random(
    const range_type (&ranges)[N],
    const seek_type(&seeks)[K]);

  template<size_t N, size_t K>
  void test_rw_seek_random_stateless(
    const range_type (&ranges)[N],
    const seek_type(&seeks)[K]);
};

template<size_t N, size_t K>
void sparse_bitmap_test_case::test_rw_seek_random_stateless(
    const range_type (&ranges)[N],
    const seek_type (&seeks)[K]) {
  std::vector<irs::sparse_bitmap_writer::block> bitmap_index;
  irs::cost::cost_t count = 0;

  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);

    for (const auto& range : ranges) {
      irs::doc_id_t doc = range.first;
      std::generate_n(
        std::back_inserter(writer),
        range.second - range.first,
        [&doc, &count] {
          ++count;
          return doc++;
        });
    }

    writer.finish();
    bitmap_index = writer.index();
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    uint32_t value_index = 0;
    for (auto& range : ranges) {
      for (auto [min, max] = range; min < max; ++min) {
        stream->seek(0);

        irs::sparse_bitmap_iterator it{
          stream.get(),
          { {bitmap_index.data(), bitmap_index.size()}, true },
          count };
        auto* index = irs::get<irs::value_index>(it);
        ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
        auto* doc = irs::get<irs::document>(it);
        ASSERT_NE(nullptr, doc);
        ASSERT_FALSE(irs::doc_limits::valid(doc->value));
        auto* cost = irs::get<irs::cost>(it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(count, cost->estimate());

        ASSERT_EQ(min, it.seek(min));
        ASSERT_EQ(value_index, it.index());
        ASSERT_EQ(min, it.seek(min));
        ASSERT_EQ(value_index, it.index());

        ++value_index;
      }
    }

    for (auto& seek : seeks) {
      stream->seek(0);
      irs::sparse_bitmap_iterator it{
        stream.get(),
        { {bitmap_index.data(), bitmap_index.size()}, true },
        count };
      auto* index = irs::get<irs::value_index>(it);
      ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
      auto* doc = irs::get<irs::document>(it);
      ASSERT_NE(nullptr, doc);
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      auto* cost = irs::get<irs::cost>(it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(count, cost->estimate());

      ASSERT_EQ(std::get<1>(seek), it.seek(std::get<0>(seek)));
      ASSERT_EQ(std::get<1>(seek), doc->value);
      if (!irs::doc_limits::eof(std::get<1>(seek))) {
        ASSERT_EQ(std::get<2>(seek), it.index());
        ASSERT_EQ(std::get<2>(seek), index->value);
      }
    }
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    uint32_t value_index = 0;
    for (auto& range : ranges) {
      for (auto [min, max] = range; min < max; ++min) {
        stream->seek(0);

        irs::sparse_bitmap_iterator it{ stream.get(), {{}, true}, count };
        auto* index = irs::get<irs::value_index>(it);
        ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
        auto* doc = irs::get<irs::document>(it);
        ASSERT_NE(nullptr, doc);
        ASSERT_FALSE(irs::doc_limits::valid(doc->value));
        auto* cost = irs::get<irs::cost>(it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(count, cost->estimate());

        ASSERT_EQ(min, it.seek(min));
        ASSERT_EQ(value_index, it.index());
        ASSERT_EQ(min, it.seek(min));
        ASSERT_EQ(value_index, it.index());

        ++value_index;
      }
    }

    for (auto& seek : seeks) {
      stream->seek(0);
      irs::sparse_bitmap_iterator it{stream.get(), {{}, false}, count};
      auto* index = irs::get<irs::value_index>(it);
      ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
      auto* doc = irs::get<irs::document>(it);
      ASSERT_NE(nullptr, doc);
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      auto* cost = irs::get<irs::cost>(it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(count, cost->estimate());

      ASSERT_EQ(std::get<1>(seek), it.seek(std::get<0>(seek)));
      ASSERT_EQ(std::get<1>(seek), doc->value);
      if (!irs::doc_limits::eof(std::get<1>(seek))) {
        ASSERT_EQ(std::get<2>(seek), it.index());
        ASSERT_EQ(std::get<2>(seek), index->value);
      }
    }
  }
}

template<size_t N, size_t K>
void sparse_bitmap_test_case::test_rw_seek_random(
    const range_type (&ranges)[N],
    const seek_type (&seeks)[K]) {
  std::vector<irs::sparse_bitmap_writer::block> bitmap_index;

  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);

    for (const auto& range : ranges) {
      irs::doc_id_t doc = range.first;
      std::generate_n(
        std::back_inserter(writer),
        range.second - range.first,
        [&doc] { return doc++; });
    }

    writer.finish();
    bitmap_index = writer.index();
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{
      std::move(stream),
      { {bitmap_index.data(), bitmap_index.size()}, true } };
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());

    for (auto& seek : seeks) {
      ASSERT_EQ(std::get<1>(seek), it.seek(std::get<0>(seek)));
      ASSERT_EQ(std::get<1>(seek), doc->value);
      if (!irs::doc_limits::eof(std::get<1>(seek))) {
        ASSERT_EQ(std::get<2>(seek), it.index());
        ASSERT_EQ(std::get<2>(seek), index->value);
      }
    }
  }

  // no index
  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{std::move(stream), {{}, true}};
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());

    for (auto& seek : seeks) {
      ASSERT_EQ(std::get<1>(seek), it.seek(std::get<0>(seek)));
      ASSERT_EQ(std::get<1>(seek), doc->value);
      if (!irs::doc_limits::eof(std::get<1>(seek))) {
        ASSERT_EQ(std::get<2>(seek), it.index());
        ASSERT_EQ(std::get<2>(seek), index->value);
      }
    }
  }
}

template<size_t N>
void sparse_bitmap_test_case::test_rw_next(const range_type (&ranges)[N]) {
  std::vector<irs::sparse_bitmap_writer::block> bitmap_index;
  irs::cost::cost_t count = 0;

  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);

    for (const auto& range : ranges) {
      irs::doc_id_t doc = range.first;
      std::generate_n(
        std::back_inserter(writer),
        range.second - range.first,
        [&doc, &count] {
          ++count;
          return doc++;
        });
    }

    writer.finish();
    bitmap_index = writer.index();
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{
      std::move(stream),
      { {bitmap_index.data(), bitmap_index.size()}, true }, count};
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(count, cost->estimate());

    irs::doc_id_t expected_index = 0;

    for (const auto range : ranges) {
      irs::doc_id_t expected_doc = range.first;

      while(expected_doc < range.second) {
        SCOPED_TRACE(expected_doc);
        ASSERT_TRUE(it.next());
        ASSERT_EQ(expected_doc, it.value());
        ASSERT_EQ(expected_doc, doc->value);
        ASSERT_EQ(expected_index, it.index());
        ASSERT_EQ(expected_index, index->value);
        ++expected_doc;
        ++expected_index;
      }
    }

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // no index
  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{std::move(stream), {{}, false}, count};
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(count, cost->estimate());

    irs::doc_id_t expected_index = 0;

    for (const auto range : ranges) {
      irs::doc_id_t expected_doc = range.first;

      while(expected_doc < range.second) {
        SCOPED_TRACE(expected_doc);
        ASSERT_TRUE(it.next());
        ASSERT_EQ(expected_doc, it.value());
        ASSERT_EQ(expected_doc, doc->value);
        ASSERT_EQ(expected_index, it.index());
        ASSERT_EQ(expected_index, index->value);
        ++expected_doc;
        ++expected_index;
      }
    }

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

template<size_t N>
void sparse_bitmap_test_case::test_rw_seek(const range_type (&ranges)[N]) {
  std::vector<irs::sparse_bitmap_writer::block> bitmap_index;
  irs::cost::cost_t count = 0;

  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);

    for (const auto& range : ranges) {
      irs::doc_id_t doc = range.first;
      std::generate_n(
        std::back_inserter(writer),
        range.second - range.first,
        [&doc, &count] {
          ++count;
          return doc++;
        });
    }

    writer.finish();
    bitmap_index = writer.index();
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{
      std::move(stream),
      { {bitmap_index.data(), bitmap_index.size()}, true },
      count };
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(count, cost->estimate());

    irs::doc_id_t expected_index = 0;
    irs::doc_id_t expected_doc = 0;

    for (const auto range : ranges) {
      expected_doc = range.first;

      while(expected_doc < range.second) {
        ASSERT_EQ(expected_doc, it.seek(expected_doc));
        ASSERT_EQ(expected_doc, it.value());
        ASSERT_EQ(expected_doc, doc->value);
        ASSERT_EQ(expected_index, it.index());
        ASSERT_EQ(expected_index, index->value);
        ++expected_doc;
        ++expected_index;
      }
    }

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_TRUE(irs::doc_limits::eof(it.seek(expected_doc)));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }

  // no index
  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{ std::move(stream), {{}, false} };
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());

    irs::doc_id_t expected_index = 0;
    irs::doc_id_t expected_doc = 0;

    for (const auto range : ranges) {
      expected_doc = range.first;

      while(expected_doc < range.second) {
        ASSERT_EQ(expected_doc, it.seek(expected_doc));
        ASSERT_EQ(expected_doc, it.value());
        ASSERT_EQ(expected_doc, doc->value);
        ASSERT_EQ(expected_index, it.index());
        ASSERT_EQ(expected_index, index->value);
        ++expected_doc;
        ++expected_index;
      }
    }

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_TRUE(irs::doc_limits::eof(it.seek(expected_doc)));
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

template<size_t N>
void sparse_bitmap_test_case::test_rw_seek_next(const range_type (&ranges)[N]) {
  std::vector<irs::sparse_bitmap_writer::block> bitmap_index;
  irs::cost::cost_t count = 0;

  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);

    for (const auto& range : ranges) {
      irs::doc_id_t doc = range.first;
      std::generate_n(
        std::back_inserter(writer),
        range.second - range.first,
        [&doc, &count] {
          ++count;
          return doc++;
        });
    }

    writer.finish();
    bitmap_index = writer.index();
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::doc_id_t expected_doc;
    irs::doc_id_t max_doc;

    auto begin = std::begin(ranges);
    for (; begin != std::end(ranges); ++begin) {
      std::tie(expected_doc, max_doc) = *begin;

      while (expected_doc < max_doc) {
        irs::doc_id_t expected_index = 0;

        for (auto range = std::begin(ranges); range != begin; ++range) {
          ASSERT_LE(range->first, range->second);
          expected_index += range->second - range->first;
        }

        stream->seek(0);
        ASSERT_EQ(0, stream->file_pointer());

        irs::sparse_bitmap_iterator it{
          stream->dup(),
          { {bitmap_index.data(), bitmap_index.size()}, true },
          [count](){return count;} };
        auto* index = irs::get<irs::value_index>(it);
        ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
        auto* doc = irs::get<irs::document>(it);
        ASSERT_NE(nullptr, doc);
        ASSERT_FALSE(irs::doc_limits::valid(doc->value));
        auto* cost = irs::get<irs::cost>(it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(count, cost->estimate());

        ASSERT_EQ(expected_doc, it.seek(expected_doc));
        ASSERT_EQ(expected_doc, it.value());
        ASSERT_EQ(expected_doc, doc->value);
        ASSERT_EQ(expected_index, it.index());
        ASSERT_EQ(expected_index, index->value);

        ++expected_doc;
        ++expected_index;

        for (auto range = begin;;) {
          for (; expected_doc < max_doc; ) {
            ASSERT_TRUE(it.next());
            ASSERT_EQ(expected_doc, it.value());
            ASSERT_EQ(expected_doc, doc->value);
            ASSERT_EQ(expected_index, it.index());
            ASSERT_EQ(expected_index, index->value);
            ++expected_doc;
            ++expected_index;
          }

          if (++range < std::end(ranges)) {
            // attempt to seek backwards
            ASSERT_EQ(it.value(), it.seek(it.value()-1));

            std::tie(expected_doc, max_doc) = *range;
          } else {
            break;
          }
        }

        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
        ASSERT_TRUE(irs::doc_limits::eof(it.seek(expected_doc)));
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
    }
  }

  // no index
  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::doc_id_t expected_doc;
    irs::doc_id_t max_doc;

    auto begin = std::begin(ranges);
    for (; begin != std::end(ranges); ++begin) {
      std::tie(expected_doc, max_doc) = *begin;

      while (expected_doc < max_doc) {
        irs::doc_id_t expected_index = 0;

        for (auto range = std::begin(ranges); range != begin; ++range) {
          ASSERT_LE(range->first, range->second);
          expected_index += range->second - range->first;
        }

        stream->seek(0);
        ASSERT_EQ(0, stream->file_pointer());

        irs::sparse_bitmap_iterator it{ stream->dup(), {{}, false} };
        auto* index = irs::get<irs::value_index>(it);
        ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
        auto* doc = irs::get<irs::document>(it);
        ASSERT_NE(nullptr, doc);
        ASSERT_FALSE(irs::doc_limits::valid(doc->value));
        auto* cost = irs::get<irs::cost>(it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(0, cost->estimate());

        ASSERT_EQ(expected_doc, it.seek(expected_doc));
        ASSERT_EQ(expected_doc, it.value());
        ASSERT_EQ(expected_doc, doc->value);
        ASSERT_EQ(expected_index, it.index());
        ASSERT_EQ(expected_index, index->value);

        ++expected_doc;
        ++expected_index;

        for (auto range = begin;;) {
          for (; expected_doc < max_doc; ) {
            ASSERT_TRUE(it.next());
            ASSERT_EQ(expected_doc, it.value());
            ASSERT_EQ(expected_doc, doc->value);
            ASSERT_EQ(expected_index, it.index());
            ASSERT_EQ(expected_index, index->value);
            ++expected_doc;
            ++expected_index;
          }

          if (++range < std::end(ranges)) {
            // attempt to seek backwards
            ASSERT_EQ(it.value(), it.seek(it.value()-1));

            std::tie(expected_doc, max_doc) = *range;
          } else {
            break;
          }
        }

        ASSERT_FALSE(it.next());
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
        ASSERT_TRUE(irs::doc_limits::eof(it.seek(expected_doc)));
        ASSERT_TRUE(irs::doc_limits::eof(it.value()));
      }
    }
  }
}

TEST_P(sparse_bitmap_test_case, read_write_empty) {
  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);
    ASSERT_TRUE(writer.index().empty());
    writer.finish();

    auto& index = writer.index();
    ASSERT_EQ(1, index.size());
    ASSERT_EQ(0, index.front().index);
    ASSERT_EQ(0, index.front().offset);
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it(std::move(stream), {{}, true});
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
    ASSERT_EQ(0, irs::cost::extract(it));
  }
}

TEST_P(sparse_bitmap_test_case, rw_mixed_next) {
  test_rw_next(MIXED);
}

TEST_P(sparse_bitmap_test_case, rw_mixed_seek) {
  test_rw_seek(MIXED);
}

TEST_P(sparse_bitmap_test_case, rw_mixed_seek_next) {
  test_rw_seek_next(MIXED);
}

TEST_P(sparse_bitmap_test_case, rw_sparse_blocks) {
  constexpr irs::doc_id_t STEP = 65536;
  static_assert(irs::doc_limits::min() + 1 < STEP);
  std::vector<irs::sparse_bitmap_writer::block> bitmap_index;

  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);

    for (irs::doc_id_t doc = irs::doc_limits::min() + 1;;) {
      writer.push_back(doc);

      doc += STEP;

      if (doc == (irs::doc_limits::min() + 1)) {
        break;
      }
    }

    writer.finish();
    bitmap_index = writer.index();
  }

  ASSERT_EQ(65536, bitmap_index.size());

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::doc_id_t expected_index = 0;

    for (irs::doc_id_t expected_doc = irs::doc_limits::min() + 1;;) {
      irs::sparse_bitmap_iterator it{
        stream->dup(),
        { {bitmap_index.data(), bitmap_index.size()}, true },
        65536};
      auto* index = irs::get<irs::value_index>(it);
      ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
      auto* doc = irs::get<irs::document>(it);
      ASSERT_NE(nullptr, doc);
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      auto* cost = irs::get<irs::cost>(it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(65536, cost->estimate());

      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.seek(expected_doc));

      expected_doc += STEP;
      ++expected_index;

      if (expected_doc == (irs::doc_limits::min() + 1)) {
        break;
      }
    }
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::doc_id_t expected_index = 0;

    for (irs::doc_id_t expected_doc = irs::doc_limits::min() + 1;;) {
      irs::sparse_bitmap_iterator it{
        stream->dup(),
        { {bitmap_index.data(), bitmap_index.size()}, true },
        []()->irs::cost::cost_t{ return 65536; }};
      auto* index = irs::get<irs::value_index>(it);
      ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
      auto* doc = irs::get<irs::document>(it);
      ASSERT_NE(nullptr, doc);
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      auto* cost = irs::get<irs::cost>(it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(65536, cost->estimate());

      ASSERT_EQ(expected_doc, it.seek(expected_doc-1));
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.seek(expected_doc-1));

      expected_doc += STEP;
      ++expected_index;

      if (expected_doc == (irs::doc_limits::min() + 1)) {
        break;
      }
    }
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::doc_id_t expected_index = 0;

    for (irs::doc_id_t expected_doc = irs::doc_limits::min() + 1 + STEP;;) {
      irs::sparse_bitmap_iterator it{
        stream->dup(),
        { {bitmap_index.data(), bitmap_index.size()}, true } };
      auto* index = irs::get<irs::value_index>(it);
      ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
      auto* doc = irs::get<irs::document>(it);
      ASSERT_NE(nullptr, doc);
      ASSERT_FALSE(irs::doc_limits::valid(doc->value));
      auto* cost = irs::get<irs::cost>(it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(0, cost->estimate());

      ASSERT_EQ(expected_doc, it.seek(expected_doc-STEP+1));

      expected_doc += STEP;
      ++expected_index;

      if (expected_doc == (irs::doc_limits::min() + 1)) {
        break;
      }
    }
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{
      stream->dup(),
      { {bitmap_index.data(), bitmap_index.size()}, true }};
    irs::sparse_bitmap_iterator it_no_index{
      stream->dup(),
      { {bitmap_index.data(), bitmap_index.size()}, true }};
    auto* index = irs::get<irs::value_index>(it);
    ASSERT_NE(nullptr, index); // index value is unspecified for invalid docs
    auto* doc = irs::get<irs::document>(it);
    ASSERT_NE(nullptr, doc);
    ASSERT_FALSE(irs::doc_limits::valid(doc->value));
    auto* cost = irs::get<irs::cost>(it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(0, cost->estimate());

    ASSERT_TRUE(irs::doc_limits::eof(it.seek(irs::doc_limits::eof())));
    ASSERT_TRUE(irs::doc_limits::eof(it_no_index.seek(irs::doc_limits::eof())));
  }
}

TEST_P(sparse_bitmap_test_case, rw_mixed_seek_random) {
  {
    constexpr seek_type seeks[] {
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 }
    };

    test_rw_seek_random(MIXED, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 160, 31 },
      { 158, 160, 31 },
      { 999, 999, 870 },
      { 998, 999, 870 },
      { 60000, 60000, 1588 },
      { 63000, 63000, 4588 },
      { 62999, 63000, 4588 },
      { 64499, 64499, 6087 },
      { 64500, 196608, 6088 },
      { 64500, 196608, 6088 },
      { 328200, 328200, 71817 },
      { 328199, 328200, 71817 },
      { 328284, 328412, 71901 },
      { 458778, 458778, 77076 },
      { 460563, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random(MIXED, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 160, 31 },
      { 158, 160, 31 },
      { 999, 999, 870 },
      { 999, 999, 870 },
      { 60000, 60000, 1588 },
      { 63000, 63000, 4588 },
      { 63000, 63000, 4588 },
      { 64499, 64499, 6087 },
      { 64500, 196608, 6088 },
      { 64500, 196608, 6088 },
      { 328200, 328200, 71817 },
      { 328200, 328200, 71817 },
      { 328284, 328412, 71901 },
      { 458778, 458778, 77076 },
      { 460563, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random_stateless(MIXED, seeks);
  }
}

TEST_P(sparse_bitmap_test_case, rw_dense) {
  test_rw_next(DENSE);
}

TEST_P(sparse_bitmap_test_case, rw_dense_seek) {
  test_rw_seek(DENSE);
}

TEST_P(sparse_bitmap_test_case, rw_dense_seek_next) {
  test_rw_seek_next(DENSE);
}

TEST_P(sparse_bitmap_test_case, rw_dense_seek_random) {
  {
    constexpr seek_type seeks[] {
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 }
    };

    test_rw_seek_random(DENSE, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 160, 31 },
      { 158, 160, 31 },
      { 999, 999, 870 },
      { 328410, 328412, 6365 },
      { 329490, 329490, 7442 },
      { 333585, 333585, 11537 },
      { 333586, irs::doc_limits::eof(), 0 },
      { 333587, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random(DENSE, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 160, 31 },
      { 158, 160, 31 },
      { 999, 999, 870 },
      { 328410, 328412, 6365 },
      { 329490, 329490, 7442 },
      { 333585, 333585, 11537 },
      { 333586, irs::doc_limits::eof(), 0 },
      { 333587, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random_stateless(DENSE, seeks);
  }
}

TEST_P(sparse_bitmap_test_case, rw_sparse_next) {
  test_rw_next(SPARSE);
}

TEST_P(sparse_bitmap_test_case, rw_sparse_seek) {
  test_rw_seek(SPARSE);
}

TEST_P(sparse_bitmap_test_case, rw_sparse_seek_next) {
  test_rw_seek_next(SPARSE);
}

TEST_P(sparse_bitmap_test_case, rw_sparse_seek_random) {
  {
    constexpr seek_type seeks[] {
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 }
    };

    test_rw_seek_random(SPARSE, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 160, 31 },
      { 1600, 1600, 1454 },
      { 1599, 1600, 1454 },
      { 328007, 328007, 1588 },
      { 328107, 328107, 1688 },
      { 328283, 328283, 1864 },
      { 329489, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random(SPARSE, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 160, 31 },
      { 1600, 1600, 1454 },
      { 1600, 1600, 1454 },
      { 328007, 328007, 1588 },
      { 328107, 328107, 1688 },
      { 328283, 328283, 1864 },
      { 329489, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random_stateless(SPARSE, seeks);
  }
}

TEST_P(sparse_bitmap_test_case, rw_all_next) {
  test_rw_next(ALL);
}

TEST_P(sparse_bitmap_test_case, rw_all_seek) {
  test_rw_seek(ALL);
}

TEST_P(sparse_bitmap_test_case, rw_all_seek_next) {
  test_rw_seek_next(ALL);
}

TEST_P(sparse_bitmap_test_case, rw_all_seek_random) {
  {
    constexpr seek_type seeks[] {
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 }
    };

    test_rw_seek_random(ALL, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 65536, 0 },
      { 131071, 131071, 65535 },
      { 131072, 196608, 65536 },
      { 196612, 196612, 65540 },
      { 196612, 196612, 65540 },
      { 196611, 196612, 65540 },
      { 262143, 262143, 131071 },
      { 262144, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random(ALL, seeks);
  }

  {
    constexpr seek_type seeks[] {
      { 33, 65536, 0 },
      { 131071, 131071, 65535 },
      { 131072, 196608, 65536 },
      { 196612, 196612, 65540 },
      { 196612, 196612, 65540 },
      { 196612, 196612, 65540 },
      { 262143, 262143, 131071 },
      { 262144, irs::doc_limits::eof(), 0 },
      { irs::doc_limits::eof(), irs::doc_limits::eof(), 0 },
    };

    test_rw_seek_random_stateless(ALL, seeks);
  }
}

TEST_P(sparse_bitmap_test_case, insert_erase) {
  {
    auto stream = dir().create("tmp");
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_writer writer(*stream);
    ASSERT_TRUE(writer.erase(42));
    writer.push_back(42);
    ASSERT_TRUE(writer.erase(42));
    writer.push_back(70000); // trigger block flush
    ASSERT_FALSE(writer.erase(42)); // can't erase an element in already flushed block
    writer.finish();
  }

  {
    auto stream = dir().open("tmp", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, stream);

    irs::sparse_bitmap_iterator it{ stream.get(), {}};
    ASSERT_TRUE(it.next());
    ASSERT_EQ(70000, it.value());
    ASSERT_FALSE(it.next());
  }
}

INSTANTIATE_TEST_SUITE_P(
  sparse_bitmap_test,
  sparse_bitmap_test_case,
  ::testing::Values(
    &tests::directory<&tests::memory_directory>,
    &tests::directory<&tests::fs_directory>,
    &tests::directory<&tests::mmap_directory>),
  sparse_bitmap_test_case::to_string
);
