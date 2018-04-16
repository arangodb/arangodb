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

#ifndef IRESEARCH_FORMATS_10_H
#define IRESEARCH_FORMATS_10_H

#include "formats.hpp"
#include "skip_list.hpp"

#include "formats_10_attributes.hpp"

#include "analysis/token_attributes.hpp"

#include "store/data_output.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"

#include "index/field_meta.hpp"
#include "utils/compression.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitset.hpp"
#include "utils/memory.hpp"
#include "utils/memory_pool.hpp"
#include "utils/noncopyable.hpp"
#include "utils/type_limits.hpp"

#include <list>

#if defined(_MSC_VER)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#include <boost/crc.hpp>

#if defined(_MSC_VER)
  #pragma warning(default: 4244)
  #pragma warning(default: 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#if defined(_MSC_VER)
  #pragma warning(disable : 4351)
#endif

NS_ROOT
NS_BEGIN( version10 )

// compiled features supported by current format
class features {
 public:
  enum Mask : uint32_t {
    POS = 3, POS_OFFS = 7, POS_PAY = 11, POS_OFFS_PAY = 15
  };

  features() = default;

  explicit features(const flags& in) NOEXCEPT;

  features operator&(const flags& in) const NOEXCEPT;
  features& operator&=(const flags& in) NOEXCEPT;

  bool freq() const { return check_bit<0>(mask_); }
  bool position() const { return check_bit<1>(mask_); }
  bool offset() const { return check_bit<2>(mask_); }
  bool payload() const { return check_bit<3>(mask_); }
  operator Mask() const { return static_cast<Mask>(mask_); }

 private:
  byte_type mask_{};
}; // features

/* -------------------------------------------------------------------
 * index_meta_writer 
 * ------------------------------------------------------------------*/

struct index_meta_writer final: public iresearch::index_meta_writer {
  static const string_ref FORMAT_NAME;
  static const string_ref FORMAT_PREFIX;
  static const string_ref FORMAT_PREFIX_TMP;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  virtual std::string filename(const index_meta& meta) const override;
  using iresearch::index_meta_writer::prepare;
  virtual bool prepare(directory& dir, index_meta& meta) override;
  virtual void commit() override;
  virtual void rollback() NOEXCEPT override;
 private:
  directory* dir_ = nullptr;
  index_meta* meta_ = nullptr;
};

/* -------------------------------------------------------------------
 * index_meta_reader
 * ------------------------------------------------------------------*/

struct index_meta_reader final: public iresearch::index_meta_reader {
  virtual bool last_segments_file(
    const directory& dir, std::string& name
  ) const override;

  virtual void read( 
    const directory& dir,
    index_meta& meta,
    const string_ref& filename = string_ref::NIL // null == use meta
  ) override;
};

/* -------------------------------------------------------------------
 * segment_meta_writer 
 * ------------------------------------------------------------------*/

struct segment_meta_writer final : public iresearch::segment_meta_writer{
  static const string_ref FORMAT_EXT;
  static const string_ref FORMAT_NAME;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  enum flags_t {
    HAS_COLUMN_STORE = 1,
  };

  virtual std::string filename(const segment_meta& meta) const override;
  virtual void write(directory& dir, const segment_meta& meta) override;
};

/* -------------------------------------------------------------------
 * segment_meta_reader
 * ------------------------------------------------------------------*/

struct segment_meta_reader final : public iresearch::segment_meta_reader{
  virtual void read(
    const directory& dir,  
    segment_meta& meta,
    const string_ref& filename = string_ref::NIL // null == use meta
  ) override;
};

/* -------------------------------------------------------------------
 * document_mask_writer
 * ------------------------------------------------------------------*/

class document_mask_reader;
class document_mask_writer final: public iresearch::document_mask_writer {
 public:
  static const string_ref FORMAT_EXT;
  static const string_ref FORMAT_NAME;

  static const int32_t FORMAT_MIN = 0;
  static const int32_t FORMAT_MAX = FORMAT_MIN;

  virtual ~document_mask_writer();
  virtual std::string filename(const segment_meta& meta) const override;
  virtual void prepare(directory& dir, const segment_meta& meta) override;
  virtual void begin(uint32_t count) override;
  virtual void write(const doc_id_t& mask) override;
  virtual void end() override;

private:
  friend document_mask_reader;
  index_output::ptr out_;
};

/* -------------------------------------------------------------------
 * document_mask_reader
 * ------------------------------------------------------------------*/

class document_mask_reader final: public iresearch::document_mask_reader {
public:
  virtual ~document_mask_reader();

  virtual bool prepare(
    const directory& dir,
    const segment_meta& meta,
    bool* seen = nullptr
  ) override;

  virtual uint32_t begin() override;
  virtual void read(iresearch::doc_id_t& mask) override;
  virtual void end() override;

private:
  uint64_t checksum_{};
  index_input::ptr in_;
};

/* -------------------------------------------------------------------
* postings_writer
* 
* Assume that doc_count = 28, skip_n = skip_0 = 12
*  
*  |       block#0       | |      block#1        | |vInts|
*  d d d d d d d d d d d d d d d d d d d d d d d d d d d d (posting list)
*                          ^                       ^       (level 0 skip point)
*
* ------------------------------------------------------------------*/
class IRESEARCH_PLUGIN postings_writer final: public iresearch::postings_writer {
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
  static const uint32_t BLOCK_SIZE = 128;
  static const uint32_t SKIP_N = 8;

