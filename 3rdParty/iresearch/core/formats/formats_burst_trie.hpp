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

#ifndef IRESEARCH_FORMAT_BURST_TRIE_H
#define IRESEARCH_FORMAT_BURST_TRIE_H

#include <list>

#include "formats.hpp"
#include "formats_10_attributes.hpp"
#include "index/field_meta.hpp"

#include "store/data_output.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/buffers.hpp"
#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-compare"
  #pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include "utils/fst_utils.hpp"

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "utils/noncopyable.hpp"

NS_ROOT
NS_BEGIN(burst_trie)

class field_reader;

NS_BEGIN(detail)

// -----------------------------------------------------------------------------
// --SECTION--                                              Forward declarations
// -----------------------------------------------------------------------------

class fst_buffer;
class term_iterator;

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

typedef std::vector<const attribute::type_id*> feature_map_t;

template<typename Char>
class volatile_ref : util::noncopyable {
 public:
  typedef irs::basic_string_ref<Char> ref_t;
  typedef std::basic_string<Char> str_t;

  volatile_ref() = default;

  volatile_ref(volatile_ref&& rhs) NOEXCEPT
   : str_(std::move(rhs.str_)),
     ref_(str_.empty() ? rhs.ref_ : ref_t(str_)) {
    rhs.ref_ = ref_;
  }

  volatile_ref& operator=(volatile_ref&& rhs) NOEXCEPT {
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
    if (Volatile) {
      str_.assign(str.c_str(), str.size());
      ref_ = str_;
    } else {
      ref_ = str;
    }
  }

  void assign(const ref_t& str, Char label) {
    str_.resize(str.size() + 1);
    std::memcpy(&str_[0], str.c_str(), str.size() * sizeof(Char));
    str_[str.size()] = label;
    ref_ = str_;
  }

  operator const ref_t&() const NOEXCEPT {
    return ref_;
  }

 private:
  str_t str_;
  ref_t ref_{ ref_t::NIL };
}; // volatile_ref

typedef volatile_ref<byte_type> volatile_byte_ref;

///////////////////////////////////////////////////////////////////////////////
/// @class block_t
/// @brief block of terms
///////////////////////////////////////////////////////////////////////////////
struct block_t : private util::noncopyable {
  struct prefixed_output final : irs::byte_weight_output {
//    explicit prefixed_output(const irs::bytes_ref& prefix) NOEXCEPT
//     : prefix(prefix.c_str(), prefix.size()) {
//    }
    explicit prefixed_output(volatile_byte_ref&& prefix) NOEXCEPT
     : prefix(std::move(prefix)) {
    }

    volatile_byte_ref prefix;
//    irs::bytes_ref prefix;
//    irs::bstring prefix; // TODO: find a way to avoid copy
  }; // prefixed_output

  static const int16_t INVALID_LABEL = -1;

  block_t(uint64_t block_start, byte_type meta, int16_t label) NOEXCEPT
    : start(block_start),
      label(label),
      meta(meta) {
  }

  block_t(block_t&& rhs) NOEXCEPT
    : index(std::move(rhs.index)),
      start(rhs.start),
      label(rhs.label),
      meta(rhs.meta) {
  }

  block_t& operator=(block_t&& rhs) NOEXCEPT {
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
  entry(entry&& rhs) NOEXCEPT;
  entry& operator=(entry&& rhs) NOEXCEPT;
  ~entry() NOEXCEPT;

  const irs::postings_writer::state& term() const NOEXCEPT {
    return *mem_.as<irs::postings_writer::state>();
  }

  irs::postings_writer::state& term() NOEXCEPT {
    return *mem_.as<irs::postings_writer::state>();
  }

  const block_t& block() const NOEXCEPT { return *mem_.as<block_t>(); }
  block_t& block() NOEXCEPT { return *mem_.as<block_t>(); }

  const volatile_byte_ref& data() const NOEXCEPT { return data_; }
  volatile_byte_ref& data() NOEXCEPT { return data_; }

  EntryType type() const NOEXCEPT { return type_; }

 private:
  void destroy() NOEXCEPT;
  void move_union(entry&& rhs) NOEXCEPT;

  volatile_byte_ref data_; // block prefix or term
  memory::aligned_type<irs::postings_writer::state, block_t> mem_; // storage
  EntryType type_; // entry type
}; // entry

///////////////////////////////////////////////////////////////////////////////
/// @class term_reader
///////////////////////////////////////////////////////////////////////////////
class term_reader : public irs::term_reader,
                    private util::noncopyable {
 public:
  term_reader() = default;
  term_reader(term_reader&& rhs) NOEXCEPT;
  virtual ~term_reader();

  void prepare(
    std::istream& in,
    const feature_map_t& features,
    field_reader& owner
  );

  virtual seek_term_iterator::ptr iterator() const override;
  virtual const field_meta& meta() const override { return field_; }
  virtual size_t size() const override { return terms_count_; }
  virtual uint64_t docs_count() const override { return doc_count_; }
  virtual const bytes_ref& min() const override { return min_term_ref_; }
  virtual const bytes_ref& max() const override { return max_term_ref_; }
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_; 
  }

