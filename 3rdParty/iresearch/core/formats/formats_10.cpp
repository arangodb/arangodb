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

#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"

#include "utils/bit_packing.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitset.hpp"
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

#if defined(_MSC_VER)
  #pragma warning(disable : 4351)
#endif

#if (!defined(IRESEARCH_FORMAT10_CODEC) || (IRESEARCH_FORMAT10_CODEC == 0))

NS_LOCAL

struct format_traits {
  static const uint32_t BLOCK_SIZE = 128;
  static const irs::string_ref NAME;

  FORCE_INLINE static void write_block(
      irs::index_output& out,
      const uint32_t* in,
      uint32_t size,
      uint32_t* buf) {
    irs::encode::bitpack::write_block(out, in, size, buf);
  }

  FORCE_INLINE static void read_block(
      irs::index_input& in,
      uint32_t size,
      uint32_t* buf,
      uint32_t* out) {
    irs::encode::bitpack::read_block(in, size, buf, out);
  }

  FORCE_INLINE static void skip_block(
      irs::index_input& in,
      size_t size) {
    irs::encode::bitpack::skip_block32(in, size);
  }
}; // format_traits

const irs::string_ref format_traits::NAME = "1_0";

NS_END

#elif (IRESEARCH_FORMAT10_CODEC == 1) // simdpack

#ifndef IRESEARCH_SSE2
  #error "Optimized format requires SSE2 support"
#endif

#include "store/store_utils_optimized.hpp"

NS_LOCAL

struct format_traits {
  static const uint32_t BLOCK_SIZE = 128;
  static const irs::string_ref NAME;

  FORCE_INLINE static void write_block(
      irs::index_output& out,
      const uint32_t* in,
      size_t size,
      uint32_t* buf) {
    irs::encode::bitpack::write_block_optimized(out, in, size, buf);
  }

  FORCE_INLINE static void read_block(
      irs::index_input& in,
      size_t size,
      uint32_t* buf,
      uint32_t* out) {
    irs::encode::bitpack::read_block_optimized(in, size, buf, out);
  }

  FORCE_INLINE static void skip_block(
      irs::index_input& in,
      size_t size) {
    irs::encode::bitpack::skip_block32(in, size);
  }
}; // format_traits

const irs::string_ref format_traits::NAME = "1_0";

NS_END

#endif

NS_LOCAL

irs::bytes_ref DUMMY; // placeholder for visiting logic in columnstore

using namespace iresearch;

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
    // MSVC2013 requires compile-time constant values for enum combinations
    // used by switch-case statements
    FREQ_POS = 3, // FREQ | POS
    FREQ_POS_OFFS = 7, // FREQ | POS | OFFS
    FREQ_POS_PAY = 11, // FREQ | POS | PAY
    FREQ_POS_OFFS_PAY = 15, // FREQ | POS | OFFS | PAY
  };

  features() = default;

  explicit features(const irs::flags& in) NOEXCEPT {
    irs::set_bit<0>(in.check<irs::frequency>(), mask_);
    irs::set_bit<1>(in.check<irs::position>(), mask_);
    irs::set_bit<2>(in.check<irs::offset>(), mask_);
    irs::set_bit<3>(in.check<irs::payload>(), mask_);
  }

  features operator&(const irs::flags& in) const NOEXCEPT {
    return features(*this) &= in;
  }

  features& operator&=(const irs::flags& in) NOEXCEPT {
    irs::unset_bit<0>(!in.check<irs::frequency>(), mask_);
    irs::unset_bit<1>(!in.check<irs::position>(), mask_);
    irs::unset_bit<2>(!in.check<irs::offset>(), mask_);
    irs::unset_bit<3>(!in.check<irs::payload>(), mask_);
    return *this;
  }

  bool freq() const NOEXCEPT { return irs::check_bit<0>(mask_); }
  bool position() const NOEXCEPT { return irs::check_bit<1>(mask_); }
  bool offset() const NOEXCEPT { return irs::check_bit<2>(mask_); }
  bool payload() const NOEXCEPT { return irs::check_bit<3>(mask_); }
  operator Mask() const NOEXCEPT { return static_cast<Mask>(mask_); }

  bool any(Mask mask) const NOEXCEPT {
    return Mask(0) != (mask_ & mask);
  }

  bool all(Mask mask) const NOEXCEPT {
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
  assert( !out );
  file_name(str, state.name, ext);
  out = state.dir->create(str);

  if (!out) {
    throw io_error(string_utils::to_string(
      "failed to create file, path: %s",
      str.c_str()
    ));
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
  assert( !in );
  file_name(str, state.meta->name, ext);
  in = state.dir->open(str, advice);

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      str.c_str()
    ));
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
class postings_writer final: public irs::postings_writer {
 public:
  static const string_ref TERMS_FORMAT_NAME;
  static const int32_t TERMS_FORMAT_MIN = 0;
  static const int32_t TERMS_FORMAT_MAX = TERMS_FORMAT_MIN;

  static const string_ref DOC_FORMAT_NAME;
  static const string_ref DOC_EXT;
  static const string_ref POS_FORMAT_NAME;
  static const string_ref POS_EXT;
  static const string_ref PAY_FORMAT_NAME;
  static const string_ref PAY_EXT;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  static const uint32_t MAX_SKIP_LEVELS = 10;
  static const uint32_t BLOCK_SIZE = format_traits::BLOCK_SIZE;
  static const uint32_t SKIP_N = 8;

  explicit postings_writer(bool volatile_attributes);

  // ------------------------------------------
  // const_attributes_provider
  // ------------------------------------------

  virtual const irs::attribute_view& attributes() const NOEXCEPT override final {
    return attrs_;
  }

  // ------------------------------------------
  // postings_writer
  // ------------------------------------------

  virtual void prepare(index_output& out, const irs::flush_state& state) override;
  virtual void begin_field(const irs::flags& meta) override;
  virtual irs::postings_writer::state write(irs::doc_iterator& docs) override;
  virtual void begin_block() override;
  virtual void encode(data_output& out, const irs::term_meta& attrs) override;
  virtual void end() override;

 protected:
  virtual void release(irs::term_meta *meta) NOEXCEPT override;

 private:
  struct stream {
    void reset() {
      start = end = 0;
    }

    uint64_t skip_ptr[MAX_SKIP_LEVELS]{}; // skip data
    index_output::ptr out;                // output stream
    uint64_t start{};                     // start position of block
    uint64_t end{};                       // end position of block
  }; // stream

  struct doc_stream : stream {
    void doc(doc_id_t delta) { deltas[size] = delta; }
    void flush(uint32_t* buf, bool freq);
    bool full() const { return BLOCK_SIZE == size; }
    void next(doc_id_t id) { last = id, ++size; }
    void freq(uint32_t frq) { freqs[size] = frq; }

    void reset() {
      stream::reset();
      last = type_limits<type_t::doc_id_t>::invalid();
      block_last = 0;
      size = 0;
    }

    doc_id_t deltas[BLOCK_SIZE]{}; // document deltas
    doc_id_t skip_doc[MAX_SKIP_LEVELS]{};
    std::unique_ptr<uint32_t[]> freqs; // document frequencies
    doc_id_t last{ type_limits<type_t::doc_id_t>::invalid() }; // last buffered document id
    doc_id_t block_last{}; // last document id in a block
    uint32_t size{}; // number of buffered elements
  }; // doc_stream

  struct pos_stream : stream {
    DECLARE_UNIQUE_PTR(pos_stream);

    void flush(uint32_t* buf);

    bool full() const { return BLOCK_SIZE == size; }
    void next(uint32_t pos) { last = pos, ++size; }
    void pos(uint32_t pos) { buf[size] = pos; }

