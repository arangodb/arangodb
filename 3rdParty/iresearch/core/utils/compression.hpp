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

#ifndef IRESEARCH_COMPRESSION_H
#define IRESEARCH_COMPRESSION_H

#include "string.hpp"
#include "noncopyable.hpp"

#include <memory>

NS_ROOT

class IRESEARCH_API compressor: public bytes_ref, private util::noncopyable {
 public:
  explicit compressor(unsigned int chunk_size);

  void compress(const char* src, size_t size);

  inline void compress(const bytes_ref& src) {
    compress(ref_cast<char>(src).c_str(), src.size());
  }

 private:
  struct IRESEARCH_API deleter {
    void operator()(void* p) NOEXCEPT;
  };

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string buf_;
  int dict_size_; // the size of the LZ4 dictionary from the previous call
  std::unique_ptr<void, deleter> stream_; // hide internal LZ4 implementation
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // compressor

class IRESEARCH_API decompressor {
 public:
  decompressor();
  decompressor(const decompressor&) = default;
  decompressor& operator=(const decompressor&) = default;

  // returns number of decompressed bytes,
  // or integer_traits<size_t>::const_max in case of error
  size_t deflate(
    const char* src, size_t src_size, 
    char* dst, size_t dst_size
  ) const;

 private:
  struct IRESEARCH_API deleter {
    void operator()(void* p) NOEXCEPT;
  };

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::unique_ptr<void, deleter> stream_; // hide internal LZ4 implementation
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // decompressor

NS_END // NS_ROOT

#endif
