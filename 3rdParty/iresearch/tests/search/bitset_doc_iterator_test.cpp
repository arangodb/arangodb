////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "filter_test_case_base.hpp"
#include "utils/bitset.hpp"
#include "utils/singleton.hpp"
#include "search/bitset_doc_iterator.hpp"

#ifndef IRESEARCH_DLL

TEST(bitset_iterator_test, next) {
  auto& reader = irs::sub_reader::empty();
  auto& filter_attrs = irs::attribute_store::empty_instance();
  irs::order order;

  order.add<tests::sort::custom_sort>(false);

  auto prepared_order = order.prepare();

  {
    // empty bitset (unordered)
    irs::bitset bs;
    irs::bitset_doc_iterator it(reader, filter_attrs, bs, irs::order::prepared::unordered());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(0, cost->estimate());
    ASSERT_FALSE(attrs.get<irs::score>());

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  {
    // empty bitset
    irs::bitset bs;
    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(0, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // non-empty bitset
  {
    irs::bitset bs(13);
    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(0, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // dense bitset
  {
    const size_t size = 73;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    for (auto i = 1; i < size; ++i) {
      ASSERT_TRUE(it.next());
      ASSERT_EQ(i, it.value());
    }
    ASSERT_FALSE(it.next());
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

  }

  // sparse bitset
  {
    const size_t size = 176;
    irs::bitset bs(size);

    // set every second bit
    for (auto i = 0; i < size; ++i) {
      bs.reset(i, i%2);
    }

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size/2, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    for (auto i = 1; i < size; i+=2) {
      ASSERT_TRUE(it.next());
      ASSERT_EQ(i, it.value());
    }
    ASSERT_FALSE(it.next());
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // sparse bitset with dense region
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      ~irs::bitset::word_t(UINT64_C(0x8000000000000000)),
      irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    irs::doc_id_t expected_doc = 64;
    while (it.next()) {
      ASSERT_EQ(expected_doc, it.value());
      ++expected_doc;
    }
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // sparse bitset with sparse region
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x420200A020440480)),
      irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    std::vector<irs::doc_id_t> docs {
      71, 74, 82, 86, 93, 101, 103, 113, 121, 126
    };

    auto begin = docs.begin();
    while (it.next()) {
      ASSERT_EQ(*begin, it.value());
      ++begin;
    }
    ASSERT_EQ(begin, docs.end());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
    ASSERT_FALSE(it.next());
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // sparse bitset
  {
    const size_t size = 189;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x200000000000000))
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_TRUE(it.next());
    ASSERT_EQ(185, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.value());
    ASSERT_FALSE(it.next());
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.value());
  }
}

TEST(bitset_iterator_test, seek) {
  auto& reader = irs::sub_reader::empty();
  auto& filter_attrs = irs::attribute_store::empty_instance();
  irs::order order;

  order.add<tests::sort::custom_sort>(false);

  auto prepared_order = order.prepare();

  {
    // empty bitset
    irs::bitset bs;
    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(0, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.seek(1)));
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // non-empty bitset
  {
    irs::bitset bs(13);
    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(0, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));

    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.seek(1)));
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // dense bitset
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    for (auto expected_doc = 0; expected_doc < size; ++expected_doc) {
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.value());
    }
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // dense bitset, seek backwards
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(size));

    for (ptrdiff_t expected_doc = size-1; expected_doc >= 0; --expected_doc) {
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.value());
    }
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it.value());
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it.seek(irs::type_limits<irs::type_t::doc_id_t>::invalid()));
  }

  // dense bitset, seek after the last document
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(size));
  }

  // dense bitset, seek to the last document
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(size-1, it.seek(size-1));
  }

  // dense bitset, seek to 'eof'
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(irs::type_limits<irs::type_t::doc_id_t>::eof()));
  }

  // dense bitset, seek before the first document
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it.seek(irs::type_limits<irs::type_t::doc_id_t>::invalid()));
  }

  // sparse bitset
  {
    const size_t size = 176;
    irs::bitset bs(size);

    // set every second bit
    for (auto i = 0; i < size; ++i) {
      bs.reset(i, i%2);
    }

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size/2, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    for (auto expected_doc = 1; expected_doc < size; expected_doc+=2) {
      ASSERT_EQ(expected_doc, it.seek(expected_doc-1));
      ASSERT_EQ(expected_doc, it.value());
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.value());
    }
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // sparse bitset, seek backwards
  {
    const size_t size = 176;
    irs::bitset bs(size);

    // set every second bit
    for (auto i = 0; i < size; ++i) {
      bs.reset(i, i%2);
    }

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size/2, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(size));

    for (ptrdiff_t i = size-1; i >= 0; i-=2) {
      ASSERT_EQ(i, it.seek(i));
      ASSERT_EQ(i, it.value());
      ASSERT_EQ(i, it.seek(i-1));
      ASSERT_EQ(i, it.value());
    }
  }

  // sparse bitset with dense region
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      ~irs::bitset::word_t(UINT64_C(0x8000000000000000)),
      irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> seeks {
      { 64, 43 }, { 64, 43 },
      { 64, 64 }, { 68, 68 },
      { 78, 78 },
      { irs::type_limits<irs::type_t::doc_id_t>::eof(), 128 },
      { irs::type_limits<irs::type_t::doc_id_t>::eof(), irs::type_limits<irs::type_t::doc_id_t>::eof() }
    };

    for (auto& seek : seeks) {
      ASSERT_EQ(seek.first, it.seek(seek.second));
      ASSERT_EQ(seek.first, it.value());
    }
  }

  // sparse bitset with sparse region
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x420200A020440480)),
      irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> seeks {
      { 71, 70 }, { 74, 72 }, { 126, 125 },
      { irs::type_limits<irs::type_t::doc_id_t>::eof(), 128 },
      { irs::type_limits<irs::type_t::doc_id_t>::eof(), irs::type_limits<irs::type_t::doc_id_t>::eof() }
    };

    for (auto& seek : seeks) {
      ASSERT_EQ(seek.first, it.seek(seek.second));
      ASSERT_EQ(seek.first, it.value());
    }
  }

  // sparse bitset with sparse region
  {
    const size_t size = 189;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x420200A020440480)),
      irs::bitset::word_t(UINT64_C(0x4440000000000000))
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> seeks {
      { irs::type_limits<irs::type_t::doc_id_t>::eof(), 187 }
    };

    for (auto& seek : seeks) {
      ASSERT_EQ(seek.first, it.seek(seek.second));
      ASSERT_EQ(seek.first, it.value());
    }
  }

  // sparse bitset with sparse region
  {
    const size_t size = 189;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x420200A020440480)),
      irs::bitset::word_t(UINT64_C(0x4440000000000000))
    };
    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> seeks {
      { 186, 186 },
      { irs::type_limits<irs::type_t::doc_id_t>::eof(), 187 }
    };

    for (auto& seek : seeks) {
      ASSERT_EQ(seek.first, it.seek(seek.second));
      ASSERT_EQ(seek.first, it.value());
    }
  }

  // sparse bitset with sparse region
  {
    const size_t size = 189;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x420200A020440480)),
      irs::bitset::word_t(UINT64_C(0x4440000000000000))
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(182 , it.seek(181));
    ASSERT_EQ(186 , it.seek(186));
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(187));
  }

  // sparse bitset
  {
    const size_t size = 189;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x200000000000000))
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(185 , it.seek(2));
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(187));
  }
}

