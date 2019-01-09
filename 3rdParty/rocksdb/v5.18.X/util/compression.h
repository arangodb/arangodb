// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#pragma once

#include <algorithm>
#include <limits>
#include <string>

#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "util/coding.h"
#include "util/compression_context_cache.h"
#include "util/memory_allocator.h"

#ifdef SNAPPY
#include <snappy.h>
#endif

#ifdef ZLIB
#include <zlib.h>
#endif

#ifdef BZIP2
#include <bzlib.h>
#endif

#if defined(LZ4)
#include <lz4.h>
#include <lz4hc.h>
#endif

#if defined(ZSTD)
#include <zstd.h>
#if ZSTD_VERSION_NUMBER >= 10103  // v1.1.3+
#include <zdict.h>
#endif  // ZSTD_VERSION_NUMBER >= 10103
namespace rocksdb {
// Need this for the context allocation override
// On windows we need to do this explicitly
#if (ZSTD_VERSION_NUMBER >= 500)
#if defined(ROCKSDB_JEMALLOC) && defined(OS_WIN) && \
    defined(ZSTD_STATIC_LINKING_ONLY)
#define ROCKSDB_ZSTD_CUSTOM_MEM
namespace port {
ZSTD_customMem GetJeZstdAllocationOverrides();
}  // namespace port
#endif  // defined(ROCKSDB_JEMALLOC) && defined(OS_WIN) &&
        // defined(ZSTD_STATIC_LINKING_ONLY)

// Cached data represents a portion that can be re-used
// If, in the future we have more than one native context to
// cache we can arrange this as a tuple
class ZSTDUncompressCachedData {
 public:
  using ZSTDNativeContext = ZSTD_DCtx*;
  ZSTDUncompressCachedData() {}
  // Init from cache
  ZSTDUncompressCachedData(const ZSTDUncompressCachedData& o) = delete;
  ZSTDUncompressCachedData& operator=(const ZSTDUncompressCachedData&) = delete;
  ZSTDUncompressCachedData(ZSTDUncompressCachedData&& o) ROCKSDB_NOEXCEPT
      : ZSTDUncompressCachedData() {
    *this = std::move(o);
  }
  ZSTDUncompressCachedData& operator=(ZSTDUncompressCachedData&& o)
      ROCKSDB_NOEXCEPT {
    assert(zstd_ctx_ == nullptr);
    std::swap(zstd_ctx_, o.zstd_ctx_);
    std::swap(cache_idx_, o.cache_idx_);
    return *this;
  }
  ZSTDNativeContext Get() const { return zstd_ctx_; }
  int64_t GetCacheIndex() const { return cache_idx_; }
  void CreateIfNeeded() {
    if (zstd_ctx_ == nullptr) {
#ifdef ROCKSDB_ZSTD_CUSTOM_MEM
      zstd_ctx_ =
          ZSTD_createDCtx_advanced(port::GetJeZstdAllocationOverrides());
#else   // ROCKSDB_ZSTD_CUSTOM_MEM
      zstd_ctx_ = ZSTD_createDCtx();
#endif  // ROCKSDB_ZSTD_CUSTOM_MEM
      cache_idx_ = -1;
    }
  }
  void InitFromCache(const ZSTDUncompressCachedData& o, int64_t idx) {
    zstd_ctx_ = o.zstd_ctx_;
    cache_idx_ = idx;
  }
  ~ZSTDUncompressCachedData() {
    if (zstd_ctx_ != nullptr && cache_idx_ == -1) {
      ZSTD_freeDCtx(zstd_ctx_);
    }
  }

 private:
  ZSTDNativeContext zstd_ctx_ = nullptr;
  int64_t cache_idx_ = -1;  // -1 means this instance owns the context
};
#endif  // (ZSTD_VERSION_NUMBER >= 500)
}  // namespace rocksdb
#endif  // ZSTD

#if !(defined ZSTD) || !(ZSTD_VERSION_NUMBER >= 500)
namespace rocksdb {
class ZSTDUncompressCachedData {
  void* padding;  // unused
 public:
  using ZSTDNativeContext = void*;
  ZSTDUncompressCachedData() {}
  ZSTDUncompressCachedData(const ZSTDUncompressCachedData&) {}
  ZSTDUncompressCachedData& operator=(const ZSTDUncompressCachedData&) = delete;
  ZSTDUncompressCachedData(ZSTDUncompressCachedData&&)
      ROCKSDB_NOEXCEPT = default;
  ZSTDUncompressCachedData& operator=(ZSTDUncompressCachedData&&)
      ROCKSDB_NOEXCEPT = default;
  ZSTDNativeContext Get() const { return nullptr; }
  int64_t GetCacheIndex() const { return -1; }
  void CreateIfNeeded() {}
  void InitFromCache(const ZSTDUncompressCachedData&, int64_t) {}
 private:
  void ignore_padding__() { padding = nullptr; }
};
}  // namespace rocksdb
#endif

