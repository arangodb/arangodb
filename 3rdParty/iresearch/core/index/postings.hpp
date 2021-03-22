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

#include "shared.hpp"
#include "utils/block_pool.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"
#include "utils/hash_set_utils.hpp"

namespace iresearch {

inline bool memcmp_less(
    const byte_type* lhs, size_t lhs_size,
    const byte_type* rhs, size_t rhs_size) noexcept {
  assert(lhs && rhs);

  const size_t size = std::min(lhs_size, rhs_size);
  const auto res = ::memcmp(lhs, rhs, size);

  if (0 == res) {
    return lhs_size < rhs_size;
  }

  return res < 0;
}

inline bool memcmp_less(
    const bytes_ref& lhs,
    const bytes_ref& rhs) noexcept {
  return memcmp_less(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size());
}

using byte_block_pool = block_pool<byte_type, 32768>;

struct posting {
  bytes_ref term;
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
  using writer_t = byte_block_pool::inserter;

  explicit postings(writer_t& writer)
    : map_{0, value_ref_hash{}, term_id_eq{postings_}},
      writer_(writer) {
  }

  void clear() noexcept {
    map_.clear();
    postings_.clear();
  }

  /// @brief fill a provided vector with terms and corresponding postings in sorted order
  void get_sorted_postings(std::vector<const posting*>& postings) const;

  /// @note on error returns std::ptr(nullptr, false)
  /// @note returned poitern remains valid until the next call
  std::pair<posting*, bool> emplace(const bytes_ref& term);

  bool empty() const noexcept { return map_.empty(); }
  size_t size() const noexcept { return map_.size(); }

 private:
  class term_id_eq : public value_ref_eq<size_t> {
   public:
    explicit term_id_eq(const std::vector<posting>& data) noexcept
      : data_(&data) {
    }

    using self_t::operator();

    bool operator()(const ref_t& lhs, const hashed_bytes_ref& rhs) const noexcept {
      assert(lhs.second < data_->size());
      return (*data_)[lhs.second].term == rhs;
    }

    bool operator()(const hashed_bytes_ref& lhs, ref_t& rhs) const noexcept {
      return this->operator()(rhs, lhs);
    }

   private:
    const std::vector<posting>* data_;
  };

  using map_t = flat_hash_set<term_id_eq>;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::vector<posting> postings_;
  map_t map_;
  writer_t& writer_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

}

#endif