TEST(bitset_iterator_test, seek_next) {
  auto& reader = irs::sub_reader::empty();
  auto& filter_attrs = irs::attribute_store::empty_instance();
  irs::order order;

  order.add<tests::sort::custom_sort>(false);

  auto prepared_order = order.prepare();

  // dense bitset
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    const size_t steps = 5;
    for (auto expected_doc = 0; expected_doc < size; ++expected_doc) {
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.value());

      for (auto j = 1; j <= steps && it.next(); ++j) {
        ASSERT_EQ(expected_doc + j, it.value());
      }
    }
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // dense bitset, seek backwards
  {
    const size_t size = 173;
    irs::bitset bs(size);

    // set all bits
    irs::bitset::word_t data[] {
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0),
      ~irs::bitset::word_t(0)
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(size));

    const size_t steps = 5;
    for (ptrdiff_t expected_doc = size-1; expected_doc >= 0; --expected_doc) {
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(expected_doc, it.value());

      for (auto j = 1; j <= steps && it.next(); ++j) {
        ASSERT_EQ(expected_doc + j, it.value());
      }
    }
    ASSERT_EQ(steps, it.value());
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it.seek(irs::type_limits<irs::type_t::doc_id_t>::invalid()));
  }

  // sparse bitset, seek+next
  {
    const size_t size = 176;
    irs::bitset bs(size);

    // set every second bit
    for (auto i = 0; i < size; ++i) {
      bs.reset(i, i%2);
    }

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size/2, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    size_t steps = 5;
    for (size_t i = 0; i < size; i+=2) {
      const auto expected_doc = irs::type_limits<irs::type_t::doc_id_t>::min() + i;
      ASSERT_EQ(expected_doc, it.seek(i));
      ASSERT_EQ(expected_doc, it.value());

      for (auto j = 1; j <= steps && it.next(); ++j) {
        ASSERT_EQ(expected_doc + 2*j, it.value());
      }
    }
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }

  // sparse bitset, seek backwards+next
  {
    const size_t size = 176;
    irs::bitset bs(size);

    // set every second bit
    for (auto i = 0; i < size; ++i) {
      bs.reset(i, i%2);
    }

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    auto& attrs = it.attributes();
    auto& cost = attrs.get<irs::cost>();
    ASSERT_TRUE(bool(cost));
    ASSERT_EQ(size/2, cost->estimate());
    ASSERT_TRUE(attrs.get<irs::score>());

    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it.seek(size));

    size_t steps = 5;
    for (ptrdiff_t i = size-1; i >= 0; i-=2) {
      ASSERT_EQ(i, it.seek(i));
      ASSERT_EQ(i, it.value());

      for (auto j = 1; j <= steps && it.next(); ++j) {
        ASSERT_EQ(i + 2*j, it.value());
      }
    }
    ASSERT_EQ(2*steps+1, it.value());
  }

  // sparse bitset with sparse region
  {
    const size_t size = 189;
    irs::bitset bs(size);

    // set bits
    irs::bitset::word_t data[] {
      irs::bitset::word_t(0),
      irs::bitset::word_t(UINT64_C(0x420200A020440480)),
      irs::bitset::word_t(UINT64_C(0x4440000000000000))
    };

    bs.memset(data);

    irs::bitset_doc_iterator it(reader, filter_attrs, bs, prepared_order);
    ASSERT_TRUE(!irs::type_limits<irs::type_t::doc_id_t>::valid(it.value()));

    ASSERT_EQ(71 , it.seek(68));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(74, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(82, it.value());
    ASSERT_TRUE(it.next());
    ASSERT_EQ(86, it.value());
    ASSERT_EQ(182 , it.seek(181));
    ASSERT_TRUE(it.next());
    ASSERT_EQ(186, it.value());
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it.value()));
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
