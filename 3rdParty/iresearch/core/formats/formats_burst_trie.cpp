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

#include "formats_burst_trie.hpp"

#include <cassert>
#include <variant>
#include <list>

#include "utils/fstext/fst_utils.hpp"

#if defined(_MSC_VER)
  #pragma warning(disable : 4291)
#elif defined (__GNUC__)
  // NOOP
#endif

#include "fst/matcher.h"

#if defined(_MSC_VER)
  #pragma warning(default: 4291)
#elif defined (__GNUC__)
  // NOOP
#endif

#if defined(_MSC_VER)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#include "boost/crc.hpp"

#if defined(_MSC_VER)
  #pragma warning(default: 4244)
  #pragma warning(default: 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#include "format_utils.hpp"
#include "analysis/token_attributes.hpp"
#include "formats/formats_10_attributes.hpp"
#include "index/iterators.hpp"
#include "index/index_meta.hpp"
#include "index/field_meta.hpp"
#include "index/file_names.hpp"
#include "index/index_meta.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/automaton.hpp"
#include "utils/buffers.hpp"
#include "utils/encryption.hpp"
#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"
#include "utils/memory_pool.hpp"
#include "utils/noncopyable.hpp"
#include "utils/directory_utils.hpp"
#include "utils/fstext/fst_string_weight.h"
#include "utils/fstext/fst_builder.hpp"
#include "utils/fstext/fst_decl.hpp"
#include "utils/fstext/fst_matcher.hpp"
#include "utils/fstext/fst_string_ref_weight.h"
#include "utils/fstext/fst_table_matcher.hpp"
#include "utils/fstext/immutable_fst.h"
#include "utils/timer_utils.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitset.hpp"
#include "utils/frozen_attributes.hpp"
#include "utils/string.hpp"
#include "utils/log.hpp"

namespace  {

using namespace irs;

template<typename Char>
class volatile_ref : util::noncopyable {
 public:
  typedef irs::basic_string_ref<Char> ref_t;
  typedef std::basic_string<Char> str_t;

  volatile_ref() = default;

  volatile_ref(volatile_ref&& rhs) noexcept
   : str_(std::move(rhs.str_)),
     ref_(str_.empty() ? rhs.ref_ : ref_t(str_)) {
    rhs.ref_ = ref_;
  }

  volatile_ref& operator=(volatile_ref&& rhs) noexcept {
    if (this != &rhs) {
      str_ = std::move(rhs.str_);
      ref_ = (str_.empty() ? rhs.ref_ : ref_t(str_));
      rhs.ref_ = ref_;
    }
    return *this;
  }

  void clear() {
    str_.clear();
    ref_ = ref_t::NIL;
  }

  template<bool Volatile>
  void assign(const ref_t& str) {
    if constexpr (Volatile) {
      str_.assign(str.c_str(), str.size());
      ref_ = str_;
    } else {
      ref_ = str;
      str_.clear();
    }
  }

  FORCE_INLINE void assign(const ref_t& str, bool Volatile) {
    (Volatile ? volatile_ref<Char>::assign<true>(str)
              : volatile_ref<Char>::assign<false>(str));
  }

  void assign(const ref_t& str, Char label) {
    str_.resize(str.size() + 1);
    std::memcpy(&str_[0], str.c_str(), str.size() * sizeof(Char));
    str_[str.size()] = label;
    ref_ = str_;
  }

  operator const ref_t&() const noexcept {
    return ref_;
  }

 private:
  str_t str_;
  ref_t ref_{ ref_t::NIL };
}; // volatile_ref

using volatile_byte_ref = volatile_ref<byte_type>;

using feature_map_t = std::vector<irs::type_info::type_id>;

///////////////////////////////////////////////////////////////////////////////
/// @class block_t
/// @brief block of terms
///////////////////////////////////////////////////////////////////////////////
struct block_t : private util::noncopyable {
  struct prefixed_output final : data_output, private util::noncopyable {
    explicit prefixed_output(volatile_byte_ref&& prefix) noexcept
     : prefix(std::move(prefix)) {
    }

    virtual void close() override {}

    virtual void write_byte(byte_type b) override final {
      weight.PushBack(b);
    }

    virtual void write_bytes(const byte_type* b, size_t len) override final {
      weight.PushBack(b, b + len);
    }

    byte_weight weight;
    volatile_byte_ref prefix;
  }; // prefixed_output

  static constexpr int16_t INVALID_LABEL = -1;

  block_t(uint64_t block_start, byte_type meta, int16_t label) noexcept
    : start(block_start),
      label(label),
      meta(meta) {
  }

  block_t(block_t&& rhs) noexcept
    : index(std::move(rhs.index)),
      start(rhs.start),
      label(rhs.label),
      meta(rhs.meta) {
  }

  block_t& operator=(block_t&& rhs) noexcept {
    if (this != &rhs) {
      index = std::move(rhs.index);
      start = rhs.start;
      label = rhs.label;
      meta =  rhs.meta;
    }
    return *this;
  }

  std::list<prefixed_output> index; // fst index data
  uint64_t start; // file pointer
  int16_t label;  // block lead label
  byte_type meta; // block metadata
}; // block_t

// FIXME std::is_nothrow_move_constructible_v<std::list<...>> == false
static_assert(std::is_nothrow_move_constructible_v<block_t>);
// FIXME std::is_nothrow_move_assignable_v<std::list<...>> == false
static_assert(std::is_nothrow_move_assignable_v<block_t>);

///////////////////////////////////////////////////////////////////////////////
/// @enum EntryType
///////////////////////////////////////////////////////////////////////////////
enum EntryType : byte_type {
  ET_TERM = 0,
  ET_BLOCK,
  ET_INVALID
}; // EntryType

///////////////////////////////////////////////////////////////////////////////
/// @class entry
/// @brief block or term
///////////////////////////////////////////////////////////////////////////////
class entry : private util::noncopyable {
 public:
  entry(const irs::bytes_ref& term, irs::postings_writer::state&& attrs, bool volatile_term);
  entry(const irs::bytes_ref& prefix, uint64_t block_start,
        byte_type meta, int16_t label, bool volatile_term);
  entry(entry&& rhs) noexcept;
  entry& operator=(entry&& rhs) noexcept;
  ~entry() noexcept;

  const irs::postings_writer::state& term() const noexcept {
    return *mem_.as<irs::postings_writer::state>();
  }

  irs::postings_writer::state& term() noexcept {
    return *mem_.as<irs::postings_writer::state>();
  }

  const block_t& block() const noexcept { return *mem_.as<block_t>(); }
  block_t& block() noexcept { return *mem_.as<block_t>(); }

  const volatile_byte_ref& data() const noexcept { return data_; }
  volatile_byte_ref& data() noexcept { return data_; }

  EntryType type() const noexcept { return type_; }

 private:
  void destroy() noexcept;
  void move_union(entry&& rhs) noexcept;

  volatile_byte_ref data_; // block prefix or term
  memory::aligned_type<irs::postings_writer::state, block_t> mem_; // storage
  EntryType type_; // entry type
}; // entry

entry::entry(
    const irs::bytes_ref& term,
    irs::postings_writer::state&& attrs,
    bool volatile_term)
  : type_(ET_TERM) {
  data_.assign(term, volatile_term);

  mem_.construct<irs::postings_writer::state>(std::move(attrs));
}

entry::entry(
    const irs::bytes_ref& prefix,
    uint64_t block_start,
    byte_type meta,
    int16_t label,
    bool volatile_term)
  : type_(ET_BLOCK) {
  if (block_t::INVALID_LABEL != label) {
    data_.assign(prefix, static_cast<byte_type>(label & 0xFF));
  } else {
    data_.assign(prefix, volatile_term);
  }

  mem_.construct<block_t>(block_start, meta, label);
}

entry::entry(entry&& rhs) noexcept
  : data_(std::move(rhs.data_)),
    type_(rhs.type_) {
  move_union(std::move(rhs));
}

entry& entry::operator=(entry&& rhs) noexcept{
  if (this != &rhs) {
    data_ = std::move(rhs.data_);
    type_ = rhs.type_;
    destroy();
    move_union(std::move(rhs));
  }

  return *this;
}

void entry::move_union(entry&& rhs) noexcept {
  switch (rhs.type_) {
    case ET_TERM  : mem_.construct<irs::postings_writer::state>(std::move(rhs.term())); break;
    case ET_BLOCK : mem_.construct<block_t>(std::move(rhs.block())); break;
    default: break;
  }

  // prevent deletion
  rhs.type_ = ET_INVALID;
}

void entry::destroy() noexcept {
  switch (type_) {
    case ET_TERM  : mem_.destroy<irs::postings_writer::state>(); break;
    case ET_BLOCK : mem_.destroy<block_t>(); break;
    default: break;
  }
}

entry::~entry() noexcept {
  destroy();
}

///////////////////////////////////////////////////////////////////////////////
/// @struct block_meta
/// @brief Provides set of helper functions to work with block metadata
///////////////////////////////////////////////////////////////////////////////
struct block_meta {
  // mask bit layout:
  // 0 - has terms
  // 1 - has sub blocks
  // 2 - is floor block

  // block has terms
  static bool terms(byte_type mask) noexcept { return check_bit<ET_TERM>(mask); }

  // block has sub-blocks
  static bool blocks(byte_type mask) noexcept { return check_bit<ET_BLOCK>(mask); }

  static void type(byte_type& mask, EntryType type) noexcept { set_bit(mask, type); }

  // block is floor block
  static bool floor(byte_type mask) noexcept { return check_bit<ET_INVALID>(mask); }
  static void floor(byte_type& mask, bool b) noexcept { set_bit<ET_INVALID>(b, mask); }

  // resets block meta
  static void reset(byte_type mask) noexcept {
    unset_bit<ET_TERM>(mask);
    unset_bit<ET_BLOCK>(mask);
  }
}; // block_meta

// -----------------------------------------------------------------------------
// --SECTION--                                                           Helpers
// -----------------------------------------------------------------------------

void read_segment_features(
    data_input& in,
    feature_map_t& feature_map,
    flags& features) {
  feature_map.clear();
  feature_map.reserve(in.read_vlong());

  features.reserve(feature_map.capacity());

  for (size_t count = feature_map.capacity(); count; --count) {
    const auto name = read_string<std::string>(in); // read feature name
    const irs::type_info feature = attributes::get(name);

    if (!feature) {
      throw irs::index_error(irs::string_utils::to_string(
        "unknown feature name '%s'",
        name.c_str()
      ));
    }

    feature_map.emplace_back(feature.id());
    features.add(feature.id());
  }
}

void read_field_features(
    data_input& in,
    const feature_map_t& feature_map,
    flags& features) {
  for (size_t count = in.read_vlong(); count; --count) {
    const size_t id = in.read_vlong(); // feature id

    if (id < feature_map.size()) {
      features.add(feature_map[id]);
    } else {
      throw irs::index_error(irs::string_utils::to_string(
        "unknown feature id '" IR_SIZE_T_SPECIFIER "'", id
      ));
    }
  }
}

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
      str.c_str()
    ));
  }

  format_utils::write_header(*out, format, version);
}

