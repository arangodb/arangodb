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

#ifndef IRESEARCH_POSTINGS_H
#define IRESEARCH_POSTINGS_H

#include <unordered_map>

#include "shared.hpp"
#include "utils/block_pool.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"

NS_ROOT

typedef block_pool<byte_type, 32768> byte_block_pool;

struct posting {
  doc_id_t doc_code;
  doc_id_t doc;
  // ...........................................................................
  // store pointers to data in the following way:
  // [0] - pointer to freq stream end
  // [1] - pointer to prox stream end
  // [2] - pointer to freq stream begin
  // [3] - pointer to prox stream begin
  // ...........................................................................
  size_t int_start;
  uint32_t freq;
  uint32_t pos;
  uint32_t offs = 0;
};

class IRESEARCH_API postings: util::noncopyable {
 public:
  typedef std::unordered_map<hashed_bytes_ref, posting> map_t;
  typedef std::pair<map_t::iterator, bool> emplace_result;
  typedef byte_block_pool::inserter writer_t;

  postings(writer_t& writer);

  inline map_t::const_iterator begin() const { return map_.begin(); }

  inline void clear() { map_.clear(); }

  // on error returns std::ptr(end(), false)
  emplace_result emplace(const bytes_ref& term);

  inline bool empty() const { return map_.empty(); }

  inline map_t::const_iterator end() const { return map_.end(); }

  inline size_t size() const { return map_.size(); }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  map_t map_;
  writer_t& writer_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif
