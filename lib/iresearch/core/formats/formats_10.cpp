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

#include "formats_10.hpp"

#include <limits>

extern "C" {
#include <simdbitpacking.h>
}

#include "analysis/token_attributes.hpp"
#include "columnstore.hpp"
#include "columnstore2.hpp"
#include "format_utils.hpp"
#include "formats_10_attributes.hpp"
#include "formats_burst_trie.hpp"
#include "index/field_meta.hpp"
#include "index/file_names.hpp"
#include "index/index_features.hpp"
#include "index/index_meta.hpp"
#include "index/index_reader.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "shared.hpp"
#include "skip_list.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitpack.hpp"
#include "utils/log.hpp"
#include "utils/memory.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"

#if defined(_MSC_VER)
#pragma warning(disable : 4351)
#endif

namespace {

using namespace irs;

// name of the module holding different formats
constexpr std::string_view MODULE_NAME = "10";

void write_strings(data_output& out, std::span<const std::string> strings) {
  write_size(out, strings.size());
  for (const auto& s : strings) {
    write_string(out, s);
  }
}

std::vector<std::string> read_strings(data_input& in) {
  const size_t size = read_size(in);

  std::vector<std::string> strings(size);

  for (auto begin = strings.begin(); begin != strings.end(); ++begin) {
    *begin = read_string<std::string>(in);
  }

  return strings;
}

template<bool Wand, uint32_t PosMin>
struct format_traits {
  using align_type = uint32_t;

  static constexpr uint32_t block_size() noexcept { return 128; }
  static_assert(block_size() <= doc_limits::eof());
  // initial base value for writing positions offsets
  static constexpr uint32_t pos_min() noexcept { return PosMin; }
  static constexpr bool wand() noexcept { return Wand; }

  IRS_FORCE_INLINE static void pack_block(const uint32_t* IRS_RESTRICT decoded,
                                          uint32_t* IRS_RESTRICT encoded,
                                          uint32_t bits) noexcept {
    packed::pack_block(decoded, encoded, bits);
    packed::pack_block(decoded + packed::BLOCK_SIZE_32, encoded + bits, bits);
    packed::pack_block(decoded + 2 * packed::BLOCK_SIZE_32, encoded + 2 * bits,
                       bits);
    packed::pack_block(decoded + 3 * packed::BLOCK_SIZE_32, encoded + 3 * bits,
                       bits);
  }

  IRS_FORCE_INLINE static void unpack_block(
    uint32_t* IRS_RESTRICT decoded, const uint32_t* IRS_RESTRICT encoded,
    uint32_t bits) noexcept {
    packed::unpack_block(encoded, decoded, bits);
    packed::unpack_block(encoded + bits, decoded + packed::BLOCK_SIZE_32, bits);
    packed::unpack_block(encoded + 2 * bits,
                         decoded + 2 * packed::BLOCK_SIZE_32, bits);
    packed::unpack_block(encoded + 3 * bits,
                         decoded + 3 * packed::BLOCK_SIZE_32, bits);
  }

  IRS_FORCE_INLINE static void write_block(index_output& out,
                                           const uint32_t* in, uint32_t* buf) {
    bitpack::write_block32<block_size()>(pack_block, out, in, buf);
  }

  IRS_FORCE_INLINE static void read_block(index_input& in, uint32_t* buf,
                                          uint32_t* out) {
    bitpack::read_block32<block_size()>(unpack_block, in, buf, out);
  }

  IRS_FORCE_INLINE static void skip_block(index_input& in) {
    bitpack::skip_block32(in, block_size());
  }
};

template<typename T, typename M>
std::string file_name(const M& meta);

void prepare_output(std::string& str, index_output::ptr& out,
                    const flush_state& state, std::string_view ext,
                    std::string_view format, const int32_t version) {
  IRS_ASSERT(!out);
  irs::file_name(str, state.name, ext);
  out = state.dir->create(str);

  if (!out) {
    throw io_error{absl::StrCat("Failed to create file, path: ", str)};
  }

  format_utils::write_header(*out, format, version);
}

void prepare_input(std::string& str, index_input::ptr& in, IOAdvice advice,
                   const ReaderState& state, std::string_view ext,
                   std::string_view format, const int32_t min_ver,
                   const int32_t max_ver) {
  IRS_ASSERT(!in);
  irs::file_name(str, state.meta->name, ext);
  in = state.dir->open(str, advice);

  if (!in) {
    throw io_error{absl::StrCat("Failed to open file, path: ", str)};
  }

  format_utils::check_header(*in, format, min_ver, max_ver);
}

// Buffer for storing skip data
struct skip_buffer {
  explicit skip_buffer(uint64_t* skip_ptr) noexcept : skip_ptr{skip_ptr} {}

  void reset() noexcept { start = end = 0; }

  uint64_t* skip_ptr;  // skip data
  uint64_t start{};    // start position of block
  uint64_t end{};      // end position of block
};

// Buffer for storing doc data
struct doc_buffer : skip_buffer {
  doc_buffer(std::span<doc_id_t>& docs, std::span<uint32_t>& freqs,
             doc_id_t* skip_doc, uint64_t* skip_ptr) noexcept
    : skip_buffer{skip_ptr}, docs{docs}, freqs{freqs}, skip_doc{skip_doc} {}

  bool full() const noexcept { return doc == std::end(docs); }

  bool empty() const noexcept { return doc == std::begin(docs); }

  void push(doc_id_t doc, uint32_t freq) noexcept {
    *this->doc = doc;
    ++this->doc;
    *this->freq = freq;
    ++this->freq;
    last = doc;
  }

  std::span<doc_id_t> docs;
  std::span<uint32_t> freqs;
  uint32_t* skip_doc;
  std::span<doc_id_t>::iterator doc{docs.begin()};
  std::span<uint32_t>::iterator freq{freqs.begin()};
  doc_id_t last{doc_limits::invalid()};    // last buffered document id
  doc_id_t block_last{doc_limits::min()};  // last document id in a block
};

// Buffer for storing positions
struct pos_buffer : skip_buffer {
  explicit pos_buffer(std::span<uint32_t> buf, uint64_t* skip_ptr) noexcept
    : skip_buffer{skip_ptr}, buf{buf} {}

  bool full() const noexcept { return buf.size() == size; }

  void next(uint32_t pos) noexcept {
    last = pos;
    ++size;
  }

  void pos(uint32_t pos) noexcept { buf[size] = pos; }

  void reset() noexcept {
    skip_buffer::reset();
    last = 0;
    block_last = 0;
    size = 0;
  }

  std::span<uint32_t> buf;  // buffer to store position deltas
  uint32_t last{};          // last buffered position
  uint32_t block_last{};    // last position in a block
  uint32_t size{};          // number of buffered elements
};

// Buffer for storing payload data
struct pay_buffer : skip_buffer {
  pay_buffer(uint32_t* pay_sizes, uint32_t* offs_start_buf,
             uint32_t* offs_len_buf, uint64_t* skip_ptr) noexcept
    : skip_buffer{skip_ptr},
      pay_sizes{pay_sizes},
      offs_start_buf{offs_start_buf},
      offs_len_buf{offs_len_buf} {}

  void push_payload(uint32_t i, bytes_view pay) {
    if (!pay.empty()) {
      pay_buf_.append(pay.data(), pay.size());
    }
    pay_sizes[i] = static_cast<uint32_t>(pay.size());
  }

  void push_offset(uint32_t i, uint32_t start, uint32_t end) noexcept {
    IRS_ASSERT(start >= last && start <= end);

    offs_start_buf[i] = start - last;
    offs_len_buf[i] = end - start;
    last = start;
  }

  void reset() noexcept {
    skip_buffer::reset();
    pay_buf_.clear();
    block_last = 0;
    last = 0;
  }

  uint32_t* pay_sizes;       // buffer to store payloads sizes
  uint32_t* offs_start_buf;  // buffer to store start offsets
  uint32_t* offs_len_buf;    // buffer to store offset lengths
  bstring pay_buf_;          // buffer for payload
  size_t block_last{};       // last payload buffer length in a block
  uint32_t last{};           // last start offset
};

std::vector<irs::WandWriter::ptr> PrepareWandWriters(ScorersView scorers,
                                                     size_t max_levels) {
  std::vector<irs::WandWriter::ptr> writers(
    std::min(scorers.size(), kMaxScorers));
  auto scorer = scorers.begin();
  for (auto& writer : writers) {
    writer = (*scorer)->prepare_wand_writer(max_levels);
    ++scorer;
  }
  return writers;
}

enum class TermsFormat : int32_t { MIN = 0, MAX = MIN };

enum class PostingsFormat : int32_t {
  MIN = 0,

  // positions are stored one based (if first osition is 1 first offset is 0)
  // This forces reader to adjust first read position of every document
  // additionally to stored increment. Or incorrect positions will be read - 1 2
  // 3 will be stored (offsets 0 1 1) but 0 1 2 will be read. At least this will
  // lead to incorrect results in by_same_positions filter if searching for
  // position 1
  POSITIONS_ONEBASED = MIN,

  // positions are stored one based, sse used
  POSITIONS_ONEBASED_SSE,

  // positions are stored zero based
  // if first position is 1 first offset is also 1
  // so no need to adjust position while reading first
  // position for document, always just increment from previous pos
  POSITIONS_ZEROBASED,

  // positions are stored zero based, sse used
  POSITIONS_ZEROBASED_SSE,

  // store competitive scores in blocks
  WAND,

  // store block max scores, sse used
  WAND_SSE,

  MAX = WAND_SSE
};

// Assume that doc_count = 28, skip_n = skip_0 = 12
//
//  |       block#0       | |      block#1        | |vInts|
//  d d d d d d d d d d d d d d d d d d d d d d d d d d d d (posting list)
//                          ^                       ^       (level 0 skip point)
class postings_writer_base : public irs::postings_writer {
 public:
  static constexpr uint32_t kMaxSkipLevels = 9;
  static constexpr uint32_t kSkipN = 8;

  static constexpr std::string_view kDocFormatName =
    "iresearch_10_postings_documents";
  static constexpr std::string_view kDocExt = "doc";
  static constexpr std::string_view kPosFormatName =
    "iresearch_10_postings_positions";
  static constexpr std::string_view kPosExt = "pos";
  static constexpr std::string_view kPayFormatName =
    "iresearch_10_postings_payloads";
  static constexpr std::string_view kPayExt = "pay";
  static constexpr std::string_view kTermsFormatName =
    "iresearch_10_postings_terms";

 protected:
  postings_writer_base(doc_id_t block_size, std::span<doc_id_t> docs,
                       std::span<uint32_t> freqs, doc_id_t* skip_doc,
                       uint64_t* doc_skip_ptr, std::span<uint32_t> prox_buf,
                       uint64_t* prox_skip_ptr, uint32_t* pay_sizes,
                       uint32_t* offs_start_buf, uint32_t* offs_len_buf,
                       uint64_t* pay_skip_ptr, uint32_t* enc_buf,
                       PostingsFormat postings_format_version,
                       TermsFormat terms_format_version, IResourceManager& rm)
    : skip_{block_size, kSkipN, rm},
      doc_{docs, freqs, skip_doc, doc_skip_ptr},
      pos_{prox_buf, prox_skip_ptr},
      pay_{pay_sizes, offs_start_buf, offs_len_buf, pay_skip_ptr},
      buf_{enc_buf},
      postings_format_version_{postings_format_version},
      terms_format_version_{terms_format_version} {
    IRS_ASSERT(postings_format_version >= PostingsFormat::MIN &&
               postings_format_version <= PostingsFormat::MAX);
    IRS_ASSERT(terms_format_version >= TermsFormat::MIN &&
               terms_format_version <= TermsFormat::MAX);
  }

 public:
  void begin_field(IndexFeatures index_features,
                   const irs::feature_map_t& features) final {
    features_.Reset(index_features);
    PrepareWriters(features);
    docs_.clear();
    last_state_.clear();
  }

  FieldStats end_field() noexcept final {
    const auto count = docs_.count();
    IRS_ASSERT(count < doc_limits::eof());
    return {.wand_mask = writers_mask_,
            .docs_count = static_cast<doc_id_t>(count)};
  }

  void begin_block() final {
    // clear state in order to write
    // absolute address of the first
    // entry in the block
    last_state_.clear();
  }

  void prepare(index_output& out, const flush_state& state) final;
  void encode(data_output& out, const irs::term_meta& attrs) final;
  void end() final;

 protected:
  class Features {
   public:
    void Reset(IndexFeatures features) noexcept {
      has_freq_ = (IndexFeatures::NONE != (features & IndexFeatures::FREQ));
      has_pos_ = (IndexFeatures::NONE != (features & IndexFeatures::POS));
      has_offs_ = (IndexFeatures::NONE != (features & IndexFeatures::OFFS));
      has_pay_ = (IndexFeatures::NONE != (features & IndexFeatures::PAY));
    }

    bool HasFrequency() const noexcept { return has_freq_; }
    bool HasPosition() const noexcept { return has_pos_; }
    bool HasOffset() const noexcept { return has_offs_; }
    bool HasPayload() const noexcept { return has_pay_; }
    bool HasOffsetOrPayload() const noexcept { return has_offs_ | has_pay_; }

   private:
    bool has_freq_{};
    bool has_pos_{};
    bool has_offs_{};
    bool has_pay_{};
  };

  struct Attributes final : attribute_provider {
    irs::document doc_;
    irs::frequency freq_value_;

    const frequency* freq_{};
    irs::position* pos_{};
    const offset* offs_{};
    const payload* pay_{};

    attribute* get_mutable(irs::type_info::type_id type) noexcept final {
      if (type == irs::type<document>::id()) {
        return &doc_;
      }

      if (type == irs::type<frequency>::id()) {
        return const_cast<frequency*>(freq_);
      }

      return nullptr;
    }

    void reset(attribute_provider& attrs) noexcept {
      pos_ = &irs::position::empty();
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
  };

  void WriteSkip(size_t level, memory_index_output& out) const;
  void BeginTerm();
  void EndTerm(version10::term_meta& meta);
  void EndDocument();
  void PrepareWriters(const irs::feature_map_t& features);

  template<typename Func>
  void ApplyWriters(Func&& func) {
    for (auto* writer : valid_writers_) {
      func(*writer);
    }
  }

