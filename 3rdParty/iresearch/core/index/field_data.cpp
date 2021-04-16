////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "field_data.hpp"
#include "field_meta.hpp"
#include "comparer.hpp"

#include "formats/formats.hpp"

#include "store/directory.hpp"
#include "store/store_utils.hpp"

#include "analysis/analyzer.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"

#include "utils/bit_utils.hpp"
#include "utils/io_utils.hpp"
#include "utils/log.hpp"
#include "utils/lz4compression.hpp"
#include "utils/map_utils.hpp"
#include "utils/memory.hpp"
#include "utils/object_pool.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/bytes_utils.hpp"

#include <set>
#include <algorithm>
#include <cassert>

namespace {

using namespace irs;

const byte_block_pool EMPTY_POOL;

const column_info NORM_COLUMN{
  type<compression::lz4>::get(),
  compression::options(),
  false
};

// -----------------------------------------------------------------------------
// --SECTION--                                                           helpers
// -----------------------------------------------------------------------------

template<typename Stream>
void write_offset(
    posting& p,
    Stream& out,
    const uint32_t base,
    const offset& offs) {
  const uint32_t start_offset = base + offs.start;
  const uint32_t end_offset = base + offs.end;

  assert(start_offset >= p.offs);

  irs::vwrite<uint32_t>(out, start_offset - p.offs);
  irs::vwrite<uint32_t>(out, end_offset - start_offset);

  p.offs = start_offset;
}

template<typename Stream>
void write_prox(
    Stream& out,
    uint32_t prox,
    const irs::payload* pay,
    flags& features
) {
  if (!pay || pay->value.empty()) {
    irs::vwrite<uint32_t>(out, shift_pack_32(prox, false));
  } else {
    irs::vwrite<uint32_t>(out, shift_pack_32(prox, true));
    irs::vwrite<size_t>(out, pay->value.size());
    out.write(pay->value.c_str(), pay->value.size());

    // saw payloads
    features.add<payload>();
  }
}

template<typename Inserter>
FORCE_INLINE void write_cookie(Inserter& out, uint64_t cookie) {
  *out = static_cast<byte_type>(cookie & 0xFF); // offset
  irs::vwrite<uint32_t>(out, static_cast<uint32_t>((cookie >> 8) & 0xFFFFFFFF)); // slice offset
}

FORCE_INLINE uint64_t cookie(size_t slice_offset, size_t offset) noexcept {
  assert(offset <= std::numeric_limits<byte_type>::max());
  return static_cast<uint64_t>(slice_offset) << 8 | static_cast<byte_type>(offset);
}

template<typename Reader>
FORCE_INLINE uint64_t read_cookie(Reader& in) {
  const size_t offset = *in; ++in;
  const size_t slice_offset = irs::vread<uint32_t>(in);
  return cookie(slice_offset, offset);
}

FORCE_INLINE uint64_t cookie(const byte_block_pool::sliced_greedy_inserter& stream) noexcept {
  // we don't span slices over the buffers
  const auto slice_offset = stream.slice_offset();
  return cookie(slice_offset, stream.pool_offset() - slice_offset);
}

FORCE_INLINE byte_block_pool::sliced_greedy_reader greedy_reader(
    const byte_block_pool& pool,
    uint64_t cookie
) noexcept {
  return byte_block_pool::sliced_greedy_reader(
    pool,
    static_cast<size_t>((cookie >> 8) & 0xFFFFFFFF),
    static_cast<size_t>(cookie & 0xFF)
  );
}

FORCE_INLINE byte_block_pool::sliced_greedy_inserter greedy_writer(
    byte_block_pool::inserter& writer,
    uint64_t cookie
) noexcept {
  return byte_block_pool::sliced_greedy_inserter(
    writer,
    static_cast<size_t>((cookie >> 8) & 0xFFFFFFFF),
    static_cast<size_t>(cookie & 0xFF)
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator
///////////////////////////////////////////////////////////////////////////////
template<typename Reader>
class pos_iterator final : public irs::position {
 public:
  pos_iterator()
    : prox_in_(EMPTY_POOL) {
  }

  void clear() noexcept {
    pos_ = 0;
    value_ = pos_limits::invalid();
    offs_.clear();
    pay_.value = bytes_ref::NIL;
  }

  // reset field
  void reset(const flags& features, const frequency& freq) {
    assert(features.check<frequency>());

    freq_ = &freq;

    std::get<attribute_ptr<offset>>(attrs_) = features.check<offset>()
      ? &offs_
      : nullptr;

    std::get<attribute_ptr<payload>>(attrs_) = features.check<payload>()
      ? &pay_
      : nullptr;
  }

  // reset value
  void reset(const Reader& prox) {
    clear();
    prox_in_ = prox;
  }

  virtual attribute* get_mutable(irs::type_info::type_id id) noexcept override final {
    return irs::get_mutable(attrs_, id);
  }

  virtual bool next() override {
    assert(freq_);

    if (pos_ == freq_->value) {
      value_ = irs::pos_limits::eof();

      return false;
    }

    uint32_t pos;

    if (shift_unpack_32(irs::vread<uint32_t>(prox_in_), pos)) {
      const size_t size = irs::vread<size_t>(prox_in_);
      payload_value_.resize(size);
      prox_in_.read(const_cast<byte_type*>(payload_value_.data()), size);
      pay_.value = payload_value_;
    }
    
    value_ += pos;
    assert(pos_limits::valid(value_));

    if (std::get<attribute_ptr<offset>>(attrs_).ptr) {
      offs_.start += irs::vread<uint32_t>(prox_in_);
      offs_.end = offs_.start + irs::vread<uint32_t>(prox_in_);
    }

    ++pos_;

    return true;
  }

  virtual void reset() override {
    assert(false); // unsupported
  }

 private:
  using attributes = std::tuple<attribute_ptr<offset>, attribute_ptr<payload>>;

  Reader prox_in_;
  bstring payload_value_;
  const frequency* freq_{}; // number of term positions in a document
  payload pay_;
  offset offs_;
  attributes attrs_;
  uint32_t pos_{}; // current position
}; // pos_iterator

}

namespace iresearch {
namespace detail {

////////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator
////////////////////////////////////////////////////////////////////////////////
class doc_iterator final : public irs::doc_iterator {
 public:
  doc_iterator() noexcept
   : freq_in_(EMPTY_POOL) {
  }

  // reset field
  void reset(const field_data& field) {
    field_ = &field;
    auto& freq = std::get<attribute_ptr<frequency>>(attrs_);
    auto& pos = std::get<attribute_ptr<position>>(attrs_);
    freq = nullptr;
    pos = nullptr;
    has_cookie_ = false;

    auto& features = field.meta().features;
    if (features.check<frequency>()) {
      freq = &freq_;

      if (features.check<position>()) {
        pos_.reset(features, freq_);
        pos = &pos_;
        has_cookie_ = field.prox_random_access();
      }
    }
  }

  // reset term
  void reset(
      const irs::posting& posting,
      const byte_block_pool::sliced_reader& freq,
      const byte_block_pool::sliced_reader* prox) {
    doc_.value = 0;
    freq_.value = 0;
    cookie_ = 0;
    freq_in_ = freq;
    posting_ = &posting;

    auto& ppos = std::get<attribute_ptr<position>>(attrs_);

    if (ppos.ptr && prox) {
      // reset positions only once,
      // as we need iterator for sequential reads
      pos_.reset(*prox);
    }
  }

  uint64_t cookie() const noexcept {
    return cookie_;
  }

  size_t cost() const noexcept {
    return posting_->size;
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override final {
    return irs::get_mutable(attrs_, type);
  }

  virtual doc_id_t seek(doc_id_t doc) override {
    irs::seek(*this, doc);
    return value();
  }

  virtual doc_id_t value() const noexcept override {
    return doc_.value;
  }

  virtual bool next() override {
    if (freq_in_.eof()) {
      if (!posting_) {
        return false;
      }

      doc_.value = posting_->doc;
      freq_.value = posting_->freq;

      if (has_cookie_) {
        // read last cookie
        cookie_ = *field_->int_writer_->parent().seek(posting_->int_start+3);
      }

      posting_ = nullptr;
    } else {
      if (std::get<attribute_ptr<frequency>>(attrs_).ptr) {
        uint64_t delta;

        if (shift_unpack_64(irs::vread<uint64_t>(freq_in_), delta)) {
          freq_.value = 1U;
        } else {
          freq_.value = irs::vread<uint32_t>(freq_in_);
        }

        assert(delta < doc_limits::eof());
        doc_.value += doc_id_t(delta);

        if (has_cookie_) {
          cookie_ += read_cookie(freq_in_);
        }
      } else {
        doc_.value += irs::vread<uint32_t>(freq_in_);
      }

      assert(doc_.value != posting_->doc);
    }

    pos_.clear();

    return true;
  }

 private:
  using attributes = std::tuple<
    attribute_ptr<frequency>, attribute_ptr<position>>;

  const field_data* field_{};
  uint64_t cookie_{};
  document doc_;
  frequency freq_;
  pos_iterator<byte_block_pool::sliced_reader> pos_;
  byte_block_pool::sliced_reader freq_in_;
  const posting* posting_{};
  attributes attrs_;
  bool has_cookie_{false}; // FIXME remove
};

////////////////////////////////////////////////////////////////////////////////
/// @class sorting_doc_iterator
////////////////////////////////////////////////////////////////////////////////
class sorting_doc_iterator final : public irs::doc_iterator {
 public:
  // reset field
  void reset(const field_data& field) {
    assert(field.prox_random_access());
    byte_pool_ = &field.byte_writer_->parent();

    auto& pfreq = std::get<attribute_ptr<frequency>>(attrs_);
    auto& ppos = std::get<attribute_ptr<position>>(attrs_);
    pfreq = nullptr;
    ppos = nullptr;

    auto& features = field.meta().features;
    if (features.check<frequency>()) {
      pfreq = &freq_;

      if (features.check<position>()) {
        pos_.reset(features, freq_);
        ppos = &pos_;
      }
    }
  }

  // reset iterator,
  // docmap == null -> segment is already sorted
  void reset(detail::doc_iterator& it, const std::vector<doc_id_t>* docmap) {
    const frequency no_frequency;
    const frequency* freq = &no_frequency;

    const auto* freq_attr = irs::get<frequency>(it);
    if (freq_attr) {
      freq = freq_attr;
    }

    docs_.reserve(it.cost());
    docs_.clear();

    if (!docmap) {
      reset_already_sorted(it, *freq);
    } else if (irs::use_dense_sort(it.cost(), docmap->size()-1)) { // -1 for first element
      reset_dense(it, *freq, *docmap);
    } else {
      reset_sparse(it, *freq, *docmap);
    }

    std::get<document>(attrs_).value = irs::doc_limits::invalid();
    freq_.value = 0;
    it_ = docs_.begin();
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override final {
    return irs::get_mutable(attrs_, type);
  }

  virtual doc_id_t seek(doc_id_t doc) noexcept override {
    irs::seek(*this, doc);
    return value();
  }

  virtual doc_id_t value() const noexcept override {
    return std::get<document>(attrs_).value;
  }

  virtual bool next() noexcept override {
    auto& value = std::get<document>(attrs_);

    while (it_ != docs_.end()) {
      if (doc_limits::eof(it_->doc)) {
        // skip invalid docs
        ++it_;
        continue;
      }

      auto& doc = *it_;
      value.value = doc.doc;
      freq_.value = doc.freq;

      if (doc.cookie) {
        // (cookie != 0) -> we have proximity data
        pos_.reset(greedy_reader(*byte_pool_, doc.cookie));
      }

      ++it_;
      return true;

    }

    value.value = doc_limits::eof();
    freq_.value = 0;
    return false;
  }

 private:
  using attributes = std::tuple<
    document, attribute_ptr<frequency>, attribute_ptr<position>>;

  struct doc_entry {
    doc_entry() = default;
    doc_entry(doc_id_t doc, uint32_t freq, uint64_t cookie) noexcept
      : doc(doc), freq(freq), cookie(cookie) {
    }

    doc_id_t doc{ doc_limits::eof() }; // doc_id
    uint32_t freq; // freq
    uint64_t cookie; // prox_cookie
  }; // doc_entry

  void reset_dense(
      detail::doc_iterator& it,
      const frequency& freq,
      const std::vector<doc_id_t>& docmap) {
    assert(!docmap.empty());
    assert(irs::use_dense_sort(it.cost(), docmap.size()-1)); // -1 for first element

    docs_.resize(docmap.size()-1); // -1 for first element

    while (it.next()) {
      assert(it.value() - doc_limits::min() < docmap.size());
      const auto new_doc = docmap[it.value()];

      if (doc_limits::eof(new_doc)) {
        // skip invalid documents
        continue;
      }

      auto& doc = docs_[new_doc - doc_limits::min()];
      doc.doc = new_doc;
      doc.freq = freq.value;
      doc.cookie = it.cookie();
    }
  }

  void reset_sparse(
      detail::doc_iterator& it,
      const frequency& freq,
      const std::vector<doc_id_t>& docmap) {
    assert(!docmap.empty());
    assert(!irs::use_dense_sort(it.cost(), docmap.size()-1)); // -1 for first element

    while (it.next()) {
      assert(it.value() - doc_limits::min() < docmap.size());
      const auto new_doc = docmap[it.value()];

      if (doc_limits::eof(new_doc)) {
        // skip invalid documents
        continue;
      }

      docs_.emplace_back(new_doc, freq.value, it.cookie());
    }

    std::sort(
      docs_.begin(), docs_.end(),
      [](const doc_entry& lhs, const doc_entry& rhs) noexcept {
        return lhs.doc < rhs.doc;
    });
  }

  void reset_already_sorted(detail::doc_iterator& it, const frequency& freq) {
    while (it.next()) {
      docs_.emplace_back(it.value(), freq.value, it.cookie());
    }
  }

  const byte_block_pool* byte_pool_{};
  std::vector<doc_entry>::const_iterator it_;
  std::vector<doc_entry> docs_;
  pos_iterator<byte_block_pool::sliced_greedy_reader> pos_;
  frequency freq_;
  attributes attrs_;
}; // sorting_doc_iterator

////////////////////////////////////////////////////////////////////////////////
/// @class term_iterator
////////////////////////////////////////////////////////////////////////////////
class term_iterator : public irs::term_iterator {
 public:
  explicit term_iterator(
      fields_data::postings_ref_t& postings,
      const doc_map* docmap) noexcept
    : postings_(&postings),
      doc_map_(docmap) {
  }

  void reset(
      const field_data& field,
      const bytes_ref*& min,
      const bytes_ref*& max) {
    field_ = &field;

    doc_itr_.reset(field);
    if (field.prox_random_access()) {
      sorting_doc_itr_.reset(field);
    }

    // reset state
    field_->terms_.get_sorted_postings(*postings_);
    next_ = it_ = postings_->begin();
    end_ = postings_->end();

    max = min = &irs::bytes_ref::NIL;
    if (it_ != end_) {
      min = &(*it_)->term;
      max = &(*(end_ - 1))->term;
    }
  }

  virtual const bytes_ref& value() const noexcept override {
    assert(it_ != end_);
    return (*it_)->term;
  }

  virtual attribute* get_mutable(irs::type_info::type_id) noexcept override {
    return nullptr;
  }

  virtual void read() noexcept override {
    // Does nothing now
  }

  virtual irs::doc_iterator::ptr postings(const flags& /*features*/) const override {
    REGISTER_TIMER_DETAILED();
    assert(it_ != end_);

    return (this->*POSTINGS[size_t(field_->prox_random_access())])(**it_);
  }

  virtual bool next() override {   
    if (next_ == end_) {
      return false;
    }

    it_ = next_;
    ++next_;
    return true;
  }

  const field_meta& meta() const noexcept {
    return field_->meta();
  }

 private:
  typedef irs::doc_iterator::ptr(term_iterator::*postings_f)(const posting&) const;

  static const postings_f POSTINGS[2];

  irs::doc_iterator::ptr postings(const posting& posting) const {
    assert(!doc_map_);

    // where the term data starts
    auto ptr = field_->int_writer_->parent().seek(posting.int_start);
    const auto freq_end = *ptr; ++ptr;
    const auto prox_end = *ptr; ++ptr;
    const auto freq_begin = *ptr; ++ptr;
    const auto prox_begin = *ptr;

    auto& pool = field_->byte_writer_->parent();
    const byte_block_pool::sliced_reader freq(pool, freq_begin, freq_end); // term's frequencies
    const byte_block_pool::sliced_reader prox(pool, prox_begin, prox_end); // term's proximity // TODO: create on demand!!!

    doc_itr_.reset(posting, freq, &prox);
    return memory::to_managed<irs::doc_iterator, false>(&doc_itr_);
  }

  irs::doc_iterator::ptr sort_postings(const posting& posting) const {
    // where the term data starts
    auto ptr = field_->int_writer_->parent().seek(posting.int_start);
    const auto freq_end = *ptr; ++ptr;
    const auto freq_begin = *ptr;

    auto& pool = field_->byte_writer_->parent();
    const byte_block_pool::sliced_reader freq(pool, freq_begin, freq_end); // term's frequencies

    doc_itr_.reset(posting, freq, nullptr);
    sorting_doc_itr_.reset(doc_itr_, doc_map_);
    return memory::to_managed<irs::doc_iterator, false>(&sorting_doc_itr_);
  }

  fields_data::postings_ref_t* postings_{};
  fields_data::postings_ref_t::const_iterator end_;
  fields_data::postings_ref_t::const_iterator next_;
  fields_data::postings_ref_t::const_iterator it_;
  const field_data* field_{};
  const doc_map* doc_map_{};
  mutable detail::doc_iterator doc_itr_;
  mutable detail::sorting_doc_iterator sorting_doc_itr_;
}; // term_iterator

/*static*/ const term_iterator::postings_f term_iterator::POSTINGS[2] {
  &term_iterator::postings,
  &term_iterator::sort_postings
};

////////////////////////////////////////////////////////////////////////////////
/// @class term_reader
////////////////////////////////////////////////////////////////////////////////
class term_reader final : public irs::basic_term_reader,
                          private util::noncopyable {
 public:
  explicit term_reader(
      fields_data::postings_ref_t& postings,
      const doc_map* docmap) noexcept
    : it_(postings, docmap) {
  }


  void reset(const field_data& field) {
    it_.reset(field, min_, max_);
  }

  virtual const irs::bytes_ref& (min)() const noexcept override {
    return *min_;
  }

  virtual const irs::bytes_ref& (max)() const noexcept override {
    return *max_;
  }

  virtual const irs::field_meta& meta() const noexcept override {
    return it_.meta();
  }

  virtual irs::term_iterator::ptr iterator() const noexcept override {
    return memory::to_managed<irs::term_iterator, false>(&it_);
  }

  virtual attribute* get_mutable(irs::type_info::type_id) noexcept override {
    return nullptr;
  }

 private:
  mutable detail::term_iterator it_;
  const irs::bytes_ref* min_{ &irs::bytes_ref::NIL };
  const irs::bytes_ref* max_{ &irs::bytes_ref::NIL };
}; // term_reader

} // detail

// -----------------------------------------------------------------------------
// --SECTION--                                         field_data implementation
// -----------------------------------------------------------------------------

/*static*/ const field_data::process_term_f field_data::TERM_PROCESSING_TABLES[2][2] = {
  // sequential access: [0] - new term, [1] - add term
  {
    &field_data::add_term,
    &field_data::new_term
  },

  // random access: [0] - new term, [1] - add term
  {
    &field_data::add_term_random_access,
    &field_data::new_term_random_access
  }
};

field_data::field_data(
    const string_ref& name,
    byte_block_pool::inserter& byte_writer,
    int_block_pool::inserter& int_writer,
    bool random_access)
  : meta_(name, flags::empty_instance()),
    terms_(*byte_writer),
    byte_writer_(&byte_writer),
    int_writer_(&int_writer),
    proc_table_(TERM_PROCESSING_TABLES[size_t(random_access)]),
    last_doc_(doc_limits::invalid()) {
}

void field_data::reset(doc_id_t doc_id) {
  assert(doc_limits::valid(doc_id));

  if (doc_id == last_doc_) {
    return; // nothing to do
  }

  pos_ = pos_limits::invalid();
  last_pos_ = 0;
  len_ = 0;
  num_overlap_ = 0;
  offs_ = 0;
  last_start_offs_ = 0;
  max_term_freq_ = 0;
  unq_term_cnt_ = 0;
  last_doc_ = doc_id;
}

data_output& field_data::norms(columnstore_writer& writer) const {
  if (!norms_) {
    // FIXME encoder for norms???
    // do not encrypt norms
    auto handle = writer.push_column(NORM_COLUMN);
    norms_ = std::move(handle.second);
    meta_.norm = handle.first;
  }

  return norms_(doc());
}

void field_data::new_term(
    posting& p,
    doc_id_t did,
    const payload* pay,
    const offset* offs
) {
  // where pointers to data starts
  p.int_start = int_writer_->pool_offset();

  const auto freq_start = byte_writer_->alloc_slice(); // pointer to freq stream
  const auto prox_start = byte_writer_->alloc_slice(); // pointer to prox stream
  *int_writer_ = freq_start; // freq stream end
  *int_writer_ = prox_start; // prox stream end
  *int_writer_ = freq_start; // freq stream start
  *int_writer_ = prox_start; // prox stream start

  auto& features = meta_.features;

  p.doc = did;
  if (!features.check<frequency>()) {
    p.doc_code = did;
  } else {
    p.doc_code = uint64_t(did) << 1;
    p.freq = 1;

    if (features.check<position>()) {
      auto& prox_stream_end = *int_writer_->parent().seek(p.int_start + 1);
      byte_block_pool::sliced_inserter prox_out(*byte_writer_, prox_stream_end);

      write_prox(prox_out, pos_, pay, meta_.features);

      if (features.check<offset>()) {
        assert(offs);
        write_offset(p, prox_out, offs_, *offs);
      }

      prox_stream_end = prox_out.pool_offset();
      p.pos = pos_;
    }
  }

  max_term_freq_ = std::max(1U, max_term_freq_);
  ++unq_term_cnt_;
}

void field_data::add_term(
    posting& p,
    doc_id_t did,
    const payload* pay,
    const offset* offs
) {
  auto& features = meta_.features;
  if (!features.check<frequency>()) {
    if (p.doc != did) {
      assert(did > p.doc);

      auto& doc_stream_end = *int_writer_->parent().seek(p.int_start);
      byte_block_pool::sliced_inserter doc_out(*byte_writer_, doc_stream_end);
      irs::vwrite<uint32_t>(doc_out, doc_id_t(p.doc_code));
      doc_stream_end = doc_out.pool_offset();

      p.doc_code = did - p.doc;
      p.doc = did;
      ++unq_term_cnt_;
    }
  } else if (p.doc != did) {
    assert(did > p.doc);

    auto& doc_stream_end = *int_writer_->parent().seek(p.int_start);
    byte_block_pool::sliced_inserter doc_out(*byte_writer_, doc_stream_end);

    if (1U == p.freq) {
      irs::vwrite<uint64_t>(doc_out, p.doc_code | UINT64_C(1));
    } else {
      irs::vwrite<uint64_t>(doc_out, p.doc_code);
      irs::vwrite<uint32_t>(doc_out, p.freq);
    }

    p.doc_code = uint64_t(did - p.doc) << 1;
    p.freq = 1;

    p.doc = did;
    max_term_freq_ = std::max(1U, max_term_freq_);
    ++unq_term_cnt_;

    if (features.check<position>()) {
      auto& prox_stream_end = *int_writer_->parent().seek(p.int_start+1);
      byte_block_pool::sliced_inserter prox_out(*byte_writer_, prox_stream_end);

      write_prox(prox_out, pos_, pay, meta_.features);

      if (features.check<offset>()) {
        assert(offs);
        p.offs = 0; // reset base offset
        write_offset(p, prox_out, offs_, *offs);
      }

      prox_stream_end = prox_out.pool_offset();
      p.pos = pos_;
    }

    doc_stream_end = doc_out.pool_offset();
  } else { // exists in current doc
    max_term_freq_ = std::max(++p.freq, max_term_freq_);
    if (features.check<position>() ) {
      auto& prox_stream_end = *int_writer_->parent().seek(p.int_start+1);
      byte_block_pool::sliced_inserter prox_out(*byte_writer_, prox_stream_end);

      write_prox(prox_out, pos_ - p.pos, pay, meta_.features);

      if (features.check<offset>()) {
        assert(offs);
        write_offset(p, prox_out, offs_, *offs);
      }

      prox_stream_end = prox_out.pool_offset();
      p.pos = pos_;
    }
  }
}

void field_data::new_term_random_access(
    posting& p,
    doc_id_t did,
    const payload* pay,
    const offset* offs
) {
  // where pointers to data starts
  p.int_start = int_writer_->pool_offset();

  const auto freq_start = byte_writer_->alloc_slice(); // pointer to freq stream
  const auto prox_start = byte_writer_->alloc_greedy_slice(); // pointer to prox stream
  *int_writer_ = freq_start; // freq stream end
  *int_writer_ = freq_start; // freq stream start

  const auto cookie = ::cookie(prox_start, 1);
  *int_writer_ = cookie; // end cookie
  *int_writer_ = cookie; // start cookie
  *int_writer_ = 0;      // last start cookie

  auto& features = meta_.features;

  p.doc = did;
  if (!features.check<frequency>()) {
    p.doc_code = did;
  } else {
    p.doc_code = uint64_t(did) << 1;
    p.freq = 1;

    if (features.check<position>()) {
      byte_block_pool::sliced_greedy_inserter prox_out(*byte_writer_, prox_start, 1);

      write_prox(prox_out, pos_, pay, meta_.features);

      if (features.check<offset>()) {
        assert(offs);
        write_offset(p, prox_out, offs_, *offs);
      }

      auto& end_cookie = *int_writer_->parent().seek(p.int_start+2);
      end_cookie = ::cookie(prox_out); // prox stream end cookie

      p.pos = pos_;
    }
  }

  max_term_freq_ = std::max(1U, max_term_freq_);
  ++unq_term_cnt_;
}

void field_data::add_term_random_access(
    posting& p,
    doc_id_t did,
    const payload* pay,
    const offset* offs
) {
  auto& features = meta_.features;
  if (!features.check<frequency>()) {
    if (p.doc != did) {
      assert(did > p.doc);

      auto& doc_stream_end = *int_writer_->parent().seek(p.int_start);
      byte_block_pool::sliced_inserter doc_out(*byte_writer_, doc_stream_end);
      irs::vwrite<uint32_t>(doc_out, doc_id_t(p.doc_code));
      doc_stream_end = doc_out.pool_offset();

      ++p.size;
      p.doc_code = did - p.doc;
      p.doc = did;
      ++unq_term_cnt_;
    }
  } else if (p.doc != did) {
    assert(did > p.doc);

    auto& doc_stream_end = *int_writer_->parent().seek(p.int_start);
    byte_block_pool::sliced_inserter doc_out(*byte_writer_, doc_stream_end);

    if (1U == p.freq) {
      irs::vwrite<uint64_t>(doc_out, p.doc_code | UINT64_C(1));
    } else {
      irs::vwrite<uint64_t>(doc_out, p.doc_code);
      irs::vwrite<uint32_t>(doc_out, p.freq);
    }

    ++p.size;
    p.doc_code = uint64_t(did - p.doc) << 1;
    p.freq = 1;

    p.doc = did;
    max_term_freq_ = std::max(1U, max_term_freq_);
    ++unq_term_cnt_;

    if (features.check<position>()) {
      auto prox_stream_cookie = int_writer_->parent().seek(p.int_start+2);

      auto& end_cookie = *prox_stream_cookie; ++prox_stream_cookie;
      auto& start_cookie = *prox_stream_cookie; ++prox_stream_cookie;
      auto& last_start_cookie = *prox_stream_cookie;

      write_cookie(doc_out, start_cookie - last_start_cookie);
      last_start_cookie = start_cookie; // update previous cookie
      start_cookie = end_cookie; // update start cookie

      auto prox_out = greedy_writer(*byte_writer_, end_cookie);

      write_prox(prox_out, pos_, pay, meta_.features);

      if (features.check<offset>()) {
        assert(offs);
        p.offs = 0; // reset base offset
        write_offset(p, prox_out, offs_, *offs);
      }

      end_cookie = cookie(prox_out);
      p.pos = pos_;
    }

    doc_stream_end = doc_out.pool_offset();
  } else { // exists in current doc
    max_term_freq_ = std::max(++p.freq, max_term_freq_);
    if (features.check<position>() ) {
      // update end cookie
      auto& end_cookie = *int_writer_->parent().seek(p.int_start+2);
      auto prox_out = greedy_writer(*byte_writer_, end_cookie);

      write_prox(prox_out, pos_ - p.pos, pay, meta_.features);

      if (features.check<offset>()) {
        assert(offs);
        write_offset(p, prox_out, offs_, *offs);
      }

      end_cookie = cookie(prox_out);
      p.pos = pos_;
    }
  }
}

bool field_data::invert(
    token_stream& stream, 
    const flags& features, 
    doc_id_t id) {
  assert(id < doc_limits::eof()); // 0-based document id
  REGISTER_TIMER_DETAILED();

  meta_.features |= features; // accumulate field features

  const auto* term = get<term_attribute>(stream);
  const auto* inc = get<increment>(stream);
  const offset* offs = nullptr;
  const payload* pay = nullptr;

  if (!inc) {
    IR_FRMT_ERROR(
      "field '%s' missing required token_stream attribute '%s'",
      meta_.name.c_str(), type<increment>::name().c_str()
    );
    return false;
  }

  if (!term) {
    IR_FRMT_ERROR(
      "field '%s' missing required token_stream attribute '%s'",
      meta_.name.c_str(), type<term_attribute>::name().c_str()
    );
    return false;
  }

  if (meta_.features.check<offset>()) {
    offs = get<offset>(stream);

    if (offs) {
      pay = get<payload>(stream);
    }
  } 

  reset(id); // initialize field_data for the supplied doc_id

  while (stream.next()) {
    pos_ += inc->value;

    if (pos_ < last_pos_) {
      IR_FRMT_ERROR("invalid position %u < %u in field '%s'", pos_, last_pos_, meta_.name.c_str());
      return false;
    }

    if (pos_ >= pos_limits::eof()) {
      IR_FRMT_ERROR("invalid position %u >= %u in field '%s'", pos_, pos_limits::eof(), meta_.name.c_str());
      return false;
    }

    if (0 == inc->value) {
      ++num_overlap_;
    }

    if (offs) {
      const uint32_t start_offset = offs_ + offs->start;
      const uint32_t end_offset = offs_ + offs->end;

      if (start_offset < last_start_offs_ || end_offset < start_offset) {
        IR_FRMT_ERROR("invalid offset start=%u end=%u in field '%s'", start_offset, end_offset, meta_.name.c_str());
        return false;
      }

      last_start_offs_ = start_offset;
    }

    const auto res = terms_.emplace(term->value);

    if (nullptr == res.first) {
      IR_FRMT_ERROR("field '%s' has invalid term of size '" IR_SIZE_T_SPECIFIER "'",
                    meta_.name.c_str(), term->value.size());
      IR_FRMT_TRACE("field '%s' has invalid term '%s'",
                    meta_.name.c_str(), ref_cast<char>(term->value).c_str());
      continue;
    }

    (this->*proc_table_[size_t(res.second)])(*res.first, id, pay, offs);

    if (0 == ++len_) {
      IR_FRMT_ERROR(
        "too many tokens in field '%s', document '" IR_UINT32_T_SPECIFIER "'",
         meta_.name.c_str(), id
      );
      return false;
    }

    last_pos_ = pos_;
  }

  if (offs) {
    offs_ += offs->end;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        fields_data implementation
// -----------------------------------------------------------------------------

fields_data::fields_data(const comparer* comparator /*= nullptr*/)
  : comparator_(comparator),
    byte_writer_(byte_pool_.begin()),
    int_writer_(int_pool_.begin()) {
}

field_data* fields_data::emplace(const hashed_string_ref& name) {
  assert(fields_map_.size() == fields_.size());

  auto it = fields_map_.lazy_emplace(
    name,
    [&name](const fields_map::constructor& ctor) {
      ctor(name.hash(), nullptr);
  });

  if (!it->second) {
    try {
      fields_.emplace_back(name, byte_writer_, int_writer_, (nullptr != comparator_));
    } catch (...) {
      fields_map_.erase(it);
    }

    const_cast<field_data*&>(it->second) = &fields_.back();
  }

  return it->second;
}

void fields_data::flush(field_writer& fw, flush_state& state) {
  REGISTER_TIMER_DETAILED();

  state.features = &features_;

  // sort fields
  sorted_fields_.resize(fields_.size());
  auto begin = sorted_fields_.begin();
  for (auto& entry : fields_) {
    *begin = &entry;
    ++begin;
  }

  std::sort(
    sorted_fields_.begin(), sorted_fields_.end(),
    [](const field_data* lhs, const field_data* rhs) noexcept {
      return lhs->meta().name < rhs->meta().name;
  });

  detail::term_reader terms(sorted_postings_, state.docmap);

  fw.prepare(state);
  for (auto* field : sorted_fields_) {
    auto& meta = field->meta();

    // reset reader
    terms.reset(*field);

    // write inverted data
    auto it = terms.iterator();
    fw.write(meta.name, meta.norm, meta.features, *it);
  }

  fw.end();
}

void fields_data::reset() noexcept {
  byte_writer_ = byte_pool_.begin(); // reset position pointer to start of pool
  features_.clear();
  fields_.clear();
  fields_map_.clear();
  int_writer_ = int_pool_.begin(); // reset position pointer to start of pool
}

// use base irs::position type for ancestors
template<typename Reader>
struct type<::pos_iterator<Reader>> : type<irs::position> { };

} // ROOT