    void reset() {
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
    DECLARE_UNIQUE_PTR(pay_stream);

    void flush_payload(uint32_t* buf);
    void flush_offsets(uint32_t* buf);

    void payload(uint32_t i, const bytes_ref& pay);
    void offsets(uint32_t i, uint32_t start, uint32_t end);

    void reset() {
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

  void write_skip(size_t level, index_output& out);
  void begin_term();
  void begin_doc(doc_id_t id, const frequency* freq);
  void add_position( uint32_t pos, const offset* offs, const payload* pay );
  void end_doc();
  void end_term(version10::term_meta& state, const uint32_t* tfreq);

  memory::memory_pool<> meta_pool_;
  memory::memory_pool_allocator<version10::term_meta, decltype(meta_pool_)> alloc_{ meta_pool_ };
  skip_writer skip_;
  irs::attribute_view attrs_;
  uint32_t buf[BLOCK_SIZE];        // buffer for encoding (worst case)
  version10::term_meta last_state; // last final term state
  doc_stream doc;                  // document stream
  pos_stream::ptr pos_;            // proximity stream
  pay_stream::ptr pay_;            // payloads and offsets stream
  size_t docs_count{};             // count of processed documents
  version10::documents docs_;      // bit set of all processed documents
  features features_;              // features supported by current field
  bool volatile_attributes_;       // attribute value memory locations may change after next()
}; // postings_writer

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1

const string_ref postings_writer::TERMS_FORMAT_NAME = "iresearch_10_postings_terms";

const string_ref postings_writer::DOC_FORMAT_NAME = "iresearch_10_postings_documents";
const string_ref postings_writer::DOC_EXT = "doc";

const string_ref postings_writer::POS_FORMAT_NAME = "iresearch_10_postings_positions";
const string_ref postings_writer::POS_EXT = "pos";

const string_ref postings_writer::PAY_FORMAT_NAME = "iresearch_10_postings_payloads";
const string_ref postings_writer::PAY_EXT = "pay";

MSVC2015_ONLY(__pragma(warning(pop)))

void postings_writer::doc_stream::flush(uint32_t* buf, bool freq) {
  format_traits::write_block(*out, deltas, BLOCK_SIZE, buf);

  if (freq) {
    format_traits::write_block(*out, freqs.get(), BLOCK_SIZE, buf);
  }
}

void postings_writer::pos_stream::flush(uint32_t* comp_buf) {
  format_traits::write_block(*out, this->buf, BLOCK_SIZE, comp_buf);
  size = 0;
}

/* postings_writer::pay_stream */

void postings_writer::pay_stream::payload(uint32_t i, const bytes_ref& pay) {
  if (!pay.empty()) {
    pay_buf_.append(pay.c_str(), pay.size());
  }

  pay_sizes[i] = static_cast<uint32_t>(pay.size());
}

void postings_writer::pay_stream::offsets(
    uint32_t i, uint32_t start_offset, uint32_t end_offset) {
  assert(start_offset >= last && start_offset <= end_offset);

  offs_start_buf[i] = start_offset - last;
  offs_len_buf[i] = end_offset - start_offset;
  last = start_offset;
}

void postings_writer::pay_stream::flush_payload(uint32_t* buf) {
  out->write_vint(static_cast<uint32_t>(pay_buf_.size()));
  if (pay_buf_.empty()) {
    return;
  }
  format_traits::write_block(*out, pay_sizes, BLOCK_SIZE, buf);
  out->write_bytes(pay_buf_.c_str(), pay_buf_.size());
  pay_buf_.clear();
}

void postings_writer::pay_stream::flush_offsets(uint32_t* buf) {
  format_traits::write_block(*out, offs_start_buf, BLOCK_SIZE, buf);
  format_traits::write_block(*out, offs_len_buf, BLOCK_SIZE, buf);
}

postings_writer::postings_writer(bool volatile_attributes)
  : skip_(BLOCK_SIZE, SKIP_N),
    volatile_attributes_(volatile_attributes) {
  attrs_.emplace(docs_);
}

void postings_writer::prepare(index_output& out, const irs::flush_state& state) {
  assert(state.dir);
  assert(!state.name.null());

  // reset writer state
  docs_count = 0;

  std::string name;

  // prepare document stream
  prepare_output(name, doc.out, state, DOC_EXT, DOC_FORMAT_NAME, FORMAT_MAX);

  auto& features = *state.features;
  if (features.check<frequency>() && !doc.freqs) {
    // prepare frequency stream
    doc.freqs = memory::make_unique<uint32_t[]>(BLOCK_SIZE);
    std::memset(doc.freqs.get(), 0, sizeof(uint32_t) * BLOCK_SIZE);
  }

  if (features.check<position>()) {
    // prepare proximity stream
    if (!pos_) {
      pos_ = memory::make_unique< pos_stream >();
    }

    pos_->reset();
    prepare_output(name, pos_->out, state, POS_EXT, POS_FORMAT_NAME, FORMAT_MAX);

    if (features.check<payload>() || features.check<offset>()) {
      // prepare payload stream
      if (!pay_) {
        pay_ = memory::make_unique<pay_stream>();
      }

      pay_->reset();
      prepare_output(name, pay_->out, state, PAY_EXT, PAY_FORMAT_NAME, FORMAT_MAX);
    }
  }

  skip_.prepare(
    MAX_SKIP_LEVELS,
    state.doc_count,
    [this] (size_t i, index_output& out) { write_skip(i, out); },
    directory_utils::get_allocator(*state.dir)
  );

  // write postings format name
  format_utils::write_header(out, TERMS_FORMAT_NAME, TERMS_FORMAT_MAX);
  // write postings block size
  out.write_vint(BLOCK_SIZE);

  // prepare documents bitset
  docs_.value.reset(state.doc_count);
}

void postings_writer::begin_field(const irs::flags& field) {
  features_ = ::features(field);
  docs_.value.clear();
  last_state.clear();
}

void postings_writer::begin_block() {
  /* clear state in order to write
   * absolute address of the first
   * entry in the block */
  last_state.clear();
}

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

irs::postings_writer::state postings_writer::write(irs::doc_iterator& docs) {
  REGISTER_TIMER_DETAILED();
  auto& freq = docs.attributes().get<frequency>();

  auto& pos = freq
    ? docs.attributes().get<position>()
    : irs::attribute_view::ref<position>::NIL;

  const offset* offs = nullptr;
  const payload* pay = nullptr;

  uint32_t* tfreq = nullptr;

  auto meta = memory::allocate_unique<version10::term_meta>(alloc_);

  if (freq) {
    if (pos && !volatile_attributes_) {
      auto& attrs = pos->attributes();
      offs = attrs.get<offset>().get();
      pay = attrs.get<payload>().get();
    }

    tfreq = &meta->freq;
  }

  begin_term();

  while (docs.next()) {
    const auto did = docs.value();

    assert(type_limits<type_t::doc_id_t>::valid(did));
    begin_doc(did, freq.get());
    docs_.value.set(did - type_limits<type_t::doc_id_t>::min());

    if (pos) {
      if (volatile_attributes_) {
        auto& attrs = pos->attributes();
        offs = attrs.get<offset>().get();
        pay = attrs.get<payload>().get();
      }

      while (pos->next()) {
        add_position(pos->value(), offs, pay);
      }
    }

    ++meta->docs_count;
    if (tfreq) {
      (*tfreq) += freq->value;
    }

    end_doc();
  }

  end_term(*meta, tfreq);

  return make_state(*meta.release());
}

void postings_writer::release(irs::term_meta *meta) NOEXCEPT {
#ifdef IRESEARCH_DEBUG
  auto* state = dynamic_cast<version10::term_meta*>(meta);
#else
  auto* state = static_cast<version10::term_meta*>(meta);
#endif // IRESEARCH_DEBUG
  assert(state);

  alloc_.destroy(state);
  alloc_.deallocate(state);
}

#if defined(_MSC_VER)
  #pragma warning( default : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

void postings_writer::begin_term() {
  doc.start = doc.out->file_pointer();
  std::fill_n(doc.skip_ptr, MAX_SKIP_LEVELS, doc.start);
  if (features_.position()) {
    assert(pos_ && pos_->out);
    pos_->start = pos_->out->file_pointer();
    std::fill_n(pos_->skip_ptr, MAX_SKIP_LEVELS, pos_->start);
    if (features_.any(features::OFFS | features::PAY)) {
      assert(pay_ && pay_->out);
      pay_->start = pay_->out->file_pointer();
      std::fill_n(pay_->skip_ptr, MAX_SKIP_LEVELS, pay_->start);
    }
  }

  doc.last = type_limits<type_t::doc_id_t>::min(); // for proper delta of 1st id
  doc.block_last = type_limits<type_t::doc_id_t>::invalid();
  skip_.reset();
}

void postings_writer::begin_doc(doc_id_t id, const frequency* freq) {
  if (type_limits<type_t::doc_id_t>::valid(doc.block_last) && 0 == doc.size) {
    skip_.skip(docs_count);
  }

  if (id < doc.last) {
    throw index_error(string_utils::to_string(
      "while beginning doc in postings_writer, error: docs out of order '%d' < '%d'",
      id, doc.last
    ));
  }

  doc.doc(id - doc.last);
  if (freq) {
    doc.freq(freq->value);
  }

  doc.next(id);
  if (doc.full()) {
    doc.flush(buf, freq != nullptr);
  }

  if (pos_) pos_->last = 0;
  if (pay_) pay_->last = 0;

  ++docs_count;
}

void postings_writer::add_position(uint32_t pos, const offset* offs, const payload* pay) {
  assert(!offs || offs->start <= offs->end);
  assert(features_.position() && pos_ && pos_->out); /* at least positions stream should be created */

  pos_->pos(pos - pos_->last);
  if (pay) pay_->payload(pos_->size, pay->value);
  if (offs) pay_->offsets(pos_->size, offs->start, offs->end);

  pos_->next(pos);

  if (pos_->full()) {
    pos_->flush(buf);

    if (pay) {
      assert(features_.payload() && pay_ && pay_->out);
      pay_->flush_payload(buf);
    }

    if (offs) {
      assert(features_.offset() && pay_ && pay_->out);
      pay_->flush_offsets(buf);
    }
  }
}

void postings_writer::end_doc() {
  if (doc.full()) {
    doc.block_last = doc.last;
    doc.end = doc.out->file_pointer();
    if (features_.position()) {
      assert(pos_ && pos_->out);
      pos_->end = pos_->out->file_pointer();
      // documents stream is full, but positions stream is not
      // save number of positions to skip before the next block
      pos_->block_last = pos_->size;
      if (features_.any(features::OFFS | features::PAY)) {
        assert(pay_ && pay_->out);
        pay_->end = pay_->out->file_pointer();
        pay_->block_last = pay_->pay_buf_.size();
      }
    }

    doc.size = 0;
  }
}

void postings_writer::end_term(version10::term_meta& meta, const uint32_t* tfreq) {
  if (docs_count == 0) {
    return; // no documents to write
  }

  if (1 == meta.docs_count) {
    meta.e_single_doc = doc.deltas[0];
  } else {
    // write remaining documents using
    // variable length encoding
    data_output& out = *doc.out;

    for (uint32_t i = 0; i < doc.size; ++i) {
      const doc_id_t doc_delta = doc.deltas[i];

      if (!features_.freq()) {
        out.write_vint(doc_delta);
      } else {
        assert(doc.freqs);
        const uint32_t freq = doc.freqs[i];

        if (1 == freq) {
          out.write_vint(shift_pack_32(doc_delta, true));
        } else {
          out.write_vint(shift_pack_32(doc_delta, false));
          out.write_vint(freq);
        }
      }
    }
  }

  meta.pos_end = type_limits<type_t::address_t>::invalid();

  /* write remaining position using
   * variable length encoding */
  if (features_.position()) {
    assert(pos_ && pos_->out);

    if (meta.freq > BLOCK_SIZE) {
      meta.pos_end = pos_->out->file_pointer() - pos_->start;
    }

    if (pos_->size > 0) {
      data_output& out = *pos_->out;
      uint32_t last_pay_size = integer_traits<uint32_t>::const_max;
      uint32_t last_offs_len = integer_traits<uint32_t>::const_max;
      uint32_t pay_buf_start = 0;
      for (uint32_t i = 0; i < pos_->size; ++i) {
        const uint32_t pos_delta = pos_->buf[i];
        if (features_.payload()) {
          assert(pay_ && pay_->out);

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
          assert(pay_ && pay_->out);

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
        assert(pay_ && pay_->out);
        pay_->pay_buf_.clear();
      }
    }
  }

  if (!tfreq) {
    meta.freq = integer_traits<uint32_t>::const_max;
  }

  /* if we have flushed at least
   * one block there was buffered
   * skip data, so we need to flush it*/
  if (docs_count > BLOCK_SIZE) {
    //const uint64_t start = doc.out->file_pointer();
    meta.e_skip_start = doc.out->file_pointer() - doc.start;
    skip_.flush(*doc.out);
  }

  docs_count = 0;
  doc.size = 0;
  doc.last = 0;
  meta.doc_start = doc.start;

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

void postings_writer::write_skip(size_t level, index_output& out) {
  const doc_id_t doc_delta = doc.block_last; //- doc.skip_doc[level];
  const uint64_t doc_ptr = doc.out->file_pointer();

  out.write_vint(doc_delta);
  out.write_vlong(doc_ptr - doc.skip_ptr[level]);

  doc.skip_doc[level] = doc.block_last;
  doc.skip_ptr[level] = doc_ptr;

  if (features_.position()) {
    assert(pos_);

    const uint64_t pos_ptr = pos_->out->file_pointer();

    out.write_vint(pos_->block_last);
    out.write_vlong(pos_ptr - pos_->skip_ptr[level]);

    pos_->skip_ptr[level] = pos_ptr;

    if (features_.any(features::OFFS | features::PAY)) {
      assert(pay_ && pay_->out);

      if (features_.payload()) {
        out.write_vint(static_cast<uint32_t>(pay_->block_last));
      }

      const uint64_t pay_ptr = pay_->out->file_pointer();

      out.write_vlong(pay_ptr - pay_->skip_ptr[level]);
      pay_->skip_ptr[level] = pay_ptr;
    }
  }
}

void postings_writer::encode(
    data_output& out,
    const irs::term_meta& state) {
#ifdef IRESEARCH_DEBUG
  const auto& meta = dynamic_cast<const version10::term_meta&>(state);
#else
  const auto& meta = static_cast<const version10::term_meta&>(state);
#endif // IRESEARCH_DEBUG

  out.write_vint(meta.docs_count);
  if (meta.freq != integer_traits<uint32_t>::const_max) {
    assert(meta.freq >= meta.docs_count);
    out.write_vint(meta.freq - meta.docs_count);
  }

  out.write_vlong(meta.doc_start - last_state.doc_start);
  if (features_.position()) {
    out.write_vlong(meta.pos_start - last_state.pos_start);
    if (type_limits<type_t::address_t>::valid(meta.pos_end)) {
      out.write_vlong(meta.pos_end);
    }
    if (features_.any(features::OFFS | features::PAY)) {
      out.write_vlong(meta.pay_start - last_state.pay_start);
    }
  }

  if (1U == meta.docs_count || meta.docs_count > postings_writer::BLOCK_SIZE) {
    out.write_vlong(meta.e_skip_start);
  }

  last_state = meta;
}

void postings_writer::end() {
  format_utils::write_footer(*doc.out);
  doc.out.reset(); // ensure stream is closed

  if (pos_ && pos_->out) { // check both for the case where the writer is reused
    format_utils::write_footer(*pos_->out);
    pos_->out.reset(); // ensure stream is closed
  }

  if (pay_ && pay_->out) { // check both for the case where the writer is reused
    format_utils::write_footer(*pay_->out);
    pay_->out.reset(); // ensure stream is closed
  }
}

struct skip_state {
  uint64_t doc_ptr{}; // pointer to the beginning of document block
  uint64_t pos_ptr{}; // pointer to the positions of the first document in a document block
  uint64_t pay_ptr{}; // pointer to the payloads of the first document in a document block
  size_t pend_pos{}; // positions to skip before new document block
  doc_id_t doc{ type_limits<type_t::doc_id_t>::invalid() }; // last document in a previous block
  uint32_t pay_pos{}; // payload size to skip before in new document block
}; // skip_state

struct skip_context : skip_state {
  size_t level{}; // skip level
}; // skip_context

struct doc_state {
  const index_input* pos_in;
  const index_input* pay_in;
  version10::term_meta* term_state;
  uint32_t* freq;
  uint32_t* enc_buf;
  uint64_t tail_start;
  size_t tail_length;
  ::features features;
}; // doc_state

// ----------------------------------------------------------------------------
// --SECTION--                                                 helper functions
// ----------------------------------------------------------------------------

FORCE_INLINE void skip_positions(index_input& in) {
  format_traits::skip_block(in, postings_writer::BLOCK_SIZE);
}

FORCE_INLINE void skip_payload(index_input& in) {
  const size_t size = in.read_vint();
  if (size) {
    format_traits::skip_block(in, postings_writer::BLOCK_SIZE);
    in.seek(in.file_pointer() + size);
  }
}

FORCE_INLINE void skip_offsets(index_input& in) {
  format_traits::skip_block(in, postings_writer::BLOCK_SIZE);
  format_traits::skip_block(in, postings_writer::BLOCK_SIZE);
}

///////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator
///////////////////////////////////////////////////////////////////////////////
class doc_iterator : public irs::doc_iterator {
 public:
  DECLARE_SHARED_PTR(doc_iterator);

  doc_iterator() NOEXCEPT
    : skip_levels_(1),
      skip_(postings_writer::BLOCK_SIZE, postings_writer::SKIP_N) {
    std::fill(docs_, docs_ + postings_writer::BLOCK_SIZE, type_limits<type_t::doc_id_t>::invalid());
  }

  void prepare(
      const features& field,
      const features& enabled,
      const irs::attribute_view& attrs,
      const index_input* doc_in,
      const index_input* pos_in,
      const index_input* pay_in) {
    features_ = field; // set field features
    enabled_ = enabled; // set enabled features

    // add mandatory attributes
    attrs_.emplace(doc_);
    begin_ = end_ = docs_;

    // get state attribute
    assert(attrs.contains<version10::term_meta>());
    term_state_ = *attrs.get<version10::term_meta>();

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

    prepare_attributes(enabled, attrs, pos_in, pay_in);
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (target <= doc_.value) {
      return doc_.value;
    }

    seek_to_block(target);

    // FIXME binary search instead of linear
    irs::seek(*this, target);
    return value();
  }

  virtual doc_id_t value() const override {
    return doc_.value;
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

  virtual bool next() override {
    if (begin_ == end_) {
      cur_pos_ += relative_pos();

      if (cur_pos_ == term_state_.docs_count) {
        doc_.value = type_limits<type_t::doc_id_t>::eof();
        begin_ = end_ = docs_; // seal the iterator
        return false;
      }

      refill();
    }

    doc_.value = *begin_++;
    freq_.value = *doc_freq_++;

    return true;
  }

#if defined(_MSC_VER)
  #pragma warning( default : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

 protected:
  virtual void prepare_attributes(
      const features& enabled,
      const irs::attribute_view& attrs,
      const index_input* pos_in,
      const index_input* pay_in) {
    UNUSED(pos_in);
    UNUSED(pay_in);

    // term frequency attributes
    if (enabled.freq()) {
      assert(attrs.contains<frequency>());
      attrs_.emplace(freq_);
      term_freq_ = attrs.get<frequency>()->value;
    }
  }

  virtual void seek_notify(const skip_context& /*ctx*/) {
  }

  void seek_to_block(doc_id_t target);

  // returns current position in the document block 'docs_'
  size_t relative_pos() NOEXCEPT {
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
    const auto left = term_state_.docs_count - cur_pos_;

    if (left >= postings_writer::BLOCK_SIZE) {
      // read doc deltas
      format_traits::read_block(
        *doc_in_,
        postings_writer::BLOCK_SIZE,
        enc_buf_,
        docs_
      );

      if (features_.freq()) {
        // read frequency it is required by
        // the iterator or just skip it otherwise
        if (enabled_.freq()) {
          format_traits::read_block(
            *doc_in_,
            postings_writer::BLOCK_SIZE,
            enc_buf_,
            doc_freqs_
          );
        } else {
          format_traits::skip_block(
            *doc_in_,
            postings_writer::BLOCK_SIZE
          );
        }
      }
      end_ = docs_ + postings_writer::BLOCK_SIZE;
    } else if (1 == term_state_.docs_count) {
      docs_[0] = term_state_.e_single_doc;
      if (term_freq_) {
        doc_freqs_[0] = term_freq_;
      }
      end_ = docs_ + 1;
    } else {
      read_end_block(left);
      end_ = docs_ + left;
    }

    // if this is the initial doc_id then set it to min() for proper delta value
    // add last doc_id before decoding
    *docs_ += type_limits<type_t::doc_id_t>::valid(doc_.value)
      ? doc_.value
      : (type_limits<type_t::doc_id_t>::min)();

    // decode delta encoded documents block
    encode::delta::decode(std::begin(docs_), end_);

    begin_ = docs_;
    doc_freq_ = docs_ + postings_writer::BLOCK_SIZE;
  }

  std::vector<skip_state> skip_levels_;
  skip_reader skip_;
  skip_context* skip_ctx_; // pointer to used skip context, will be used by skip reader
  irs::attribute_view attrs_;
  uint32_t enc_buf_[postings_writer::BLOCK_SIZE]; // buffer for encoding
  doc_id_t docs_[postings_writer::BLOCK_SIZE]; // doc values
  uint32_t doc_freqs_[postings_writer::BLOCK_SIZE]; // document frequencies
  uint32_t cur_pos_{};
  const doc_id_t* begin_{docs_};
  doc_id_t* end_{docs_};
  uint32_t* doc_freq_{}; // pointer into docs_ to the frequency attribute value for the current doc
  uint32_t term_freq_{}; // total term frequency
  document doc_;
  frequency freq_;
  index_input::ptr doc_in_;
  version10::term_meta term_state_;
  features features_; // field features
  features enabled_; // enabled iterator features
}; // doc_iterator

void doc_iterator::seek_to_block(doc_id_t target) {
  // check whether it make sense to use skip-list
  if (skip_levels_.front().doc < target && term_state_.docs_count > postings_writer::BLOCK_SIZE) {
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
            return (next.doc = type_limits<type_t::doc_id_t>::eof());
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
      doc_.value = last.doc;
      cur_pos_ = skipped;
      begin_ = end_ = docs_; // will trigger refill in "next"
      seek_notify(last); // notifies derivatives
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
/// @class mask_doc_iterator
///////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
class mask_doc_iterator final: public DocIterator {
 public:
  typedef DocIterator doc_iterator_t;

  static_assert(
    std::is_base_of<irs::doc_iterator, doc_iterator_t>::value,
    "DocIterator must be derived from irs::doc_iterator"
   );

  explicit mask_doc_iterator(const document_mask& mask)
    : mask_(mask) {
  }

  virtual bool next() override {
    while (doc_iterator_t::next()) {
      if (mask_.find(this->value()) == mask_.end()) {
        return true;
      }
    }

    return false;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = doc_iterator_t::seek(target);

    if (mask_.find(doc) == mask_.end()) {
      return doc;
    }

    this->next();

    return this->value();
  }

 private:
  const document_mask& mask_; /* excluded document ids */
}; // mask_doc_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class pos_iterator
///////////////////////////////////////////////////////////////////////////////
class pos_iterator: public position {
 public:
  DECLARE_UNIQUE_PTR(pos_iterator);

  pos_iterator(size_t reserve_attrs = 0): position(reserve_attrs) {}

  virtual void clear() override {
    value_ = irs::type_limits<irs::type_t::pos_t>::invalid();
  }

  virtual bool next() override {
    if (0 == pend_pos_) {
      value_ = irs::type_limits<irs::type_t::pos_t>::eof();

      return false;
    }

    const uint32_t freq = *freq_;

    if (pend_pos_ > freq) {
      skip(pend_pos_ - freq);
      pend_pos_ = freq;
    }

    if (buf_pos_ == postings_writer::BLOCK_SIZE) {
      refill();
      buf_pos_ = 0;
    }

    // FIXME TODO: make INVALID = 0, remove this
    if (!irs::type_limits<irs::type_t::pos_t>::valid(value_)) {
      value_ = 0;
    }

    value_ += pos_deltas_[buf_pos_];
    read_attributes();
    ++buf_pos_;
    --pend_pos_;
    return true;
  }

  // prepares iterator to work
  virtual void prepare(const doc_state& state) {
    pos_in_ = state.pos_in->reopen(); // reopen thread-safe stream

    if (!pos_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen positions input in: %s", __FUNCTION__);

      throw io_error("failed to reopen positions input");
    }

    pos_in_->seek(state.term_state->pos_start);
    freq_ = state.freq;
    features_ = state.features;
    enc_buf_ = reinterpret_cast<uint32_t*>(state.enc_buf);
    tail_start_ = state.tail_start;
    tail_length_ = state.tail_length;
  }

  // notifies iterator that doc iterator has skipped to a new block
  virtual void prepare(const skip_state& state) {
    pos_in_->seek(state.pos_ptr);
    pend_pos_ = state.pend_pos;
    buf_pos_ = postings_writer::BLOCK_SIZE;
  }

  virtual uint32_t value() const override { return value_; }

 protected:
  virtual void read_attributes() { }

  virtual void refill() {
    if (pos_in_->file_pointer() == tail_start_) {
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
    } else {
      format_traits::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);
    }
  }

  virtual void skip(uint32_t count) {
    auto left = postings_writer::BLOCK_SIZE - buf_pos_;
    if (count >= left) {
      count -= left;
      while (count >= postings_writer::BLOCK_SIZE) {
        // skip positions
        skip_positions(*pos_in_);
        count -= postings_writer::BLOCK_SIZE;
      }
      refill();
      buf_pos_ = 0;
      left = postings_writer::BLOCK_SIZE;
    }

    if (count < left) {
      buf_pos_ += count;
    }
    clear();
    value_ = 0;
  }

  uint32_t pos_deltas_[postings_writer::BLOCK_SIZE]; /* buffer to store position deltas */
  const uint32_t* freq_; /* lenght of the posting list for a document */
  uint32_t* enc_buf_; /* auxillary buffer to decode data */
  uint32_t pend_pos_{}; /* how many positions "behind" we are */
  uint64_t tail_start_; /* file pointer where the last (vInt encoded) pos delta block is */
  size_t tail_length_; /* number of positions in the last (vInt encoded) pos delta block */
  uint32_t value_{ irs::type_limits<irs::type_t::pos_t>::invalid() }; // current position
  uint32_t buf_pos_{ postings_writer::BLOCK_SIZE } ; /* current position in pos_deltas_ buffer */
  index_input::ptr pos_in_;
  features features_; /* field features */

 private:
  template<typename T>
  friend class pos_doc_iterator;
}; // pos_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class offs_pay_iterator
///////////////////////////////////////////////////////////////////////////////
class offs_pay_iterator final: public pos_iterator {
 public:
  DECLARE_UNIQUE_PTR(offs_pay_iterator);

  offs_pay_iterator()
    : pos_iterator(2) { // offset + payload
    attrs_.emplace(offs_);
    attrs_.emplace(pay_);
  }

  virtual void clear() override {
    pos_iterator::clear();
    offs_.clear();
    pay_.clear();
  }

  virtual void prepare(const doc_state& state) override {
    pos_iterator::prepare(state);
    pay_in_ = state.pay_in->reopen(); // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen payload input in: %s", __FUNCTION__);

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  virtual void prepare(const skip_state& state) override {
    pos_iterator::prepare(state);
    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

 protected:
  virtual void read_attributes() override {
    offs_.start += offs_start_deltas_[buf_pos_];
    offs_.end = offs_.start + offs_lengts_[buf_pos_];

    pay_.value = bytes_ref(
      pay_data_.c_str() + pay_data_pos_,
      pay_lengths_[buf_pos_]);
    pay_data_pos_ += pay_lengths_[buf_pos_];
  }

  virtual void skip(uint32_t count) override {
    auto left = postings_writer::BLOCK_SIZE - buf_pos_;
    if (count >= left) {
      count -= left;
      // skip block by block
      while (count >= postings_writer::BLOCK_SIZE) {
        skip_positions(*pos_in_);
        skip_payload(*pay_in_);
        skip_offsets(*pay_in_);
        count -= postings_writer::BLOCK_SIZE;
      }
      refill();
      buf_pos_ = 0;
      left = postings_writer::BLOCK_SIZE;
    }

    if (count < left) {
      // current payload start
      const auto begin = pay_lengths_ + buf_pos_;
      const auto end = begin + count;
      pay_data_pos_ = std::accumulate(begin, end, pay_data_pos_);
      buf_pos_ += count;
    }
    clear();
    value_ = 0;
  }

  virtual void refill() override {
    if (pos_in_->file_pointer() == tail_start_) {
      size_t pos = 0;

      for (size_t i = 0; i < tail_length_; ++i) {
        // read payloads
        if (shift_unpack_32(pos_in_->read_vint(), pos_deltas_[i])) {
          pay_lengths_[i] = pos_in_->read_vint();
        } else {
          assert(i);
          pay_lengths_[i] = pay_lengths_[i-1];
        }

        if (pay_lengths_[i]) {
          const auto size = pay_lengths_[i]; // length of current payload

          string_utils::oversize(pay_data_, pos + size);

          #ifdef IRESEARCH_DEBUG
            const auto read = pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
            assert(read == size);
            UNUSED(read);
          #else
            pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
          #endif // IRESEARCH_DEBUG

          pos += size;
        }

        if (shift_unpack_32(pos_in_->read_vint(), offs_start_deltas_[i])) {
          offs_lengts_[i] = pos_in_->read_vint();
        } else {
          assert(i);
          offs_lengts_[i] = offs_lengts_[i - 1];
        }
      }
    } else {
      format_traits::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);

      // read payloads
      const uint32_t size = pay_in_->read_vint();
      if (size) {
        format_traits::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, pay_lengths_);
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
      format_traits::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_start_deltas_);
      format_traits::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_lengts_);
    }
    pay_data_pos_ = 0;
  }

  index_input::ptr pay_in_;
  offset offs_;
  payload pay_;
  uint32_t offs_start_deltas_[postings_writer::BLOCK_SIZE]{}; /* buffer to store offset starts */
  uint32_t offs_lengts_[postings_writer::BLOCK_SIZE]{}; /* buffer to store offset lengths */
  uint32_t pay_lengths_[postings_writer::BLOCK_SIZE]{}; /* buffer to store payload lengths */
  size_t pay_data_pos_{}; /* current position in a payload buffer */
  bstring pay_data_; // buffer to store payload data
}; // pay_offs_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class offs_iterator
///////////////////////////////////////////////////////////////////////////////
class offs_iterator final : public pos_iterator {
 public:
  DECLARE_UNIQUE_PTR(offs_iterator);

  offs_iterator()
    : pos_iterator(1) { // offset
    attrs_.emplace(offs_);
  }

  virtual void clear() override {
    pos_iterator::clear();
    offs_.clear();
  }

  virtual void prepare(const doc_state& state) override {
    pos_iterator::prepare(state);
    pay_in_ = state.pay_in->reopen(); // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen payload input in: %s", __FUNCTION__);

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  virtual void prepare(const skip_state& state) override {
    pos_iterator::prepare(state);
    pay_in_->seek(state.pay_ptr);
  }

 protected:
  virtual void read_attributes() override {
    offs_.start += offs_start_deltas_[buf_pos_];
    offs_.end = offs_.start + offs_lengts_[buf_pos_];
  }

  virtual void refill() override {
    if (pos_in_->file_pointer() == tail_start_) {
      uint32_t pay_size = 0;
      for (size_t i = 0; i < tail_length_; ++i) {
        /* skip payloads */
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

        /* read offsets */
        if (shift_unpack_32(pos_in_->read_vint(), offs_start_deltas_[i])) {
          offs_lengts_[i] = pos_in_->read_vint();
        } else {
          assert(i);
          offs_lengts_[i] = offs_lengts_[i - 1];
        }
      }
    } else {
      format_traits::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);

      // skip payload
      if (features_.payload()) {
        skip_payload(*pay_in_);
      }

      // read offsets
      format_traits::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_start_deltas_);
      format_traits::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_lengts_);
    }
  }

  virtual void skip(uint32_t count) override {
    auto left = postings_writer::BLOCK_SIZE - buf_pos_;
    if (count >= left) {
      count -= left;
      // skip block by block
      while (count >= postings_writer::BLOCK_SIZE) {
        skip_positions(*pos_in_);
        if (features_.payload()) {
          skip_payload(*pay_in_);
        }
        skip_offsets(*pay_in_);
        count -= postings_writer::BLOCK_SIZE;
      }
      refill();
      buf_pos_ = 0;
      left = postings_writer::BLOCK_SIZE;
    }

    if (count < left) {
      buf_pos_ += count;
    }
    clear();
    value_ = 0;
  }

  index_input::ptr pay_in_;
  offset offs_;
  uint32_t offs_start_deltas_[postings_writer::BLOCK_SIZE]; /* buffer to store offset starts */
  uint32_t offs_lengts_[postings_writer::BLOCK_SIZE]; /* buffer to store offset lengths */
}; // offs_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class pay_iterator
///////////////////////////////////////////////////////////////////////////////
class pay_iterator final : public pos_iterator {
 public:
  DECLARE_UNIQUE_PTR(pay_iterator);

  pay_iterator()
    : pos_iterator(1) { // payload
    attrs_.emplace(pay_);
  }

  virtual void clear() override {
    pos_iterator::clear();
    pay_.clear();
  }

  virtual void prepare(const doc_state& state) override {
    pos_iterator::prepare(state);
    pay_in_ = state.pay_in->reopen(); // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen payload input in: %s", __FUNCTION__);

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  virtual void prepare(const skip_state& state) override {
    pos_iterator::prepare(state);
    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

 protected:
  virtual void read_attributes() override {
    pay_.value = bytes_ref(
      pay_data_.data() + pay_data_pos_,
      pay_lengths_[buf_pos_]
    );
    pay_data_pos_ += pay_lengths_[buf_pos_];
  }

  virtual void skip(uint32_t count) override {
    auto left = postings_writer::BLOCK_SIZE - buf_pos_;
    if (count >= left) {
      count -= left;
      // skip block by block
      while (count >= postings_writer::BLOCK_SIZE) {
        skip_positions(*pos_in_);
        skip_payload(*pay_in_);
        if (features_.offset()) {
          skip_offsets(*pay_in_);
        }
        count -= postings_writer::BLOCK_SIZE;
      }
      refill();
      buf_pos_ = 0;
      left = postings_writer::BLOCK_SIZE;
    }

    if (count < left) {
      // current payload start
      const auto begin = pay_lengths_ + buf_pos_;
      const auto end = begin + count;
      pay_data_pos_ = std::accumulate(begin, end, pay_data_pos_);
      buf_pos_ += count;
    }
    clear();
    value_ = 0;
  }

  virtual void refill() override {
    if (pos_in_->file_pointer() == tail_start_) {
      size_t pos = 0;

      for (size_t i = 0; i < tail_length_; ++i) {
        // read payloads
        if (shift_unpack_32(pos_in_->read_vint(), pos_deltas_[i])) {
          pay_lengths_[i] = pos_in_->read_vint();
        } else {
          assert(i);
          pay_lengths_[i] = pay_lengths_[i-1];
        }

        if (pay_lengths_[i]) {
          const auto size = pay_lengths_[i]; // current payload length

          string_utils::oversize(pay_data_, pos + size);

          #ifdef IRESEARCH_DEBUG
            const auto read = pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
            assert(read == size);
            UNUSED(read);
          #else
            pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
          #endif // IRESEARCH_DEBUG

          pos += size;
        }

        // skip offsets
        if (features_.offset()) {
          uint32_t code;
          if (shift_unpack_32(pos_in_->read_vint(), code)) {
            pos_in_->read_vint();
          }
        }
      }
    } else {
      format_traits::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);

      /* read payloads */
      const uint32_t size = pay_in_->read_vint();
      if (size) {
        format_traits::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, pay_lengths_);
        string_utils::oversize(pay_data_, size);

        #ifdef IRESEARCH_DEBUG
          const auto read = pay_in_->read_bytes(&(pay_data_[0]), size);
          assert(read == size);
          UNUSED(read);
        #else
          pay_in_->read_bytes(&(pay_data_[0]), size);
        #endif // IRESEARCH_DEBUG
      }

      // skip offsets
      if (features_.offset()) {
        skip_offsets(*pay_in_);
      }
    }
    pay_data_pos_ = 0;
  }

  index_input::ptr pay_in_;
  payload pay_;
  uint32_t pay_lengths_[postings_writer::BLOCK_SIZE]{}; /* buffer to store payload lengths */
  uint64_t pay_data_pos_{}; /* current postition in payload buffer */
  bstring pay_data_; // buffer to store payload data
}; // pay_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class pos_doc_iterator
///////////////////////////////////////////////////////////////////////////////
template<typename PosItrType>
class pos_doc_iterator final: public doc_iterator {
 public:
  virtual bool next() override {
    if (begin_ == end_) {
      cur_pos_ += relative_pos();

      if (cur_pos_ == term_state_.docs_count) {
        doc_.value = type_limits<type_t::doc_id_t>::eof();
        begin_ = end_ = docs_; // seal the iterator
        return false;
      }

      refill();
    }

    // update document attribute
    doc_.value = *begin_++;

    // update frequency attribute
    freq_.value = *doc_freq_++;

    // update position attribute
    pos_.pend_pos_ += freq_.value;
    pos_.clear();

    return true;
  }

 protected:
  virtual void prepare_attributes(
    const ::features& features,
    const irs::attribute_view& attrs,
    const index_input* pos_in,
    const index_input* pay_in
  ) final;

  virtual void seek_notify(const skip_context &ctx) override final {
    pos_.prepare(ctx); // notify positions
  }

 private:
  PosItrType pos_;
}; // pos_doc_iterator

template<typename PosItrType>
void pos_doc_iterator<PosItrType>::prepare_attributes(
    const ::features& enabled,
    const irs::attribute_view& attrs,
    const index_input* pos_in,
    const index_input* pay_in) {
  assert(attrs.contains<frequency>());
  assert(enabled.position());
  attrs_.emplace(freq_);
  term_freq_ = attrs.get<frequency>()->value;

  // ...........................................................................
  // position attribute
  // ...........................................................................

  doc_state state;
  state.pos_in = pos_in;
  state.pay_in = pay_in;
  state.term_state = &term_state_;
  state.freq = &freq_.value;
  state.features = features_;
  state.enc_buf = enc_buf_;

  if (term_freq_ < postings_writer::BLOCK_SIZE) {
    state.tail_start = term_state_.pos_start;
  } else if (term_freq_ == postings_writer::BLOCK_SIZE) {
    state.tail_start = type_limits<type_t::address_t>::invalid();
  } else {
    state.tail_start = term_state_.pos_start + term_state_.pos_end;
  }

  state.tail_length = term_freq_ % postings_writer::BLOCK_SIZE;
  pos_.prepare(state);
  attrs_.emplace(pos_);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                index_meta_writer
// ----------------------------------------------------------------------------

struct index_meta_writer final: public irs::index_meta_writer {
  static const string_ref FORMAT_NAME;
  static const string_ref FORMAT_PREFIX;
  static const string_ref FORMAT_PREFIX_TMP;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  virtual std::string filename(const index_meta& meta) const override;
  using irs::index_meta_writer::prepare;
  virtual bool prepare(directory& dir, index_meta& meta) override;
  virtual bool commit() override;
  virtual void rollback() NOEXCEPT override;
 private:
  directory* dir_ = nullptr;
  index_meta* meta_ = nullptr;
}; // index_meta_writer

template<>
std::string file_name<irs::index_meta_writer, index_meta>(const index_meta& meta) {
  return file_name(index_meta_writer::FORMAT_PREFIX_TMP, meta.generation());
};

struct index_meta_reader final: public irs::index_meta_reader {
  virtual bool last_segments_file(
    const directory& dir, std::string& name
  ) const override;

  virtual void read(
    const directory& dir,
    index_meta& meta,
    const string_ref& filename = string_ref::NIL // null == use meta
  ) override;
}; // index_meta_reader

template<>
std::string file_name<irs::index_meta_reader, index_meta>(const index_meta& meta) {
  return file_name(index_meta_writer::FORMAT_PREFIX, meta.generation());
};

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref index_meta_writer::FORMAT_PREFIX = "segments_";
const string_ref index_meta_writer::FORMAT_PREFIX_TMP = "pending_segments_";
const string_ref index_meta_writer::FORMAT_NAME = "iresearch_10_index_meta";
MSVC2015_ONLY(__pragma(warning(pop)))

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
      seg_file.c_str()
    ));
  }

