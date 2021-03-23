#include <benchmark/benchmark.h>

#include "search/top_terms_collector.hpp"
#include "formats/empty_term_reader.hpp"

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

template<typename T>
class seek_term_iterator final : public irs::seek_term_iterator {
 public:
  typedef const std::tuple<irs::bytes_ref, term_meta, T>* iterator_type;

  seek_term_iterator(iterator_type begin, size_t count)
    : begin_(begin), end_(begin + count), cookie_ptr_(begin) {
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

    value_ = std::get<0>(*begin_);
    cookie_ptr_ = begin_;
    meta_ = std::get<1>(*begin_);
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

void BM_top_term_collector(benchmark::State& state) {
  using collector_type = irs::top_terms_collector<irs::top_term_state<int>>;
  collector_type collector(64); // same as collector(1)
  irs::empty_term_reader term_reader(42);
  sub_reader segment(100);

  std::vector<std::tuple<irs::bytes_ref, term_meta, int>> terms(state.range(0));
  for (auto& term : terms) {
    auto& key = std::get<2>(term) = ::rand();
    std::get<0>(term) = irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(&key), sizeof(key));
  }

  for (auto _ : state) {
    seek_term_iterator<int> it(terms.data(), terms.size());
    collector.prepare(segment, term_reader, it);

    while (it.next()) {
      collector.visit(*reinterpret_cast<const int*>(it.value().c_str()));
    }
  }
}

}

BENCHMARK(BM_top_term_collector)->DenseRange(0, 2048, 32);
