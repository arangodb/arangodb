////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "search/top_terms_collector.hpp"

#include "formats/empty_term_reader.hpp"
#include "search/collectors.hpp"
#include "search/filter.hpp"
#include "search/multiterm_query.hpp"
#include "search/scorers.hpp"
#include "tests_shared.hpp"
#include "utils/memory.hpp"

namespace {

struct term_meta : irs::term_meta {
  term_meta(uint32_t docs_count = 0, uint32_t freq = 0) noexcept {
    this->docs_count = docs_count;
    this->freq = freq;
  }
};

}  // namespace

namespace irs {

// use base irs::term_meta type for ancestors
template<>
struct type<::term_meta> : type<irs::term_meta> {};

}  // namespace irs

namespace {

struct sort : irs::Scorer {
  struct field_collector final : irs::FieldCollector {
    uint64_t docs_with_field = 0;  // number of documents containing the matched
                                   // field (possibly without matching terms)
    uint64_t total_term_freq = 0;  // number of terms for processed field

    void collect(const irs::SubReader&, const irs::term_reader& field) final {
      docs_with_field += field.docs_count();

      auto* freq = irs::get<irs::frequency>(field);

      if (freq) {
        total_term_freq += freq->value;
      }
    }

    void reset() noexcept final {
      docs_with_field = 0;
      total_term_freq = 0;
    }

    void collect(irs::bytes_view) final {}
    void write(irs::data_output&) const final {}
  };

  struct term_collector final : irs::TermCollector {
    uint64_t docs_with_term =
      0;  // number of documents containing the matched term

    void collect(const irs::SubReader&, const irs::term_reader&,
                 const irs::attribute_provider& term_attrs) final {
      auto* meta = irs::get<irs::term_meta>(term_attrs);

      if (meta) {
        docs_with_term += meta->docs_count;
      }
    }

    void reset() noexcept final { docs_with_term = 0; }

    void collect(irs::bytes_view) final {}
    void write(irs::data_output&) const final {}
  };

  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::NONE;
  }

  irs::WandWriter::ptr prepare_wand_writer(size_t) const final {
    return nullptr;
  }

  irs::WandSource::ptr prepare_wand_source() const final { return nullptr; }

  irs::FieldCollector::ptr prepare_field_collector() const final {
    return std::make_unique<field_collector>();
  }

  irs::TermCollector::ptr prepare_term_collector() const final {
    return std::make_unique<term_collector>();
  }

  irs::ScoreFunction prepare_scorer(
    const irs::ColumnProvider& /*segment*/, const irs::feature_map_t& /*field*/,
    const irs::byte_type* /*stats*/,
    const irs::attribute_provider& /*doc_attrs*/,
    irs::score_t /*boost*/) const final {
    return irs::ScoreFunction::Default(1);
  }

  std::pair<size_t, size_t> stats_size() const final { return {0, 0}; }
};

class seek_term_iterator : public irs::seek_term_iterator {
 public:
  typedef const std::pair<std::string_view, term_meta>* iterator_type;

  seek_term_iterator(iterator_type begin, iterator_type end)
    : begin_(begin), end_(end), cookie_ptr_(begin) {}

  irs::SeekResult seek_ge(irs::bytes_view) final {
    return irs::SeekResult::NOT_FOUND;
  }

  bool seek(irs::bytes_view) final { return false; }

  irs::seek_cookie::ptr cookie() const final {
    return std::make_unique<struct seek_ptr>(cookie_ptr_);
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    if (type == irs::type<decltype(meta_)>::id()) {
      return &meta_;
    }
    if (type == irs::type<irs::term_attribute>::id()) {
      return &value_;
    }
    return nullptr;
  }

  bool next() noexcept final {
    if (begin_ == end_) {
      return false;
    }

    value_.value = irs::ViewCast<irs::byte_type>(begin_->first);
    cookie_ptr_ = begin_;
    meta_ = begin_->second;
    ++begin_;
    return true;
  }

  irs::bytes_view value() const noexcept final { return value_.value; }

