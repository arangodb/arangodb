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
    size_t size
) {
  return memory::make_unique<byte_type[]>(size);
}

/*static*/ memory_allocator& memory_allocator::global() noexcept {
  return GLOBAL_ALLOC;
}

/*static*/ memory_allocator::ptr memory_allocator::make(size_t pool_size) {
  return memory::make_unique<memory_allocator>(pool_size);
}

memory_allocator::memory_allocator(size_t pool_size)
  : allocator_(pool_size) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      fd_pool_size
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(fd_pool_size)

const size_t FD_POOL_DEFAULT_SIZE = 8;

fd_pool_size::fd_pool_size() noexcept
  : size(FD_POOL_DEFAULT_SIZE) { // arbitrary size
}

void fd_pool_size::clear() noexcept {
  size = FD_POOL_DEFAULT_SIZE;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   index_file_refs
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(index_file_refs)

index_file_refs::ref_t index_file_refs::add(const std::string& key) {
  return refs_.add(std::string(key));
}

index_file_refs::ref_t index_file_refs::add(std::string&& key) {
  return refs_.add(std::move(key));
}

void index_file_refs::clear() {
  static auto cleaner = [](const std::string&, size_t)->bool { return true; };

  refs_.visit(cleaner, true);

  if (!refs_.empty()) {
    throw illegal_state(); // cannot clear ref_counter due to live refs
  }
}

}
