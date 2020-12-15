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

#ifndef IRESEARCH_DLL

#include "tests_shared.hpp"

#include "search/collectors.hpp"
#include "search/top_terms_collector.hpp"
#include "search/multiterm_query.hpp"
#include "search/filter.hpp"
#include "search/scorers.hpp"
#include "formats/empty_term_reader.hpp"
#include "utils/memory.hpp"

namespace {

struct term_meta : irs::term_meta {
  term_meta(uint32_t docs_count = 0, uint32_t freq = 0) noexcept {
    this->docs_count = docs_count;
    this->freq = freq;
  }
};

}

namespace iresearch {

// use base irs::term_meta type for ancestors
template<>
struct type<::term_meta> : type<irs::term_meta> { };

}

namespace {

struct sort : irs::sort {
  static irs::sort::ptr make() {
    return std::make_unique<sort>();
  }

  sort() noexcept : irs::sort(irs::type<sort>::get()) { }

  struct prepared final : irs::sort::prepared {
    struct field_collector final : irs::sort::field_collector {
      uint64_t docs_with_field = 0; // number of documents containing the matched field (possibly without matching terms)
      uint64_t total_term_freq = 0; // number of terms for processed field

      virtual void collect(const irs::sub_reader& segment,
                           const irs::term_reader& field) {
        docs_with_field += field.docs_count();

        auto* freq = irs::get<irs::frequency>(field);

        if (freq) {
          total_term_freq += freq->value;
        }
      }

      virtual void reset() noexcept override {
        docs_with_field = 0;
        total_term_freq = 0;
      }

      virtual void collect(const irs::bytes_ref& in) { }
      virtual void write(irs::data_output& out) const override { }
    };

    struct term_collector final : irs::sort::term_collector {
      uint64_t docs_with_term = 0; // number of documents containing the matched term

      virtual void collect(const irs::sub_reader& segment,
                           const irs::term_reader& field,
                           const irs::attribute_provider& term_attrs) override {
        auto* meta = irs::get<irs::term_meta>(term_attrs);

        if (meta) {
          docs_with_term += meta->docs_count;
        }
      }

      virtual void reset() noexcept override {
        docs_with_term = 0;
      }

      virtual void collect(const irs::bytes_ref& in) override { }
      virtual void write(irs::data_output& out) const override { }
    };

    virtual void collect(
        irs::byte_type* stats,
        const irs::index_reader& index,
        const irs::sort::field_collector* field,
        const irs::sort::term_collector* term) const {
    }

    virtual const irs::flags& features() const {
      return irs::flags::empty_instance();
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector() const {
      return irs::memory::make_unique<field_collector>();
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const {
      return irs::memory::make_unique<term_collector>();
    }

    virtual irs::score_function prepare_scorer(
        const irs::sub_reader& /*segment*/,
        const irs::term_reader& /*field*/,
        const irs::byte_type* /*stats*/,
        irs::byte_type* /*score_buf*/,
        const irs::attribute_provider& /*doc_attrs*/,
        irs::boost_t /*boost*/) const {
      return { nullptr, nullptr };
    }

    virtual bool less(const irs::byte_type* lhs,
                      const irs::byte_type* rhs) const {
      return false;
    }

    virtual std::pair<size_t, size_t> score_size() const {
      return { 0, 0 };
    }

    virtual std::pair<size_t, size_t> stats_size() const {
      return { 0, 0 };
    }
  };

  irs::sort::prepared::ptr prepare() const {
    return irs::memory::make_unique<prepared>();
  }
};

class seek_term_iterator final : public irs::seek_term_iterator {
 public:
  typedef const std::pair<irs::string_ref, term_meta>* iterator_type;

  seek_term_iterator(iterator_type begin, iterator_type end)
    : begin_(begin), end_(end), cookie_ptr_(begin) {
  }

  virtual irs::SeekResult seek_ge(const irs::bytes_ref& value) {
    return irs::SeekResult::NOT_FOUND;
  }

  virtual bool seek(const irs::bytes_ref& value) {
    return false;
  }

  virtual bool seek(
      const irs::bytes_ref& term,
      const seek_cookie& cookie) {

    return true;
  }

  virtual seek_cookie::ptr cookie() const override {
    return irs::memory::make_unique<struct seek_ptr>(cookie_ptr_);
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<decltype(meta_)>::id()) {
      return &meta_;
    }
    return nullptr;
  }

  virtual bool next() noexcept override {
    if (begin_ == end_) {
      return false;
    }

    value_ = irs::ref_cast<irs::byte_type>(begin_->first);
    cookie_ptr_ = begin_;
    meta_ = begin_->second;
    ++begin_;
    return true;
  }

  virtual const irs::bytes_ref& value() const noexcept override {
    return value_;
  }

  virtual void read() override { }

  virtual irs::doc_iterator::ptr postings(const irs::flags& /*features*/) const override {
    return irs::doc_iterator::empty();
  }

  struct seek_ptr : seek_cookie {
    explicit seek_ptr(iterator_type ptr) noexcept
      : ptr(ptr) {
    }

    iterator_type  ptr;
  };

 private:
  term_meta meta_;
  irs::bytes_ref value_;
  iterator_type begin_;
  iterator_type end_;
  iterator_type cookie_ptr_;
}; // term_iterator

struct sub_reader final : irs::sub_reader {
  sub_reader(size_t num_docs)
    : num_docs(num_docs) {
  }