  {
    format_utils::write_header(*out, FORMAT_NAME, FORMAT_MAX);
    out->write_vlong(meta.generation());
    out->write_long(meta.counter());
    assert(meta.size() <= integer_traits<uint32_t>::const_max);
    out->write_vint(uint32_t(meta.size()));

    for (auto& segment: meta) {
      write_string(*out, segment.filename);
      write_string(*out, segment.meta.codec->type().name());
    }

    format_utils::write_footer(*out);
    // important to close output here
  }

  if (!dir.sync(seg_file)) {
    throw io_error(string_utils::to_string(
      "failed to sync file, path: %s",
      seg_file.c_str()
    ));
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
      dst.c_str()
    ));
  }

  // only noexcept operations below
  complete(*meta_);

  // clear pending state
  meta_ = nullptr;
  dir_ = nullptr;

  return true;
}

void index_meta_writer::rollback() NOEXCEPT {
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
    meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      meta_file.c_str()
    ));
  }

  const auto checksum = format_utils::checksum(*in);

  // check header
  format_utils::check_header(
    *in,
    index_meta_writer::FORMAT_NAME,
    index_meta_writer::FORMAT_MIN,
    index_meta_writer::FORMAT_MAX
  );

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

  format_utils::check_footer(*in, checksum);
  complete(meta, gen, cnt, std::move(segments));
}