  postings_writer(bool volatile_attributes);

  /*------------------------------------------
  * const_attributes_provider 
  * ------------------------------------------*/

  virtual const irs::attribute_view& attributes() const NOEXCEPT override final {
    return attrs_;
  }

  /*------------------------------------------
  * postings_writer
  * ------------------------------------------*/

  virtual void prepare(index_output& out, const iresearch::flush_state& state) override;
  virtual void begin_field(const iresearch::flags& meta) override;
  virtual irs::postings_writer::state write(doc_iterator& docs) override;
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

    uint64_t skip_ptr[MAX_SKIP_LEVELS]{};   /* skip data */
    index_output::ptr out;                  /* output stream*/
    uint64_t start{};                       /* start position of block */
    uint64_t end{};                         /* end position of block */
  }; // stream

  struct doc_stream : stream {
    void doc(doc_id_t delta) { deltas[size] = delta; }
    void flush(uint64_t* buf, bool freq);
    bool full() const { return BLOCK_SIZE == size; }
    void next(doc_id_t id) { last = id, ++size; }
    void freq(uint64_t frq) { freqs[size] = frq; }

    void reset() {
      stream::reset();
      last = type_limits<type_t::doc_id_t>::invalid();
      block_last = 0;
      size = 0;
    }

    doc_id_t deltas[BLOCK_SIZE]{}; // document deltas
    doc_id_t skip_doc[MAX_SKIP_LEVELS]{};
    std::unique_ptr<uint64_t[]> freqs; /* document frequencies */
    doc_id_t last{ type_limits<type_t::doc_id_t>::invalid() }; // last buffered document id
    doc_id_t block_last{}; // last document id in a block
    uint32_t size{};            /* number of buffered elements */
  }; // doc_stream

  struct pos_stream : stream {
    DECLARE_PTR(pos_stream);

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

    uint32_t buf[BLOCK_SIZE]{};        /* buffer to store position deltas */
    uint32_t last{};                   /* last buffered position */
    uint32_t block_last{};             /* last position in a block */
    uint32_t size{};                   /* number of buffered elements */
  }; // pos_stream

  struct pay_stream : stream {
    DECLARE_PTR(pay_stream);

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

    bstring pay_buf_; // buffer for payload
    uint32_t pay_sizes[BLOCK_SIZE]{};             /* buffer to store payloads sizes */
    uint32_t offs_start_buf[BLOCK_SIZE]{};        /* buffer to store start offsets */
    uint32_t offs_len_buf[BLOCK_SIZE]{};          /* buffer to store offset lengths */
    size_t block_last{};                          /* last payload buffer length in a block */
    uint32_t last{};                              /* last start offset */
  }; // pay_stream 

  void write_skip(size_t level, index_output& out);
  void begin_term();
  void begin_doc(doc_id_t id, const frequency* freq);
  void add_position( uint32_t pos, const offset* offs, const payload* pay );
  void end_doc();
  void end_term(version10::term_meta& state, const uint64_t* tfreq);

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  memory::memory_pool<> meta_pool_;
  memory::memory_pool_allocator<version10::term_meta, decltype(meta_pool_)> alloc_{ meta_pool_ };
  skip_writer skip_;
  irs::attribute_view attrs_;
  uint64_t buf[BLOCK_SIZE]; // buffer for encoding (worst case)
  version10::term_meta last_state;    /* last final term state*/
  doc_stream doc;           /* document stream */
  pos_stream::ptr pos_;      /* proximity stream */
  pay_stream::ptr pay_;      /* payloads and offsets stream */
  uint64_t docs_count{};      /* count of processed documents */
  version10::documents docs_; /* bit set of all processed documents */
  features features_; /* features supported by current field */
  bool volatile_attributes_; // attribute value memory locations may change after next()
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

/* -------------------------------------------------------------------
* postings_reader
* ------------------------------------------------------------------*/

class IRESEARCH_PLUGIN postings_reader final: public iresearch::postings_reader {
 public:
  virtual bool prepare(
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

  virtual doc_iterator::ptr iterator(
    const flags& field,
    const attribute_view& attrs,
    const flags& features
  ) override;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  index_input::ptr doc_in_;
  index_input::ptr pos_in_;
  index_input::ptr pay_in_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

/* -------------------------------------------------------------------
 * format
 * ------------------------------------------------------------------*/

class IRESEARCH_PLUGIN format final : public iresearch::format {
 public:
  DECLARE_FORMAT_TYPE();
  DECLARE_FACTORY_DEFAULT();
  format();

  virtual index_meta_writer::ptr get_index_meta_writer() const override;
  virtual index_meta_reader::ptr get_index_meta_reader() const override;

  virtual segment_meta_writer::ptr get_segment_meta_writer() const override;
  virtual segment_meta_reader::ptr get_segment_meta_reader() const override;

  virtual document_mask_writer::ptr get_document_mask_writer() const override;
  virtual document_mask_reader::ptr get_document_mask_reader() const override;

  virtual field_writer::ptr get_field_writer(bool volatile_state) const override;
  virtual field_reader::ptr get_field_reader() const override;
  
  virtual column_meta_writer::ptr get_column_meta_writer() const override;
  virtual column_meta_reader::ptr get_column_meta_reader() const override;

  virtual columnstore_writer::ptr get_columnstore_writer() const override;
  virtual columnstore_reader::ptr get_columnstore_reader() const override;
};

NS_END
NS_END

#endif