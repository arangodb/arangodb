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

#include "formats_10.hpp"
#include "formats_burst_trie.hpp"
#include "format_utils.hpp"

#include "analysis/token_attributes.hpp"

#include "index/field_meta.hpp"
#include "index/index_meta.hpp"
#include "index/file_names.hpp"
#include "index/index_reader.hpp"

#include "store/store_utils.hpp"

#include "utils/bit_utils.hpp"
#include "utils/log.hpp"
#include "utils/timer_utils.hpp"
#include "utils/std.hpp"
#include "utils/bit_packing.hpp"
#include "utils/type_limits.hpp"
#include "utils/object_pool.hpp"
#include "formats.hpp"

#include <array>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <type_traits>
#include <deque>

NS_LOCAL

irs::bytes_ref DUMMY; // placeholder for visiting logic in columnstore

NS_END

NS_ROOT
NS_BEGIN(version10)

// ----------------------------------------------------------------------------
// --SECTION--                                             forward declarations
// ----------------------------------------------------------------------------

template<typename T, typename M>
std::string file_name(const M& meta);

// ----------------------------------------------------------------------------
// --SECTION--                                                 helper functions 
// ----------------------------------------------------------------------------

NS_BEGIN(detail)
NS_LOCAL

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
    std::stringstream ss;

    ss << "Failed to create file, path: " << str;

    throw detailed_io_error(ss.str());
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
    std::stringstream ss;

    ss << "Failed to open file, path: " << str;

    throw detailed_io_error(ss.str());
  }

  format_utils::check_header(*in, format, min_ver, max_ver);
}

FORCE_INLINE void skip_positions(index_input& in) {
  encode::bitpack::skip_block32(in, postings_writer::BLOCK_SIZE);
}

FORCE_INLINE void skip_payload(index_input& in) {
  const size_t size = in.read_vint();
  if (size) {
    encode::bitpack::skip_block32(in, postings_writer::BLOCK_SIZE);
    in.seek(in.file_pointer() + size);
  }
}

FORCE_INLINE void skip_offsets(index_input& in) {
  encode::bitpack::skip_block32(in, postings_writer::BLOCK_SIZE);
  encode::bitpack::skip_block32(in, postings_writer::BLOCK_SIZE);
}

NS_END // NS_LOCAL

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
  uint64_t* freq;
  uint64_t* enc_buf;
  uint64_t tail_start;
  size_t tail_length;
  version10::features features;
}; // doc_state

///////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator
///////////////////////////////////////////////////////////////////////////////
class doc_iterator : public iresearch::doc_iterator {
 public:
  DECLARE_PTR(doc_iterator);

  DECLARE_FACTORY(doc_iterator);

  doc_iterator() NOEXCEPT
    : skip_levels_(1),
      skip_(postings_writer::BLOCK_SIZE, postings_writer::SKIP_N) {
    std::fill(docs_, docs_ + postings_writer::BLOCK_SIZE, type_limits<type_t::doc_id_t>::invalid());
  }

  void prepare(
      const version10::features& field,
      const version10::features& enabled,
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
        doc_in_ = doc_in->reopen();

        if (!doc_in_) {
          IR_FRMT_FATAL("Failed to reopen document input in: %s", __FUNCTION__);

          throw detailed_io_error("Failed to reopen document input");
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
    iresearch::seek(*this, target);
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
      const version10::features& enabled,
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
    state.doc = in.read_vlong();
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

  void read_end_block(uint64_t size) {
    if (features_.freq()) {
      for (uint64_t i = 0; i < size; ++i) {
        if (shift_unpack_64(doc_in_->read_vlong(), docs_[i])) {
          doc_freqs_[i] = 1;
        } else {
          doc_freqs_[i] = doc_in_->read_vlong();
        }
      }
    } else {
      for (uint64_t i = 0; i < size; ++i) {
        docs_[i] = doc_in_->read_vlong();
      }
    }
  }

  void refill() {
    const auto left = term_state_.docs_count - cur_pos_;

    if (left >= postings_writer::BLOCK_SIZE) {
      // read doc deltas
      encode::bitpack::read_block(*doc_in_, postings_writer::BLOCK_SIZE, enc_buf_, docs_);

      if (features_.freq()) {
        // read frequency it is required by
        // the iterator or just skip it otherwise
        if (enabled_.freq()) {
          encode::bitpack::read_block(
            *doc_in_,
            postings_writer::BLOCK_SIZE,
            enc_buf_,
            doc_freqs_
          );
        } else {
          encode::bitpack::skip_block32(
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
  uint64_t enc_buf_[postings_writer::BLOCK_SIZE]; // buffer for encoding
  doc_id_t docs_[postings_writer::BLOCK_SIZE]; // doc values
  uint64_t doc_freqs_[postings_writer::BLOCK_SIZE]; // document frequencies
  uint64_t cur_pos_{};
  const doc_id_t* begin_{docs_};
  doc_id_t* end_{docs_};
  uint64_t* doc_freq_{}; // pointer into docs_ to the frequency attribute value for the current doc
  uint64_t term_freq_{}; // total term frequency
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
      index_input::ptr skip_in = doc_in_->dup();
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
    "DocIterator must be derived from iresearch::doc_iterator"
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
class pos_iterator : public position::impl {
 public:
  DECLARE_PTR(pos_iterator);

  pos_iterator() = default;

  pos_iterator(size_t reserve_attrs): position::impl(reserve_attrs) {
  }

  virtual void clear() override {
    value_ = position::INVALID;
  }

  virtual bool next() override {
    if (0 == pend_pos_) {
      value_ = position::NO_MORE;
      return false;
    }

    const uint64_t freq = *freq_;
    if (pend_pos_ > freq) {
      skip(pend_pos_ - freq);
      pend_pos_ = freq;
    }

    if (buf_pos_ == postings_writer::BLOCK_SIZE) {
      refill();
      buf_pos_ = 0;
    }

    // TODO: make INVALID = 0, remove this
    if (value_ == position::INVALID) {
      value_ = 0;
    } 

    value_ += pos_deltas_[buf_pos_];
    read_attributes();
    ++buf_pos_;
    --pend_pos_;
    return true;
  }

  virtual uint32_t value() const override { return value_; }

 protected:
  // prepares iterator to work
  virtual void prepare(const doc_state& state) {
    pos_in_ = state.pos_in->reopen();

    if (!pos_in_) {
      IR_FRMT_FATAL("Failed to reopen positions input in: %s", __FUNCTION__);

      throw detailed_io_error("Failed to reopen positions input");
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
      encode::bitpack::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);
    }
  }

  virtual void skip(uint64_t count) {
    uint64_t left = postings_writer::BLOCK_SIZE - buf_pos_;
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
      buf_pos_ += uint32_t(count);
    }
    clear();
    value_ = 0;
  }

  uint32_t pos_deltas_[postings_writer::BLOCK_SIZE]; /* buffer to store position deltas */
  const uint64_t* freq_; /* lenght of the posting list for a document */
  uint32_t* enc_buf_; /* auxillary buffer to decode data */
  uint64_t pend_pos_{}; /* how many positions "behind" we are */
  uint64_t tail_start_; /* file pointer where the last (vInt encoded) pos delta block is */
  size_t tail_length_; /* number of positions in the last (vInt encoded) pos delta block */
  uint32_t value_{ position::INVALID }; /* current position */
  uint32_t buf_pos_{ postings_writer::BLOCK_SIZE } ; /* current position in pos_deltas_ buffer */
  index_input::ptr pos_in_;
  features features_; /* field features */
 
 private:
  friend class pos_doc_iterator;

  static pos_iterator::ptr make(const features& enabled);
}; // pos_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class offs_pay_iterator
///////////////////////////////////////////////////////////////////////////////
class offs_pay_iterator final : public pos_iterator {
 public:
  DECLARE_PTR(offs_pay_iterator);

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

 protected:
  virtual void prepare(const doc_state& state) override {
    pos_iterator::prepare(state);
    pay_in_ = state.pay_in->reopen();

    if (!pay_in_) {
      IR_FRMT_FATAL("Failed to reopen payload input in: %s", __FUNCTION__);

      throw detailed_io_error("Failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  virtual void prepare(const skip_state& state) override {
    pos_iterator::prepare(state);
    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

  virtual void read_attributes() override {
    offs_.start += offs_start_deltas_[buf_pos_];
    offs_.end = offs_.start + offs_lengts_[buf_pos_];

    pay_.value = bytes_ref(
      pay_data_.c_str() + pay_data_pos_,
      pay_lengths_[buf_pos_]);
    pay_data_pos_ += pay_lengths_[buf_pos_];
  }

  virtual void skip(uint64_t count) override {
    uint64_t left = postings_writer::BLOCK_SIZE - buf_pos_;
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
      buf_pos_ += uint32_t(count);
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

          oversize(pay_data_, pos + size);

          #ifdef IRESEARCH_DEBUG
            const auto read = pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
            assert(read == size);
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
      encode::bitpack::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);

      // read payloads
      const uint32_t size = pay_in_->read_vint();
      if (size) {
        encode::bitpack::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, pay_lengths_);
        oversize(pay_data_, size);

        #ifdef IRESEARCH_DEBUG
          const auto read = pay_in_->read_bytes(&(pay_data_[0]), size);
          assert(read == size);
        #else
          pay_in_->read_bytes(&(pay_data_[0]), size);
        #endif // IRESEARCH_DEBUG
      }

      // read offsets
      encode::bitpack::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_start_deltas_);
      encode::bitpack::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_lengts_);
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
  DECLARE_PTR(offs_iterator);

  offs_iterator()
    : pos_iterator(1) { // offset
    attrs_.emplace(offs_);
  }

  virtual void clear() override {
    pos_iterator::clear();
    offs_.clear();
  }

 protected:
  virtual void prepare(const doc_state& state) override {
    pos_iterator::prepare(state);
    pay_in_ = state.pay_in->reopen();

    if (!pay_in_) {
      IR_FRMT_FATAL("Failed to reopen payload input in: %s", __FUNCTION__);

      throw detailed_io_error("Failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  virtual void prepare(const skip_state& state) override {
    pos_iterator::prepare(state);
    pay_in_->seek(state.pay_ptr);
  }

  virtual void read_attributes() override {
    offs_.start += offs_start_deltas_[buf_pos_];
    offs_.end = offs_.start + offs_lengts_[buf_pos_];
  }

  virtual void refill() override {
    if (pos_in_->file_pointer() == tail_start_) {
      uint32_t pay_size = 0;
      for (uint64_t i = 0; i < tail_length_; ++i) {
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
      encode::bitpack::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);

      // skip payload
      if (features_.payload()) {
        skip_payload(*pay_in_);
      }

      // read offsets
      encode::bitpack::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_start_deltas_);
      encode::bitpack::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, offs_lengts_);
    }
  }

  virtual void skip(uint64_t count) override {
    uint64_t left = postings_writer::BLOCK_SIZE - buf_pos_;
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
      buf_pos_ += uint32_t(count);
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
  DECLARE_PTR(pay_iterator);

  pay_iterator()
    : pos_iterator(1) { // payload
    attrs_.emplace(pay_);
  }

  virtual void clear() override {
    pos_iterator::clear();
    pay_.clear();
  }

 protected:
  virtual void prepare(const doc_state& state) override {
    pos_iterator::prepare(state);
    pay_in_ = state.pay_in->reopen();

    if (!pay_in_) {
      IR_FRMT_FATAL("Failed to reopen payload input in: %s", __FUNCTION__);

      throw detailed_io_error("Failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  virtual void prepare(const skip_state& state) override {
    pos_iterator::prepare(state);
    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

  virtual void read_attributes() override {
    pay_.value = bytes_ref(
      pay_data_.data() + pay_data_pos_,
      pay_lengths_[buf_pos_]
    );
    pay_data_pos_ += pay_lengths_[buf_pos_];
  }

  virtual void skip(uint64_t count) override {
    uint64_t left = postings_writer::BLOCK_SIZE - buf_pos_;
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
      buf_pos_ += uint32_t(count);
    }
    clear();
    value_ = 0;
  }

  virtual void refill() override {
    if (pos_in_->file_pointer() == tail_start_) {
      size_t pos = 0;

      for (uint64_t i = 0; i < tail_length_; ++i) {
        // read payloads
        if (shift_unpack_32(pos_in_->read_vint(), pos_deltas_[i])) {
          pay_lengths_[i] = pos_in_->read_vint();
        } else {
          assert(i);
          pay_lengths_[i] = pay_lengths_[i-1];
        }

        if (pay_lengths_[i]) {
          const auto size = pay_lengths_[i]; // current payload length

          oversize(pay_data_, pos + size);

          #ifdef IRESEARCH_DEBUG
            const auto read = pos_in_->read_bytes(&(pay_data_[0]) + pos, size);
            assert(read == size);
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
      encode::bitpack::read_block(*pos_in_, postings_writer::BLOCK_SIZE, enc_buf_, pos_deltas_);

      /* read payloads */
      const uint32_t size = pay_in_->read_vint();
      if (size) {
        encode::bitpack::read_block(*pay_in_, postings_writer::BLOCK_SIZE, enc_buf_, pay_lengths_);
        oversize(pay_data_, size);

        #ifdef IRESEARCH_DEBUG
          const auto read = pay_in_->read_bytes(&(pay_data_[0]), size);
          assert(read == size);
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

/* static */ pos_iterator::ptr pos_iterator::make(const features& enabled) {
  switch (enabled) {
    case features::POS : 
      return memory::make_unique<pos_iterator>(); 
    case features::POS_OFFS :
      return memory::make_unique<offs_iterator>();
    case features::POS_PAY :
      return memory::make_unique<pay_iterator>();
    case features::POS_OFFS_PAY :
      return memory::make_unique<offs_pay_iterator>();
  }

  assert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
/// @class pos_doc_iterator
///////////////////////////////////////////////////////////////////////////////
class pos_doc_iterator : public doc_iterator {
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
    assert(pos_);
    pos_->pend_pos_ += freq_.value;
    pos_->clear();

    return true;
  }

 protected:
  virtual void prepare_attributes(
    const version10::features& features,
    const irs::attribute_view& attrs,
    const index_input* pos_in,
    const index_input* pay_in
  ) final;

  virtual void seek_notify(const skip_context &ctx) final {
    assert(pos_);
    // notify positions
    pos_->prepare(ctx);
  }

 private:
  position position_;
  pos_iterator* pos_{};
}; // pos_doc_iterator

void pos_doc_iterator::prepare_attributes(
    const version10::features& enabled,
    const irs::attribute_view& attrs,
    const index_input* pos_in,
    const index_input* pay_in) {
  doc_iterator::prepare_attributes(
    enabled, attrs, pos_in, pay_in
  );

  assert(attrs.contains<frequency>());
  assert(enabled.position());

  // position attribute
  pos_iterator::ptr it = pos_iterator::make(enabled);
  pos_ = it.get();

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
  it->prepare(state);

  // finish initialization
  attrs_.emplace(position_);
  position_.reset(std::move(it));
}

NS_END // detail

// ----------------------------------------------------------------------------
// --SECTION--                                                index_meta_writer
// ----------------------------------------------------------------------------

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref index_meta_writer::FORMAT_PREFIX = "segments_";
const string_ref index_meta_writer::FORMAT_PREFIX_TMP = "pending_segments_";
const string_ref index_meta_writer::FORMAT_NAME = "iresearch_10_index_meta";
MSVC2015_ONLY(__pragma(warning(pop)))

template<>
std::string file_name<index_meta_reader, index_meta>(const index_meta& meta) {
  return file_name(index_meta_writer::FORMAT_PREFIX, meta.generation());
};

template<>
std::string file_name<index_meta_writer, index_meta>(const index_meta& meta) {
  return file_name(index_meta_writer::FORMAT_PREFIX_TMP, meta.generation());
};

std::string index_meta_writer::filename(const index_meta& meta) const {
  return file_name<index_meta_reader>(meta);
}

bool index_meta_writer::prepare(directory& dir, index_meta& meta) {
  if (meta_) {
    // prepare() was already called with no corresponding call to commit()
    return false;
  }

  prepare(meta); // prepare meta before generating filename

  auto seg_file = file_name<index_meta_writer>(meta);

  try {
    auto out = dir.create(seg_file);

    if (!out) {
      IR_FRMT_ERROR("Failed to create output file, path: %s", seg_file.c_str());
      return false;
    }

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
  } catch (const io_error& e) {
    IR_FRMT_ERROR("Caught i/o error, reason: %s", e.what());
    return false;
  }

  if (!dir.sync(seg_file)) {
    IR_FRMT_ERROR("Failed to sync output file, path: %s", seg_file.c_str());
    return false;
  }

  dir_ = &dir;
  meta_ = &meta;

  return true;
}

void index_meta_writer::commit() {
  if (!meta_) {
    return;
  }

  auto src = file_name<index_meta_writer>(*meta_);
  auto dst = file_name<index_meta_reader>(*meta_);

  try {
    auto clear_pending = make_finally([this]{ meta_ = nullptr; });

    if (!dir_->rename(src, dst)) {
      std::stringstream ss;

      ss << "Failed to rename file, src path: " << src
         << " dst path: " << dst;

      throw(detailed_io_error(ss.str()));
    }

    complete(*meta_);
    dir_ = nullptr;
  } catch ( ... ) {
    rollback();
    throw;
  }
}

void index_meta_writer::rollback() NOEXCEPT {
  if (!meta_) {
    return;
  }

  auto seg_file = file_name<index_meta_writer>(*meta_);

  if (!dir_->remove(seg_file)) { // suppress all errors
    IR_FRMT_ERROR("Failed to remove file, path: %s", seg_file.c_str());
  }

  dir_ = nullptr;
  meta_ = nullptr;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                index_meta_reader
// ----------------------------------------------------------------------------

uint64_t parse_generation(const std::string& segments_file) {
  assert(iresearch::starts_with(segments_file, index_meta_writer::FORMAT_PREFIX));

  const char* gen_str = segments_file.c_str() + index_meta_writer::FORMAT_PREFIX.size();
  char* suffix;
  auto gen = std::strtoull(gen_str, &suffix, 10); // 10 for base-10

  return suffix[0] ? type_limits<type_t::index_gen_t>::invalid() : gen;
}

bool index_meta_reader::last_segments_file(const directory& dir, std::string& out) const {
  uint64_t max_gen = 0;
  directory::visitor_f visitor = [&out, &max_gen] (std::string& name) {
    if (iresearch::starts_with(name, index_meta_writer::FORMAT_PREFIX)) {
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
    ? file_name<index_meta_reader>(meta)
    : static_cast<std::string>(filename);

  auto in = dir.open(
    meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    std::stringstream ss;

    ss << "Failed to open file, path: " << meta_file;

    throw detailed_io_error(ss.str());
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

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref segment_meta_writer::FORMAT_EXT = "sm";
const string_ref segment_meta_writer::FORMAT_NAME = "iresearch_10_segment_meta";
MSVC2015_ONLY(__pragma(warning(pop)))

template<>
std::string file_name<segment_meta_writer, segment_meta>(const segment_meta& meta) {
  return iresearch::file_name(meta.name, meta.version, segment_meta_writer::FORMAT_EXT);
};

std::string segment_meta_writer::filename(const segment_meta& meta) const {
  return file_name<segment_meta_writer>(meta);
}

void segment_meta_writer::write(directory& dir, const segment_meta& meta) {
  auto meta_file = file_name<segment_meta_writer>(meta);
  auto out = dir.create(meta_file);
  byte_type flags = meta.column_store ? segment_meta_writer::flags_t::HAS_COLUMN_STORE : 0;

  if (!out) {
    std::stringstream ss;

    ss << "Failed to create file, path: " << meta_file;

    throw detailed_io_error(ss.str());
  }

  format_utils::write_header(*out, FORMAT_NAME, FORMAT_MAX);
  write_string(*out, meta.name);
  out->write_vlong(meta.version);
  out->write_vlong( meta.docs_count);
  out->write_byte(flags);
  write_strings( *out, meta.files );
  format_utils::write_footer(*out);
}

// ----------------------------------------------------------------------------
// --SECTION--                                              segment_meta_reader
// ----------------------------------------------------------------------------

void segment_meta_reader::read(
    const directory& dir,
    segment_meta& meta,
    const string_ref& filename /*= string_ref::NIL*/) {

  const std::string meta_file = filename.null()
    ? file_name<segment_meta_writer>(meta)
    : static_cast<std::string>(filename);

  auto in = dir.open(
    meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    std::stringstream ss;

    ss << "Failed to open file, path: " << meta_file;

    throw detailed_io_error(ss.str());
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
  const int64_t count = in->read_vlong();
  const auto flags = in->read_byte();

  if (count < 0) {
    // corrupted index
    throw index_error();
  }

  if (flags & ~(segment_meta_writer::flags_t::HAS_COLUMN_STORE)) {
    // corrupted index
    throw index_error(); // use of unsupported flags
  }

  meta.name = std::move(name);
  meta.version = version;
  meta.column_store = flags & segment_meta_writer::flags_t::HAS_COLUMN_STORE;
  meta.docs_count = count;
  meta.files = read_strings<segment_meta::file_set>(*in);

  format_utils::check_footer(*in, checksum);
}

// ----------------------------------------------------------------------------
// --SECTION--                                             document_mask_writer 
// ----------------------------------------------------------------------------

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref document_mask_writer::FORMAT_NAME = "iresearch_10_doc_mask";
const string_ref document_mask_writer::FORMAT_EXT = "doc_mask";
MSVC2015_ONLY(__pragma(warning(pop)))

template<>
std::string file_name<document_mask_writer, segment_meta>(const segment_meta& meta) {
  return iresearch::file_name(meta.name, meta.version, document_mask_writer::FORMAT_EXT);
};

document_mask_writer::~document_mask_writer() {}

std::string document_mask_writer::filename(const segment_meta& meta) const {
  return file_name<document_mask_writer>(meta);
}

void document_mask_writer::prepare(directory& dir, const segment_meta& meta) {
  auto filename = file_name<document_mask_writer>(meta);

  out_ = dir.create(filename);

  if (!out_) {
    std::stringstream ss;

    ss << "Failed to create file, path: " << filename;

    throw detailed_io_error(ss.str());
  }
}

void document_mask_writer::begin(uint32_t count) {
  format_utils::write_header(*out_, FORMAT_NAME, FORMAT_MAX);
  out_->write_vint(count);
}

void document_mask_writer::write(const doc_id_t& mask) {
  out_->write_vlong(mask);
}

void document_mask_writer::end() {
  format_utils::write_footer(*out_);
}

// ----------------------------------------------------------------------------
// --SECTION--                                             document_mask_reader 
// ----------------------------------------------------------------------------

document_mask_reader::~document_mask_reader() {}

bool document_mask_reader::prepare(
    const directory& dir,
    const segment_meta& meta,
    bool* seen /*= nullptr*/
) {
  auto in_name = file_name<document_mask_writer>(meta);
  bool exists;

  // possible that the file does not exist since document_mask is optional
  if (dir.exists(exists, in_name) && !exists) {
    if (!seen) {
      IR_FRMT_ERROR("Failed to open file, path: %s", in_name.c_str());

      return false;
    }

    *seen = false;

    return true;
  }

  auto in = dir.open(
    in_name, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    IR_FRMT_ERROR("Failed to open file, path: %s", in_name.c_str());

    return false;
  }

  checksum_ = format_utils::checksum(*in);

  if (seen) {
    *seen = true;
  }

  in_ = std::move(in);

  return true;
}

uint32_t document_mask_reader::begin() {
  format_utils::check_header(
    *in_,
    document_mask_writer::FORMAT_NAME,
    document_mask_writer::FORMAT_MIN,
    document_mask_writer::FORMAT_MAX
  );

  return in_->read_vint();
}

void document_mask_reader::read(doc_id_t& doc_id) {
  auto id = in_->read_vlong();

  static_assert(sizeof(doc_id_t) == sizeof(decltype(id)), "sizeof(doc_id) != sizeof(decltype(id))");
  doc_id = id;
}

void document_mask_reader::end() {
  format_utils::check_footer(*in_, checksum_);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                      columnstore
// ----------------------------------------------------------------------------

NS_BEGIN(columns)

template<typename T, typename M>
std::string file_name(const M& meta); // forward declaration

template<typename T, typename M>
void file_name(std::string& buf, const M& meta); // forward declaration

class meta_writer final : public iresearch::column_meta_writer {
 public:
  static const string_ref FORMAT_NAME;
  static const string_ref FORMAT_EXT;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  virtual bool prepare(directory& dir, const segment_meta& meta) override;
  virtual void write(const std::string& name, field_id id) override;
  virtual void flush() override;

 private:
  index_output::ptr out_;
  field_id count_{}; // number of written objects
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

bool meta_writer::prepare(directory& dir, const segment_meta& meta) {
  auto filename = file_name<column_meta_writer>(meta);

  out_ = dir.create(filename);

  if (!out_) {
    IR_FRMT_ERROR("Failed to create file, path: %s", filename.c_str());
    return false;
  }

  format_utils::write_header(*out_, FORMAT_NAME, FORMAT_MAX);

  return true;
}

void meta_writer::write(const std::string& name, field_id id) {
  out_->write_vlong(id);
  write_string(*out_, name);
  ++count_;
}

void meta_writer::flush() {
  out_->write_long(count_); // write total number of written objects
  format_utils::write_footer(*out_);
  out_.reset();
  count_ = 0;
}

class meta_reader final : public iresearch::column_meta_reader {
 public:
  virtual bool prepare(
    const directory& dir, 
    const segment_meta& meta,
    field_id& count
  ) override;
  virtual bool read(column_meta& column) override;

 private:
  index_input::ptr in_;
  field_id count_{0};
}; // meta_writer

bool meta_reader::prepare(
    const directory& dir,
    const segment_meta& meta,
    field_id& count
) {
  auto filename = file_name<column_meta_writer>(meta);

  auto in = dir.open(
    filename, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE
  );

  if (!in) {
    IR_FRMT_ERROR("Failed to open file, path: %s", filename.c_str());
    return false;
  }

  const auto checksum = format_utils::checksum(*in);

  in->seek(in->length() - sizeof(field_id) - format_utils::FOOTER_LEN);

  // read number of objects to read
  count = in->read_long();

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
  return true;
}

bool meta_reader::read(column_meta& column) {
  if (!count_) {
    return false;
  }

  const auto id = in_->read_vlong();
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

const size_t INDEX_BLOCK_SIZE = 1024;
const size_t MAX_DATA_BLOCK_SIZE = 4096;

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
#else
    in.read_bytes(&(decode_buf[0]), buf_size);
#endif // IRESEARCH_DEBUG
    return;
  }

  irs::oversize(encode_buf, buf_size);

#ifdef IRESEARCH_DEBUG
  const auto read = in.read_bytes(&(encode_buf[0]), buf_size);
  assert(read == buf_size);
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

  if (!irs::type_limits<iresearch::type_t::address_t>::valid(buf_size)) {
    throw irs::index_error(); // corrupted index
  }
}

template<size_t Size>
class index_block {
 public:
  static const size_t SIZE = Size;

  bool push_back(doc_id_t key, uint64_t offset) {
    assert(keys_ <= key_);
    *key_++ = key;
    assert(key >= key_[-1]);
    *offset_++ = offset;
    assert(offset >= offset_[-1]);
    return key_ == std::end(keys_);
  }

  // returns total number of items
  size_t total() const {
    return flushed_ + this->size();
  }

  // returns number of items to be flushed
  size_t size() const {
    assert(key_ >= keys_);
    return key_ - keys_;
  }

  bool empty() const {
    return keys_ == key_;
  }

  ColumnProperty flush(data_output& out, uint64_t* buf) {
    if (empty()) {
      return CP_DENSE | CP_FIXED;
    }

    const auto size = this->size();

    // adjust number of elements to pack to the nearest value
    // that is multiple to the block size
    const auto block_size = math::ceil64(size, packed::BLOCK_SIZE_64);
    assert(block_size >= size);

    ColumnProperty props = CP_SPARSE;

    // write keys
    {
      const auto stats = encode::avg::encode(keys_, key_);
      const auto bits = encode::avg::write_block(
        out, stats.first, stats.second,
        keys_, block_size, buf
      );

      if (1 == stats.second && 0 == keys_[0] && encode::bitpack::rl(bits)) {
        props |= CP_DENSE;
      }
    }

    // write offsets
    {
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
  doc_id_t keys_[Size]{};
  uint64_t offsets_[Size]{};
  doc_id_t* key_{ keys_ };
  uint64_t* offset_{ offsets_ };
  size_t flushed_{}; // number of flushed items
}; // index_block

//////////////////////////////////////////////////////////////////////////////
/// @class writer
//////////////////////////////////////////////////////////////////////////////
class writer final : public iresearch::columnstore_writer {
 public:
  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  static const string_ref FORMAT_NAME;
  static const string_ref FORMAT_EXT;

  virtual bool prepare(directory& dir, const segment_meta& meta) override;
  virtual column_t push_column() override;
  virtual bool flush() override;

 private:
  class column final : public iresearch::columnstore_writer::column_output {
   public:
    column(writer& ctx) // compression context
      : ctx_(&ctx) {
      // initialize value offset
      // because of initial MAX_DATA_BLOCK_SIZE 'min_' will be set on the first 'write'
      offsets_[0] = MAX_DATA_BLOCK_SIZE;
      offsets_[1] = MAX_DATA_BLOCK_SIZE;
    }

    void prepare(doc_id_t key) {
      assert(key >= max_ || irs::type_limits<irs::type_t::doc_id_t>::eof(max_));

      auto& offset = offsets_[0];

      // commit previous key and offset unless the 'reset' method has been called
      if (max_ != pending_key_) {
        // will trigger 'flush_block' if offset >= MAX_DATA_BLOCK_SIZE
        offset = offsets_[size_t(block_index_.push_back(pending_key_, offset))];
        max_ = pending_key_;
      }

      // flush block if we've overcome MAX_DATA_BLOCK_SIZE size
      if (offset >= MAX_DATA_BLOCK_SIZE && key != pending_key_) {
        flush_block();
        min_ = key;
      }

      // reset key and offset (will be commited during the next 'write')
      offset = block_buf_.size();
      pending_key_ = key;

      assert(pending_key_ >= min_);
    }

    bool empty() const {
      return !block_index_.total();
    }

    void finish() {
      auto& out = *ctx_->data_out_;
      write_enum(out, props_); // column properties
      out.write_vlong(block_index_.total()); // total number of items
      out.write_vlong(max_); // max key
      out.write_vlong(avg_block_size_); // avg data block size
      out.write_vlong(avg_block_count_); // avg number of elements per block
      out.write_vlong(column_index_.total()); // total number of index blocks
      blocks_index_.file >> out; // column blocks index
    }

    void flush() {
      const auto blocks_count = std::max(size_t(1), column_index_.total());
      avg_block_count_ = (block_index_.total()-block_index_.size()) / blocks_count;
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
      block_buf_.reset(offsets_[0]);
      pending_key_ = max_;
    }

   private:
    void flush_block() {
      if (block_index_.empty()) {
        // nothing to flush
        return;
      }

      auto& out = *ctx_->data_out_;
      auto* buf = ctx_->buf_;

      // write first block key & where block starts
      if (column_index_.push_back(min_, out.file_pointer())) {
        column_index_.flush(blocks_index_.stream, buf);
      }

      // flush current block

      // write total number of elements in the block
      out.write_vlong(block_index_.size());

      // write block index, compressed data and aggregate block properties
      // note that order of calls is important here, since it is not defined
      // which expression should be evaluated first in the following example:
      //   const auto res = expr0() | expr1();
      // otherwise it would violate format layout
      auto block_props = block_index_.flush(out, buf);
      block_props |= write_compact(out, ctx_->comp_, static_cast<bytes_ref>(block_buf_));
      length_ += block_buf_.size();

      // refresh column properties
      props_ &= block_props;
      // reset buffer stream after flush
      block_buf_.reset();
    }

    writer* ctx_; // writer context
    uint64_t offsets_[2]; // value offset, because of initial MAX_DATA_BLOCK_SIZE 'min_' will be set on the first 'write'
    uint64_t length_{}; // size of the all column data blocks
    index_block<INDEX_BLOCK_SIZE> block_index_; // current block index (per document key/offset)
    index_block<INDEX_BLOCK_SIZE> column_index_; // column block index (per block key/offset)
    memory_output blocks_index_; // blocks index
    bytes_output block_buf_{ 2*MAX_DATA_BLOCK_SIZE }; // data buffer
    doc_id_t min_{ type_limits<type_t::doc_id_t>::eof() }; // min key
    doc_id_t max_{ type_limits<type_t::doc_id_t>::eof() }; // max key
    doc_id_t pending_key_{ type_limits<type_t::doc_id_t>::eof() }; // current pending key
    ColumnProperty props_{ CP_DENSE | CP_FIXED | CP_MASK }; // aggregated column properties
    uint64_t avg_block_count_{}; // average number of items per block (tail block has not taken into account since it may skew distribution)
    uint64_t avg_block_size_{}; // average size of the block (tail block has not taken into account since it may skew distribution)
  };

  uint64_t buf_[INDEX_BLOCK_SIZE]; // reusable temporary buffer for packing
  std::deque<column> columns_; // pointers remain valid
  compressor comp_{ 2*MAX_DATA_BLOCK_SIZE };
  index_output::ptr data_out_;
  std::string filename_;
  directory* dir_;
}; // writer

MSVC2015_ONLY(__pragma(warning(push)))
MSVC2015_ONLY(__pragma(warning(disable: 4592))) // symbol will be dynamically initialized (implementation limitation) false positive bug in VS2015.1
const string_ref writer::FORMAT_NAME = "iresearch_10_columnstore";
const string_ref writer::FORMAT_EXT = "cs";
MSVC2015_ONLY(__pragma(warning(pop)))

template<>
std::string file_name<columnstore_writer, segment_meta>(
    const segment_meta& meta
) {
  return irs::file_name(meta.name, columns::writer::FORMAT_EXT);
};

template<>
void file_name<columnstore_writer, segment_meta>(
    std::string& buf,
    const segment_meta& meta
) {
  irs::file_name(buf, meta.name, columns::writer::FORMAT_EXT);
};

bool writer::prepare(directory& dir, const segment_meta& meta) {
  columns_.clear();

  dir_ = &dir;
  file_name<columnstore_writer>(filename_, meta);
  data_out_ = dir.create(filename_);

  if (!data_out_) {
    IR_FRMT_ERROR("Failed to create file, path: %s", filename_.c_str());
    return false;
  }

  format_utils::write_header(*data_out_, FORMAT_NAME, FORMAT_MAX);

  return true;
}

columnstore_writer::column_t writer::push_column() {
  const auto id = columns_.size();
  columns_.emplace_back(*this);
  auto& column = columns_.back();

  return std::make_pair(id, [&column, this] (doc_id_t doc) -> column_output& {
    assert(!irs::type_limits<irs::type_t::doc_id_t>::eof(doc));

    column.prepare(doc);
    return column;
  });
}

bool writer::flush() {
  // trigger commit for each pending key
  for (auto& column : columns_) {
    column.prepare(irs::type_limits<irs::type_t::doc_id_t>::eof());
  }

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
  data_out_.reset();

  return true;
}

template<typename Block, typename Allocator>
class block_cache : irs::util::noncopyable {
 public:
  block_cache(const Allocator& alloc = Allocator())
    : cache_(alloc) {
  }
  block_cache(block_cache&& rhs) 
    : cache_(std::move(rhs.cache_)) {
  }

  template<typename... Args>
  Block& emplace_back(Args&&... args) {
    cache_.emplace_back(std::forward<Args>(args)...);
    return cache_.back();
  }

  void pop_back() {
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

  bool load(index_input& in, decompressor& decomp, bstring& buf) {
    const size_t size = in.read_vlong(); // total number of entries in a block
    assert(size);

    auto begin = std::begin(index_);

    // read keys
    encode::avg::visit_block_packed_tail(
      in, size, reinterpret_cast<uint64_t*>(&buf[0]),
      [begin](uint64_t key) mutable {
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

    return true;
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

    const uint64_t* begin_{};
    const uint64_t* it_{};
    const uint64_t* end_{};
    const bstring* data_{};
    doc_id_t base_{};
  }; // iterator

  bool load(index_input& in, decompressor& decomp, bstring& buf) {
    const size_t size = in.read_vlong(); // total number of entries in a block
    assert(size);

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint64_t avg;
    if (!encode::avg::read_block_rl64(in, base_, avg) || 1 != avg) {
      // invalid block type
      return false;
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

    return true;
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
  uint64_t index_[INDEX_BLOCK_SIZE];
  bstring data_;
  uint64_t* end_{ index_ };
  doc_id_t base_{ };
}; // dense_block

class dense_fixed_length_block : util::noncopyable {
 public:
  class iterator {
   public:
    bool seek(doc_id_t doc) NOEXCEPT {
      if (doc < value_) {
        doc = value_;
      }

      // FIXME refactor
      begin_ = avg_length_*(doc-base_);

      return next();
    }

    const irs::doc_id_t& value() const NOEXCEPT { return value_; }

    const irs::bytes_ref& value_payload() const NOEXCEPT {
      return value_payload_;
    }

    bool next() NOEXCEPT {
      if (begin_ >= end_) {
        return false;
      }

      value_ = base_ + begin_ / avg_length_;
      next_value();

      return true;
    }

    void seal() NOEXCEPT {
      value_ = irs::type_limits<irs::type_t::doc_id_t>::eof();
      value_payload_ = irs::bytes_ref::NIL;
      begin_ = end_ = 0;
    }

    void reset(const dense_fixed_length_block& block) NOEXCEPT {
      value_ = block.base_key_;
      value_payload_ = bytes_ref::NIL;
      begin_ = 0;
      end_ = block.avg_length_*block.size_;
      avg_length_ = block.avg_length_;
      base_ = block.base_key_;
      data_ = &block.data_;
    }

    bool operator==(const dense_fixed_length_block& rhs) const NOEXCEPT {
      return data_ == &rhs.data_;
    }

    bool operator!=(const dense_fixed_length_block& rhs) const NOEXCEPT {
      return !(*this == rhs);
    }

   private:
    irs::bytes_ref value_payload_ { irs::bytes_ref::NIL };
    irs::doc_id_t value_ { irs::type_limits<irs::type_t::doc_id_t>::invalid() };

    // note that function increases 'begin_' value
    void next_value() NOEXCEPT {
      value_payload_ = bytes_ref(data_->c_str() + begin_, avg_length_);
      begin_ += avg_length_;
    }

    uint64_t begin_{}; // start offset
    uint64_t end_{}; // end offset
    uint64_t avg_length_{}; // average value length
    doc_id_t base_{}; // base doc_id
    const bstring* data_{};
  }; // iterator

  bool load(index_input& in, decompressor& decomp, bstring& buf) {
    size_ = in.read_vlong(); // total number of entries in a block
    assert(size_);

    // dense block must be encoded with RL encoding, avg must be equal to 1
    uint64_t avg;
    if (!encode::avg::read_block_rl64(in, base_key_, avg) || 1 != avg) {
      // invalid block type
      return false;
    }

    // fixed length block must be encoded with RL encoding
    if (!encode::avg::read_block_rl64(in, base_offset_, avg_length_)) {
      // invalid block type
      return false;
    }

    // read data
    read_compact(in, decomp, buf, data_);

    return true;
  }

  bool value(doc_id_t key, bytes_ref& out) const {
    // expect 0-based key
    assert(key < size_);

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
  uint64_t base_offset_{}; // base offset
  uint64_t avg_length_{}; // entry length
  doc_id_t size_{}; // total number of entries
  bstring data_;
}; // dense_fixed_length_block

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

  sparse_mask_block() {
    std::fill(
      std::begin(keys_), std::end(keys_),
      type_limits<type_t::doc_id_t>::eof()
    );
  }

  bool load(index_input& in, decompressor& /*decomp*/, bstring& buf) {
    size_ = in.read_vlong(); // total number of entries in a block
    assert(size_);

    auto begin = std::begin(keys_);

    encode::avg::visit_block_packed_tail(
      in, size_, reinterpret_cast<uint64_t*>(&buf[0]),
      [begin](uint64_t key) mutable {
        *begin++ = key;
    });

    // mask block has no data, so all offsets should be equal to 0
    if (!encode::avg::check_block_rl64(in, 0)) {
      // invalid block type
      return false;
    }

    return true;
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

// placeholder for 'dense_fixed_length_column' specialization
// doesn't store any data
struct dense_mask_block;

template<typename Allocator = std::allocator<sparse_block>>
class read_context
  : public block_cache_traits<sparse_block, Allocator>::cache_t,
    public block_cache_traits<dense_block, Allocator>::cache_t,
    public block_cache_traits<dense_fixed_length_block, Allocator>::cache_t,
    public block_cache_traits<sparse_mask_block, Allocator>::cache_t {
 public:
  DECLARE_SPTR(read_context);

  static ptr make(const index_input& stream) {
    auto clone = stream.reopen();

    if (!clone) {
      IR_FRMT_FATAL("Failed to reopen document input in: %s", __FUNCTION__);

      return nullptr;
    }

    return std::make_shared<read_context>(std::move(clone));
  }

  read_context(index_input::ptr&& in = index_input::ptr(), const Allocator& alloc = Allocator())
    : block_cache_traits<sparse_block, Allocator>::cache_t(typename block_cache_traits<sparse_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_block, Allocator>::cache_t(typename block_cache_traits<dense_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<dense_fixed_length_block, Allocator>::cache_t(typename block_cache_traits<dense_fixed_length_block, Allocator>::allocator_t(alloc)),
      block_cache_traits<sparse_mask_block, Allocator>::cache_t(typename block_cache_traits<sparse_mask_block, Allocator>::allocator_t(alloc)),
      buf_(INDEX_BLOCK_SIZE*sizeof(uint64_t), 0),
      stream_(std::move(in)) {
  }

  template<typename Block, typename... Args>
  Block* emplace_back(uint64_t offset, Args&&... args) {
    auto& block = emplace_block<Block>(
      std::forward<Args>(args)...
    ); // add cache entry

    if (!load(block, offset)) {
      // unable to load block
      pop_back<Block>();
      return nullptr;
    }

    return &block;
  }

  template<typename Block>
  bool load(Block& block, uint64_t offset) {
    stream_->seek(offset); // seek to the offset
    return block.load(*stream_, decomp_, buf_);
  }

  template<typename Block>
  void pop_back() {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;
    cache.pop_back();
  }

 private:
  template<typename Block, typename... Args>
  Block& emplace_block(Args&&... args) {
    typename block_cache_traits<Block, Allocator>::cache_t& cache = *this;
    return cache.emplace_back(std::forward<Args>(args)...);
  }

  decompressor decomp_; // decompressor
  bstring buf_; // temporary buffer for decoding/unpacking
  index_input::ptr stream_;
}; // read_context

typedef read_context<> read_context_t;

class context_provider : private util::noncopyable {
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
// by 'ref' and retuns a pointer to cached
// instance, nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t* load_block(
    const context_provider& ctxs,
    BlockRef& ref) {
  typedef typename BlockRef::block_t block_t;

  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();

    if (!ctx) {
      // unable to get context
      return nullptr;
    }

    // load block
    const auto* block = ctx->template emplace_back<block_t>(ref.offset);

    if (!block) {
      // failed to load block
      return nullptr;
    }

    // mark block as loaded
    if (ref.pblock.compare_exchange_strong(cached, block)) {
      cached = block;
    } else {
      // already cached by another thread
      ctx->template pop_back<block_t>();
    }
  }

  return cached;
}

// in case of success caches block pointed
// by 'ref' in the specified 'block' and
// retuns a pointer to cached instance,
// nullptr otherwise
template<typename BlockRef>
const typename BlockRef::block_t* load_block(
    const context_provider& ctxs,
    const BlockRef& ref,
    typename BlockRef::block_t& block) {
  const auto* cached = ref.pblock.load();

  if (!cached) {
    auto ctx = ctxs.get_context();

    if (!ctx) {
      // unable to get context
      return nullptr;
    }

    if (!ctx->load(block, ref.offset)) {
      // unable to load block
      return nullptr;
    }

    cached = &block;
  }

  return cached;
}

////////////////////////////////////////////////////////////////////////////////
/// @class column
////////////////////////////////////////////////////////////////////////////////
class column
    : public irs::columnstore_reader::column_reader,
      private util::noncopyable {
 public:
  DECLARE_PTR(column);

  column(ColumnProperty props)
    : props_(props) {
  }

  virtual ~column() { }

  virtual bool read(data_input& in, uint64_t* /*buf*/) {
    count_ = in.read_vlong();
    max_ = in.read_vlong();
    avg_block_size_ = in.read_vlong();
    avg_block_count_ = in.read_vlong();
    if (!avg_block_count_) {
      avg_block_count_ = count_;
    }
    return true;
  }

  doc_id_t max() const NOEXCEPT { return max_; }
  virtual size_t size() const NOEXCEPT override { return count_; }
  bool empty() const NOEXCEPT { return 0 == size(); }
  size_t avg_block_size() const NOEXCEPT { return avg_block_size_; }
  size_t avg_block_count() const NOEXCEPT { return avg_block_count_; }
  ColumnProperty props() const NOEXCEPT { return props_; }

 private:
  doc_id_t max_{ type_limits<type_t::doc_id_t>::eof() };
  size_t count_{};
  size_t avg_block_size_{};
  size_t avg_block_count_{};
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
    virtual bool next() { return nullptr != value_; }
    virtual const irs::bytes_ref& value() const {
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

    const auto* cached = load_block(*column_->ctxs_, *begin_);

    if (!cached) {
      // unable to load block, seal the iterator
      block_.seal();
      begin_ = end_;
      payload_.value_ = nullptr;

      return false;
    }

    if (block_ != *cached) {
      block_.reset(*cached);
      payload_.value_ = &(block_.value_payload());
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

  virtual bool read(data_input& in, uint64_t* buf) override {
    if (!column::read(in, buf)) {
      return false;
    }
    size_t blocks_count = in.read_vlong(); // total number of column index blocks

    std::vector<block_ref> refs(blocks_count + 1); // +1 for upper bound

    auto begin = refs.begin();
    while (blocks_count >= INDEX_BLOCK_SIZE) {
      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, buf,
        [begin](uint64_t key) mutable {
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
        in, blocks_count, buf,
        [begin](uint64_t key) mutable {
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
    return true;
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

    const auto* cached = load_block(*ctxs_, *it);

    if (!cached) {
      // unable to load block
      return false;
    }

    assert(cached);
    return cached->value(key, value);
  };

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor
  ) const override {
    block_t block; // don't cache new blocks
    for (auto begin = refs_.begin(), end = refs_.end()-1; begin != end; ++begin) { // -1 for upper bound
      const auto* cached = load_block(*ctxs_, *begin, block);

      if (!cached) {
        // unable to load block
        return false;
      }

      assert(cached);

      if (!cached->visit(visitor)) {
        return false;
      }
    }
    return true;
  }

  virtual doc_iterator::ptr iterator() const override {
    typedef column_iterator<column_t> iterator_t;

    return empty()
      ? doc_iterator::empty()
      : doc_iterator::make<iterator_t>(
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
/// @class dense_fixed_length_column
////////////////////////////////////////////////////////////////////////////////
template<typename Block>
class dense_fixed_length_column final : public column {
 public:
  typedef dense_fixed_length_column column_t;
  typedef Block block_t;

  static column::ptr make(const context_provider& ctxs, ColumnProperty props) {
    return memory::make_unique<column_t>(ctxs, props);
  }

  dense_fixed_length_column(const context_provider& ctxs, ColumnProperty prop)
    : column(prop), ctxs_(&ctxs) {
  }

  virtual bool read(data_input& in, uint64_t* buf) override {
    if (!column::read(in, buf)) {
      return false;
    }
    size_t blocks_count = in.read_vlong(); // total number of column index blocks

    std::vector<block_ref> refs(blocks_count);

    auto begin = refs.begin();
    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl64(in, this->avg_block_count())) {
        // invalid column type
        return false;
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
        : 0; // in this case avg == 0

      if (!encode::avg::check_block_rl64(in, avg_block_count)) {
        // invalid column type
        return false;
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
    min_ = this->max() - this->size() + 1;

    return true;
  }

  bool value(doc_id_t key, bytes_ref& value) const {
    if ((key -= min_) >= this->size()) {
      return false;
    }

    const auto block_idx = key / this->avg_block_count();
    assert(block_idx < refs_.size());

    auto& ref = const_cast<block_ref&>(refs_[block_idx]);

    const auto* cached = load_block(*ctxs_, ref);

    if (!cached) {
      // unable to load block
      return false;
    }

    assert(cached);
    return cached->value(key -= block_idx*this->avg_block_count(), value);
  }

  virtual bool visit(
      const columnstore_reader::values_visitor_f& visitor
  ) const override {
    block_t block; // don't cache new blocks
    for (auto& ref : refs_) {
      const auto* cached = load_block(*ctxs_, ref, block);

      if (!cached) {
        // unable to load block
        return false;
      }

      assert(cached);

      if (!cached->visit(visitor)) {
        return false;
      }
    }

    return true;
  }

  virtual doc_iterator::ptr iterator() const override {
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
    const auto min  = min_ + this->avg_block_count()*std::distance(refs_.data(), begin);

    if (key < min) {
      return begin;
    }

    if ((key -= min_) >= this->size()) {
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

    if ((key -= min_) >= this->size()) {
      return refs_.end();
    }

    const auto block_idx = key / this->avg_block_count();
    assert(block_idx < refs_.size());

    return std::begin(refs_) + block_idx;
  }

  const context_provider* ctxs_;
  refs_t refs_;
  doc_id_t min_{}; // min key
}; // dense_fixed_length_column

template<>
class dense_fixed_length_column<dense_mask_block> final : public column {
 public:
  typedef dense_fixed_length_column column_t;

  static column::ptr make(const context_provider&, ColumnProperty props) {
    return memory::make_unique<column_t>(props);
  }

  dense_fixed_length_column(ColumnProperty prop) NOEXCEPT
    : column(prop) {
  }

  virtual bool read(data_input& in, uint64_t* buf) override {
    // we treat data in blocks as "garbage" which could be
    // potentially removed on merge, so we don't validate
    // column properties using such blocks

    if (!column::read(in, buf)) {
      return false;
    }

    size_t blocks_count = in.read_vlong(); // total number of column index blocks

    while (blocks_count >= INDEX_BLOCK_SIZE) {
      if (!encode::avg::check_block_rl64(in, this->avg_block_count())) {
        // invalid column type
        return false;
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed(
        in, INDEX_BLOCK_SIZE, buf,
        [](uint64_t ) {}
      );

      blocks_count -= INDEX_BLOCK_SIZE;
    }

    // tail block
    if (blocks_count) {
      const auto avg_block_count = blocks_count > 1
        ? this->avg_block_count()
        : 0; // in this case avg == 0

      if (!encode::avg::check_block_rl64(in, avg_block_count)) {
        // invalid column type
        return false;
      }

      // skip offsets, they point to "garbage" data
      encode::avg::visit_block_packed_tail(
        in, blocks_count, buf,
        [](uint64_t ) { }
      );
    }


    min_ = this->max() - this->size();

    return true;
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

  virtual doc_iterator::ptr iterator() const override;

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
}; // dense_fixed_length_column

doc_iterator::ptr dense_fixed_length_column<dense_mask_block>::iterator() const {
  return empty()
    ? doc_iterator::empty()
    : doc_iterator::make<column_iterator>(*this)
    ;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                 column factories
// ----------------------------------------------------------------------------

typedef std::function<
  column::ptr(const context_provider& ctxs, ColumnProperty prop)
> column_factory_f;

column_factory_f g_column_factories[] {
  &sparse_column<sparse_block>::make,                          // CP_SPARSE == 0
  &sparse_column<dense_block>::make,                           // CP_DENSE  == 1
  &sparse_column<sparse_block>::make,                          // CP_FIXED  == 2
  &dense_fixed_length_column<dense_fixed_length_block>::make, // CP_DENSE | CP_FIXED == 3
  nullptr,                                                     // CP_MASK == 4
  nullptr,                                                     // CP_DENSE | CP_MASK == 5
  &sparse_column<sparse_mask_block>::make,                    // CP_FIXED | CP_MASK == 6
  &dense_fixed_length_column<dense_mask_block>::make          // CP_DENSE | CP_FIXED | CP_MASK == 7
};

//////////////////////////////////////////////////////////////////////////////
/// @class reader
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_PLUGIN reader final : public columnstore_reader, public context_provider {
 public:
  explicit reader(size_t pool_size = 16)
    : context_provider(pool_size) {
  }

  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta,
    bool* seen = nullptr
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
    const segment_meta& meta,
    bool* seen /*= nullptr*/
) {
  auto filename = file_name<columnstore_writer>(meta);
  bool exists;

  // possible that the file does not exist since columnstore is optional
  if (dir.exists(exists, filename) && !exists) {
    if (!seen) {
      IR_FRMT_ERROR("Failed to open file, path: %s", filename.c_str());

      return false;
    }

    *seen = false;

    return true;
  }

  // open columstore stream
  auto stream = dir.open(filename, irs::IOAdvice::RANDOM);

  if (!stream) {
    IR_FRMT_ERROR("Failed to open file, path: %s", filename.c_str());

    return false;
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
    // create column
    const auto& factory = g_column_factories[props];
    assert(factory);
    auto column = factory(*this, props);
    // read column
    if (!column || !column->read(*stream, buf)) {
      IR_FRMT_ERROR("Unable to load blocks index for column id=" IR_SIZE_T_SPECIFIER, i);
      return false;
    }

    columns.emplace_back(std::move(column));
  }

  // noexcept
  context_provider::prepare(std::move(stream));
  columns_ = std::move(columns);

  if (seen) {
    *seen = true;
  }

  return true;
}

const reader::column_reader* reader::column(field_id field) const {
  return field >= columns_.size()
    ? nullptr // can't find column with the specified identifier
    : columns_[field].get();
}

NS_END // columns

// ----------------------------------------------------------------------------
// --SECTION--                                                  postings_writer
// ----------------------------------------------------------------------------

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

void postings_writer::doc_stream::flush(uint64_t* buf, bool freq) {
  encode::bitpack::write_block(*out, deltas, BLOCK_SIZE, buf);

  if (freq) {
    encode::bitpack::write_block(*out, freqs.get(), BLOCK_SIZE, buf);
  }
}

void postings_writer::pos_stream::flush(uint32_t* comp_buf) {
  encode::bitpack::write_block(*out, this->buf, BLOCK_SIZE, comp_buf);
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
  encode::bitpack::write_block(*out, pay_sizes, BLOCK_SIZE, buf);
  out->write_bytes(pay_buf_.c_str(), pay_buf_.size());
  pay_buf_.clear();
}

void postings_writer::pay_stream::flush_offsets(uint32_t* buf) {
  encode::bitpack::write_block(*out, offs_start_buf, BLOCK_SIZE, buf);
  encode::bitpack::write_block(*out, offs_len_buf, BLOCK_SIZE, buf);
}

postings_writer::postings_writer(bool volatile_attributes)
  : skip_(BLOCK_SIZE, SKIP_N),
    volatile_attributes_(volatile_attributes) {
  attrs_.emplace(docs_);
}

void postings_writer::prepare(index_output& out, const iresearch::flush_state& state) {
  assert(!state.name.null());

  // reset writer state
  docs_count = 0;

  std::string name;

  // prepare document stream
  detail::prepare_output(name, doc.out, state, DOC_EXT, DOC_FORMAT_NAME, FORMAT_MAX);

  auto& features = *state.features;
  if (features.check<frequency>() && !doc.freqs) {
    // prepare frequency stream
    doc.freqs = memory::make_unique<uint64_t[]>(BLOCK_SIZE);
    std::memset(doc.freqs.get(), 0, sizeof(uint64_t) * BLOCK_SIZE);
  }

  if (features.check< position >()) {
    // prepare proximity stream
    if (!pos_) {
      pos_ = memory::make_unique< pos_stream >();
    }

    pos_->reset();
    detail::prepare_output(name, pos_->out, state, POS_EXT, POS_FORMAT_NAME, FORMAT_MAX);

    if (features.check< payload >() || features.check< offset >()) {
      // prepare payload stream
      if (!pay_) {
        pay_ = memory::make_unique<pay_stream>();
      }

      pay_->reset();
      detail::prepare_output(name, pay_->out, state, PAY_EXT, PAY_FORMAT_NAME, FORMAT_MAX);
    }
  }

  skip_.prepare(
    MAX_SKIP_LEVELS, state.doc_count,
    [this](size_t i, index_output& out) {
      write_skip(i, out);
  });

  // write postings format name
  format_utils::write_header(out, TERMS_FORMAT_NAME, TERMS_FORMAT_MAX);
  // write postings block size
  out.write_vint(BLOCK_SIZE);

  // prepare documents bitset
  docs_.value.reset(state.doc_count);
}

void postings_writer::begin_field(const iresearch::flags& field) {
  features_ = version10::features(field);
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

irs::postings_writer::state postings_writer::write(doc_iterator& docs) {
  REGISTER_TIMER_DETAILED();
  auto& freq = docs.attributes().get<frequency>();

  auto& pos = freq
    ? docs.attributes().get<position>()
    : irs::attribute_view::ref<position>::NIL;

  const offset* offs = nullptr;
  const payload* pay = nullptr;

  uint64_t* tfreq = nullptr;

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
    assert(pos_);
    pos_->start = pos_->out->file_pointer();
    std::fill_n(pos_->skip_ptr, MAX_SKIP_LEVELS, pos_->start);
    if (features_.payload() || features_.offset()) {
      assert(pay_);
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
    // docs out of order
    throw index_error();
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
  assert(pos_); /* at least positions stream should be created */

  pos_->pos(pos - pos_->last);
  if (pay) pay_->payload(pos_->size, pay->value);
  if (offs) pay_->offsets(pos_->size, offs->start, offs->end);

  pos_->next(pos);

  if (pos_->full()) {
    auto* buf32 = reinterpret_cast<uint32_t*>(buf);

    pos_->flush(buf32);

    if (pay) {
      pay_->flush_payload(buf32);
    }

    if (offs) {
      pay_->flush_offsets(buf32);
    }
  }
}

void postings_writer::end_doc() {
  if ( doc.full() ) {
    doc.block_last = doc.last;
    doc.end = doc.out->file_pointer();
    if ( pos_ ) {
      assert( pos_ );
      pos_->end = pos_->out->file_pointer();
      // documents stream is full, but positions stream is not
      // save number of positions to skip before the next block
      pos_->block_last = pos_->size;       
      if ( pay_ ) {
        pay_->end = pay_->out->file_pointer();
        pay_->block_last = pay_->pay_buf_.size();
      }
    }

    doc.size = 0;
  }
}

void postings_writer::end_term(version10::term_meta& meta, const uint64_t* tfreq) {
  if (docs_count == 0) {
    return; // no documents to write
  }

  if (1 == meta.docs_count) {
    meta.e_single_doc = doc.deltas[0];
  } else {
    /* write remaining documents using
     * variable length encoding */
    data_output& out = *doc.out;

    for (uint32_t i = 0; i < doc.size; ++i) {
      const uint64_t doc_delta = doc.deltas[i];

      if (!features_.freq()) {
        out.write_vlong(doc_delta);
      } else {
        assert(doc.freqs);
        const uint64_t freq = doc.freqs[i];

        if (1 == freq) {
          out.write_vlong(shift_pack_64(doc_delta, true));
        } else {
          out.write_vlong(shift_pack_64(doc_delta, false));
          out.write_vlong(freq);
        }
      }
    }
  }

  meta.pos_end = type_limits<type_t::address_t>::invalid();

  /* write remaining position using
   * variable length encoding */
  if (features_.position()) {
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
        pay_->pay_buf_.clear();
      }
    }
  }

  if (!tfreq) {
    meta.freq = integer_traits<uint64_t>::const_max;
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
  const uint64_t doc_delta = doc.block_last; //- doc.skip_doc[level];
  const uint64_t doc_ptr = doc.out->file_pointer();

  out.write_vlong(doc_delta);
  out.write_vlong(doc_ptr - doc.skip_ptr[level]);

  doc.skip_doc[level] = doc.block_last;
  doc.skip_ptr[level] = doc_ptr;

  if (features_.position()) {
    assert(pos_);

    const uint64_t pos_ptr = pos_->out->file_pointer();

    out.write_vint(pos_->block_last);
    out.write_vlong(pos_ptr - pos_->skip_ptr[level]);

    pos_->skip_ptr[level] = pos_ptr;

    if (features_.payload() || features_.offset()) {
      assert(pay_);

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

  out.write_vlong(meta.docs_count);
  if (meta.freq != integer_traits<uint64_t>::const_max) {
    assert(meta.freq >= meta.docs_count);
    out.write_vlong(meta.freq - meta.docs_count);
  }

  out.write_vlong(meta.doc_start - last_state.doc_start);
  if (features_.position()) {
    out.write_vlong(meta.pos_start - last_state.pos_start);
    if (type_limits<type_t::address_t>::valid(meta.pos_end)) {
      out.write_vlong(meta.pos_end);
    }
    if (features_.payload() || features_.offset()) {
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

  if (pos_) {
    format_utils::write_footer(*pos_->out);
    pos_->out.reset(); // ensure stream is closed
  }

  if (pay_) {
    format_utils::write_footer(*pay_->out);
    pay_->out.reset(); // ensure stream is closed
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                  postings_reader 
// ----------------------------------------------------------------------------

bool postings_reader::prepare(
    index_input& in, 
    const reader_state& state,
    const flags& features) {
  std::string buf;

  // prepare document input
  detail::prepare_input(
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
    detail::prepare_input(
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
      detail::prepare_input(
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

  const uint64_t block_size = in.read_vlong();
  if (block_size != postings_writer::BLOCK_SIZE) {
    // invalid block size
    throw index_error();
  }

  return true;
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

  term_meta.docs_count = in.read_vlong();
  if (term_freq) {
    term_freq->value = term_meta.docs_count + in.read_vlong();
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

doc_iterator::ptr postings_reader::iterator(
    const flags& field,
    const attribute_view& attrs,
    const flags& req) {
  typedef detail::doc_iterator doc_iterator_t;
  typedef detail::pos_doc_iterator pos_doc_iterator_t;

  // compile field features
  const auto features = version10::features(field);
  // get enabled features:
  // find intersection between requested and available features
  const auto enabled = features & req;

  auto it = enabled.position()
    ? doc_iterator_t::make<pos_doc_iterator_t>()
    : doc_iterator_t::make<doc_iterator_t>();

  it->prepare(
    features, enabled, attrs,
    doc_in_.get(), pos_in_.get(), pay_in_.get() 
  );

  return IMPLICIT_MOVE_WORKAROUND(it);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                         features
// ----------------------------------------------------------------------------

features::features(const flags& in) NOEXCEPT {
  set_bit<0>(in.check<iresearch::frequency>(), mask_);
  set_bit<1>(in.check<iresearch::position>(), mask_);
  set_bit<2>(in.check<iresearch::offset>(), mask_);
  set_bit<3>(in.check<iresearch::payload>(), mask_);
}

features features::operator&(const flags& in) const NOEXCEPT {
  return features(*this) &= in;
}

features& features::operator&=(const flags& in) NOEXCEPT {
  unset_bit<0>(!in.check<iresearch::frequency>(), mask_);
  unset_bit<1>(!in.check<iresearch::position>(), mask_);
  unset_bit<2>(!in.check<iresearch::offset>(), mask_);
  unset_bit<3>(!in.check<iresearch::payload>(), mask_);

  return *this;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                           format 
// ----------------------------------------------------------------------------


format::format(): iresearch::format(format::type()) {}

index_meta_writer::ptr format::get_index_meta_writer() const  {
  return iresearch::index_meta_writer::make<index_meta_writer>();
}

index_meta_reader::ptr format::get_index_meta_reader() const {
  // can reuse stateless reader
  static index_meta_reader reader;

  return memory::make_managed<irs::index_meta_reader, false>(&reader);
}

segment_meta_writer::ptr format::get_segment_meta_writer() const {
  // can reuse stateless writer
  static segment_meta_writer writer;

  return memory::make_managed<irs::segment_meta_writer, false>(&writer);
}

segment_meta_reader::ptr format::get_segment_meta_reader() const {
  // can reuse stateless writer
  static segment_meta_reader reader;

  return memory::make_managed<irs::segment_meta_reader, false>(&reader);
}

document_mask_writer::ptr format::get_document_mask_writer() const {
  return iresearch::document_mask_writer::make<document_mask_writer>();
}

document_mask_reader::ptr format::get_document_mask_reader() const {
  return iresearch::document_mask_reader::make<document_mask_reader>();
}

field_writer::ptr format::get_field_writer(bool volatile_state) const {
  return iresearch::field_writer::make<burst_trie::field_writer>(
    iresearch::postings_writer::make<version10::postings_writer>(volatile_state),
    volatile_state
  );
}

field_reader::ptr format::get_field_reader() const  {
  return iresearch::field_reader::make<iresearch::burst_trie::field_reader>(
    iresearch::postings_reader::make<version10::postings_reader>()
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

DEFINE_FORMAT_TYPE_NAMED(iresearch::version10::format, "1_0");
REGISTER_FORMAT( iresearch::version10::format );
DEFINE_FACTORY_SINGLETON(format);

NS_END /* version10 */
NS_END /* root */

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------