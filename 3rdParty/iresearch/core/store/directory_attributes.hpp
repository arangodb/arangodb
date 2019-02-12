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

#ifndef IRESEARCH_DIRECTORY_ATTRIBUTES_H
#define IRESEARCH_DIRECTORY_ATTRIBUTES_H

#include "shared.hpp"
#include "utils/attributes.hpp"
#include "utils/ref_counter.hpp"
#include "utils/container_utils.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class memory_allocator
/// @brief a reusable thread-safe allocator for memory files
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API memory_allocator : public stored_attribute {
 private:
  struct IRESEARCH_API buffer {
    typedef std::unique_ptr<byte_type[]> ptr;
    DECLARE_FACTORY(size_t size);
  }; // buffer

 public:
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY(size_t pool_size);

  typedef container_utils::memory::bucket_allocator<
    buffer,
    16 // as in memory_file
  > allocator_type;

  static memory_allocator& global() NOEXCEPT;

  explicit memory_allocator(size_t pool_size);

  operator allocator_type&() const NOEXCEPT {
    return const_cast<allocator_type&>(allocator_);
  }

 private:
  allocator_type allocator_;
}; // memory_allocator

//////////////////////////////////////////////////////////////////////////////
/// @class fd_pool_size
/// @brief the size of file descriptor pools
///        where applicable, e.g. fs_directory
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API fd_pool_size: public stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  fd_pool_size() NOEXCEPT;
  void clear() NOEXCEPT;

  size_t size;
}; // fd_pool_size

//////////////////////////////////////////////////////////////////////////////
/// @class index_file_refs
/// @brief represents a ref_counter for index related files
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API index_file_refs : public stored_attribute {
 public:
  typedef attribute_store::ref<index_file_refs>::type attribute_t;
  typedef ref_counter<std::string> counter_t;
  typedef counter_t::ref_t ref_t;
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();
  index_file_refs() = default;
  ref_t add(const std::string& key);
  ref_t add(std::string&& key);
  void clear();
  bool remove(const std::string& key) { return refs_.remove(key); }
  counter_t& refs() NOEXCEPT {
    return refs_;
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  counter_t refs_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // index_file_refs

NS_END

#endif
