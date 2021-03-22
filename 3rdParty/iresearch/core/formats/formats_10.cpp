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
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <deque>
#include <list>
#include <numeric>

#include "shared.hpp"

#include "skip_list.hpp"

#include "formats_10.hpp"
#include "formats_10_attributes.hpp"
#include "formats_burst_trie.hpp"
#include "format_utils.hpp"

#include "analysis/token_attributes.hpp"

#include "index/field_meta.hpp"
#include "index/file_names.hpp"
#include "index/index_reader.hpp"
#include "index/index_meta.hpp"

#include "search/cost.hpp"
#include "search/score.hpp"

#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"

#include "utils/bit_packing.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitset.hpp"
#include "utils/lz4compression.hpp"
#include "utils/encryption.hpp"
#include "utils/frozen_attributes.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/log.hpp"
#include "utils/memory.hpp"
#include "utils/memory_pool.hpp"
#include "utils/noncopyable.hpp"
#include "utils/object_pool.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/std.hpp"

#ifdef IRESEARCH_SSE2
#include "store/store_utils_simd.hpp"
#endif

#if defined(_MSC_VER)
  #pragma warning(disable : 4351)
#endif

namespace {

using namespace irs;

// name of the module holding different formats
constexpr string_ref MODULE_NAME = "10";

struct format_traits {
  static constexpr uint32_t BLOCK_SIZE = 128;

  FORCE_INLINE static void write_block(
      index_output& out, const uint32_t* in, uint32_t* buf) {
    encode::bitpack::write_block(out, in, buf);
  }

  FORCE_INLINE static void read_block(
      index_input& in, uint32_t* buf,  uint32_t* out) {
    encode::bitpack::read_block(in, buf, out);
  }

  FORCE_INLINE static void skip_block(index_input& in) {
    encode::bitpack::skip_block32(in, BLOCK_SIZE);
  }
}; // format_traits

bytes_ref DUMMY; // placeholder for visiting logic in columnstore

class noop_compressor final : compression::compressor {
 public:
  static compression::compressor::ptr make() {
    typedef compression::compressor::ptr ptr;
    static noop_compressor INSTANCE;
    return ptr(ptr(), &INSTANCE);
  }

  virtual bytes_ref compress(byte_type* in, size_t size, bstring& /*buf*/) override {
    return bytes_ref(in, size);
  }

  virtual void flush(data_output& /*out*/) override { }

 private:
  noop_compressor() = default;
}; // noop_compressor

// ----------------------------------------------------------------------------
// --SECTION--                                                         features
// ----------------------------------------------------------------------------

// compiled features supported by current format
class features {
 public:
  enum Mask : uint32_t {
    DOCS = 0,
    FREQ = 1,
    POS = 2,
    OFFS = 4,
    PAY = 8,
  };

  features() = default;

  explicit features(const irs::flags& in) noexcept {
    irs::set_bit<0>(in.check<irs::frequency>(), mask_);
    irs::set_bit<1>(in.check<irs::position>(), mask_);
    irs::set_bit<2>(in.check<irs::offset>(), mask_);
    irs::set_bit<3>(in.check<irs::payload>(), mask_);
  }

  features operator&(const irs::flags& in) const noexcept {
    return features(*this) &= in;
  }

  features& operator&=(const irs::flags& in) noexcept {
    irs::unset_bit<0>(!in.check<irs::frequency>(), mask_);
    irs::unset_bit<1>(!in.check<irs::position>(), mask_);
    irs::unset_bit<2>(!in.check<irs::offset>(), mask_);
    irs::unset_bit<3>(!in.check<irs::payload>(), mask_);
    return *this;
  }

  bool freq() const noexcept { return irs::check_bit<0>(mask_); }
  bool position() const noexcept { return irs::check_bit<1>(mask_); }
  bool offset() const noexcept { return irs::check_bit<2>(mask_); }
  bool payload() const noexcept { return irs::check_bit<3>(mask_); }
  operator Mask() const noexcept { return static_cast<Mask>(mask_); }

  bool any(Mask mask) const noexcept {
    return Mask(0) != (mask_ & mask);
  }

  bool all(Mask mask) const noexcept {
    return mask != (mask_ & mask);
  }

 private:
  irs::byte_type mask_{};
}; // features

ENABLE_BITMASK_ENUM(features::Mask);

// ----------------------------------------------------------------------------
// --SECTION--                                             forward declarations
// ----------------------------------------------------------------------------

template<typename T, typename M>
std::string file_name(const M& meta);

// ----------------------------------------------------------------------------
// --SECTION--                                                 helper functions
// ----------------------------------------------------------------------------

inline void prepare_output(
    std::string& str,
    index_output::ptr& out,
    const flush_state& state,
    const string_ref& ext,
    const string_ref& format,
    const int32_t version) {
  assert(!out);
  file_name(str, state.name, ext);
  out = state.dir->create(str);

  if (!out) {
    throw io_error(string_utils::to_string(
      "failed to create file, path: %s",
      str.c_str()));
  }

  format_utils::write_header(*out, format, version);
}

inline void prepare_input(
    std::string& str,
    index_input::ptr& in,
    IOAdvice advice,
    const reader_state& state,
    const string_ref& ext,
    const string_ref& format,
    const int32_t min_ver,
    const int32_t max_ver ) {
  assert(!in);
  file_name(str, state.meta->name, ext);
  in = state.dir->open(str, advice);

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      str.c_str()));
  }

  format_utils::check_header(*in, format, min_ver, max_ver);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                  postings_writer
// ----------------------------------------------------------------------------
//
// Assume that doc_count = 28, skip_n = skip_0 = 12
//
//  |       block#0       | |      block#1        | |vInts|
//  d d d d d d d d d d d d d d d d d d d d d d d d d d d d (posting list)
//                          ^                       ^       (level 0 skip point)
//
// ----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @class postings_writer_base
//////////////////////////////////////////////////////////////////////////////
class postings_writer_base : public irs::postings_writer {
 public:
  static constexpr int32_t TERMS_FORMAT_MIN = 0;
  static constexpr int32_t TERMS_FORMAT_MAX = TERMS_FORMAT_MIN;

  static constexpr int32_t FORMAT_MIN = 0;
  // positions are stored one based (if first osition is 1 first offset is 0)
  // This forces reader to adjust first read position of every document additionally to
  // stored increment. Or incorrect positions will be read - 1 2 3 will be stored
  // (offsets 0 1 1) but 0 1 2 will be read. At least this will lead to incorrect results in
  // by_same_positions filter if searching for position 1
  static constexpr int32_t FORMAT_POSITIONS_ONEBASED = FORMAT_MIN;
  // positions are stored one based, sse used
  static constexpr int32_t FORMAT_SSE_POSITIONS_ONEBASED = FORMAT_POSITIONS_ONEBASED + 1;

  // positions are stored zero based
  // if first position is 1 first offset is also 1
  // so no need to adjust position while reading first
  // position for document, always just increment from previous pos
  static constexpr int32_t FORMAT_POSITIONS_ZEROBASED = FORMAT_SSE_POSITIONS_ONEBASED + 1;
  // positions are stored zero based, sse used
  static constexpr int32_t FORMAT_SSE_POSITIONS_ZEROBASED = FORMAT_POSITIONS_ZEROBASED + 1;
  static constexpr int32_t FORMAT_MAX = FORMAT_SSE_POSITIONS_ZEROBASED;

  static constexpr uint32_t MAX_SKIP_LEVELS = 10;
  static constexpr uint32_t BLOCK_SIZE = 128;
  static constexpr uint32_t SKIP_N = 8;

  static constexpr string_ref DOC_FORMAT_NAME = "iresearch_10_postings_documents";
  static constexpr string_ref DOC_EXT = "doc";
  static constexpr string_ref POS_FORMAT_NAME = "iresearch_10_postings_positions";
  static constexpr string_ref POS_EXT = "pos";
  static constexpr string_ref PAY_FORMAT_NAME = "iresearch_10_postings_payloads";
  static constexpr string_ref PAY_EXT = "pay";
  static constexpr string_ref TERMS_FORMAT_NAME = "iresearch_10_postings_terms";

 protected:
  postings_writer_base(int32_t postings_format_version, int32_t terms_format_version)
    : skip_(BLOCK_SIZE, SKIP_N),
      postings_format_version_(postings_format_version),
      terms_format_version_(terms_format_version),
      pos_min_(postings_format_version_ >= FORMAT_POSITIONS_ZEROBASED ?   // first position offsets now is format dependent
               pos_limits::invalid(): pos_limits::min()) {
    assert(postings_format_version >= FORMAT_MIN && postings_format_version <= FORMAT_MAX);
    assert(terms_format_version >= TERMS_FORMAT_MIN && terms_format_version <= TERMS_FORMAT_MAX);
  }

 public:
  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::type<version10::documents>::id() == type ? &docs_ : nullptr;
  }

  virtual void begin_field(const irs::flags& field) final {
    features_ = ::features(field);
    docs_.value.clear();
    last_state_.clear();
  }

  virtual void begin_block() final {
    // clear state in order to write
    // absolute address of the first
    // entry in the block
    last_state_.clear();
  }

  virtual void prepare(index_output& out, const irs::flush_state& state) final;
  virtual void encode(data_output& out, const irs::term_meta& attrs) final;
  virtual void end() final;

 protected:
  virtual void release(irs::term_meta *meta) noexcept final {
    auto* state = static_cast<version10::term_meta*>(meta);
    assert(state);

    alloc_.destroy(state);
    alloc_.deallocate(state);
  }

  struct stream {
    void reset() noexcept {
      start = end = 0;
    }

    uint64_t skip_ptr[MAX_SKIP_LEVELS]{}; // skip data
    uint64_t start{};                     // start position of block
    uint64_t end{};                       // end position of block
  }; // stream

  struct doc_stream : stream {
    bool full() const noexcept {
      return delta == std::end(deltas);
    }

    bool empty() const noexcept {
      return delta == deltas;
    }

    void push(doc_id_t doc, uint32_t freq) noexcept {
      *this->delta++ = doc - last;
      *this->freq++ = freq;
      last = doc;
    }

    void reset() noexcept {
      stream::reset();
      delta = deltas;
      freq = freqs;
      last = doc_limits::invalid();
      block_last = doc_limits::invalid();
    }

    doc_id_t skip_doc[MAX_SKIP_LEVELS]{};
    doc_id_t deltas[BLOCK_SIZE]{}; // document deltas
    uint32_t freqs[BLOCK_SIZE]{};
    doc_id_t* delta{ deltas };
    uint32_t* freq{ freqs };
    doc_id_t last{ doc_limits::invalid() }; // last buffered document id
    doc_id_t block_last{ doc_limits::invalid() }; // last document id in a block
  }; // doc_stream

  struct pos_stream : stream {
    using ptr = std::unique_ptr<pos_stream>;

    bool full() const { return BLOCK_SIZE == size; }
    void next(uint32_t pos) { last = pos, ++size; }
    void pos(uint32_t pos) {
        buf[size] = pos;
    }

    void reset() noexcept {
      stream::reset();
      last = 0;
      block_last = 0;
      size = 0;
    }

    uint32_t buf[BLOCK_SIZE]{}; // buffer to store position deltas
    uint32_t last{};            // last buffered position
    uint32_t block_last{};      // last position in a block
    uint32_t size{};            // number of buffered elements
  }; // pos_stream

  struct pay_stream : stream {
    using ptr = std::unique_ptr<pay_stream>;

    void push_payload(uint32_t i, const bytes_ref& pay) {
      if (!pay.empty()) {
        pay_buf_.append(pay.c_str(), pay.size());
      }
      pay_sizes[i] = static_cast<uint32_t>(pay.size());
    }

    void push_offset(uint32_t i, uint32_t start, uint32_t end) {
      assert(start >= last && start <= end);

      offs_start_buf[i] = start - last;
      offs_len_buf[i] = end - start;
      last = start;
    }

    void reset() noexcept {
      stream::reset();
      pay_buf_.clear();
      block_last = 0;
      last = 0;
    }

    bstring pay_buf_;                       // buffer for payload
    uint32_t pay_sizes[BLOCK_SIZE]{};       // buffer to store payloads sizes
    uint32_t offs_start_buf[BLOCK_SIZE]{};  // buffer to store start offsets
    uint32_t offs_len_buf[BLOCK_SIZE]{};    // buffer to store offset lengths
    size_t block_last{};                    // last payload buffer length in a block
    uint32_t last{};                        // last start offset
  }; // pay_stream

  void begin_term();
  void end_term(version10::term_meta& meta, const uint32_t* tfreq);

  template<typename FormatTraits>
  void begin_doc(doc_id_t id, const frequency* freq);
  template<typename FormatTraits>
  void add_position(uint32_t pos, const offset* offs, const payload* pay);
  void end_doc();

  void write_skip(size_t level, index_output& out);

  memory::memory_pool<> meta_pool_;
  memory::memory_pool_allocator<version10::term_meta, decltype(meta_pool_)> alloc_{ meta_pool_ };
  skip_writer skip_;
  version10::term_meta last_state_; // last final term state
  version10::documents docs_;       // bit set of all processed documents
  features features_;               // features supported by current field
  index_output::ptr doc_out_;       // postings (doc + freq)
  index_output::ptr pos_out_;       // positions
  index_output::ptr pay_out_;       // payload (payl + offs)
  uint32_t buf_[BLOCK_SIZE];        // buffer for encoding (worst case)
  doc_stream doc_;                  // document stream
  pos_stream::ptr pos_;             // proximity stream
  pay_stream::ptr pay_;             // payloads and offsets stream
  size_t docs_count_{};             // number of processed documents
  const int32_t postings_format_version_;
  const int32_t terms_format_version_;
  uint32_t pos_min_; // initial base value for writing positions offsets
};

void postings_writer_base::prepare(index_output& out, const irs::flush_state& state) {
  assert(state.dir);
  assert(!state.name.null());

  // reset writer state
  docs_count_ = 0;

  std::string name;

  // prepare document stream
  prepare_output(name, doc_out_, state, DOC_EXT, DOC_FORMAT_NAME, postings_format_version_);

  auto& features = *state.features;

  if (features.check<position>()) {
    // prepare proximity stream
    if (!pos_) {
      pos_ = memory::make_unique<pos_stream>();
    }

    pos_->reset();
    prepare_output(name, pos_out_, state, POS_EXT, POS_FORMAT_NAME, postings_format_version_);

    if (features.check<payload>() || features.check<offset>()) {
      // prepare payload stream
      if (!pay_) {
        pay_ = memory::make_unique<pay_stream>();
      }

      pay_->reset();
      prepare_output(name, pay_out_, state, PAY_EXT, PAY_FORMAT_NAME, postings_format_version_);
    }
  }

  skip_.prepare(
    MAX_SKIP_LEVELS,
    state.doc_count,
    [this] (size_t i, index_output& out) { write_skip(i, out); },
    directory_utils::get_allocator(*state.dir));

  format_utils::write_header(out, TERMS_FORMAT_NAME, terms_format_version_); // write postings format name
  out.write_vint(BLOCK_SIZE); // write postings block size

  // prepare documents bitset
  docs_.value.reset(doc_limits::min() + state.doc_count);
}

void postings_writer_base::encode(
    data_output& out,
    const irs::term_meta& state) {
  const auto& meta = static_cast<const version10::term_meta&>(state);

  out.write_vint(meta.docs_count);
  if (meta.freq != std::numeric_limits<uint32_t>::max()) {
    assert(meta.freq >= meta.docs_count);
    out.write_vint(meta.freq - meta.docs_count);
  }

  out.write_vlong(meta.doc_start - last_state_.doc_start);
  if (features_.position()) {
    out.write_vlong(meta.pos_start - last_state_.pos_start);
    if (type_limits<type_t::address_t>::valid(meta.pos_end)) {
      out.write_vlong(meta.pos_end);
    }
    if (features_.any(features::OFFS | features::PAY)) {
      out.write_vlong(meta.pay_start - last_state_.pay_start);
    }
  }

  if (1U == meta.docs_count || meta.docs_count > BLOCK_SIZE) {
    out.write_vlong(meta.e_skip_start);
  }

  last_state_ = meta;
}

void postings_writer_base::end() {
  format_utils::write_footer(*doc_out_);
  doc_out_.reset(); // ensure stream is closed

  if (pos_ && pos_out_) { // check both for the case where the writer is reused
    format_utils::write_footer(*pos_out_);
    pos_out_.reset(); // ensure stream is closed
  }

  if (pay_ && pay_out_) { // check both for the case where the writer is reused
    format_utils::write_footer(*pay_out_);
    pay_out_.reset(); // ensure stream is closed
  }
}

void postings_writer_base::write_skip(size_t level, index_output& out) {
  const doc_id_t doc_delta = doc_.block_last; //- doc_.skip_doc[level];
  const uint64_t doc_ptr = doc_out_->file_pointer();

  out.write_vint(doc_delta);
  out.write_vlong(doc_ptr - doc_.skip_ptr[level]);

  doc_.skip_doc[level] = doc_.block_last;
  doc_.skip_ptr[level] = doc_ptr;

  if (features_.position()) {
    assert(pos_);

    const uint64_t pos_ptr = pos_out_->file_pointer();

    out.write_vint(pos_->block_last);
    out.write_vlong(pos_ptr - pos_->skip_ptr[level]);

    pos_->skip_ptr[level] = pos_ptr;

    if (features_.any(features::OFFS | features::PAY)) {
      assert(pay_ && pay_out_);

      if (features_.payload()) {
        out.write_vint(static_cast<uint32_t>(pay_->block_last));
      }

      const uint64_t pay_ptr = pay_out_->file_pointer();

      out.write_vlong(pay_ptr - pay_->skip_ptr[level]);
      pay_->skip_ptr[level] = pay_ptr;
    }
  }
}

void postings_writer_base::begin_term() {
  doc_.start = doc_out_->file_pointer();
  std::fill_n(doc_.skip_ptr, MAX_SKIP_LEVELS, doc_.start);
  if (features_.position()) {
    assert(pos_ && pos_out_);
    pos_->start = pos_out_->file_pointer();
    std::fill_n(pos_->skip_ptr, MAX_SKIP_LEVELS, pos_->start);
    if (features_.any(features::OFFS | features::PAY)) {
      assert(pay_ && pay_out_);
      pay_->start = pay_out_->file_pointer();
      std::fill_n(pay_->skip_ptr, MAX_SKIP_LEVELS, pay_->start);
    }
  }

  doc_.last = doc_limits::min(); // for proper delta of 1st id
  doc_.block_last = doc_limits::invalid();
  skip_.reset();
}

void postings_writer_base::end_doc() {
  if (doc_.full()) {
    doc_.block_last = doc_.last;
    doc_.end = doc_out_->file_pointer();
    if (features_.position()) {
      assert(pos_ && pos_out_);
      pos_->end = pos_out_->file_pointer();
      // documents stream is full, but positions stream is not
      // save number of positions to skip before the next block
      pos_->block_last = pos_->size;
      if (features_.any(features::OFFS | features::PAY)) {
        assert(pay_ && pay_out_);
        pay_->end = pay_out_->file_pointer();
        pay_->block_last = pay_->pay_buf_.size();
      }
    }

    doc_.delta = doc_.deltas;
    doc_.freq = doc_.freqs;
  }
}

