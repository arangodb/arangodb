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

#include "utils/map_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "postings.hpp"

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                           postings implementation
// -----------------------------------------------------------------------------

postings::postings(writer_t& writer):
  writer_(writer) {
}

postings::emplace_result postings::emplace(const bytes_ref& term) {
  REGISTER_TIMER_DETAILED();
  auto& parent = writer_.parent();
 
  // maximum number to bytes needed for storage of term length and data
  const auto max_term_len = term.size(); // + vencode_size(term.size());

  if (writer_t::container::block_type::SIZE < max_term_len) {
    // TODO: maybe move big terms it to a separate storage
    // reject terms that do not fit in a block
    return std::make_pair(map_.end(), false);
  }

  const auto slice_end = writer_.pool_offset() + max_term_len;
  const auto next_block_start = writer_.pool_offset() < parent.value_count()
                        ? writer_.position().block_offset() + writer_t::container::block_type::SIZE
                        : writer_t::container::block_type::SIZE * parent.block_count();

  // do not span slice over 2 blocks, start slice at the start of the next block
  if (slice_end > next_block_start) {
    writer_.seek(next_block_start);
  }

  assert(size() < doc_limits::eof()); // not larger then the static flag

  auto generator = [&term, this](const hashed_bytes_ref& key, const posting&) {
    // for new terms also write out their value
    writer_.write(term.c_str(), term.size());

    // reuse hash but point ref at data in pool
    return hashed_bytes_ref(
      key.hash(),
      (writer_.position() - term.size()).buffer(), term.size()
    );
  };

  // replace original reference to 'term' provided by the caller
  // with a reference to the cached copy in 'writer_'
  return map_utils::try_emplace_update_key(
    map_,                  // container
    generator,             // key generator
    make_hashed_ref(term)  // key
  );
}

}