inline int32_t prepare_input(
    std::string& str,
    index_input::ptr& in,
    irs::IOAdvice advice,
    const reader_state& state,
    const string_ref& ext,
    const string_ref& format,
    const int32_t min_ver,
    const int32_t max_ver,
    int64_t* checksum = nullptr) {
  assert(!in);

  file_name(str, state.meta->name, ext);
  in = state.dir->open(str, advice);

  if (!in) {
    throw io_error(string_utils::to_string(
      "failed to open file, path: %s",
      str.c_str()
    ));
  }

  if (checksum) {
    *checksum = format_utils::checksum(*in);
  }

  return format_utils::check_header(*in, format, min_ver, max_ver);
}

///////////////////////////////////////////////////////////////////////////////
/// @struct cookie
///////////////////////////////////////////////////////////////////////////////
struct cookie : irs::seek_term_iterator::seek_cookie {
  cookie(const version10::term_meta& meta, uint64_t term_freq)
    : meta(meta), term_freq(term_freq) {
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief declaration/implementation of DECLARE_FACTORY()
  //////////////////////////////////////////////////////////////////////////////
  static seek_term_iterator::seek_cookie::ptr make(const version10::term_meta& meta, uint64_t term_freq) {
    return memory::make_unique<cookie>(meta, term_freq);
  }

  version10::term_meta meta; // term metadata
  uint64_t term_freq; // length of the positions list
}; // cookie

const fst::FstWriteOptions& fst_write_options() {
  static const auto INSTANCE = [](){
    fst::FstWriteOptions options;
    options.write_osymbols = false; // we don't need output symbols
    options.stream_write = true;

    return options;
  }();

  return INSTANCE;
}

const fst::FstReadOptions& fst_read_options() {
  static const auto INSTANCE = [](){
    fst::FstReadOptions options;
    options.read_osymbols = false; // we don't need output symbols

    return options;
  }();

  return INSTANCE;
}

// mininum size of string weight we store in FST
constexpr const size_t MIN_WEIGHT_SIZE = 2;

void merge_blocks(std::list<entry>& blocks) {
  assert(!blocks.empty());

  auto it = blocks.begin();

  auto& root = *it;
  auto& root_block = root.block();
  auto& root_index = root_block.index;

  root_index.emplace_front(std::move(root.data()));

  // First byte in block header must not be equal to fst::kStringInfinity
  // Consider the following:
  //   StringWeight0 -> { fst::kStringInfinity 0x11 ... }
  //   StringWeight1 -> { fst::KStringInfinity 0x22 ... }
  //   CommonPrefix = fst::Plus(StringWeight0, StringWeight1) -> { fst::kStringInfinity }
  //   Suffix = fst::Divide(StringWeight1, CommonPrefix) -> { fst::kStringBad }
  // But actually Suffix should be equal to { 0x22 ... }
  assert(char(root_block.meta) != fst::kStringInfinity);

  // will store just several bytes here
  auto& out = *root_index.begin();
  out.write_byte(static_cast<byte_type>(root_block.meta)); // block metadata
  out.write_vlong(root_block.start); // start pointer of the block

  if (block_meta::floor(root_block.meta)) {
    out.write_vlong(static_cast<uint64_t>(blocks.size()-1));
    for (++it; it != blocks.end(); ++it ) {
      const auto* block = &it->block();
      assert(block->label != block_t::INVALID_LABEL);
      assert(block->start > root_block.start);

      const uint64_t start_delta = it->block().start - root_block.start;
      out.write_byte(static_cast<byte_type>(block->label & 0xFF));
      out.write_vlong(start_delta);
      out.write_byte(static_cast<byte_type>(block->meta));

      root_index.splice(root_index.end(), it->block().index);
    }
  } else {
    for (++it; it != blocks.end(); ++it) {
      root_index.splice(root_index.end(), it->block().index);
    }
  }

  // ensure weight we've written doesn't interfere
  // with semiring members and other constants
  assert(
    out.weight != byte_weight::One()
      && out.weight != byte_weight::Zero()
      && out.weight != byte_weight::NoWeight()
      && out.weight.Size() >= MIN_WEIGHT_SIZE
      && byte_weight::One().Size() < MIN_WEIGHT_SIZE
      && byte_weight::Zero().Size() < MIN_WEIGHT_SIZE
      && byte_weight::NoWeight().Size() < MIN_WEIGHT_SIZE
  );
}

///////////////////////////////////////////////////////////////////////////////
/// @class fst_buffer
/// @brief resetable FST buffer
///////////////////////////////////////////////////////////////////////////////
class fst_buffer : public vector_byte_fst {
 public:
  ///////////////////////////////////////////////////////////////////////////////
  /// @class fst_stats
  /// @brief fst builder stats
  ///////////////////////////////////////////////////////////////////////////////
  struct fst_stats : iresearch::fst_stats {
    size_t total_weight_size{};

    void operator()(const byte_weight& w) noexcept {
      total_weight_size += w.Size();
    }

    [[maybe_unused]] bool operator==(const fst_stats& rhs) const noexcept {
      return num_states == rhs.num_states &&
             num_arcs == rhs.num_arcs &&
             total_weight_size == rhs.total_weight_size;
    }
  };

  using fst_byte_builder = fst_builder<byte_type, vector_byte_fst, fst_stats>;

  template<typename Data>
  fst_stats reset(const Data& data) {
    builder.reset();

    for (auto& fst_data: data) {
      builder.add(fst_data.prefix, fst_data.weight);
    }

    return builder.finish();
  }

 private:
  fst_byte_builder builder{*this};
}; // fst_buffer

///////////////////////////////////////////////////////////////////////////////
/// @class field_writer
////////////////////////////////////////////////////////////////////////////////
class field_writer final : public irs::field_writer {
 public:
  static constexpr uint32_t DEFAULT_MIN_BLOCK_SIZE = 25;
  static constexpr uint32_t DEFAULT_MAX_BLOCK_SIZE = 48;

  static constexpr string_ref FORMAT_TERMS = "block_tree_terms_dict";
  static constexpr string_ref TERMS_EXT = "tm";
  static constexpr string_ref FORMAT_TERMS_INDEX = "block_tree_terms_index";
  static constexpr string_ref TERMS_INDEX_EXT = "ti";

  field_writer(
    irs::postings_writer::ptr&& pw,
    bool volatile_state,
    burst_trie::Version version = burst_trie::Version::MAX,
    uint32_t min_block_size = DEFAULT_MIN_BLOCK_SIZE,
    uint32_t max_block_size = DEFAULT_MAX_BLOCK_SIZE);

  virtual ~field_writer();

  virtual void prepare(const irs::flush_state& state) override;

  virtual void end() override;

  virtual void write(
    const std::string& name,
    irs::field_id norm,
    const irs::flags& features,
    irs::term_iterator& terms) override;

 private:
  static constexpr size_t DEFAULT_SIZE = 8;

  void write_segment_features(data_output& out, const flags& features);

  void write_field_features(data_output& out, const flags& features) const;

  void begin_field(const irs::flags& field);

  void end_field(
    const std::string& name,
    irs::field_id norm,
    const irs::flags& features,
    uint64_t total_doc_freq,
    uint64_t total_term_freq,
    size_t doc_count);

  void write_term_entry(const entry& e, size_t prefix, bool leaf);

  void write_block_entry(const entry& e, size_t prefix, uint64_t block_start);

  // prefix - prefix length (in last_term)
  // begin - index of the first entry in the block
  // end - index of the last entry in the block
  // meta - block metadata
  // label - block lead label (if present)
  void write_block(
    std::list<entry>& blocks,
    size_t prefix, size_t begin,
    size_t end, byte_type meta,
    int16_t label);

  // prefix - prefix length ( in last_term
  // count - number of entries to write into block
  void write_blocks(size_t prefix, size_t count);

  void push(const irs::bytes_ref& term);