void postings_writer_base::end_term(version10::term_meta& meta, const uint32_t* tfreq) {
  if (docs_count_ == 0) {
    return; // no documents to write
  }

  if (1 == meta.docs_count) {
    meta.e_single_doc = doc_.deltas[0];
  } else {
    // write remaining documents using
    // variable length encoding
    auto& out = *doc_out_;
    auto* doc_delta = doc_.deltas;

    if (features_.freq()) {
      auto* doc_freq = doc_.freqs;
      for (; doc_delta < doc_.delta; ++doc_delta) {
        const uint32_t freq = *doc_freq;

        if (1 == freq) {
          out.write_vint(shift_pack_32(*doc_delta, true));
        } else {
          out.write_vint(shift_pack_32(*doc_delta, false));
          out.write_vint(freq);
        }

        ++doc_freq;
      }
    } else {
      for (; doc_delta < doc_.delta; ++doc_delta) {
        out.write_vint(*doc_delta);
      }
    }
  }

  meta.pos_end = type_limits<type_t::address_t>::invalid();

  // write remaining position using
  // variable length encoding
  if (features_.position()) {
    assert(pos_ && pos_out_);

    if (meta.freq > BLOCK_SIZE) {
      meta.pos_end = pos_out_->file_pointer() - pos_->start;
    }

    if (pos_->size > 0) {
      data_output& out = *pos_out_;
      uint32_t last_pay_size = std::numeric_limits<uint32_t>::max();
      uint32_t last_offs_len = std::numeric_limits<uint32_t>::max();
      uint32_t pay_buf_start = 0;
      for (uint32_t i = 0; i < pos_->size; ++i) {
        const uint32_t pos_delta = pos_->buf[i];
        if (features_.payload()) {
          assert(pay_ && pay_out_);

          const uint32_t size = pay_->pay_sizes[i];
          if (last_pay_size != size) {
            last_pay_size = size;
            out.write_vint(shift_pack_32(pos_delta, true));
            out.write_vint(size);
          } else {
            out.write_vint(shift_pack_32(pos_delta, false));
          }

          if (size != 0) {
            out.write_bytes(pay_->pay_buf_.c_str() + pay_buf_start, size);
            pay_buf_start += size;
          }
        } else {
          out.write_vint(pos_delta);
        }

        if (features_.offset()) {
          assert(pay_ && pay_out_);

          const uint32_t pay_offs_delta = pay_->offs_start_buf[i];
          const uint32_t len = pay_->offs_len_buf[i];
          if (len == last_offs_len) {
            out.write_vint(shift_pack_32(pay_offs_delta, false));
          } else {
            out.write_vint(shift_pack_32(pay_offs_delta, true));
            out.write_vint(len);
            last_offs_len = len;
          }
        }
      }

      if (features_.payload()) {
        assert(pay_ && pay_out_);
        pay_->pay_buf_.clear();
      }
    }
  }

  if (!tfreq) {
    meta.freq = std::numeric_limits<uint32_t>::max();
  }

  // if we have flushed at least
  // one block there was buffered
  // skip data, so we need to flush it*/
  if (docs_count_ > BLOCK_SIZE) {
    //const uint64_t start = doc.out->file_pointer();
    meta.e_skip_start = doc_out_->file_pointer() - doc_.start;
    skip_.flush(*doc_out_);
  }

  docs_count_ = 0;
  doc_.delta = doc_.deltas;
  doc_.freq = doc_.freqs;
  doc_.last = 0;
  meta.doc_start = doc_.start;

  if (pos_) {
    pos_->size = 0;
    meta.pos_start = pos_->start;
  }

  if (pay_) {
    //pay_->buf_size = 0;
    pay_->pay_buf_.clear();
    pay_->last = 0;
    meta.pay_start = pay_->start;
  }
}

template<typename FormatTraits>
void postings_writer_base::begin_doc(doc_id_t id, const frequency* freq) {
  if (doc_limits::valid(doc_.block_last) && doc_.empty()) {
    skip_.skip(docs_count_);
  }

  if (id < doc_.last) {
    throw index_error(string_utils::to_string(
      "while beginning doc_ in postings_writer, error: docs out of order '%d' < '%d'",
      id, doc_.last));
  }

  doc_.push(id, freq ? freq->value : 0);

  if (doc_.full()) {
    FormatTraits::write_block(*doc_out_, doc_.deltas, buf_);

    if (freq) {
      FormatTraits::write_block(*doc_out_, doc_.freqs, buf_);
    }
  }
  if (pos_) {
    pos_->last = pos_min_;
  }

  if (pay_) {
    pay_->last = 0;
  }

  ++docs_count_;
}

template<typename FormatTraits>
void postings_writer_base::add_position(uint32_t pos, const offset* offs, const payload* pay) {
  assert(!offs || offs->start <= offs->end);
  assert(features_.position() && pos_ && pos_out_); /* at least positions stream should be created */

  pos_->pos(pos - pos_->last);

  if (pay) {
    pay_->push_payload(pos_->size, pay->value);
  }

  if (offs) {
    pay_->push_offset(pos_->size, offs->start, offs->end);
  }

  pos_->next(pos);

  if (pos_->full()) {
    FormatTraits::write_block(*pos_out_, pos_->buf, buf_);
    pos_->size = 0;

    if (pay) {
      assert(features_.payload() && pay_ && pay_out_);
      auto& pay_buf = pay_->pay_buf_;

      pay_out_->write_vint(static_cast<uint32_t>(pay_buf.size()));
      if (!pay_buf.empty()) {
        FormatTraits::write_block(*pay_out_, pay_->pay_sizes, buf_);
        pay_out_->write_bytes(pay_buf.c_str(), pay_buf.size());
        pay_buf.clear();
      }
    }

    if (offs) {
      assert(features_.offset() && pay_ && pay_out_);
      FormatTraits::write_block(*pay_out_, pay_->offs_start_buf, buf_);
      FormatTraits::write_block(*pay_out_, pay_->offs_len_buf, buf_);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @class postings_writer
//////////////////////////////////////////////////////////////////////////////
template<typename FormatTraits, bool VolatileAttributes>
class postings_writer final: public postings_writer_base {
 public:
  explicit postings_writer(int32_t version)
    : postings_writer_base(version, TERMS_FORMAT_MAX) {
  }

  virtual irs::postings_writer::state write(irs::doc_iterator& docs) override;

 private:
  void refresh(attribute_provider& attrs) noexcept {
    pos_ = irs::position::empty();
    offs_ = nullptr;
    pay_ = nullptr;

    freq_ = irs::get<frequency>(attrs);
    if (freq_) {
      auto* pos = irs::get_mutable<irs::position>(&attrs);
      if (pos) {
        pos_ = pos;
        offs_ = irs::get<irs::offset>(*pos_);
        pay_ = irs::get<irs::payload>(*pos_);
      }
    }
  }

  const frequency* freq_{};
  irs::position* pos_{};
  const offset* offs_{};
  const payload* pay_{};
}; // postings_writer

#if defined(_MSC_VER)
  #pragma warning(disable : 4706)
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

template<typename FormatTraits, bool VolatileAttributes>
irs::postings_writer::state postings_writer<FormatTraits, VolatileAttributes>::write(irs::doc_iterator& docs) {
  REGISTER_TIMER_DETAILED();

  if constexpr (VolatileAttributes) {
    auto* subscription = irs::get<attribute_provider_change>(docs);
    assert(subscription);

    subscription->subscribe([this](attribute_provider& attrs) {
      refresh(attrs);
    });
  } else {
    refresh(docs);
  }

  auto meta = memory::allocate_unique<version10::term_meta>(alloc_);

  begin_term();

  while (docs.next()) {
    const auto did = docs.value();
    assert(doc_limits::valid(did));

    begin_doc<FormatTraits>(did, freq_);
    docs_.value.set(did);

    assert(pos_);
    while (pos_->next()) {
      assert(pos_limits::valid(pos_->value()));
      add_position<FormatTraits>(pos_->value(), offs_, pay_);
    }

    ++meta->docs_count;
    if (freq_) {
      meta->freq += freq_->value;
    }

    end_doc();
  }

  end_term(*meta, freq_ ? &meta->freq : nullptr);

  return make_state(*meta.release());
}

#if defined(_MSC_VER)
  #pragma warning(default : 4706)
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

struct skip_state {
  uint64_t doc_ptr{}; // pointer to the beginning of document block
  uint64_t pos_ptr{}; // pointer to the positions of the first document in a document block
  uint64_t pay_ptr{}; // pointer to the payloads of the first document in a document block
  size_t pend_pos{}; // positions to skip before new document block
  doc_id_t doc{ doc_limits::invalid() }; // last document in a previous block
  uint32_t pay_pos{}; // payload size to skip before in new document block
}; // skip_state

struct skip_context : skip_state {
  size_t level{}; // skip level
}; // skip_context

struct doc_state {
  const index_input* pos_in;
  const index_input* pay_in;
  const version10::term_meta* term_state;
  const uint32_t* freq;
  uint32_t* enc_buf;
  uint64_t tail_start;
  size_t tail_length;
  ::features features;
}; // doc_state

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator_base
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits,
         bool Offset = IteratorTraits::offset(),
         bool Payload = IteratorTraits::payload()>
struct position_impl;

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator_base (position + payload + offset)
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits>
struct position_impl<IteratorTraits, true, true>
    : public position_impl<IteratorTraits, false, false> {
  typedef position_impl<IteratorTraits, false, false> base;

  irs::attribute* attribute(irs::type_info::type_id type) noexcept {
    if (irs::type<payload>::id() == type) {
      return &pay_;
    }

    return irs::type<offset>::id() == type ? &offs_ : nullptr;
  }

  void prepare(const doc_state& state) {
    base::prepare(state);

    pay_in_ = state.pay_in->reopen(); // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen payload input in: %s", __FUNCTION__);

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  void prepare(const skip_state& state)  {
    base::prepare(state);

    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

  void read_attributes() noexcept {
    offs_.start += offs_start_deltas_[this->buf_pos_];
    offs_.end = offs_.start + offs_lengts_[this->buf_pos_];

    pay_.value = bytes_ref(
      pay_data_.c_str() + pay_data_pos_,
      pay_lengths_[this->buf_pos_]);
    pay_data_pos_ += pay_lengths_[this->buf_pos_];
  }

  void clear_attributes() noexcept {
    offs_.clear();
    pay_.value = bytes_ref::NIL;
  }

  void read_block() {
    base::read_block();

    // read payload
    const uint32_t size = pay_in_->read_vint();
    if (size) {
      IteratorTraits::read_block(*pay_in_, this->enc_buf_, pay_lengths_);
      string_utils::oversize(pay_data_, size);

      #ifdef IRESEARCH_DEBUG
        const auto read = pay_in_->read_bytes(&(pay_data_[0]), size);
        assert(read == size);
        UNUSED(read);
      #else
        pay_in_->read_bytes(&(pay_data_[0]), size);
      #endif // IRESEARCH_DEBUG
    }

    // read offsets
    IteratorTraits::read_block(*pay_in_, this->enc_buf_, offs_start_deltas_);
    IteratorTraits::read_block(*pay_in_, this->enc_buf_, offs_lengts_);

    pay_data_pos_ = 0;
  }

  void read_tail_block() {
    size_t pos = 0;

    for (size_t i = 0; i < this->tail_length_; ++i) {
      // read payloads
      if (shift_unpack_32(this->pos_in_->read_vint(), base::pos_deltas_[i])) {
        pay_lengths_[i] = this->pos_in_->read_vint();
      } else {
        assert(i);
        pay_lengths_[i] = pay_lengths_[i-1];
      }

      if (pay_lengths_[i]) {
        const auto size = pay_lengths_[i]; // length of current payload

        string_utils::oversize(pay_data_, pos + size);

        #ifdef IRESEARCH_DEBUG
          const auto read = this->pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
          assert(read == size);
          UNUSED(read);
        #else
          this->pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
        #endif // IRESEARCH_DEBUG

        pos += size;
      }

      if (shift_unpack_32(this->pos_in_->read_vint(), offs_start_deltas_[i])) {
        offs_lengts_[i] = this->pos_in_->read_vint();
      } else {
        assert(i);
        offs_lengts_[i] = offs_lengts_[i - 1];
      }
    }

    pay_data_pos_ = 0;
  }

  void skip_block() {
    base::skip_block();
    base::skip_payload(*pay_in_);
    base::skip_offsets(*pay_in_);
  }

  void skip(size_t count) noexcept {
    // current payload start
    const auto begin = this->pay_lengths_ + this->buf_pos_;
    const auto end = begin + count;
    this->pay_data_pos_ = std::accumulate(begin, end, this->pay_data_pos_);

    base::skip(count);
  }

  index_input::ptr pay_in_;
  offset offs_;
  payload pay_;
  uint32_t offs_start_deltas_[postings_writer_base::BLOCK_SIZE]{}; // buffer to store offset starts
  uint32_t offs_lengts_[postings_writer_base::BLOCK_SIZE]{}; // buffer to store offset lengths
  uint32_t pay_lengths_[postings_writer_base::BLOCK_SIZE]{}; // buffer to store payload lengths
  size_t pay_data_pos_{}; // current position in a payload buffer
  bstring pay_data_; // buffer to store payload data
}; // position_impl

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator_base (position + payload)
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits>
struct position_impl<IteratorTraits, false, true>
    : public position_impl<IteratorTraits, false, false> {
  typedef position_impl<IteratorTraits, false, false> base;

  irs::attribute* attribute(irs::type_info::type_id type) noexcept {
    return irs::type<payload>::id() == type ? &pay_ : nullptr;
  }

  void prepare(const doc_state& state) {
    base::prepare(state);

    pay_in_ = state.pay_in->reopen(); // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen payload input in: %s", __FUNCTION__);

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  void prepare(const skip_state& state)  {
    base::prepare(state);

    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

  void read_attributes() noexcept {
    pay_.value = bytes_ref(
      pay_data_.c_str() + pay_data_pos_,
      pay_lengths_[this->buf_pos_]);
    pay_data_pos_ += pay_lengths_[this->buf_pos_];
  }

  void clear_attributes() noexcept {
    pay_.value = bytes_ref::NIL;
  }

  void read_block() {
    base::read_block();

    // read payload
    const uint32_t size = pay_in_->read_vint();
    if (size) {
      IteratorTraits::read_block(*pay_in_, this->enc_buf_, pay_lengths_);
      string_utils::oversize(pay_data_, size);

      #ifdef IRESEARCH_DEBUG
        const auto read = pay_in_->read_bytes(&(pay_data_[0]), size);
        assert(read == size);
        UNUSED(read);
      #else
        pay_in_->read_bytes(&(pay_data_[0]), size);
      #endif // IRESEARCH_DEBUG
    }

    if (this->features_.offset()) {
      base::skip_offsets(*pay_in_);
    }

    pay_data_pos_ = 0;
  }

  void read_tail_block() {
    size_t pos = 0;

    for (size_t i = 0; i < this->tail_length_; ++i) {
      // read payloads
      if (shift_unpack_32(this->pos_in_->read_vint(), this->pos_deltas_[i])) {
        pay_lengths_[i] = this->pos_in_->read_vint();
      } else {
        assert(i);
        pay_lengths_[i] = pay_lengths_[i-1];
      }

      if (pay_lengths_[i]) {
        const auto size = pay_lengths_[i]; // current payload length

        string_utils::oversize(pay_data_, pos + size);

        #ifdef IRESEARCH_DEBUG
          const auto read = this->pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
          assert(read == size);
          UNUSED(read);
        #else
          this->pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
        #endif // IRESEARCH_DEBUG

        pos += size;
      }

      // skip offsets
      if (this->features_.offset()) {
        uint32_t code;
        if (shift_unpack_32(this->pos_in_->read_vint(), code)) {
          this->pos_in_->read_vint();
        }
      }
    }

    pay_data_pos_ = 0;
  }

  void skip_block() {
    base::skip_block();
    base::skip_payload(*pay_in_);
    if (this->features_.offset()) {
      base::skip_offsets(*pay_in_);
    }
  }

  void skip(size_t count) noexcept {
    // current payload start
    const auto begin = this->pay_lengths_ + this->buf_pos_;
    const auto end = begin + count;
    this->pay_data_pos_ = std::accumulate(begin, end, this->pay_data_pos_);

    base::skip(count);
  }

  index_input::ptr pay_in_;
  payload pay_;
  uint32_t pay_lengths_[postings_writer_base::BLOCK_SIZE]{}; // buffer to store payload lengths
  size_t pay_data_pos_{}; // current position in a payload buffer
  bstring pay_data_; // buffer to store payload data
}; // position_impl

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator_base (position + offset)
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits>
struct position_impl<IteratorTraits, true, false>
    : public position_impl<IteratorTraits, false, false> {
  typedef position_impl<IteratorTraits, false, false> base;

  irs::attribute* attribute(irs::type_info::type_id type) noexcept {
    return irs::type<offset>::id() == type ? &offs_ : nullptr;
  }

  void prepare(const doc_state& state) {
    base::prepare(state);

    pay_in_ = state.pay_in->reopen(); // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen payload input in: %s", __FUNCTION__);

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  void prepare(const skip_state& state) {
    base::prepare(state);

    pay_in_->seek(state.pay_ptr);
  }

  void read_attributes() noexcept {
    offs_.start += offs_start_deltas_[this->buf_pos_];
    offs_.end = offs_.start + offs_lengts_[this->buf_pos_];
  }

  void clear_attributes() noexcept {
    offs_.clear();
  }

  void read_block() {
    base::read_block();

    if (this->features_.payload()) {
      base::skip_payload(*pay_in_);
    }

    // read offsets
    IteratorTraits::read_block(*pay_in_, this->enc_buf_, offs_start_deltas_);
    IteratorTraits::read_block(*pay_in_, this->enc_buf_, offs_lengts_);
  }

  void read_tail_block() {
    uint32_t pay_size = 0;
    for (size_t i = 0; i < this->tail_length_; ++i) {
      // skip payloads
      if (this->features_.payload()) {
        if (shift_unpack_32(this->pos_in_->read_vint(), this->pos_deltas_[i])) {
          pay_size = this->pos_in_->read_vint();
        }
        if (pay_size) {
          this->pos_in_->seek(this->pos_in_->file_pointer() + pay_size);
        }
      } else {
        this->pos_deltas_[i] = this->pos_in_->read_vint();
      }

      // read offsets
      if (shift_unpack_32(this->pos_in_->read_vint(), offs_start_deltas_[i])) {
        offs_lengts_[i] = this->pos_in_->read_vint();
      } else {
        assert(i);
        offs_lengts_[i] = offs_lengts_[i - 1];
      }
    }
  }

  void skip_block() {
    base::skip_block();
    if (this->features_.payload()) {
      base::skip_payload(*pay_in_);
    }
    base::skip_offsets(*pay_in_);
  }

  index_input::ptr pay_in_;
  offset offs_;
  uint32_t offs_start_deltas_[postings_writer_base::BLOCK_SIZE]{}; // buffer to store offset starts
  uint32_t offs_lengts_[postings_writer_base::BLOCK_SIZE]{}; // buffer to store offset lengths
}; // position_impl

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator_base (position)
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits>
struct position_impl<IteratorTraits, false, false> {
  static void skip_payload(index_input& in) {
    const size_t size = in.read_vint();
    if (size) {
      IteratorTraits::skip_block(in);
      in.seek(in.file_pointer() + size);
    }
  }

  static void skip_offsets(index_input& in) {
    IteratorTraits::skip_block(in);
    IteratorTraits::skip_block(in);
  }

  irs::attribute* attribute(irs::type_info::type_id) noexcept {
    // implementation has no additional attributes
    return nullptr;
  }

  void prepare(const doc_state& state) {
    pos_in_ = state.pos_in->reopen(); // reopen thread-safe stream

    if (!pos_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen positions input in: %s", __FUNCTION__);

      throw io_error("failed to reopen positions input");
    }

    cookie_.file_pointer_ = state.term_state->pos_start;
    pos_in_->seek(state.term_state->pos_start);
    freq_ = state.freq;
    features_ = state.features;
    enc_buf_ = state.enc_buf;
    tail_start_ = state.tail_start;
    tail_length_ = state.tail_length;
  }

  void prepare(const skip_state& state) {
    pos_in_->seek(state.pos_ptr);
    pend_pos_ = state.pend_pos;
    buf_pos_ = postings_writer_base::BLOCK_SIZE;
    cookie_.file_pointer_ = state.pos_ptr;
    cookie_.pend_pos_ = pend_pos_;
  }

  void reset() {
    if (std::numeric_limits<size_t>::max() != cookie_.file_pointer_) {
      buf_pos_ = postings_writer_base::BLOCK_SIZE;
      pend_pos_ = cookie_.pend_pos_;
      pos_in_->seek(cookie_.file_pointer_);
    }
  }

  void read_attributes() { }

  void clear_attributes() { }

  void read_tail_block() {
    uint32_t pay_size = 0;
    for (size_t i = 0; i < tail_length_; ++i) {
      if (features_.payload()) {
        if (shift_unpack_32(pos_in_->read_vint(), pos_deltas_[i])) {
          pay_size = pos_in_->read_vint();
        }
        if (pay_size) {
          pos_in_->seek(pos_in_->file_pointer() + pay_size);
        }
      } else {
        pos_deltas_[i] = pos_in_->read_vint();
      }

      if (features_.offset()) {
        uint32_t delta;
        if (shift_unpack_32(pos_in_->read_vint(), delta)) {
          pos_in_->read_vint();
        }
      }
    }
  }

  void read_block() {
    IteratorTraits::read_block(*pos_in_, enc_buf_, pos_deltas_);
  }

  void skip_block() {
    IteratorTraits::skip_block(*pos_in_);
  }

  // skip within a block
  void skip(size_t count) noexcept {
    buf_pos_ += count;
  }

  struct cookie {
    uint32_t pend_pos_{};
    size_t file_pointer_ = std::numeric_limits<size_t>::max();
  };

  uint32_t pos_deltas_[postings_writer_base::BLOCK_SIZE]; // buffer to store position deltas
  const uint32_t* freq_; // lenght of the posting list for a document
  uint32_t* enc_buf_; // auxillary buffer to decode data
  uint32_t pend_pos_{}; // how many positions "behind" we are
  uint64_t tail_start_; // file pointer where the last (vInt encoded) pos delta block is
  size_t tail_length_; // number of positions in the last (vInt encoded) pos delta block
  uint32_t buf_pos_{ postings_writer_base::BLOCK_SIZE }; // current position in pos_deltas_ buffer
  cookie cookie_;
  index_input::ptr pos_in_;
  features features_;
}; // position_impl

template<typename IteratorTraits, bool Position = IteratorTraits::position()>
class position final : public irs::position,
                       protected position_impl<IteratorTraits> {
 public:
  typedef position_impl<IteratorTraits> impl;

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) override {
    return impl::attribute(type);
  }

  virtual value_t seek(value_t target) override {
    const uint32_t freq = *this->freq_;
    if (this->pend_pos_ > freq) {
        skip(this->pend_pos_ - freq);
        this->pend_pos_ = freq;
    }
    while (value_ < target && this->pend_pos_) {
      if (this->buf_pos_ == postings_writer_base::BLOCK_SIZE) {
        refill();
        this->buf_pos_ = 0;
      }
      if constexpr (IteratorTraits::one_based_position_storage()) {
        value_ += (uint32_t)(!pos_limits::valid(value_));
      }
      value_ += this->pos_deltas_[this->buf_pos_];
      assert(irs::pos_limits::valid(value_));
      this->read_attributes();

      ++this->buf_pos_;
      --this->pend_pos_;
    }
    if (0 == this->pend_pos_ && value_ < target) {
      value_ = pos_limits::eof();
    }
    return value_;
  }

  virtual bool next() override {
    if (0 == this->pend_pos_) {
      value_ = pos_limits::eof();

      return false;
    }

    const uint32_t freq = *this->freq_;

    if (this->pend_pos_ > freq) {
      skip(this->pend_pos_ - freq);
      this->pend_pos_ = freq;
    }

    if (this->buf_pos_ == postings_writer_base::BLOCK_SIZE) {
      refill();
      this->buf_pos_ = 0;
    }
    if constexpr (IteratorTraits::one_based_position_storage()) {
      value_ += (uint32_t)(!pos_limits::valid(value_));
    }
    value_ += this->pos_deltas_[this->buf_pos_];
    assert(irs::pos_limits::valid(value_));
    this->read_attributes();

    ++this->buf_pos_;
    --this->pend_pos_;
    return true;
  }

  virtual void reset() override {
    value_ = pos_limits::invalid();
    impl::reset();
  }

  // prepares iterator to work
  // or notifies iterator that doc iterator has skipped to a new block
  using impl::prepare;

  // notify iterator that corresponding doc_iterator has moved forward
  void notify(uint32_t n) {
    this->pend_pos_ += n;
    this->cookie_.pend_pos_ += n;
  }

  void clear() noexcept {
    value_ = pos_limits::invalid();
    impl::clear_attributes();
  }

 private:
  void refill() {
    if (this->pos_in_->file_pointer() == this->tail_start_) {
      this->read_tail_block();
    } else {
      this->read_block();
    }
  }

  void skip(uint32_t count) {
    auto left = postings_writer_base::BLOCK_SIZE - this->buf_pos_;
    if (count >= left) {
      count -= left;
      while (count >= postings_writer_base::BLOCK_SIZE) {
        this->skip_block();
        count -= postings_writer_base::BLOCK_SIZE;
      }
      refill();
      this->buf_pos_ = 0;
      left = postings_writer_base::BLOCK_SIZE;
    }

    if (count < left) {
      impl::skip(count);
    }
    clear();
  }
}; // position

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator (empty)
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits>
struct position<IteratorTraits, false> : attribute {
  static constexpr string_ref type_name() noexcept {
    return irs::position::type_name();
  }

  void prepare(doc_state&) { }
  void prepare(skip_state&) { }
  void notify(uint32_t) { }
  void clear() { }
}; // position

///////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator
///////////////////////////////////////////////////////////////////////////////
template<typename IteratorTraits>
class doc_iterator final : public irs::doc_iterator {
 private:
  using attributes = std::conditional_t<
    IteratorTraits::frequency() && IteratorTraits::position(),
      std::tuple<document, frequency, cost, score, position<IteratorTraits>>,
      std::conditional_t<IteratorTraits::frequency(),
        std::tuple<document, frequency, cost, score>,
        std::tuple<document, cost, score>
      >>;

 public:
  // hide 'ptr' defined in irs::doc_iterator
  using ptr = memory::managed_ptr<doc_iterator>;

  doc_iterator() noexcept
    : skip_levels_(1),
      skip_(postings_writer_base::BLOCK_SIZE, postings_writer_base::SKIP_N) {
    assert(
      std::all_of(docs_, docs_ + postings_writer_base::BLOCK_SIZE,
                  [](doc_id_t doc) { return doc == doc_limits::invalid(); }));
  }

  void prepare(
      const features& field,
      const term_meta& meta,
      const index_input* doc_in,
      [[maybe_unused]] const index_input* pos_in,
      [[maybe_unused]] const index_input* pay_in) {
    features_ = field; // set field features

    assert(!IteratorTraits::frequency() || IteratorTraits::frequency() == features_.freq());
    assert(!IteratorTraits::position() || IteratorTraits::position() == features_.position());
    assert(!IteratorTraits::offset() || IteratorTraits::offset() == features_.offset());
    assert(!IteratorTraits::payload() || IteratorTraits::payload() == features_.payload());

    // add mandatory attributes
    begin_ = end_ = docs_;

    term_state_ = static_cast<const version10::term_meta&>(meta);

    // init document stream
    if (term_state_.docs_count > 1) {
      if (!doc_in_) {
        doc_in_ = doc_in->reopen(); // reopen thread-safe stream

        if (!doc_in_) {
          // implementation returned wrong pointer
          IR_FRMT_ERROR("Failed to reopen document input in: %s", __FUNCTION__);

          throw io_error("failed to reopen document input");
        }
      }

      doc_in_->seek(term_state_.doc_start);
      assert(!doc_in_->eof());
    }

    std::get<cost>(attrs_).reset(term_state_.docs_count); // estimate iterator

    if constexpr (IteratorTraits::frequency()) {
      assert(meta.freq);
      term_freq_ = meta.freq;

      if constexpr (IteratorTraits::position()) {
        doc_state state;
        state.pos_in = pos_in;
        state.pay_in = pay_in;
        state.term_state = &term_state_;
        state.freq = &std::get<frequency>(attrs_).value;
        state.features = features_;
        state.enc_buf = enc_buf_;

        if (term_freq_ < postings_writer_base::BLOCK_SIZE) {
          state.tail_start = term_state_.pos_start;
        } else if (term_freq_ == postings_writer_base::BLOCK_SIZE) {
          state.tail_start = type_limits<type_t::address_t>::invalid();
        } else {
          state.tail_start = term_state_.pos_start + term_state_.pos_end;
        }

        state.tail_length = term_freq_ % postings_writer_base::BLOCK_SIZE;
        std::get<position<IteratorTraits>>(attrs_).prepare(state);
      }
    }

    if (1 == term_state_.docs_count) {
      *docs_ = (doc_limits::min)() + term_state_.e_single_doc;
      *doc_freqs_ = term_freq_;
      doc_freq_ = doc_freqs_;
      ++end_;
    }
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::get_mutable(attrs_, type);
  }

  virtual doc_id_t seek(doc_id_t target) override {
    auto& doc = std::get<document>(attrs_);

    if (target <= doc.value) {
      return doc.value;
    }

    seek_to_block(target);

    if (begin_ == end_) {
      cur_pos_ += relative_pos();

      if (cur_pos_ == term_state_.docs_count) {
        doc.value = doc_limits::eof();
        begin_ = end_ = docs_; // seal the iterator
        return doc_limits::eof();
      }

      refill();
    }

    [[maybe_unused]] uint32_t notify{0};
    while (begin_ < end_) {
      doc.value += *begin_++;

      if constexpr (!IteratorTraits::position()) {
        if (doc.value >= target) {
          if constexpr (IteratorTraits::frequency()) {
            doc_freq_ = doc_freqs_ + relative_pos();
            assert((doc_freq_ - 1) >= doc_freqs_ && (doc_freq_ - 1) < std::end(doc_freqs_));
            std::get<frequency>(attrs_).value = doc_freq_[-1];
          }
          return doc.value;
        }
      } else {
        assert(IteratorTraits::frequency());
        auto& freq = std::get<frequency>(attrs_);
        auto& pos = std::get<position<IteratorTraits>>(attrs_);
        freq.value = *doc_freq_++;
        notify += freq.value;

        if (doc.value >= target) {
          pos.notify(notify);
          pos.clear();
          return doc.value;
        }
      }
    }

    if constexpr (IteratorTraits::position()) {
      std::get<position<IteratorTraits>>(attrs_).notify(notify);
    }
    while (doc.value < target) {
      next();
    }

    return doc.value;
  }

  virtual doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

#if defined(_MSC_VER)
  #pragma warning(disable : 4706)
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

  virtual bool next() override {
    auto& doc = std::get<document>(attrs_);

    if (begin_ == end_) {
      cur_pos_ += relative_pos();

      if (cur_pos_ == term_state_.docs_count) {
        doc.value = doc_limits::eof();
        begin_ = end_ = docs_; // seal the iterator
        return false;
      }

      refill();
    }

    doc.value += *begin_++; // update document attribute

    if constexpr (IteratorTraits::frequency()) {
      auto& freq = std::get<frequency>(attrs_);
      freq.value = *doc_freq_++; // update frequency attribute

      if constexpr (IteratorTraits::position()) {
        auto& pos = std::get<position<IteratorTraits>>(attrs_);
        pos.notify(freq.value);
        pos.clear();
      }
    }

    return true;
  }

#if defined(_MSC_VER)
  #pragma warning(default : 4706)
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

 private:
  void seek_to_block(doc_id_t target);

  // returns current position in the document block 'docs_'
  size_t relative_pos() noexcept {
    assert(begin_ >= docs_);
    return begin_ - docs_;
  }

  doc_id_t read_skip(skip_state& state, index_input& in) {
    state.doc = in.read_vint();
    state.doc_ptr += in.read_vlong();

    if (features_.position()) {
      state.pend_pos = in.read_vint();
      state.pos_ptr += in.read_vlong();

      const bool has_pay = features_.payload();

      if (has_pay || features_.offset()) {
        if (has_pay) {
          state.pay_pos = in.read_vint();
        }

        state.pay_ptr += in.read_vlong();
      }
    }

    return state.doc;
  }

  void read_end_block(size_t size) {
    if (features_.freq()) {
      for (size_t i = 0; i < size; ++i) {
        if (shift_unpack_32(doc_in_->read_vint(), docs_[i])) {
          doc_freqs_[i] = 1;
        } else {
          doc_freqs_[i] = doc_in_->read_vint();
        }
      }
    } else {
      for (size_t i = 0; i < size; ++i) {
        docs_[i] = doc_in_->read_vint();
      }
    }
  }

  void refill() {
    // should never call refill for singleton documents
    assert(1 != term_state_.docs_count);
    const auto left = term_state_.docs_count - cur_pos_;

    if (left >= postings_writer_base::BLOCK_SIZE) {
      // read doc deltas
      IteratorTraits::read_block(
        *doc_in_,
        enc_buf_,
        docs_);

      if constexpr (IteratorTraits::frequency()) {
        IteratorTraits::read_block(
          *doc_in_,
          enc_buf_,
          doc_freqs_);
      } else if (features_.freq()) {
        IteratorTraits::skip_block(*doc_in_);
      }

      end_ = docs_ + postings_writer_base::BLOCK_SIZE;
    } else {
      read_end_block(left);
      end_ = docs_ + left;
    }

    // if this is the initial doc_id then set it to min() for proper delta value
    if (auto& doc = std::get<document>(attrs_);
        !doc_limits::valid(doc.value)) {
      doc.value = (doc_limits::min)();
    }

    begin_ = docs_;
    doc_freq_ = doc_freqs_;
  }

  std::vector<skip_state> skip_levels_;
  skip_reader skip_;
  skip_context* skip_ctx_; // pointer to used skip context, will be used by skip reader
  uint32_t enc_buf_[postings_writer_base::BLOCK_SIZE]; // buffer for encoding
  doc_id_t docs_[postings_writer_base::BLOCK_SIZE]{ }; // doc values
  uint32_t doc_freqs_[postings_writer_base::BLOCK_SIZE]; // document frequencies
  uint32_t cur_pos_{};
  const doc_id_t* begin_{docs_};
  doc_id_t* end_{docs_};
  uint32_t* doc_freq_{}; // pointer into docs_ to the frequency attribute value for the current doc
  uint32_t term_freq_{}; // total term frequency
  index_input::ptr doc_in_;
  version10::term_meta term_state_;
  features features_; // field features
  attributes attrs_;
}; // doc_iterator

template<typename IteratorTraits>
void doc_iterator<IteratorTraits>::seek_to_block(doc_id_t target) {
  // check whether it make sense to use skip-list
  if (skip_levels_.front().doc < target && term_state_.docs_count > postings_writer_base::BLOCK_SIZE) {
    skip_context last; // where block starts
    skip_ctx_ = &last;

    // init skip writer in lazy fashion
    if (!skip_) {
      auto skip_in = doc_in_->dup();

      if (!skip_in) {
        IR_FRMT_ERROR("Failed to duplicate input in: %s", __FUNCTION__);

        throw io_error("Failed to duplicate document input");
      }

      skip_in->seek(term_state_.doc_start + term_state_.e_skip_start);

      skip_.prepare(
        std::move(skip_in),
        [this](size_t level, index_input& in) {
          skip_state& last = *skip_ctx_;
          auto& last_level = skip_ctx_->level;
          auto& next = skip_levels_[level];

          if (last_level > level) {
            // move to the more granular level
            next = last;
          } else {
            // store previous step on the same level
            last = next;
          }

          last_level = level;

          if (in.eof()) {
            // stream exhausted
            return (next.doc = doc_limits::eof());
          }

          return read_skip(next, in);
      });

      // initialize skip levels
      const auto num_levels = skip_.num_levels();
      if (num_levels) {
        skip_levels_.resize(num_levels);

        // since we store pointer deltas, add postings offset
        auto& top = skip_levels_.back();
        top.doc_ptr = term_state_.doc_start;
        top.pos_ptr = term_state_.pos_start;
        top.pay_ptr = term_state_.pay_start;
      }
    }

    const size_t skipped = skip_.seek(target);
    if (skipped > (cur_pos_ + relative_pos())) {
      doc_in_->seek(last.doc_ptr);
      std::get<document>(attrs_).value = last.doc;
      cur_pos_ = skipped;
      begin_ = end_ = docs_; // will trigger refill in "next"
      if constexpr (IteratorTraits::position()) {
        std::get<position<IteratorTraits>>(attrs_).prepare(last); // notify positions
      }
    }
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                index_meta_writer
// ----------------------------------------------------------------------------

struct index_meta_writer final: public irs::index_meta_writer {
  static constexpr string_ref FORMAT_NAME = "iresearch_10_index_meta";
  static constexpr string_ref FORMAT_PREFIX = "segments_";
  static constexpr string_ref FORMAT_PREFIX_TMP = "pending_segments_";

  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = 1;

  enum { HAS_PAYLOAD = 1 };

  explicit index_meta_writer(int32_t version) noexcept
    : version_(version) {
    assert(version_ >= FORMAT_MIN && version <= FORMAT_MAX);
  }

  virtual std::string filename(const index_meta& meta) const override;
  using irs::index_meta_writer::prepare;
  virtual bool prepare(directory& dir, index_meta& meta) override;
  virtual bool commit() override;
  virtual void rollback() noexcept override;

 private:
  directory* dir_ = nullptr;
  index_meta* meta_ = nullptr;
  int32_t version_;
}; // index_meta_writer

template<>
std::string file_name<irs::index_meta_writer, index_meta>(const index_meta& meta) {
  return file_name(index_meta_writer::FORMAT_PREFIX_TMP, meta.generation());
}

struct index_meta_reader final: public irs::index_meta_reader {
  virtual bool last_segments_file(
    const directory& dir, std::string& name) const override;

  virtual void read(
    const directory& dir,
    index_meta& meta,
    const string_ref& filename = string_ref::NIL) override; // null == use meta
}; // index_meta_reader

template<>
std::string file_name<irs::index_meta_reader, index_meta>(const index_meta& meta) {
  return file_name(index_meta_writer::FORMAT_PREFIX, meta.generation());
}

std::string index_meta_writer::filename(const index_meta& meta) const {
  return file_name<irs::index_meta_reader>(meta);
}

bool index_meta_writer::prepare(directory& dir, index_meta& meta) {
  if (meta_) {
    // prepare() was already called with no corresponding call to commit()
    return false;
  }

  prepare(meta); // prepare meta before generating filename

  const auto seg_file = file_name<irs::index_meta_writer>(meta);

  auto out = dir.create(seg_file);

  if (!out) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s",
      seg_file.c_str()));
  }

  {
    format_utils::write_header(*out, FORMAT_NAME, version_);
    out->write_vlong(meta.generation());
    out->write_long(meta.counter());
    assert(meta.size() <= std::numeric_limits<uint32_t>::max());
    out->write_vint(uint32_t(meta.size()));

    for (auto& segment : meta) {
      write_string(*out, segment.filename);
      write_string(*out, segment.meta.codec->type().name());
    }

    if (version_ > FORMAT_MIN) {
      const byte_type flags = meta.payload().null() ? 0 : HAS_PAYLOAD;
      out->write_byte(flags);

      if (flags == HAS_PAYLOAD) {
        irs::write_string(*out, meta.payload());
      }
    }

    format_utils::write_footer(*out);
    // important to close output here
  }

  if (!dir.sync(seg_file)) {
    throw io_error(string_utils::to_string(
      "failed to sync file, path: %s",
      seg_file.c_str()));
  }

  // only noexcept operations below
  dir_ = &dir;
  meta_ = &meta;

  return true;
}

bool index_meta_writer::commit() {
  if (!meta_) {
    return false;
  }

  const auto src = file_name<irs::index_meta_writer>(*meta_);
  const auto dst = file_name<irs::index_meta_reader>(*meta_);

  if (!dir_->rename(src, dst)) {
    rollback();

    throw io_error(string_utils::to_string(
      "failed to rename file, src path: '%s' dst path: '%s'",
      src.c_str(),
      dst.c_str()));
  }

  // only noexcept operations below
  complete(*meta_);

  // clear pending state
  meta_ = nullptr;
  dir_ = nullptr;

  return true;
}

void index_meta_writer::rollback() noexcept {
  if (!meta_) {
    return;
  }

  std::string seg_file;

  try {
    seg_file = file_name<irs::index_meta_writer>(*meta_);
  } catch (const std::exception& e) {
    IR_FRMT_ERROR("Caught error while generating file name for index meta, reason: %s", e.what());
    return;
  } catch (...) {
    IR_FRMT_ERROR("Caught error while generating file name for index meta");
    return;
  }

  if (!dir_->remove(seg_file)) { // suppress all errors
    IR_FRMT_ERROR("Failed to remove file, path: %s", seg_file.c_str());
  }

  // clear pending state
  dir_ = nullptr;
  meta_ = nullptr;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                index_meta_reader
// ----------------------------------------------------------------------------

uint64_t parse_generation(const std::string& segments_file) {
  assert(irs::starts_with(segments_file, index_meta_writer::FORMAT_PREFIX));

  const char* gen_str = segments_file.c_str() + index_meta_writer::FORMAT_PREFIX.size();
  char* suffix;
  auto gen = std::strtoull(gen_str, &suffix, 10); // 10 for base-10

  return suffix[0] ? type_limits<type_t::index_gen_t>::invalid() : gen;
}

bool index_meta_reader::last_segments_file(const directory& dir, std::string& out) const {
  uint64_t max_gen = 0;
  directory::visitor_f visitor = [&out, &max_gen] (std::string& name) {
    if (irs::starts_with(name, index_meta_writer::FORMAT_PREFIX)) {
      const uint64_t gen = parse_generation(name);

      if (type_limits<type_t::index_gen_t>::valid(gen) && gen > max_gen) {
        out = std::move(name);
        max_gen = gen;
      }
    }
    return true; // continue iteration
  };

  dir.visit(visitor);
  return max_gen > 0;
}

void index_meta_reader::read(
    const directory& dir,
    index_meta& meta,
    const string_ref& filename /*= string_ref::NIL*/) {

  const std::string meta_file = filename.null()
    ? file_name<irs::index_meta_reader>(meta)
    : static_cast<std::string>(filename);

  auto in = dir.open(
    meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      meta_file.c_str()));
  }

  const auto checksum = format_utils::checksum(*in);

  // check header
  const int32_t version = format_utils::check_header(
    *in,
    index_meta_writer::FORMAT_NAME,
    index_meta_writer::FORMAT_MIN,
    index_meta_writer::FORMAT_MAX);

  // read data from segments file
  auto gen = in->read_vlong();
  auto cnt = in->read_long();
  auto seg_count = in->read_vint();
  index_meta::index_segments_t segments(seg_count);

  for (size_t i = 0, count = segments.size(); i < count; ++i) {
    auto& segment = segments[i];

    segment.filename = read_string<std::string>(*in);
    segment.meta.codec = formats::get(read_string<std::string>(*in));

    auto reader = segment.meta.codec->get_segment_meta_reader();

    reader->read(dir, segment.meta, segment.filename);
  }

  bool has_payload = false;
  bstring payload;
  if (version > index_meta_writer::FORMAT_MIN) {
    has_payload = (in->read_byte() & index_meta_writer::HAS_PAYLOAD);

    if (has_payload) {
      payload = irs::read_string<bstring>(*in);
    }
  }

  format_utils::check_footer(*in, checksum);

  complete(
    meta, gen, cnt,
    std::move(segments),
    has_payload ? &payload : nullptr);
}

// ----------------------------------------------------------------------------
// --SECTION--                                              segment_meta_writer
// ----------------------------------------------------------------------------

struct segment_meta_writer final : public irs::segment_meta_writer{
  static constexpr string_ref FORMAT_EXT = "sm";
  static constexpr string_ref FORMAT_NAME = "iresearch_10_segment_meta";

  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = 1;

  enum {
    HAS_COLUMN_STORE = 1,
    SORTED = 2
  };

  explicit segment_meta_writer(int32_t version) noexcept
    : version_(version) {
    assert(version_ >= FORMAT_MIN && version <= FORMAT_MAX);
  }

  virtual void write(
    directory& dir,
    std::string& filename,
    const segment_meta& meta) override;

 private:
  int32_t version_;
}; // segment_meta_writer

template<>
std::string file_name<irs::segment_meta_writer, segment_meta>(const segment_meta& meta) {
  return irs::file_name(meta.name, meta.version, segment_meta_writer::FORMAT_EXT);
}

void segment_meta_writer::write(directory& dir, std::string& meta_file, const segment_meta& meta) {
  if (meta.docs_count < meta.live_docs_count) {
    throw index_error(string_utils::to_string(
      "invalid segment meta '%s' detected : docs_count=" IR_SIZE_T_SPECIFIER ", live_docs_count=" IR_SIZE_T_SPECIFIER "",
      meta.name.c_str(), meta.docs_count, meta.live_docs_count));
  }

  meta_file = file_name<irs::segment_meta_writer>(meta);
  auto out = dir.create(meta_file);

  if (!out) {
    throw io_error(string_utils::to_string(
      "failed to create file, path: %s",
      meta_file.c_str()));
  }

  byte_type flags = meta.column_store ? HAS_COLUMN_STORE : 0;

  format_utils::write_header(*out, FORMAT_NAME, version_);
  write_string(*out, meta.name);
  out->write_vlong(meta.version);
  out->write_vlong(meta.live_docs_count);
  out->write_vlong(meta.docs_count - meta.live_docs_count); // docs_count >= live_docs_count
  out->write_vlong(meta.size);
  if (version_ > FORMAT_MIN) {
    // sorted indices are not supported in version 1.0
    if (field_limits::valid(meta.sort)) {
      flags |= SORTED;
    }

    out->write_byte(flags);
    out->write_vlong(1+meta.sort); // max->0
  } else {
    out->write_byte(flags);
  }
  write_strings(*out, meta.files);
  format_utils::write_footer(*out);
}

// ----------------------------------------------------------------------------
// --SECTION--                                              segment_meta_reader
// ----------------------------------------------------------------------------

struct segment_meta_reader final : public irs::segment_meta_reader {
  virtual void read(
    const directory& dir,
    segment_meta& meta,
    const string_ref& filename = string_ref::NIL) override; // null == use meta
}; // segment_meta_reader

void segment_meta_reader::read(
    const directory& dir,
    segment_meta& meta,
    const string_ref& filename /*= string_ref::NIL*/) {

  const std::string meta_file = filename.null()
    ? file_name<irs::segment_meta_writer>(meta)
    : static_cast<std::string>(filename);

  auto in = dir.open(
    meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      meta_file.c_str()));
  }

  const auto checksum = format_utils::checksum(*in);

  const int32_t version = format_utils::check_header(
    *in,
    segment_meta_writer::FORMAT_NAME,
    segment_meta_writer::FORMAT_MIN,
    segment_meta_writer::FORMAT_MAX);

  auto name = read_string<std::string>(*in);
  const auto segment_version = in->read_vlong();
  const auto live_docs_count = in->read_vlong();
  const auto docs_count = in->read_vlong() + live_docs_count;

  if (docs_count < live_docs_count) {
    throw index_error(std::string("while reader segment meta '") + name
      + "', error: docs_count(" + std::to_string(docs_count)
      + ") < live_docs_count(" + std::to_string(live_docs_count) + ")");
  }

  const auto size = in->read_vlong();
  const auto flags = in->read_byte();
  field_id sort = field_limits::invalid();
  if (version > segment_meta_writer::FORMAT_MIN) {
    sort = in->read_vlong() - 1;
  }
  auto files = read_strings<segment_meta::file_set>(*in);

  if (flags & ~(segment_meta_writer::HAS_COLUMN_STORE | segment_meta_writer::SORTED)) {
    throw index_error(string_utils::to_string(
      "while reading segment meta '%s', error: use of unsupported flags '%u'",
      name.c_str(), flags));
  }

  const auto sorted = bool(flags & segment_meta_writer::SORTED);

  if ((!field_limits::valid(sort)) && sorted) {
    throw index_error(string_utils::to_string(
      "while reading segment meta '%s', error: incorrectly marked as sorted",
      name.c_str()));
  }

  if ((field_limits::valid(sort)) && !sorted) {
    throw index_error(string_utils::to_string(
      "while reading segment meta '%s', error: incorrectly marked as unsorted",
      name.c_str()));
  }

  format_utils::check_footer(*in, checksum);

  // ...........................................................................
  // all operations below are noexcept
  // ...........................................................................

  meta.name = std::move(name);
  meta.version = segment_version;
  meta.column_store = flags & segment_meta_writer::HAS_COLUMN_STORE;
  meta.docs_count = docs_count;
  meta.live_docs_count = live_docs_count;
  meta.sort = sort;
  meta.size = size;
  meta.files = std::move(files);
}

// ----------------------------------------------------------------------------
// --SECTION--                                             document_mask_writer
// ----------------------------------------------------------------------------

class document_mask_writer final: public irs::document_mask_writer {
 public:
  static constexpr string_ref FORMAT_NAME = "iresearch_10_doc_mask";
  static constexpr string_ref FORMAT_EXT = "doc_mask";

  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = FORMAT_MIN;

  virtual ~document_mask_writer() = default;

  virtual std::string filename(const segment_meta& meta) const override;

  virtual void write(directory& dir,
                     const segment_meta& meta,
                     const document_mask& docs_mask) override;
}; // document_mask_writer

template<>
std::string file_name<irs::document_mask_writer, segment_meta>(const segment_meta& meta) {
  return file_name(meta.name, meta.version, document_mask_writer::FORMAT_EXT);
}

std::string document_mask_writer::filename(const segment_meta& meta) const {
  return file_name<irs::document_mask_writer>(meta);
}

void document_mask_writer::write(
    directory& dir,
    const segment_meta& meta,
    const document_mask& docs_mask) {
  const auto filename = file_name<irs::document_mask_writer>(meta);
  auto out = dir.create(filename);

  if (!out) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s",
      filename.c_str()));
  }

  // segment can't have more than std::numeric_limits<uint32_t>::max() documents
  assert(docs_mask.size() <= std::numeric_limits<uint32_t>::max());
  const auto count = static_cast<uint32_t>(docs_mask.size());

  format_utils::write_header(*out, FORMAT_NAME, FORMAT_MAX);
  out->write_vint(count);

  for (auto mask : docs_mask) {
    out->write_vint(mask);
  }

  format_utils::write_footer(*out);
}

