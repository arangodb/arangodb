//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string buf_;
  int dict_size_; // the size of the LZ4 dictionary from the previous call
  std::shared_ptr<void> stream_; // hide internal LZ4 implementation
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
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::shared_ptr<void> stream_; // hide internal LZ4 implementation
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // decompressor

NS_END // NS_ROOT

#endif