  virtual const irs::column_meta* column(const irs::string_ref& name) const override {
    UNUSED(name);
    return nullptr;
  }
  virtual irs::column_iterator::ptr columns() const override {
    return irs::column_iterator::empty();
  }
  virtual const irs::columnstore_reader::column_reader* column_reader(irs::field_id field) const override {
    UNUSED(field);
    return nullptr;
  }
  virtual uint64_t docs_count() const override {
    return 0;
  }
  virtual irs::doc_iterator::ptr docs_iterator() const override {
    return irs::doc_iterator::empty();
  }
  virtual const irs::term_reader* field(const irs::string_ref& field) const override {
    UNUSED(field);
    return nullptr;
  }
  virtual irs::field_iterator::ptr fields() const override {
    return irs::field_iterator::empty();
  }
  virtual uint64_t live_docs_count() const override {
    return 0;
  }
  virtual const irs::sub_reader& operator[](size_t) const override {
    throw std::out_of_range("index out of range");
  }
  virtual size_t size() const override { return 0; }
  virtual const irs::columnstore_reader::column_reader* sort() const override {
    return nullptr;
  }

  size_t num_docs;
}; // index_reader

struct state {
  struct segment_state {
    const irs::term_reader* field;
    uint32_t docs_count;
    std::vector<const std::pair<irs::string_ref, term_meta>*> cookies;
  };

  std::map<const irs::sub_reader*, segment_state> segments;
};

struct state_visitor {
  void operator()(const irs::sub_reader& segment, const irs::term_reader& field, uint32_t docs) const {
    auto it = expected_state.segments.find(&segment);
    ASSERT_NE(it, expected_state.segments.end());
    ASSERT_EQ(it->second.field, &field);
    ASSERT_EQ(it->second.docs_count, docs);
    expected_cookie = it->second.cookies.begin();
  }

  void operator()(seek_term_iterator::cookie_ptr& cookie) const {
    auto* cookie_impl =  static_cast<const ::seek_term_iterator::seek_ptr*>(cookie.get());

    ASSERT_EQ(*expected_cookie, cookie_impl->ptr);

    ++expected_cookie;
  }

  mutable decltype(state::segment_state::cookies)::const_iterator expected_cookie;
  const struct state& expected_state;
};

}

TEST(top_terms_collector_test, test_top_k) {
  using collector_type = irs::top_terms_collector<irs::top_term_state<irs::byte_type>>;
  collector_type collector(5);

  // segment 0
  irs::empty_term_reader term_reader0(42);
  sub_reader segment0(100);
  const std::pair<irs::string_ref, term_meta> TERMS0[] {
    { "F", { 1, 1 } },
    { "G", { 2, 2 } },
    { "H", { 3, 3 } },
    { "B", { 3, 3 } },
    { "C", { 3, 3 } },
    { "A", { 3, 3 } },
    { "H", { 2, 2 } },
    { "D", { 5, 5 } },
    { "E", { 5, 5 } },
    { "I", { 15, 15 } },
    { "J", { 5, 25 } },
    { "K", { 15, 35 } },
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
  const std::pair<irs::string_ref, term_meta> TERMS1[] {
    { "F", { 1, 1 } },
    { "G", { 2, 2 } },
    { "H", { 3, 3 } },
    { "B", { 3, 3 } },
    { "C", { 3, 3 } },
    { "A", { 3, 3 } },
    { "K", { 15, 35 } },
  };

  {
    seek_term_iterator it(std::begin(TERMS1), std::end(TERMS1));
    collector.prepare(segment1, term_reader1, it);

    while (it.next()) {
      collector.visit(it.value().front());
    }
  }

  std::map<char, state> expected_states {
    { 'J', { { { &segment0, { &term_reader0, 5,  { TERMS0 + 10 } } } } } },
    { 'K', { { { &segment0, { &term_reader0, 15, { TERMS0 + 11 } } }, { &segment1, { &term_reader1, 15, { TERMS1 + 6 } } } } }  },
    { 'I', { { { &segment0, { &term_reader0, 15, { TERMS0 + 9  } } } } }  },
    { 'H', { { { &segment0, { &term_reader0, 5,  { TERMS0 + 2, TERMS0 + 6 } } }, { &segment1, { &term_reader1, 3, { TERMS1 + 2 } } } } }  },
    { 'G', { { { &segment0, { &term_reader0, 2,  { TERMS0 + 1  } } } } }  },
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
  using collector_type = irs::top_terms_collector<irs::top_term_state<irs::byte_type>>;
  collector_type collector(0); // same as collector(1)

  // segment 0
  irs::empty_term_reader term_reader0(42);
  sub_reader segment0(100);
  const std::pair<irs::string_ref, term_meta> TERMS0[] {
    { "F", { 1, 1 } },
    { "G", { 2, 2 } },
    { "H", { 3, 3 } },
    { "B", { 3, 3 } },
    { "C", { 3, 3 } },
    { "A", { 3, 3 } },
    { "H", { 2, 2 } },
    { "D", { 5, 5 } },
    { "E", { 5, 5 } },
    { "I", { 15, 15 } },
    { "J", { 5, 25 } },
    { "K", { 15, 35 } },
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
  const std::pair<irs::string_ref, term_meta> TERMS1[] {
    { "F", { 1, 1 } },
    { "G", { 2, 2 } },
    { "H", { 3, 3 } },
    { "B", { 3, 3 } },
    { "C", { 3, 3 } },
    { "A", { 3, 3 } },
    { "K", { 15, 35 } },
  };

  {
    seek_term_iterator it(std::begin(TERMS1), std::end(TERMS1));
    collector.prepare(segment1, term_reader1, it);

    while (it.next()) {
      collector.visit(it.value().front());
    }
  }

  std::map<char, state> expected_states {
    { 'K', { { { &segment0, { &term_reader0, 15, { TERMS0 + 11 } } }, { &segment1, { &term_reader1, 15, { TERMS1 + 6 } } } } }  },
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

#endif // IRESEARCH_DLL