  SkipWriter skip_;
  version10::term_meta last_state_;  // Last final term state
  bitset docs_;                      // Set of all processed documents
  index_output::ptr doc_out_;        // Postings (doc + freq)
  index_output::ptr pos_out_;        // Positions
  index_output::ptr pay_out_;        // Payload (pay + offs)
  doc_buffer doc_;                   // Document stream
  pos_buffer pos_;                   // Proximity stream
  pay_buffer pay_;                   // Payloads and offsets stream
  uint32_t* buf_;                    // Buffer for encoding
  Attributes attrs_;                 // Set of attributes
  const ColumnProvider* columns_{};
  std::vector<irs::WandWriter::ptr> writers_;    // List of wand writers
  std::vector<irs::WandWriter*> valid_writers_;  // Valid wand writers
  uint64_t writers_mask_{};
  Features features_;  // Features supported by current field
  const PostingsFormat postings_format_version_;
  const TermsFormat terms_format_version_;
};

void postings_writer_base::PrepareWriters(const irs::feature_map_t& features) {
  writers_mask_ = 0;
  valid_writers_.clear();

  if (IRS_UNLIKELY(!columns_)) {
    return;
  }

  // Make frequency avaliable for Prepare
  attrs_.freq_ = features_.HasFrequency() ? &attrs_.freq_value_ : nullptr;

  for (size_t i = 0; auto& writer : writers_) {
    const bool valid = writer && writer->Prepare(*columns_, features, attrs_);
    if (valid) {
      irs::set_bit(writers_mask_, i);
      valid_writers_.emplace_back(writer.get());
    }
    ++i;
  }
}

void postings_writer_base::WriteSkip(size_t level,
                                     memory_index_output& out) const {
  const doc_id_t doc_delta = doc_.block_last;  //- doc_.skip_doc[level];
  const uint64_t doc_ptr = doc_out_->file_pointer();

  out.write_vint(doc_delta);
  out.write_vlong(doc_ptr - doc_.skip_ptr[level]);

  doc_.skip_doc[level] = doc_.block_last;
  doc_.skip_ptr[level] = doc_ptr;

  if (features_.HasPosition()) {
    const uint64_t pos_ptr = pos_out_->file_pointer();

    out.write_vint(pos_.block_last);
    out.write_vlong(pos_ptr - pos_.skip_ptr[level]);

    pos_.skip_ptr[level] = pos_ptr;

    if (features_.HasOffsetOrPayload()) {
      IRS_ASSERT(pay_out_);

      if (features_.HasPayload()) {
        out.write_vint(static_cast<uint32_t>(pay_.block_last));
      }

      const uint64_t pay_ptr = pay_out_->file_pointer();

      out.write_vlong(pay_ptr - pay_.skip_ptr[level]);
      pay_.skip_ptr[level] = pay_ptr;
    }
  }
}

void postings_writer_base::prepare(index_output& out,
                                   const flush_state& state) {
  IRS_ASSERT(state.dir);
  IRS_ASSERT(!IsNull(state.name));

  std::string name;

  // Prepare document stream
  prepare_output(name, doc_out_, state, kDocExt, kDocFormatName,
                 static_cast<int32_t>(postings_format_version_));

  if (IndexFeatures::NONE != (state.index_features & IndexFeatures::POS)) {
    // Prepare proximity stream
    pos_.reset();
    prepare_output(name, pos_out_, state, kPosExt, kPosFormatName,
                   static_cast<int32_t>(postings_format_version_));

    if (IndexFeatures::NONE !=
        (state.index_features & (IndexFeatures::PAY | IndexFeatures::OFFS))) {
      // Prepare payload stream
      pay_.reset();
      prepare_output(name, pay_out_, state, kPayExt, kPayFormatName,
                     static_cast<int32_t>(postings_format_version_));
    }
  }

  skip_.Prepare(kMaxSkipLevels, state.doc_count);

  format_utils::write_header(out, kTermsFormatName,
                             static_cast<int32_t>(terms_format_version_));
  out.write_vint(skip_.Skip0());  // Write postings block size

  // Prepare wand writers
  writers_ = PrepareWandWriters(state.scorers, kMaxSkipLevels);
  valid_writers_.reserve(writers_.size());
  columns_ = state.columns;

  // Prepare documents bitset
  docs_.reset(doc_limits::min() + state.doc_count);
}

void postings_writer_base::encode(data_output& out,
                                  const irs::term_meta& state) {
  const auto& meta = static_cast<const version10::term_meta&>(state);

  out.write_vint(meta.docs_count);
  if (meta.freq) {
    IRS_ASSERT(meta.freq >= meta.docs_count);
    out.write_vint(meta.freq - meta.docs_count);
  }

  out.write_vlong(meta.doc_start - last_state_.doc_start);
  if (features_.HasPosition()) {
    out.write_vlong(meta.pos_start - last_state_.pos_start);
    if (address_limits::valid(meta.pos_end)) {
      out.write_vlong(meta.pos_end);
    }
    if (features_.HasOffsetOrPayload()) {
      out.write_vlong(meta.pay_start - last_state_.pay_start);
    }
  }

  if (1 == meta.docs_count) {
    out.write_vint(meta.e_single_doc);
  } else if (skip_.Skip0() < meta.docs_count) {
    out.write_vlong(meta.e_skip_start);
  }

  last_state_ = meta;
}

void postings_writer_base::end() {
  format_utils::write_footer(*doc_out_);
  doc_out_.reset();  // ensure stream is closed

  if (pos_out_) {
    format_utils::write_footer(*pos_out_);
    pos_out_.reset();  // ensure stream is closed
  }

  if (pay_out_) {
    format_utils::write_footer(*pay_out_);
    pay_out_.reset();  // ensure stream is closed
  }
}

void postings_writer_base::BeginTerm() {
  doc_.start = doc_out_->file_pointer();
  std::fill_n(doc_.skip_ptr, kMaxSkipLevels, doc_.start);
  if (features_.HasPosition()) {
    IRS_ASSERT(pos_out_);
    pos_.start = pos_out_->file_pointer();
    std::fill_n(pos_.skip_ptr, kMaxSkipLevels, pos_.start);
    if (features_.HasOffsetOrPayload()) {
      IRS_ASSERT(pay_out_);
      pay_.start = pay_out_->file_pointer();
      std::fill_n(pay_.skip_ptr, kMaxSkipLevels, pay_.start);
    }
  }

  doc_.last = doc_limits::invalid();
  doc_.block_last = doc_limits::min();
  skip_.Reset();
}

void postings_writer_base::EndDocument() {
  if (doc_.full()) {
    doc_.block_last = doc_.last;
    doc_.end = doc_out_->file_pointer();
    if (features_.HasPosition()) {
      IRS_ASSERT(pos_out_);
      pos_.end = pos_out_->file_pointer();
      // documents stream is full, but positions stream is not
      // save number of positions to skip before the next block
      pos_.block_last = pos_.size;
      if (features_.HasOffsetOrPayload()) {
        IRS_ASSERT(pay_out_);
        pay_.end = pay_out_->file_pointer();
        pay_.block_last = pay_.pay_buf_.size();
      }
    }

    doc_.doc = doc_.docs.begin();
    doc_.freq = doc_.freqs.begin();
  }
}

void postings_writer_base::EndTerm(version10::term_meta& meta) {
  if (meta.docs_count == 0) {
    return;  // no documents to write
  }

  const bool has_skip_list = skip_.Skip0() < meta.docs_count;
  auto write_max_score = [&](size_t level) {
    ApplyWriters([&](auto& writer) {
      const uint8_t size = writer.SizeRoot(level);
      doc_out_->write_byte(size);
    });
    ApplyWriters([&](auto& writer) { writer.WriteRoot(level, *doc_out_); });
  };

  if (1 == meta.docs_count) {
    meta.e_single_doc = doc_.docs[0] - doc_limits::min();
  } else {
    // write remaining documents using
    // variable length encoding
    auto& out = *doc_out_;
    auto doc = doc_.docs.begin();
    auto prev = doc_.block_last;

    if (!has_skip_list) {
      write_max_score(0);
    }
    // TODO(MBkkt) using bits not full block encoding
    if (features_.HasFrequency()) {
      auto doc_freq = doc_.freqs.begin();
      for (; doc < doc_.doc; ++doc) {
        const uint32_t freq = *doc_freq;
        const doc_id_t delta = *doc - prev;

        if (1 == freq) {
          out.write_vint(shift_pack_32(delta, true));
        } else {
          out.write_vint(shift_pack_32(delta, false));
          out.write_vint(freq);
        }

        ++doc_freq;
        prev = *doc;
      }
    } else {
      for (; doc < doc_.doc; ++doc) {
        out.write_vint(*doc - prev);
        prev = *doc;
      }
    }
  }

  meta.pos_end = address_limits::invalid();

  // write remaining position using
  // variable length encoding
  if (features_.HasPosition()) {
    IRS_ASSERT(pos_out_);

    if (meta.freq > skip_.Skip0()) {
      meta.pos_end = pos_out_->file_pointer() - pos_.start;
    }

    if (pos_.size > 0) {
      data_output& out = *pos_out_;
      uint32_t last_pay_size = std::numeric_limits<uint32_t>::max();
      uint32_t last_offs_len = std::numeric_limits<uint32_t>::max();
      uint32_t pay_buf_start = 0;
      for (uint32_t i = 0; i < pos_.size; ++i) {
        const uint32_t pos_delta = pos_.buf[i];
        if (features_.HasPayload()) {
          IRS_ASSERT(pay_out_);

          const uint32_t size = pay_.pay_sizes[i];
          if (last_pay_size != size) {
            last_pay_size = size;
            out.write_vint(shift_pack_32(pos_delta, true));
            out.write_vint(size);
          } else {
            out.write_vint(shift_pack_32(pos_delta, false));
          }

          if (size != 0) {
            out.write_bytes(pay_.pay_buf_.c_str() + pay_buf_start, size);
            pay_buf_start += size;
          }
        } else {
          out.write_vint(pos_delta);
        }

        if (features_.HasOffset()) {
          IRS_ASSERT(pay_out_);

          const uint32_t pay_offs_delta = pay_.offs_start_buf[i];
          const uint32_t len = pay_.offs_len_buf[i];
          if (len == last_offs_len) {
            out.write_vint(shift_pack_32(pay_offs_delta, false));
          } else {
            out.write_vint(shift_pack_32(pay_offs_delta, true));
            out.write_vint(len);
            last_offs_len = len;
          }
        }
      }

      if (features_.HasPayload()) {
        IRS_ASSERT(pay_out_);
        pay_.pay_buf_.clear();
      }
    }
  }

  // if we have flushed at least
  // one block there was buffered
  // skip data, so we need to flush it
  if (has_skip_list) {
    meta.e_skip_start = doc_out_->file_pointer() - doc_.start;
    const auto num_levels = skip_.CountLevels();
    write_max_score(num_levels);
    skip_.FlushLevels(num_levels, *doc_out_);
  }

  doc_.doc = doc_.docs.begin();
  doc_.freq = doc_.freqs.begin();
  doc_.last = doc_limits::invalid();
  meta.doc_start = doc_.start;

  if (pos_out_) {
    pos_.size = 0;
    meta.pos_start = pos_.start;
  }

  if (pay_out_) {
    pay_.pay_buf_.clear();
    pay_.last = 0;
    meta.pay_start = pay_.start;
  }
}

template<typename FormatTraits>
class postings_writer final : public postings_writer_base {
 public:
  explicit postings_writer(PostingsFormat version, bool volatile_attributes,
                           IResourceManager& rm)
    : postings_writer_base{FormatTraits::block_size(),
                           std::span{doc_buf_.docs},
                           std::span{doc_buf_.freqs},
                           doc_buf_.skip_doc,
                           doc_buf_.skip_ptr,
                           std::span{prox_buf_.buf},
                           prox_buf_.skip_ptr,
                           pay_buf_.pay_sizes,
                           pay_buf_.offs_start_buf,
                           pay_buf_.offs_len_buf,
                           pay_buf_.skip_ptr,
                           encbuf_.buf,
                           version,
                           TermsFormat::MAX,
                           rm},
      volatile_attributes_{volatile_attributes} {
    IRS_ASSERT((postings_format_version_ >= PostingsFormat::POSITIONS_ZEROBASED
                  ? pos_limits::invalid()
                  : pos_limits::min()) == FormatTraits::pos_min());
  }

  void write(irs::doc_iterator& docs, irs::term_meta& base_meta) final;

 private:
  void AddPosition(uint32_t pos);
  void BeginDocument();