// ----------------------------------------------------------------------------
// --SECTION--                                              segment_meta_writer
// ----------------------------------------------------------------------------

struct segment_meta_writer final : public irs::segment_meta_writer{
  static const string_ref FORMAT_EXT;
  static const string_ref FORMAT_NAME;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  enum {
    HAS_COLUMN_STORE = 1,
  };

  virtual void write(
    directory& dir,
    std::string& filename,
    const segment_meta& meta
  ) override;
}; // segment_meta_writer

template<>
std::string file_name<irs::segment_meta_writer, segment_meta>(const segment_meta& meta) {
  return irs::file_name(meta.name, meta.version, segment_meta_writer::FORMAT_EXT);
}

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref segment_meta_writer::FORMAT_EXT = "sm";
const string_ref segment_meta_writer::FORMAT_NAME = "iresearch_10_segment_meta";
MSVC2015_ONLY(__pragma(warning(pop)))

void segment_meta_writer::write(directory& dir, std::string& meta_file, const segment_meta& meta) {
  meta_file = file_name<irs::segment_meta_writer>(meta);
  auto out = dir.create(meta_file);

  if (!out) {
    throw io_error(string_utils::to_string(
      "failed to create file, path: %s",
      meta_file.c_str()
    ));
  }

  const byte_type flags = meta.column_store
    ? segment_meta_writer::HAS_COLUMN_STORE
    : 0;

  format_utils::write_header(*out, FORMAT_NAME, FORMAT_MAX);
  write_string(*out, meta.name);
  out->write_vlong(meta.version);
  out->write_vlong(meta.live_docs_count);
  out->write_vlong(meta.docs_count - meta.live_docs_count); // docs_count >= live_docs_count
  out->write_vlong(meta.size);
  out->write_byte(flags);
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
    const string_ref& filename = string_ref::NIL // null == use meta
  ) override;
}; // segment_meta_reader

void segment_meta_reader::read(
    const directory& dir,
    segment_meta& meta,
    const string_ref& filename /*= string_ref::NIL*/) {

  const std::string meta_file = filename.null()
    ? file_name<irs::segment_meta_writer>(meta)
    : static_cast<std::string>(filename);

  auto in = dir.open(
    meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      meta_file.c_str()
    ));
  }

  const auto checksum = format_utils::checksum(*in);

  format_utils::check_header(
    *in,
    segment_meta_writer::FORMAT_NAME,
    segment_meta_writer::FORMAT_MIN,
    segment_meta_writer::FORMAT_MAX
  );

  auto name = read_string<std::string>(*in);
  const auto version = in->read_vlong();
  const auto live_docs_count = in->read_vlong();
  const auto docs_count = in->read_vlong() + live_docs_count;

  if (docs_count < live_docs_count) {
    throw index_error(std::string("while reader segment meta '") + name
      + "', error: docs_count(" + std::to_string(docs_count)
      + ") < live_docs_count(" + std::to_string(live_docs_count) + ")"
    );
  }

  const auto size = in->read_vlong();
  const auto flags = in->read_byte();
  auto files = read_strings<segment_meta::file_set>(*in);

  if (flags & ~(segment_meta_writer::HAS_COLUMN_STORE)) {
    throw index_error(
      std::string("while reading segment meta '" + name
      + "', error: use of unsupported flags '" + std::to_string(flags) + "'")
    );
  }

  format_utils::check_footer(*in, checksum);

  // ...........................................................................
  // all operations below are noexcept
  // ...........................................................................

  meta.name = std::move(name);
  meta.version = version;
  meta.column_store = flags & segment_meta_writer::HAS_COLUMN_STORE;
  meta.docs_count = docs_count;
  meta.live_docs_count = live_docs_count;
  meta.size = size;
  meta.files = std::move(files);
}

