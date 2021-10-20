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

#include "error/error.hpp"
#include "directory_attributes.hpp"

namespace {

irs::memory_allocator GLOBAL_ALLOC(128);

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                  memory_allocator
// -----------------------------------------------------------------------------

/*static*/ memory_allocator::buffer::ptr memory_allocator::buffer::make(
    size_t size) {
  return memory::make_unique<byte_type[]>(size);
}

/*static*/ memory_allocator& memory_allocator::global() noexcept {
  return GLOBAL_ALLOC;
}

/*static*/ memory_allocator::ptr memory_allocator::make(size_t pool_size) {
  if (pool_size) {
    return memory::make_managed<memory_allocator>(pool_size);
  }

  return memory::to_managed<memory_allocator, false>(&GLOBAL_ALLOC);
}

memory_allocator::memory_allocator(size_t pool_size)
  : allocator_(pool_size) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   index_file_refs
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(index_file_refs) // cppcheck-suppress unknownMacro

index_file_refs::ref_t index_file_refs::add(const std::string& key) {
  return refs_.add(std::string(key));
}

index_file_refs::ref_t index_file_refs::add(std::string&& key) {
  return refs_.add(std::move(key));
}

void index_file_refs::clear() {
  refs_.visit([](const std::string&, size_t){ return true; }, true);

  if (!refs_.empty()) {
    throw illegal_state(); // cannot clear ref_counter due to live refs
  }
}

directory_attributes::directory_attributes(
    size_t memory_pool_size,
    std::unique_ptr<irs::encryption> enc)
  : alloc_{memory_allocator::make(memory_pool_size)},
    enc_{std::move(enc)},
    refs_{memory::make_unique<index_file_refs>()} {
}


}
