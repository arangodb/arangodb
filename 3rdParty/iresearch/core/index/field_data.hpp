//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_FIELD_DATA_H
#define IRESEARCH_FIELD_DATA_H

#include "field_meta.hpp"
#include "postings.hpp"
#include "formats/formats.hpp"

#include "index/iterators.hpp"

#include "utils/block_pool.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"

#include <vector>
#include <tuple>
#include <unordered_map>

NS_ROOT

struct field_writer;
class token_stream;
class analyzer;
struct offset;
struct payload;
class format;
struct directory;

typedef block_pool<size_t, 8192> int_block_pool;

NS_BEGIN( detail ) 
class term_iterator;
class doc_iterator;
NS_END

/* TODO: since actual string data stores in global ids
 * it looks like we should not to create additional copy
 * of field name. It is implicity interned by global ids */
class IRESEARCH_API field_data : util::noncopyable {
 public:
  field_data(
    const string_ref& name,
    byte_block_pool::inserter* byte_writer,
    int_block_pool::inserter* int_writer
  );

  ~field_data();

  doc_id_t doc() const { return last_doc_; }

  // returns number of terms in a field within a document
  size_t size() const { return len_; }

  float_t boost() const { return boost_; }

  const field_meta& meta() const { return meta_; }

  data_output& norms(columnstore_writer& writer);

  // returns false if field contains indexed data
  bool empty() const {
    return !type_limits<type_t::doc_id_t>::valid(last_doc_);
  }

  bool invert(token_stream& tokens, const flags& features, float_t boost, doc_id_t id);

 private:
  friend class detail::term_iterator;
  friend class detail::doc_iterator;
  friend class fields_data;

  void init(doc_id_t doc_id);

  void new_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  void add_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  void write_prox(posting& p, int_block_pool::iterator& where,
                  uint32_t prox, const payload* pay);

  void write_offset(posting& p, int_block_pool::iterator& where,
                    const offset* offs);

  columnstore_writer::values_writer_f norms_;
  field_meta meta_;
  postings terms_;
  byte_block_pool::inserter* byte_writer_;
  int_block_pool::inserter* int_writer_;
  doc_id_t last_doc_{ type_limits<type_t::doc_id_t>::invalid() };
  uint32_t pos_;
  uint32_t last_pos_;
  uint32_t len_; // total number of terms
  uint32_t num_overlap_; // number of overlapped terms
  uint32_t offs_;
  uint32_t last_start_offs_;
  uint32_t max_term_freq_; // maximum number of terms in a field across all indexed documents 
  uint32_t unq_term_cnt_;
  float_t boost_;
};

struct flush_state;

class IRESEARCH_API fields_data: util::noncopyable {
 public:
  typedef std::unordered_map<hashed_string_ref, field_data> fields_map;

  fields_data();

  field_data& get(const hashed_string_ref& name);
  size_t size() const { return fields_.size(); }
  fields_data& operator+=(const flags& features) {
    features_ |= features;
    return *this;
  }
  const flags& features() { return features_; }
  void flush(field_writer& fw, flush_state& state);
  void reset();

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  fields_map fields_;
  byte_block_pool byte_pool_;
  byte_block_pool::inserter byte_writer_;
  int_block_pool int_pool_;
  int_block_pool::inserter int_writer_;
  flags features_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif
