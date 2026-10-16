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
#include "utils/container_utils.hpp"
#include "utils/ref_counter.hpp"

namespace irs {

// Directory encryption provider
struct encryption {
  // FIXME check if it's possible to rename to irs::encryption?
  static constexpr std::string_view type_name() noexcept {
    return "encryption";
  }

  virtual ~encryption() = default;

  struct stream {
    using ptr = std::unique_ptr<stream>;

    virtual ~stream() = default;

    // Returns size of the block supported by stream
    virtual size_t block_size() const = 0;

    // Decrypt specified data at a provided offset
    virtual bool decrypt(uint64_t offset, byte_type* data, size_t size) = 0;

    // Encrypt specified data at a provided offset
    virtual bool encrypt(uint64_t offset, byte_type* data, size_t size) = 0;
  };

  // Returns the length of the header that is added to every file
  // and used for storing encryption options
  virtual size_t header_length() = 0;

  // Creates cipher header in an allocated block for a new file
  virtual bool create_header(std::string_view filename, byte_type* header) = 0;

  // Returns a cipher stream for a file given file name
  virtual stream::ptr create_stream(std::string_view filename,
                                    byte_type* header) = 0;
};

// Represents a reference counter for index related files
class index_file_refs final {
 public:
  using counter_t = ref_counter<std::string, absl::Hash<std::string_view>,
                                std::equal_to<std::string_view>>;
  using ref_t = counter_t::ref_t;

  index_file_refs() = default;
  ref_t add(std::string_view key) { return refs_.add(key); }
  void clear();
  bool remove(std::string_view key) { return refs_.remove(key); }

  counter_t& refs() noexcept { return refs_; }

 private:
  counter_t refs_;
};

using FileRefs = std::vector<index_file_refs::ref_t>;

// Represents common directory attributes
class directory_attributes {
 public:
  // 0 == pool_size -> use global allocator, noexcept
  explicit directory_attributes(std::unique_ptr<irs::encryption> enc = nullptr);
  virtual ~directory_attributes() = default;

  directory_attributes(directory_attributes&&) = default;
  directory_attributes& operator=(directory_attributes&&) = default;

  irs::encryption* encryption() const noexcept { return enc_.get(); }
  index_file_refs& refs() const noexcept { return *refs_; }

 private:
  std::unique_ptr<irs::encryption> enc_;
  std::unique_ptr<index_file_refs> refs_;
};

}  // namespace irs