  void read() final {}

  irs::doc_iterator::ptr postings(irs::IndexFeatures /*features*/) const final {
    return irs::doc_iterator::empty();
  }

  struct seek_ptr final : irs::seek_cookie {
    explicit seek_ptr(iterator_type ptr) noexcept : ptr(ptr) {}

    irs::attribute* get_mutable(irs::type_info::type_id) final {
      return nullptr;
    }

    bool IsEqual(const irs::seek_cookie& rhs) const noexcept final {
      return ptr == irs::DownCast<seek_ptr>(rhs).ptr;
    }

    size_t Hash() const noexcept final {
      return std::hash<std::string_view>{}(ptr->first);
    }

    iterator_type ptr;
  };

 private:
  term_meta meta_;
  irs::term_attribute value_;
  iterator_type begin_;
  iterator_type end_;
  iterator_type cookie_ptr_;
};

struct sub_reader final : irs::SubReader {
  explicit sub_reader(size_t num_docs) {
    info.docs_count = num_docs;
    info.live_docs_count = num_docs;
  }

  uint64_t CountMappedMemory() const final { return 0; }

  const irs::SegmentInfo& Meta() const final { return info; }

  const irs::DocumentMask* docs_mask() const final { return nullptr; }

  irs::column_iterator::ptr columns() const final {
    return irs::column_iterator::empty();
  }
  const irs::column_reader* column(irs::field_id) const final {
    return nullptr;
  }
  const irs::column_reader* column(std::string_view) const final {
    return nullptr;
  }
  irs::doc_iterator::ptr docs_iterator() const final {
    return irs::doc_iterator::empty();
  }
  const irs::term_reader* field(std::string_view) const final {
    return nullptr;
  }
  irs::field_iterator::ptr fields() const final {
    return irs::field_iterator::empty();
  }
  const irs::column_reader* sort() const final { return nullptr; }

  irs::SegmentInfo info;
};

struct state {
  struct segment_state {
    const irs::term_reader* field;
    uint32_t docs_count;
    std::vector<const std::pair<std::string_view, term_meta>*> cookies;
  };

  std::map<const irs::SubReader*, segment_state> segments;
};

struct state_visitor {
  void operator()(const irs::SubReader& segment, const irs::term_reader& field,
                  uint32_t docs) const {
    auto it = expected_state.segments.find(&segment);
    ASSERT_NE(it, expected_state.segments.end());
    ASSERT_EQ(it->second.field, &field);
    ASSERT_EQ(it->second.docs_count, docs);
    expected_cookie = it->second.cookies.begin();
  }

  void operator()(irs::seek_cookie::ptr& cookie) const {
    auto* cookie_impl =
      static_cast<const ::seek_term_iterator::seek_ptr*>(cookie.get());

    ASSERT_EQ(*expected_cookie, cookie_impl->ptr);

    ++expected_cookie;
  }

  mutable decltype(state::segment_state::cookies)::const_iterator
    expected_cookie;
  const struct state& expected_state;
};

}  // namespace

