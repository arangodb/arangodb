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

#include <benchmark/benchmark.h>

#include "formats/empty_term_reader.hpp"
#include "search/top_terms_collector.hpp"

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

template<typename T>
class seek_term_iterator : public irs::seek_term_iterator {
 public:
  using iterator_type = const std::tuple<irs::bytes_view, term_meta, T>*;

  seek_term_iterator(iterator_type begin, size_t count)
    : begin_(begin), end_(begin + count), cookie_ptr_(begin) {}

  irs::SeekResult seek_ge(irs::bytes_view) final {
    return irs::SeekResult::NOT_FOUND;
  }

  bool seek(irs::bytes_view) final { return false; }

  irs::seek_cookie::ptr cookie() const final {
    return std::make_unique<seek_ptr>(cookie_ptr_);
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    if (type == irs::type<decltype(meta_)>::id()) {
      return &meta_;
    }
    return nullptr;
  }

  bool next() noexcept final {
    if (begin_ == end_) {
      return false;
    }

    value_ = std::get<0>(*begin_);
    cookie_ptr_ = begin_;
    meta_ = std::get<1>(*begin_);
    ++begin_;
    return true;
  }

  irs::bytes_view value() const noexcept final { return value_; }

  void read() final {}

  irs::doc_iterator::ptr postings(irs::IndexFeatures /*features*/) const final {
    return irs::doc_iterator::empty();
  }

  struct seek_ptr final : irs::seek_cookie {
    explicit seek_ptr(iterator_type ptr) noexcept : ptr(ptr) {}

    irs::attribute* get_mutable(irs::type_info::type_id) noexcept final {
      return nullptr;
    }

    bool IsEqual(const seek_cookie& /*rhs*/) const final { return false; }

    size_t Hash() const final { return 0; }

    iterator_type ptr;
  };

 private:
  term_meta meta_;
  irs::bytes_view value_;
  iterator_type begin_;
  iterator_type end_;
  iterator_type cookie_ptr_;
};

struct SubReader final : irs::SubReader {
  explicit SubReader(size_t num_docs)
    : info{.docs_count = num_docs, .live_docs_count = num_docs} {}

  uint64_t CountMappedMemory() const final { return 0; }

  const irs::SegmentInfo& Meta() const noexcept final { return info; }
  const irs::column_reader* column(std::string_view) const final {
    return nullptr;
  }
  const irs::DocumentMask* docs_mask() const noexcept final { return nullptr; }
  irs::column_iterator::ptr columns() const final {
    return irs::column_iterator::empty();
  }
  const irs::column_reader* column(irs::field_id) const final {
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

void BM_top_term_collector(benchmark::State& state) {
  using collector_type = irs::top_terms_collector<irs::top_term_state<int>>;
  collector_type collector(64);  // same as collector(1)
  irs::empty_term_reader term_reader(42);
  SubReader segment(100);

  std::vector<std::tuple<irs::bytes_view, term_meta, int>> terms(
    state.range(0));
  for (auto& term : terms) {
    auto& key = std::get<2>(term) = ::rand();
    std::get<0>(term) = irs::bytes_view(
      reinterpret_cast<const irs::byte_type*>(&key), sizeof(key));
  }

  for (auto _ : state) {
    seek_term_iterator<int> it(terms.data(), terms.size());
    collector.prepare(segment, term_reader, it);

    while (it.next()) {
      collector.visit(*reinterpret_cast<const int*>(it.value().data()));
    }
  }
}

}  // namespace

BENCHMARK(BM_top_term_collector)->DenseRange(0, 2048, 32);