#if defined(XPRESS)
#include "port/xpress.h"
#endif

namespace rocksdb {

// Instantiate this class and pass it to the uncompression API below
class CompressionContext {
 private:
  const CompressionType type_;
  const CompressionOptions opts_;
  Slice dict_;
#if defined(ZSTD) && (ZSTD_VERSION_NUMBER >= 500)
  ZSTD_CCtx* zstd_ctx_ = nullptr;
  void CreateNativeContext() {
    if (type_ == kZSTD || type_ == kZSTDNotFinalCompression) {
#ifdef ROCKSDB_ZSTD_CUSTOM_MEM
      zstd_ctx_ =
          ZSTD_createCCtx_advanced(port::GetJeZstdAllocationOverrides());
#else   // ROCKSDB_ZSTD_CUSTOM_MEM
      zstd_ctx_ = ZSTD_createCCtx();
#endif  // ROCKSDB_ZSTD_CUSTOM_MEM
    }
  }
  void DestroyNativeContext() {
    if (zstd_ctx_ != nullptr) {
      ZSTD_freeCCtx(zstd_ctx_);
    }
  }

 public:
  // callable inside ZSTD_Compress
  ZSTD_CCtx* ZSTDPreallocCtx() const {
    assert(type_ == kZSTD || type_ == kZSTDNotFinalCompression);
    return zstd_ctx_;
  }
#else   // ZSTD && (ZSTD_VERSION_NUMBER >= 500)
 private:
  void CreateNativeContext() {}
  void DestroyNativeContext() {}
#endif  // ZSTD && (ZSTD_VERSION_NUMBER >= 500)
 public:
  explicit CompressionContext(CompressionType comp_type) : type_(comp_type) {
    CreateNativeContext();
  }
  CompressionContext(CompressionType comp_type, const CompressionOptions& opts,
                     const Slice& comp_dict = Slice())
      : type_(comp_type), opts_(opts), dict_(comp_dict) {
    CreateNativeContext();
  }
  ~CompressionContext() { DestroyNativeContext(); }
  CompressionContext(const CompressionContext&) = delete;
  CompressionContext& operator=(const CompressionContext&) = delete;

  const CompressionOptions& options() const { return opts_; }
  CompressionType type() const { return type_; }
  const Slice& dict() const { return dict_; }
  Slice& dict() { return dict_; }
};

// Instantiate this class and pass it to the uncompression API below
class UncompressionContext {
 private:
  CompressionType type_;
  Slice dict_;
  CompressionContextCache* ctx_cache_ = nullptr;
  ZSTDUncompressCachedData uncomp_cached_data_;

 public:
  struct NoCache {};
  // Do not use context cache, used by TableBuilder
  UncompressionContext(NoCache, CompressionType comp_type) : type_(comp_type) {}
  explicit UncompressionContext(CompressionType comp_type)
      : UncompressionContext(comp_type, Slice()) {}
  UncompressionContext(CompressionType comp_type, const Slice& comp_dict)
      : type_(comp_type), dict_(comp_dict) {
    if (type_ == kZSTD || type_ == kZSTDNotFinalCompression) {
      ctx_cache_ = CompressionContextCache::Instance();
      uncomp_cached_data_ = ctx_cache_->GetCachedZSTDUncompressData();
    }
  }
  ~UncompressionContext() {
    if ((type_ == kZSTD || type_ == kZSTDNotFinalCompression) &&
        uncomp_cached_data_.GetCacheIndex() != -1) {
      assert(ctx_cache_ != nullptr);
      ctx_cache_->ReturnCachedZSTDUncompressData(
          uncomp_cached_data_.GetCacheIndex());
    }
  }
  UncompressionContext(const UncompressionContext&) = delete;
  UncompressionContext& operator=(const UncompressionContext&) = delete;

