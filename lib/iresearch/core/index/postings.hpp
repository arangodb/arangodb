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

#pragma once

#include "shared.hpp"
#include "utils/block_pool.hpp"
#include "utils/hash_set_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"
#include "utils/type_limits.hpp"

namespace irs {

inline bool memcmp_less(const byte_type* lhs, size_t lhs_size,
                        const byte_type* rhs, size_t rhs_size) noexcept {
  IRS_ASSERT(lhs && rhs);

  const size_t size = std::min(lhs_size, rhs_size);
  const auto res = ::memcmp(lhs, rhs, size);

  if (0 == res) {
    return lhs_size < rhs_size;
  }

  return res < 0;
}

inline bool memcmp_less(bytes_view lhs, bytes_view rhs) noexcept {
  return memcmp_less(lhs.data(), lhs.size(), rhs.data(), rhs.size());
}

using byte_block_pool =
  block_pool<byte_type, 32768, ManagedTypedAllocator<byte_type>>;

struct posting {
  explicit posting(const byte_type* data, size_t size) noexcept
    : term{data, size} {}

  bytes_view term;
  uint64_t doc_code;
  // ...........................................................................
  // store pointers to data in the following way:
  // [0] - pointer to freq stream end
  // [1] - pointer to prox stream end
  // [2] - pointer to freq stream begin
  // [3] - pointer to prox stream begin
  // ...........................................................................
  size_t int_start;
  doc_id_t doc{doc_limits::invalid()};
  uint32_t freq;
  uint32_t pos;
  uint32_t offs{0};
  doc_id_t size{1};  // length of postings
};

class postings : util::noncopyable {
 public:
  using writer_t = byte_block_pool::inserter;

  // cppcheck-suppress constParameter
  explicit postings(writer_t& writer)
    : terms_{0, ValueRefHash{}, TermEq{postings_}}, writer_(writer) {}

  void clear() noexcept {
    terms_.clear();
    postings_.clear();
  }

  /// @brief fill a provided vector with terms and corresponding postings in
  /// sorted order
  void get_sorted_postings(std::vector<const posting*>& postings) const;

  /// @note on error returns nullptr
  /// @note returned poitern remains valid until the next call
  posting* emplace(bytes_view term);

  bool empty() const noexcept { return terms_.empty(); }
  size_t size() const noexcept { return terms_.size(); }

 private:
  struct TermEq : ValueRefEq<size_t> {
    using is_transparent = void;
    using Self::operator();

    explicit TermEq(const std::vector<posting>& data) noexcept : data_{&data} {}

    bool operator()(const Ref& lhs,
                    const hashed_bytes_view& rhs) const noexcept {
      IRS_ASSERT(lhs.ref < data_->size());
      return (*data_)[lhs.ref].term == rhs;
    }

    bool operator()(const hashed_bytes_view& lhs,
                    const Ref& rhs) const noexcept {
      return this->operator()(rhs, lhs);
    }

   private:
    const std::vector<posting>* data_;
  };

  // TODO(MBkkt) Maybe just flat_hash_set<unique_ptr<posting>>?
  std::vector<posting> postings_;
  flat_hash_set<TermEq> terms_;
  writer_t& writer_;
};

}  // namespace irs