  struct {
    // Buffer for document deltas
    doc_id_t docs[FormatTraits::block_size()]{};
    // Buffer for frequencies
    uint32_t freqs[FormatTraits::block_size()]{};
    // Buffer for skip documents
    doc_id_t skip_doc[kMaxSkipLevels]{};
    // Buffer for skip pointers
    uint64_t skip_ptr[kMaxSkipLevels]{};
  } doc_buf_;
  struct {
    // Buffer for position deltas
    uint32_t buf[FormatTraits::block_size()]{};
    // Buffer for skip pointers
    uint64_t skip_ptr[kMaxSkipLevels]{};
  } prox_buf_;
  struct {
    // Buffer for payloads sizes
    uint32_t pay_sizes[FormatTraits::block_size()]{};
    // Buffer for start offsets
    uint32_t offs_start_buf[FormatTraits::block_size()]{};
    // Buffer for offset lengths
    uint32_t offs_len_buf[FormatTraits::block_size()]{};
    // Buffer for skip pointers
    uint64_t skip_ptr[kMaxSkipLevels]{};
  } pay_buf_;
  struct {
    // Buffer for encoding (worst case)
    uint32_t buf[FormatTraits::block_size()];
  } encbuf_;
  bool volatile_attributes_;
};

template<typename FormatTraits>
void postings_writer<FormatTraits>::BeginDocument() {
  if (const auto id = attrs_.doc_.value; IRS_LIKELY(doc_.last < id)) {
    doc_.push(id, attrs_.freq_value_.value);

    if (doc_.full()) {
      // FIXME do aligned?
      simd::delta_encode<FormatTraits::block_size(), false>(doc_.docs.data(),
                                                            doc_.block_last);
      FormatTraits::write_block(*doc_out_, doc_.docs.data(), buf_);
      if (features_.HasFrequency()) {
        FormatTraits::write_block(*doc_out_, doc_.freqs.data(), buf_);
      }
    }

    docs_.set(id);

    // First position offsets now is format dependent
    pos_.last = FormatTraits::pos_min();
    pay_.last = 0;
  } else {
    throw index_error{
      absl::StrCat("While beginning document in postings_writer, error: "
                   "docs out of order '",
                   id, "' < '", doc_.last, "'")};
  }
}

template<typename FormatTraits>
void postings_writer<FormatTraits>::AddPosition(uint32_t pos) {
  // at least positions stream should be created
  IRS_ASSERT(features_.HasPosition() && pos_out_);
  IRS_ASSERT(!attrs_.offs_ || attrs_.offs_->start <= attrs_.offs_->end);

  pos_.pos(pos - pos_.last);

  if (attrs_.pay_) {
    pay_.push_payload(pos_.size, attrs_.pay_->value);
  }

  if (attrs_.offs_) {
    pay_.push_offset(pos_.size, attrs_.offs_->start, attrs_.offs_->end);
  }

  pos_.next(pos);

  if (pos_.full()) {
    FormatTraits::write_block(*pos_out_, pos_.buf.data(), buf_);
    pos_.size = 0;

    if (features_.HasPayload()) {
      IRS_ASSERT(pay_out_);
      auto& pay_buf = pay_.pay_buf_;

      pay_out_->write_vint(static_cast<uint32_t>(pay_buf.size()));
      if (!pay_buf.empty()) {
        FormatTraits::write_block(*pay_out_, pay_.pay_sizes, buf_);
        pay_out_->write_bytes(pay_buf.c_str(), pay_buf.size());
        pay_buf.clear();
      }
    }

    if (features_.HasOffset()) {
      IRS_ASSERT(pay_out_);
      FormatTraits::write_block(*pay_out_, pay_.offs_start_buf, buf_);
      FormatTraits::write_block(*pay_out_, pay_.offs_len_buf, buf_);
    }
  }
}

#if defined(_MSC_VER)
#pragma warning(disable : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

template<typename FormatTraits>
void postings_writer<FormatTraits>::write(irs::doc_iterator& docs,
                                          irs::term_meta& base_meta) {
  REGISTER_TIMER_DETAILED();

  const auto* doc = irs::get<document>(docs);

  if (IRS_UNLIKELY(!doc)) {
    IRS_ASSERT(false);
    throw illegal_argument{"'document' attribute is missing"};
  }

  auto refresh = [this, no_freq = frequency{}](auto& attrs) noexcept {
    attrs_.reset(attrs);
    if (!attrs_.freq_) {
      attrs_.freq_ = &no_freq;
    }
  };

  if (!volatile_attributes_) {
    refresh(docs);
  } else {
    auto* subscription = irs::get<attribute_provider_change>(docs);
    IRS_ASSERT(subscription);

    subscription->subscribe(
      [refresh](attribute_provider& attrs) { refresh(attrs); });
  }

  auto& meta = static_cast<version10::term_meta&>(base_meta);

  BeginTerm();
  if constexpr (FormatTraits::wand()) {
    ApplyWriters([&](auto& writer) { writer.Reset(); });
  }

  uint32_t docs_count = 0;
  uint32_t total_freq = 0;

  while (docs.next()) {
    IRS_ASSERT(doc_limits::valid(doc->value));
    IRS_ASSERT(attrs_.freq_);
    attrs_.doc_.value = doc->value;
    attrs_.freq_value_.value = attrs_.freq_->value;

    if (doc_limits::valid(doc_.last) && doc_.empty()) {
      skip_.Skip(docs_count, [this](size_t level, memory_index_output& out) {
        WriteSkip(level, out);

        if constexpr (FormatTraits::wand()) {
          // FIXME(gnusi): optimize for 1 writer case? compile? maybe just 1
          // composite wand writer?
          ApplyWriters([&](auto& writer) {
            const uint8_t size = writer.Size(level);
            IRS_ASSERT(size <= irs::WandWriter::kMaxSize);
            out.write_byte(size);
          });
          ApplyWriters([&](auto& writer) { writer.Write(level, out); });
        }
      });
    }

    BeginDocument();
    if constexpr (FormatTraits::wand()) {
      ApplyWriters([](auto& writer) { writer.Update(); });
    } else {
      IRS_ASSERT(valid_writers_.empty());
    }
    IRS_ASSERT(attrs_.pos_);
    while (attrs_.pos_->next()) {
      IRS_ASSERT(pos_limits::valid(attrs_.pos_->value()));
      AddPosition(attrs_.pos_->value());
    }
    ++docs_count;
    total_freq += attrs_.freq_value_.value;
    EndDocument();
  }

  // FIXME(gnusi): do we need to write terminal skip if present?

  meta.docs_count = docs_count;
  meta.freq = total_freq;
  EndTerm(meta);
}

#if defined(_MSC_VER)
#pragma warning(default : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

struct SkipState {
  // pointer to the payloads of the first document in a document block
  uint64_t pay_ptr{};
  // pointer to the positions of the first document in a document block
  uint64_t pos_ptr{};
  // positions to skip before new document block
  size_t pend_pos{};
  // pointer to the beginning of document block
  uint64_t doc_ptr{};
  // last document in a previous block
  doc_id_t doc{doc_limits::invalid()};
  // payload size to skip before in new document block
  uint32_t pay_pos{};
};

template<typename IteratorTraits>
IRS_FORCE_INLINE void CopyState(SkipState& to, const SkipState& from) noexcept {
  if constexpr (IteratorTraits::position() &&
                (IteratorTraits::payload() || IteratorTraits::offset())) {
    to = from;
  } else {
    if constexpr (IteratorTraits::position()) {
      to.pos_ptr = from.pos_ptr;
      to.pend_pos = from.pend_pos;
    }
    to.doc_ptr = from.doc_ptr;
    to.doc = from.doc;
  }
}

template<typename FieldTraits>
IRS_FORCE_INLINE void ReadState(SkipState& state, index_input& in) {
  state.doc = in.read_vint();
  state.doc_ptr += in.read_vlong();

  if constexpr (FieldTraits::position()) {
    state.pend_pos = in.read_vint();
    state.pos_ptr += in.read_vlong();

    if constexpr (FieldTraits::payload() || FieldTraits::offset()) {
      if constexpr (FieldTraits::payload()) {
        state.pay_pos = in.read_vint();
      }

      state.pay_ptr += in.read_vlong();
    }
  }
}

struct DocState {
  const index_input* pos_in;
  const index_input* pay_in;
  const version10::term_meta* term_state;
  const uint32_t* freq;
  uint32_t* enc_buf;
  uint64_t tail_start;
  size_t tail_length;
};

template<typename IteratorTraits>
IRS_FORCE_INLINE void CopyState(SkipState& to,
                                const version10::term_meta& from) noexcept {
  to.doc_ptr = from.doc_start;
  if constexpr (IteratorTraits::position()) {
    to.pos_ptr = from.pos_start;
    if constexpr (IteratorTraits::payload() || IteratorTraits::offset()) {
      to.pay_ptr = from.pay_start;
    }
  }
}

template<typename IteratorTraits, typename FieldTraits,
         bool Offset = IteratorTraits::offset(),
         bool Payload = IteratorTraits::payload()>
struct position_impl;

// Implementation of iterator over positions, payloads and offsets
template<typename IteratorTraits, typename FieldTraits>
struct position_impl<IteratorTraits, FieldTraits, true, true>
  : public position_impl<IteratorTraits, FieldTraits, false, false> {
  using base = position_impl<IteratorTraits, FieldTraits, false, false>;

  irs::attribute* attribute(irs::type_info::type_id type) noexcept {
    if (irs::type<payload>::id() == type) {
      return &pay_;
    }

    return irs::type<offset>::id() == type ? &offs_ : nullptr;
  }

  void prepare(const DocState& state) {
    base::prepare(state);

    pay_in_ = state.pay_in->reopen();  // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen payload input in");

      throw io_error{"failed to reopen payload input"};
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  void prepare(const SkipState& state) {
    base::prepare(state);

    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

  void read_attributes() noexcept {
    offs_.start += offs_start_deltas_[this->buf_pos_];
    offs_.end = offs_.start + offs_lengts_[this->buf_pos_];

    pay_.value = bytes_view(pay_data_.c_str() + pay_data_pos_,
                            pay_lengths_[this->buf_pos_]);
    pay_data_pos_ += pay_lengths_[this->buf_pos_];
  }

  void clear_attributes() noexcept {
    offs_.clear();
    pay_.value = {};
  }

  void read_block() {
    base::read_block();

    // read payload
    const uint32_t size = pay_in_->read_vint();
    if (size) {
      IteratorTraits::read_block(*pay_in_, this->enc_buf_, pay_lengths_);
      pay_data_.resize(size);

      [[maybe_unused]] const auto read =
        pay_in_->read_bytes(pay_data_.data(), size);
      IRS_ASSERT(read == size);
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
        IRS_ASSERT(i);
        pay_lengths_[i] = pay_lengths_[i - 1];
      }

      if (pay_lengths_[i]) {
        const auto size = pay_lengths_[i];  // length of current payload
        pay_data_.resize(pos + size);  // FIXME(gnusi): use oversize from absl

        [[maybe_unused]] const auto read =
          this->pos_in_->read_bytes(pay_data_.data() + pos, size);
        IRS_ASSERT(read == size);

        pos += size;
      }

      if (shift_unpack_32(this->pos_in_->read_vint(), offs_start_deltas_[i])) {
        offs_lengts_[i] = this->pos_in_->read_vint();
      } else {
        IRS_ASSERT(i);
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

  uint32_t
    offs_start_deltas_[IteratorTraits::block_size()]{};   // buffer to store
                                                          // offset starts
  uint32_t offs_lengts_[IteratorTraits::block_size()]{};  // buffer to store
                                                          // offset lengths
  uint32_t pay_lengths_[IteratorTraits::block_size()]{};  // buffer to store
                                                          // payload lengths
  index_input::ptr pay_in_;
  offset offs_;
  payload pay_;
  size_t pay_data_pos_{};  // current position in a payload buffer
  bstring pay_data_;       // buffer to store payload data
};

// Implementation of iterator over positions and payloads
template<typename IteratorTraits, typename FieldTraits>
struct position_impl<IteratorTraits, FieldTraits, false, true>
  : public position_impl<IteratorTraits, FieldTraits, false, false> {
  using base = position_impl<IteratorTraits, FieldTraits, false, false>;

  irs::attribute* attribute(irs::type_info::type_id type) noexcept {
    return irs::type<payload>::id() == type ? &pay_ : nullptr;
  }

  void prepare(const DocState& state) {
    base::prepare(state);

    pay_in_ = state.pay_in->reopen();  // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen payload input");

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  void prepare(const SkipState& state) {
    base::prepare(state);

    pay_in_->seek(state.pay_ptr);
    pay_data_pos_ = state.pay_pos;
  }

  void read_attributes() noexcept {
    pay_.value = bytes_view(pay_data_.c_str() + pay_data_pos_,
                            pay_lengths_[this->buf_pos_]);
    pay_data_pos_ += pay_lengths_[this->buf_pos_];
  }

  void clear_attributes() noexcept { pay_.value = {}; }

  void read_block() {
    base::read_block();

    // read payload
    const uint32_t size = pay_in_->read_vint();
    if (size) {
      IteratorTraits::read_block(*pay_in_, this->enc_buf_, pay_lengths_);
      pay_data_.resize(size);

      [[maybe_unused]] const auto read =
        pay_in_->read_bytes(pay_data_.data(), size);
      IRS_ASSERT(read == size);
    }

    if constexpr (FieldTraits::offset()) {
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
        IRS_ASSERT(i);
        pay_lengths_[i] = pay_lengths_[i - 1];
      }

      if (pay_lengths_[i]) {
        const auto size = pay_lengths_[i];  // current payload length
        pay_data_.resize(pos + size);  // FIXME(gnusi): use oversize from absl

        [[maybe_unused]] const auto read =
          this->pos_in_->read_bytes(pay_data_.data() + pos, size);
        IRS_ASSERT(read == size);

        pos += size;
      }

      // skip offsets
      if constexpr (FieldTraits::offset()) {
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
    if constexpr (FieldTraits::offset()) {
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

  uint32_t pay_lengths_[IteratorTraits::block_size()]{};  // buffer to store
                                                          // payload lengths
  index_input::ptr pay_in_;
  payload pay_;
  size_t pay_data_pos_{};  // current position in a payload buffer
  bstring pay_data_;       // buffer to store payload data
};

// Implementation of iterator over positions and offsets
template<typename IteratorTraits, typename FieldTraits>
struct position_impl<IteratorTraits, FieldTraits, true, false>
  : public position_impl<IteratorTraits, FieldTraits, false, false> {
  using base = position_impl<IteratorTraits, FieldTraits, false, false>;

  irs::attribute* attribute(irs::type_info::type_id type) noexcept {
    return irs::type<offset>::id() == type ? &offs_ : nullptr;
  }

  void prepare(const DocState& state) {
    base::prepare(state);

    pay_in_ = state.pay_in->reopen();  // reopen thread-safe stream

    if (!pay_in_) {
      // implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen payload input");

      throw io_error("failed to reopen payload input");
    }

    pay_in_->seek(state.term_state->pay_start);
  }

  void prepare(const SkipState& state) {
    base::prepare(state);

    pay_in_->seek(state.pay_ptr);
  }

  void read_attributes() noexcept {
    offs_.start += offs_start_deltas_[this->buf_pos_];
    offs_.end = offs_.start + offs_lengts_[this->buf_pos_];
  }

  void clear_attributes() noexcept { offs_.clear(); }

  void read_block() {
    base::read_block();

    if constexpr (FieldTraits::payload()) {
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
      if constexpr (FieldTraits::payload()) {
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
        IRS_ASSERT(i);
        offs_lengts_[i] = offs_lengts_[i - 1];
      }
    }
  }

  void skip_block() {
    base::skip_block();
    if constexpr (FieldTraits::payload()) {
      base::skip_payload(*pay_in_);
    }
    base::skip_offsets(*pay_in_);
  }

  uint32_t
    offs_start_deltas_[IteratorTraits::block_size()]{};   // buffer to store
                                                          // offset starts
  uint32_t offs_lengts_[IteratorTraits::block_size()]{};  // buffer to store
                                                          // offset lengths
  index_input::ptr pay_in_;
  offset offs_;
};

// Implementation of iterator over positions
template<typename IteratorTraits, typename FieldTraits>
struct position_impl<IteratorTraits, FieldTraits, false, false> {
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

  void prepare(const DocState& state) {
    pos_in_ = state.pos_in->reopen();  // reopen thread-safe stream

    if (!pos_in_) {
      // implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen positions input");

      throw io_error("failed to reopen positions input");
    }

    cookie_.file_pointer_ = state.term_state->pos_start;
    pos_in_->seek(state.term_state->pos_start);
    freq_ = state.freq;
    enc_buf_ = state.enc_buf;
    tail_start_ = state.tail_start;
    tail_length_ = state.tail_length;
  }

  void prepare(const SkipState& state) {
    pos_in_->seek(state.pos_ptr);
    pend_pos_ = state.pend_pos;
    buf_pos_ = IteratorTraits::block_size();
    cookie_.file_pointer_ = state.pos_ptr;
    cookie_.pend_pos_ = pend_pos_;
  }

  void reset() {
    if (std::numeric_limits<size_t>::max() != cookie_.file_pointer_) {
      buf_pos_ = IteratorTraits::block_size();
      pend_pos_ = cookie_.pend_pos_;
      pos_in_->seek(cookie_.file_pointer_);
    }
  }

  void read_attributes() {}

  void clear_attributes() {}

  void read_tail_block() {
    uint32_t pay_size = 0;
    for (size_t i = 0; i < tail_length_; ++i) {
      if constexpr (FieldTraits::payload()) {
        if (shift_unpack_32(pos_in_->read_vint(), pos_deltas_[i])) {
          pay_size = pos_in_->read_vint();
        }
        if (pay_size) {
          pos_in_->seek(pos_in_->file_pointer() + pay_size);
        }
      } else {
        pos_deltas_[i] = pos_in_->read_vint();
      }

      if constexpr (FieldTraits::offset()) {
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

  void skip_block() { IteratorTraits::skip_block(*pos_in_); }

  // skip within a block
  void skip(size_t count) noexcept { buf_pos_ += count; }

  struct cookie {
    size_t pend_pos_{};
    size_t file_pointer_ = std::numeric_limits<size_t>::max();
  };

  uint32_t pos_deltas_[IteratorTraits::block_size()];  // buffer to store
                                                       // position deltas
  const uint32_t* freq_;  // lenght of the posting list for a document
  uint32_t* enc_buf_;     // auxillary buffer to decode data
  size_t pend_pos_{};     // how many positions "behind" we are
  uint64_t tail_start_;  // file pointer where the last (vInt encoded) pos delta
                         // block is
  size_t tail_length_;   // number of positions in the last (vInt encoded) pos
                         // delta block
  uint32_t buf_pos_{
    IteratorTraits::block_size()};  // current position in pos_deltas_ buffer
  cookie cookie_;
  index_input::ptr pos_in_;
};

template<typename IteratorTraits, typename FieldTraits,
         bool Position = IteratorTraits::position()>
class position final : public irs::position,
                       private position_impl<IteratorTraits, FieldTraits> {
 public:
  using impl = position_impl<IteratorTraits, FieldTraits>;

  irs::attribute* get_mutable(irs::type_info::type_id type) final {
    return impl::attribute(type);
  }

  value_t seek(value_t target) final {
    const uint32_t freq = *this->freq_;
    if (this->pend_pos_ > freq) {
      skip(static_cast<uint32_t>(this->pend_pos_ - freq));
      this->pend_pos_ = freq;
    }
    while (value_ < target && this->pend_pos_) {
      if (this->buf_pos_ == IteratorTraits::block_size()) {
        refill();
        this->buf_pos_ = 0;
      }
      if constexpr (IteratorTraits::one_based_position_storage()) {
        value_ += (uint32_t)(!pos_limits::valid(value_));
      }
      value_ += this->pos_deltas_[this->buf_pos_];
      IRS_ASSERT(irs::pos_limits::valid(value_));
      this->read_attributes();

      ++this->buf_pos_;
      --this->pend_pos_;
    }
    if (0 == this->pend_pos_ && value_ < target) {
      value_ = pos_limits::eof();
    }
    return value_;
  }

  bool next() final {
    if (0 == this->pend_pos_) {
      value_ = pos_limits::eof();

      return false;
    }

    const uint32_t freq = *this->freq_;

    if (this->pend_pos_ > freq) {
      skip(static_cast<uint32_t>(this->pend_pos_ - freq));
      this->pend_pos_ = freq;
    }

    if (this->buf_pos_ == IteratorTraits::block_size()) {
      refill();
      this->buf_pos_ = 0;
    }
    if constexpr (IteratorTraits::one_based_position_storage()) {
      value_ += (uint32_t)(!pos_limits::valid(value_));
    }
    value_ += this->pos_deltas_[this->buf_pos_];
    IRS_ASSERT(irs::pos_limits::valid(value_));
    this->read_attributes();

    ++this->buf_pos_;
    --this->pend_pos_;
    return true;
  }

  void reset() final {
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
    auto left = IteratorTraits::block_size() - this->buf_pos_;
    if (count >= left) {
      count -= left;
      while (count >= IteratorTraits::block_size()) {
        this->skip_block();
        count -= IteratorTraits::block_size();
      }
      refill();
      this->buf_pos_ = 0;
      left = IteratorTraits::block_size();
    }

    if (count < left) {
      impl::skip(count);
    }
    clear();
  }
};

// Empty iterator over positions
template<typename IteratorTraits, typename FieldTraits>
struct position<IteratorTraits, FieldTraits, false> : attribute {
  static constexpr std::string_view type_name() noexcept {
    return irs::position::type_name();
  }

  void prepare(const DocState&) noexcept {}
  void prepare(const SkipState&) noexcept {}
  void notify(uint32_t) noexcept {}
  void clear() noexcept {}
};

struct empty {};

// Buffer type containing only document buffer
template<typename IteratorTraits>
struct data_buffer {
  doc_id_t docs[IteratorTraits::block_size()]{};
};

// Buffer type containing both document and fequency buffers
template<typename IteratorTraits>
struct freq_buffer : data_buffer<IteratorTraits> {
  uint32_t freqs[IteratorTraits::block_size()];
};

template<typename IteratorTraits>
using buffer_type =
  std::conditional_t<IteratorTraits::frequency(), freq_buffer<IteratorTraits>,
                     data_buffer<IteratorTraits>>;

template<typename IteratorTraits, typename FieldTraits>
class doc_iterator_base : public irs::doc_iterator {
  static_assert((IteratorTraits::features() & FieldTraits::features()) ==
                IteratorTraits::features());
  void read_tail_block();

 protected:
  // returns current position in the document block 'docs_'
  doc_id_t relative_pos() noexcept {
    IRS_ASSERT(begin_ >= buf_.docs);
    return static_cast<doc_id_t>(begin_ - buf_.docs);
  }

  void refill();

  buffer_type<IteratorTraits> buf_;
  uint32_t enc_buf_[IteratorTraits::block_size()];  // buffer for encoding
  const doc_id_t* begin_{std::end(buf_.docs)};
  uint32_t* freq_{};  // pointer into docs_ to the frequency attribute value for
                      // the current doc
  index_input::ptr doc_in_;
  doc_id_t left_{};
};

template<typename IteratorTraits, typename FieldTraits>
void doc_iterator_base<IteratorTraits, FieldTraits>::refill() {
  if (IRS_LIKELY(left_ >= IteratorTraits::block_size())) {
    // read doc deltas
    IteratorTraits::read_block(*doc_in_, enc_buf_, buf_.docs);

    if constexpr (IteratorTraits::frequency()) {
      IteratorTraits::read_block(*doc_in_, enc_buf_, buf_.freqs);
    } else if constexpr (FieldTraits::frequency()) {
      IteratorTraits::skip_block(*doc_in_);
    }

    static_assert(std::size(decltype(buf_.docs){}) ==
                  IteratorTraits::block_size());
    begin_ = std::begin(buf_.docs);
    if constexpr (IteratorTraits::frequency()) {
      freq_ = buf_.freqs;
    }
    left_ -= IteratorTraits::block_size();
  } else {
    read_tail_block();
  }
}

template<typename IteratorTraits, typename FieldTraits>
void doc_iterator_base<IteratorTraits, FieldTraits>::read_tail_block() {
  auto* doc = std::end(buf_.docs) - left_;
  begin_ = doc;

  [[maybe_unused]] uint32_t* freq;
  if constexpr (IteratorTraits::frequency()) {
    freq_ = freq = std::end(buf_.freqs) - left_;
  }

  while (doc < std::end(buf_.docs)) {
    if constexpr (FieldTraits::frequency()) {
      if constexpr (IteratorTraits::frequency()) {
        if (shift_unpack_32(doc_in_->read_vint(), *doc++)) {
          *freq++ = 1;
        } else {
          *freq++ = doc_in_->read_vint();
        }
      } else {
        if (!shift_unpack_32(doc_in_->read_vint(), *doc++)) {
          doc_in_->read_vint();
        }
      }
    } else {
      *doc++ = doc_in_->read_vint();
    }
  }
  left_ = 0;
}

template<typename IteratorTraits, typename FieldTraits>
using AttributesT = std::conditional_t<
  IteratorTraits::position(),
  std::tuple<document, frequency, score, cost,
             position<IteratorTraits, FieldTraits>>,
  std::conditional_t<IteratorTraits::frequency(),
                     std::tuple<document, frequency, score, cost>,
                     std::tuple<document, score, cost>>>;

template<typename IteratorTraits, typename FieldTraits>
class single_doc_iterator
  : public irs::doc_iterator,
    private std::conditional_t<IteratorTraits::position(),
                               data_buffer<IteratorTraits>, empty> {
  static_assert((IteratorTraits::features() & FieldTraits::features()) ==
                IteratorTraits::features());

  using Attributes = AttributesT<IteratorTraits, FieldTraits>;
  using Position = position<IteratorTraits, FieldTraits>;

 public:
  single_doc_iterator() = default;

  void WandPrepare(const term_meta& meta, const index_input* pos_in,
                   const index_input* pay_in,
                   const ScoreFunctionFactory& factory) {
    prepare(meta, pos_in, pay_in);
    auto func = factory(*this);

    auto& doc = std::get<document>(attrs_);
    doc.value = next_;

    auto& score = std::get<irs::score>(attrs_);
    func(&score.max.leaf);
    score.max.tail = score.max.leaf;
    score = ScoreFunction::Constant(score.max.tail);

    doc.value = doc_limits::invalid();
  }

  void prepare(const term_meta& meta,
               [[maybe_unused]] const index_input* pos_in,
               [[maybe_unused]] const index_input* pay_in);

 private:
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t seek(doc_id_t target) final {
    auto& doc = std::get<document>(attrs_);

    while (doc.value < target) {
      next();
    }

    return doc.value;
  }

  doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

  bool next() final {
    auto& doc = std::get<document>(attrs_);

    doc.value = next_;
    next_ = doc_limits::eof();

    if constexpr (IteratorTraits::position()) {
      if (!doc_limits::eof(doc.value)) {
        auto& pos = std::get<Position>(attrs_);
        pos.notify(std::get<frequency>(attrs_).value);
        pos.clear();
      }
    }

    return !doc_limits::eof(doc.value);
  }

  doc_id_t next_{doc_limits::eof()};
  Attributes attrs_;
};

template<typename IteratorTraits, typename FieldTraits>
void single_doc_iterator<IteratorTraits, FieldTraits>::prepare(
  const term_meta& meta, [[maybe_unused]] const index_input* pos_in,
  [[maybe_unused]] const index_input* pay_in) {
  // use single_doc_iterator for singleton docs only,
  // must be ensured by the caller
  IRS_ASSERT(meta.docs_count == 1);

  const auto& term_state = static_cast<const version10::term_meta&>(meta);
  next_ = doc_limits::min() + term_state.e_single_doc;
  std::get<cost>(attrs_).reset(1);  // estimate iterator

  if constexpr (IteratorTraits::frequency()) {
    const auto term_freq = meta.freq;

    IRS_ASSERT(term_freq);
    std::get<frequency>(attrs_).value = term_freq;

    if constexpr (IteratorTraits::position()) {
      auto get_tail_start = [&]() noexcept {
        if (term_freq < IteratorTraits::block_size()) {
          return term_state.pos_start;
        } else if (term_freq == IteratorTraits::block_size()) {
          return address_limits::invalid();
        } else {
          return term_state.pos_start + term_state.pos_end;
        }
      };

      const DocState state{
        .pos_in = pos_in,
        .pay_in = pay_in,
        .term_state = &term_state,
        .freq = &std::get<frequency>(attrs_).value,
        .enc_buf = this->docs,
        .tail_start = get_tail_start(),
        .tail_length = term_freq % IteratorTraits::block_size()};

      std::get<Position>(attrs_).prepare(state);
    }
  }
}

static_assert(kMaxScorers < WandContext::kDisable);

template<uint8_t Value>
struct Extent {
  static constexpr uint8_t GetExtent() noexcept { return Value; }
};

template<>
struct Extent<WandContext::kDisable> {
  Extent(uint8_t value) noexcept : value{value} {}

  constexpr uint8_t GetExtent() const noexcept { return value; }

  uint8_t value;
};

using DynamicExtent = Extent<WandContext::kDisable>;

template<uint8_t PossibleMin, typename Func>
auto ResolveExtent(uint8_t extent, Func&& func) {
  if constexpr (PossibleMin == WandContext::kDisable) {
    return std::forward<Func>(func)(Extent<0>{});
  } else {
    switch (extent) {
      case 1:
        return std::forward<Func>(func)(Extent<1>{});
      case 0:
        if constexpr (PossibleMin <= 0) {
          return std::forward<Func>(func)(Extent<0>{});
        }
        [[fallthrough]];
      default:
        return std::forward<Func>(func)(DynamicExtent{extent});
    }
  }
}

// TODO(MBkkt) Make it overloads
// Remove to many Readers implementations

template<typename WandExtent>
void CommonSkipWandData(WandExtent extent, index_input& in) {
  switch (auto count = extent.GetExtent(); count) {
    case 0:
      return;
    case 1:
      in.skip(in.read_byte());
      return;
    default: {
      uint64_t skip{};
      for (; count; --count) {
        skip += in.read_byte();
      }
      in.skip(skip);
      return;
    }
  }
}

template<typename WandExtent>
void CommonReadWandData(WandExtent wextent, uint8_t index,
                        const ScoreFunction& func, WandSource& ctx,
                        index_input& in, score_t& score) {
  const auto extent = wextent.GetExtent();
  IRS_ASSERT(extent);
  IRS_ASSERT(index < extent);
  if (IRS_LIKELY(extent == 1)) {
    const auto size = in.read_byte();
    ctx.Read(in, size);
    func(&score);
    return;
  }

  uint8_t i = 0;
  uint64_t scorer_offset = 0;
  for (; i < index; ++i) {
    scorer_offset += in.read_byte();
  }
  const auto size = in.read_byte();
  ++i;
  uint64_t block_offset = 0;
  for (; i < extent; ++i) {
    block_offset += in.read_byte();
  }

  if (scorer_offset) {
    in.skip(scorer_offset);
  }
  ctx.Read(in, size);
  func(&score);
  if (block_offset) {
    in.skip(block_offset);
  }
}

// Iterator over posting list.
// IteratorTraits defines requested features.
// FieldTraits defines requested features.
template<typename IteratorTraits, typename FieldTraits, typename WandExtent>
class doc_iterator : public doc_iterator_base<IteratorTraits, FieldTraits> {
  using Attributes = AttributesT<IteratorTraits, FieldTraits>;
  using Position = position<IteratorTraits, FieldTraits>;

 public:
  // hide 'ptr' defined in irs::doc_iterator
  using ptr = memory::managed_ptr<doc_iterator>;

  doc_iterator(WandExtent extent)
    : skip_{IteratorTraits::block_size(), postings_writer_base::kSkipN,
            ReadSkip{extent}} {
    IRS_ASSERT(
      std::all_of(std::begin(this->buf_.docs), std::end(this->buf_.docs),
                  [](doc_id_t doc) { return !doc_limits::valid(doc); }));
  }

  void WandPrepare(const term_meta& meta, const index_input* doc_in,
                   const index_input* pos_in, const index_input* pay_in,
                   const ScoreFunctionFactory& factory, const Scorer& scorer,
                   uint8_t wand_index) {
    prepare(meta, doc_in, pos_in, pay_in, wand_index);
    if (meta.docs_count > FieldTraits::block_size()) {
      return;
    }
    auto ctx = scorer.prepare_wand_source();
    auto func = factory(*ctx);

    auto old_offset = std::numeric_limits<size_t>::max();
    if (meta.docs_count == FieldTraits::block_size()) {
      old_offset = this->doc_in_->file_pointer();
      FieldTraits::skip_block(*this->doc_in_);
      if constexpr (FieldTraits::frequency()) {
        FieldTraits::skip_block(*this->doc_in_);
      }
    }

    auto& score = std::get<irs::score>(attrs_);
    skip_.Reader().ReadMaxScore(wand_index, func, *ctx, *this->doc_in_,
                                score.max.tail);
    score.max.leaf = score.max.tail;

    if (old_offset != std::numeric_limits<size_t>::max()) {
      this->doc_in_->seek(old_offset);
    }
  }

  void prepare(const term_meta& meta, const index_input* doc_in,
               [[maybe_unused]] const index_input* pos_in,
               [[maybe_unused]] const index_input* pay_in,
               uint8_t wand_index = WandContext::kDisable);

 private:
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t seek(doc_id_t target) final;

  doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

#if defined(_MSC_VER)
#pragma warning(disable : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

  bool next() final {
    auto& doc_value = std::get<document>(attrs_).value;

    if (this->begin_ == std::end(this->buf_.docs)) {
      if (IRS_UNLIKELY(!this->left_)) {
        doc_value = doc_limits::eof();
        return false;
      }

      this->refill();

      // If this is the initial doc_id then
      // set it to min() for proper delta value
      doc_value += doc_id_t{!doc_limits::valid(doc_value)};
    }

    doc_value += *this->begin_++;  // update document attribute

    if constexpr (IteratorTraits::frequency()) {
      auto& freq = std::get<frequency>(attrs_);
      freq.value = *this->freq_++;  // update frequency attribute

      if constexpr (IteratorTraits::position()) {
        auto& pos = std::get<Position>(attrs_);
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
  class ReadSkip : private WandExtent {
   public:
    explicit ReadSkip(WandExtent extent) : WandExtent{extent}, skip_levels_(1) {
      Disable();  // Prevent using skip-list by default
    }

    void ReadMaxScore(uint8_t index, const ScoreFunction& func, WandSource& ctx,
                      index_input& in, score_t& score) {
      CommonReadWandData(static_cast<WandExtent>(*this), index, func, ctx, in,
                         score);
    }

    void Disable() noexcept {
      IRS_ASSERT(!skip_levels_.empty());
      IRS_ASSERT(!doc_limits::valid(skip_levels_.back().doc));
      skip_levels_.back().doc = doc_limits::eof();
    }

    void Enable(const version10::term_meta& state) noexcept {
      IRS_ASSERT(state.docs_count > IteratorTraits::block_size());

      // Since we store pointer deltas, add postings offset
      auto& top = skip_levels_.front();
      CopyState<IteratorTraits>(top, state);
      Enable();
    }

    void Init(size_t num_levels) {
      IRS_ASSERT(num_levels);
      skip_levels_.resize(num_levels);
    }

    bool IsLess(size_t level, doc_id_t target) const noexcept {
      return skip_levels_[level].doc < target;
    }

    void MoveDown(size_t level) noexcept {
      auto& next = skip_levels_[level];
      // Move to the more granular level
      IRS_ASSERT(prev_);
      CopyState<IteratorTraits>(next, *prev_);
    }

    void Read(size_t level, index_input& in);
    void Seal(size_t level);
    size_t AdjustLevel(size_t level) const noexcept { return level; }

    void Reset(SkipState& state) noexcept {
      IRS_ASSERT(std::is_sorted(
        std::begin(skip_levels_), std::end(skip_levels_),
        [](const auto& lhs, const auto& rhs) { return lhs.doc > rhs.doc; }));

      prev_ = &state;
    }

    doc_id_t UpperBound() const noexcept {
      IRS_ASSERT(!skip_levels_.empty());
      return skip_levels_.back().doc;
    }

    void SkipWandData(index_input& in) {
      if constexpr (FieldTraits::wand()) {
        CommonSkipWandData(static_cast<WandExtent>(*this), in);
      }
    }

   private:
    void Enable() noexcept {
      IRS_ASSERT(!skip_levels_.empty());
      IRS_ASSERT(doc_limits::eof(skip_levels_.back().doc));
      skip_levels_.back().doc = doc_limits::invalid();
    }

    std::vector<SkipState> skip_levels_;
    SkipState* prev_{};  // Pointer to skip context used by skip reader
  };

  void seek_to_block(doc_id_t target);

  uint64_t skip_offs_{};
  SkipReader<ReadSkip> skip_;
  Attributes attrs_;
  uint32_t docs_count_{};
};

template<typename IteratorTraits, typename FieldTraits, typename WandExtent>
void doc_iterator<IteratorTraits, FieldTraits, WandExtent>::ReadSkip::Read(
  size_t level, index_input& in) {
  auto& next = skip_levels_[level];

  // Store previous step on the same level
  CopyState<IteratorTraits>(*prev_, next);

  ReadState<FieldTraits>(next, in);

  SkipWandData(in);
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent>
void doc_iterator<IteratorTraits, FieldTraits, WandExtent>::ReadSkip::Seal(
  size_t level) {
  auto& next = skip_levels_[level];

  // Store previous step on the same level
  CopyState<IteratorTraits>(*prev_, next);

  // Stream exhausted
  next.doc = doc_limits::eof();
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent>
void doc_iterator<IteratorTraits, FieldTraits, WandExtent>::prepare(
  const term_meta& meta, const index_input* doc_in,
  [[maybe_unused]] const index_input* pos_in,
  [[maybe_unused]] const index_input* pay_in, uint8_t wand_index) {
  // Don't use doc_iterator for singleton docs, must be ensured by the caller
  IRS_ASSERT(meta.docs_count > 1);
  IRS_ASSERT(this->begin_ == std::end(this->buf_.docs));

  auto& term_state = static_cast<const version10::term_meta&>(meta);
  this->left_ = term_state.docs_count;

  // Init document stream
  if (!this->doc_in_) {
    this->doc_in_ = doc_in->reopen();  // Reopen thread-safe stream

    if (!this->doc_in_) {
      // Implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen document input");

      throw io_error("failed to reopen document input");
    }
  }

  this->doc_in_->seek(term_state.doc_start);
  IRS_ASSERT(!this->doc_in_->eof());

  std::get<cost>(attrs_).reset(term_state.docs_count);  // Estimate iterator

  IRS_ASSERT(!IteratorTraits::frequency() || meta.freq);
  if constexpr (IteratorTraits::frequency() && IteratorTraits::position()) {
    const auto term_freq = meta.freq;

    auto get_tail_start = [&]() noexcept {
      if (term_freq < IteratorTraits::block_size()) {
        return term_state.pos_start;
      } else if (term_freq == IteratorTraits::block_size()) {
        return address_limits::invalid();
      } else {
        return term_state.pos_start + term_state.pos_end;
      }
    };

    const DocState state{
      .pos_in = pos_in,
      .pay_in = pay_in,
      .term_state = &term_state,
      .freq = &std::get<frequency>(attrs_).value,
      .enc_buf = this->enc_buf_,
      .tail_start = get_tail_start(),
      .tail_length = term_freq % IteratorTraits::block_size()};

    std::get<Position>(attrs_).prepare(state);
  }

  if (term_state.docs_count > IteratorTraits::block_size()) {
    // Allow using skip-list for long enough postings
    skip_.Reader().Enable(term_state);
    skip_offs_ = term_state.doc_start + term_state.e_skip_start;
    docs_count_ = term_state.docs_count;
  } else if (term_state.docs_count != IteratorTraits::block_size() &&
             wand_index == WandContext::kDisable) {
    skip_.Reader().SkipWandData(*this->doc_in_);
  }
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent>
doc_id_t doc_iterator<IteratorTraits, FieldTraits, WandExtent>::seek(
  doc_id_t target) {
  auto& doc_value = std::get<document>(attrs_).value;

  if (IRS_UNLIKELY(target <= doc_value)) {
    return doc_value;
  }

  // Check whether it makes sense to use skip-list
  if (skip_.Reader().UpperBound() < target) {
    seek_to_block(target);

    if (IRS_UNLIKELY(!this->left_)) {
      return doc_value = doc_limits::eof();
    }

    this->refill();

    // If this is the initial doc_id then
    // set it to min() for proper delta value
    doc_value += doc_id_t{!doc_limits::valid(doc_value)};
  }

  [[maybe_unused]] uint32_t notify{0};
  while (this->begin_ != std::end(this->buf_.docs)) {
    doc_value += *this->begin_++;

    if constexpr (!IteratorTraits::position()) {
      if (doc_value >= target) {
        if constexpr (IteratorTraits::frequency()) {
          this->freq_ = this->buf_.freqs + this->relative_pos();
          IRS_ASSERT((this->freq_ - 1) >= this->buf_.freqs);
          IRS_ASSERT((this->freq_ - 1) < std::end(this->buf_.freqs));
          std::get<frequency>(attrs_).value = this->freq_[-1];
        }
        return doc_value;
      }
    } else {
      IRS_ASSERT(IteratorTraits::frequency());
      auto& freq = std::get<frequency>(attrs_);
      auto& pos = std::get<Position>(attrs_);
      freq.value = *this->freq_++;
      notify += freq.value;

      if (doc_value >= target) {
        pos.notify(notify);
        pos.clear();
        return doc_value;
      }
    }
  }

  if constexpr (IteratorTraits::position()) {
    std::get<Position>(attrs_).notify(notify);
  }
  while (doc_value < target) {
    next();
  }

  return doc_value;
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent>
void doc_iterator<IteratorTraits, FieldTraits, WandExtent>::seek_to_block(
  doc_id_t target) {
  // Ensured by caller
  IRS_ASSERT(skip_.Reader().UpperBound() < target);

  SkipState last;  // Where block starts
  skip_.Reader().Reset(last);

  // Init skip writer in lazy fashion
  if (IRS_LIKELY(!docs_count_)) {
  seek_after_initialization:
    IRS_ASSERT(skip_.NumLevels());

    this->left_ = skip_.Seek(target);
    this->doc_in_->seek(last.doc_ptr);
    std::get<document>(attrs_).value = last.doc;
    if constexpr (IteratorTraits::position()) {
      auto& pos = std::get<Position>(attrs_);
      pos.prepare(last);  // Notify positions
    }

    return;
  }

  auto skip_in = this->doc_in_->dup();

  if (!skip_in) {
    IRS_LOG_ERROR("Failed to duplicate document input");

    throw io_error("Failed to duplicate document input");
  }

  IRS_ASSERT(!skip_.NumLevels());
  skip_in->seek(skip_offs_);
  skip_.Reader().SkipWandData(*skip_in);
  skip_.Prepare(std::move(skip_in), docs_count_);
  docs_count_ = 0;

  // initialize skip levels
  if (const auto num_levels = skip_.NumLevels(); IRS_LIKELY(
        num_levels > 0 && num_levels <= postings_writer_base::kMaxSkipLevels)) {
    IRS_ASSERT(!doc_limits::valid(skip_.Reader().UpperBound()));
    skip_.Reader().Init(num_levels);

    goto seek_after_initialization;
  } else {
    IRS_ASSERT(false);
    throw index_error{absl::StrCat("Invalid number of skip levels ", num_levels,
                                   ", must be in range of [1, ",
                                   postings_writer_base::kMaxSkipLevels, "].")};
  }
}

// WAND iterator over posting list.
// IteratorTraits defines requested features.
// FieldTraits defines requested features.
template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
class wanderator : public doc_iterator_base<IteratorTraits, FieldTraits>,
                   private score_ctx {
  static_assert(IteratorTraits::wand());
  static_assert(IteratorTraits::frequency());

  using Attributes = AttributesT<IteratorTraits, FieldTraits>;
  using Position = position<IteratorTraits, FieldTraits>;

  static void MinStrict(score_ctx* ctx, score_t arg) noexcept {
    auto& self = static_cast<wanderator&>(*ctx);
    self.skip_.Reader().threshold_ = arg;
  }

  static void MinWeak(score_ctx* ctx, score_t arg) noexcept {
    MinStrict(ctx, std::nextafter(arg, 0.f));
  }

 public:
  // Hide 'ptr' defined in irs::doc_iterator
  using ptr = memory::managed_ptr<wanderator>;

  wanderator(const ScoreFunctionFactory& factory, const Scorer& scorer,
             WandExtent extent, uint8_t index, bool strict)
    : skip_{IteratorTraits::block_size(), postings_writer_base::kSkipN,
            ReadSkip{factory, scorer, index, extent}},
      scorer_{factory(*this)} {
    IRS_ASSERT(
      std::all_of(std::begin(this->buf_.docs), std::end(this->buf_.docs),
                  [](doc_id_t doc) { return doc == doc_limits::invalid(); }));
    std::get<irs::score>(attrs_).Reset(
      *this,
      [](score_ctx* ctx, score_t* res) noexcept {
        auto& self = static_cast<wanderator&>(*ctx);
        if constexpr (Root) {
          *res = self.score_;
        } else {
          self.scorer_(res);
        }
      },
      strict ? MinStrict : MinWeak);
  }

  void WandPrepare(const term_meta& meta, const index_input* doc_in,
                   [[maybe_unused]] const index_input* pos_in,
                   [[maybe_unused]] const index_input* pay_in);

 private:
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t seek(doc_id_t target) final;

  doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

  bool next() final { return !doc_limits::eof(seek(value() + 1)); }

  struct SkipScoreContext final : attribute_provider {
    frequency freq;

    attribute* get_mutable(irs::type_info::type_id id) noexcept final {
      if (id == irs::type<frequency>::id()) {
        return &freq;
      }
      return nullptr;
    }
  };

  class ReadSkip {
   public:
    ReadSkip(const ScoreFunctionFactory& factory, const Scorer& scorer,
             uint8_t index, WandExtent extent)
      : ctx_{scorer.prepare_wand_source()},
        func_{factory(*ctx_)},
        index_{index},
        extent_{extent} {
      IRS_ASSERT(extent_.GetExtent() > 0);
    }

    void EnsureSorted() const noexcept {
      IRS_ASSERT(std::is_sorted(
        skip_levels_.begin(), skip_levels_.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.doc > rhs.doc; }));
      IRS_ASSERT(std::is_sorted(skip_scores_.begin(), skip_scores_.end(),
                                std::greater<>{}));
    }

    void ReadMaxScore(irs::score::UpperBounds& max, index_input& input) {
      CommonReadWandData(extent_, index_, func_, *ctx_, input, max.tail);
      max.leaf = max.tail;
    }
    void Init(const version10::term_meta& state, size_t num_levels,
              irs::score::UpperBounds& max);
    bool IsLess(size_t level, doc_id_t target) const noexcept {
      return skip_levels_[level].doc < target ||
             skip_scores_[level] <= threshold_;
    }
    bool IsLessThanUpperBound(doc_id_t target) const noexcept {
      return skip_levels_.back().doc < target ||
             skip_scores_.back() <= threshold_;
    }
    void MoveDown(size_t level) noexcept {
      auto& next = skip_levels_[level];

      // Move to the more granular level
      CopyState<IteratorTraits>(next, prev_skip_);
    }
    void Read(size_t level, index_input& in);
    void Seal(size_t level);
    size_t AdjustLevel(size_t level) const noexcept;
    SkipState& State() noexcept { return prev_skip_; }
    SkipState& Next() noexcept { return skip_levels_.back(); }

    WandSource::ptr ctx_;
    ScoreFunction func_;
    std::vector<SkipState> skip_levels_;
    std::vector<score_t> skip_scores_;
    SkipState prev_skip_;  // skip context used by skip reader
    score_t threshold_{};
    uint8_t index_;
    IRS_NO_UNIQUE_ADDRESS WandExtent extent_;
  };

  doc_id_t shallow_seek(doc_id_t target) final {
    seek_to_block(target);
    return skip_.Reader().Next().doc;
  }

  void seek_to_block(doc_id_t target) {
    skip_.Reader().EnsureSorted();

    // Check whether we have to use skip-list
    if (skip_.Reader().IsLessThanUpperBound(target)) {
      // Ensured by prepare(...)
      IRS_ASSERT(skip_.NumLevels());
      // We could decide to make new skip before actual read block
      // IRS_ASSERT(0 == skip_.Reader().State().doc_ptr);

      this->left_ = skip_.Seek(target);
      auto& doc_value = std::get<document>(attrs_).value;
      doc_value = skip_.Reader().State().doc;
      std::get<score>(attrs_).max.leaf = skip_.Reader().skip_scores_.back();
      // Will trigger refill in "next"
      this->begin_ = std::end(this->buf_.docs);
    }
  }

  SkipReader<ReadSkip> skip_;
  Attributes attrs_;
  ScoreFunction scorer_;  // FIXME(gnusi): can we use only one ScoreFunction?
  score_t score_{};
};

template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
void wanderator<IteratorTraits, FieldTraits, WandExtent, Root>::ReadSkip::Init(
  const version10::term_meta& term_state, size_t num_levels,
  irs::score::UpperBounds& max) {
  // Don't use wanderator for short posting lists, must be ensured by the caller
  IRS_ASSERT(term_state.docs_count > IteratorTraits::block_size());

  skip_levels_.resize(num_levels);
  skip_scores_.resize(num_levels);
  max.leaf = skip_scores_.back();
#ifdef IRESEARCH_TEST
  max.levels = std::span{skip_scores_};
#endif

  // Since we store pointer deltas, add postings offset
  auto& top = skip_levels_.front();
  CopyState<IteratorTraits>(top, term_state);
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
void wanderator<IteratorTraits, FieldTraits, WandExtent, Root>::ReadSkip::Read(
  size_t level, index_input& in) {
  auto& last = prev_skip_;
  auto& next = skip_levels_[level];
  auto& score = skip_scores_[level];

  // Store previous step on the same level
  CopyState<IteratorTraits>(last, next);

  ReadState<FieldTraits>(next, in);
  // TODO(MBkkt) We could don't read actual wand data for not just term query
  //  It looks almost no difference now, but if we will have bigger wand data
  //  it could be useful
  // if constexpr (Root) {
  CommonReadWandData(extent_, index_, func_, *ctx_, in, score);
  // } else {
  //   auto& back = skip_scores_.back();
  //   if (&score == &back || threshold_ > back) {
  //     CommonReadWandData(extent_, index_, func_, *ctx_, in, score);
  //   } else {
  //     CommonSkipWandData(extent_, in);
  //     score = std::numeric_limits<score_t>::max();
  //   }
  // }
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
size_t wanderator<IteratorTraits, FieldTraits, WandExtent,
                  Root>::ReadSkip::AdjustLevel(size_t level) const noexcept {
  while (level && skip_levels_[level].doc >= skip_levels_[level - 1].doc) {
    IRS_ASSERT(skip_levels_[level - 1].doc != doc_limits::eof());
    --level;
  }
  return level;
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
void wanderator<IteratorTraits, FieldTraits, WandExtent, Root>::ReadSkip::Seal(
  size_t level) {
  auto& last = prev_skip_;
  auto& next = skip_levels_[level];

  // Store previous step on the same level
  CopyState<IteratorTraits>(last, next);

  // Stream exhausted
  next.doc = doc_limits::eof();
  skip_scores_[level] = std::numeric_limits<score_t>::max();
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
void wanderator<IteratorTraits, FieldTraits, WandExtent, Root>::WandPrepare(
  const term_meta& meta, const index_input* doc_in,
  [[maybe_unused]] const index_input* pos_in,
  [[maybe_unused]] const index_input* pay_in) {
  // Don't use wanderator for short posting lists, must be ensured by the caller
  IRS_ASSERT(meta.docs_count > IteratorTraits::block_size());
  IRS_ASSERT(this->begin_ == std::end(this->buf_.docs));

  auto& term_state = static_cast<const version10::term_meta&>(meta);
  this->left_ = term_state.docs_count;

  // Init document stream
  if (!this->doc_in_) {
    this->doc_in_ = doc_in->reopen();  // reopen thread-safe stream

    if (!this->doc_in_) {
      // Implementation returned wrong pointer
      IRS_LOG_ERROR("Failed to reopen document input");

      throw io_error("failed to reopen document input");
    }
  }

  this->doc_in_->seek(term_state.doc_start);
  IRS_ASSERT(!this->doc_in_->eof());

  std::get<cost>(attrs_).reset(term_state.docs_count);  // Estimate iterator

  IRS_ASSERT(!IteratorTraits::frequency() || meta.freq);

  if constexpr (IteratorTraits::frequency() && IteratorTraits::position()) {
    const auto term_freq = meta.freq;

    auto get_tail_start = [&]() noexcept {
      if (term_freq < IteratorTraits::block_size()) {
        return term_state.pos_start;
      } else if (term_freq == IteratorTraits::block_size()) {
        return address_limits::invalid();
      } else {
        return term_state.pos_start + term_state.pos_end;
      }
    };

    const DocState state{
      .pos_in = pos_in,
      .pay_in = pay_in,
      .term_state = &term_state,
      .freq = &std::get<frequency>(attrs_).value,
      .enc_buf = this->enc_buf_,
      .tail_start = get_tail_start(),
      .tail_length = term_freq % IteratorTraits::block_size()};

    std::get<Position>(attrs_).prepare(state);
  }

  auto skip_in = this->doc_in_->dup();

  if (!skip_in) {
    IRS_LOG_ERROR("Failed to duplicate document input");

    throw io_error("Failed to duplicate document input");
  }

  skip_in->seek(term_state.doc_start + term_state.e_skip_start);

  auto& max = std::get<irs::score>(attrs_).max;
  skip_.Reader().ReadMaxScore(max, *skip_in);

  skip_.Prepare(std::move(skip_in), term_state.docs_count);

  // Initialize skip levels
  if (const auto num_levels = skip_.NumLevels(); IRS_LIKELY(
        num_levels > 0 && num_levels <= postings_writer_base::kMaxSkipLevels)) {
    skip_.Reader().Init(term_state, num_levels, max);
  } else {
    IRS_ASSERT(false);
    throw index_error{absl::StrCat("Invalid number of skip levels ", num_levels,
                                   ", must be in range of [1, ",
                                   postings_writer_base::kMaxSkipLevels, "].")};
  }
}

template<typename IteratorTraits, typename FieldTraits, typename WandExtent,
         bool Root>
doc_id_t wanderator<IteratorTraits, FieldTraits, WandExtent, Root>::seek(
  doc_id_t target) {
  auto& doc_value = std::get<document>(attrs_).value;

  if (IRS_UNLIKELY(target <= doc_value)) {
    return doc_value;
  }

  while (true) {
    seek_to_block(target);

    if (this->begin_ == std::end(this->buf_.docs)) {
      if (IRS_UNLIKELY(!this->left_)) {
        return doc_value = doc_limits::eof();
      }

      if (auto& state = skip_.Reader().State(); state.doc_ptr) {
        this->doc_in_->seek(state.doc_ptr);
        if constexpr (IteratorTraits::position()) {
          auto& pos = std::get<Position>(attrs_);
          pos.prepare(state);  // Notify positions
        }
        state.doc_ptr = 0;
      }

      doc_value += !doc_limits::valid(doc_value);
      this->refill();
    }

    [[maybe_unused]] uint32_t notify{0};
    [[maybe_unused]] const auto threshold = skip_.Reader().threshold_;

    while (this->begin_ != std::end(this->buf_.docs)) {
      doc_value += *this->begin_++;

      if constexpr (!IteratorTraits::position()) {
        if (doc_value >= target) {
          if constexpr (IteratorTraits::frequency()) {
            this->freq_ = this->buf_.freqs + this->relative_pos();
            IRS_ASSERT((this->freq_ - 1) >= this->buf_.freqs);
            IRS_ASSERT((this->freq_ - 1) < std::end(this->buf_.freqs));

            auto& freq = std::get<frequency>(attrs_);
            freq.value = this->freq_[-1];
            if constexpr (Root) {
              // We can use approximation before actual score for bm11, bm15,
              // tfidf(true/false) but only for term query, so I don't think
              // it's really good idea. And I don't except big difference
              // because in such case, compution is pretty cheap
              scorer_(&score_);
              if (score_ <= threshold) {
                continue;
              }
            }
          }
          return doc_value;
        }
      } else {
        static_assert(IteratorTraits::frequency());
        auto& freq = std::get<frequency>(attrs_);
        freq.value = *this->freq_++;
        notify += freq.value;
        if (doc_value >= target) {
          if constexpr (Root) {
            scorer_(&score_);
            if (score_ <= threshold) {
              continue;
            }
          }
          auto& pos = std::get<Position>(attrs_);
          pos.notify(notify);
          pos.clear();
          return doc_value;
        }
      }
    }

    if constexpr (IteratorTraits::position()) {
      std::get<Position>(attrs_).notify(notify);
    }

    target = doc_value + 1;
  }
}

struct IndexMetaWriter final : public index_meta_writer {
  static constexpr std::string_view kFormatName = "iresearch_10_index_meta";
  static constexpr std::string_view kFormatPrefix = "segments_";
  static constexpr std::string_view kFormatPrefixTmp = "pending_segments_";

  static constexpr int32_t kFormatMin = 0;
  static constexpr int32_t kFormatMax = 1;

  enum { kHasPayload = 1 };

  static std::string FileName(uint64_t gen) {
    return FileName(kFormatPrefix, gen);
  }

  explicit IndexMetaWriter(int32_t version) noexcept : version_{version} {
    IRS_ASSERT(version_ >= kFormatMin && version <= kFormatMax);
  }

  // FIXME(gnusi): Better to split prepare into 2 methods and pass meta by
  // const reference
  bool prepare(directory& dir, IndexMeta& meta, std::string& pending_filename,
               std::string& filename) final;
  bool commit() final;
  void rollback() noexcept final;

 private:
  static std::string FileName(std::string_view prefix, uint64_t gen) {
    IRS_ASSERT(index_gen_limits::valid(gen));
    return irs::file_name(prefix, gen);
  }

  static std::string PendingFileName(uint64_t gen) {
    return FileName(kFormatPrefixTmp, gen);
  }

  directory* dir_{};
  uint64_t pending_gen_{index_gen_limits::invalid()};  // Generation to commit
  int32_t version_;
};

bool IndexMetaWriter::prepare(directory& dir, IndexMeta& meta,
                              std::string& pending_filename,
                              std::string& filename) {
  if (index_gen_limits::valid(pending_gen_)) {
    // prepare() was already called with no corresponding call to commit()
    return false;
  }

  ++meta.gen;  // Increment generation before generating filename
  pending_filename = PendingFileName(meta.gen);
  filename = FileName(meta.gen);

  auto out = dir.create(pending_filename);

  if (!out) {
    throw io_error{
      absl::StrCat("Failed to create file, path: ", pending_filename)};
  }

  {
    format_utils::write_header(*out, kFormatName, version_);
    out->write_vlong(meta.gen);
    out->write_long(meta.seg_counter);
    IRS_ASSERT(meta.segments.size() <= std::numeric_limits<uint32_t>::max());
    out->write_vint(uint32_t(meta.segments.size()));

    for (const auto& segment : meta.segments) {
      write_string(*out, segment.filename);
      write_string(*out, segment.meta.codec->type()().name());
    }

    if (version_ > kFormatMin) {
      const auto payload = GetPayload(meta);
      const uint8_t flags = IsNull(payload) ? 0 : kHasPayload;
      out->write_byte(flags);

      if (flags == kHasPayload) {
        irs::write_string(*out, payload);
      }
    } else {
      // Earlier versions don't support payload.
      meta.payload.reset();
    }

    format_utils::write_footer(*out);
  }  // Important to close output here

  // Only noexcept operations below
  dir_ = &dir;
  pending_gen_ = meta.gen;

  return true;
}

bool IndexMetaWriter::commit() {
  if (!index_gen_limits::valid(pending_gen_)) {
    return false;
  }

  const auto src = PendingFileName(pending_gen_);
  const auto dst = FileName(pending_gen_);

  if (!dir_->rename(src, dst)) {
    rollback();

    throw io_error{absl::StrCat("Failed to rename file, src path: '", src,
                                "' dst path: '", dst, "'")};
  }

  // only noexcept operations below
  // clear pending state
  pending_gen_ = index_gen_limits::invalid();
  dir_ = nullptr;

  return true;
}

void IndexMetaWriter::rollback() noexcept {
  if (!index_gen_limits::valid(pending_gen_)) {
    return;
  }

  std::string seg_file;

  try {
    seg_file = PendingFileName(pending_gen_);
  } catch (const std::exception& e) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught error while generating file name for index meta, reason: ",
      e.what()));
    return;
  } catch (...) {
    IRS_LOG_ERROR("Caught error while generating file name for index meta");
    return;
  }

  if (!dir_->remove(seg_file)) {  // suppress all errors
    IRS_LOG_ERROR(absl::StrCat("Failed to remove file, path: ", seg_file));
  }

  // clear pending state
  dir_ = nullptr;
  pending_gen_ = index_gen_limits::invalid();
}

uint64_t ParseGeneration(std::string_view file) noexcept {
  if (file.starts_with(IndexMetaWriter::kFormatPrefix)) {
    constexpr size_t kPrefixLength = IndexMetaWriter::kFormatPrefix.size();

    if (uint64_t gen; absl::SimpleAtoi(file.substr(kPrefixLength), &gen)) {
      return gen;
    }
  }

  return index_gen_limits::invalid();
}

struct IndexMetaReader : public index_meta_reader {
  bool last_segments_file(const directory& dir, std::string& name) const final;

  void read(const directory& dir, IndexMeta& meta,
            std::string_view filename) final;
};

bool IndexMetaReader::last_segments_file(const directory& dir,
                                         std::string& out) const {
  uint64_t max_gen = index_gen_limits::invalid();
  directory::visitor_f visitor = [&out, &max_gen](std::string_view name) {
    const uint64_t gen = ParseGeneration(name);

    if (gen > max_gen) {
      out = std::move(name);
      max_gen = gen;
    }
    return true;  // continue iteration
  };

  dir.visit(visitor);
  return index_gen_limits::valid(max_gen);
}

void IndexMetaReader::read(const directory& dir, IndexMeta& meta,
                           std::string_view filename) {
  std::string meta_file;
  if (IsNull(filename)) {
    meta_file = IndexMetaWriter::FileName(meta.gen);
    filename = meta_file;
  }

  auto in =
    dir.open(filename, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in) {
    throw io_error{absl::StrCat("Failed to open file, path: ", filename)};
  }

  const auto checksum = format_utils::checksum(*in);

  // check header
  const int32_t version = format_utils::check_header(
    *in, IndexMetaWriter::kFormatName, IndexMetaWriter::kFormatMin,
    IndexMetaWriter::kFormatMax);

  // read data from segments file
  auto gen = in->read_vlong();
  auto cnt = in->read_long();
  auto seg_count = in->read_vint();
  std::vector<IndexSegment> segments(seg_count);

  for (size_t i = 0, count = segments.size(); i < count; ++i) {
    auto& segment = segments[i];

    segment.filename = read_string<std::string>(*in);
    segment.meta.codec = formats::get(read_string<std::string>(*in));

    auto reader = segment.meta.codec->get_segment_meta_reader();

    reader->read(dir, segment.meta, segment.filename);
  }

  bool has_payload = false;
  bstring payload;
  if (version > IndexMetaWriter::kFormatMin) {
    has_payload = (in->read_byte() & IndexMetaWriter::kHasPayload);

    if (has_payload) {
      payload = irs::read_string<bstring>(*in);
    }
  }

  format_utils::check_footer(*in, checksum);

  meta.gen = gen;
  meta.seg_counter = cnt;
  meta.segments = std::move(segments);
  if (has_payload) {
    meta.payload = std::move(payload);
  } else {
    meta.payload.reset();
  }
}

struct SegmentMetaWriter : public segment_meta_writer {
  static constexpr std::string_view FORMAT_EXT = "sm";
  static constexpr std::string_view FORMAT_NAME = "iresearch_10_segment_meta";

  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = 1;

  enum { HAS_COLUMN_STORE = 1, SORTED = 2 };

  explicit SegmentMetaWriter(int32_t version) noexcept : version_(version) {
    IRS_ASSERT(version_ >= FORMAT_MIN && version <= FORMAT_MAX);
  }

  bool SupportPrimarySort() const noexcept final;

  // FIXME(gnusi): Better to split write into 2 methods and pass meta by const
  // reference
  void write(directory& dir, std::string& filename, SegmentMeta& meta) final;

 private:
  int32_t version_;
};

template<>
std::string file_name<segment_meta_writer, SegmentMeta>(
  const SegmentMeta& meta) {
  return irs::file_name(meta.name, meta.version, SegmentMetaWriter::FORMAT_EXT);
}

bool SegmentMetaWriter::SupportPrimarySort() const noexcept {
  // Earlier versions don't support primary sort
  return version_ > FORMAT_MIN;
}

void SegmentMetaWriter::write(directory& dir, std::string& meta_file,
                              SegmentMeta& meta) {
  if (meta.docs_count < meta.live_docs_count) {
    throw index_error{absl::StrCat("Invalid segment meta '", meta.name,
                                   "' detected : docs_count=", meta.docs_count,
                                   ", live_docs_count=", meta.live_docs_count)};
  }

  meta_file = file_name<irs::segment_meta_writer>(meta);
  auto out = dir.create(meta_file);

  if (!out) {
    throw io_error{absl::StrCat("failed to create file, path: ", meta_file)};
  }

  uint8_t flags = meta.column_store ? HAS_COLUMN_STORE : 0;

  format_utils::write_header(*out, FORMAT_NAME, version_);
  write_string(*out, meta.name);
  out->write_vlong(meta.version);
  out->write_vlong(meta.live_docs_count);
  out->write_vlong(meta.docs_count -
                   meta.live_docs_count);  // docs_count >= live_docs_count
  out->write_vlong(meta.byte_size);
  if (SupportPrimarySort()) {
    // sorted indices are not supported in version 1.0
    if (field_limits::valid(meta.sort)) {
      flags |= SORTED;
    }

    out->write_byte(flags);
    out->write_vlong(1 + meta.sort);  // max->0
  } else {
    out->write_byte(flags);
    meta.sort = field_limits::invalid();
  }
  write_strings(*out, meta.files);
  format_utils::write_footer(*out);
}

struct SegmentMetaReader : public segment_meta_reader {
  void read(const directory& dir, SegmentMeta& meta,
            std::string_view filename = {}) final;  // null == use meta
};

void SegmentMetaReader::read(const directory& dir, SegmentMeta& meta,
                             std::string_view filename /*= {} */) {
  const std::string meta_file = IsNull(filename)
                                  ? file_name<segment_meta_writer>(meta)
                                  : std::string{filename};

  auto in =
    dir.open(meta_file, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in) {
    throw io_error{absl::StrCat("Failed to open file, path: ", meta_file)};
  }

  const auto checksum = format_utils::checksum(*in);

  const int32_t version = format_utils::check_header(
    *in, SegmentMetaWriter::FORMAT_NAME, SegmentMetaWriter::FORMAT_MIN,
    SegmentMetaWriter::FORMAT_MAX);

  auto name = read_string<std::string>(*in);
  const auto segment_version = in->read_vlong();
  const auto live_docs_count = in->read_vlong();
  const auto docs_count = in->read_vlong() + live_docs_count;

  if (docs_count < live_docs_count) {
    throw index_error{
      absl::StrCat("While reader segment meta '", name, "', error: docs_count(",
                   docs_count, ") < live_docs_count(", live_docs_count, ")")};
  }

  const auto size = in->read_vlong();
  const auto flags = in->read_byte();
  field_id sort = field_limits::invalid();
  if (version > SegmentMetaWriter::FORMAT_MIN) {
    sort = in->read_vlong() - 1;
  }
  auto files = read_strings(*in);

  if (flags &
      ~(SegmentMetaWriter::HAS_COLUMN_STORE | SegmentMetaWriter::SORTED)) {
    throw index_error{absl::StrCat("While reading segment meta '", name,
                                   "', error: use of unsupported flags '",
                                   flags, "'")};
  }

  const auto sorted = bool(flags & SegmentMetaWriter::SORTED);

  if ((!field_limits::valid(sort)) && sorted) {
    throw index_error{absl::StrCat("While reading segment meta '", name,
                                   "', error: incorrectly marked as sorted")};
  }

  if ((field_limits::valid(sort)) && !sorted) {
    throw index_error{absl::StrCat("While reading segment meta '", name,
                                   "', error: incorrectly marked as unsorted")};
  }

  format_utils::check_footer(*in, checksum);

  // ...........................................................................
  // all operations below are noexcept
  // ...........................................................................

  meta.name = std::move(name);
  meta.version = segment_version;
  meta.column_store = flags & SegmentMetaWriter::HAS_COLUMN_STORE;
  meta.docs_count = docs_count;
  meta.live_docs_count = live_docs_count;
  meta.sort = sort;
  meta.byte_size = size;
  meta.files = std::move(files);
}

class DocumentMaskWriter : public document_mask_writer {
 public:
  static constexpr std::string_view FORMAT_NAME = "iresearch_10_doc_mask";
  static constexpr std::string_view FORMAT_EXT = "doc_mask";

  static constexpr int32_t FORMAT_MIN = 0;
  static constexpr int32_t FORMAT_MAX = FORMAT_MIN;

  std::string filename(const SegmentMeta& meta) const final;

  size_t write(directory& dir, const SegmentMeta& meta,
               const DocumentMask& docs_mask) final;
};

template<>
std::string file_name<document_mask_writer, SegmentMeta>(
  const SegmentMeta& meta) {
  return irs::file_name(meta.name, meta.version,
                        DocumentMaskWriter::FORMAT_EXT);
}

std::string DocumentMaskWriter::filename(const SegmentMeta& meta) const {
  return file_name<document_mask_writer>(meta);
}

size_t DocumentMaskWriter::write(directory& dir, const SegmentMeta& meta,
                                 const DocumentMask& docs_mask) {
  const auto filename = file_name<document_mask_writer>(meta);
  auto out = dir.create(filename);

  if (!out) {
    throw io_error{absl::StrCat("Failed to create file, path: ", filename)};
  }

  // segment can't have more than std::numeric_limits<uint32_t>::max()
  // documents
  IRS_ASSERT(docs_mask.size() <= std::numeric_limits<uint32_t>::max());
  const auto count = static_cast<uint32_t>(docs_mask.size());

  format_utils::write_header(*out, FORMAT_NAME, FORMAT_MAX);
  out->write_vint(count);

  for (auto mask : docs_mask) {
    out->write_vint(mask);
  }

  format_utils::write_footer(*out);
  return out->file_pointer();
}

class DocumentMaskReader : public document_mask_reader {
 public:
  bool read(const directory& dir, const SegmentMeta& meta,
            DocumentMask& docs_mask) final;
};

bool DocumentMaskReader::read(const directory& dir, const SegmentMeta& meta,
                              DocumentMask& docs_mask) {
  const auto in_name = file_name<document_mask_writer>(meta);

  bool exists;

  if (!dir.exists(exists, in_name)) {
    throw io_error{
      absl::StrCat("failed to check existence of file, path: ", in_name)};
  }

  if (!exists) {
    // possible that the file does not exist since document_mask is optional
    return false;
  }

  auto in =
    dir.open(in_name, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE);

  if (!in) {
    throw io_error{absl::StrCat("failed to open file, path: ", in_name)};
  }

  const auto checksum = format_utils::checksum(*in);

  format_utils::check_header(*in, DocumentMaskWriter::FORMAT_NAME,
                             DocumentMaskWriter::FORMAT_MIN,
                             DocumentMaskWriter::FORMAT_MAX);

  size_t count = in->read_vint();
  docs_mask.reserve(count);

  while (count--) {
    static_assert(sizeof(doc_id_t) == sizeof(decltype(in->read_vint())));

    docs_mask.insert(in->read_vint());
  }

  format_utils::check_footer(*in, checksum);

  return true;
}

class postings_reader_base : public irs::postings_reader {
 public:
  uint64_t CountMappedMemory() const final {
    uint64_t bytes{0};
    if (doc_in_ != nullptr) {
      bytes += doc_in_->CountMappedMemory();
    }
    if (pos_in_ != nullptr) {
      bytes += pos_in_->CountMappedMemory();
    }
    if (pay_in_ != nullptr) {
      bytes += pay_in_->CountMappedMemory();
    }
    return bytes;
  }

  void prepare(index_input& in, const ReaderState& state,
               IndexFeatures features) final;

  size_t decode(const byte_type* in, IndexFeatures field_features,
                irs::term_meta& state) final;

 protected:
  explicit postings_reader_base(size_t block_size) noexcept
    : block_size_{block_size} {}

  ScorersView scorers_;
  index_input::ptr doc_in_;
  index_input::ptr pos_in_;
  index_input::ptr pay_in_;
  size_t block_size_;
};

void postings_reader_base::prepare(index_input& in, const ReaderState& state,
                                   IndexFeatures features) {
  std::string buf;

  // prepare document input
  prepare_input(buf, doc_in_, irs::IOAdvice::RANDOM, state,
                postings_writer_base::kDocExt,
                postings_writer_base::kDocFormatName,
                static_cast<int32_t>(PostingsFormat::MIN),
                static_cast<int32_t>(PostingsFormat::MAX));

  // Since terms doc postings too large
  //  it is too costly to verify checksum of
  //  the entire file. Here we perform cheap
  //  error detection which could recognize
  //  some forms of corruption.
  format_utils::read_checksum(*doc_in_);

  if (IndexFeatures::NONE != (features & IndexFeatures::POS)) {
    /* prepare positions input */
    prepare_input(buf, pos_in_, irs::IOAdvice::RANDOM, state,
                  postings_writer_base::kPosExt,
                  postings_writer_base::kPosFormatName,
                  static_cast<int32_t>(PostingsFormat::MIN),
                  static_cast<int32_t>(PostingsFormat::MAX));

    // Since terms pos postings too large
    // it is too costly to verify checksum of
    // the entire file. Here we perform cheap
    // error detection which could recognize
    // some forms of corruption.
    format_utils::read_checksum(*pos_in_);

    if (IndexFeatures::NONE !=
        (features & (IndexFeatures::PAY | IndexFeatures::OFFS))) {
      // prepare positions input
      prepare_input(buf, pay_in_, irs::IOAdvice::RANDOM, state,
                    postings_writer_base::kPayExt,
                    postings_writer_base::kPayFormatName,
                    static_cast<int32_t>(PostingsFormat::MIN),
                    static_cast<int32_t>(PostingsFormat::MAX));

      // Since terms pos postings too large
      // it is too costly to verify checksum of
      // the entire file. Here we perform cheap
      // error detection which could recognize
      // some forms of corruption.
      format_utils::read_checksum(*pay_in_);
    }
  }

  // check postings format
  format_utils::check_header(in, postings_writer_base::kTermsFormatName,
                             static_cast<int32_t>(TermsFormat::MIN),
                             static_cast<int32_t>(TermsFormat::MAX));

  const uint64_t block_size = in.read_vint();

  if (block_size != block_size_) {
    throw index_error{
      absl::StrCat("while preparing postings_reader, error: "
                   "invalid block size '",
                   block_size, "', expected '", block_size_, "'")};
  }

  scorers_ =
    state.scorers.subspan(0, std::min(state.scorers.size(), kMaxScorers));
}

size_t postings_reader_base::decode(const byte_type* in, IndexFeatures features,
                                    irs::term_meta& state) {
  auto& term_meta = static_cast<version10::term_meta&>(state);

  const bool has_freq = IndexFeatures::NONE != (features & IndexFeatures::FREQ);
  const auto* p = in;

  term_meta.docs_count = vread<uint32_t>(p);
  if (has_freq) {
    term_meta.freq = term_meta.docs_count + vread<uint32_t>(p);
  }

  term_meta.doc_start += vread<uint64_t>(p);
  if (has_freq && term_meta.freq &&
      IndexFeatures::NONE != (features & IndexFeatures::POS)) {
    term_meta.pos_start += vread<uint64_t>(p);

    term_meta.pos_end = term_meta.freq > block_size_
                          ? vread<uint64_t>(p)
                          : address_limits::invalid();

    if (IndexFeatures::NONE !=
        (features & (IndexFeatures::PAY | IndexFeatures::OFFS))) {
      term_meta.pay_start += vread<uint64_t>(p);
    }
  }

  if (1 == term_meta.docs_count) {
    term_meta.e_single_doc = vread<uint32_t>(p);
  } else if (block_size_ < term_meta.docs_count) {
    term_meta.e_skip_start = vread<uint64_t>(p);
  }

  IRS_ASSERT(p >= in);
  return size_t(std::distance(in, p));
}

template<typename FormatTraits>
class postings_reader final : public postings_reader_base {
 public:
  template<bool Freq, bool Pos, bool Offset, bool Payload>
  struct iterator_traits : FormatTraits {
    static constexpr bool frequency() noexcept { return Freq; }
    static constexpr bool offset() noexcept { return position() && Offset; }
    static constexpr bool payload() noexcept { return position() && Payload; }
    static constexpr bool position() noexcept { return Freq && Pos; }
    static constexpr IndexFeatures features() noexcept {
      auto r = IndexFeatures::NONE;
      if constexpr (Freq) {
        r |= IndexFeatures::FREQ;
      }
      if constexpr (Pos) {
        r |= IndexFeatures::POS;
      }
      if constexpr (Offset) {
        r |= IndexFeatures::OFFS;
      }
      if constexpr (Payload) {
        r |= IndexFeatures::PAY;
      }
      return r;
    }
    static constexpr bool one_based_position_storage() noexcept {
      return FormatTraits::pos_min() == pos_limits::min();
    }
  };

  postings_reader() noexcept
    : postings_reader_base{FormatTraits::block_size()} {}

  irs::doc_iterator::ptr iterator(IndexFeatures field_features,
                                  IndexFeatures required_features,
                                  const term_meta& meta,
                                  uint8_t wand_count) final {
    if (meta.docs_count == 0) {
      IRS_ASSERT(false);
      return irs::doc_iterator::empty();
    }
    return iterator_impl(
      field_features, required_features,
      [&]<typename IteratorTraits, typename FieldTraits>()
        -> irs::doc_iterator::ptr {
        if (meta.docs_count == 1) {
          auto it = memory::make_managed<
            single_doc_iterator<IteratorTraits, FieldTraits>>();
          it->prepare(meta, pos_in_.get(), pay_in_.get());
          return it;
        }
        return ResolveExtent<(FieldTraits::wand() ? 0 : WandContext::kDisable)>(
          wand_count,
          [&]<typename Extent>(Extent&& extent) -> irs::doc_iterator::ptr {
            auto it = memory::make_managed<
              doc_iterator<IteratorTraits, FieldTraits, Extent>>(
              std::forward<Extent>(extent));
            it->prepare(meta, doc_in_.get(), pos_in_.get(), pay_in_.get());
            return it;
          });
      });
  }

  irs::doc_iterator::ptr wanderator(
    IndexFeatures field_features, IndexFeatures required_features,
    const term_meta& meta, [[maybe_unused]] const WanderatorOptions& options,
    WandContext ctx, WandInfo info) final {
    if constexpr (FormatTraits::wand()) {
      auto it = MakeWanderator(field_features, required_features, meta, options,
                               ctx, info);
      if (it) {
        return it;
      }
    }
    return iterator(field_features, required_features, meta, info.count);
  }

  size_t bit_union(IndexFeatures field, const term_provider_f& provider,
                   size_t* set, uint8_t wand_count) final;

 private:
  irs::doc_iterator::ptr MakeWanderator(IndexFeatures field_features,
                                        IndexFeatures required_features,
                                        const term_meta& meta,
                                        const WanderatorOptions& options,
                                        WandContext ctx, WandInfo info) {
    if (meta.docs_count == 0 ||
        info.mapped_index == irs::WandContext::kDisable ||
        scorers_.size() <= ctx.index) {
      return {};
    }
    IRS_ASSERT(info.count != 0);
    const auto& scorer = *scorers_[ctx.index];
    const auto scorer_features = scorer.index_features();
    // TODO(MBkkt) Should we also check against required_features?
    //  Or we should to do required_features |= scorer_features;
    if (scorer_features != (field_features & scorer_features)) {
      return {};
    }
    return iterator_impl(
      field_features, required_features,
      [&]<typename IteratorTraits, typename FieldTraits>()
        -> irs::doc_iterator::ptr {
        // No need to use wanderator for short lists
        if (meta.docs_count == 1) {
          auto it = memory::make_managed<
            single_doc_iterator<IteratorTraits, FieldTraits>>();
          it->WandPrepare(meta, pos_in_.get(), pay_in_.get(), options.factory);
          return it;
        }
        return ResolveExtent<1>(
          info.count,
          [&]<typename WandExtent>(
            WandExtent extent) -> irs::doc_iterator::ptr {
            // TODO(MBkkt) Now we don't support wanderator without frequency
            if constexpr (IteratorTraits::frequency()) {
              // No need to use wanderator for short lists
              if (meta.docs_count > FormatTraits::block_size()) {
                return ResolveBool(
                  ctx.root, [&]<bool Root>() -> irs::doc_iterator::ptr {
                    auto it = memory::make_managed<::wanderator<
                      IteratorTraits, FieldTraits, WandExtent, Root>>(
                      options.factory, scorer, extent, info.mapped_index,
                      ctx.strict);

                    it->WandPrepare(meta, doc_in_.get(), pos_in_.get(),
                                    pay_in_.get());

                    return it;
                  });
              }
            }
            auto it = memory::make_managed<
              doc_iterator<IteratorTraits, FieldTraits, WandExtent>>(extent);
            it->WandPrepare(meta, doc_in_.get(), pos_in_.get(), pay_in_.get(),
                            options.factory, scorer, info.mapped_index);
            return it;
          });
      });
  }

  template<typename FieldTraits, typename Factory>
  irs::doc_iterator::ptr iterator_impl(IndexFeatures enabled,
                                       Factory&& factory);

  template<typename Factory>
  irs::doc_iterator::ptr iterator_impl(IndexFeatures field_features,
                                       IndexFeatures required_features,
                                       Factory&& factory);
};

#if defined(_MSC_VER)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif

template<typename FormatTraits>
template<typename FieldTraits, typename Factory>
irs::doc_iterator::ptr postings_reader<FormatTraits>::iterator_impl(
  IndexFeatures enabled, Factory&& factory) {
  switch (enabled) {
    case IndexFeatures::ALL: {
      using IteratorTraits = iterator_traits<true, true, true, true>;
      if constexpr ((FieldTraits::features() & IteratorTraits::features()) ==
                    IteratorTraits::features()) {
        return std::forward<Factory>(factory)
          .template operator()<IteratorTraits, FieldTraits>();
      }
    } break;
    case IndexFeatures::FREQ | IndexFeatures::POS | IndexFeatures::OFFS: {
      using IteratorTraits = iterator_traits<true, true, true, false>;
      if constexpr ((FieldTraits::features() & IteratorTraits::features()) ==
                    IteratorTraits::features()) {
        return std::forward<Factory>(factory)
          .template operator()<IteratorTraits, FieldTraits>();
      }
    } break;
    case IndexFeatures::FREQ | IndexFeatures::POS | IndexFeatures::PAY: {
      using IteratorTraits = iterator_traits<true, true, false, true>;
      if constexpr ((FieldTraits::features() & IteratorTraits::features()) ==
                    IteratorTraits::features()) {
        return std::forward<Factory>(factory)
          .template operator()<IteratorTraits, FieldTraits>();
      }
    } break;
    case IndexFeatures::FREQ | IndexFeatures::POS: {
      using IteratorTraits = iterator_traits<true, true, false, false>;
      if constexpr ((FieldTraits::features() & IteratorTraits::features()) ==
                    IteratorTraits::features()) {
        return std::forward<Factory>(factory)
          .template operator()<IteratorTraits, FieldTraits>();
      }
    } break;
    case IndexFeatures::FREQ: {
      using IteratorTraits = iterator_traits<true, false, false, false>;
      if constexpr ((FieldTraits::features() & IteratorTraits::features()) ==
                    IteratorTraits::features()) {
        return std::forward<Factory>(factory)
          .template operator()<IteratorTraits, FieldTraits>();
      }
    } break;
    default:
      break;
  }
  using iterator_traits_t = iterator_traits<false, false, false, false>;
  return std::forward<Factory>(factory)
    .template operator()<iterator_traits_t, FieldTraits>();
}

template<typename FormatTraits>
template<typename Factory>
irs::doc_iterator::ptr postings_reader<FormatTraits>::iterator_impl(
  IndexFeatures field_features, IndexFeatures required_features,
  Factory&& factory) {
  // get enabled features as the intersection
  // between requested and available features
  const auto enabled = field_features & required_features;

  switch (field_features) {
    case IndexFeatures::ALL: {
      using FieldTraits = iterator_traits<true, true, true, true>;
      return iterator_impl<FieldTraits>(enabled,
                                        std::forward<Factory>(factory));
    }
    case IndexFeatures::FREQ | IndexFeatures::POS | IndexFeatures::OFFS: {
      using FieldTraits = iterator_traits<true, true, true, false>;
      return iterator_impl<FieldTraits>(enabled,
                                        std::forward<Factory>(factory));
    }
    case IndexFeatures::FREQ | IndexFeatures::POS | IndexFeatures::PAY: {
      using FieldTraits = iterator_traits<true, true, false, true>;
      return iterator_impl<FieldTraits>(enabled,
                                        std::forward<Factory>(factory));
    }
    case IndexFeatures::FREQ | IndexFeatures::POS: {
      using FieldTraits = iterator_traits<true, true, false, false>;
      return iterator_impl<FieldTraits>(enabled,
                                        std::forward<Factory>(factory));
    }
    case IndexFeatures::FREQ: {
      using FieldTraits = iterator_traits<true, false, false, false>;
      return iterator_impl<FieldTraits>(enabled,
                                        std::forward<Factory>(factory));
    }
    default: {
      using FieldTraits = iterator_traits<false, false, false, false>;
      return iterator_impl<FieldTraits>(enabled,
                                        std::forward<Factory>(factory));
    }
  }
}

#if defined(_MSC_VER)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

template<typename FieldTraits, size_t N>
void bit_union(index_input& doc_in, doc_id_t docs_count, uint32_t (&docs)[N],
               uint32_t (&enc_buf)[N], size_t* set) {
  constexpr auto BITS{bits_required<std::remove_pointer_t<decltype(set)>>()};
  size_t num_blocks = docs_count / FieldTraits::block_size();

  doc_id_t doc = doc_limits::min();
  while (num_blocks--) {
    FieldTraits::read_block(doc_in, enc_buf, docs);
    if constexpr (FieldTraits::frequency()) {
      FieldTraits::skip_block(doc_in);
    }

    // FIXME optimize
    for (const auto delta : docs) {
      doc += delta;
      irs::set_bit(set[doc / BITS], doc % BITS);
    }
  }

  doc_id_t docs_left = docs_count % FieldTraits::block_size();

  while (docs_left--) {
    doc_id_t delta;
    if constexpr (FieldTraits::frequency()) {
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

template<typename FormatTraits>
size_t postings_reader<FormatTraits>::bit_union(
  const IndexFeatures field_features, const term_provider_f& provider,
  size_t* set, uint8_t wand_count) {
  constexpr auto BITS{bits_required<std::remove_pointer_t<decltype(set)>>()};
  uint32_t enc_buf[FormatTraits::block_size()];
  uint32_t docs[FormatTraits::block_size()];
  const bool has_freq =
    IndexFeatures::NONE != (field_features & IndexFeatures::FREQ);

  IRS_ASSERT(doc_in_);
  auto doc_in = doc_in_->reopen();  // reopen thread-safe stream

  if (!doc_in) {
    // implementation returned wrong pointer
    IRS_LOG_ERROR("Failed to reopen document input");

    throw io_error("failed to reopen document input");
  }

  size_t count = 0;
  while (const irs::term_meta* meta = provider()) {
    auto& term_state = static_cast<const version10::term_meta&>(*meta);

    if (term_state.docs_count > 1) {
      doc_in->seek(term_state.doc_start);
      IRS_ASSERT(!doc_in->eof());
      if (FormatTraits::wand() &&
          term_state.docs_count < FormatTraits::block_size()) {
        CommonSkipWandData(DynamicExtent{wand_count}, *doc_in);
      }
      IRS_ASSERT(!doc_in->eof());

      if (has_freq) {
        using field_traits_t = iterator_traits<true, false, false, false>;
        ::bit_union<field_traits_t>(*doc_in, term_state.docs_count, docs,
                                    enc_buf, set);
      } else {
        using field_traits_t = iterator_traits<false, false, false, false>;
        ::bit_union<field_traits_t>(*doc_in, term_state.docs_count, docs,
                                    enc_buf, set);
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

class format10 : public irs::version10::format {
 public:
  using format_traits = ::format_traits<false, pos_limits::min()>;

  static constexpr std::string_view type_name() noexcept { return "1_0"; }

  static ptr make();

  index_meta_writer::ptr get_index_meta_writer() const override;
  index_meta_reader::ptr get_index_meta_reader() const final;

  segment_meta_writer::ptr get_segment_meta_writer() const override;
  segment_meta_reader::ptr get_segment_meta_reader() const final;

  document_mask_writer::ptr get_document_mask_writer() const final;
  document_mask_reader::ptr get_document_mask_reader() const final;

  field_writer::ptr get_field_writer(bool consolidation,
                                     IResourceManager&) const override;
  field_reader::ptr get_field_reader(IResourceManager&) const final;

  columnstore_writer::ptr get_columnstore_writer(
    bool consolidation, IResourceManager&) const override;
  columnstore_reader::ptr get_columnstore_reader() const override;

  irs::postings_writer::ptr get_postings_writer(
    bool consolidation, IResourceManager&) const override;
  irs::postings_reader::ptr get_postings_reader() const override;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format10>::id();
  }
};

static const ::format10 FORMAT10_INSTANCE;

index_meta_writer::ptr format10::get_index_meta_writer() const {
  return std::make_unique<IndexMetaWriter>(IndexMetaWriter::kFormatMin);
}

index_meta_reader::ptr format10::get_index_meta_reader() const {
  // can reuse stateless reader
  static IndexMetaReader kInstance;
  return memory::to_managed<index_meta_reader>(kInstance);
}

segment_meta_writer::ptr format10::get_segment_meta_writer() const {
  // can reuse stateless writer
  static SegmentMetaWriter kInstance{SegmentMetaWriter::FORMAT_MIN};
  return memory::to_managed<segment_meta_writer>(kInstance);
}

segment_meta_reader::ptr format10::get_segment_meta_reader() const {
  // can reuse stateless writer
  static SegmentMetaReader kInstance;
  return memory::to_managed<segment_meta_reader>(kInstance);
}

document_mask_writer::ptr format10::get_document_mask_writer() const {
  // can reuse stateless writer
  static DocumentMaskWriter kInstance;
  return memory::to_managed<document_mask_writer>(kInstance);
}

document_mask_reader::ptr format10::get_document_mask_reader() const {
  // can reuse stateless writer
  static DocumentMaskReader kInstance;
  return memory::to_managed<document_mask_reader>(kInstance);
}

field_writer::ptr format10::get_field_writer(bool consolidation,
                                             IResourceManager& rm) const {
  return burst_trie::make_writer(burst_trie::Version::MIN,
                                 get_postings_writer(consolidation, rm), rm,
                                 consolidation);
}

field_reader::ptr format10::get_field_reader(IResourceManager& rm) const {
  return burst_trie::make_reader(get_postings_reader(), rm);
}

columnstore_writer::ptr format10::get_columnstore_writer(
  bool /*consolidation*/, IResourceManager&) const {
  return columnstore::make_writer(columnstore::Version::MIN,
                                  columnstore::ColumnMetaVersion::MIN);
}

columnstore_reader::ptr format10::get_columnstore_reader() const {
  return columnstore::make_reader();
}

irs::postings_writer::ptr format10::get_postings_writer(
  bool consolidation, IResourceManager& rm) const {
  return std::make_unique<::postings_writer<format_traits>>(
    PostingsFormat::POSITIONS_ONEBASED, consolidation, rm);
}

irs::postings_reader::ptr format10::get_postings_reader() const {
  return std::make_unique<::postings_reader<format_traits>>();
}

irs::format::ptr format10::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT10_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format10, MODULE_NAME);

class format11 : public format10 {
 public:
  static constexpr std::string_view type_name() noexcept { return "1_1"; }

  static ptr make();

  index_meta_writer::ptr get_index_meta_writer() const final;

  field_writer::ptr get_field_writer(bool consolidation,
                                     IResourceManager&) const override;

  segment_meta_writer::ptr get_segment_meta_writer() const final;

  columnstore_writer::ptr get_columnstore_writer(
    bool /*consolidation*/, IResourceManager&) const override;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format11>::id();
  }
};

static const ::format11 FORMAT11_INSTANCE;

IndexMetaWriter::ptr format11::get_index_meta_writer() const {
  return std::make_unique<IndexMetaWriter>(IndexMetaWriter::kFormatMax);
}

field_writer::ptr format11::get_field_writer(bool consolidation,
                                             IResourceManager& rm) const {
  return burst_trie::make_writer(burst_trie::Version::ENCRYPTION_MIN,
                                 get_postings_writer(consolidation, rm), rm,
                                 consolidation);
}

segment_meta_writer::ptr format11::get_segment_meta_writer() const {
  // can reuse stateless writer
  static SegmentMetaWriter kInstance{SegmentMetaWriter::FORMAT_MAX};
  return memory::to_managed<segment_meta_writer>(kInstance);
}

columnstore_writer::ptr format11::get_columnstore_writer(
  bool /*consolidation*/, IResourceManager&) const {
  return columnstore::make_writer(columnstore::Version::MIN,
                                  columnstore::ColumnMetaVersion::MAX);
}

irs::format::ptr format11::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT11_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format11, MODULE_NAME);

class format12 : public format11 {
 public:
  static constexpr std::string_view type_name() noexcept { return "1_2"; }

  static ptr make();

  columnstore_writer::ptr get_columnstore_writer(
    bool /*consolidation*/, IResourceManager&) const override;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format12>::id();
  }
};

static const ::format12 FORMAT12_INSTANCE;

columnstore_writer::ptr format12::get_columnstore_writer(
  bool /*consolidation*/, IResourceManager&) const {
  return columnstore::make_writer(columnstore::Version::MAX,
                                  columnstore::ColumnMetaVersion::MAX);
}

irs::format::ptr format12::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT12_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format12, MODULE_NAME);

class format13 : public format12 {
 public:
  using format_traits = ::format_traits<false, pos_limits::invalid()>;

  static constexpr std::string_view type_name() noexcept { return "1_3"; }

  static ptr make();

  irs::postings_writer::ptr get_postings_writer(
    bool consolidation, IResourceManager&) const override;
  irs::postings_reader::ptr get_postings_reader() const override;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format13>::id();
  }
};

static const ::format13 FORMAT13_INSTANCE;

irs::postings_writer::ptr format13::get_postings_writer(
  bool consolidation, IResourceManager& rm) const {
  return std::make_unique<::postings_writer<format_traits>>(
    PostingsFormat::POSITIONS_ZEROBASED, consolidation, rm);
}

irs::postings_reader::ptr format13::get_postings_reader() const {
  return std::make_unique<::postings_reader<format_traits>>();
}

irs::format::ptr format13::make() {
  static const ::format13 INSTANCE;

  return irs::format::ptr(irs::format::ptr(), &FORMAT13_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format13, MODULE_NAME);

class format14 : public format13 {
 public:
  static constexpr std::string_view type_name() noexcept { return "1_4"; }

  static ptr make();

  irs::field_writer::ptr get_field_writer(bool consolidation,
                                          IResourceManager& rm) const override;

  irs::columnstore_writer::ptr get_columnstore_writer(
    bool consolidation, IResourceManager& rm) const final;
  irs::columnstore_reader::ptr get_columnstore_reader() const final;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format14>::id();
  }
};

static const ::format14 FORMAT14_INSTANCE;

irs::field_writer::ptr format14::get_field_writer(bool consolidation,
                                                  IResourceManager& rm) const {
  return burst_trie::make_writer(burst_trie::Version::IMMUTABLE_FST,
                                 get_postings_writer(consolidation, rm), rm,
                                 consolidation);
}

columnstore_writer::ptr format14::get_columnstore_writer(
  bool consolidation, IResourceManager& rm) const {
  return columnstore2::make_writer(columnstore2::Version::kMin, consolidation,
                                   rm);
}

columnstore_reader::ptr format14::get_columnstore_reader() const {
  return columnstore2::make_reader();
}

irs::format::ptr format14::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT14_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format14, MODULE_NAME);

class format15 : public format14 {
 public:
  using format_traits = ::format_traits<true, pos_limits::invalid()>;

  static constexpr std::string_view type_name() noexcept { return "1_5"; }

  static ptr make();

  irs::field_writer::ptr get_field_writer(bool consolidation,
                                          IResourceManager& rm) const final;

  irs::postings_writer::ptr get_postings_writer(bool consolidation,
                                                IResourceManager&) const final;
  irs::postings_reader::ptr get_postings_reader() const final;

  irs::type_info::type_id type() const noexcept final {
    return irs::type<format15>::id();
  }
};

static const ::format15 FORMAT15_INSTANCE;

irs::field_writer::ptr format15::get_field_writer(bool consolidation,
                                                  IResourceManager& rm) const {
  return burst_trie::make_writer(burst_trie::Version::WAND,
                                 get_postings_writer(consolidation, rm), rm,
                                 consolidation);
}

irs::postings_writer::ptr format15::get_postings_writer(
  bool consolidation, IResourceManager& rm) const {
  return std::make_unique<::postings_writer<format_traits>>(
    PostingsFormat::WAND, consolidation, rm);
}

irs::postings_reader::ptr format15::get_postings_reader() const {
  return std::make_unique<::postings_reader<format_traits>>();
}

irs::format::ptr format15::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT15_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format15, MODULE_NAME);

#ifdef IRESEARCH_SSE2

template<bool Wand, uint32_t PosMin>
struct format_traits_sse4 {
  using align_type = __m128i;

  static constexpr bool wand() noexcept { return Wand; }
  static constexpr uint32_t pos_min() noexcept { return PosMin; }
  static constexpr uint32_t block_size() noexcept { return SIMDBlockSize; }
  static_assert(block_size() <= doc_limits::eof());

  IRS_FORCE_INLINE static void pack_block(const uint32_t* IRS_RESTRICT decoded,
                                          uint32_t* IRS_RESTRICT encoded,
                                          uint32_t bits) noexcept {
    ::simdpackwithoutmask(decoded, reinterpret_cast<align_type*>(encoded),
                          bits);
  }

  IRS_FORCE_INLINE static void unpack_block(
    uint32_t* IRS_RESTRICT decoded, const uint32_t* IRS_RESTRICT encoded,
    uint32_t bits) noexcept {
    ::simdunpack(reinterpret_cast<const align_type*>(encoded), decoded, bits);
  }

  IRS_FORCE_INLINE static void write_block(index_output& out,
                                           const uint32_t* in, uint32_t* buf) {
    bitpack::write_block32<block_size()>(pack_block, out, in, buf);
  }

  IRS_FORCE_INLINE static void read_block(index_input& in, uint32_t* buf,
                                          uint32_t* out) {
    bitpack::read_block32<block_size()>(unpack_block, in, buf, out);
  }

  IRS_FORCE_INLINE static void skip_block(index_input& in) {
    bitpack::skip_block32(in, block_size());
  }
};

class format12simd final : public format12 {
 public:
  using format_traits = format_traits_sse4<false, pos_limits::min()>;

  static constexpr std::string_view type_name() noexcept { return "1_2simd"; }

  static ptr make();

  irs::postings_writer::ptr get_postings_writer(bool consolidation,
                                                IResourceManager&) const final;
  irs::postings_reader::ptr get_postings_reader() const final;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format12simd>::id();
  }
};

static const ::format12simd FORMAT12SIMD_INSTANCE;

irs::postings_writer::ptr format12simd::get_postings_writer(
  bool consolidation, IResourceManager& rm) const {
  return std::make_unique<::postings_writer<format_traits>>(
    PostingsFormat::POSITIONS_ONEBASED_SSE, consolidation, rm);
}

irs::postings_reader::ptr format12simd::get_postings_reader() const {
  return std::make_unique<::postings_reader<format_traits>>();
}

irs::format::ptr format12simd::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT12SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format12simd, MODULE_NAME);

class format13simd : public format13 {
 public:
  using format_traits = format_traits_sse4<false, pos_limits::invalid()>;

  static constexpr std::string_view type_name() noexcept { return "1_3simd"; }

  static ptr make();

  irs::postings_writer::ptr get_postings_writer(
    bool consolidation, IResourceManager&) const override;
  irs::postings_reader::ptr get_postings_reader() const override;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format13simd>::id();
  }
};

static const ::format13simd FORMAT13SIMD_INSTANCE;

irs::postings_writer::ptr format13simd::get_postings_writer(
  bool consolidation, IResourceManager& rm) const {
  return std::make_unique<::postings_writer<format_traits>>(
    PostingsFormat::POSITIONS_ZEROBASED_SSE, consolidation, rm);
}

irs::postings_reader::ptr format13simd::get_postings_reader() const {
  return std::make_unique<::postings_reader<format_traits>>();
}

irs::format::ptr format13simd::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT13SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format13simd, MODULE_NAME);

class format14simd : public format13simd {
 public:
  using format_traits = format_traits_sse4<true, pos_limits::invalid()>;

  static constexpr std::string_view type_name() noexcept { return "1_4simd"; }

  static ptr make();

  columnstore_writer::ptr get_columnstore_writer(bool consolidation,
                                                 IResourceManager&) const final;
  columnstore_reader::ptr get_columnstore_reader() const final;

  irs::field_writer::ptr get_field_writer(bool consolidation,
                                          IResourceManager&) const override;

  irs::type_info::type_id type() const noexcept override {
    return irs::type<format14simd>::id();
  }
};

static const ::format14simd FORMAT14SIMD_INSTANCE;

irs::field_writer::ptr format14simd::get_field_writer(
  bool consolidation, IResourceManager& rm) const {
  return burst_trie::make_writer(burst_trie::Version::IMMUTABLE_FST,
                                 get_postings_writer(consolidation, rm), rm,
                                 consolidation);
}

columnstore_writer::ptr format14simd::get_columnstore_writer(
  bool consolidation, IResourceManager& rm) const {
  return columnstore2::make_writer(columnstore2::Version::kMin, consolidation,
                                   rm);
}

columnstore_reader::ptr format14simd::get_columnstore_reader() const {
  return columnstore2::make_reader();
}

irs::format::ptr format14simd::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT14SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format14simd, MODULE_NAME);

class format15simd : public format14simd {
 public:
  using format_traits = format_traits_sse4<true, pos_limits::invalid()>;

  static constexpr std::string_view type_name() noexcept { return "1_5simd"; }

  static ptr make();

  irs::field_writer::ptr get_field_writer(bool consolidation,
                                          IResourceManager&) const final;

  irs::postings_writer::ptr get_postings_writer(bool consolidation,
                                                IResourceManager&) const final;
  irs::postings_reader::ptr get_postings_reader() const final;

  irs::type_info::type_id type() const noexcept final {
    return irs::type<format15simd>::id();
  }
};

static const ::format15simd FORMAT15SIMD_INSTANCE;

irs::field_writer::ptr format15simd::get_field_writer(
  bool consolidation, IResourceManager& rm) const {
  return burst_trie::make_writer(burst_trie::Version::WAND,
                                 get_postings_writer(consolidation, rm), rm,
                                 consolidation);
}

irs::postings_writer::ptr format15simd::get_postings_writer(
  bool consolidation, IResourceManager& rm) const {
  return std::make_unique<::postings_writer<format_traits>>(
    PostingsFormat::WAND_SSE, consolidation, rm);
}

irs::postings_reader::ptr format15simd::get_postings_reader() const {
  return std::make_unique<::postings_reader<format_traits>>();
}

irs::format::ptr format15simd::make() {
  return irs::format::ptr(irs::format::ptr(), &FORMAT15SIMD_INSTANCE);
}

REGISTER_FORMAT_MODULE(::format15simd, MODULE_NAME);

#endif  // IRESEARCH_SSE2

}  // namespace

namespace irs {
namespace version10 {

void init() {
  REGISTER_FORMAT(::format10);
  REGISTER_FORMAT(::format11);
  REGISTER_FORMAT(::format12);
  REGISTER_FORMAT(::format13);
  REGISTER_FORMAT(::format14);
  REGISTER_FORMAT(::format15);
#ifdef IRESEARCH_SSE2
  REGISTER_FORMAT(::format12simd);
  REGISTER_FORMAT(::format13simd);
  REGISTER_FORMAT(::format14simd);
  REGISTER_FORMAT(::format15simd);
#endif  // IRESEARCH_SSE2
}

}  // namespace version10

// use base irs::position type for ancestors
template<typename IteratorTraits, typename FieldTraits, bool Position>
struct type<::position<IteratorTraits, FieldTraits, Position>>
  : type<irs::position> {};

}  // namespace irs
