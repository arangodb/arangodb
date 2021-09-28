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

#include "directory.hpp"
#include "directory_attributes.hpp"
#include "directory_cleaner.hpp"

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                 directory_cleaner
// -----------------------------------------------------------------------------

/*static*/ size_t directory_cleaner::clean(
    directory& dir, const removal_acceptor_t& acceptor) {
  auto& refs = dir.attributes().refs().refs();

  size_t remove_count = 0;
  index_file_refs::ref_t tmp_ref;
  auto visitor = [&dir, &refs, &acceptor, &remove_count, &tmp_ref](
      const std::string& filename, size_t count )->bool {
    // for retained files add a temporary reference to avoid removal
    if (!acceptor(filename)) {
      tmp_ref = refs.add(std::string(filename));
    } else if (!count && dir.remove(filename)) {
      ++remove_count;
    }

    return true;
  };

  refs.visit(visitor, true);

  return remove_count;
}

}
