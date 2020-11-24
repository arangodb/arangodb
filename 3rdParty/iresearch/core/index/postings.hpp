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

#ifndef IRESEARCH_POSTINGS_H
#define IRESEARCH_POSTINGS_H

#include <unordered_map>

#include "shared.hpp"
#include "utils/block_pool.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"

namespace iresearch {

typedef block_pool<byte_type, 32768> byte_block_pool;

struct posting {
  uint64_t doc_code;
  // ...........................................................................
  // store pointers to data in the following way:
  // [0] - pointer to freq stream end
  // [1] - pointer to prox stream end
  // [2] - pointer to freq stream begin
  // [3] - pointer to prox stream begin
  // ...........................................................................
  size_t int_start;
  doc_id_t doc;
  uint32_t freq;
  uint32_t pos;
  uint32_t offs{ 0 };
  doc_id_t size{ 1 }; // length of postings
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

}

#endif