// ----------------------------------------------------------------------------
// --SECTION--                                             document_mask_reader
// ----------------------------------------------------------------------------

class document_mask_reader final: public irs::document_mask_reader {
 public:
  virtual ~document_mask_reader() = default;

  virtual bool read(
    const directory& dir,
    const segment_meta& meta,
    document_mask& docs_mask) override;
}; // document_mask_reader

bool document_mask_reader::read(
    const directory& dir,
    const segment_meta& meta,
    document_mask& docs_mask
) {
  const auto in_name = file_name<irs::document_mask_writer>(meta);

  bool exists;

  if (!dir.exists(exists, in_name)) {
    throw io_error(string_utils::to_string(
      "failed to check existence of file, path: %s",
      in_name.c_str()));
  }

  if (!exists) {
    // possible that the file does not exist since document_mask is optional
    return false;
  }

  auto in = dir.open(
    in_name, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      in_name.c_str()));
  }

  const auto checksum = format_utils::checksum(*in);

  format_utils::check_header(
    *in,
    document_mask_writer::FORMAT_NAME,
    document_mask_writer::FORMAT_MIN,
    document_mask_writer::FORMAT_MAX);

  size_t count = in->read_vint();
  docs_mask.reserve(count);

  while (count--) {
    static_assert(
      sizeof(doc_id_t) == sizeof(decltype(in->read_vint())),
      "sizeof(doc_id) != sizeof(decltype(id))");

    docs_mask.insert(in->read_vint());
  }

  format_utils::check_footer(*in, checksum);

  return true;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                      columnstore
// ----------------------------------------------------------------------------

namespace columns {

template<typename T, typename M>
std::string file_name(const M& meta); // forward declaration

template<typename T, typename M>
void file_name(std::string& buf, const M& meta); // forward declaration

class meta_writer final : public irs::column_meta_writer {
 public:
  static constexpr string_ref FORMAT_NAME = "iresearch_10_columnmeta";
  static constexpr string_ref FORMAT_EXT = "cm";

  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = 1;

  explicit meta_writer(int32_t version) noexcept
    : version_(version) {
    assert(version >= FORMAT_MIN && version <= FORMAT_MAX);
  }

  virtual void prepare(directory& dir, const segment_meta& meta) override;
  virtual void write(const std::string& name, field_id id) override;
  virtual void flush() override;

 private:
  encryption::stream::ptr out_cipher_;
  index_output::ptr out_;
  size_t count_{}; // number of written objects
  field_id max_id_{}; // the highest column id written (optimization for vector resize on read to highest id)
  int32_t version_;
}; // meta_writer

template<>
std::string file_name<column_meta_writer, segment_meta>(
    const segment_meta& meta) {
  return irs::file_name(meta.name, columns::meta_writer::FORMAT_EXT);
}

void meta_writer::prepare(directory& dir, const segment_meta& meta) {
  auto filename = file_name<column_meta_writer>(meta);

  out_ = dir.create(filename);

  if (!out_) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s", filename.c_str()));
  }

  format_utils::write_header(*out_, FORMAT_NAME, version_);

  if (version_ > FORMAT_MIN) {
    bstring enc_header;
    auto* enc = get_encryption(dir.attributes());

    if (irs::encrypt(filename, *out_, enc, enc_header, out_cipher_)) {
      assert(out_cipher_ && out_cipher_->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        DEFAULT_ENCRYPTION_BUFFER_SIZE,
        out_cipher_->block_size());

      out_ = index_output::make<encrypted_output>(
        std::move(out_),
        *out_cipher_,
        blocks_in_buffer);
    }
  }
}

void meta_writer::write(const std::string& name, field_id id) {
  assert(out_);
  out_->write_vlong(id);
  write_string(*out_, name);
  ++count_;
  max_id_ = std::max(max_id_, id);
}

void meta_writer::flush() {
  assert(out_);

  if (out_cipher_) {
    auto& out = static_cast<encrypted_output&>(*out_);
    out.flush();
    out_ = out.release();
  }

  out_->write_long(count_); // write total number of written objects
  out_->write_long(max_id_); // write highest column id written
  format_utils::write_footer(*out_);
  out_.reset();
  count_ = 0;
}

class meta_reader final : public irs::column_meta_reader {
 public:
  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta,
    size_t& count,
    field_id& max_id) override;
  virtual bool read(column_meta& column) override;

 private:
  encryption::stream::ptr in_cipher_;
  index_input::ptr in_;
  size_t count_{0};
  field_id max_id_{0};
}; // meta_writer

bool meta_reader::prepare(
    const directory& dir,
    const segment_meta& meta,
    size_t& count,
    field_id& max_id) {
  const auto filename = file_name<column_meta_writer>(meta);

  bool exists;

  if (!dir.exists(exists, filename)) {
    throw io_error(string_utils::to_string(
      "failed to check existence of file, path: %s",
      filename.c_str()));
  }

  if (!exists) {
    // column meta is optional
    return false;
  }

  in_ = dir.open(
    filename, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in_) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      filename.c_str()));
  }

  const auto checksum = format_utils::checksum(*in_);

  constexpr const size_t FOOTER_LEN =
      sizeof(uint64_t) // count
    + sizeof(field_id) // max id
    + format_utils::FOOTER_LEN;

  // seek to start of meta footer (before count and max_id)
  in_->seek(in_->length() - FOOTER_LEN);
  count = in_->read_long(); // read number of objects to read
  max_id = in_->read_long(); // read highest column id written

  if (max_id >= std::numeric_limits<size_t>::max()) {
    throw index_error(string_utils::to_string(
      "invalid max column id: " IR_UINT64_T_SPECIFIER ", path: %s",
      max_id, filename.c_str()));
  }

  format_utils::check_footer(*in_, checksum);

  in_->seek(0);

  const auto version = format_utils::check_header(
    *in_,
    meta_writer::FORMAT_NAME,
    meta_writer::FORMAT_MIN,
    meta_writer::FORMAT_MAX);

  if (version > meta_writer::FORMAT_MIN) {
    encryption* enc = get_encryption(dir.attributes());

    if (irs::decrypt(filename, *in_, enc, in_cipher_)) {
      assert(in_cipher_ && in_cipher_->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        DEFAULT_ENCRYPTION_BUFFER_SIZE,
        in_cipher_->block_size());

      in_ = memory::make_unique<encrypted_input>(
        std::move(in_),
        *in_cipher_,
        blocks_in_buffer,
        FOOTER_LEN);
    }
  }

  count_ = count;
  max_id_ = max_id;
  return true;
}