TEST(top_terms_collector_test, test_top_k) {
  using collector_type =
    irs::top_terms_collector<irs::top_term_state<irs::byte_type>>;
  collector_type collector(5);

  // segment 0
  irs::empty_term_reader term_reader0(42);
  sub_reader segment0(100);
  const std::pair<std::string_view, term_meta> TERMS0[]{
    {"F", {1, 1}}, {"G", {2, 2}},   {"H", {3, 3}},  {"B", {3, 3}},
    {"C", {3, 3}}, {"A", {3, 3}},   {"H", {2, 2}},  {"D", {5, 5}},
    {"E", {5, 5}}, {"I", {15, 15}}, {"J", {5, 25}}, {"K", {15, 35}},
  };

  {
    seek_term_iterator it(std::begin(TERMS0), std::end(TERMS0));
    collector.prepare(segment0, term_reader0, it);

    while (it.next()) {
      collector.visit(it.value().front());
    }
  }

  // segment 1
  irs::empty_term_reader term_reader1(42);
  sub_reader segment1(100);
  const std::pair<std::string_view, term_meta> TERMS1[]{
    {"F", {1, 1}}, {"G", {2, 2}}, {"H", {3, 3}},   {"B", {3, 3}},
    {"C", {3, 3}}, {"A", {3, 3}}, {"K", {15, 35}},
  };

  {
    seek_term_iterator it(std::begin(TERMS1), std::end(TERMS1));
    collector.prepare(segment1, term_reader1, it);

    while (it.next()) {
      collector.visit(it.value().front());
    }
  }

  std::map<char, state> expected_states{
    {'J', {{{&segment0, {&term_reader0, 5, {TERMS0 + 10}}}}}},
    {'K',
     {{{&segment0, {&term_reader0, 15, {TERMS0 + 11}}},
       {&segment1, {&term_reader1, 15, {TERMS1 + 6}}}}}},
    {'I', {{{&segment0, {&term_reader0, 15, {TERMS0 + 9}}}}}},
    {'H',
     {{{&segment0, {&term_reader0, 5, {TERMS0 + 2, TERMS0 + 6}}},
       {&segment1, {&term_reader1, 3, {TERMS1 + 2}}}}}},
    {'G', {{{&segment0, {&term_reader0, 2, {TERMS0 + 1}}}}}},
  };

  auto visitor = [&expected_states](collector_type::state_type& state) {
    auto it = expected_states.find(char(state.key));
    ASSERT_NE(it, expected_states.end());
    ASSERT_EQ(it->first, state.key);
    ASSERT_EQ(irs::bstring(1, irs::byte_type(it->first)), state.term);

    ::state_visitor state_visitor{{}, it->second};

    state.visit(state_visitor);
  };

  collector.visit(visitor);
}

TEST(top_terms_collector_test, test_top_0) {
  using collector_type =
    irs::top_terms_collector<irs::top_term_state<irs::byte_type>>;
  collector_type collector(0);  // same as collector(1)

  // segment 0
  irs::empty_term_reader term_reader0(42);
  sub_reader segment0(100);
  const std::pair<std::string_view, term_meta> TERMS0[]{
    {"F", {1, 1}}, {"G", {2, 2}},   {"H", {3, 3}},  {"B", {3, 3}},
    {"C", {3, 3}}, {"A", {3, 3}},   {"H", {2, 2}},  {"D", {5, 5}},
    {"E", {5, 5}}, {"I", {15, 15}}, {"J", {5, 25}}, {"K", {15, 35}},
  };

  {
    seek_term_iterator it(std::begin(TERMS0), std::end(TERMS0));
    collector.prepare(segment0, term_reader0, it);

    while (it.next()) {
      collector.visit(it.value().front());
    }
  }

  // segment 1
  irs::empty_term_reader term_reader1(42);
  sub_reader segment1(100);
  const std::pair<std::string_view, term_meta> TERMS1[]{
    {"F", {1, 1}}, {"G", {2, 2}}, {"H", {3, 3}},   {"B", {3, 3}},
    {"C", {3, 3}}, {"A", {3, 3}}, {"K", {15, 35}},
  };

  {
    seek_term_iterator it(std::begin(TERMS1), std::end(TERMS1));
    collector.prepare(segment1, term_reader1, it);

    while (it.next()) {
      collector.visit(it.value().front());
    }
  }

  std::map<char, state> expected_states{
    {'K',
     {{{&segment0, {&term_reader0, 15, {TERMS0 + 11}}},
       {&segment1, {&term_reader1, 15, {TERMS1 + 6}}}}}},
  };

  auto visitor = [&expected_states](collector_type::state_type& state) {
    auto it = expected_states.find(char(state.key));
    ASSERT_NE(it, expected_states.end());
    ASSERT_EQ(it->first, state.key);
    ASSERT_EQ(irs::bstring(1, irs::byte_type(it->first)), state.term);

    ::state_visitor state_visitor{{}, it->second};

    state.visit(state_visitor);
  };

  collector.visit(visitor);
}