// ----------------------------------------------------------------------------
// --SECTION--                                             document_mask_writer
// ----------------------------------------------------------------------------

class document_mask_writer final: public irs::document_mask_writer {
 public:
  static const string_ref FORMAT_EXT;
  static const string_ref FORMAT_NAME;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  virtual ~document_mask_writer() = default;

  virtual std::string filename(
    const segment_meta& meta
  ) const override;

  virtual void write(
    directory& dir,
    const segment_meta& meta,
    const document_mask& docs_mask
  ) override;
}; // document_mask_writer

template<>
std::string file_name<irs::document_mask_writer, segment_meta>(const segment_meta& meta) {
  return file_name(meta.name, meta.version, document_mask_writer::FORMAT_EXT);
};

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref document_mask_writer::FORMAT_NAME = "iresearch_10_doc_mask";
const string_ref document_mask_writer::FORMAT_EXT = "doc_mask";
MSVC2015_ONLY(__pragma(warning(pop)))

std::string document_mask_writer::filename(const segment_meta& meta) const {
  return file_name<irs::document_mask_writer>(meta);
}

void document_mask_writer::write(
    directory& dir,
    const segment_meta& meta,
    const document_mask& docs_mask
) {
  const auto filename = file_name<irs::document_mask_writer>(meta);
  auto out = dir.create(filename);

  if (!out) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s",
      filename.c_str()
    ));
  }

  // segment can't have more than integer_traits<uint32_t>::const_max documents
  assert(docs_mask.size() <= integer_traits<uint32_t>::const_max);
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
    document_mask& docs_mask
  ) override;
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
      in_name.c_str()
    ));
  }

  if (!exists) {
    // possible that the file does not exist since document_mask is optional
    return false;
  }

  auto in = dir.open(
    in_name, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      in_name.c_str()
    ));
  }

  const auto checksum = format_utils::checksum(*in);

  format_utils::check_header(
    *in,
    document_mask_writer::FORMAT_NAME,
    document_mask_writer::FORMAT_MIN,
    document_mask_writer::FORMAT_MAX
  );

  auto count = in->read_vint();

  while (count--) {
    static_assert(
      sizeof(doc_id_t) == sizeof(decltype(in->read_vint())),
      "sizeof(doc_id) != sizeof(decltype(id))"
    );

    docs_mask.insert(in->read_vint());
  }

  format_utils::check_footer(*in, checksum);

  return true;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                      columnstore
// ----------------------------------------------------------------------------

NS_BEGIN(columns)

template<typename T, typename M>
std::string file_name(const M& meta); // forward declaration

template<typename T, typename M>
void file_name(std::string& buf, const M& meta); // forward declaration

class meta_writer final : public irs::column_meta_writer {
 public:
  static const string_ref FORMAT_NAME;
  static const string_ref FORMAT_EXT;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  virtual void prepare(directory& dir, const segment_meta& meta) override;
  virtual void write(const std::string& name, field_id id) override;
  virtual void flush() override;

 private:
  index_output::ptr out_;
  size_t count_{}; // number of written objects
  field_id max_id_{}; // the highest column id written (optimization for vector resize on read to highest id)
}; // meta_writer

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref meta_writer::FORMAT_NAME = "iresearch_10_columnmeta";
const string_ref meta_writer::FORMAT_EXT = "cm";
MSVC2015_ONLY(__pragma(warning(pop)))

template<>
std::string file_name<column_meta_writer, segment_meta>(
    const segment_meta& meta
) {
  return irs::file_name(meta.name, columns::meta_writer::FORMAT_EXT);
};

void meta_writer::prepare(directory& dir, const segment_meta& meta) {
  auto filename = file_name<column_meta_writer>(meta);

  out_ = dir.create(filename);

  if (!out_) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s", filename.c_str()
    ));
  }

  format_utils::write_header(*out_, FORMAT_NAME, FORMAT_MAX);
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
    field_id& max_id
  ) override;
  virtual bool read(column_meta& column) override;

 private:
  index_input::ptr in_;
  size_t count_{0};
  field_id max_id_{0};
}; // meta_writer

bool meta_reader::prepare(
    const directory& dir,
    const segment_meta& meta,
    size_t& count,
    field_id& max_id
) {
  const auto filename = file_name<column_meta_writer>(meta);

  bool exists;

  if (!dir.exists(exists, filename)) {
    throw io_error(string_utils::to_string(
      "failed to check existence of file, path: %s",
      filename.c_str()
    ));
  }

  if (!exists) {
    // column meta is optional
    return false;
  }

  auto in = dir.open(
    filename, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      filename.c_str()
    ));
  }

  const auto checksum = format_utils::checksum(*in);

  in->seek( // seek to start of meta footer (before count and max_id)
    in->length() - sizeof(size_t) - sizeof(field_id) - format_utils::FOOTER_LEN
  );
  count = in->read_long(); // read number of objects to read
  max_id = in->read_long(); // read highest column id written

  if (max_id >= irs::integer_traits<size_t>::const_max) {
    throw index_error(string_utils::to_string(
      "invalid max column id: " IR_UINT64_T_SPECIFIER ", path: %s",
      max_id, filename.c_str()
    ));
  }

  format_utils::check_footer(*in, checksum);

  in->seek(0);

  format_utils::check_header(
    *in,
    meta_writer::FORMAT_NAME,
    meta_writer::FORMAT_MIN,
    meta_writer::FORMAT_MAX
  );

  in_ = std::move(in);
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
// |Bloom Filter| <- not implemented yet
// |Last block #0 key|Block #0 offset|
// |Last block #1 key|Block #1 offset| <-- Columnstore blocks index
// |Last block #2 key|Block #2 offset|
// ...
// |Bloom filter offset| <- not implemented yet
// |Footer|

const uint32_t INDEX_BLOCK_SIZE = 1024;
const size_t MAX_DATA_BLOCK_SIZE = 8192;

// By default we treat columns as a variable length sparse columns
enum ColumnProperty : uint32_t {
  CP_SPARSE = 0,
  CP_DENSE = 1, // keys can be presented as an array indices
  CP_FIXED = 2, // fixed length colums
  CP_MASK = 4, // column contains no data
}; // ColumnProperty

ENABLE_BITMASK_ENUM(ColumnProperty);

ColumnProperty write_compact(
    irs::index_output& out,
    irs::compressor& compressor,
    const irs::bytes_ref& data) {
  if (data.empty()) {
    out.write_byte(0); // zig_zag_encode32(0) == 0
    return CP_MASK;
  }

  // compressor can only handle size of int32_t, so can use the negative flag as a compression flag
  compressor.compress(reinterpret_cast<const char*>(data.c_str()), data.size());

  if (compressor.size() < data.size()) {
    assert(compressor.size() <= irs::integer_traits<int32_t>::const_max);
    irs::write_zvint(out, int32_t(compressor.size())); // compressed size
    out.write_bytes(compressor.c_str(), compressor.size());
    irs::write_zvlong(out, data.size() - MAX_DATA_BLOCK_SIZE); // original size
  } else {
    assert(data.size() <= irs::integer_traits<int32_t>::const_max);
    irs::write_zvint(out, int32_t(0) - int32_t(data.size())); // -ve to mark uncompressed
    out.write_bytes(data.c_str(), data.size());
  }

  return CP_SPARSE;
}

void read_compact(
    irs::index_input& in,
    const irs::decompressor& decompressor,
    irs::bstring& encode_buf,
    irs::bstring& decode_buf) {
  const auto size = irs::read_zvint(in);

  if (!size) {
    return;
  }

  size_t buf_size = std::abs(size);

  // -ve to mark uncompressed
  if (size < 0) {
    decode_buf.resize(buf_size); // ensure that we have enough space to store decompressed data

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(&(decode_buf[0]), buf_size);
    assert(read == buf_size);
    UNUSED(read);
#else
    in.read_bytes(&(decode_buf[0]), buf_size);
#endif // IRESEARCH_DEBUG
    return;
  }

  irs::string_utils::oversize(encode_buf, buf_size);

#ifdef IRESEARCH_DEBUG
  const auto read = in.read_bytes(&(encode_buf[0]), buf_size);
  assert(read == buf_size);
  UNUSED(read);
#else
  in.read_bytes(&(encode_buf[0]), buf_size);
#endif // IRESEARCH_DEBUG

  // ensure that we have enough space to store decompressed data
  decode_buf.resize(irs::read_zvlong(in) + MAX_DATA_BLOCK_SIZE);

  buf_size = decompressor.deflate(
    reinterpret_cast<const char*>(encode_buf.c_str()),
    buf_size,
    reinterpret_cast<char*>(&decode_buf[0]),
    decode_buf.size()
  );

  if (!irs::type_limits<irs::type_t::address_t>::valid(buf_size)) {
    throw irs::index_error(string_utils::to_string(
      "while reading compact, error: invalid buffer size '" IR_SIZE_T_SPECIFIER "'",
      buf_size
    ));
  }
}

template<size_t Size>
class index_block {
 public:
  static const size_t SIZE = Size;

  void push_back(doc_id_t key, uint64_t offset) NOEXCEPT {
    assert(key_ >= keys_);
    assert(key_ < keys_ + Size);
    *key_++ = key;
    assert(key >= key_[-1]);
    assert(offset_ >= offsets_);
    assert(offset_ < offsets_ + Size);
    *offset_++ = offset;
    assert(offset >= offset_[-1]);
  }

  void pop_back() NOEXCEPT {
    assert(key_ > keys_);
    *key_-- = 0;
    assert(offset_ > offsets_);
    *offset_-- = 0;
  }

  // returns total number of items
  uint32_t total() const NOEXCEPT {
    return flushed()+ size();
  }

  // returns total number of flushed items
  uint32_t flushed() const NOEXCEPT {
    return flushed_;
  }

  // returns number of items to be flushed
  uint32_t size() const NOEXCEPT {
    assert(key_ >= keys_);
    return uint32_t(key_ - keys_);
  }

  bool empty() const NOEXCEPT {
    return keys_ == key_;
  }

  bool full() const NOEXCEPT {
    return key_ == std::end(keys_);
  }

  doc_id_t min_key() const NOEXCEPT {
    return *keys_;
  }

  doc_id_t max_key() const NOEXCEPT {
    // if this->empty(), will point to last offset
    // value which is 0 in this case
    return *(key_-1);
  }

  uint64_t max_offset() const NOEXCEPT {
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
      // that is multiple to the block size
      const auto block_size = math::ceil32(size, packed::BLOCK_SIZE_32);
      assert(block_size >= size);

      const auto stats = encode::avg::encode(keys_, key_);
      const auto bits = encode::avg::write_block(
        out, stats.first, stats.second,
        keys_, block_size, reinterpret_cast<uint32_t*>(buf)
      );

      if (1 == stats.second && 0 == keys_[0] && encode::bitpack::rl(bits)) {
        props |= CP_DENSE;
      }
    }

    // write offsets
    {
      // adjust number of elements to pack to the nearest value
      // that is multiple to the block size
      const auto block_size = math::ceil64(size, packed::BLOCK_SIZE_64);
      assert(block_size >= size);

      const auto stats = encode::avg::encode(offsets_, offset_);
      const auto bits = encode::avg::write_block(
        out, stats.first, stats.second,
        offsets_, block_size, buf
      );

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
  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  static const string_ref FORMAT_NAME;
  static const string_ref FORMAT_EXT;

  virtual void prepare(directory& dir, const segment_meta& meta) override;
  virtual column_t push_column() override;
  virtual bool commit() override;
  virtual void rollback() NOEXCEPT override;

 private:
  class column final : public irs::columnstore_writer::column_output {
   public:
    explicit column(writer& ctx)
      : ctx_(&ctx),
        blocks_index_(*ctx.alloc_) {
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

    bool empty() const NOEXCEPT {
      return !block_index_.total();
    }

    void finish() {
      auto& out = *ctx_->data_out_;
      write_enum(out, ColumnProperty(((column_props_ & CP_DENSE) << 3) | blocks_props_)); // column properties
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
      column_index_.flush(blocks_index_.stream, ctx_->buf_);
      blocks_index_.stream.flush();
    }

    virtual void close() override {
      // NOOP
    }

    virtual void write_byte(byte_type b) override {
      block_buf_.write_byte(b);
    }

    virtual void write_bytes(const byte_type* b, size_t size) override {
      block_buf_.write_bytes(b, size);
    }

    virtual void reset() override {
      if (block_index_.empty()) {
        // nothing to reset
        return;
      }

      // reset to previous offset
      block_buf_.reset(block_index_.max_offset());
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
      auto* buf = ctx_->buf_;

      // write first block key & where block starts
      column_index_.push_back(block_index_.min_key(), out.file_pointer());

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
      block_props |= write_compact(out, ctx_->comp_, static_cast<bytes_ref>(block_buf_));
      length_ += block_buf_.size();

      // refresh blocks properties
      blocks_props_ &= block_props;
      // reset buffer stream after flush
      block_buf_.reset();

      // refresh column properties
      // column is dense IFF
      // - all blocks are dense
      // - there are no gaps between blocks
      column_props_ &= ColumnProperty(0 != (block_props & CP_DENSE));
    }

    writer* ctx_; // writer context
    uint64_t length_{}; // size of all data blocks in the column
    index_block<INDEX_BLOCK_SIZE> block_index_; // current block index (per document key/offset)
    index_block<INDEX_BLOCK_SIZE> column_index_; // column block index (per block key/offset)
    memory_output blocks_index_; // blocks index
    bytes_output block_buf_{ 2*MAX_DATA_BLOCK_SIZE }; // data buffer
    doc_id_t max_{ type_limits<type_t::doc_id_t>::invalid() }; // max key (among flushed blocks)
    ColumnProperty blocks_props_{ CP_DENSE | CP_FIXED | CP_MASK }; // aggregated column blocks properties
    ColumnProperty column_props_{ CP_DENSE }; // aggregated column block index properties
    uint32_t avg_block_count_{}; // average number of items per block (tail block is not taken into account since it may skew distribution)
    uint32_t avg_block_size_{}; // average size of the block (tail block is not taken into account since it may skew distribution)
  }; // column

  memory_allocator* alloc_{ &memory_allocator::global() };
  uint64_t buf_[INDEX_BLOCK_SIZE]; // reusable temporary buffer for packing
  std::deque<column> columns_; // pointers remain valid
  compressor comp_{ 2*MAX_DATA_BLOCK_SIZE };
  index_output::ptr data_out_;
  std::string filename_;
  directory* dir_;
}; // writer

template<>
std::string file_name<columnstore_writer, segment_meta>(
    const segment_meta& meta
) {
  return file_name(meta.name, columns::writer::FORMAT_EXT);
};

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref writer::FORMAT_NAME = "iresearch_10_columnstore";
const string_ref writer::FORMAT_EXT = "cs";
MSVC2015_ONLY(__pragma(warning(pop)))

void writer::prepare(directory& dir, const segment_meta& meta) {
  columns_.clear();

  auto filename = file_name<columnstore_writer>(meta);
  auto data_out = dir.create(filename);

  if (!data_out) {
    throw io_error(string_utils::to_string(
      "Failed to create file, path: %s",
      filename.c_str()
    ));
  }

  format_utils::write_header(*data_out, FORMAT_NAME, FORMAT_MAX);

  alloc_ = &directory_utils::get_allocator(dir);

  // noexcept block
  dir_ = &dir;
  data_out_ = std::move(data_out);
  filename_ = std::move(filename);
}

columnstore_writer::column_t writer::push_column() {
  const auto id = columns_.size();
  columns_.emplace_back(*this);
  auto& column = columns_.back();

  return std::make_pair(id, [&column] (doc_id_t doc) -> column_output& {
    // to avoid extra (and useless in our case) check for block index
    // emptiness in 'writer::column::prepare', we disallow passing
    // doc <= irs::type_limits<irs::type_t::doc_id_t>::invalid() || doc >= irs::type_limits<irs::type_t::doc_id_t>::eof()
    assert(doc > irs::type_limits<irs::type_t::doc_id_t>::invalid() && doc < irs::type_limits<irs::type_t::doc_id_t>::eof());

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

void writer::rollback() NOEXCEPT {
  filename_.clear();
  dir_ = nullptr;
  data_out_.reset(); // close output
  columns_.clear(); // ensure next flush (without prepare(...)) will use the section without 'data_out_'
}

template<typename Block, typename Allocator>
class block_cache : irs::util::noncopyable {
 public:
  block_cache(const Allocator& alloc = Allocator())
    : cache_(alloc) {
  }
  block_cache(block_cache&& rhs) NOEXCEPT
    : cache_(std::move(rhs.cache_)) {
  }

  template<typename... Args>
  Block& emplace_back(Args&&... args) {
    cache_.emplace_back(std::forward<Args>(args)...);
    return cache_.back();
  }

  void pop_back() NOEXCEPT {
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
    doc_id_t key{ type_limits<type_t::doc_id_t>::eof() };
    uint64_t offset;
  };

 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) NOEXCEPT {
      next_ = std::lower_bound(
        begin_, end_, doc,
        [](const ref& lhs, doc_id_t rhs) {
          return lhs.key < rhs;
      });

      return next();
    }

    const irs::doc_id_t& value() const NOEXCEPT { return value_; }

    const irs::bytes_ref& value_payload() const NOEXCEPT {
      return value_payload_;
    }

    bool next() NOEXCEPT {
      if (next_ == end_) {
        return false;
      }

      value_ = next_->key;
      const auto vbegin = next_->offset;

      begin_ = next_;
      const auto vend = (++next_ == end_ ? data_->size() : next_->offset);

      assert(vend >= vbegin);
      value_payload_ = bytes_ref(
        data_->c_str() + vbegin, // start
        vend - vbegin // length
      );

      return true;
    }

    void seal() NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::eof();
      value_payload_ = irs::bytes_ref::NIL;
      next_ = begin_ = end_;
    }

    void reset(const sparse_block& block) NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      value_payload_ = irs::bytes_ref::NIL;
      next_ = begin_ = std::begin(block.index_);
      end_ = block.end_;
      data_ = &block.data_;

      assert(std::is_sorted(
        begin_, end_,
        [](const sparse_block::ref& lhs, const sparse_block::ref& rhs) {
          return lhs.key < rhs.key;
      }));
    }

    bool operator==(const sparse_block& rhs) const NOEXCEPT {
      return data_ == &rhs.data_;
    }

    bool operator!=(const sparse_block& rhs) const NOEXCEPT {
      return !(*this == rhs);
    }

   private:
    irs::bytes_ref value_payload_ { irs::bytes_ref::NIL };
    irs::doc_id_t value_ { irs::type_limits<irs::type_t::doc_id_t>::invalid() };
    const sparse_block::ref* next_{}; // next position
    const sparse_block::ref* begin_{};
    const sparse_block::ref* end_{};
    const bstring* data_{};
  }; // iterator

  void load(index_input& in, decompressor& decomp, bstring& buf) {
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
    read_compact(in, decomp, buf, data_);
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
      vend - vbegin // length
    );

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
        begin->offset - vbegin // length
      );