bool meta_reader::read(column_meta& column) {
  if (!count_) {
    return false;
  }

  const auto id = in_->read_vlong();

  assert(id <= max_id_);
  column.name = read_string<std::string>(*in_);
  column.id = id;
  --count_;

  return true;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                 Format constants
// ----------------------------------------------------------------------------

// |Header|
// |Compressed block #0|
// |Compressed block #1|
// |Compressed block #2|
// |Compressed block #1| <-- Columnstore data blocks
// |Compressed block #1|
// |Compressed block #1|
// |Compressed block #2|
// ...
// |Last block #0 key|Block #0 offset|
// |Last block #1 key|Block #1 offset| <-- Columnstore blocks index
// |Last block #2 key|Block #2 offset|
// ...
// |Footer|

constexpr uint32_t INDEX_BLOCK_SIZE = 1024;
constexpr size_t MAX_DATA_BLOCK_SIZE = 8192;

/// @brief Column flags
/// @note by default we treat columns as a variable length sparse columns
enum ColumnProperty : uint32_t {
  CP_SPARSE = 0,
  CP_DENSE = 1,                // keys can be presented as an array indices
  CP_FIXED = 1 << 1,           // fixed length colums
  CP_MASK = 1 << 2,            // column contains no data
  CP_COLUMN_DENSE = 1 << 3,    // column index is dense
  CP_COLUMN_ENCRYPT = 1 << 4   // column contains encrypted data
}; // ColumnProperty

ENABLE_BITMASK_ENUM(ColumnProperty);

bool is_good_compression_ratio(size_t raw_size, size_t compressed_size) noexcept {
  // check if compressed is less than 12.5%
  return compressed_size < raw_size - (raw_size / 8U);
}

ColumnProperty write_compact(
    index_output& out,
    bstring& encode_buf,
    encryption::stream* cipher,
    compression::compressor& compressor,
    bstring& data) {
  if (data.empty()) {
    out.write_byte(0); // zig_zag_encode32(0) == 0
    return CP_MASK;
  }

  // compressor can only handle size of int32_t, so can use the negative flag as a compression flag
  const bytes_ref compressed = compressor.compress(&data[0], data.size(), encode_buf);

  if (is_good_compression_ratio(data.size(), compressed.size())) {
    assert(compressed.size() <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    irs::write_zvint(out, int32_t(compressed.size())); // compressed size
    if (cipher) {
      cipher->encrypt(out.file_pointer(), const_cast<irs::byte_type*>(compressed.c_str()), compressed.size());
    }
    out.write_bytes(compressed.c_str(), compressed.size());
    irs::write_zvlong(out, data.size() - MAX_DATA_BLOCK_SIZE); // original size
  } else {
    assert(data.size() <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    irs::write_zvint(out, int32_t(0) - int32_t(data.size())); // -ve to mark uncompressed
    if (cipher) {
      cipher->encrypt(out.file_pointer(), const_cast<irs::byte_type*>(data.c_str()), data.size());
    }
    out.write_bytes(data.c_str(), data.size());
  }

  return CP_SPARSE;
}

void read_compact(
    irs::index_input& in,
    irs::encryption::stream* cipher,
    irs::compression::decompressor* decompressor,
    irs::bstring& encode_buf,
    irs::bstring& decode_buf) {
  const auto size = irs::read_zvint(in);

  if (!size) {
    return;
  }

  const size_t buf_size = std::abs(size);

  // -ve to mark uncompressed
  if (size < 0) {
    decode_buf.resize(buf_size); // ensure that we have enough space to store decompressed data

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(const_cast<byte_type*>(decode_buf.c_str()), buf_size);
    assert(read == buf_size);
    UNUSED(read);
#else
    in.read_bytes(const_cast<byte_type*>(decode_buf.c_str()), buf_size);
#endif // IRESEARCH_DEBUG

    if (cipher) {
      cipher->decrypt(in.file_pointer() - buf_size,
                      const_cast<byte_type*>(decode_buf.c_str()), buf_size);
    }

    return;
  }

  if (IRS_UNLIKELY(!decompressor)) {
    throw irs::index_error(string_utils::to_string(
      "while reading compact, error: can't decompress block of size %d for whithout decompressor",
      size));
  }

  // try direct buffer access
  const byte_type* buf = cipher ? nullptr : in.read_buffer(buf_size, BufferHint::NORMAL);

  if (!buf) {
    irs::string_utils::oversize(encode_buf, buf_size);

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(const_cast<byte_type*>(encode_buf.c_str()), buf_size);
    assert(read == buf_size);
    UNUSED(read);
#else
    in.read_bytes(const_cast<byte_type*>(encode_buf.c_str()), buf_size);
#endif // IRESEARCH_DEBUG

    if (cipher) {
      cipher->decrypt(in.file_pointer() - buf_size,
                      const_cast<byte_type*>(encode_buf.c_str()), buf_size);
    }

    buf = encode_buf.c_str();
  }

  // ensure that we have enough space to store decompressed data
  decode_buf.resize(irs::read_zvlong(in) + MAX_DATA_BLOCK_SIZE);

  const auto decoded = decompressor->decompress(
    buf, buf_size,
    &decode_buf[0], decode_buf.size());

  if (decoded.null()) {
    throw irs::index_error("error while reading compact");
  }
}

template<size_t Size>
class index_block {
 public:
  static const size_t SIZE = Size;

  void push_back(doc_id_t key, uint64_t offset) noexcept {
    assert(key_ >= keys_);
    assert(key_ < keys_ + Size);
    *key_++ = key;
    assert(key >= key_[-1]);
    assert(offset_ >= offsets_);
    assert(offset_ < offsets_ + Size);
    *offset_++ = offset;
    assert(offset >= offset_[-1]);
  }

  void pop_back() noexcept {
    assert(key_ > keys_);
    *key_-- = 0;
    assert(offset_ > offsets_);
    *offset_-- = 0;
  }

  // returns total number of items
  uint32_t total() const noexcept {
    return flushed()+ size();
  }

  // returns total number of flushed items
  uint32_t flushed() const noexcept {
    return flushed_;
  }

  // returns number of items to be flushed
  uint32_t size() const noexcept {
    assert(key_ >= keys_);
    return uint32_t(key_ - keys_);
  }

  bool empty() const noexcept {
    return keys_ == key_;
  }

  bool full() const noexcept {
    return key_ == std::end(keys_);
  }

  doc_id_t min_key() const noexcept {
    return *keys_;
  }

  doc_id_t max_key() const noexcept {
    // if this->empty(), will point to last offset
    // value which is 0 in this case
    return *(key_-1);
  }

  uint64_t max_offset() const noexcept {
    assert(offset_ > offsets_);
    return *(offset_-1);
  }

  ColumnProperty flush(data_output& out, uint64_t* buf) {
    if (empty()) {
      return CP_DENSE | CP_FIXED;
    }

    const auto size = this->size();

    ColumnProperty props = CP_SPARSE;

    // write keys
    {
      // adjust number of elements to pack to the nearest value
      // that is multiple of the block size
      const auto block_size = math::ceil32(size, packed::BLOCK_SIZE_32);
      assert(block_size >= size);

      assert(std::is_sorted(keys_, key_));
      const auto stats = encode::avg::encode(keys_, key_);
      const auto bits = encode::avg::write_block(
        out, stats.first, stats.second,
        keys_, block_size, reinterpret_cast<uint32_t*>(buf));

      if (1 == stats.second && 0 == keys_[0] && encode::bitpack::rl(bits)) {
        props |= CP_DENSE;
      }
    }

    // write offsets
    {
      // adjust number of elements to pack to the nearest value
      // that is multiple of the block size
      const auto block_size = math::ceil64(size, packed::BLOCK_SIZE_64);
      assert(block_size >= size);

      assert(std::is_sorted(offsets_, offset_));
      const auto stats = encode::avg::encode(offsets_, offset_);
      const auto bits = encode::avg::write_block(
        out, stats.first, stats.second,
        offsets_, block_size, buf);

      if (0 == offsets_[0] && encode::bitpack::rl(bits)) {
        props |= CP_FIXED;
      }
    }

    flushed_ += size;

    // reset pointers and clear data
    key_ = keys_;
    std::memset(keys_, 0, sizeof keys_);
    offset_ = offsets_;
    std::memset(offsets_, 0, sizeof offsets_);

    return props;
  }

 private:
  // order is important (see max_key())
  uint64_t offsets_[Size]{};
  doc_id_t keys_[Size]{};
  uint64_t* offset_{ offsets_ };
  doc_id_t* key_{ keys_ };
  uint32_t flushed_{}; // number of flushed items
}; // index_block

//////////////////////////////////////////////////////////////////////////////
/// @class writer
//////////////////////////////////////////////////////////////////////////////
class writer final : public irs::columnstore_writer {
 public:
  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = 1;

  static constexpr string_ref FORMAT_NAME = "iresearch_10_columnstore";
  static constexpr string_ref FORMAT_EXT = "cs";

  explicit writer(int32_t version) noexcept
    : buf_(2*MAX_DATA_BLOCK_SIZE, 0),
      version_(version) {
    static_assert(
      2*MAX_DATA_BLOCK_SIZE >= INDEX_BLOCK_SIZE*sizeof(uint64_t),
      "buffer is not big enough");

    assert(version >= FORMAT_MIN && version <= FORMAT_MAX);
  }

  virtual void prepare(directory& dir, const segment_meta& meta) override;
  virtual column_t push_column(const column_info& info) override;
  virtual bool commit() override;
  virtual void rollback() noexcept override;

 private:
  class column final : public irs::columnstore_writer::column_output {
   public:
    explicit column(writer& ctx, const irs::type_info& type,
                    const compression::compressor::ptr& compressor,
                    encryption::stream* cipher)
      : ctx_(&ctx),
        comp_type_(type),
        comp_(compressor),
        cipher_(cipher),
        blocks_index_(*ctx.alloc_),
        block_buf_(2*MAX_DATA_BLOCK_SIZE, 0) {
      assert(comp_); // ensured by `push_column'
      block_buf_.clear(); // reset size to '0'
    }

    void prepare(doc_id_t key) {
      assert(key >= block_index_.max_key());

      if (key <= block_index_.max_key()) {
        // less or equal to previous key
        return;
      }

      // flush block if we've overcome MAX_DATA_BLOCK_SIZE size
      // or reached the end of the index block
      if (block_buf_.size() >= MAX_DATA_BLOCK_SIZE || block_index_.full()) {
        flush_block();
      }

      block_index_.push_back(key, block_buf_.size());
    }

    bool empty() const noexcept {
      return !block_index_.total();
    }

    void finish() {
      auto& out = *ctx_->data_out_;

       // evaluate overall column properties
      auto column_props = blocks_props_;
      if (0 != (column_props_ & CP_DENSE)) { column_props |= CP_COLUMN_DENSE; }
      if (cipher_) { column_props |= CP_COLUMN_ENCRYPT; }

      write_enum(out, column_props);
      if (ctx_->version_ > FORMAT_MIN) {
        write_string(out, comp_type_.name());
        comp_->flush(out); // flush compression dependent data
      }
      out.write_vint(block_index_.total()); // total number of items
      out.write_vint(max_); // max column key
      out.write_vint(avg_block_size_); // avg data block size
      out.write_vint(avg_block_count_); // avg number of elements per block
      out.write_vint(column_index_.total()); // total number of index blocks
      blocks_index_.file >> out; // column blocks index
    }

    void flush() {
      // do not take into account last block
      const auto blocks_count = std::max(1U, column_index_.total());
      avg_block_count_ = block_index_.flushed() / blocks_count;
      avg_block_size_ = length_ / blocks_count;

      // commit and flush remain blocks
      flush_block();

      // finish column blocks index
      assert(ctx_->buf_.size() >= INDEX_BLOCK_SIZE*sizeof(uint64_t));
      auto* buf = reinterpret_cast<uint64_t*>(&ctx_->buf_[0]);
      column_index_.flush(blocks_index_.stream, buf);
      blocks_index_.stream.flush();
    }

    virtual void close() override {
      // NOOP
    }

    virtual void write_byte(byte_type b) override {
      block_buf_ += b;
    }

    virtual void write_bytes(const byte_type* b, size_t size) override {
      block_buf_.append(b, size);
    }

    virtual void reset() override {
      if (block_index_.empty()) {
        // nothing to reset
        return;
      }

      // reset to previous offset
      block_buf_.resize(block_index_.max_offset());
      block_index_.pop_back();
    }

   private:
    void flush_block() {
      if (block_index_.empty()) {
        // nothing to flush
        return;
      }

      // refresh column properties
      // column is dense IFF
      // - all blocks are dense
      // - there are no gaps between blocks
      column_props_ &= ColumnProperty(column_index_.empty() || 1 == block_index_.min_key() - max_);

      // update max element
      max_ = block_index_.max_key();

      auto& out = *ctx_->data_out_;

      // write first block key & where block starts
      column_index_.push_back(block_index_.min_key(), out.file_pointer());

      assert(ctx_->buf_.size() >= INDEX_BLOCK_SIZE*sizeof(uint64_t));
      auto* buf = reinterpret_cast<uint64_t*>(&ctx_->buf_[0]);

      if (column_index_.full()) {
        column_index_.flush(blocks_index_.stream, buf);
      }

      // flush current block

      // write total number of elements in the block
      out.write_vint(block_index_.size());

      // write block index, compressed data and aggregate block properties
      // note that order of calls is important here, since it is not defined
      // which expression should be evaluated first in the following example:
      //   const auto res = expr0() | expr1();
      // otherwise it would violate format layout
      auto block_props = block_index_.flush(out, buf);
      block_props |= write_compact(out, ctx_->buf_, cipher_, *comp_, block_buf_);

      length_ += block_buf_.size();

      // refresh blocks properties
      blocks_props_ &= block_props;
      // reset buffer stream after flush
      block_buf_.clear();

      // refresh column properties
      // column is dense IFF
      // - all blocks are dense
      // - there are no gaps between blocks
      column_props_ &= ColumnProperty(0 != (block_props & CP_DENSE));
    }

    writer* ctx_; // writer context
    irs::type_info comp_type_;
    compression::compressor::ptr comp_; // compressor used for column
    encryption::stream* cipher_;
    uint64_t length_{}; // size of all data blocks in the column
    index_block<INDEX_BLOCK_SIZE> block_index_; // current block index (per document key/offset)
    index_block<INDEX_BLOCK_SIZE> column_index_; // column block index (per block key/offset)
    memory_output blocks_index_; // blocks index
    bstring block_buf_; // data buffer
    doc_id_t max_{ doc_limits::invalid() }; // max key (among flushed blocks)
    ColumnProperty blocks_props_{ CP_DENSE | CP_FIXED | CP_MASK }; // aggregated column blocks properties
    ColumnProperty column_props_{ CP_DENSE }; // aggregated column block index properties
    uint32_t avg_block_count_{}; // average number of items per block (tail block is not taken into account since it may skew distribution)
    uint32_t avg_block_size_{}; // average size of the block (tail block is not taken into account since it may skew distribution)
  }; // column

  memory_allocator* alloc_{ &memory_allocator::global() };
  std::deque<column> columns_; // pointers remain valid
  bstring buf_; // reusable temporary buffer for packing/compression
  index_output::ptr data_out_;
  std::string filename_;
  directory* dir_;
  encryption::stream::ptr data_out_cipher_;
  int32_t version_;
}; // writer

template<>
std::string file_name<columnstore_writer, segment_meta>(const segment_meta& meta) {
  return file_name(meta.name, columns::writer::FORMAT_EXT);
}

void writer::prepare(directory& dir, const segment_meta& meta) {
  columns_.clear();

  auto filename = file_name<columnstore_writer>(meta);
  auto data_out = dir.create(filename);

  if (!data_out) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s",
      filename.c_str()));
  }

  format_utils::write_header(*data_out, FORMAT_NAME, version_);

  encryption::stream::ptr data_out_cipher;

  if (version_ > FORMAT_MIN) {
    bstring enc_header;
    auto* enc = get_encryption(dir.attributes());

    const auto encrypt = irs::encrypt(filename, *data_out, enc, enc_header, data_out_cipher);
    assert(!encrypt || (data_out_cipher && data_out_cipher->block_size()));
    UNUSED(encrypt);
  }

  alloc_ = &directory_utils::get_allocator(dir);

  // noexcept block
  dir_ = &dir;
  data_out_ = std::move(data_out);
  data_out_cipher_ = std::move(data_out_cipher);
  filename_ = std::move(filename);
}

columnstore_writer::column_t writer::push_column(const column_info& info) {
  encryption::stream* cipher;
  irs::type_info compression;

  if (version_ > FORMAT_MIN) {
    compression = info.compression();
    cipher = info.encryption() ? data_out_cipher_.get() : nullptr;
  } else {
    // we don't support encryption and custom
    // compression for 'FORMAT_MIN' version
    compression = type<compression::lz4>::get();
    cipher = nullptr;
  }

  auto compressor = compression::get_compressor(compression, info.options());

  if (!compressor) {
    compressor = noop_compressor::make();
  }

  const auto id = columns_.size();
  columns_.emplace_back(*this, info.compression(), compressor, cipher);
  auto& column = columns_.back();

  return std::make_pair(id, [&column] (doc_id_t doc) -> column_output& {
    // to avoid extra (and useless in our case) check for block index
    // emptiness in 'writer::column::prepare', we disallow passing
    // doc <= doc_limits::invalid() || doc >= doc_limits::eof()
    assert(doc > doc_limits::invalid() && doc < doc_limits::eof());

    column.prepare(doc);
    return column;
  });
}

bool writer::commit() {
  assert(dir_);

  // remove all empty columns from tail
  while (!columns_.empty() && columns_.back().empty()) {
    columns_.pop_back();
  }

  // remove file if there is no data to write
  if (columns_.empty()) {
    data_out_.reset();

    if (!dir_->remove(filename_)) { // ignore error
      IR_FRMT_ERROR("Failed to remove file, path: %s", filename_.c_str());
    }

    return false; // nothing to flush
  }

  // flush all remain data including possible empty columns among filled columns
  for (auto& column : columns_) {
    // commit and flush remain blocks
    column.flush();
  }

  const auto block_index_ptr = data_out_->file_pointer(); // where blocks index start

  data_out_->write_vlong(columns_.size()); // number of columns

  for (auto& column : columns_) {
    column.finish(); // column blocks index
  }

  data_out_->write_long(block_index_ptr);
  format_utils::write_footer(*data_out_);

  rollback();

  return true;
}

void writer::rollback() noexcept {
  filename_.clear();
  dir_ = nullptr;
  data_out_.reset(); // close output
  columns_.clear(); // ensure next flush (without prepare(...)) will use the section without 'data_out_'
}

template<typename Block, typename Allocator>
class block_cache : irs::util::noncopyable {
 public:
  explicit block_cache(const Allocator& alloc = Allocator())
    : cache_(alloc) {
  }
  block_cache(block_cache&&) = default;

  template<typename... Args>
  Block& emplace_back(Args&&... args) {
    cache_.emplace_back(std::forward<Args>(args)...);
    return cache_.back();
  }

  void pop_back() noexcept {
    cache_.pop_back();
  }

 private:
  std::deque<Block, Allocator> cache_; // pointers remain valid
}; // block_cache

template<typename Block, typename Allocator>
struct block_cache_traits {
  typedef Block block_t;
  typedef typename std::allocator_traits<Allocator>::template rebind_alloc<block_t> allocator_t;
  typedef block_cache<Block, allocator_t> cache_t;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                            Blocks
// -----------------------------------------------------------------------------

class sparse_block : util::noncopyable {
 private:
  struct ref {
    doc_id_t key{ doc_limits::eof() };
    uint64_t offset;
  };

 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      next_ = std::lower_bound(
        begin_, end_, doc,
        [](const ref& lhs, doc_id_t rhs) {
          return lhs.key < rhs;
      });

      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (next_ == end_) {
        return false;
      }

      value_ = next_->key;
      const auto vbegin = next_->offset;

      begin_ = next_;
      const auto vend = (++next_ == end_ ? data_->size() : next_->offset);

      assert(vend >= vbegin);
      assert(payload_ != &DUMMY);
      *payload_ = bytes_ref(
        data_->c_str() + vbegin, // start
        vend - vbegin); // length
      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      payload_ = &DUMMY;
      next_ = begin_ = end_;
    }

    void reset(const sparse_block& block, irs::payload& payload) noexcept {
      value_ = doc_limits::invalid();
      payload.value = bytes_ref::NIL;
      payload_ = &payload.value;
      next_ = begin_ = std::begin(block.index_);
      end_ = block.end_;
      data_ = &block.data_;

      assert(std::is_sorted(
        begin_, end_,
        [](const sparse_block::ref& lhs, const sparse_block::ref& rhs) {
          return lhs.key < rhs.key;
      }));
    }

    bool operator==(const sparse_block& rhs) const noexcept {
      return data_ == &rhs.data_;
    }

    bool operator!=(const sparse_block& rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    irs::bytes_ref* payload_{ &DUMMY };
    irs::doc_id_t value_ { doc_limits::invalid() };
    const sparse_block::ref* next_{}; // next position
    const sparse_block::ref* begin_{};
    const sparse_block::ref* end_{};
    const bstring* data_{};
  }; // iterator

  void load(index_input& in,
            compression::decompressor* decomp,
            encryption::stream* cipher,
            bstring& buf) {
    const uint32_t size = in.read_vint(); // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'sparse_block' found in columnstore");
    }

    auto begin = std::begin(index_);

    // read keys
    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint32_t*>(&buf[0]),
      [begin](uint32_t key) mutable {
        begin->key = key;
        ++begin;
    });

    // read offsets
    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint64_t*>(&buf[0]),
      [begin](uint64_t offset) mutable {
        begin->offset = offset;
        ++begin;
    });

    // read data
    read_compact(in, cipher, decomp, buf, data_);
    end_ = index_ + size;
  }

  bool value(doc_id_t key, bytes_ref& out) const {
    // find the right ref
    auto it = std::lower_bound(
      index_, end_, key,
        [] (const ref& lhs, doc_id_t rhs) {
        return lhs.key < rhs;
    });

    if (end_ == it || key < it->key) {
      // no document with such id in the block
      return false;
    }

    if (data_.empty()) {
      // block without data_, but we've found a key
      return true;
    }

    const auto vbegin = it->offset;
    const auto vend = (++it == end_ ? data_.size() : it->offset);

    assert(vend >= vbegin);
    out = bytes_ref(
      data_.c_str() + vbegin, // start
      vend - vbegin); // length

    return true;
  }

  bool visit(const columnstore_reader::values_reader_f& visitor) const {
    bytes_ref value;

    // visit first [begin;end-1) blocks
    doc_id_t key;
    size_t vbegin;
    auto begin = std::begin(index_);
    for (const auto end = end_-1; begin != end;) { // -1 for tail item
      key = begin->key;
      vbegin = begin->offset;

      ++begin;

      assert(begin->offset >= vbegin);
      value = bytes_ref(
        data_.c_str() + vbegin, // start
        begin->offset - vbegin); // length

      if (!visitor(key, value)) {
        return false;
      }
    }

    // visit tail block
    assert(data_.size() >= begin->offset);
    value = bytes_ref(
      data_.c_str() + begin->offset, // start
      data_.size() - begin->offset); // length

    return visitor(begin->key, value);
  }

 private:
  // TODO: use single memory block for both index & data

  // all blocks except the tail are going to be fully filled,
  // so we don't track size of each block here since we could
  // waste just INDEX_BLOCK_SIZE*sizeof(ref)-1 per column
  // in the worst case
  ref index_[INDEX_BLOCK_SIZE];
  bstring data_;
  const ref* end_{ std::end(index_) };
}; // sparse_block

class dense_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      // before the current element
      if (doc <= value_) {
        doc = value_;
      }

      // FIXME refactor
      it_ = begin_ + doc - base_;

      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (it_ >= end_) {
        // after the last element
        return false;
      }

      value_ = base_ + std::distance(begin_, it_);
      next_value();

      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      payload_ = &DUMMY;
      it_ = begin_ = end_;
    }

    void reset(const dense_block& block, irs::payload& payload) noexcept {
      value_ = block.base_;
      payload.value = bytes_ref::NIL;
      payload_ = &payload.value;
      it_ = begin_ = std::begin(block.index_);
      end_ = block.end_;
      data_ = &block.data_;
      base_ = block.base_;
    }

    bool operator==(const dense_block& rhs) const noexcept {
      return data_ == &rhs.data_;
    }

    bool operator!=(const dense_block& rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    // note that function increments 'it_'
    void next_value() noexcept {
      const auto vbegin = *it_;
      const auto vend = (++it_ == end_ ? data_->size() : *it_);

      assert(vend >= vbegin);
      assert(payload_ != &DUMMY);
      *payload_ = bytes_ref(
        data_->c_str() + vbegin, // start
        vend - vbegin); // length
    }

    irs::bytes_ref* payload_{ &DUMMY };
    irs::doc_id_t value_ { doc_limits::invalid() };
    const uint32_t* begin_{};
    const uint32_t* it_{};
    const uint32_t* end_{};
    const bstring* data_{};
    doc_id_t base_{};
  }; // iterator

  void load(index_input& in,
            compression::decompressor* decomp,
            encryption::stream* cipher,
            bstring& buf) {
    const uint32_t size = in.read_vint(); // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'dense_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, base_, avg) || 1 != avg) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_block', base_key=%du, avg_delta=%du",
        base_, avg));
    }

    // read data offsets
    auto begin = std::begin(index_);

    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint64_t*>(&buf[0]),
      [begin](uint64_t offset) mutable {
        *begin = offset;
        ++begin;
    });

    // read data
    read_compact(in, cipher, decomp, buf, data_);
    end_ = index_ + size;
  }

  bool value(doc_id_t key, bytes_ref& out) const {
    const auto* begin = index_ + key - base_;
    if (begin < index_ || begin >= end_) {
      // there is no item with the speicified key
      return false;
    }

    if (data_.empty()) {
      // block without data, but we've found a key
      return true;
    }

    const auto vbegin = *begin;
    const auto vend = (++begin == end_ ? data_.size() : *begin);
    assert(vend >= vbegin);

    out = bytes_ref(data_.c_str() + vbegin, vend - vbegin);

    return true;
  }

  bool visit(const columnstore_reader::values_reader_f& visitor) const {
    bytes_ref value;

    // visit first [begin;end-1) blocks
    doc_id_t key = base_;
    size_t vbegin;
    auto begin = std::begin(index_);
    for (const auto end = end_ - 1; begin != end; ++key) { // -1 for tail item
      vbegin = *begin;

      ++begin;

      assert(*begin >= vbegin);
      value = bytes_ref(
        data_.c_str() + vbegin, // start
        *begin - vbegin); // length

      if (!visitor(key, value)) {
        return false;
      }
    }

    // visit tail block
    assert(data_.size() >= *begin);
    value = bytes_ref(
      data_.c_str() + *begin, // start
      data_.size() - *begin); // length

    return visitor(key, value);
  }

 private:
  // TODO: use single memory block for both index & data

  // all blocks except the tail are going to be fully filled,
  // so we don't track size of each block here since we could
  // waste just INDEX_BLOCK_SIZE*sizeof(ref)-1 per column
  // in the worst case
  uint32_t index_[INDEX_BLOCK_SIZE];
  bstring data_;
  uint32_t* end_{ index_ };
  doc_id_t base_{ };
}; // dense_block

class dense_fixed_offset_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      if (doc < value_next_) {
        if (!doc_limits::valid(value_)) {
          return next();
        }

        // don't seek backwards
        return true;
      }

      value_next_ = doc;
      return next();
    }

    const doc_id_t& value() const noexcept {
      return value_;
    }

    bool next() noexcept {
      if (value_next_ >= value_end_) {
        seal();
        return false;
      }

      value_ = value_next_++;
      const auto offset = (value_ - value_min_)*avg_length_;

      assert(payload_ != &DUMMY);
      *payload_ = bytes_ref(
        data_.c_str() + offset,
        value_ == value_back_ ? data_.size() - offset : avg_length_);

      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      value_next_ = doc_limits::eof();
      value_min_ = doc_limits::eof();
      value_end_ = doc_limits::eof();
      payload_ = &DUMMY;
    }

    void reset(const dense_fixed_offset_block& block, irs::payload& payload) noexcept {
      avg_length_ = block.avg_length_;
      data_ = block.data_;
      payload.value = bytes_ref::NIL;
      payload_ = &payload.value;
      value_ = doc_limits::invalid();
      value_next_ = block.base_key_;
      value_min_ = block.base_key_;
      value_end_ = value_min_ + block.size_;
      value_back_ = value_end_ - 1;
    }

    bool operator==(const dense_fixed_offset_block& rhs) const noexcept {
      return data_.c_str() == rhs.data_.c_str();
    }

    bool operator!=(const dense_fixed_offset_block& rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    uint64_t avg_length_{}; // average value length
    bytes_ref data_;
    irs::bytes_ref* payload_{ &DUMMY };
    doc_id_t value_ { doc_limits::invalid() }; // current value
    doc_id_t value_next_{ doc_limits::invalid() }; // next value
    doc_id_t value_min_{}; // min doc_id
    doc_id_t value_end_{}; // after the last valid doc id
    doc_id_t value_back_{}; // last valid doc id
  }; // iterator

  void load(index_input& in,
            compression::decompressor* decomp,
            encryption::stream* cipher,
            bstring& buf) {
    size_ = in.read_vint(); // total number of entries in a block

    if (!size_) {
      throw index_error("Empty 'dense_fixed_offset_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, base_key_, avg) || 1 != avg) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_fixed_offset_block', base_key=%du, avg_delta=%du",
        base_key_, avg));
    }

    // fixed length block must be encoded with RL encoding
    if (!encode::avg::read_block_rl32(in, base_offset_, avg_length_)) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_fixed_offset_block', base_offset=%du, avg_length=%du",
        base_key_, avg_length_));
    }

    // read data
    read_compact(in, cipher, decomp, buf, data_);
  }

  bool value(doc_id_t key, bytes_ref& out) const {
    key -= base_key_;

    if (key >= size_) {
      return false;
    }

    // expect 0-based key

    if (data_.empty()) {
      // block without data, but we've found a key
      return true;
    }

    const auto vbegin = base_offset_ + key*avg_length_;
    const auto vlength = (size_ == ++key ? data_.size() - vbegin : avg_length_);

    out = bytes_ref(data_.c_str() + vbegin, vlength);
    return true;
  }

  bool visit(const columnstore_reader::values_reader_f& visitor) const {
    assert(size_);

    bytes_ref value;

    // visit first 'size_-1' blocks
    doc_id_t key = base_key_;
    size_t offset = base_offset_;
    for (const doc_id_t end = key + size_ - 1;  key < end; ++key, offset += avg_length_) {
      value = bytes_ref(data_.c_str() + offset, avg_length_);
      if (!visitor(key, value)) {
        return false;
      }
    }

    // visit tail block
    assert(data_.size() >= offset);
    value = bytes_ref(
      data_.c_str() + offset, // start
      data_.size() - offset); // length

    return visitor(key, value);
  }

 private:
  doc_id_t base_key_{}; // base key
  uint32_t base_offset_{}; // base offset
  uint32_t avg_length_{}; // entry length
  doc_id_t size_{}; // total number of entries
  bstring data_;
}; // dense_fixed_offset_block

class sparse_mask_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      it_ = std::lower_bound(begin_, end_, doc);

      return next();
    }

    const irs::doc_id_t& value() const noexcept { return value_; }

    bool next() noexcept {
      if (it_ == end_) {
        return false;
      }

      begin_ = it_;
      value_ = *it_++;
      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      it_ = begin_ = end_;
    }

    void reset(const sparse_mask_block& block, irs::payload& payload) noexcept {
      value_ = doc_limits::invalid();
      payload.value = bytes_ref::NIL; // mask block doesn't have payload
      it_ = begin_ = std::begin(block.keys_);
      end_ = begin_ + block.size_;

      assert(std::is_sorted(begin_, end_));
    }

    bool operator==(const sparse_mask_block& rhs) const noexcept {
      return end_ == (rhs.keys_ + rhs.size_);
    }

    bool operator!=(const sparse_mask_block& rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    irs::doc_id_t value_ { doc_limits::invalid() };
    const doc_id_t* it_{};
    const doc_id_t* begin_{};
    const doc_id_t* end_{};
  }; // iterator

  sparse_mask_block() noexcept {
    std::fill(
      std::begin(keys_), std::end(keys_),
      doc_limits::eof());
  }

  void load(index_input& in,
            compression::decompressor* /*decomp*/,
            encryption::stream* /*cipher*/,
            bstring& buf) {
    size_ = in.read_vint(); // total number of entries in a block

    if (!size_) {
      throw index_error("Empty 'sparse_mask_block' found in columnstore");
    }

    auto begin = std::begin(keys_);

    encode::avg::visit_block_packed_tail(
      in, size_, reinterpret_cast<uint32_t*>(&buf[0]),
      [begin](uint32_t key) mutable {
        *begin++ = key;
    });

    // mask block has no data, so all offsets should be equal to 0
    if (!encode::avg::check_block_rl64(in, 0)) {
      throw index_error("'sparse_mask_block' expected to contain no data");
    }
  }

  bool value(doc_id_t key, bytes_ref& /*reader*/) const {
    // we don't evaluate 'end' here as 'keys_ + size_'
    // since it all blocks except the tail one are
    // going to be fully filled, that allows compiler
    // to generate better optimized code

    const auto it = std::lower_bound(
      std::begin(keys_), std::end(keys_), key);

    return !(std::end(keys_) == it || *it > key);
  }

  bool visit(const columnstore_reader::values_reader_f& reader) const {
    for (auto begin = std::begin(keys_), end = begin + size_; begin != end; ++begin) {
      if (!reader(*begin, DUMMY)) {
        return false;
      }
    }
    return true;
  }

 private:
  // all blocks except the tail one are going to be fully filled,
  // so we store keys in a fixed length array since we could
  // waste just INDEX_BLOCK_SIZE*sizeof(ref)-1 per column
  // in the worst case
  doc_id_t keys_[INDEX_BLOCK_SIZE];
  doc_id_t size_{}; // number of documents in a block
}; // sparse_mask_block

class dense_mask_block {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) noexcept {
      if (doc < doc_) {
        if (!doc_limits::valid(value_)) {
          return next();
        }

        // don't seek backwards
        return true;
      }

      doc_ = doc;
      return next();
    }

    const irs::doc_id_t& value() const noexcept {
      return value_;
    }

    bool next() noexcept {
      if (doc_ >= max_) {
        seal();
        return false;
      }

      value_ = doc_++;

      return true;
    }

    void seal() noexcept {
      value_ = doc_limits::eof();
      doc_ = max_;
    }

    void reset(const dense_mask_block& block, irs::payload& payload) noexcept {
      block_ = &block;
      payload.value = bytes_ref::NIL; // mask block doesn't have payload
      doc_ = block.min_;
      max_ = block.max_;
    }

    bool operator==(const dense_mask_block& rhs) const noexcept {
      return block_ == &rhs;
    }

    bool operator!=(const dense_mask_block& rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    const dense_mask_block* block_{};
    doc_id_t value_{ doc_limits::invalid() };
    doc_id_t doc_{ doc_limits::invalid() };
    doc_id_t max_{ doc_limits::invalid() };
  }; // iterator

  dense_mask_block() noexcept
    : min_(doc_limits::invalid()),
      max_(doc_limits::invalid()) {
  }

  void load(index_input& in,
            compression::decompressor* /*decomp*/,
            encryption::stream* /*cipher*/,
            bstring& /*buf*/) {
    const auto size = in.read_vint(); // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'dense_mask_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, min_, avg) || 1 != avg) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_mask_block', base_key=%du, avg_delta=%du",
        min_, avg));
    }

    // mask block has no data, so all offsets should be equal to 0
    if (!encode::avg::check_block_rl64(in, 0)) {
      throw index_error("'dense_mask_block' expected to contain no data");
    }

    max_ = min_ + size;
  }

  bool value(doc_id_t key, bytes_ref& /*reader*/) const noexcept {
    return min_ <= key && key < max_;
  }

  bool visit(const columnstore_reader::values_reader_f& visitor) const {
    for (auto doc = min_; doc < max_; ++doc) {
      if (!visitor(doc, DUMMY)) {
        return false;
      }
    }

    return true;
  }

 private:
  doc_id_t min_;
  doc_id_t max_;
}; // dense_mask_block

template<typename Allocator = std::allocator<sparse_block>>
class read_context
  : public block_cache_traits<sparse_block, Allocator>::cache_t,
    public block_cache_traits<dense_block, Allocator>::cache_t,
    public block_cache_traits<dense_fixed_offset_block, Allocator>::cache_t,
    public block_cache_traits<sparse_mask_block, Allocator>::cache_t,
    public block_cache_traits<dense_mask_block, Allocator>::cache_t {
 public:
  using ptr = std::shared_ptr<read_context>;

  static ptr make(const index_input& stream, encryption::stream* cipher) {
    auto clone = stream.reopen(); // reopen thead-safe stream

    if (!clone) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen columpstore input in: %s", __FUNCTION__);

      throw io_error("Failed to reopen columnstore input in");
    }

    return memory::make_shared<read_context>(std::move(clone), cipher);
  }

  read_context(
      index_input::ptr&& in,
      encryption::stream* cipher,
      const Allocator& alloc = Allocator())
    : block_cache_traits<sparse_block, Allocator>::cache_t(typename block_cache_traits<sparse_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_block, Allocator>::cache_t(typename block_cache_traits<dense_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_fixed_offset_block, Allocator>::cache_t(typename block_cache_traits<dense_fixed_offset_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<sparse_mask_block, Allocator>::cache_t(typename block_cache_traits<sparse_mask_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_mask_block, Allocator>::cache_t(typename block_cache_traits<dense_mask_block, Allocator>::allocator_t(alloc)),
      buf_(INDEX_BLOCK_SIZE*sizeof(uint32_t), 0),
      stream_(std::move(in)),
      cipher_(cipher) {
  }

  template<typename Block, typename... Args>
  Block& emplace_back(uint64_t offset, compression::decompressor* decomp, bool decrypt, Args&&... args) {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;

    // add cache entry
    auto& block = cache.emplace_back(std::forward<Args>(args)...);

    try {
      load(block, decomp, decrypt, offset);
    } catch (...) {
      // failed to load block
      pop_back<Block>();

      throw;
    }

    return block;
  }

  template<typename Block>
  void load(Block& block, compression::decompressor* decomp, bool decrypt, uint64_t offset) {
    stream_->seek(offset); // seek to the offset
    block.load(*stream_, decomp, decrypt ? cipher_ : nullptr, buf_);
  }

  template<typename Block>
  void pop_back() noexcept {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;
    cache.pop_back();
  }

 private:
  bstring buf_; // temporary buffer for decoding/unpacking
  index_input::ptr stream_;
  encryption::stream* cipher_; // options cipher stream
}; // read_context

typedef read_context<> read_context_t;

class context_provider: private util::noncopyable {
 public:
  explicit context_provider(size_t max_pool_size)
    : pool_(std::max(size_t(1), max_pool_size)) {
  }

  void prepare(index_input::ptr&& stream, encryption::stream::ptr&& cipher) noexcept {
    assert(stream);

    stream_ = std::move(stream);
    cipher_ = std::move(cipher);
  }

  bounded_object_pool<read_context_t>::ptr get_context() const {
    return pool_.emplace(*stream_, cipher_.get());
  }

 private:
  mutable bounded_object_pool<read_context_t> pool_;
  encryption::stream::ptr cipher_;
  index_input::ptr stream_;
}; // context_provider

// in case of success caches block pointed
// instance, nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t& load_block(
    const context_provider& ctxs,
    compression::decompressor* decomp,
    bool decrypt,
    BlockRef& ref) {
  typedef typename BlockRef::block_t block_t;

  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();
    assert(ctx);

    // load block
    const auto& block = ctx->template emplace_back<block_t>(ref.offset, decomp, decrypt);

    // mark block as loaded
    if (ref.pblock.compare_exchange_strong(cached, &block)) {
      cached = &block;
    } else {
      // already cached by another thread
      ctx->template pop_back<block_t>();
    }
  }

  return *cached;
}

// in case of success caches block pointed
// by 'ref' in the specified 'block' and
// retuns a pointer to cached instance,
// nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t& load_block(
    const context_provider& ctxs,
    compression::decompressor* decomp,
    bool decrypt,
    const BlockRef& ref,
    typename BlockRef::block_t& block) {
  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();
    assert(ctx);

    ctx->load(block, decomp, decrypt, ref.offset);

    cached = &block;
  }

  return *cached;
}

////////////////////////////////////////////////////////////////////////////////
/// @class column
////////////////////////////////////////////////////////////////////////////////
class column
    : public irs::columnstore_reader::column_reader,
      private util::noncopyable {
 public:
  using ptr = std::unique_ptr<column>;

  explicit column(ColumnProperty props) noexcept
    : props_(props),
      encrypted_(0 != (props & CP_COLUMN_ENCRYPT)) {
  }

  virtual ~column() = default;

  virtual void read(data_input& in, uint64_t* /*buf*/, compression::decompressor::ptr decomp) {
    count_ = in.read_vint();
    max_ = in.read_vint();
    avg_block_size_ = in.read_vint();
    avg_block_count_ = in.read_vint();
    if (!avg_block_count_) {
      avg_block_count_ = count_;
    }
    decomp_ = decomp;
  }

  bool encrypted() const noexcept { return encrypted_; }
  doc_id_t max() const noexcept { return max_; }
  virtual size_t size() const noexcept override { return count_; }
  bool empty() const noexcept { return 0 == size(); }
  uint32_t avg_block_size() const noexcept { return avg_block_size_; }
  uint32_t avg_block_count() const noexcept { return avg_block_count_; }
  ColumnProperty props() const noexcept { return props_; }
  compression::decompressor* decompressor() const noexcept { return decomp_.get(); }

 protected:
  // same as size() but returns uint32_t to avoid type convertions
  uint32_t count() const noexcept { return count_; }

 private:
  compression::decompressor::ptr decomp_;
  doc_id_t max_{ doc_limits::eof() };
  uint32_t count_{};
  uint32_t avg_block_size_{};
  uint32_t avg_block_count_{};
  ColumnProperty props_{ CP_SPARSE };
  bool encrypted_{ false }; // cached encryption mark
}; // column

template<typename Column>
class column_iterator final : public irs::doc_iterator {
 private:
  using attributes = std::tuple<
    document, cost, score, payload>;

 public:
  typedef Column column_t;
  typedef typename Column::block_t block_t;
  typedef typename block_t::iterator block_iterator_t;

  explicit column_iterator(
      const column_t& column,
      const typename column_t::block_ref* begin,
      const typename column_t::block_ref* end)
    : begin_(begin),
      seek_origin_(begin),
      end_(end),
      column_(&column) {
    std::get<cost>(attrs_).reset(column.size());
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::get_mutable(attrs_, type);
  }

  virtual doc_id_t value() const noexcept override {
    return std::get<document>(attrs_).value;
  }

  virtual doc_id_t seek(irs::doc_id_t doc) override {
    begin_ = column_->find_block(seek_origin_, end_, doc);

    if (!next_block()) {
      return value();
    }

    if (!block_.seek(doc)) {
      // reached the end of block,
      // advance to the next one
      while (next_block() && !block_.next()) { }
    }

    std::get<document>(attrs_).value = block_.value();
    return value();
  }

  virtual bool next() override {
    while (!block_.next()) {
      if (!next_block()) {
        return false;
      }
    }

    std::get<document>(attrs_).value = block_.value();
    return true;
  }

 private:
  typedef typename column_t::refs_t refs_t;

  bool next_block() {
    auto& doc = std::get<document>(attrs_);
    auto& payload = std::get<irs::payload>(attrs_);

    if (begin_ == end_) {
      // reached the end of the column
      block_.seal();
      seek_origin_ = end_;
      payload.value = bytes_ref::NIL;
      doc.value = irs::doc_limits::eof();

      return false;
    }

    try {
      const auto& cached = load_block(*column_->ctxs_, column_->decompressor(), column_->encrypted(), *begin_);

      if (block_ != cached) {
        block_.reset(cached, payload);
      }
    } catch (...) {
      // unable to load block, seal the iterator
      block_.seal();
      begin_ = end_;
      payload.value = bytes_ref::NIL;
      doc.value = irs::doc_limits::eof();

      throw;
    }

    seek_origin_ = begin_++;

    return true;
  }

  block_iterator_t block_;
  attributes attrs_;
  const typename column_t::block_ref* begin_;
  const typename column_t::block_ref* seek_origin_;
  const typename column_t::block_ref* end_;
  const column_t* column_;
}; // column_iterator

// -----------------------------------------------------------------------------
// --SECTION--                                                           Columns
// -----------------------------------------------------------------------------