  std::unordered_map<irs::type_info::type_id, size_t> feature_map_;
  memory_output suffix_; // term suffix column
  memory_output stats_; // term stats column
  encryption::stream::ptr terms_out_cipher_;
  index_output::ptr terms_out_; // output stream for terms
  encryption::stream::ptr index_out_cipher_;
  index_output::ptr index_out_; // output stream for indexes
  postings_writer::ptr pw_; // postings writer
  std::vector<entry> stack_;
  fst_buffer* fst_buf_; // pimpl buffer used for building FST for fields
  volatile_byte_ref last_term_; // last pushed term
  std::vector<size_t> prefixes_;
  std::pair<bool, volatile_byte_ref> min_term_; // current min term in a block
  volatile_byte_ref max_term_; // current max term in a block
  uint64_t term_count_; // count of terms
  size_t fields_count_{};
  const burst_trie::Version version_;
  const uint32_t min_block_size_;
  const uint32_t max_block_size_;
  const bool volatile_state_;
}; // field_writer

void field_writer::write_term_entry(const entry& e, size_t prefix, bool leaf) {
  const irs::bytes_ref& data = e.data();
  const size_t suf_size = data.size() - prefix;
  suffix_.stream.write_vlong(leaf ? suf_size : shift_pack_64(suf_size, false));
  suffix_.stream.write_bytes(data.c_str() + prefix, suf_size);

  pw_->encode(stats_.stream, *e.term());
}

void field_writer::write_block_entry(
    const entry& e, size_t prefix, uint64_t block_start) {
  const irs::bytes_ref& data = e.data();
  const size_t suf_size = data.size() - prefix;
  suffix_.stream.write_vlong(shift_pack_64(suf_size, true));
  suffix_.stream.write_bytes(data.c_str() + prefix, suf_size);

  // current block start pointer should be greater
  assert(block_start > e.block().start );
  suffix_.stream.write_vlong(block_start - e.block().start);
}

void field_writer::write_block(
    std::list<entry>& blocks,
    size_t prefix,
    size_t begin, size_t end,
    irs::byte_type meta,
    int16_t label) {
  assert(end > begin);

  // begin of the block
  const uint64_t block_start = terms_out_->file_pointer();

  // write block header
  terms_out_->write_vint(
    shift_pack_32(static_cast<uint32_t>(end - begin),
                  end == stack_.size())
  );

  // write block entries
  const uint64_t leaf = !block_meta::blocks(meta);

  std::list<block_t::prefixed_output> index;

  pw_->begin_block();

  for (; begin < end; ++begin) {
    auto& e = stack_[begin];
    assert(starts_with(static_cast<const bytes_ref&>(e.data()), bytes_ref(last_term_, prefix)));

    switch (e.type()) {
      case ET_TERM:
        write_term_entry(e, prefix, leaf > 0);
        break;
      case ET_BLOCK: {
        write_block_entry(e, prefix, block_start);
        index.splice(index.end(), e.block().index);
      } break;
      default:
        assert(false);
        break;
    }
  }

  size_t block_size = suffix_.stream.file_pointer();

  suffix_.stream.flush();
  stats_.stream.flush();

  terms_out_->write_vlong(
    shift_pack_64(static_cast<uint64_t>(block_size), leaf > 0)
  );

  auto copy = [this](const irs::byte_type* b, size_t len) {
    terms_out_->write_bytes(b, len);
    return true;
  };

  if (terms_out_cipher_) {
    auto offset = block_start;

    auto encrypt_and_copy = [this, &offset](irs::byte_type* b, size_t len) {
      assert(terms_out_cipher_);

      if (!terms_out_cipher_->encrypt(offset, b, len)) {
        return false;
      }

      terms_out_->write_bytes(b, len);
      offset += len;
      return true;
    };

    if (!suffix_.file.visit(encrypt_and_copy)) {
      throw io_error("failed to encrypt term dictionary");
    }
  } else {
    suffix_.file.visit(copy);
  }

  terms_out_->write_vlong(static_cast<uint64_t>(stats_.stream.file_pointer()));
  stats_.file.visit(copy);

  suffix_.stream.reset();
  stats_.stream.reset();

  // add new block to the list of created blocks
  blocks.emplace_back(
    bytes_ref(last_term_, prefix),
    block_start,
    meta,
    label,
    volatile_state_
  );

  if (!index.empty()) {
    blocks.back().block().index = std::move(index);
  }
}

void field_writer::write_blocks( size_t prefix, size_t count ) {
  // only root node able to write whole stack
  assert(prefix || count == stack_.size());

  // block metadata
  irs::byte_type meta{};

  // created blocks
  std::list<entry> blocks;

  const size_t end = stack_.size();
  const size_t begin = end - count;
  size_t block_start = begin; // begin of current block to write

  size_t min_suffix = integer_traits<size_t>::const_max;
  size_t max_suffix = 0;

  int16_t last_label = block_t::INVALID_LABEL; // last lead suffix label
  int16_t next_label = block_t::INVALID_LABEL; // next lead suffix label in current block
  for (size_t i = begin; i < end; ++i) {
    const entry& e = stack_[i];
    const irs::bytes_ref& data = e.data();

    const size_t suffix = data.size() - prefix;
    min_suffix = std::min(suffix, min_suffix);
    max_suffix = std::max(suffix, max_suffix);

    const int16_t label = data.size() == prefix
      ? block_t::INVALID_LABEL
      : data[prefix];

    if (last_label != label) {
      const size_t block_size = i - block_start;

      if (block_size >= min_block_size_
           && end - block_start > max_block_size_) {
        block_meta::floor(meta, block_size < count);
        write_block( blocks, prefix, block_start, i, meta, next_label );
        next_label = label;
        block_meta::reset(meta);
        block_start = i;
        min_suffix = integer_traits<size_t>::const_max;
        max_suffix = 0;
      }

      last_label = label;
    }

    block_meta::type(meta, e.type());
  }

  // write remaining block
  if ( block_start < end ) {
    block_meta::floor(meta, end - block_start < count);
    write_block(blocks, prefix, block_start, end, meta, next_label);
  }

  // merge blocks into 1st block
  ::merge_blocks(blocks);

  // remove processed entries from the
  // top of the stack
  stack_.erase(stack_.begin() + begin, stack_.end());

  // move root block from temporary storage
  // to the top of the stack
  if (!blocks.empty()) {
    stack_.emplace_back(std::move(blocks.front()));
  }
}

void field_writer::push( const bytes_ref& term ) {
  const irs::bytes_ref& last = last_term_;
  const size_t limit = std::min(last.size(), term.size());

  // find common prefix
  size_t pos = 0;
  while (pos < limit && term[pos] == last[pos]) {
    ++pos;
  }

  for (size_t i = last.empty() ? 0 : last.size() - 1; i > pos;) {
    --i; // should use it here as we use size_t
    const size_t top = stack_.size() - prefixes_[i];
    if (top > min_block_size_) {
      write_blocks(i + 1, top);
      prefixes_[i] -= (top - 1);
    }
  }

  prefixes_.resize(term.size());
  std::fill(prefixes_.begin() + pos, prefixes_.end(), stack_.size());
  last_term_.assign(term, volatile_state_);
}

field_writer::field_writer(
    irs::postings_writer::ptr&& pw,
    bool volatile_state,
    burst_trie::Version version /* = Format::MAX */,
    uint32_t min_block_size /* = DEFAULT_MIN_BLOCK_SIZE */,
    uint32_t max_block_size /* = DEFAULT_MAX_BLOCK_SIZE */)
  : suffix_(memory_allocator::global()),
    stats_(memory_allocator::global()),
    pw_(std::move(pw)),
    fst_buf_(new fst_buffer()),
    prefixes_(DEFAULT_SIZE, 0),
    term_count_(0),
    version_(version),
    min_block_size_(min_block_size),
    max_block_size_(max_block_size),
    volatile_state_(volatile_state) {
  assert(this->pw_);
  assert(min_block_size > 1);
  assert(min_block_size <= max_block_size);
  assert(2 * (min_block_size - 1) <= max_block_size);
  assert(version_ >= burst_trie::Version::MIN && version_ <= burst_trie::Version::MAX);

  min_term_.first = false;
}

field_writer::~field_writer() {
  delete fst_buf_;
}

void field_writer::prepare(const irs::flush_state& state) {
  assert(state.dir);

  // reset writer state
  last_term_.clear();
  max_term_.clear();
  min_term_.first = false;
  min_term_.second.clear();
  prefixes_.assign(DEFAULT_SIZE, 0);
  stack_.clear();
  stats_.reset();
  suffix_.reset();
  term_count_ = 0;
  fields_count_ = 0;

  std::string filename;
  bstring enc_header;
  auto* enc = get_encryption(state.dir->attributes());

  // prepare term dictionary
  prepare_output(filename, terms_out_, state,
                 TERMS_EXT, FORMAT_TERMS,
                 static_cast<int32_t>(version_));

  if (version_ > burst_trie::Version::MIN) {
    // encrypt term dictionary
    const auto encrypt = irs::encrypt(filename, *terms_out_, enc, enc_header, terms_out_cipher_);
    assert(!encrypt || (terms_out_cipher_ && terms_out_cipher_->block_size()));
    UNUSED(encrypt);
  }

  // prepare term index
  prepare_output(filename, index_out_, state,
                 TERMS_INDEX_EXT, FORMAT_TERMS_INDEX,
                 static_cast<int32_t>(version_));

  if (version_ > burst_trie::Version::MIN) {
    // encrypt term index
    if (irs::encrypt(filename, *index_out_, enc, enc_header, index_out_cipher_)) {
      assert(index_out_cipher_ && index_out_cipher_->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        buffered_index_output::DEFAULT_BUFFER_SIZE,
        index_out_cipher_->block_size()
      );

      index_out_ = index_output::make<encrypted_output>(
        std::move(index_out_),
        *index_out_cipher_,
        blocks_in_buffer);
    }
  }

  write_segment_features(*index_out_, *state.features);

  // prepare postings writer
  pw_->prepare(*terms_out_, state);

  // reset allocator from a directory
  auto& allocator = directory_utils::get_allocator(*state.dir);
  suffix_.reset(allocator);
  stats_.reset(allocator);
}

void field_writer::write(
    const std::string& name,
    irs::field_id norm,
    const irs::flags& features,
    irs::term_iterator& terms) {
  REGISTER_TIMER_DETAILED();
  begin_field(features);

  uint64_t sum_dfreq = 0;
  uint64_t sum_tfreq = 0;

  const bool freq_exists = features.check<frequency>();
  auto* docs = irs::get<version10::documents>(*pw_);
  assert(docs);

  for (; terms.next();) {
    auto postings = terms.postings(features);
    auto meta = pw_->write(*postings);

    if (freq_exists) {
      sum_tfreq += meta->freq;
    }

    if (meta->docs_count) {
      sum_dfreq += meta->docs_count;

      const bytes_ref& term = terms.value();
      push(term);

      // push term to the top of the stack
      stack_.emplace_back(term, std::move(meta), volatile_state_);

      if (!min_term_.first) {
        min_term_.first = true;
        min_term_.second.assign(term, volatile_state_);
      }

      max_term_.assign(term, volatile_state_);

      // increase processed term count
      ++term_count_;
    }
  }

  end_field(name, norm, features, sum_dfreq, sum_tfreq, docs->value.count());
}

void field_writer::begin_field(const irs::flags& field) {
  assert(terms_out_);
  assert(index_out_);

  // at the beginning of the field
  // there should be no pending
  // entries at all
  assert(stack_.empty());

  // reset first field term
  min_term_.first = false;
  min_term_.second.clear();
  term_count_ = 0;

  pw_->begin_field(field);
}

void field_writer::write_segment_features(data_output& out, const flags& features) {
  out.write_vlong(features.size());
  feature_map_.clear();
  feature_map_.reserve(features.size());
  for (const irs::type_info::type_id feature : features) {
    write_string(out, feature().name());
    feature_map_.emplace(feature, feature_map_.size());
  }
}

void field_writer::write_field_features(data_output& out, const flags& features) const {
  out.write_vlong(features.size());
  for (auto feature : features) {
    const auto it = feature_map_.find(feature);
    assert(it != feature_map_.end());

    if (feature_map_.end() == it) {
      // should not happen in reality
      throw irs::index_error(string_utils::to_string(
        "feature '%s' is not listed in segment features",
        feature().name().c_str()));
    }

    out.write_vlong(it->second);
  }
}

void field_writer::end_field(
    const std::string& name,
    field_id norm,
    const irs::flags& features,
    uint64_t total_doc_freq,
    uint64_t total_term_freq,
    size_t doc_count) {
  assert(terms_out_);
  assert(index_out_);
  if (!term_count_) {
    // nothing to write
    return;
  }

  // cause creation of all final blocks
  push(bytes_ref::EMPTY);

  // write root block with empty prefix
  write_blocks(0, stack_.size());
  assert(1 == stack_.size());

  // write field meta
  write_string(*index_out_, name);
  write_field_features(*index_out_, features);
  write_zvlong(*index_out_, norm);
  index_out_->write_vlong(term_count_);
  index_out_->write_vlong(doc_count);
  index_out_->write_vlong(total_doc_freq);
  write_string<irs::bytes_ref>(*index_out_, min_term_.second);
  write_string<irs::bytes_ref>(*index_out_, max_term_);
  if (features.check<frequency>()) {
    index_out_->write_vlong(total_term_freq);
  }

  // build fst
  const entry& root = *stack_.begin();
  assert(fst_buf_);
  [[maybe_unused]] const auto fst_stats = fst_buf_->reset(root.block().index);
  const vector_byte_fst& fst = *fst_buf_;

#ifdef IRESEARCH_DEBUG
  // ensure evaluated stats are correct
  struct fst_buffer::fst_stats stats{};
  for (fst::StateIterator<vector_byte_fst> states(fst); !states.Done(); states.Next()) {
    const auto stateid = states.Value();
    ++stats.num_states;
    stats.num_arcs += fst.NumArcs(stateid);
    stats(fst.Final(stateid));
    for (fst::ArcIterator<vector_byte_fst> arcs(fst, stateid); !arcs.Done(); arcs.Next()) {
      stats(arcs.Value().weight);
    }
  }
  assert(stats == fst_stats);
#endif

  // write FST
  if (version_ > burst_trie::Version::ENCRYPTION_MIN) {
    immutable_byte_fst::Write(fst, *index_out_, fst_stats);
  } else {
    // wrap stream to be OpenFST compliant
    output_buf isb(index_out_.get());
    std::ostream os(&isb);
    fst.Write(os, fst_write_options());
  }

  stack_.clear();
  ++fields_count_;
}

void field_writer::end() {
  assert(terms_out_);
  assert(index_out_);

  // finish postings
  pw_->end();

  format_utils::write_footer(*terms_out_);
  terms_out_.reset(); // ensure stream is closed

  if (index_out_cipher_) {
    auto& out = static_cast<encrypted_output&>(*index_out_);
    out.flush();
    index_out_ = out.release();
  }

  index_out_->write_long(fields_count_);
  format_utils::write_footer(*index_out_);
  index_out_.reset(); // ensure stream is closed
}

///////////////////////////////////////////////////////////////////////////////
/// @class term_reader
///////////////////////////////////////////////////////////////////////////////
class term_reader_base : public irs::term_reader,
                         private util::noncopyable {
 public:
  term_reader_base() = default;
  term_reader_base(term_reader_base&& rhs) = default;
  term_reader_base& operator=(term_reader_base&&) = delete;

  virtual const field_meta& meta() const noexcept override { return field_; }
  virtual size_t size() const noexcept override { return terms_count_; }
  virtual uint64_t docs_count() const noexcept override { return doc_count_; }
  virtual const bytes_ref& min() const noexcept override { return min_term_ref_; }
  virtual const bytes_ref& max() const noexcept override { return max_term_ref_; }
  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override;

  virtual void prepare(index_input& in, const feature_map_t& features);

 private:
  bstring min_term_;
  bstring max_term_;
  bytes_ref min_term_ref_;
  bytes_ref max_term_ref_;
  uint64_t terms_count_;
  uint64_t doc_count_;
  uint64_t doc_freq_;
  frequency freq_; // total term freq
  frequency* pfreq_{};
  field_meta field_;
}; // term_reader_base

void term_reader_base::prepare(
    index_input& in,
    const feature_map_t& feature_map) {
  // read field metadata
  field_.name = read_string<std::string>(in);

  read_field_features(in, feature_map, field_.features);

  field_.norm = static_cast<field_id>(read_zvlong(in));
  terms_count_ = in.read_vlong();
  doc_count_ = in.read_vlong();
  doc_freq_ = in.read_vlong();
  min_term_ = read_string<bstring>(in);
  min_term_ref_ = min_term_;
  max_term_ = read_string<bstring>(in);
  max_term_ref_ = max_term_;

  if (field_.features.check<frequency>()) {
    freq_.value = in.read_vlong();
    pfreq_ = &freq_;
  }
}

attribute* term_reader_base::get_mutable(irs::type_info::type_id type) noexcept {
  return irs::type<irs::frequency>::id() == type ? pfreq_ : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
/// @class block_iterator
///////////////////////////////////////////////////////////////////////////////
class block_iterator : util::noncopyable {
 public:
  static constexpr uint64_t UNDEFINED = integer_traits<uint64_t>::const_max;

  block_iterator(byte_weight&& header, size_t prefix) noexcept;

  block_iterator(const bytes_ref& header, size_t prefix)
    : block_iterator(byte_weight(header), prefix) {
  }

  block_iterator(uint64_t start, size_t prefix) noexcept
    : start_(start),
      cur_start_(start),
      cur_end_(start),
      prefix_(prefix),
      sub_count_(UNDEFINED) {
  }

  void load(index_input& in, encryption::stream* cipher);

  bool next_sub_block() noexcept {
    if (!sub_count_) {
      return false;
    }

    cur_start_ = cur_end_;
    if (sub_count_ != UNDEFINED) {
      --sub_count_;
    }
    dirty_ = true;
    return true;
  }

  template<typename Reader>
  void next(Reader& reader) {
    assert(!dirty_ && cur_ent_ < ent_count_);
    if (leaf_) {
      read_entry_leaf(reader);
    } else {
      read_entry_nonleaf(reader);
    }
    ++cur_ent_;
  }

  void reset();

  const version10::term_meta& state() const noexcept { return state_; }
  bool dirty() const noexcept { return dirty_; }
  byte_type meta() const noexcept { return cur_meta_; }
  size_t prefix() const noexcept { return prefix_; }
  EntryType type() const noexcept { return cur_type_; }
  uint64_t block_start() const noexcept { return cur_block_start_; }
  int16_t next_label() const noexcept { return next_label_; }
  uint64_t sub_count() const noexcept { return sub_count_; }
  uint64_t start() const noexcept { return start_; }
  bool end() const noexcept { return cur_ent_ == ent_count_; }
  uint64_t size() const noexcept { return ent_count_; }

  template<typename Reader>
  SeekResult scan_to_term(const bytes_ref& term, Reader& reader) {
    assert(!dirty_);

    const SeekResult res = leaf_
      ? scan_to_term_leaf(term)
      : scan_to_term_nonleaf(term);

    reader(suffix_, suffix_length_);

    return res;
  }

  // scan to floor block
  void scan_to_sub_block(byte_type label);

  // scan to entry with the following start address
  void scan_to_block(uint64_t ptr);

  // read attributes
  void load_data(const field_meta& meta, attribute_provider& attrs,
                 version10::term_meta& state, irs::postings_reader& pr);

 private:
  template<typename Reader>
  void read_entry_leaf(Reader& reader) {
    assert(leaf_ && cur_ent_ < ent_count_);
    cur_type_ = ET_TERM; // always term
    ++term_count_;
    suffix_length_ = vread<uint64_t>(suffix_begin_);
    suffix_ = suffix_begin_;
    reader(suffix_, suffix_length_);
    suffix_begin_ += suffix_length_;
    assert(suffix_begin_ <= suffix_end_);
  }

  template<typename Reader>
  void read_entry_nonleaf(Reader& reader);

  SeekResult scan_to_term_nonleaf(const bytes_ref& term);
  SeekResult scan_to_term_leaf(const bytes_ref& term);

  byte_weight header_; // block header
  bstring suffix_block_; // suffix data block
  bstring stats_block_; // statis data block
  const byte_type* header_begin_{header_.c_str()}; // beginning of block header stream
  const byte_type* suffix_begin_{suffix_block_.c_str()}; // beginning of suffix input stream
  const byte_type* stats_begin_{stats_block_.c_str()}; // beginning of suffix stats stream
#ifdef IRESEARCH_DEBUG
  const byte_type* header_end_{header_begin_}; // end of valid header input stream
  const byte_type* suffix_end_{suffix_begin_ + suffix_block_.size()}; // end of valid suffix input stream
  const byte_type* stats_end_{stats_begin_ + stats_block_.size()}; // end of valid stats input stream
#endif
  const byte_type* suffix_{}; // current suffix
  uint64_t suffix_length_{}; // current suffix length
  version10::term_meta state_;
  uint64_t cur_ent_{}; // current entry in a block
  uint64_t ent_count_{}; // number of entries in a current block
  uint64_t start_; // initial block start pointer
  uint64_t cur_start_; // current block start pointer
  uint64_t cur_end_; // block end pointer
  uint64_t term_count_{}; // number terms in a block we have seen
  uint64_t cur_stats_ent_{}; // current position of loaded stats
  uint64_t cur_block_start_{ UNDEFINED }; // start pointer of the current sub-block entry
  size_t prefix_; // block prefix length
  uint64_t sub_count_; // number of sub-blocks
  int16_t next_label_{ block_t::INVALID_LABEL }; // next label (of the next sub-block)
  EntryType cur_type_{ ET_INVALID }; // term or block
  byte_type meta_{ }; // initial block metadata
  byte_type cur_meta_{ }; // current block metadata
  bool dirty_{ true }; // current block is dirty
  bool leaf_{ false }; // current block is leaf block
}; // block_iterator

block_iterator::block_iterator(byte_weight&& header, size_t prefix) noexcept
  : header_(std::move(header)),
    prefix_(prefix),
    sub_count_(0) {
#ifdef IRESEARCH_DEBUG
  header_end_ = header_.c_str() + header_.Size();
#endif
  cur_meta_ = meta_ = *header_begin_++;
  cur_end_ = cur_start_ = start_ = vread<uint64_t>(header_begin_);
  if (block_meta::floor(meta_)) {
    sub_count_ = vread<uint64_t>(header_begin_);
    next_label_ = *header_begin_++;
  }
  assert(header_begin_ <= header_end_);
}

void block_iterator::load(index_input& in, irs::encryption::stream* cipher) {
  if (!dirty_) {
    return;
  }

  in.seek(cur_start_);
  if (shift_unpack_64(in.read_vint(), ent_count_)) {
    sub_count_ = 0; // no sub-blocks
  }

  // read suffix block
  uint64_t block_size;
  leaf_ = shift_unpack_64(in.read_vlong(), block_size);

  // for non-encrypted index try direct buffer access first
  suffix_begin_ = cipher ? nullptr : in.read_buffer(block_size, BufferHint::PERSISTENT);

  if (!suffix_begin_) {
    string_utils::oversize(suffix_block_, block_size);
#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(&(suffix_block_[0]), block_size);
    assert(read == block_size);
    UNUSED(read);
#else
    in.read_bytes(&(suffix_block_[0]), block_size);
#endif // IRESEARCH_DEBUG
    suffix_begin_ = suffix_block_.c_str();

    if (cipher) {
      cipher->decrypt(cur_start_, &(suffix_block_[0]), block_size);
    }
  }
#ifdef IRESEARCH_DEBUG
  suffix_end_ = suffix_begin_ + block_size;
#endif // IRESEARCH_DEBUG

  // read stats block
  block_size = in.read_vlong();

  // try direct buffer access first
  stats_begin_ = in.read_buffer(block_size, BufferHint::PERSISTENT);

  if (!stats_begin_) {
    string_utils::oversize(stats_block_, block_size);
#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(&(stats_block_[0]), block_size);
    assert(read == block_size);
    UNUSED(read);
#else
    in.read_bytes(&(stats_block_[0]), block_size);
#endif // IRESEARCH_DEBUG
    stats_begin_ = stats_block_.c_str();
  }
#ifdef IRESEARCH_DEBUG
  stats_end_ = stats_begin_ + block_size;
#endif // IRESEARCH_DEBUG

  cur_end_ = in.file_pointer();
  cur_ent_ = 0;
  cur_block_start_ = UNDEFINED;
  term_count_ = 0;
  cur_stats_ent_ = 0;
  dirty_ = false;
}

template<typename Reader>
void block_iterator::read_entry_nonleaf(Reader& reader) {
  assert(!leaf_ && cur_ent_ < ent_count_);

  cur_type_ = shift_unpack_64(vread<uint64_t>(suffix_begin_), suffix_length_)
    ? ET_BLOCK
    : ET_TERM;

  suffix_ = suffix_begin_;
  suffix_begin_ += suffix_length_;
  assert(suffix_begin_ <= suffix_end_);

  switch (cur_type_) {
    case ET_TERM: ++term_count_; break;
    case ET_BLOCK: cur_block_start_ = cur_start_ - vread<uint64_t>(suffix_begin_); break;
    default: assert(false); break;
  }

  // read after state is updated
  reader(suffix_, suffix_length_);

  assert(suffix_begin_ <= suffix_end_);
}

SeekResult block_iterator::scan_to_term_leaf(const bytes_ref& term) {
  assert(leaf_);
  assert(!dirty_);

  while (cur_ent_ < ent_count_) {
    ++cur_ent_;
    ++term_count_;
    cur_type_ = ET_TERM;
    suffix_length_ = vread<uint64_t>(suffix_begin_);
    assert(suffix_begin_ <= suffix_end_);
    suffix_ = suffix_begin_; // start of the current suffix
    suffix_begin_ += suffix_length_; // skip to the next term
    assert(suffix_begin_ <= suffix_end_);

    const byte_type* suffix = suffix_;
    const byte_type* target = term.c_str() + prefix_;
    const size_t target_len = prefix_ + suffix_length_;
    const byte_type* target_end = term.c_str() + std::min(term.size(), target_len);

    ptrdiff_t cmp;
    for (bool stop = false;;) {
      if (target < target_end) {
        cmp = *suffix - *target;
        ++target;
        ++suffix;
      } else {
        assert(target == target_end);
        cmp = target_len - term.size();
        stop = true;
      }

      if (cmp < 0) {
        // we before the target, move to next entry
        break;
      } else if (cmp > 0) {
        // we after the target, not found
        return SeekResult::NOT_FOUND;
      } else if (stop) { // && cmp == 0
        // match!
        return SeekResult::FOUND;
      }
    }
  }

  // we have reached the end of the block
  return SeekResult::END;
}

SeekResult block_iterator::scan_to_term_nonleaf(const bytes_ref& term) {
  assert(!leaf_);
  assert(!dirty_);

  while (cur_ent_ < ent_count_) {
    ++cur_ent_;
    cur_type_ = shift_unpack_64(vread<uint64_t>(suffix_begin_), suffix_length_)
        ? ET_BLOCK : ET_TERM;
    assert(suffix_begin_ <= suffix_end_);
    suffix_ = suffix_begin_;
    suffix_begin_ += suffix_length_; // skip to the next entry
    assert(suffix_begin_ <= suffix_end_);

    switch (cur_type_) {
      case ET_TERM: ++term_count_; break;
      case ET_BLOCK: cur_block_start_ = cur_start_ - vread<uint64_t>(suffix_begin_); break;
      default: assert(false); break;
    }

    const byte_type* suffix = suffix_;
    const size_t target_len = prefix_ + suffix_length_;
    const byte_type* target = term.c_str() + prefix_;
    const byte_type* target_end = term.c_str() + std::min(term.size(), target_len);

    ptrdiff_t cmp;
    for (bool stop = false;;) {
      if (target < target_end) {
        cmp = *suffix - *target;
        ++target;
        ++suffix;
      } else {
        assert(target == target_end);
        cmp = target_len - term.size();
        stop = true;
      }

      if (cmp < 0) {
        // we before the target, move to next entry
        break;
      } else if (cmp > 0) {
        // we after the target, not found
        return SeekResult::NOT_FOUND;
      } else if (stop) { // && cmp == 0
        // match!
        return SeekResult::FOUND;
      }
    }
  }

  // we have reached the end of the block
  return SeekResult::END;
}

void block_iterator::scan_to_sub_block(byte_type label) {
  assert(sub_count_ != UNDEFINED);

  if (!sub_count_ || !block_meta::floor(meta_)) {
    // no sub-blocks, nothing to do
    return;
  }

  const int16_t target = label; // avoid byte_type vs int16_t comparison

  if (target < next_label_) {
    // we don't need search
    return;
  }

  // FIXME: binary search???
  uint64_t start_delta = 0;
  for (;;) {
    start_delta = vread<uint64_t>(header_begin_);
    cur_meta_ = *header_begin_++;
    if (--sub_count_) {
      next_label_ = *header_begin_++;

      if (target < next_label_) {
        break;
      }
    } else {
      next_label_ = block_t::INVALID_LABEL;
      break;
    }
  }

  if (start_delta) {
    cur_start_ = start_ + start_delta;
    cur_ent_ = 0;
    dirty_ = true;
  }

  assert(header_begin_ <= header_end_);
}

void block_iterator::scan_to_block(uint64_t start) {
  if (leaf_) {
    // must be a non leaf block
    return;
  }

  if (cur_block_start_ == start) {
    // nothing to do
    return;
  }

  const uint64_t target = cur_start_ - start; // delta
  for (; cur_ent_ < ent_count_;) {
    ++cur_ent_;
    const EntryType type = shift_unpack_64(vread<uint64_t>(suffix_begin_), suffix_length_)
        ? ET_BLOCK : ET_TERM;
    assert(suffix_begin_ <= suffix_end_);
    suffix_ = suffix_begin_;
    suffix_begin_ += suffix_length_;
    assert(suffix_begin_ <= suffix_end_);

    switch (type) {
      case ET_TERM:
        ++term_count_;
        break;
      case ET_BLOCK:
        if (vread<uint64_t>(suffix_begin_) == target) {
          assert(suffix_begin_ <= suffix_end_);
          cur_block_start_ = target;
          return;
        }
        assert(suffix_begin_ <= suffix_end_);
        break;
      default:
        assert(false);
        break;
    }
  }
  assert(false);
}

void block_iterator::load_data(const field_meta& meta,
                               attribute_provider& attrs,
                               version10::term_meta& state,
                               irs::postings_reader& pr) {
  assert(ET_TERM == cur_type_);

  if (cur_stats_ent_ >= term_count_) {
    return;
  }

  if (0 == cur_stats_ent_) {
    // clear state at the beginning
    state.clear();
  } else {
    state = state_;
  }

  for (; cur_stats_ent_ < term_count_; ++cur_stats_ent_) {
    stats_begin_ += pr.decode(stats_begin_, meta.features, attrs, state);
    assert(stats_begin_ <= stats_end_);
  }

  state_ = state;
}

void block_iterator::reset() {
  if (sub_count_ != UNDEFINED) {
    sub_count_ = 0;
  }
  next_label_ = block_t::INVALID_LABEL;
  cur_start_ = start_;
  cur_meta_ = meta_;
  if (block_meta::floor(meta_)) {
    assert(sub_count_ != UNDEFINED);
    header_begin_ = header_.c_str() + 1; // +1 to skip meta
    vread<uint64_t>(header_begin_); // skip address
    sub_count_ = vread<uint64_t>(header_begin_);
    next_label_ = *header_begin_++;
  }
  dirty_ = true;

  assert(header_begin_ <= header_end_);
}

///////////////////////////////////////////////////////////////////////////////
/// @class term_iterator_base
/// @brief base class for term_iterator and automaton_term_iterator
///////////////////////////////////////////////////////////////////////////////
class term_iterator_base
    : public frozen_attributes<3, seek_term_iterator> {
 public:
  explicit term_iterator_base(
      const field_meta& field,
      postings_reader& postings,
      const index_input& terms_in,
      irs::encryption::stream* terms_cipher,
      payload* pay = nullptr)
    : attributes{{
        { type<version10::term_meta>::id(), &state_ },
        { type<frequency>::id(), field.features.check<frequency>() ? &freq_ : nullptr },
        { type<payload>::id(), pay }
      }},
      field_(&field),
      postings_(&postings),
      terms_in_source_(&terms_in),
      terms_cipher_(terms_cipher) {
  }

  // read attributes
  void read(block_iterator& it) {
    it.load_data(*field_, *this, state_, *postings_);
  }

  virtual seek_term_iterator::seek_cookie::ptr cookie() const final {
    return ::cookie::make(state_, freq_.value);
  }

  virtual const bytes_ref& value() const noexcept final { return term_; }

  virtual bool seek(
      const bytes_ref& term,
      const irs::seek_term_iterator::seek_cookie& cookie) override {
#ifdef IRESEARCH_DEBUG
    const auto& state = dynamic_cast<const ::cookie&>(cookie);
#else
    const auto& state = static_cast<const ::cookie&>(cookie);
#endif // IRESEARCH_DEBUG

    // copy state
    state_ = state.meta;
    freq_.value = state.term_freq;

    // copy term
    term_.reset();
    term_ += term;

    return true;
  }

  doc_iterator::ptr postings(block_iterator* it, const flags& features) const {
    if (it) {
      it->load_data(*field_, const_cast<term_iterator_base&>(*this), state_, *postings_); // read attributes
    }
    return postings_->iterator(field_->features, *this, features);
  }

  index_input& terms_input() const;

  irs::encryption::stream* terms_cipher() const noexcept {
    return terms_cipher_;
  }

 protected:
  void copy(const byte_type* suffix, size_t prefix_size, size_t suffix_size) {
    const auto size = prefix_size + suffix_size;
    term_.oversize(size);
    term_.reset(size);
    std::memcpy(term_.data() + prefix_size, suffix, suffix_size);
  }

  const field_meta& field() const noexcept {
    return *field_;
  }

  const field_meta* field_;
  postings_reader* postings_;
  const index_input* terms_in_source_;
  irs::encryption::stream* terms_cipher_;
  mutable version10::term_meta state_;
  frequency freq_;
  mutable index_input::ptr terms_in_;
  bytes_builder term_;
  byte_weight weight_; // aggregated fst output
}; // term_iterator_base

index_input& term_iterator_base::terms_input() const {
  if (!terms_in_) {
    terms_in_ = terms_in_source_->reopen(); // reopen thread-safe stream

    if (!terms_in_) {
      // implementation returned wrong pointer
      IR_FRMT_ERROR("Failed to reopen terms input in: %s", __FUNCTION__);

      throw io_error("failed to reopen terms input");
    }
  }

  return *terms_in_;
}

///////////////////////////////////////////////////////////////////////////////
/// @class term_iterator
///////////////////////////////////////////////////////////////////////////////
template<typename FST>
class term_iterator final : public term_iterator_base {
 public:
  using weight_t = typename FST::Weight;
  using stateid_t = typename FST::StateId;

  explicit term_iterator(
      const field_meta& field,
      postings_reader& postings,
      const index_input& terms_in,
      irs::encryption::stream* terms_cipher,
      const FST& fst)
    : term_iterator_base(field, postings, terms_in, terms_cipher, nullptr),
      fst_(&fst),
      matcher_(&fst, fst::MATCH_INPUT) { // pass pointer to avoid copying FST
  }

  virtual bool next() override;
  virtual SeekResult seek_ge(const bytes_ref& term) override;
  virtual bool seek(const bytes_ref& term) override {
    return SeekResult::FOUND == seek_equal(term);
  }
  virtual bool seek(
      const bytes_ref& term,
      const irs::seek_term_iterator::seek_cookie& cookie) override {
    term_iterator_base::seek(term, cookie);

    // reset seek state
    sstate_.clear();

    // mark block as invalid
    cur_block_ = nullptr;
    return true;
  }

  virtual void read() override {
    assert(cur_block_);
    term_iterator_base::read(*cur_block_);
  }

  virtual doc_iterator::ptr postings(const flags& features) const override {
    return term_iterator_base::postings(cur_block_, features);
  }

 private:
  typedef fst::SortedMatcher<FST> sorted_matcher_t;
  typedef fst::explicit_matcher<sorted_matcher_t> matcher_t; // avoid implicit loops

  friend class block_iterator;

  struct arc {
    arc() = default;
    arc(arc&&) = default;
    arc& operator=(arc&&) = delete;
    arc(stateid_t state, const bytes_ref& weight, block_iterator* block)
      : state(state), weight(weight), block(block) {
    }

    stateid_t state;
    bytes_ref weight;
    block_iterator* block{};
  }; // arc

  static_assert(std::is_nothrow_move_constructible_v<arc>);

  typedef std::vector<arc> seek_state_t;
  typedef std::deque<block_iterator> block_stack_t;

  ptrdiff_t seek_cached(
    size_t& prefix, stateid_t& state,
    byte_weight& weight, const bytes_ref& term);

  /// @brief Seek to the closest block which contain a specified term
  /// @param prefix size of the common term/block prefix
  /// @returns true if we're already at a requested term
  bool seek_to_block(const bytes_ref& term, size_t& prefix);

  /// @brief Seeks to the specified term using FST
  /// There may be several sutuations:
  ///   1. There is no term in a block (SeekResult::NOT_FOUND)
  ///   2. There is no term in a block and we have
  ///      reached the end of the block (SeekResult::END)
  ///   3. We have found term in a block (SeekResult::FOUND)
  ///
  /// Note, that search may end up on a BLOCK entry. In all cases
  /// "owner_->term_" will be refreshed with the valid number of
  /// common bytes
  SeekResult seek_equal(const bytes_ref& term);

  block_iterator* pop_block() noexcept {
    block_stack_.pop_back();
    assert(!block_stack_.empty());
    return &block_stack_.back();
  }

  block_iterator* push_block(const bytes_ref& out, size_t prefix) {
    // ensure final weight correctess
    assert(out.size() >= MIN_WEIGHT_SIZE);

    block_stack_.emplace_back(out, prefix);
    return &block_stack_.back();
  }

  block_iterator* push_block(byte_weight&& out, size_t prefix) {
    // ensure final weight correctess
    assert(out.Size() >= MIN_WEIGHT_SIZE);

    block_stack_.emplace_back(std::move(out), prefix);
    return &block_stack_.back();
  }

  block_iterator* push_block(uint64_t start, size_t prefix) {
    block_stack_.emplace_back(start, prefix);
    return &block_stack_.back();
  }

  const FST* fst_;
  matcher_t matcher_;
  seek_state_t sstate_;
  block_stack_t block_stack_;
  block_iterator* cur_block_{};
}; // term_iterator

// -----------------------------------------------------------------------------
// --SECTION--                                      term_iterator implementation
// -----------------------------------------------------------------------------

template<typename FST>
bool term_iterator<FST>::next() {
  // iterator at the beginning or seek to cached state was called
  if (!cur_block_) {
    if (term_.empty()) {
      // iterator at the beginning
      cur_block_ = push_block(fst_->Final(fst_->Start()), 0);
      cur_block_->load(terms_input(), terms_cipher());
    } else {
      // seek to the term with the specified state was called from
      // term_iterator::seek(const bytes_ref&, const attribute&),
      // need create temporary "bytes_ref" here, since "seek" calls
      // term_.reset() internally,
      // note, that since we do not create extra copy of term_
      // make sure that it does not reallocate memory !!!
#ifdef IRESEARCH_DEBUG
      const SeekResult res = seek_equal(bytes_ref(term_));
      assert(SeekResult::FOUND == res);
      UNUSED(res);
#else
      seek_equal(bytes_ref(term_));
#endif
    }
  }

  // pop finished blocks
  while (cur_block_->end()) {
    if (cur_block_->next_sub_block()) {
      cur_block_->load(terms_input(), terms_cipher());
    } else if (&block_stack_.front() == cur_block_) { // root
      term_.reset();
      cur_block_->reset();
      sstate_.resize(0);
      return false;
    } else {
      const uint64_t start = cur_block_->start();
      cur_block_ = pop_block();
      state_ = cur_block_->state();
      if (cur_block_->dirty() || cur_block_->block_start() != start) {
        // here we're currently at non block that was not loaded yet
        assert(cur_block_->prefix() < term_.size());
        cur_block_->scan_to_sub_block(term_[cur_block_->prefix()]); // to sub-block
        cur_block_->load(terms_input(), terms_cipher());
        cur_block_->scan_to_block(start);
      }
    }

    sstate_.resize(std::min(sstate_.size(), cur_block_->prefix()));
  }

  auto copy_suffix = [this](const byte_type* suffix, size_t suffix_size) {
    copy(suffix, cur_block_->prefix(), suffix_size);
  };

  // push new block or next term
  for (cur_block_->next(copy_suffix);
       EntryType::ET_BLOCK == cur_block_->type();
       cur_block_->next(copy_suffix)) {
    cur_block_ = push_block(cur_block_->block_start(), term_.size());
    cur_block_->load(terms_input(), terms_cipher());
  }

  return true;
}

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

template<typename FST>
ptrdiff_t term_iterator<FST>::seek_cached(
    size_t& prefix, stateid_t& state,
    byte_weight& weight, const bytes_ref& target) {
  assert(!block_stack_.empty());
  const byte_type* pterm = term_.c_str();
  const byte_type* ptarget = target.c_str();

  // reset current block to root
  auto* cur_block = &block_stack_.front();

  // determine common prefix between target term and current
  {
    auto begin = sstate_.begin();
    auto end = begin + std::min(target.size(), sstate_.size());

    for (; begin != end && *pterm == *ptarget; ++begin, ++pterm, ++ptarget) {
      auto& state_weight = begin->weight;
      weight.PushBack(state_weight.begin(), state_weight.end());
      state = begin->state;
      cur_block = begin->block;
    }

    prefix = size_t(pterm - term_.c_str());
  }

  // inspect suffix and determine our current position
  // with respect to target term (before, after, equal)
  ptrdiff_t cmp = std::char_traits<byte_type>::compare(
    pterm, ptarget,
    std::min(target.size(), term_.size()) - prefix);

  if (!cmp) {
    cmp = term_.size() - target.size();
  }

  if (cmp) {
    cur_block_ = cur_block; // update cur_block if not at the same block

    // truncate block_stack_ to match path
    while (!block_stack_.empty() && &(block_stack_.back()) != cur_block_) {
      block_stack_.pop_back();
    }
  }

  // cmp < 0 : target term is after the current term
  // cmp == 0 : target term is current term
  // cmp > 0 : target term is before current term
  return cmp;
}

template<typename FST>
bool term_iterator<FST>::seek_to_block(const bytes_ref& term, size_t& prefix) {
  assert(fst_->GetImpl());

  auto& fst = *fst_->GetImpl();

  prefix = 0; // number of current symbol to process
  stateid_t state = fst.Start(); // start state
  weight_.Clear(); // clear aggregated fst output

  if (cur_block_) {
    const auto cmp = seek_cached(prefix, state, weight_, term);
    if (cmp > 0) {
      // target term is before the current term
      cur_block_->reset();
    } else if (0 == cmp) {
      // we're already at current term
      return true;
    }
  } else {
    cur_block_ = push_block(fst.Final(state), prefix);
  }

  term_.oversize(term.size());
  term_.reset(prefix); // reset to common seek prefix
  sstate_.resize(prefix); // remove invalid cached arcs

  while (fst_buffer::fst_byte_builder::final != state && prefix < term.size()) {
    matcher_.SetState(state);

    if (!matcher_.Find(term[prefix])) {
      break;
    }

    const auto& arc = matcher_.Value();

    term_ += byte_type(arc.ilabel); // aggregate arc label
    weight_.PushBack(arc.weight.begin(), arc.weight.end()); // aggregate arc weight
    ++prefix;

    const auto& weight = fst.FinalRef(arc.nextstate);

    if (!weight.Empty()) {
      cur_block_ = push_block(fst::Times(weight_, weight), prefix);
    } else if (fst_buffer::fst_byte_builder::final == arc.nextstate) {
      // ensure final state has no weight assigned
      // the only case when it's wrong is degenerated FST composed of only
      // 'fst_byte_builder::final' state.
      // in that case we'll never get there due to the loop condition above.
      assert(fst.FinalRef(fst_buffer::fst_byte_builder::final).Empty());

      cur_block_ = push_block(std::move(weight_), prefix);
    }

    // cache found arcs, we can reuse it in further seeks
    // avoiding relatively expensive FST lookups
    sstate_.emplace_back(arc.nextstate, arc.weight, cur_block_);

    // proceed to the next state
    state = arc.nextstate;
  }

  assert(cur_block_);
  prefix = cur_block_->prefix();
  sstate_.resize(prefix);

  if (prefix < term.size()) {
    cur_block_->scan_to_sub_block(term[prefix]);
  }

  return false;
}

template<typename FST>
SeekResult term_iterator<FST>::seek_equal(const bytes_ref& term) {
  size_t prefix;
  if (seek_to_block(term, prefix)) {
    return SeekResult::FOUND;
  }

  assert(cur_block_);

  if (!block_meta::terms(cur_block_->meta())) {
    // current block has no terms
    term_.reset(prefix);
    return SeekResult::NOT_FOUND;
  }

  auto append_suffix = [this](const byte_type* suffix, size_t suffix_size) {
    const auto prefix = cur_block_->prefix();
    const auto size = prefix + suffix_size;
    term_.oversize(size);
    term_.reset(size);
    std::memcpy(term_.data() + prefix, suffix, suffix_size);
  };

  cur_block_->load(terms_input(), terms_cipher());

  assert(starts_with(term, term_));
  return cur_block_->scan_to_term(term, append_suffix);
}

template<typename FST>
SeekResult term_iterator<FST>::seek_ge(const bytes_ref& term) {
  size_t prefix;
  if (seek_to_block(term, prefix)) {
    return SeekResult::FOUND;
  }
  UNUSED(prefix);

  assert(cur_block_);

  auto append_suffix = [this](const byte_type* suffix, size_t suffix_size) {
    const auto prefix = cur_block_->prefix();
    const auto size = prefix + suffix_size;
    term_.oversize(size);
    term_.reset(size);
    std::memcpy(term_.data() + prefix, suffix, suffix_size);
  };

  cur_block_->load(terms_input(), terms_cipher());

  assert(starts_with(term, term_));
  switch (cur_block_->scan_to_term(term, append_suffix)) {
    case SeekResult::FOUND:
      assert(ET_TERM == cur_block_->type());
      return SeekResult::FOUND;
    case SeekResult::NOT_FOUND:
      switch (cur_block_->type()) {
        case ET_TERM:
          // we're already at greater term
          return SeekResult::NOT_FOUND;
        case ET_BLOCK:
          // we're at the greater block, load it and call next
          cur_block_ = push_block(cur_block_->block_start(), term_.size());
          cur_block_->load(terms_input(), terms_cipher());
          break;
        default:
          assert(false);
          return SeekResult::END;
      }
    [[fallthrough]];
    case SeekResult::END:
      return next()
        ? SeekResult::NOT_FOUND // have moved to the next entry
        : SeekResult::END; // have no more terms
  }

  assert(false);
  return SeekResult::END;
}

///////////////////////////////////////////////////////////////////////////////
/// @class automaton_term_iterator
///////////////////////////////////////////////////////////////////////////////
template<typename FST>
class automaton_term_iterator final : public term_iterator_base {
 public:
  explicit automaton_term_iterator(const field_meta& field,
                                   postings_reader& postings,
                                   const index_input& terms_in,
                                   irs::encryption::stream* terms_cipher,
                                   const FST& fst,
                                   automaton_table_matcher& matcher)
    : term_iterator_base(field, postings, terms_in, terms_cipher, &payload_),
      fst_(&fst),
      acceptor_(&matcher.GetFst()),
      matcher_(&matcher) {
    // init payload value
    payload_.value = {&payload_value_, sizeof(payload_value_)};
  }

  virtual bool next() override;

  virtual SeekResult seek_ge(const bytes_ref& term) override {
    if (!irs::seek(*this, term)) {
      return SeekResult::END;
    }

    return term_ == term ? SeekResult::FOUND : SeekResult::NOT_FOUND;
  }

  virtual bool seek(const bytes_ref& term) override {
    return SeekResult::FOUND == seek_ge(term);
  }

  virtual bool seek(
      const bytes_ref& term,
      const irs::seek_term_iterator::seek_cookie& cookie) override {
    term_iterator_base::seek(term, cookie);

    // mark block as invalid
    cur_block_ = nullptr;
    return true;
  }

  virtual void read() override {
    assert(cur_block_);
    term_iterator_base::read(*cur_block_);
  }

  virtual doc_iterator::ptr postings(const flags& features) const override {
    return term_iterator_base::postings(cur_block_, features);
  }

 private:
  class arc_matcher {
   public:
    arc_matcher(const automaton::Arc* arcs, size_t narcs) noexcept
      : begin_(arcs), end_(arcs + narcs),
        rho_(rho_arc(begin_, end_)) {
    }

    const automaton::Arc* seek(automaton::Arc::Label label) noexcept {
      // FIXME: binary search???
      for (;begin_ != end_; ++begin_) {
        if (label <= begin_->ilabel) {
          return label == begin_->ilabel ? begin_ : rho_;
        }
      }

      return nullptr;
    }

    const automaton::Arc* value() const noexcept {
      return begin_;
    }

    bool done() const noexcept {
      return begin_ == end_;
    }

   private:
    static const automaton::Arc* rho_arc(const automaton::Arc* begin,
                                         const automaton::Arc* end) noexcept {
      if (begin != end) {
        auto* back = end-1;
        if (back->ilabel == fst::fsa::kRho) {
          return back;
        }
      }

      return nullptr;
    }

    const automaton::Arc* begin_;  // current arc
    const automaton::Arc* end_;    // end of arcs range
    const automaton::Arc* rho_{};  // rho arc if present
  }; // begin_matcher

  class block_iterator : public ::block_iterator {
   public:
    block_iterator(const bytes_ref& out, size_t prefix,
                   automaton::StateId state,
                   const automaton::Arc* arcs, size_t narcs) noexcept
      : ::block_iterator(out, prefix),
        arcs_(arcs, narcs),
        state_(state) {
    }

    block_iterator(byte_weight&& out, size_t prefix,
                   automaton::StateId state,
                   const automaton::Arc* arcs, size_t narcs) noexcept
      : ::block_iterator(std::move(out), prefix),
        arcs_(arcs, narcs),
        state_(state) {
    }

    block_iterator(uint64_t start, size_t prefix,
                automaton::StateId state,
                const automaton::Arc* arcs, size_t narcs) noexcept
      : ::block_iterator(start, prefix),
        arcs_(arcs, narcs),
        state_(state) {
    }

   public:
    arc_matcher& arcs() noexcept { return arcs_; }
    automaton::StateId acceptor_state() const noexcept { return state_; }

   private:
    arc_matcher arcs_;
    automaton::StateId state_;  // state to which current block belongs
  }; // block_iterator

  typedef std::deque<block_iterator> block_stack_t;

  block_iterator* pop_block() noexcept {
    block_stack_.pop_back();
    assert(!block_stack_.empty());
    return &block_stack_.back();
  }

  block_iterator* push_block(const bytes_ref& out, size_t prefix, automaton::StateId state) {
    // ensure final weight correctess
    assert(out.size() >= MIN_WEIGHT_SIZE);

    fst::ArcIteratorData<automaton::Arc> data;
    acceptor_->InitArcIterator(state, &data);

    block_stack_.emplace_back(out, prefix, state, data.arcs, data.narcs);
    return &block_stack_.back();
  }

  block_iterator* push_block(byte_weight&& out, size_t prefix, automaton::StateId state) {
    // ensure final weight correctess
    assert(out.Size() >= MIN_WEIGHT_SIZE);

    fst::ArcIteratorData<automaton::Arc> data;
    acceptor_->InitArcIterator(state, &data);

    block_stack_.emplace_back(std::move(out), prefix, state, data.arcs, data.narcs);
    return &block_stack_.back();
  }

  block_iterator* push_block(uint64_t start, size_t prefix, automaton::StateId state) {
    fst::ArcIteratorData<automaton::Arc> data;
    acceptor_->InitArcIterator(state, &data);

    block_stack_.emplace_back(start, prefix, state, data.arcs, data.narcs);
    return &block_stack_.back();
  }

  const FST* fst_;
  const automaton* acceptor_;
  automaton_table_matcher* matcher_;
  block_stack_t block_stack_;
  block_iterator* cur_block_{};
  automaton::Weight::PayloadType payload_value_;
  payload payload_; // payload of the matched automaton state
}; // automaton_term_iterator

template<typename FST>
bool automaton_term_iterator<FST>::next() {
  // iterator at the beginning or seek to cached state was called
  if (!cur_block_) {
    if (term_.empty()) {
      // iterator at the beginning
      cur_block_ = push_block(fst_->Final(fst_->Start()), 0, acceptor_->Start());
      cur_block_->load(terms_input(), terms_cipher());
    } else {
      // seek to the term with the specified state was called from
      // term_iterator::seek(const bytes_ref&, const attribute&),
      // need create temporary "bytes_ref" here, since "seek" calls
      // term_.reset() internally,
      // note, that since we do not create extra copy of term_
      // make sure that it does not reallocate memory !!!
      const SeekResult res = seek_ge(bytes_ref(term_));
      assert(SeekResult::FOUND == res);
      UNUSED(res);
    }
  }

  enum {
    MATCH, // current entry is valid term
    SKIP,  // skip current entry
    POP    // pop current block
  } match;

  automaton::StateId state;

  auto read_suffix = [this, &match, &state](const byte_type* suffix, size_t suffix_size) {
    match = SKIP;
    state = cur_block_->acceptor_state();

    const auto* begin = suffix;
    const auto* end = begin + suffix_size;

    if (begin != end) {
      const auto* arc = cur_block_->arcs().seek(*begin);

      if (!arc) {
        if (cur_block_->arcs().done()) {
          match = POP; // pop current block
        }

        return;
      }

      assert(*begin == arc->ilabel || fst::fsa::kRho == arc->ilabel);
      state = arc->nextstate;

#ifdef IRESEARCH_DEBUG
      matcher_->SetState(cur_block_->acceptor_state());
      assert(matcher_->Find(*begin));
      assert(matcher_->Value().nextstate == state);
#endif

      ++begin; // already match first suffix label

      for (matcher_->SetState(state); begin < end; ++begin) {
        if (!matcher_->Find(*begin)) {
          // suffix doesn't match
          return;
        }

        state = matcher_->Value().nextstate;
        matcher_->SetState(state);
      }
    }

    assert(begin == end);

    switch (cur_block_->type()) {
      case ET_TERM: {
        const auto weight = acceptor_->Final(state);
        if (weight) {
          payload_value_ = weight.Payload();
          copy(suffix, cur_block_->prefix(), suffix_size);
          match = MATCH;
        }
      } break;
      case ET_BLOCK: {
        copy(suffix, cur_block_->prefix(), suffix_size);
        cur_block_ = push_block(cur_block_->block_start(), term_.size(), state);
        cur_block_->load(terms_input(), terms_cipher());
      } break;
      default: {
        assert(false);
      } break;
    }
  };

  for (;;) {
    // pop finished blocks
    while (cur_block_->end()) {
      if (cur_block_->sub_count()) {
        if (block_t::INVALID_LABEL != cur_block_->next_label()) {
          auto& arcs = cur_block_->arcs();
          const auto* arc = arcs.seek(cur_block_->next_label());

          if (arcs.done()) {
            if (&block_stack_.front() == cur_block_) {
              // need to pop root block, we're done
              term_.reset();
              cur_block_->reset();
              return false;
            }

            cur_block_ = pop_block();
            continue;
          }

          if (arc && arc->ilabel == fst::fsa::kRho) {
            cur_block_->next_sub_block();
          } else {
            assert(arcs.value()->ilabel <= integer_traits<byte_type>::const_max);
            cur_block_->scan_to_sub_block(byte_type(arcs.value()->ilabel));
          }
        } else {
          cur_block_->next_sub_block();
        }
        cur_block_->load(terms_input(), terms_cipher());
      } else if (&block_stack_.front() == cur_block_) { // root
        term_.reset();
        cur_block_->reset();
        return false;
      } else {
        const uint64_t start = cur_block_->start();
        cur_block_ = pop_block();
        state_ = cur_block_->state();
        if (cur_block_->dirty() || cur_block_->block_start() != start) {
          // here we're currently at non block that was not loaded yet
          assert(cur_block_->prefix() < term_.size());
          cur_block_->scan_to_sub_block(term_[cur_block_->prefix()]); // to sub-block
          cur_block_->load(terms_input(), terms_cipher());
          cur_block_->scan_to_block(start);
        }
      }
    }

    do {
      cur_block_->next(read_suffix);

      if (MATCH == match) {
        return true;
      } else if (POP == match) {
        if (&block_stack_.front() == cur_block_) {
          // need to pop root block, we're done
          term_.reset();
          cur_block_->reset();
          return false;
        }

        cur_block_ = pop_block();
        break; // continue with popped block
      }
    } while (!cur_block_->end());
  }
}

#if defined(_MSC_VER)
  #pragma warning( default : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif


///////////////////////////////////////////////////////////////////////////////
/// @class field_reader
///////////////////////////////////////////////////////////////////////////////
class field_reader final : public irs::field_reader {
 public:
  explicit field_reader(irs::postings_reader::ptr&& pr);

  virtual void prepare(
    const directory& dir,
    const segment_meta& meta,
    const document_mask& mask) override;

  virtual const irs::term_reader* field(const string_ref& field) const override;
  virtual irs::field_iterator::ptr iterator() const override;
  virtual size_t size() const noexcept override {
    return name_to_field_.size();
  }

 private:
  template<typename FST>
  class term_reader final : public term_reader_base {
   public:
    explicit term_reader(field_reader& owner) noexcept
      : owner_(&owner) {
    }
    term_reader(term_reader&& rhs) = default;
    term_reader& operator=(term_reader&& rhs) = delete;

    virtual void prepare(index_input& in, const feature_map_t& features) override {
      term_reader_base::prepare(in, features);

      // read FST
      input_buf isb(&in);
      std::istream input(&isb); // wrap stream to be OpenFST compliant
      fst_.reset(FST::Read(input, fst_read_options()));

      if (!fst_) {
        throw irs::index_error(string_utils::to_string(
          "failed to read term index for field '%s'",
          meta().name.c_str()));
      }
    }

    virtual seek_term_iterator::ptr iterator() const override {
      return memory::make_managed<term_iterator<FST>>(
        meta(), *owner_->pr_, *owner_->terms_in_,
        owner_->terms_in_cipher_.get(), *fst_);
    }

    virtual seek_term_iterator::ptr iterator(automaton_table_matcher& matcher) const override {
      return memory::make_managed<automaton_term_iterator<FST>>(
        meta(), *owner_->pr_, *owner_->terms_in_,
        owner_->terms_in_cipher_.get(), *fst_, matcher);
    }

   private:
    field_reader* owner_;
    std::unique_ptr<FST> fst_;
  }; // term_reader

  using vector_fst_reader = term_reader<vector_byte_fst>;
  using immutable_fst_reader = term_reader<immutable_byte_fst>;

  using vector_fst_readers = std::vector<term_reader<vector_byte_fst>>;
  using immutable_fst_readers = std::vector<term_reader<immutable_byte_fst>>;

  std::variant<immutable_fst_readers, vector_fst_readers> fields_;
  std::unordered_map<hashed_string_ref, irs::term_reader*> name_to_field_;
  irs::postings_reader::ptr pr_;
  encryption::stream::ptr terms_in_cipher_;
  index_input::ptr terms_in_;
}; // field_reader

// -----------------------------------------------------------------------------
// --SECTION--                                        term_reader implementation
// -----------------------------------------------------------------------------

field_reader::field_reader(irs::postings_reader::ptr&& pr)
  : pr_(std::move(pr)) {
  assert(pr_);
}

void field_reader::prepare(
    const directory& dir,
    const segment_meta& meta,
    const document_mask& /*mask*/) {
  std::string filename;

  //-----------------------------------------------------------------
  // read term index
  //-----------------------------------------------------------------

  feature_map_t feature_map;
  flags features;
  reader_state state;

  state.dir = &dir;
  state.meta = &meta;

  // check index header
  index_input::ptr index_in;

  int64_t checksum = 0;

  const auto term_index_version = burst_trie::Version(prepare_input(
    filename, index_in,
    irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE, state,
    field_writer::TERMS_INDEX_EXT,
    field_writer::FORMAT_TERMS_INDEX,
    static_cast<int32_t>(burst_trie::Version::MIN),
    static_cast<int32_t>(burst_trie::Version::MAX),
    &checksum));

  constexpr const size_t FOOTER_LEN =
      sizeof(uint64_t) // fields count
    + format_utils::FOOTER_LEN;

  // read total number of indexed fields
  size_t fields_count{ 0 };
  {
    const uint64_t ptr = index_in->file_pointer();

    index_in->seek(index_in->length() - FOOTER_LEN);

    fields_count = index_in->read_long();

    // check index checksum
    format_utils::check_footer(*index_in, checksum);

    index_in->seek(ptr);
  }

  auto* enc = get_encryption(dir.attributes());
  encryption::stream::ptr index_in_cipher;

  if (term_index_version > burst_trie::Version::MIN) {
    if (irs::decrypt(filename, *index_in, enc, index_in_cipher)) {
      assert(index_in_cipher && index_in_cipher->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        buffered_index_input::DEFAULT_BUFFER_SIZE,
        index_in_cipher->block_size());

      index_in = memory::make_unique<encrypted_input>(
        std::move(index_in),
        *index_in_cipher,
        blocks_in_buffer,
        FOOTER_LEN);
    }
  }

  read_segment_features(*index_in, feature_map, features);

  // read terms for each indexed field
  if (term_index_version <= burst_trie::Version::ENCRYPTION_MIN) {
    fields_ = vector_fst_readers{};
  }

  std::visit([&](auto& fields) {
    fields.reserve(fields_count);
    name_to_field_.reserve(fields_count);

    for (string_ref previous_field_name = string_ref::EMPTY; fields_count; --fields_count) {
      auto& field = fields.emplace_back(*this);
      field.prepare(*index_in, feature_map);

      const auto& name = field.meta().name;

      // ensure that fields are sorted properly
      if (previous_field_name > name) {
        throw index_error(string_utils::to_string(
          "invalid field order in segment '%s'",
          meta.name.c_str()));
      }

      const auto res = name_to_field_.emplace(
        make_hashed_ref(string_ref(name)),
        &field);

      if (!res.second) {
        throw irs::index_error(string_utils::to_string(
          "duplicated field: '%s' found in segment: '%s'",
          name.c_str(),
          meta.name.c_str()));
      }

      previous_field_name = name;
    }
  }, fields_);

  //-----------------------------------------------------------------
  // prepare terms input
  //-----------------------------------------------------------------

  // check term header
  const auto term_dict_version = burst_trie::Version(prepare_input(
    filename, terms_in_, irs::IOAdvice::RANDOM, state,
    field_writer::TERMS_EXT,
    field_writer::FORMAT_TERMS,
    static_cast<int32_t>(burst_trie::Version::MIN),
    static_cast<int32_t>(burst_trie::Version::MAX)));

  if (term_index_version != term_dict_version) {
    throw index_error(string_utils::to_string(
      "term index version '%d' mismatches term dictionary version '%d' in segment '%s'",
      term_index_version,
      meta.name.c_str(),
      term_dict_version));
  }

  if (term_dict_version > burst_trie::Version::MIN) {
    if (irs::decrypt(filename, *terms_in_, enc, terms_in_cipher_)) {
      assert(terms_in_cipher_ && terms_in_cipher_->block_size());
    }
  }

  // prepare postings reader
  pr_->prepare(*terms_in_, state, features);

  // Since terms dictionary are too large
  // it is too expensive to verify checksum of
  // the entire file. Here we perform cheap
  // error detection which could recognize
  // some forms of corruption.
  format_utils::read_checksum(*terms_in_);
}

const irs::term_reader* field_reader::field(const string_ref& field) const {
  auto it = name_to_field_.find(make_hashed_ref(field));
  return it == name_to_field_.end() ? nullptr : it->second;
}

irs::field_iterator::ptr field_reader::iterator() const {
  struct less {
    bool operator()(
        const irs::term_reader& lhs,
        const string_ref& rhs) const noexcept {
      return lhs.meta().name < rhs;
    }
  }; // less

  return std::visit(
    [](const auto& fields) -> irs::field_iterator::ptr {
      using reader_type = typename std::remove_reference_t<decltype(fields)>::value_type;

      using iterator_t = iterator_adaptor<
        string_ref, reader_type,
        irs::field_iterator, less>;

      return memory::make_managed<iterator_t>(
          fields.data(), fields.data() + fields.size());
    }, fields_);
}

//////////////////////////////////////////////////////////////////////////////////
///// @class term_reader_visitor
///// @brief implements generalized visitation logic for term dictionary
//////////////////////////////////////////////////////////////////////////////////
template<typename FST>
class term_reader_visitor {
 public:
  explicit term_reader_visitor(
      const FST& field,
      index_input& terms_in,
      encryption::stream* terms_in_cipher)
    : fst_(&field),
      terms_in_(terms_in.reopen()),
      terms_in_cipher_(terms_in_cipher) {
  }

  template<typename Visitor>
  void operator()(Visitor& visitor) {
    block_iterator* cur_block = push_block(fst_->Final(fst_->Start()), 0);
    cur_block->load(*terms_in_, terms_in_cipher_);
    visitor.push_block(term_, *cur_block);

    auto copy_suffix = [&cur_block, this](const byte_type* suffix, size_t suffix_size) {
      copy(suffix, cur_block->prefix(), suffix_size);
    };

    while (true) {
      while (cur_block->end()) {
        if (cur_block->next_sub_block()) {
          cur_block->load(*terms_in_, terms_in_cipher_);
          visitor.sub_block(*cur_block);
        } else if (&block_stack_.front() == cur_block) { // root
          cur_block->reset();
          return;
        } else {
          visitor.pop_block(*cur_block);
          cur_block = pop_block();
        }
      }

      while (!cur_block->end()) {
        cur_block->next(copy_suffix);
        switch (cur_block->type()) {
          case ET_TERM:
            visitor.push_term(term_);
            break;
          case ET_BLOCK:
            cur_block = push_block(cur_block->block_start(), term_.size());
            cur_block->load(*terms_in_, terms_in_cipher_);
            visitor.push_block(term_, *cur_block);
            break;
          default:
            assert(false);
            break;
        }
      }
    }
  }

 private:
  void copy(const byte_type* suffix, size_t prefix_size, size_t suffix_size) {
    const auto size = prefix_size + suffix_size;
    term_.oversize(size);
    term_.reset(size);
    std::memcpy(term_.data() + prefix_size, suffix, suffix_size);
  }

  block_iterator* pop_block() noexcept {
    block_stack_.pop_back();
    assert(!block_stack_.empty());
    return &block_stack_.back();
  }

  block_iterator* push_block(byte_weight&& out, size_t prefix) {
    // ensure final weight correctess
    assert(out.Size() >= MIN_WEIGHT_SIZE);

    block_stack_.emplace_back(std::move(out), prefix);
    return &block_stack_.back();
  }

  block_iterator* push_block(uint64_t start, size_t prefix) {
    block_stack_.emplace_back(start, prefix);
    return &block_stack_.back();
  }

  const FST* fst_;
  std::deque<block_iterator> block_stack_;
  bytes_builder term_;
  index_input::ptr terms_in_;
  encryption::stream* terms_in_cipher_;
}; // term_reader_visitor

//////////////////////////////////////////////////////////////////////////////////
///// @brief "Dumper" visitor for term_reader_visitor. Suitable for debugging needs.
//////////////////////////////////////////////////////////////////////////////////
class dumper : util::noncopyable {
 public:
  explicit dumper(std::ostream& out)
    : out_(out) {
  }

  void push_term(const bytes_ref& term) {
    indent();
    out_ << "TERM|" << suffix(term) << "\n";
  }

  void push_block(const bytes_ref& term, const block_iterator& block) {
    indent();
    out_ << "BLOCK|" << block.size() << "|" << suffix(term) << "\n";
    indent_ += 2;
    prefix_ = block.prefix();
  }

  void sub_block(const block_iterator& /*block*/){
    indent();
    out_ << "|\n";
    indent();
    out_ << "V\n";
  };

  void pop_block(const block_iterator& block) {
    indent_ -= 2;
    prefix_ -= block.prefix();
  }

 private:
  void indent() {
    for (size_t i = 0; i < indent_; ++i) {
      out_ << " ";
    }
  }

  string_ref suffix(const bytes_ref& term) {
    return ref_cast<char>(bytes_ref(term.c_str() + prefix_, term.size() - prefix_));
  }

  std::ostream& out_;
  size_t indent_ = 0;
  size_t prefix_ = 0;
}; // dumper

}

namespace iresearch {
namespace burst_trie {

irs::field_writer::ptr make_writer(
    Version version,
    irs::postings_writer::ptr&& writer,
    bool volatile_state) {
  return memory::make_unique<::field_writer>(std::move(writer), volatile_state, version);
}

irs::field_reader::ptr make_reader(irs::postings_reader::ptr&& reader) {
  return memory::make_unique<::field_reader>(std::move(reader));
}

} // burst_trie
} // iresearch