      if (!visitor(key, value)) {
        return false;
      }
    }

    // visit tail block
    assert(data_.size() >= begin->offset);
    value = bytes_ref(
      data_.c_str() + begin->offset, // start
      data_.size() - begin->offset // length
    );

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
    bool seek(doc_id_t doc) NOEXCEPT {
      // before the current element
      if (doc <= value_) {
        doc = value_;
      }

      // FIXME refactor
      it_ = begin_ + doc - base_;

      return next();
    }

    const irs::doc_id_t& value() const NOEXCEPT { return value_; }

    const irs::bytes_ref& value_payload() const NOEXCEPT {
      return value_payload_;
    }

    bool next() NOEXCEPT {
      if (it_ >= end_) {
        // after the last element
        return false;
      }

      value_ = base_ + std::distance(begin_, it_);
      next_value();

      return true;
    }

    void seal() NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::eof();
      value_payload_ = irs::bytes_ref::NIL;
      it_ = begin_ = end_;
    }

    void reset(const dense_block& block) NOEXCEPT {
      value_ = block.base_;
      value_payload_ = bytes_ref::NIL;
      it_ = begin_ = std::begin(block.index_);
      end_ = block.end_;
      data_ = &block.data_;
      base_ = block.base_;
    }

    bool operator==(const dense_block& rhs) const NOEXCEPT {
      return data_ == &rhs.data_;
    }

    bool operator!=(const dense_block& rhs) const NOEXCEPT {
      return !(*this == rhs);
    }

   private:
    irs::bytes_ref value_payload_ { irs::bytes_ref::NIL };
    irs::doc_id_t value_ { irs::type_limits<irs::type_t::doc_id_t>::invalid() };

    // note that function increments 'it_'
    void next_value() NOEXCEPT {
      const auto vbegin = *it_;
      const auto vend = (++it_ == end_ ? data_->size() : *it_);

      assert(vend >= vbegin);
      value_payload_ = bytes_ref(
        data_->c_str() + vbegin, // start
        vend - vbegin // length
      );
    }

    const uint32_t* begin_{};
    const uint32_t* it_{};
    const uint32_t* end_{};
    const bstring* data_{};
    doc_id_t base_{};
  }; // iterator

  void load(index_input& in, decompressor& decomp, bstring& buf) {
    const uint32_t size = in.read_vint(); // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'dense_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, base_, avg) || 1 != avg) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_block', base_key=%du, avg_delta=%du",
        base_, avg
      ));
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
    read_compact(in, decomp, buf, data_);
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
        *begin - vbegin // length
      );

      if (!visitor(key, value)) {
        return false;
      }
    }

    // visit tail block
    assert(data_.size() >= *begin);
    value = bytes_ref(
      data_.c_str() + *begin, // start
      data_.size() - *begin // length
    );

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
    bool seek(doc_id_t doc) NOEXCEPT {
      if (doc < value_next_) {
        if (!type_limits<type_t::doc_id_t>::valid(value_)) {
          return next();
        }

        // don't seek backwards
        return true;
      }

      value_next_ = doc;
      return next();
    }

    const doc_id_t& value() const NOEXCEPT {
      return value_;
    }

    const bytes_ref& value_payload() const NOEXCEPT {
      return value_payload_;
    }

    bool next() NOEXCEPT {
      if (value_next_ >= value_end_) {
        seal();
        return false;
      }

      value_ = value_next_++;
      const auto offset = (value_ - value_min_)*avg_length_;

      value_payload_ = bytes_ref(
        data_.c_str() + offset,
        value_ == value_back_ ? data_.size() - offset : avg_length_
      );

      return true;
    }

    void seal() NOEXCEPT {
      value_ = type_limits<type_t::doc_id_t>::eof();
      value_next_ = type_limits<type_t::doc_id_t>::eof();
      value_min_ = type_limits<type_t::doc_id_t>::eof();
      value_end_ = type_limits<type_t::doc_id_t>::eof();
      value_payload_ = bytes_ref::NIL;
    }

    void reset(const dense_fixed_offset_block& block) NOEXCEPT {
      avg_length_ = block.avg_length_;
      data_ = block.data_;
      value_payload_ = bytes_ref::NIL;
      value_ = type_limits<type_t::doc_id_t>::invalid();
      value_next_ = block.base_key_;
      value_min_ = block.base_key_;
      value_end_ = value_min_ + block.size_;
      value_back_ = value_end_ - 1;
    }

    bool operator==(const dense_fixed_offset_block& rhs) const NOEXCEPT {
      return data_.c_str() == rhs.data_.c_str();
    }

    bool operator!=(const dense_fixed_offset_block& rhs) const NOEXCEPT {
      return !(*this == rhs);
    }

   private:
    uint64_t avg_length_{}; // average value length
    bytes_ref data_;
    bytes_ref value_payload_;
    doc_id_t value_ { type_limits<type_t::doc_id_t>::invalid() }; // current value
    doc_id_t value_next_{ type_limits<type_t::doc_id_t>::invalid() }; // next value
    doc_id_t value_min_{}; // min doc_id
    doc_id_t value_end_{}; // after the last valid doc id
    doc_id_t value_back_{}; // last valid doc id
  }; // iterator

  void load(index_input& in, decompressor& decomp, bstring& buf) {
    size_ = in.read_vint(); // total number of entries in a block

    if (!size_) {
      throw index_error("Empty 'dense_fixed_offset_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, base_key_, avg) || 1 != avg) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_fixed_offset_block', base_key=%du, avg_delta=%du",
        base_key_, avg
      ));
    }

    // fixed length block must be encoded with RL encoding
    if (!encode::avg::read_block_rl32(in, base_offset_, avg_length_)) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_fixed_offset_block', base_offset=%du, avg_length=%du",
        base_key_, avg_length_
      ));
    }

    // read data
    read_compact(in, decomp, buf, data_);
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
      data_.size() - offset // length
    );

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
    bool seek(doc_id_t doc) NOEXCEPT {
      it_ = std::lower_bound(begin_, end_, doc);

      return next();
    }

    const irs::doc_id_t& value() const NOEXCEPT { return value_; }

    const irs::bytes_ref& value_payload() const NOEXCEPT {
      return value_payload_;
    }

    bool next() NOEXCEPT {
      if (it_ == end_) {
        return false;
      }

      begin_ = it_;
      value_ = *it_++;
      return true;
    }

    void seal() NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::eof();
      value_payload_ = irs::bytes_ref::NIL;
      it_ = begin_ = end_;
    }

    void reset(const sparse_mask_block& block) NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      value_payload_ = irs::bytes_ref::NIL;
      it_ = begin_ = std::begin(block.keys_);
      end_ = begin_ + block.size_;

      assert(std::is_sorted(begin_, end_));
    }

    bool operator==(const sparse_mask_block& rhs) const NOEXCEPT {
      return end_ == (rhs.keys_ + rhs.size_);
    }

    bool operator!=(const sparse_mask_block& rhs) const NOEXCEPT {
      return !(*this == rhs);
    }

   private:
    irs::bytes_ref value_payload_ { irs::bytes_ref::NIL };
    irs::doc_id_t value_ { irs::type_limits<irs::type_t::doc_id_t>::invalid() };
    const doc_id_t* it_{};
    const doc_id_t* begin_{};
    const doc_id_t* end_{};
  }; // iterator

  sparse_mask_block() NOEXCEPT {
    std::fill(
      std::begin(keys_), std::end(keys_),
      type_limits<type_t::doc_id_t>::eof()
    );
  }

  void load(index_input& in, decompressor& /*decomp*/, bstring& buf) {
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
      std::begin(keys_), std::end(keys_), key
    );

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
    bool seek(doc_id_t doc) NOEXCEPT {
      if (doc < doc_) {
        if (!type_limits<type_t::doc_id_t>::valid(value_)) {
          return next();
        }

        // don't seek backwards
        return true;
      }

      doc_ = doc;
      return next();
    }

    const irs::doc_id_t& value() const NOEXCEPT {
      return value_;
    }

    const irs::bytes_ref& value_payload() const NOEXCEPT {
      return irs::bytes_ref::NIL;
    }

    bool next() NOEXCEPT {
      if (doc_ >= max_) {
        seal();
        return false;
      }

      value_ = doc_++;

      return true;
    }

    void seal() NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::eof();
      doc_ = max_;
    }

    void reset(const dense_mask_block& block) NOEXCEPT {
      block_ = &block;
      value_ = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      doc_ = block.min_;
      max_ = block.max_;
    }

    bool operator==(const dense_mask_block& rhs) const NOEXCEPT {
      return block_ == &rhs;
    }

    bool operator!=(const dense_mask_block& rhs) const NOEXCEPT {
      return !(*this == rhs);
    }

   private:
    const dense_mask_block* block_{};
    doc_id_t value_{ irs::type_limits<irs::type_t::doc_id_t>::invalid() };
    doc_id_t doc_{ irs::type_limits<irs::type_t::doc_id_t>::invalid() };
    doc_id_t max_{ irs::type_limits<irs::type_t::doc_id_t>::invalid() };
  }; // iterator

  dense_mask_block() NOEXCEPT
    : min_(type_limits<type_t::doc_id_t>::invalid()),
      max_(type_limits<type_t::doc_id_t>::invalid()) {
  }

  void load(index_input& in, decompressor& /*decomp*/, bstring& /*buf*/) {
    const auto size = in.read_vint(); // total number of entries in a block

    if (!size) {
      throw index_error("Empty 'dense_mask_block' found in columnstore");
    }

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint32_t avg;
    if (!encode::avg::read_block_rl32(in, min_, avg) || 1 != avg) {
      throw index_error(string_utils::to_string(
        "Invalid RL encoding in 'dense_mask_block', base_key=%du, avg_delta=%du",
        min_, avg
      ));
    }

    // mask block has no data, so all offsets should be equal to 0
    if (!encode::avg::check_block_rl64(in, 0)) {
      throw index_error("'dense_mask_block' expected to contain no data");
    }

    max_ = min_ + size;
  }

  bool value(doc_id_t key, bytes_ref& /*reader*/) const NOEXCEPT {
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
  DECLARE_SHARED_PTR(read_context);

  static ptr make(const index_input& stream) {
    auto clone = stream.reopen(); // reopen thead-safe stream

    if (!clone) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen columpstore input in: %s", __FUNCTION__);

      throw io_error("Failed to reopen columnstore input in");
    }

    return memory::make_shared<read_context>(std::move(clone));
  }

  read_context(index_input::ptr&& in = index_input::ptr(), const Allocator& alloc = Allocator())
    : block_cache_traits<sparse_block, Allocator>::cache_t(typename block_cache_traits<sparse_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_block, Allocator>::cache_t(typename block_cache_traits<dense_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_fixed_offset_block, Allocator>::cache_t(typename block_cache_traits<dense_fixed_offset_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<sparse_mask_block, Allocator>::cache_t(typename block_cache_traits<sparse_mask_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_mask_block, Allocator>::cache_t(typename block_cache_traits<dense_mask_block, Allocator>::allocator_t(alloc)),
      buf_(INDEX_BLOCK_SIZE*sizeof(uint32_t), 0),
      stream_(std::move(in)) {
  }

  template<typename Block, typename... Args>
  Block& emplace_back(uint64_t offset, Args&&... args) {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;

    // add cache entry
    auto& block = cache.emplace_back(std::forward<Args>(args)...);

    try {
      load(block, offset);
    } catch (...) {
      // failed to load block
      pop_back<Block>();

      throw;
    }

    return block;
  }

  template<typename Block>
  void load(Block& block, uint64_t offset) {
    stream_->seek(offset); // seek to the offset
    block.load(*stream_, decomp_, buf_);
  }

  template<typename Block>
  void pop_back() NOEXCEPT {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;
    cache.pop_back();
  }

 private:
  decompressor decomp_; // decompressor
  bstring buf_; // temporary buffer for decoding/unpacking
  index_input::ptr stream_;
}; // read_context

typedef read_context<> read_context_t;

class context_provider: private util::noncopyable {
 public:
  context_provider(size_t max_pool_size)
    : pool_(std::max(size_t(1), max_pool_size)) {
  }

  void prepare(index_input::ptr&& stream) NOEXCEPT {
    stream_ = std::move(stream);
  }

  bounded_object_pool<read_context_t>::ptr get_context() const {
    return pool_.emplace(*stream_);
  }

 private:
  mutable bounded_object_pool<read_context_t> pool_;
  index_input::ptr stream_;
}; // context_provider

// in case of success caches block pointed
// instance, nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t& load_block(
    const context_provider& ctxs,
    BlockRef& ref) {
  typedef typename BlockRef::block_t block_t;

  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();
    assert(ctx);

    // load block
    const auto& block = ctx->template emplace_back<block_t>(ref.offset);

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
    const BlockRef& ref,
    typename BlockRef::block_t& block) {
  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();
    assert(ctx);

    ctx->load(block, ref.offset);

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
  DECLARE_UNIQUE_PTR(column);

  explicit column(ColumnProperty props) NOEXCEPT
    : props_(props) {
  }

  virtual ~column() { }

  virtual void read(data_input& in, uint64_t* /*buf*/) {
    count_ = in.read_vint();
    max_ = in.read_vint();
    avg_block_size_ = in.read_vint();
    avg_block_count_ = in.read_vint();
    if (!avg_block_count_) {
      avg_block_count_ = count_;
    }
  }

  doc_id_t max() const NOEXCEPT { return max_; }
  virtual size_t size() const NOEXCEPT override { return count_; }
  bool empty() const NOEXCEPT { return 0 == size(); }
  uint32_t avg_block_size() const NOEXCEPT { return avg_block_size_; }
  uint32_t avg_block_count() const NOEXCEPT { return avg_block_count_; }
  ColumnProperty props() const NOEXCEPT { return props_; }

 protected:
  // same as size() but returns uint32_t to avoid type convertions
  uint32_t count() const NOEXCEPT { return count_; }

 private:
  doc_id_t max_{ type_limits<type_t::doc_id_t>::eof() };
  uint32_t count_{};
  uint32_t avg_block_size_{};
  uint32_t avg_block_count_{};
  ColumnProperty props_{ CP_SPARSE };
}; // column

template<typename Column>
class column_iterator final: public irs::doc_iterator {
 public:
  typedef Column column_t;
  typedef typename Column::block_t block_t;
  typedef typename block_t::iterator block_iterator_t;

  explicit column_iterator(
      const column_t& column,
      const typename column_t::block_ref* begin,
      const typename column_t::block_ref* end
  ): attrs_(1), // payload_iterator
     begin_(begin),
     seek_origin_(begin),
     end_(end),
     column_(&column) {
    attrs_.emplace(payload_);
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

  virtual doc_id_t value() const NOEXCEPT override {
    return block_.value();
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

    return value();
  }

  virtual bool next() override {
    while (!block_.next()) {
      if (!next_block()) {
        return false;
      }
    }

    return true;
  }

 private:
  typedef typename column_t::refs_t refs_t;

  struct payload_iterator: public irs::payload_iterator {
    const irs::bytes_ref* value_{ nullptr };
    virtual bool next() override { return nullptr != value_; }
    virtual const irs::bytes_ref& value() const override {
      return value_ ? *value_ : irs::bytes_ref::NIL;
    }
  };

  bool next_block() {
    if (begin_ == end_) {
      // reached the end of the column
      block_.seal();
      seek_origin_ = end_;
      payload_.value_ = nullptr;

      return false;
    }

    try {
      const auto& cached = load_block(*column_->ctxs_, *begin_);

      if (block_ != cached) {
        block_.reset(cached);
        payload_.value_ = &(block_.value_payload());
      }
    } catch (...) {
      // unable to load block, seal the iterator
      block_.seal();
      begin_ = end_;
      payload_.value_ = nullptr;

      throw;
    }

    seek_origin_ = begin_++;

    return true;
  }

  irs::attribute_view attrs_;
  block_iterator_t block_;
  payload_iterator payload_;
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

  virtual void read(data_input& in, uint64_t* buf) override {
    column::read(in, buf); // read common header

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
    if (this->max() < type_limits<type_t::doc_id_t>::eof()) {
      begin->key = this->max() + 1;
    } else {
      begin->key = type_limits<type_t::doc_id_t>::eof();
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

    const auto& cached = load_block(*ctxs_, *it);

    return cached.value(key, value);
  };

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor
  ) const override {
    block_t block; // don't cache new blocks
    for (auto begin = refs_.begin(), end = refs_.end()-1; begin != end; ++begin) { // -1 for upper bound
      const auto& cached = load_block(*ctxs_, *begin, block);

      if (!cached.visit(visitor)) {
        return false;
      }
    }
    return true;
  }

  virtual irs::doc_iterator::ptr iterator() const override {
    typedef column_iterator<column_t> iterator_t;

    return empty()
      ? irs::doc_iterator::empty()
      : irs::doc_iterator::make<iterator_t>(
          *this,
          refs_.data(),
          refs_.data() + refs_.size() - 1 // -1 for upper bound
        );
  }

  virtual columnstore_reader::values_reader_f values() const override {
    return column_values<column_t>(*this);
  }

 private:
  friend class column_iterator<column_t>;

  struct block_ref : util::noncopyable {
    typedef typename column_t::block_t block_t;

    block_ref() = default;

    block_ref(block_ref&& other) NOEXCEPT
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
      doc_id_t key) const NOEXCEPT {
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

  typename refs_t::const_iterator find_block(doc_id_t key) const NOEXCEPT {
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

  virtual void read(data_input& in, uint64_t* buf) override {
    column::read(in, buf); // read common header

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

    const auto& cached = load_block(*ctxs_, ref);

    return cached.value(key, value);
  }

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor
  ) const override {
    block_t block; // don't cache new blocks
    for (auto& ref : refs_) {
      const auto& cached = load_block(*ctxs_, ref, block);

      if (!cached.visit(visitor)) {
        return false;
      }
    }

    return true;
  }

  virtual irs::doc_iterator::ptr iterator() const override {
    typedef column_iterator<column_t> iterator_t;

    return empty()
      ? irs::doc_iterator::empty()
      : irs::doc_iterator::make<iterator_t>(
          *this,
          refs_.data(),
          refs_.data() + refs_.size()
        )
      ;
  }

  virtual columnstore_reader::values_reader_f values() const override {
    return column_values<column_t>(*this);
  }

 private:
  friend class column_iterator<column_t>;

  struct block_ref {
    typedef typename column_t::block_t block_t;

    block_ref() = default;

    block_ref(block_ref&& other) NOEXCEPT
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
      doc_id_t key) const NOEXCEPT {
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

  typename refs_t::const_iterator find_block(doc_id_t key) const NOEXCEPT {
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

  explicit dense_fixed_offset_column(ColumnProperty prop) NOEXCEPT
    : column(prop) {
  }

  virtual void read(data_input& in, uint64_t* buf) override {
    // we treat data in blocks as "garbage" which could be
    // potentially removed on merge, so we don't validate
    // column properties using such blocks

    column::read(in, buf); // read common header

    uint32_t blocks_count = in.read_vint(); // total number of column index blocks

    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl32(in, this->avg_block_count())) {
        throw index_error("Invalid RL encoding in 'dense_fixed_offset_column<dense_mask_block>' (keys)");
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, buf,
        [](uint64_t) {}
      );

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
        [](uint64_t) { }
      );
    }


    min_ = this->max() - this->count();
  }

  bool value(doc_id_t key, bytes_ref& value) const NOEXCEPT {
    value = bytes_ref::NIL;
    return key > min_ && key <= this->max();
  }

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor
  ) const override {
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
  class column_iterator final: public doc_iterator {
   public:
    explicit column_iterator(const column_t& column) NOEXCEPT
      : min_(1 + column.min_), max_(column.max()) {
    }

    virtual const irs::attribute_view& attributes() const NOEXCEPT override {
      return irs::attribute_view::empty_instance();
    }

    virtual irs::doc_id_t value() const NOEXCEPT override {
      return value_;
    }

    virtual irs::doc_id_t seek(irs::doc_id_t doc) NOEXCEPT override {
      if (doc < min_) {
        if (!type_limits<type_t::doc_id_t>::valid(value_)) {
          next();
        }

        return value();
      }

      min_ = doc;
      next();

      return value();
    }

    virtual bool next() NOEXCEPT override {
      if (min_ > max_) {
        value_ = type_limits<type_t::doc_id_t>::eof();

        return false;
      }


      value_ = min_++;

      return true;
    }

   private:
    irs::doc_id_t value_ { irs::type_limits<irs::type_t::doc_id_t>::invalid() };
    doc_id_t min_{ type_limits<type_t::doc_id_t>::invalid() };
    doc_id_t max_{ type_limits<type_t::doc_id_t>::invalid() };
  }; // column_iterator

  doc_id_t min_{}; // min key (less than any key in column)
}; // dense_fixed_offset_column

irs::doc_iterator::ptr dense_fixed_offset_column<dense_mask_block>::iterator() const {
  return empty()
    ? irs::doc_iterator::empty()
    : irs::doc_iterator::make<column_iterator>(*this);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                 column factories
// ----------------------------------------------------------------------------

typedef std::function<
  column::ptr(const context_provider& ctxs, ColumnProperty prop)
> column_factory_f;
                                                               //  Column  |          Blocks
const column_factory_f g_column_factories[] {                  // CP_DENSE | CP_MASK CP_FIXED CP_DENSE
  &sparse_column<sparse_block>::make,                          //    0     |    0        0        0
  &sparse_column<dense_block>::make,                           //    0     |    0        0        1
  &sparse_column<sparse_block>::make,                          //    0     |    0        1        0
  &sparse_column<dense_fixed_offset_block>::make,              //    0     |    0        1        1
  nullptr, /* invalid properties, should never happen */       //    0     |    1        0        0
  nullptr, /* invalid properties, should never happen */       //    0     |    1        0        1
  &sparse_column<sparse_mask_block>::make,                     //    0     |    1        1        0
  &sparse_column<dense_mask_block>::make,                      //    0     |    1        1        1

  &sparse_column<sparse_block>::make,                          //    1     |    0        0        0
  &sparse_column<dense_block>::make,                           //    1     |    0        0        1
  &sparse_column<sparse_block>::make,                          //    1     |    0        1        0
  &dense_fixed_offset_column<dense_fixed_offset_block>::make,  //    1     |    0        1        1
  nullptr, /* invalid properties, should never happen */       //    1     |    1        0        0
  nullptr, /* invalid properties, should never happen */       //    1     |    1        0        1
  &sparse_column<sparse_mask_block>::make,                     //    1     |    1        1        0
  &dense_fixed_offset_column<dense_mask_block>::make           //    1     |    1        1        1
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
    const segment_meta& meta
  ) override;

  virtual const column_reader* column(field_id field) const override;

  virtual size_t size() const NOEXCEPT override {
    return columns_.size();
  }

 private:
  std::vector<column::ptr> columns_;
}; // reader

bool reader::prepare(
    const directory& dir,
    const segment_meta& meta
) {
  const auto filename = file_name<columnstore_writer>(meta);
  bool exists;

  if (!dir.exists(exists, filename)) {
    throw io_error(string_utils::to_string(
      "failed to check existence of file, path: %s",
      filename.c_str()
    ));
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
      filename.c_str()
    ));
  }

  // check header
  format_utils::check_header(
    *stream,
    writer::FORMAT_NAME,
    writer::FORMAT_MIN,
    writer::FORMAT_MAX
  );

  // since columns data are too large
  // it is too costly to verify checksum of
  // the entire file. here we perform cheap
  // error detection which could recognize
  // some forms of corruption. */
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

    if (props >= IRESEARCH_COUNTOF(g_column_factories)) {
      throw index_error(string_utils::to_string(
        "Failed to load column id=" IR_SIZE_T_SPECIFIER ", got invalid properties=%d",
        i, static_cast<uint32_t>(props)
      ));
    }

    // create column
    const auto& factory = g_column_factories[props];

    if (!factory) {
      static_assert(
        std::is_same<std::underlying_type<ColumnProperty>::type, uint32_t>::value,
        "Enum 'ColumnProperty' has different underlying type"
      );

      throw index_error(string_utils::to_string(
        "Failed to open column id=" IR_SIZE_T_SPECIFIER ", properties=%d",
        i, static_cast<uint32_t>(props)
      ));
    }

    auto column = factory(*this, props);

    if (!column) {
      throw index_error(string_utils::to_string(
        "Factory failed to create column id=" IR_SIZE_T_SPECIFIER, i
      ));
    }

    try {
      column->read(*stream, buf);
    } catch (...) {
      IR_FRMT_ERROR("Failed to load column id=" IR_SIZE_T_SPECIFIER, i);

      throw;
    }

    // noexcept since space has been already reserved
    columns.emplace_back(std::move(column));
  }

  // noexcept
  context_provider::prepare(std::move(stream));
  columns_ = std::move(columns);

  return true;
}