template<typename Column>
columnstore_reader::values_reader_f column_values(const Column& column) {
if (column.empty()) {
    return columnstore_reader::empty_reader();
  }

  return [&column](doc_id_t key, bytes_ref& value) {
    return column.value(key, value);
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @class sparse_column
////////////////////////////////////////////////////////////////////////////////
template<typename Block>
class sparse_column final : public column {
 public:
  typedef sparse_column column_t;
  typedef Block block_t;

  static column::ptr make(const context_provider& ctxs, ColumnProperty props) {
    return memory::make_unique<column_t>(ctxs, props);
  }

  sparse_column(const context_provider& ctxs, ColumnProperty props)
    : column(props), ctxs_(&ctxs) {
  }

  virtual void read(data_input& in, uint64_t* buf, compression::decompressor::ptr decomp) override {
    column::read(in, buf, decomp); // read common header

    uint32_t blocks_count = in.read_vint(); // total number of column index blocks

    std::vector<block_ref> refs(blocks_count + 1); // +1 for upper bound

    auto begin = refs.begin();
    while (blocks_count >= INDEX_BLOCK_SIZE) {
      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, reinterpret_cast<uint32_t*>(buf),
        [begin](uint32_t key) mutable {
          begin->key = key;
          ++begin;
      });

      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, buf,
        [begin](uint64_t offset) mutable {
          begin->offset = offset;
          ++begin;
      });

      begin += INDEX_BLOCK_SIZE;
      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      encode::avg::visit_block_packed_tail(
        in, blocks_count, reinterpret_cast<uint32_t*>(buf),
        [begin](uint32_t key) mutable {
          begin->key = key;
          ++begin;
      });

      encode::avg::visit_block_packed_tail(
        in, blocks_count, buf,
        [begin](uint64_t offset) mutable {
          begin->offset = offset;
          ++begin;
      });

      begin += blocks_count;
    }

    // upper bound
    if (this->max() < doc_limits::eof()) {
      begin->key = this->max() + 1;
    } else {
      begin->key = doc_limits::eof();
    }
    begin->offset = type_limits<type_t::address_t>::invalid();

    refs_ = std::move(refs);
  }

  bool value(doc_id_t key, bytes_ref& value) const {
    // find the right block
    const auto rbegin = refs_.rbegin(); // upper bound
    const auto rend = refs_.rend();
    const auto it = std::lower_bound(
      rbegin, rend, key,
      [] (const block_ref& lhs, doc_id_t rhs) {
        return lhs.key > rhs;
    });

    if (it == rend || it == rbegin) {
      return false;
    }

    const auto& cached = load_block(*ctxs_, decompressor(), encrypted(), *it);

    return cached.value(key, value);
  }

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor) const override {
    block_t block; // don't cache new blocks
    for (auto begin = refs_.begin(), end = refs_.end()-1; begin != end; ++begin) { // -1 for upper bound
      const auto& cached = load_block(*ctxs_, decompressor(), encrypted(), *begin, block);

      if (!cached.visit(visitor)) {
        return false;
      }
    }
    return true;
  }

  virtual irs::doc_iterator::ptr iterator() const override {
    typedef column_iterator<column_t> iterator_t;

    if (empty()) {
      return irs::doc_iterator::empty();
    }

    return memory::make_managed<iterator_t>(
      *this,
      refs_.data(),
      refs_.data() + refs_.size() - 1); // -1 for upper bound
  }

  virtual columnstore_reader::values_reader_f values() const override {
    return column_values<column_t>(*this);
  }

 private:
  friend class column_iterator<column_t>;

  struct block_ref : util::noncopyable {
    typedef typename column_t::block_t block_t;

    block_ref() = default;

    block_ref(block_ref&& other) noexcept
      : key(std::move(other.key)), offset(std::move(other.offset)) {
      pblock = other.pblock.exchange(nullptr); // no std::move(...) for std::atomic<...>
    }

    doc_id_t key; // min key in a block
    uint64_t offset; // block offset
    mutable std::atomic<const block_t*> pblock; // pointer to cached block
  }; // block_ref

  typedef std::vector<block_ref> refs_t;

  const block_ref* find_block(
      const block_ref* begin,
      const block_ref* end,
      doc_id_t key) const noexcept {
    UNUSED(end);

    if (key <= begin->key) {
      return begin;
    }

    const auto rbegin = irstd::make_reverse_iterator(refs_.data() + refs_.size()); // upper bound
    const auto rend = irstd::make_reverse_iterator(begin);
    const auto it = std::lower_bound(
      rbegin, rend, key,
      [] (const block_ref& lhs, doc_id_t rhs) {
        return lhs.key > rhs;
    });

    if (it == rend) {
      return &*rbegin;
    }

    return it.base()-1;
  }

  typename refs_t::const_iterator find_block(doc_id_t key) const noexcept {
    if (key <= refs_.front().key) {
      return refs_.begin();
    }

    const auto rbegin = refs_.rbegin(); // upper bound
    const auto rend = refs_.rend();
    const auto it = std::lower_bound(
      rbegin, rend, key,
      [] (const block_ref& lhs, doc_id_t rhs) {
        return lhs.key > rhs;
    });

    if (it == rend) {
      return refs_.end() - 1; // -1 for upper bound
    }

    return irstd::to_forward(it);
  }

  const context_provider* ctxs_;
  refs_t refs_; // blocks index
}; // sparse_column

////////////////////////////////////////////////////////////////////////////////
/// @class dense_fixed_offset_column
////////////////////////////////////////////////////////////////////////////////
template<typename Block>
class dense_fixed_offset_column final : public column {
 public:
  typedef dense_fixed_offset_column column_t;
  typedef Block block_t;

  static column::ptr make(const context_provider& ctxs, ColumnProperty props) {
    return memory::make_unique<column_t>(ctxs, props);
  }

  dense_fixed_offset_column(const context_provider& ctxs, ColumnProperty prop)
    : column(prop), ctxs_(&ctxs) {
  }

  virtual void read(data_input& in, uint64_t* buf, compression::decompressor::ptr decomp) override {
    column::read(in, buf, decomp); // read common header

    size_t blocks_count = in.read_vint(); // total number of column index blocks

    std::vector<block_ref> refs(blocks_count);

    auto begin = refs.begin();
    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl32(in, this->avg_block_count())) {
        throw index_error("Invalid RL encoding in 'dense_fixed_offset_column' (keys)");
      }

      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, buf,
        [begin](uint64_t offset) mutable {
          begin->offset = offset;
          ++begin;
      });

      begin += INDEX_BLOCK_SIZE;
      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      const auto avg_block_count = blocks_count > 1
        ? this->avg_block_count()
        : 0U; // in this case avg == 0

      if (!encode::avg::check_block_rl32(in, avg_block_count)) {
        throw index_error("Invalid RL encoding in 'dense_fixed_offset_column' (keys)");
      }

      encode::avg::visit_block_packed_tail(
        in, blocks_count, buf,
        [begin](uint64_t offset) mutable {
          begin->offset = offset;
          ++begin;
      });

      begin += blocks_count;
    }

    refs_ = std::move(refs);
    min_ = this->max() - this->count() + 1;
  }

  bool value(doc_id_t key, bytes_ref& value) const {
    const auto base_key = key - min_;

    if (base_key >= this->count()) {
      return false;
    }

    const auto block_idx = base_key / this->avg_block_count();
    assert(block_idx < refs_.size());

    auto& ref = const_cast<block_ref&>(refs_[block_idx]);

    const auto& cached = load_block(*ctxs_, decompressor(), encrypted(), ref);

    return cached.value(key, value);
  }

  virtual bool visit(const columnstore_reader::values_visitor_f& visitor) const override {
    block_t block; // don't cache new blocks
    for (auto& ref : refs_) {
      const auto& cached = load_block(*ctxs_, decompressor(), encrypted(), ref, block);

      if (!cached.visit(visitor)) {
        return false;
      }
    }

    return true;
  }

  virtual irs::doc_iterator::ptr iterator() const override {
    typedef column_iterator<column_t> iterator_t;

    if (empty()) {
      return irs::doc_iterator::empty();
    }

    return memory::make_managed<iterator_t>(
      *this,
      refs_.data(),
      refs_.data() + refs_.size());
  }

  virtual columnstore_reader::values_reader_f values() const override {
    return column_values<column_t>(*this);
  }

 private:
  friend class column_iterator<column_t>;

  struct block_ref {
    typedef typename column_t::block_t block_t;

    block_ref() = default;

    block_ref(block_ref&& other) noexcept
      : offset(std::move(other.offset)) {
      pblock = other.pblock.exchange(nullptr); // no std::move(...) for std::atomic<...>
    }

    uint64_t offset; // need to store base offset since blocks may not be located sequentially
    mutable std::atomic<const block_t*> pblock;
  }; // block_ref

  typedef std::vector<block_ref> refs_t;

  const block_ref* find_block(
      const block_ref* begin,
      const block_ref* end,
      doc_id_t key) const noexcept {
    const auto min  = min_ + this->avg_block_count() * std::distance(refs_.data(), begin);

    if (key < min) {
      return begin;
    }

    if ((key -= min_) >= this->count()) {
      return end;
    }

    const auto block_idx = key / this->avg_block_count();
    assert(block_idx < refs_.size());

    return refs_.data() + block_idx;
  }

  typename refs_t::const_iterator find_block(doc_id_t key) const noexcept {
    if (key < min_) {
      return refs_.begin();
    }

    if ((key -= min_) >= this->count()) {
      return refs_.end();
    }

    const auto block_idx = key / this->avg_block_count();
    assert(block_idx < refs_.size());

    return std::begin(refs_) + block_idx;
  }

  const context_provider* ctxs_;
  refs_t refs_;
  doc_id_t min_{}; // min key
}; // dense_fixed_offset_column

template<>
class dense_fixed_offset_column<dense_mask_block> final : public column {
 public:
  typedef dense_fixed_offset_column column_t;

  static column::ptr make(const context_provider&, ColumnProperty props) {
    return memory::make_unique<column_t>(props);
  }

  explicit dense_fixed_offset_column(ColumnProperty prop) noexcept
    : column(prop) {
  }

  virtual void read(data_input& in, uint64_t* buf, compression::decompressor::ptr decomp) override {
    // we treat data in blocks as "garbage" which could be
    // potentially removed on merge, so we don't validate
    // column properties using such blocks

    column::read(in, buf, decomp); // read common header

    uint32_t blocks_count = in.read_vint(); // total number of column index blocks

    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl32(in, this->avg_block_count())) {
        throw index_error("Invalid RL encoding in 'dense_fixed_offset_column<dense_mask_block>' (keys)");
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, buf,
        [](uint64_t) {});

      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      const auto avg_block_count = blocks_count > 1
        ? this->avg_block_count()
        : 0; // in this case avg == 0

      if (!encode::avg::check_block_rl32(in, avg_block_count)) {
        throw index_error("Invalid RL encoding in 'dense_fixed_offset_column<dense_mask_block>' (keys)");
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed_tail(
        in, blocks_count, buf,
        [](uint64_t) { });
    }


    min_ = this->max() - this->count();
  }

  bool value(doc_id_t key, bytes_ref& value) const noexcept {
    value = bytes_ref::NIL;
    return key > min_ && key <= this->max();
  }

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor) const override {
    auto doc = min_;

    for (auto left = this->size(); left; --left) {
      if (!visitor(++doc, bytes_ref::NIL)) {
        return false;
      }
    }

    return true;
  }

  virtual irs::doc_iterator::ptr iterator() const override;

  virtual columnstore_reader::values_reader_f values() const override {
    return column_values<column_t>(*this);
  }

 private:
  class column_iterator final : public irs::doc_iterator {
   private:
    using attributes = std::tuple<
      document, cost, score>;

   public:
    explicit column_iterator(const column_t& column) noexcept
      : min_(1 + column.min_),
        max_(column.max()) {
      std::get<cost>(attrs_).reset(column.size());
    }

    virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override final {
      return irs::get_mutable(attrs_, type);
    }

    virtual irs::doc_id_t value() const noexcept override {
      return std::get<document>(attrs_).value;
    }

    virtual irs::doc_id_t seek(irs::doc_id_t doc) noexcept override {
      auto& value = std::get<document>(attrs_);

      if (doc < min_) {
        if (!doc_limits::valid(value.value)) {
          next();
        }

        return this->value();
      }

      min_ = doc;
      next();

      return value.value;
    }

    virtual bool next() noexcept override {
      auto& value = std::get<document>(attrs_);

      if (min_ > max_) {
        value.value = doc_limits::eof();

        return false;
      }


      value.value = min_++;

      return true;
    }

   private:
    attributes attrs_;
    doc_id_t min_{ doc_limits::invalid() };
    doc_id_t max_{ doc_limits::invalid() };
  }; // column_iterator

  doc_id_t min_{}; // min key (less than any key in column)
}; // dense_fixed_offset_column

irs::doc_iterator::ptr dense_fixed_offset_column<dense_mask_block>::iterator() const {
  return empty()
    ? irs::doc_iterator::empty()
    : memory::make_managed<column_iterator>(*this);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                 column factories
// ----------------------------------------------------------------------------

typedef std::function<
  column::ptr(const context_provider& ctxs, ColumnProperty prop)
> column_factory_f;
                                                               //     Column      |          Blocks
const column_factory_f COLUMN_FACTORIES[] {                    // CP_COLUMN_DENSE | CP_MASK CP_FIXED CP_DENSE
  &sparse_column<sparse_block>::make,                          //       0         |    0        0        0
  &sparse_column<dense_block>::make,                           //       0         |    0        0        1
  &sparse_column<sparse_block>::make,                          //       0         |    0        1        0
  &sparse_column<dense_fixed_offset_block>::make,              //       0         |    0        1        1
  nullptr, /* invalid properties, should never happen */       //       0         |    1        0        0
  nullptr, /* invalid properties, should never happen */       //       0         |    1        0        1
  &sparse_column<sparse_mask_block>::make,                     //       0         |    1        1        0
  &sparse_column<dense_mask_block>::make,                      //       0         |    1        1        1

  &sparse_column<sparse_block>::make,                          //       1         |    0        0        0
  &sparse_column<dense_block>::make,                           //       1         |    0        0        1
  &sparse_column<sparse_block>::make,                          //       1         |    0        1        0
  &dense_fixed_offset_column<dense_fixed_offset_block>::make,  //       1         |    0        1        1
  nullptr, /* invalid properties, should never happen */       //       1         |    1        0        0
  nullptr, /* invalid properties, should never happen */       //       1         |    1        0        1
  &sparse_column<sparse_mask_block>::make,                     //       1         |    1        1        0
  &dense_fixed_offset_column<dense_mask_block>::make           //       1         |    1        1        1
};

//////////////////////////////////////////////////////////////////////////////
/// @class reader
//////////////////////////////////////////////////////////////////////////////
class reader final: public columnstore_reader, public context_provider {
 public:
  explicit reader(size_t pool_size = 16)
    : context_provider(pool_size) {
  }

  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta) override;

  virtual const column_reader* column(field_id field) const override;

  virtual size_t size() const noexcept override {
    return columns_.size();
  }

 private:
  std::vector<column::ptr> columns_;
}; // reader

bool reader::prepare(const directory& dir, const segment_meta& meta) {
  const auto filename = file_name<columnstore_writer>(meta);
  bool exists;

  if (!dir.exists(exists, filename)) {
    throw io_error(string_utils::to_string(
      "failed to check existence of file, path: %s",
      filename.c_str()));
  }

  if (!exists) {
    // possible that the file does not exist since columnstore is optional
    return false;
  }

  // open columstore stream
  auto stream = dir.open(filename, irs::IOAdvice::RANDOM);

  if (!stream) {
    throw io_error(string_utils::to_string(
      "Failed to open file, path: %s",
      filename.c_str()));
  }

  // check header
  const auto version = format_utils::check_header(
    *stream,
    writer::FORMAT_NAME,
    writer::FORMAT_MIN,
    writer::FORMAT_MAX);

  encryption::stream::ptr cipher;

  if (version > writer::FORMAT_MIN) {
    auto* enc = get_encryption(dir.attributes());

    if (irs::decrypt(filename, *stream, enc, cipher)) {
      assert(cipher && cipher->block_size());
    }
  }

  // since columns data are too large
  // it is too costly to verify checksum of
  // the entire file. here we perform cheap
  // error detection which could recognize
  // some forms of corruption
  format_utils::read_checksum(*stream);

  // seek to data start
  stream->seek(stream->length() - format_utils::FOOTER_LEN - sizeof(uint64_t));
  stream->seek(stream->read_long()); // seek to blocks index

  uint64_t buf[INDEX_BLOCK_SIZE]; // temporary buffer for bit packing
  std::vector<column::ptr> columns;
  columns.reserve(stream->read_vlong());
  for (size_t i = 0, size = columns.capacity(); i < size; ++i) {
    // read column properties
    const auto props = read_enum<ColumnProperty>(*stream);
    const auto factory_id = (props & (~CP_COLUMN_ENCRYPT));

    if (factory_id >= IRESEARCH_COUNTOF(COLUMN_FACTORIES)) {
      throw index_error(string_utils::to_string(
        "Failed to load column id=" IR_SIZE_T_SPECIFIER ", got invalid properties=%d",
        i, static_cast<uint32_t>(props)));
    }

    // create column
    const auto& factory = COLUMN_FACTORIES[factory_id];

    if (!factory) {
      static_assert(
        std::is_same<std::underlying_type<ColumnProperty>::type, uint32_t>::value,
        "Enum 'ColumnProperty' has different underlying type");

      throw index_error(string_utils::to_string(
        "Failed to open column id=" IR_SIZE_T_SPECIFIER ", properties=%d",
        i, static_cast<uint32_t>(props)));
    }

    auto column = factory(*this, props);

    if (!column) {
      throw index_error(string_utils::to_string(
        "Factory failed to create column id=" IR_SIZE_T_SPECIFIER, i));
    }

    compression::decompressor::ptr decomp;

    if (version > writer::FORMAT_MIN) {
      const auto compression_id = read_string<std::string>(*stream);
      decomp = compression::get_decompressor(compression_id);

      if (!decomp && !compression::exists(compression_id)) {
        throw index_error(string_utils::to_string(
          "Failed to load compression '%s' for column id=" IR_SIZE_T_SPECIFIER,
          compression_id.c_str(), i));
      }

      if (decomp && !decomp->prepare(*stream)) {
        throw index_error(string_utils::to_string(
          "Failed to prepare compression '%s' for column id=" IR_SIZE_T_SPECIFIER,
          compression_id.c_str(), i));
      }
    } else {
      // we don't support encryption and custom
      // compression for 'FORMAT_MIN' version
      decomp = compression::get_decompressor(type<compression::lz4>::get());
      assert(decomp);
    }

    try {
      column->read(*stream, buf, decomp);
    } catch (...) {
      IR_FRMT_ERROR("Failed to load column id=" IR_SIZE_T_SPECIFIER, i);

      throw;
    }

    // noexcept since space has been already reserved
    columns.emplace_back(std::move(column));
  }

  // noexcept
  context_provider::prepare(std::move(stream), std::move(cipher));
  columns_ = std::move(columns);

  return true;
}

const reader::column_reader* reader::column(field_id field) const {
  return field >= columns_.size()
    ? nullptr // can't find column with the specified identifier
    : columns_[field].get();
}

} // columns

// ----------------------------------------------------------------------------
// --SECTION--                                                  postings_reader
// ----------------------------------------------------------------------------

class postings_reader_base : public irs::postings_reader {
 public:
  virtual void prepare(
    index_input& in,
    const reader_state& state,
    const flags& features) final;

  virtual size_t decode(
    const byte_type* in,
    const flags& field,
    irs::term_meta& state) final;

 protected:
  index_input::ptr doc_in_;
  index_input::ptr pos_in_;
  index_input::ptr pay_in_;
}; // postings_reader

void postings_reader_base::prepare(
    index_input& in,
    const reader_state& state,
    const flags& features) {
  std::string buf;

  // prepare document input
  prepare_input(
    buf, doc_in_, irs::IOAdvice::RANDOM, state,
    postings_writer_base::DOC_EXT,
    postings_writer_base::DOC_FORMAT_NAME,
    postings_writer_base::FORMAT_MIN,
    postings_writer_base::FORMAT_MAX);

  // Since terms doc postings too large
  //  it is too costly to verify checksum of
  //  the entire file. Here we perform cheap
  //  error detection which could recognize
  //  some forms of corruption.
  format_utils::read_checksum(*doc_in_);

  if (features.check<irs::position>()) {
    /* prepare positions input */
    prepare_input(
      buf, pos_in_, irs::IOAdvice::RANDOM, state,
      postings_writer_base::POS_EXT,
      postings_writer_base::POS_FORMAT_NAME,
      postings_writer_base::FORMAT_MIN,
      postings_writer_base::FORMAT_MAX);

    // Since terms pos postings too large
    // it is too costly to verify checksum of
    // the entire file. Here we perform cheap
    // error detection which could recognize
    // some forms of corruption.
    format_utils::read_checksum(*pos_in_);

    if (features.check<payload>() || features.check<offset>()) {
      // prepare positions input
      prepare_input(
        buf, pay_in_, irs::IOAdvice::RANDOM, state,
        postings_writer_base::PAY_EXT,
        postings_writer_base::PAY_FORMAT_NAME,
        postings_writer_base::FORMAT_MIN,
        postings_writer_base::FORMAT_MAX);

      // Since terms pos postings too large
      // it is too costly to verify checksum of
      // the entire file. Here we perform cheap
      // error detection which could recognize
      // some forms of corruption.
      format_utils::read_checksum(*pay_in_);
    }
  }

  // check postings format
  format_utils::check_header(in,
    postings_writer_base::TERMS_FORMAT_NAME,
    postings_writer_base::TERMS_FORMAT_MIN,
    postings_writer_base::TERMS_FORMAT_MAX);

  const uint64_t block_size = in.read_vint();

  if (block_size != postings_writer_base::BLOCK_SIZE) {
    throw index_error(string_utils::to_string(
      "while preparing postings_reader, error: invalid block size '" IR_UINT64_T_SPECIFIER "'",
      block_size));
  }
}

