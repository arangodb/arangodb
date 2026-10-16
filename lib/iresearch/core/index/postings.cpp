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

#include "postings.hpp"

#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"

namespace irs {

// -----------------------------------------------------------------------------
// --SECTION--                                           postings implementation
// -----------------------------------------------------------------------------

void postings::get_sorted_postings(
  std::vector<const posting*>& postings) const {
  IRS_ASSERT(terms_.size() == postings_.size());

  postings.resize(postings_.size());

  for (auto* p = postings.data(); const auto& posting : postings_) {
    *p++ = &posting;
  }

  std::sort(postings.begin(), postings.end(),
            [](const auto lhs, const auto rhs) {
              return memcmp_less(lhs->term, rhs->term);
            });
}

posting* postings::emplace(bytes_view term) {
  REGISTER_TIMER_DETAILED();
  auto& parent = writer_.parent();

  // maximum number to bytes needed for storage of term length and data
  const auto term_size = term.size();  // + vencode_size(term.size());

  if (writer_t::container::block_type::SIZE < term_size) {
    // TODO: maybe move big terms it to a separate storage
    // reject terms that do not fit in a block
    return nullptr;
  }

  const auto slice_end = writer_.pool_offset() + term_size;
  const auto next_block_start =
    writer_.pool_offset() < parent.value_count()
      ? writer_.position().block_offset() +
          writer_t::container::block_type::SIZE
      : writer_t::container::block_type::SIZE * parent.block_count();

  // do not span slice over 2 blocks, start slice at the start of the next block
  if (slice_end > next_block_start) {
    writer_.seek(next_block_start);
  }

  IRS_ASSERT(size() < doc_limits::eof());  // not larger then the static flag
  IRS_ASSERT(terms_.size() == postings_.size());

  const hashed_bytes_view hashed_term{term};

  bool is_new = false;
  const auto it = terms_.lazy_emplace(
    hashed_term, [&, size = terms_.size()](const auto& ctor) {
      ctor(size, hashed_term.hash());
      is_new = true;
    });
  if (IRS_LIKELY(!is_new)) {
    return &postings_[it->ref];
  }
  // for new terms also write out their value
  try {
    auto* start = writer_.position().buffer();
    writer_.write(term.data(), term_size);
    IRS_ASSERT(start == (writer_.position() - term_size).buffer());
    return &postings_.emplace_back(start, term_size);
  } catch (...) {
    // we leave some garbage in block pool
    terms_.erase(it);
    throw;
  }
}

}  // namespace irs