  ZSTDUncompressCachedData::ZSTDNativeContext GetZSTDContext() const {
    return uncomp_cached_data_.Get();
  }
  CompressionType type() const { return type_; }
  const Slice& dict() const { return dict_; }
  Slice& dict() { return dict_; }
};

inline bool Snappy_Supported() {
#ifdef SNAPPY
  return true;
#else
  return false;
#endif
}

inline bool Zlib_Supported() {
#ifdef ZLIB
  return true;
#else
  return false;
#endif
}

inline bool BZip2_Supported() {
#ifdef BZIP2
  return true;
#else
  return false;
#endif
}

inline bool LZ4_Supported() {
#ifdef LZ4
  return true;
#else
  return false;
#endif
}

inline bool XPRESS_Supported() {
#ifdef XPRESS
  return true;
#else
  return false;
#endif
}

inline bool ZSTD_Supported() {
#ifdef ZSTD
  // ZSTD format is finalized since version 0.8.0.
  return (ZSTD_versionNumber() >= 800);
#else
  return false;
#endif
}

inline bool ZSTDNotFinal_Supported() {
#ifdef ZSTD
  return true;
#else
  return false;
#endif
}

inline bool CompressionTypeSupported(CompressionType compression_type) {
  switch (compression_type) {
    case kNoCompression:
      return true;
    case kSnappyCompression:
      return Snappy_Supported();
    case kZlibCompression:
      return Zlib_Supported();
    case kBZip2Compression:
      return BZip2_Supported();
    case kLZ4Compression:
      return LZ4_Supported();
    case kLZ4HCCompression:
      return LZ4_Supported();
    case kXpressCompression:
      return XPRESS_Supported();
    case kZSTDNotFinalCompression:
      return ZSTDNotFinal_Supported();
    case kZSTD:
      return ZSTD_Supported();
    default:
      assert(false);
      return false;
  }
}

inline std::string CompressionTypeToString(CompressionType compression_type) {
  switch (compression_type) {
    case kNoCompression:
      return "NoCompression";
    case kSnappyCompression:
      return "Snappy";
    case kZlibCompression:
      return "Zlib";
    case kBZip2Compression:
      return "BZip2";
    case kLZ4Compression:
      return "LZ4";
    case kLZ4HCCompression:
      return "LZ4HC";
    case kXpressCompression:
      return "Xpress";
    case kZSTD:
      return "ZSTD";
    case kZSTDNotFinalCompression:
      return "ZSTDNotFinal";
    default:
      assert(false);
      return "";
  }
}

// compress_format_version can have two values:
// 1 -- decompressed sizes for BZip2 and Zlib are not included in the compressed
// block. Also, decompressed sizes for LZ4 are encoded in platform-dependent
// way.
// 2 -- Zlib, BZip2 and LZ4 encode decompressed size as Varint32 just before the
// start of compressed block. Snappy format is the same as version 1.

inline bool Snappy_Compress(const CompressionContext& /*ctx*/,
                            const char* input, size_t length,
                            ::std::string* output) {
#ifdef SNAPPY
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#else
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                         size_t* result) {
#ifdef SNAPPY
  return snappy::GetUncompressedLength(input, length, result);
#else
  (void)input;
  (void)length;
  (void)result;
  return false;
#endif
}

inline bool Snappy_Uncompress(const char* input, size_t length, char* output) {
#ifdef SNAPPY
  return snappy::RawUncompress(input, length, output);
#else
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

namespace compression {
// returns size
inline size_t PutDecompressedSizeInfo(std::string* output, uint32_t length) {
  PutVarint32(output, length);
  return output->size();
}

inline bool GetDecompressedSizeInfo(const char** input_data,
                                    size_t* input_length,
                                    uint32_t* output_len) {
  auto new_input_data =
      GetVarint32Ptr(*input_data, *input_data + *input_length, output_len);
  if (new_input_data == nullptr) {
    return false;
  }
  *input_length -= (new_input_data - *input_data);
  *input_data = new_input_data;
  return true;
}
}  // namespace compression

// compress_format_version == 1 -- decompressed size is not included in the
// block header
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline bool Zlib_Compress(const CompressionContext& ctx,
                          uint32_t compress_format_version, const char* input,
                          size_t length, ::std::string* output) {
#ifdef ZLIB
  if (length > std::numeric_limits<uint32_t>::max()) {
    // Can't compress more than 4GB
    return false;
  }

  size_t output_header_len = 0;
  if (compress_format_version == 2) {
    output_header_len = compression::PutDecompressedSizeInfo(
        output, static_cast<uint32_t>(length));
  }
  // Resize output to be the plain data length.
  // This may not be big enough if the compression actually expands data.
  output->resize(output_header_len + length);

  // The memLevel parameter specifies how much memory should be allocated for
  // the internal compression state.
  // memLevel=1 uses minimum memory but is slow and reduces compression ratio.
  // memLevel=9 uses maximum memory for optimal speed.
  // The default value is 8. See zconf.h for more details.
  static const int memLevel = 8;
  int level;
  if (ctx.options().level == CompressionOptions::kDefaultCompressionLevel) {
    level = Z_DEFAULT_COMPRESSION;
  } else {
    level = ctx.options().level;
  }
  z_stream _stream;
  memset(&_stream, 0, sizeof(z_stream));
  int st = deflateInit2(&_stream, level, Z_DEFLATED, ctx.options().window_bits,
                        memLevel, ctx.options().strategy);
  if (st != Z_OK) {
    return false;
  }

  if (ctx.dict().size()) {
    // Initialize the compression library's dictionary
    st = deflateSetDictionary(&_stream,
                              reinterpret_cast<const Bytef*>(ctx.dict().data()),
                              static_cast<unsigned int>(ctx.dict().size()));
    if (st != Z_OK) {
      deflateEnd(&_stream);
      return false;
    }
  }

  // Compress the input, and put compressed data in output.
  _stream.next_in = (Bytef*)input;
  _stream.avail_in = static_cast<unsigned int>(length);

  // Initialize the output size.
  _stream.avail_out = static_cast<unsigned int>(length);
  _stream.next_out = reinterpret_cast<Bytef*>(&(*output)[output_header_len]);

  bool compressed = false;
  st = deflate(&_stream, Z_FINISH);
  if (st == Z_STREAM_END) {
    compressed = true;
    output->resize(output->size() - _stream.avail_out);
  }
  // The only return value we really care about is Z_STREAM_END.
  // Z_OK means insufficient output space. This means the compression is
  // bigger than decompressed size. Just fail the compression in that case.

  deflateEnd(&_stream);
  return compressed;
#else
  (void)ctx;
  (void)compress_format_version;
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

// compress_format_version == 1 -- decompressed size is not included in the
// block header
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline CacheAllocationPtr Zlib_Uncompress(
    const UncompressionContext& ctx, const char* input_data,
    size_t input_length, int* decompress_size, uint32_t compress_format_version,
    MemoryAllocator* allocator = nullptr, int windowBits = -14) {
#ifdef ZLIB
  uint32_t output_len = 0;
  if (compress_format_version == 2) {
    if (!compression::GetDecompressedSizeInfo(&input_data, &input_length,
                                              &output_len)) {
      return nullptr;
    }
  } else {
    // Assume the decompressed data size will 5x of compressed size, but round
    // to the page size
    size_t proposed_output_len = ((input_length * 5) & (~(4096 - 1))) + 4096;
    output_len = static_cast<uint32_t>(
        std::min(proposed_output_len,
                 static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
  }

  z_stream _stream;
  memset(&_stream, 0, sizeof(z_stream));

  // For raw inflate, the windowBits should be -8..-15.
  // If windowBits is bigger than zero, it will use either zlib
  // header or gzip header. Adding 32 to it will do automatic detection.
  int st =
      inflateInit2(&_stream, windowBits > 0 ? windowBits + 32 : windowBits);
  if (st != Z_OK) {
    return nullptr;
  }

  if (ctx.dict().size()) {
    // Initialize the compression library's dictionary
    st = inflateSetDictionary(&_stream,
                              reinterpret_cast<const Bytef*>(ctx.dict().data()),
                              static_cast<unsigned int>(ctx.dict().size()));
    if (st != Z_OK) {
      return nullptr;
    }
  }

  _stream.next_in = (Bytef*)input_data;
  _stream.avail_in = static_cast<unsigned int>(input_length);

  auto output = AllocateBlock(output_len, allocator);

  _stream.next_out = (Bytef*)output.get();
  _stream.avail_out = static_cast<unsigned int>(output_len);

  bool done = false;
  while (!done) {
    st = inflate(&_stream, Z_SYNC_FLUSH);
    switch (st) {
      case Z_STREAM_END:
        done = true;
        break;
      case Z_OK: {
        // No output space. Increase the output space by 20%.
        // We should never run out of output space if
        // compress_format_version == 2
        assert(compress_format_version != 2);
        size_t old_sz = output_len;
        uint32_t output_len_delta = output_len / 5;
        output_len += output_len_delta < 10 ? 10 : output_len_delta;
        auto tmp = AllocateBlock(output_len, allocator);
        memcpy(tmp.get(), output.get(), old_sz);
        output = std::move(tmp);

        // Set more output.
        _stream.next_out = (Bytef*)(output.get() + old_sz);
        _stream.avail_out = static_cast<unsigned int>(output_len - old_sz);
        break;
      }
      case Z_BUF_ERROR:
      default:
        inflateEnd(&_stream);
        return nullptr;
    }
  }

  // If we encoded decompressed block size, we should have no bytes left
  assert(compress_format_version != 2 || _stream.avail_out == 0);
  *decompress_size = static_cast<int>(output_len - _stream.avail_out);
  inflateEnd(&_stream);
  return output;
#else
  (void)ctx;
  (void)input_data;
  (void)input_length;
  (void)decompress_size;
  (void)compress_format_version;
  (void)allocator;
  (void)windowBits;
  return nullptr;
#endif
}

// compress_format_version == 1 -- decompressed size is not included in the
// block header
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
inline bool BZip2_Compress(const CompressionContext& /*ctx*/,
                           uint32_t compress_format_version, const char* input,
                           size_t length, ::std::string* output) {
#ifdef BZIP2
  if (length > std::numeric_limits<uint32_t>::max()) {
    // Can't compress more than 4GB
    return false;
  }
  size_t output_header_len = 0;
  if (compress_format_version == 2) {
    output_header_len = compression::PutDecompressedSizeInfo(
        output, static_cast<uint32_t>(length));
  }
  // Resize output to be the plain data length.
  // This may not be big enough if the compression actually expands data.
  output->resize(output_header_len + length);

  bz_stream _stream;
  memset(&_stream, 0, sizeof(bz_stream));

  // Block size 1 is 100K.
  // 0 is for silent.
  // 30 is the default workFactor
  int st = BZ2_bzCompressInit(&_stream, 1, 0, 30);
  if (st != BZ_OK) {
    return false;
  }

  // Compress the input, and put compressed data in output.
  _stream.next_in = (char*)input;
  _stream.avail_in = static_cast<unsigned int>(length);

  // Initialize the output size.
  _stream.avail_out = static_cast<unsigned int>(length);
  _stream.next_out = reinterpret_cast<char*>(&(*output)[output_header_len]);

  bool compressed = false;
  st = BZ2_bzCompress(&_stream, BZ_FINISH);
  if (st == BZ_STREAM_END) {
    compressed = true;
    output->resize(output->size() - _stream.avail_out);
  }
  // The only return value we really care about is BZ_STREAM_END.
  // BZ_FINISH_OK means insufficient output space. This means the compression
  // is bigger than decompressed size. Just fail the compression in that case.

  BZ2_bzCompressEnd(&_stream);
  return compressed;
#else
  (void)compress_format_version;
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

// compress_format_version == 1 -- decompressed size is not included in the
// block header
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
inline CacheAllocationPtr BZip2_Uncompress(
    const char* input_data, size_t input_length, int* decompress_size,
    uint32_t compress_format_version, MemoryAllocator* allocator = nullptr) {
#ifdef BZIP2
  uint32_t output_len = 0;
  if (compress_format_version == 2) {
    if (!compression::GetDecompressedSizeInfo(&input_data, &input_length,
                                              &output_len)) {
      return nullptr;
    }
  } else {
    // Assume the decompressed data size will 5x of compressed size, but round
    // to the next page size
    size_t proposed_output_len = ((input_length * 5) & (~(4096 - 1))) + 4096;
    output_len = static_cast<uint32_t>(
        std::min(proposed_output_len,
                 static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
  }

  bz_stream _stream;
  memset(&_stream, 0, sizeof(bz_stream));

  int st = BZ2_bzDecompressInit(&_stream, 0, 0);
  if (st != BZ_OK) {
    return nullptr;
  }

  _stream.next_in = (char*)input_data;
  _stream.avail_in = static_cast<unsigned int>(input_length);

  auto output = AllocateBlock(output_len, allocator);

  _stream.next_out = (char*)output.get();
  _stream.avail_out = static_cast<unsigned int>(output_len);

  bool done = false;
  while (!done) {
    st = BZ2_bzDecompress(&_stream);
    switch (st) {
      case BZ_STREAM_END:
        done = true;
        break;
      case BZ_OK: {
        // No output space. Increase the output space by 20%.
        // We should never run out of output space if
        // compress_format_version == 2
        assert(compress_format_version != 2);
        uint32_t old_sz = output_len;
        output_len = output_len * 1.2;
        auto tmp = AllocateBlock(output_len, allocator);
        memcpy(tmp.get(), output.get(), old_sz);
        output = std::move(tmp);

        // Set more output.
        _stream.next_out = (char*)(output.get() + old_sz);
        _stream.avail_out = static_cast<unsigned int>(output_len - old_sz);
        break;
      }
      default:
        BZ2_bzDecompressEnd(&_stream);
        return nullptr;
    }
  }

  // If we encoded decompressed block size, we should have no bytes left
  assert(compress_format_version != 2 || _stream.avail_out == 0);
  *decompress_size = static_cast<int>(output_len - _stream.avail_out);
  BZ2_bzDecompressEnd(&_stream);
  return output;
#else
  (void)input_data;
  (void)input_length;
  (void)decompress_size;
  (void)compress_format_version;
  (void)allocator;
  return nullptr;
#endif
}

// compress_format_version == 1 -- decompressed size is included in the
// block header using memcpy, which makes database non-portable)
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline bool LZ4_Compress(const CompressionContext& ctx,
                         uint32_t compress_format_version, const char* input,
                         size_t length, ::std::string* output) {
#ifdef LZ4
  if (length > std::numeric_limits<uint32_t>::max()) {
    // Can't compress more than 4GB
    return false;
  }

  size_t output_header_len = 0;
  if (compress_format_version == 2) {
    // new encoding, using varint32 to store size information
    output_header_len = compression::PutDecompressedSizeInfo(
        output, static_cast<uint32_t>(length));
  } else {
    // legacy encoding, which is not really portable (depends on big/little
    // endianness)
    output_header_len = 8;
    output->resize(output_header_len);
    char* p = const_cast<char*>(output->c_str());
    memcpy(p, &length, sizeof(length));
  }
  int compress_bound = LZ4_compressBound(static_cast<int>(length));
  output->resize(static_cast<size_t>(output_header_len + compress_bound));

  int outlen;
#if LZ4_VERSION_NUMBER >= 10400  // r124+
  LZ4_stream_t* stream = LZ4_createStream();
  if (ctx.dict().size()) {
    LZ4_loadDict(stream, ctx.dict().data(),
                 static_cast<int>(ctx.dict().size()));
  }
#if LZ4_VERSION_NUMBER >= 10700  // r129+
  outlen =
      LZ4_compress_fast_continue(stream, input, &(*output)[output_header_len],
                                 static_cast<int>(length), compress_bound, 1);
#else  // up to r128
  outlen = LZ4_compress_limitedOutput_continue(
      stream, input, &(*output)[output_header_len], static_cast<int>(length),
      compress_bound);
#endif
  LZ4_freeStream(stream);
#else   // up to r123
  outlen = LZ4_compress_limitedOutput(input, &(*output)[output_header_len],
                                      static_cast<int>(length), compress_bound);
  (void)ctx;
#endif  // LZ4_VERSION_NUMBER >= 10400

  if (outlen == 0) {
    return false;
  }
  output->resize(static_cast<size_t>(output_header_len + outlen));
  return true;
#else  // LZ4
  (void)ctx;
  (void)compress_format_version;
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

// compress_format_version == 1 -- decompressed size is included in the
// block header using memcpy, which makes database non-portable)
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline CacheAllocationPtr LZ4_Uncompress(const UncompressionContext& ctx,
                                         const char* input_data,
                                         size_t input_length,
                                         int* decompress_size,
                                         uint32_t compress_format_version,
                                         MemoryAllocator* allocator = nullptr) {
#ifdef LZ4
  uint32_t output_len = 0;
  if (compress_format_version == 2) {
    // new encoding, using varint32 to store size information
    if (!compression::GetDecompressedSizeInfo(&input_data, &input_length,
                                              &output_len)) {
      return nullptr;
    }
  } else {
    // legacy encoding, which is not really portable (depends on big/little
    // endianness)
    if (input_length < 8) {
      return nullptr;
    }
    memcpy(&output_len, input_data, sizeof(output_len));
    input_length -= 8;
    input_data += 8;
  }

  auto output = AllocateBlock(output_len, allocator);
#if LZ4_VERSION_NUMBER >= 10400  // r124+
  LZ4_streamDecode_t* stream = LZ4_createStreamDecode();
  if (ctx.dict().size()) {
    LZ4_setStreamDecode(stream, ctx.dict().data(),
                        static_cast<int>(ctx.dict().size()));
  }
  *decompress_size = LZ4_decompress_safe_continue(
      stream, input_data, output.get(), static_cast<int>(input_length),
      static_cast<int>(output_len));
  LZ4_freeStreamDecode(stream);
#else   // up to r123
  *decompress_size = LZ4_decompress_safe(input_data, output.get(),
                                         static_cast<int>(input_length),
                                         static_cast<int>(output_len));
  (void)ctx;
#endif  // LZ4_VERSION_NUMBER >= 10400

  if (*decompress_size < 0) {
    return nullptr;
  }
  assert(*decompress_size == static_cast<int>(output_len));
  return output;
#else  // LZ4
  (void)ctx;
  (void)input_data;
  (void)input_length;
  (void)decompress_size;
  (void)compress_format_version;
  (void)allocator;
  return nullptr;
#endif
}

// compress_format_version == 1 -- decompressed size is included in the
// block header using memcpy, which makes database non-portable)
// compress_format_version == 2 -- decompressed size is included in the block
// header in varint32 format
// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline bool LZ4HC_Compress(const CompressionContext& ctx,
                           uint32_t compress_format_version, const char* input,
                           size_t length, ::std::string* output) {
#ifdef LZ4
  if (length > std::numeric_limits<uint32_t>::max()) {
    // Can't compress more than 4GB
    return false;
  }

  size_t output_header_len = 0;
  if (compress_format_version == 2) {
    // new encoding, using varint32 to store size information
    output_header_len = compression::PutDecompressedSizeInfo(
        output, static_cast<uint32_t>(length));
  } else {
    // legacy encoding, which is not really portable (depends on big/little
    // endianness)
    output_header_len = 8;
    output->resize(output_header_len);
    char* p = const_cast<char*>(output->c_str());
    memcpy(p, &length, sizeof(length));
  }
  int compress_bound = LZ4_compressBound(static_cast<int>(length));
  output->resize(static_cast<size_t>(output_header_len + compress_bound));

  int outlen;
  int level;
  if (ctx.options().level == CompressionOptions::kDefaultCompressionLevel) {
    level = 0;  // lz4hc.h says any value < 1 will be sanitized to default
  } else {
    level = ctx.options().level;
  }
#if LZ4_VERSION_NUMBER >= 10400  // r124+
  LZ4_streamHC_t* stream = LZ4_createStreamHC();
  LZ4_resetStreamHC(stream, level);
  const char* compression_dict_data =
      ctx.dict().size() > 0 ? ctx.dict().data() : nullptr;
  size_t compression_dict_size = ctx.dict().size();
  LZ4_loadDictHC(stream, compression_dict_data,
                 static_cast<int>(compression_dict_size));

#if LZ4_VERSION_NUMBER >= 10700  // r129+
  outlen =
      LZ4_compress_HC_continue(stream, input, &(*output)[output_header_len],
                               static_cast<int>(length), compress_bound);
#else   // r124-r128
  outlen = LZ4_compressHC_limitedOutput_continue(
      stream, input, &(*output)[output_header_len], static_cast<int>(length),
      compress_bound);
#endif  // LZ4_VERSION_NUMBER >= 10700
  LZ4_freeStreamHC(stream);

#elif LZ4_VERSION_MAJOR  // r113-r123
  outlen = LZ4_compressHC2_limitedOutput(input, &(*output)[output_header_len],
                                         static_cast<int>(length),
                                         compress_bound, level);
#else                    // up to r112
  outlen =
      LZ4_compressHC_limitedOutput(input, &(*output)[output_header_len],
                                   static_cast<int>(length), compress_bound);
#endif                   // LZ4_VERSION_NUMBER >= 10400

  if (outlen == 0) {
    return false;
  }
  output->resize(static_cast<size_t>(output_header_len + outlen));
  return true;
#else  // LZ4
  (void)ctx;
  (void)compress_format_version;
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

#ifdef XPRESS
inline bool XPRESS_Compress(const char* input, size_t length,
                            std::string* output) {
  return port::xpress::Compress(input, length, output);
}
#else
inline bool XPRESS_Compress(const char* /*input*/, size_t /*length*/,
                            std::string* /*output*/) {
  return false;
}
#endif

#ifdef XPRESS
inline char* XPRESS_Uncompress(const char* input_data, size_t input_length,
                               int* decompress_size) {
  return port::xpress::Decompress(input_data, input_length, decompress_size);
}
#else
inline char* XPRESS_Uncompress(const char* /*input_data*/,
                               size_t /*input_length*/,
                               int* /*decompress_size*/) {
  return nullptr;
}
#endif

// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline bool ZSTD_Compress(const CompressionContext& ctx, const char* input,
                          size_t length, ::std::string* output) {
#ifdef ZSTD
  if (length > std::numeric_limits<uint32_t>::max()) {
    // Can't compress more than 4GB
    return false;
  }

  size_t output_header_len = compression::PutDecompressedSizeInfo(
      output, static_cast<uint32_t>(length));

  size_t compressBound = ZSTD_compressBound(length);
  output->resize(static_cast<size_t>(output_header_len + compressBound));
  size_t outlen = 0;
  int level;
  if (ctx.options().level == CompressionOptions::kDefaultCompressionLevel) {
    // 3 is the value of ZSTD_CLEVEL_DEFAULT (not exposed publicly), see
    // https://github.com/facebook/zstd/issues/1148
    level = 3;
  } else {
    level = ctx.options().level;
  }
#if ZSTD_VERSION_NUMBER >= 500  // v0.5.0+
  ZSTD_CCtx* context = ctx.ZSTDPreallocCtx();
  assert(context != nullptr);
  outlen = ZSTD_compress_usingDict(context, &(*output)[output_header_len],
                                   compressBound, input, length,
                                   ctx.dict().data(), ctx.dict().size(), level);
#else   // up to v0.4.x
  outlen = ZSTD_compress(&(*output)[output_header_len], compressBound, input,
                         length, level);
#endif  // ZSTD_VERSION_NUMBER >= 500
  if (outlen == 0) {
    return false;
  }
  output->resize(output_header_len + outlen);
  return true;
#else  // ZSTD
  (void)ctx;
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

// @param compression_dict Data for presetting the compression library's
//    dictionary.
inline CacheAllocationPtr ZSTD_Uncompress(
    const UncompressionContext& ctx, const char* input_data,
    size_t input_length, int* decompress_size,
    MemoryAllocator* allocator = nullptr) {
#ifdef ZSTD
  uint32_t output_len = 0;
  if (!compression::GetDecompressedSizeInfo(&input_data, &input_length,
                                            &output_len)) {
    return nullptr;
  }

  auto output = AllocateBlock(output_len, allocator);
  size_t actual_output_length;
#if ZSTD_VERSION_NUMBER >= 500  // v0.5.0+
  ZSTD_DCtx* context = ctx.GetZSTDContext();
  assert(context != nullptr);
  actual_output_length = ZSTD_decompress_usingDict(
      context, output.get(), output_len, input_data, input_length,
      ctx.dict().data(), ctx.dict().size());
#else   // up to v0.4.x
  actual_output_length =
      ZSTD_decompress(output.get(), output_len, input_data, input_length);
#endif  // ZSTD_VERSION_NUMBER >= 500
  assert(actual_output_length == output_len);
  *decompress_size = static_cast<int>(actual_output_length);
  return output;
#else  // ZSTD
  (void)ctx;
  (void)input_data;
  (void)input_length;
  (void)decompress_size;
  (void)allocator;
  return nullptr;
#endif
}

inline std::string ZSTD_TrainDictionary(const std::string& samples,
                                        const std::vector<size_t>& sample_lens,
                                        size_t max_dict_bytes) {
  // Dictionary trainer is available since v0.6.1 for static linking, but not
  // available for dynamic linking until v1.1.3. For now we enable the feature
  // in v1.1.3+ only.
#if ZSTD_VERSION_NUMBER >= 10103  // v1.1.3+
  std::string dict_data(max_dict_bytes, '\0');
  size_t dict_len = ZDICT_trainFromBuffer(
      &dict_data[0], max_dict_bytes, &samples[0], &sample_lens[0],
      static_cast<unsigned>(sample_lens.size()));
  if (ZDICT_isError(dict_len)) {
    return "";
  }
  assert(dict_len <= max_dict_bytes);
  dict_data.resize(dict_len);
  return dict_data;
#else   // up to v1.1.2
  assert(false);
  (void)samples;
  (void)sample_lens;
  (void)max_dict_bytes;
  return "";
#endif  // ZSTD_VERSION_NUMBER >= 10103
}

inline std::string ZSTD_TrainDictionary(const std::string& samples,
                                        size_t sample_len_shift,
                                        size_t max_dict_bytes) {
  // Dictionary trainer is available since v0.6.1, but ZSTD was marked stable
  // only since v0.8.0. For now we enable the feature in stable versions only.
#if ZSTD_VERSION_NUMBER >= 10103  // v1.1.3+
  // skips potential partial sample at the end of "samples"
  size_t num_samples = samples.size() >> sample_len_shift;
  std::vector<size_t> sample_lens(num_samples, size_t(1) << sample_len_shift);
  return ZSTD_TrainDictionary(samples, sample_lens, max_dict_bytes);
#else   // up to v1.1.2
  assert(false);
  (void)samples;
  (void)sample_len_shift;
  (void)max_dict_bytes;
  return "";
#endif  // ZSTD_VERSION_NUMBER >= 10103
}

}  // namespace rocksdb
