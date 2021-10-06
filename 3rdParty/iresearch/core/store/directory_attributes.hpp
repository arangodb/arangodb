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
#include "utils/ref_counter.hpp"
#include "utils/container_utils.hpp"

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @class memory_allocator
/// @brief a reusable thread-safe allocator for memory files
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API memory_allocator final {
 private:
  struct IRESEARCH_API buffer {
    using ptr = std::unique_ptr<byte_type[]>;
    static ptr make(size_t size);
  }; // buffer

 public:
  using ptr = memory::managed_ptr<memory_allocator>;

  static ptr make(size_t pool_size);

  using allocator_type = container_utils::memory::bucket_allocator<
    buffer, 16>; // as in memory_file

  static memory_allocator& global() noexcept;

  explicit memory_allocator(size_t pool_size);

  operator allocator_type&() const noexcept {
    return const_cast<allocator_type&>(allocator_);
  }

 private:
  allocator_type allocator_;
}; // memory_allocator

//////////////////////////////////////////////////////////////////////////////
/// @struct encryption
/// @brief directory encryption provider
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API encryption {
  DECLARE_UNIQUE_PTR(encryption);

  // FIXME check if it's possible to rename to iresearch::encryption?
  static constexpr string_ref type_name() noexcept {
    return "encryption";
  }

  virtual ~encryption() = default;

  ////////////////////////////////////////////////////////////////////////////
  /// @struct stream
  ////////////////////////////////////////////////////////////////////////////
  struct stream {
    using ptr = std::unique_ptr<stream>;

    virtual ~stream() = default;

    /// @returns size of the block supported by stream
    virtual size_t block_size() const = 0;

    /// @brief decrypt specified data at a provided offset
    virtual bool decrypt(uint64_t offset, byte_type* data, size_t size) = 0;

    /// @brief encrypt specified data at a provided offset
    virtual bool encrypt(uint64_t offset, byte_type* data, size_t size) = 0;
  };

  /// @returns the length of the header that is added to every file
  ///          and used for storing encryption options
  virtual size_t header_length() = 0;

  /// @brief an allocated block of header memory for a new file
  virtual bool create_header(
    const std::string& filename,
    byte_type* header) = 0;

  /// @returns a cipher stream for a file given file name
  virtual stream::ptr create_stream(
    const std::string& filename,
    byte_type* header) = 0;
};

//////////////////////////////////////////////////////////////////////////////
/// @class index_file_refs
/// @brief represents a ref_counter for index related files
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API index_file_refs final {
 public:
  typedef std::unique_ptr<index_file_refs> ptr;
  typedef ref_counter<std::string> counter_t;
  typedef counter_t::ref_t ref_t;

  static ptr make();

  index_file_refs() = default;
  ref_t add(const std::string& key);
  ref_t add(std::string&& key);
  void clear();
  bool remove(const std::string& key) {
    return refs_.remove(key);
  }
  counter_t& refs() noexcept {
    return refs_;
  }

 private:
  counter_t refs_;
}; // index_file_refs

//////////////////////////////////////////////////////////////////////////////
/// @class directory_attributes
/// @brief represents common directory attributes
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API directory_attributes {
 public:
  // 0 == pool_size -> use global allocator, noexcept
  explicit directory_attributes(
    size_t memory_pool_size = 0,
    std::unique_ptr<irs::encryption> enc = nullptr);
  virtual ~directory_attributes() = default;

  directory_attributes(directory_attributes&&) = default;
  directory_attributes& operator=(directory_attributes&&) = default;

  memory_allocator& allocator() const noexcept { return *alloc_; }
  irs::encryption* encryption() const noexcept { return enc_.get(); }
  index_file_refs& refs() const noexcept { return *refs_; }

 private:
  memory_allocator::ptr alloc_;
  irs::encryption::ptr enc_;
  index_file_refs::ptr refs_;
}; // directory_attributes

}

#endif