size_t postings_reader_base::decode(
    const byte_type* in,
    const flags& meta,
    irs::term_meta& state) {
  auto& term_meta = static_cast<version10::term_meta&>(state);

  const bool has_freq = meta.check<frequency>();
  const auto* p = in;

  term_meta.docs_count = vread<uint32_t>(p);
  if (has_freq) {
    term_meta.freq = term_meta.docs_count + vread<uint32_t>(p);
  }

  term_meta.doc_start += vread<uint64_t>(p);
  if (has_freq && term_meta.freq && meta.check<irs::position>()) {
    term_meta.pos_start += vread<uint64_t>(p);

    term_meta.pos_end = term_meta.freq > postings_writer_base::BLOCK_SIZE
        ? vread<uint64_t>(p)
        : type_limits<type_t::address_t>::invalid();

    if (meta.check<payload>() || meta.check<offset>()) {
      term_meta.pay_start += vread<uint64_t>(p);
    }
  }

  if (1U == term_meta.docs_count || term_meta.docs_count > postings_writer_base::BLOCK_SIZE) {
    term_meta.e_skip_start = vread<uint64_t>(p);
  }

  assert(p >= in);
  return size_t(std::distance(in, p));
}

template<typename FormatTraits, bool OneBasedPositionStorage>
class postings_reader final: public postings_reader_base {
 public:
  template<bool Freq, bool Pos, bool Offset, bool Payload>
  struct iterator_traits : FormatTraits {
    static constexpr bool frequency() { return Freq; }
    static constexpr bool position() { return Freq && Pos; }
    static constexpr bool offset() { return position() && Offset; }
    static constexpr bool payload() { return position() && Payload; }
    static constexpr bool one_based_position_storage() { return OneBasedPositionStorage; }
  };

  virtual irs::doc_iterator::ptr iterator(
    const flags& field,
    const flags& features,
    const term_meta& meta) override;

  virtual size_t bit_union(
    const flags& field,
    const term_provider_f& provider,
    size_t* set) override;

 private:
  struct doc_iterator_maker {
    template<typename IteratorTraits>
    static typename doc_iterator<IteratorTraits>::ptr make(
        const ::features& features,
        const postings_reader& ctx,
        const term_meta& meta) {
      auto it = memory::make_managed<doc_iterator<IteratorTraits>>();

      it->prepare(
        features, meta,
        ctx.doc_in_.get(),
        ctx.pos_in_.get(),
        ctx.pay_in_.get());

      return it;
    }
  };

  template<typename Maker, typename... Args>
  irs::doc_iterator::ptr iterator_impl(
    const flags& field,
    const flags& features,
    Args&&... args);
}; // postings_reader

#if defined(_MSC_VER)
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wswitch"
#endif

template<typename FormatTraits, bool OneBasedPositionStorage>
template<typename Maker, typename... Args>
irs::doc_iterator::ptr postings_reader<FormatTraits, OneBasedPositionStorage>::iterator_impl(
    const flags& field,
    const flags& req,
    Args&&... args) {
  const auto features = ::features(field);
  // get enabled features as the intersection
  // between requested and available features
  const auto enabled = features & req;

  switch (enabled) {
    case features::FREQ | features::POS | features::OFFS | features::PAY: {
      return Maker::template make<iterator_traits<true, true, true, true>>(features, *this, std::forward<Args>(args)...);
    }
    case features::FREQ | features::POS | features::OFFS: {
      return Maker::template make<iterator_traits<true, true, true, false>>(features, *this, std::forward<Args>(args)...);
    }
    case features::FREQ | features::POS | features::PAY: {
      return Maker::template make<iterator_traits<true, true, false, true>>(features, *this, std::forward<Args>(args)...);
    }
    case features::FREQ | features::POS: {
      return Maker::template make<iterator_traits<true, true, false, false>>(features, *this, std::forward<Args>(args)...);
    }
    case features::FREQ: {
      return Maker::template make<iterator_traits<true, false, false, false>>(features, *this, std::forward<Args>(args)...);
    }
    default: {
      return Maker::template make<iterator_traits<false, false, false, false>>(features, *this, std::forward<Args>(args)...);
    }
  }

  assert(false);
  return irs::doc_iterator::empty();
}

#if defined(_MSC_VER)
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

template<typename FormatTraits, bool OneBasedPositionStorage>
irs::doc_iterator::ptr postings_reader<FormatTraits, OneBasedPositionStorage>::iterator(
    const flags& field,
    const flags& req,
    const term_meta& meta) {
  return iterator_impl<doc_iterator_maker>(field, req, meta);
}

template<typename IteratorTraits, size_t N>
void bit_union(
    index_input& doc_in, doc_id_t docs_count,
    uint32_t (&docs)[N], uint32_t (&enc_buf)[N],
    size_t* set) {
  constexpr auto BITS{bits_required<std::remove_pointer_t<decltype(set)>>()};
  size_t num_blocks = docs_count / postings_writer_base::BLOCK_SIZE;

  doc_id_t doc = doc_limits::min();
  while (num_blocks--) {
    IteratorTraits::read_block(doc_in, enc_buf, docs);
    if constexpr (IteratorTraits::frequency()) {
      IteratorTraits::skip_block(doc_in);
    }

    // FIXME optimize
    for (const auto delta : docs) {
      doc += delta;
      irs::set_bit(set[doc / BITS], doc % BITS);
    }
  }

  doc_id_t docs_left = docs_count % postings_writer_base::BLOCK_SIZE;

  while (docs_left--) {
    doc_id_t delta;
    if constexpr (IteratorTraits::frequency()) {
      if (!shift_unpack_32(doc_in.read_vint(), delta)) {
        doc_in.read_vint();
      }
    } else {
      delta = doc_in.read_vint();
    }

    doc += delta;
    irs::set_bit(set[doc / BITS], doc % BITS);
  }
}

template<typename FormatTraits, bool OneBasedPositionStorage>
size_t postings_reader<FormatTraits, OneBasedPositionStorage>::bit_union(
    const flags& field,
    const term_provider_f& provider,
    size_t* set) {
  constexpr auto BITS{bits_required<std::remove_pointer_t<decltype(set)>>()};
  uint32_t enc_buf[postings_writer_base::BLOCK_SIZE];
  uint32_t docs[postings_writer_base::BLOCK_SIZE];
  const bool has_freq = field.check<frequency>();

  assert(doc_in_);
  auto doc_in = doc_in_->reopen(); // reopen thread-safe stream

  if (!doc_in) {
    // implementation returned wrong pointer
    IR_FRMT_ERROR("Failed to reopen document input in: %s", __FUNCTION__);

    throw io_error("failed to reopen document input");
  }

  size_t count = 0;
  while (const irs::term_meta* meta = provider()) {
    auto& term_state = static_cast<const version10::term_meta&>(*meta);

    if (term_state.docs_count > 1) {
      doc_in->seek(term_state.doc_start);
      assert(!doc_in->eof());

      if (has_freq) {
        using iterator_traits_t = iterator_traits<true, false, false, false>;
        ::bit_union<iterator_traits_t>(*doc_in, term_state.docs_count, docs, enc_buf, set);
      } else {
        using iterator_traits_t = iterator_traits<false, false, false, false>;
        ::bit_union<iterator_traits_t>(*doc_in, term_state.docs_count, docs, enc_buf, set);
      }

      count += term_state.docs_count;
    } else {
      const doc_id_t doc = doc_limits::min() + term_state.e_single_doc;
      irs::set_bit(set[doc / BITS], doc % BITS);

      ++count;
    }
  }

  return count;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                         format10
// ----------------------------------------------------------------------------

class format10 : public irs::version10::format {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_0";
  }

  DECLARE_FACTORY();

  format10() noexcept : format10(irs::type<format10>::get()) { }

  virtual index_meta_writer::ptr get_index_meta_writer() const override;
  virtual index_meta_reader::ptr get_index_meta_reader() const override final;

  virtual segment_meta_writer::ptr get_segment_meta_writer() const override;
  virtual segment_meta_reader::ptr get_segment_meta_reader() const override final;

  virtual document_mask_writer::ptr get_document_mask_writer() const override final;
  virtual document_mask_reader::ptr get_document_mask_reader() const override final;

  virtual field_writer::ptr get_field_writer(bool volatile_state) const override;
  virtual field_reader::ptr get_field_reader() const override final;

  virtual column_meta_writer::ptr get_column_meta_writer() const override;
  virtual column_meta_reader::ptr get_column_meta_reader() const override final;

  virtual columnstore_writer::ptr get_columnstore_writer() const override;
  virtual columnstore_reader::ptr get_columnstore_reader() const override final;

  virtual irs::postings_writer::ptr get_postings_writer(bool volatile_state) const override;
  virtual irs::postings_reader::ptr get_postings_reader() const override;

 protected:
  explicit format10(const irs::type_info& type) noexcept
    : version10::format(type) {
  }
}; // format10

const ::format10 FORMAT10_INSTANCE;

index_meta_writer::ptr format10::get_index_meta_writer() const {
  return memory::make_unique<::index_meta_writer>(
    int32_t(::index_meta_writer::FORMAT_MIN));
}

index_meta_reader::ptr format10::get_index_meta_reader() const {
  // can reuse stateless reader
  static ::index_meta_reader INSTANCE;

  return memory::to_managed<irs::index_meta_reader, false>(&INSTANCE);
}

segment_meta_writer::ptr format10::get_segment_meta_writer() const {
  // can reuse stateless writer
  static ::segment_meta_writer INSTANCE(::segment_meta_writer::FORMAT_MIN);

  return memory::to_managed<irs::segment_meta_writer, false>(&INSTANCE);
}

segment_meta_reader::ptr format10::get_segment_meta_reader() const {
  // can reuse stateless writer
  static ::segment_meta_reader INSTANCE;

  return memory::to_managed<irs::segment_meta_reader, false>(&INSTANCE);
}

document_mask_writer::ptr format10::get_document_mask_writer() const {
  // can reuse stateless writer
  static ::document_mask_writer INSTANCE;

  return memory::to_managed<irs::document_mask_writer, false>(&INSTANCE);
}

document_mask_reader::ptr format10::get_document_mask_reader() const {
  // can reuse stateless writer
  static ::document_mask_reader INSTANCE;

  return memory::to_managed<irs::document_mask_reader, false>(&INSTANCE);
}

field_writer::ptr format10::get_field_writer(bool volatile_state) const {
  return burst_trie::make_writer(
    burst_trie::Version::MIN,
    get_postings_writer(volatile_state),
    volatile_state);
}

field_reader::ptr format10::get_field_reader() const  {
  return burst_trie::make_reader(get_postings_reader());
}

column_meta_writer::ptr format10::get_column_meta_writer() const {
  return memory::make_unique<columns::meta_writer>(
    int32_t(columns::meta_writer::FORMAT_MIN));
}

column_meta_reader::ptr format10::get_column_meta_reader() const {
  return memory::make_unique<columns::meta_reader>();
}

columnstore_writer::ptr format10::get_columnstore_writer() const {
  return memory::make_unique<columns::writer>(
    int32_t(columns::writer::FORMAT_MIN));
}

columnstore_reader::ptr format10::get_columnstore_reader() const {
  return memory::make_unique<columns::reader>();
}

irs::postings_writer::ptr format10::get_postings_writer(bool volatile_state) const {
  constexpr const auto VERSION = postings_writer_base::FORMAT_MIN;

  if (volatile_state) {
    return memory::make_unique<::postings_writer<format_traits, true>>(VERSION);
  }

  return memory::make_unique<::postings_writer<format_traits, false>>(VERSION);
}

irs::postings_reader::ptr format10::get_postings_reader() const {
  return memory::make_unique<::postings_reader<format_traits, true>>();
}

/*static*/ irs::format::ptr format10::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT10_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format10, MODULE_NAME);

// ----------------------------------------------------------------------------
// --SECTION--                                                         format11
// ----------------------------------------------------------------------------

class format11 : public format10 {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_1";
  }

  DECLARE_FACTORY();

  format11() noexcept : format10(irs::type<format11>::get()) { }

  virtual index_meta_writer::ptr get_index_meta_writer() const override final;

  virtual field_writer::ptr get_field_writer(bool volatile_state) const override;

  virtual segment_meta_writer::ptr get_segment_meta_writer() const override final;

  virtual column_meta_writer::ptr get_column_meta_writer() const override final;

 protected:
  explicit format11(const irs::type_info& type) noexcept
    : format10(type) {
  }
}; // format11

const ::format11 FORMAT11_INSTANCE;

index_meta_writer::ptr format11::get_index_meta_writer() const {
  return memory::make_unique<::index_meta_writer>(
    int32_t(::index_meta_writer::FORMAT_MAX));
}

field_writer::ptr format11::get_field_writer(bool volatile_state) const {
  return burst_trie::make_writer(
    burst_trie::Version::ENCRYPTION_MIN,
    get_postings_writer(volatile_state),
    volatile_state);
}

segment_meta_writer::ptr format11::get_segment_meta_writer() const {
  // can reuse stateless writer
  static ::segment_meta_writer INSTANCE(::segment_meta_writer::FORMAT_MAX);

  return memory::to_managed<irs::segment_meta_writer, false>(&INSTANCE);
}

column_meta_writer::ptr format11::get_column_meta_writer() const {
  return memory::make_unique<columns::meta_writer>(
    int32_t(columns::meta_writer::FORMAT_MAX));
}

/*static*/ irs::format::ptr format11::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT11_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format11, MODULE_NAME);

// ----------------------------------------------------------------------------
// --SECTION--                                                         format12
// ----------------------------------------------------------------------------

class format12 : public format11 {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_2";
  }

  DECLARE_FACTORY();

  format12() noexcept : format11(irs::type<format12>::get()) { }

  virtual columnstore_writer::ptr get_columnstore_writer() const override final;

 protected:
  explicit format12(const irs::type_info& type) noexcept
    : format11(type) {
  }
}; // format12

const ::format12 FORMAT12_INSTANCE;

columnstore_writer::ptr format12::get_columnstore_writer() const {
  return memory::make_unique<columns::writer>(
    int32_t(columns::writer::FORMAT_MAX));
}

/*static*/ irs::format::ptr format12::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT12_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format12, MODULE_NAME);

// ----------------------------------------------------------------------------
// --SECTION--                                                         format13
// ----------------------------------------------------------------------------

class format13 : public format12 {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_3";
  }

  DECLARE_FACTORY();

  format13() noexcept : format12(irs::type<format13>::get()) { }

  virtual irs::postings_writer::ptr get_postings_writer(bool volatile_state) const override;
  virtual irs::postings_reader::ptr get_postings_reader() const override;

 protected:
  explicit format13(const irs::type_info& type) noexcept
    : format12(type) {
  }
};

const ::format13 FORMAT13_INSTANCE;

irs::postings_writer::ptr format13::get_postings_writer(bool volatile_state) const {
  constexpr const auto VERSION = postings_writer_base::FORMAT_POSITIONS_ZEROBASED;

  if (volatile_state) {
    return memory::make_unique<::postings_writer<format_traits, true>>(VERSION);
  }

  return memory::make_unique<::postings_writer<format_traits, false>>(VERSION);
}

irs::postings_reader::ptr format13::get_postings_reader() const {
  return memory::make_unique<::postings_reader<format_traits, false>>();
}

/*static*/ irs::format::ptr format13::make() {
  static const ::format13 INSTANCE;

  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT13_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format13, MODULE_NAME);

// ----------------------------------------------------------------------------
// --SECTION--                                                         format14
// ----------------------------------------------------------------------------

class format14 : public format13 {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_4";
  }

  DECLARE_FACTORY();

  format14() noexcept : format13(irs::type<format14>::get()) { }

  virtual irs::field_writer::ptr get_field_writer(bool volatile_state) const override;

 protected:
  explicit format14(const irs::type_info& type) noexcept
    : format13(type) {
  }
};

const ::format14 FORMAT14_INSTANCE;

irs::field_writer::ptr format14::get_field_writer(bool volatile_state) const {
  return burst_trie::make_writer(
    burst_trie::Version::MAX,
    get_postings_writer(volatile_state),
    volatile_state);
}

/*static*/ irs::format::ptr format14::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT14_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format14, MODULE_NAME);

// ----------------------------------------------------------------------------
// --SECTION--                                                      format12sse
// ----------------------------------------------------------------------------

#ifdef IRESEARCH_SSE2

struct format_traits_simd {
  static constexpr uint32_t BLOCK_SIZE = 128;

  FORCE_INLINE static void write_block(
      index_output& out, const uint32_t* in, uint32_t* buf) {
    encode::bitpack::write_block_simd(out, in, buf);
  }

  FORCE_INLINE static void read_block(
      index_input& in, uint32_t* buf, uint32_t* out) {
    encode::bitpack::read_block_simd(in, buf, out);
  }

  FORCE_INLINE static void skip_block(index_input& in) {
    encode::bitpack::skip_block32(in, BLOCK_SIZE);
  }
}; // format_traits_simd

class format12simd final : public format12 {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_2simd";
  }

  DECLARE_FACTORY();

  format12simd() noexcept : format12(irs::type<format12simd>::get()) { }

  virtual irs::postings_writer::ptr get_postings_writer(bool volatile_state) const override;
  virtual irs::postings_reader::ptr get_postings_reader() const override;
}; // format12simd

const ::format12simd FORMAT12SIMD_INSTANCE;

irs::postings_writer::ptr format12simd::get_postings_writer(bool volatile_state) const {
  constexpr const auto VERSION = postings_writer_base::FORMAT_SSE_POSITIONS_ONEBASED;

  if (volatile_state) {
    return memory::make_unique<::postings_writer<format_traits_simd, true>>(VERSION);
  }

  return memory::make_unique<::postings_writer<format_traits_simd, false>>(VERSION);
}

irs::postings_reader::ptr format12simd::get_postings_reader() const {
  return memory::make_unique<::postings_reader<format_traits_simd, true>>();
}

/*static*/ irs::format::ptr format12simd::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT12SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format12simd, MODULE_NAME);


// ----------------------------------------------------------------------------
// --SECTION--                                                      format13sse
// ----------------------------------------------------------------------------

class format13simd : public format13 {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_3simd";
  }

  DECLARE_FACTORY();

  format13simd() noexcept : format13(irs::type<format13simd>::get()) { }

  virtual irs::postings_writer::ptr get_postings_writer(bool volatile_state) const override;
  virtual irs::postings_reader::ptr get_postings_reader() const override;

 protected:
  explicit format13simd(const irs::type_info& type) noexcept
    : format13(type) {
  }
}; // format13simd

const ::format13simd FORMAT13SIMD_INSTANCE;

irs::postings_writer::ptr format13simd::get_postings_writer(bool volatile_state) const {
  constexpr const auto VERSION = postings_writer_base::FORMAT_SSE_POSITIONS_ZEROBASED;

  if (volatile_state) {
    return memory::make_unique<::postings_writer<format_traits_simd, true>>(VERSION);
  }

  return memory::make_unique<::postings_writer<format_traits_simd, false>>(VERSION);
}

irs::postings_reader::ptr format13simd::get_postings_reader() const {
  return memory::make_unique<::postings_reader<format_traits_simd, false>>();
}

/*static*/ irs::format::ptr format13simd::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT13SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format13simd, MODULE_NAME);

// ----------------------------------------------------------------------------
// --SECTION--                                                         format14
// ----------------------------------------------------------------------------

class format14simd : public format13simd {
 public:
  static constexpr string_ref type_name() noexcept {
    return "1_4simd";
  }

  DECLARE_FACTORY();

  format14simd() noexcept : format13simd(irs::type<format14simd>::get()) { }

  virtual irs::field_writer::ptr get_field_writer(bool volatile_state) const override;

 protected:
  explicit format14simd(const irs::type_info& type) noexcept
    : format13simd(type) {
  }
};

const ::format14 FORMAT14SIMD_INSTANCE;

irs::field_writer::ptr format14simd::get_field_writer(bool volatile_state) const {
  return burst_trie::make_writer(
    burst_trie::Version::MAX,
    get_postings_writer(volatile_state),
    volatile_state);
}

/*static*/ irs::format::ptr format14simd::make() {
  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &FORMAT14SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format14simd, MODULE_NAME);

#endif // IRESEARCH_SSE2

}

namespace iresearch {
namespace version10 {

void init() {
#ifndef IRESEARCH_DLL
  REGISTER_FORMAT(::format10);
  REGISTER_FORMAT(::format11);
  REGISTER_FORMAT(::format12);
  REGISTER_FORMAT(::format13);
  REGISTER_FORMAT(::format14);
#ifdef IRESEARCH_SSE2
  REGISTER_FORMAT(::format12simd);
  REGISTER_FORMAT(::format13simd);
  REGISTER_FORMAT(::format14simd);
#endif // IRESEARCH_SSE2
#endif // IRESEARCH_DLL
}

// ----------------------------------------------------------------------------
// --SECTION--                                                           format
// ----------------------------------------------------------------------------

format::format(const irs::type_info& type) noexcept
  : irs::format(type) {
}

} // version10

// use base irs::position type for ancestors
template<typename IteratorTraits, bool Position>
struct type<::position<IteratorTraits, Position>> : type<irs::position> { };

} // root