const reader::column_reader* reader::column(field_id field) const {
  return field >= columns_.size()
    ? nullptr // can't find column with the specified identifier
    : columns_[field].get();
}

NS_END // columns

// ----------------------------------------------------------------------------
// --SECTION--                                                  postings_reader
// ----------------------------------------------------------------------------

class postings_reader final: public irs::postings_reader {
 public:
  virtual void prepare(
    index_input& in,
    const reader_state& state,
    const flags& features
  ) override;

  virtual void decode(
    data_input& in,
    const flags& field,
    const attribute_view& attrs,
    irs::term_meta& state
  ) override;

  virtual irs::doc_iterator::ptr iterator(
    const flags& field,
    const attribute_view& attrs,
    const flags& features
  ) override;

 private:
  index_input::ptr doc_in_;
  index_input::ptr pos_in_;
  index_input::ptr pay_in_;
}; // postings_reader

void postings_reader::prepare(
    index_input& in,
    const reader_state& state,
    const flags& features) {
  std::string buf;

  // prepare document input
  prepare_input(
    buf, doc_in_, irs::IOAdvice::RANDOM, state,
    postings_writer::DOC_EXT,
    postings_writer::DOC_FORMAT_NAME,
    postings_writer::FORMAT_MIN,
    postings_writer::FORMAT_MAX
  );

  // Since terms doc postings too large
  //  it is too costly to verify checksum of
  //  the entire file. Here we perform cheap
  //  error detection which could recognize
  //  some forms of corruption.
  format_utils::read_checksum(*doc_in_);

  if (features.check<position>()) {
    /* prepare positions input */
    prepare_input(
      buf, pos_in_, irs::IOAdvice::RANDOM, state,
      postings_writer::POS_EXT,
      postings_writer::POS_FORMAT_NAME,
      postings_writer::FORMAT_MIN,
      postings_writer::FORMAT_MAX
    );

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
        postings_writer::PAY_EXT,
        postings_writer::PAY_FORMAT_NAME,
        postings_writer::FORMAT_MIN,
        postings_writer::FORMAT_MAX
      );

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
    postings_writer::TERMS_FORMAT_NAME,
    postings_writer::TERMS_FORMAT_MIN,
    postings_writer::TERMS_FORMAT_MAX
  );

  const uint64_t block_size = in.read_vint();

  if (block_size != postings_writer::BLOCK_SIZE) {
    throw index_error(string_utils::to_string(
      "while preparing postings_reader, error: invalid block size '" IR_UINT64_T_SPECIFIER "'",
      block_size
    ));
  }
}

void postings_reader::decode(
    data_input& in,
    const flags& meta,
    const attribute_view& attrs,
    irs::term_meta& state
) {
#ifdef IRESEARCH_DEBUG
  auto& term_meta = dynamic_cast<version10::term_meta&>(state);
#else
  auto& term_meta = static_cast<version10::term_meta&>(state);
#endif // IRESEARCH_DEBUG

  auto& term_freq = attrs.get<frequency>();

  term_meta.docs_count = in.read_vint();
  if (term_freq) {
    term_freq->value = term_meta.docs_count + in.read_vint();
  }

  term_meta.doc_start += in.read_vlong();
  if (term_freq && term_freq->value && meta.check<position>()) {
    term_meta.pos_start += in.read_vlong();

    term_meta.pos_end = term_freq->value > postings_writer::BLOCK_SIZE
        ? in.read_vlong()
        : type_limits<type_t::address_t>::invalid();

    if (meta.check<payload>() || meta.check<offset>()) {
      term_meta.pay_start += in.read_vlong();
    }
  }

  if (1U == term_meta.docs_count || term_meta.docs_count > postings_writer::BLOCK_SIZE) {
    term_meta.e_skip_start = in.read_vlong();
  }
}

#if defined(_MSC_VER)
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wswitch"
#endif

irs::doc_iterator::ptr postings_reader::iterator(
    const flags& field,
    const attribute_view& attrs,
    const flags& req
) {
  // compile field features
  const auto features = ::features(field);
  // get enabled features:
  // find intersection between requested and available features
  const auto enabled = features & req;
  doc_iterator::ptr it;

  // MSVC 2013 doesn't support constexpr, can't use
  // 'operator|' in the following switch statement
  switch (enabled) {
   case features::FREQ_POS_OFFS_PAY:
    it = std::make_shared<pos_doc_iterator<offs_pay_iterator>>();
    break;
   case features::FREQ_POS_OFFS:
    it = std::make_shared<pos_doc_iterator<offs_iterator>>();
    break;
   case features::FREQ_POS_PAY:
    it = std::make_shared<pos_doc_iterator<pay_iterator>>();
    break;
   case features::FREQ_POS:
    it = std::make_shared<pos_doc_iterator<pos_iterator>>();
    break;
   default:
    it = std::make_shared<doc_iterator>();
  }

  it->prepare(
    features, enabled, attrs,
    doc_in_.get(), pos_in_.get(), pay_in_.get()
  );

  return it;
}

#if defined(_MSC_VER)
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

// actual implementation
class format : public irs::version10::format {
 public:
  DECLARE_FORMAT_TYPE();
  DECLARE_FACTORY();

  format() NOEXCEPT;

  virtual index_meta_writer::ptr get_index_meta_writer() const override final;
  virtual index_meta_reader::ptr get_index_meta_reader() const override final;

  virtual segment_meta_writer::ptr get_segment_meta_writer() const override final;
  virtual segment_meta_reader::ptr get_segment_meta_reader() const override final;

  virtual document_mask_writer::ptr get_document_mask_writer() const override final;
  virtual document_mask_reader::ptr get_document_mask_reader() const override final;

  virtual field_writer::ptr get_field_writer(bool volatile_state) const override final;
  virtual field_reader::ptr get_field_reader() const override final;

  virtual column_meta_writer::ptr get_column_meta_writer() const override final;
  virtual column_meta_reader::ptr get_column_meta_reader() const override final;

  virtual columnstore_writer::ptr get_columnstore_writer() const override final;
  virtual columnstore_reader::ptr get_columnstore_reader() const override final;

  virtual postings_writer::ptr get_postings_writer(bool volatile_state) const override;
  virtual postings_reader::ptr get_postings_reader() const override;
};

format::format() NOEXCEPT : irs::version10::format(format::type()) {}

index_meta_writer::ptr format::get_index_meta_writer() const  {
  return irs::index_meta_writer::make<::index_meta_writer>();
}

index_meta_reader::ptr format::get_index_meta_reader() const {
  // can reuse stateless reader
  static ::index_meta_reader INSTANCE;

  return memory::make_managed<irs::index_meta_reader, false>(&INSTANCE);
}

segment_meta_writer::ptr format::get_segment_meta_writer() const {
  // can reuse stateless writer
  static ::segment_meta_writer INSTANCE;

  return memory::make_managed<irs::segment_meta_writer, false>(&INSTANCE);
}

segment_meta_reader::ptr format::get_segment_meta_reader() const {
  // can reuse stateless writer
  static ::segment_meta_reader INSTANCE;

  return memory::make_managed<irs::segment_meta_reader, false>(&INSTANCE);
}

document_mask_writer::ptr format::get_document_mask_writer() const {
  // can reuse stateless writer
  static ::document_mask_writer INSTANCE;

  return memory::make_managed<irs::document_mask_writer, false>(&INSTANCE);
}

document_mask_reader::ptr format::get_document_mask_reader() const {
  // can reuse stateless writer
  static ::document_mask_reader INSTANCE;

  return memory::make_managed<irs::document_mask_reader, false>(&INSTANCE);
}

field_writer::ptr format::get_field_writer(bool volatile_state) const {
  return irs::field_writer::make<burst_trie::field_writer>(
    get_postings_writer(volatile_state),
    volatile_state
  );
}

field_reader::ptr format::get_field_reader() const  {
  return irs::field_reader::make<burst_trie::field_reader>(
    get_postings_reader()
  );
}

column_meta_writer::ptr format::get_column_meta_writer() const {
  return memory::make_unique<columns::meta_writer>();
}

column_meta_reader::ptr format::get_column_meta_reader() const {
  return memory::make_unique<columns::meta_reader>();
}

columnstore_writer::ptr format::get_columnstore_writer() const {
  return memory::make_unique<columns::writer>();
}

columnstore_reader::ptr format::get_columnstore_reader() const {
  return memory::make_unique<columns::reader>();
}

irs::postings_writer::ptr format::get_postings_writer(bool volatile_state) const {
  return irs::postings_writer::make<::postings_writer>(volatile_state);
}

irs::postings_reader::ptr format::get_postings_reader() const {
  return irs::postings_reader::make<::postings_reader>();
}

/*static*/ irs::format::ptr format::make() {
  static const ::format INSTANCE;

  // aliasing constructor
  return irs::format::ptr(irs::format::ptr(), &INSTANCE);
}

DEFINE_FORMAT_TYPE_NAMED(::format, format_traits::NAME);
REGISTER_FORMAT(::format);

NS_END

NS_ROOT
NS_BEGIN(version10)

void init() {
#ifndef IRESEARCH_DLL
  REGISTER_FORMAT(::format);
#endif
}

// ----------------------------------------------------------------------------
// --SECTION--                                                           format
// ----------------------------------------------------------------------------

format::format(const irs::format::type_id& type) NOEXCEPT
  : irs::format(type) {
}

NS_END // version10
NS_END // root

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