 private:
  typedef fst::VectorFst<byte_arc> fst_t;
  friend class term_iterator;

  irs::attribute_view attrs_;
  bstring min_term_;
  bstring max_term_;
  bytes_ref min_term_ref_;
  bytes_ref max_term_ref_;
  uint64_t terms_count_;
  uint64_t doc_count_;
  uint64_t doc_freq_;
  uint64_t term_freq_;
  frequency freq_; // total term freq
  field_meta field_;
  fst_t* fst_; // TODO: use compact fst here!!!
  field_reader* owner_;
}; // term_reader

NS_END // detail

///////////////////////////////////////////////////////////////////////////////
/// @class field_writer
///////////////////////////////////////////////////////////////////////////////
class field_writer final : public irs::field_writer {
 public:
  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;
  static const uint32_t DEFAULT_MIN_BLOCK_SIZE = 25;
  static const uint32_t DEFAULT_MAX_BLOCK_SIZE = 48;

  static const string_ref FORMAT_TERMS;
  static const string_ref TERMS_EXT;
  static const string_ref FORMAT_TERMS_INDEX;
  static const string_ref TERMS_INDEX_EXT;

  field_writer(irs::postings_writer::ptr&& pw,
               bool volatile_state,
               uint32_t min_block_size = DEFAULT_MIN_BLOCK_SIZE,
               uint32_t max_block_size = DEFAULT_MAX_BLOCK_SIZE);

  virtual void prepare( const irs::flush_state& state ) override;
  virtual void end() override;
  virtual void write( 
    const std::string& name,
    irs::field_id norm,
    const irs::flags& features,
    irs::term_iterator& terms
  ) override;

 private:
  static const size_t DEFAULT_SIZE = 8;

  static void merge_blocks(std::list< detail::entry >& blocks);

  void write_segment_features(data_output& out, const flags& features);
  void write_field_features(data_output& out, const flags& features) const;

  void begin_field(const irs::flags& field);
  void end_field(
    const std::string& name,
    irs::field_id norm,
    const irs::flags& features,
    uint64_t total_doc_freq, 
    uint64_t total_term_freq, 
    size_t doc_count
  );

  void write_term_entry( const detail::entry& e, size_t prefix, bool leaf );
  void write_block_entry( const detail::entry& e, size_t prefix, uint64_t block_start );
  /* prefix - prefix length ( in last_term )
  * begin - index of the first entry in the block
  * end - index of the last entry in the block
  * meta - block metadata
  * label - block lead label ( if present ) */
  void write_block( std::list< detail::entry >& blocks,
                    size_t prefix, size_t begin,
                    size_t end, byte_type meta,
                    int16_t label );
  /* prefix - prefix length ( in last_term
  * count - number of entries to write into block */
  void write_blocks( size_t prefix, size_t count );
  void push( const irs::bytes_ref& term );

  std::unordered_map<const attribute::type_id*, size_t> feature_map_;
  irs::memory_output suffix; /* term suffix column */
  irs::memory_output stats; /* term stats column */
  irs::index_output::ptr terms_out; /* output stream for terms */
  irs::index_output::ptr index_out; /* output stream for indexes*/
  irs::postings_writer::ptr pw; /* postings writer */
  std::vector< detail::entry > stack;
  std::unique_ptr<detail::fst_buffer> fst_buf_; // pimpl buffer used for building FST for fields
  detail::volatile_byte_ref last_term; // last pushed term
  std::vector<size_t> prefixes;
  std::pair<bool, detail::volatile_byte_ref> min_term; // current min term in a block
  detail::volatile_byte_ref max_term; // current max term in a block
  uint64_t term_count;    /* count of terms */
  size_t fields_count{};
  uint32_t min_block_size;
  uint32_t max_block_size;
  const bool volatile_state_;
}; // field_writer

///////////////////////////////////////////////////////////////////////////////
/// @class field_reader
///////////////////////////////////////////////////////////////////////////////
class field_reader final : public irs::field_reader {
 public:
  explicit field_reader(irs::postings_reader::ptr&& pr);

  virtual void prepare(
    const directory& dir,
    const segment_meta& meta,
    const document_mask& mask
  ) override;

  virtual const irs::term_reader* field(const string_ref& field) const override;
  virtual irs::field_iterator::ptr iterator() const override;
  virtual size_t size() const override;

 private:
  friend class detail::term_iterator;

  std::vector<detail::term_reader> fields_;
  std::unordered_map<hashed_string_ref, term_reader*> name_to_field_;
  std::vector<const detail::term_reader*> fields_mask_;
  irs::postings_reader::ptr pr_;
  irs::index_input::ptr terms_in_;
}; // field_reader

NS_END // burst_trie
NS_END // ROOT

#endif
