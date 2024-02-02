////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "EncodingUtils.h"
#include "Basics/Endian.h"
#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

#include <velocypack/Buffer.h>

#include <string>
#include <lz4.h>
#include <zconf.h>
#include <zlib.h>

namespace {
constexpr size_t maxUncompressedSize = 512 * 1024 * 1024;

constexpr size_t lz4HeaderLength = 1 + sizeof(uint32_t);

template<typename T>
int uncompressStream(z_stream& strm, T& uncompressed) noexcept {
  TRI_ASSERT(strm.opaque == Z_NULL);

  // check that uncompressed size will be in the allowed range
  if (strm.total_out > ::maxUncompressedSize) {
    return Z_MEM_ERROR;
  }

  char outbuffer[32768];
  int ret = Z_DATA_ERROR;

  try {
    if constexpr (std::is_same_v<T, arangodb::basics::StringBuffer>) {
      // non-throwing variant of reserve(): check return value
      auto res = uncompressed.reserve(static_cast<size_t>(strm.total_out));
      if (res != TRI_ERROR_NO_ERROR) {
        return Z_MEM_ERROR;
      }
    } else {
      // throwing variant of reserve()
      uncompressed.reserve(static_cast<size_t>(strm.total_out));
    }
    // reserve() was successful if we get here

    do {
      strm.next_out = reinterpret_cast<Bytef*>(outbuffer);
      strm.avail_out = sizeof(outbuffer);

      // ret will change from Z_OK to Z_STREAM_END if we have read all data
      ret = inflate(&strm, Z_NO_FLUSH);

      if (uncompressed.size() < strm.total_out) {
        uncompressed.append(outbuffer, strm.total_out - uncompressed.size());
      }
      if (uncompressed.size() > ::maxUncompressedSize) {
        // should not happen
        uncompressed.clear();
        return Z_DATA_ERROR;
      }
    } while (ret == Z_OK);
  } catch (...) {
    // we can get here only if uncompressed.reserve() or uncompressed.append()
    // throw
    uncompressed.clear();
    ret = Z_MEM_ERROR;
  }

  return ret;
}

template<typename T>
int compressStream(z_stream& strm, T& compressed) noexcept {
  TRI_ASSERT(strm.opaque == Z_NULL);

  char outbuffer[32768];
  int ret = Z_DATA_ERROR;

  Bytef* p = reinterpret_cast<Bytef*>(strm.next_in);
  Bytef* e = p + strm.avail_in;

  try {
    while (p < e) {
      int flush;
      strm.next_in = p;

      if (e - p > static_cast<ptrdiff_t>(sizeof(outbuffer))) {
        strm.avail_in = static_cast<uInt>(sizeof(outbuffer));
        flush = Z_NO_FLUSH;
      } else {
        strm.avail_in = static_cast<uInt>(e - p);
        flush = Z_FINISH;
      }
      p += strm.avail_in;

      do {
        strm.next_out = reinterpret_cast<Bytef*>(outbuffer);
        strm.avail_out = sizeof(outbuffer);

        // ret will change from Z_OK to Z_STREAM_END if we have read all data
        ret = deflate(&strm, flush);

        if (ret == Z_STREAM_ERROR) {
          break;
        }
        compressed.append(&outbuffer[0], sizeof(outbuffer) - strm.avail_out);

      } while (strm.avail_out == 0);
    }
  } catch (...) {
    // we can get here only if compressed.append() throws
    compressed.clear();
    ret = Z_MEM_ERROR;
  }

  return ret;
}

template<typename T>
ErrorCode uncompressWrapper(
    uint8_t const* compressed, size_t compressedLength, T& uncompressed,
    std::function<ErrorCode(z_stream& strm)> const& initCb) {
  uncompressed.clear();

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<Bytef const*>(compressed));
  strm.avail_in = static_cast<uInt>(compressedLength);

  ErrorCode res = initCb(strm);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  int ret = uncompressStream(strm, uncompressed);
  inflateEnd(&strm);

  switch (ret) {
    case Z_STREAM_END:
      return TRI_ERROR_NO_ERROR;
    case Z_MEM_ERROR:
      return TRI_ERROR_OUT_OF_MEMORY;
    default:
      return TRI_ERROR_INTERNAL;
  }
}

}  // namespace

namespace arangodb {

template<typename T>
ErrorCode encoding::gzipUncompress(uint8_t const* compressed,
                                   size_t compressedLength, T& uncompressed) {
  if (compressedLength == 0) {
    // empty input
    return TRI_ERROR_NO_ERROR;
  }

  return ::uncompressWrapper<T>(
      compressed, compressedLength, uncompressed,
      [](z_stream& strm) -> ErrorCode {
        if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }
        return TRI_ERROR_NO_ERROR;
      });
}

template<typename T>
ErrorCode encoding::zlibInflate(uint8_t const* compressed,
                                size_t compressedLength, T& uncompressed) {
  return ::uncompressWrapper<T>(compressed, compressedLength, uncompressed,
                                [](z_stream& strm) -> ErrorCode {
                                  if (inflateInit(&strm) != Z_OK) {
                                    return TRI_ERROR_OUT_OF_MEMORY;
                                  }
                                  return TRI_ERROR_NO_ERROR;
                                });
}

template<typename T>
ErrorCode encoding::lz4Uncompress(uint8_t const* compressed,
                                  size_t compressedLength, T& uncompressed) {
  if (compressedLength <= ::lz4HeaderLength) {
    // empty/bad input.
    return TRI_ERROR_BAD_PARAMETER;
  }

  size_t initial = uncompressed.size();

  uint32_t uncompressedLength;
  // uncompressed size is encoded in bytes 1-4, starting from offset 0,
  // encoded as a uint32_t in big endian format
  memcpy(&uncompressedLength, compressed + 1, sizeof(uncompressedLength));
  uncompressedLength = basics::bigToHost<uint32_t>(uncompressedLength);

  if (uncompressedLength == 0 ||
      uncompressedLength >= static_cast<size_t>(LZ4_MAX_INPUT_SIZE)) {
    // uncompressed size is larger than what LZ4 can actually compress.
    // suspicious!
    return TRI_ERROR_BAD_PARAMETER;
  }

  if constexpr (std::is_same_v<T, std::string>) {
    uncompressed.resize(uncompressedLength);
  } else if constexpr (std::is_same_v<T,
                                      arangodb::velocypack::Buffer<uint8_t>>) {
    uncompressed.reserve(uncompressedLength);
  }

  // uncompress directly into the result
  // this should not go wrong because we have a big enough output buffer.
  int size = LZ4_decompress_safe(
      reinterpret_cast<char const*>(compressed) + ::lz4HeaderLength,
      const_cast<char*>(reinterpret_cast<char const*>(uncompressed.data())),
      static_cast<int>(compressedLength - ::lz4HeaderLength),
      static_cast<int>(uncompressedLength));
  TRI_ASSERT(size > 0);
  TRI_ASSERT(size < LZ4_MAX_INPUT_SIZE);
  TRI_ASSERT(uncompressedLength == static_cast<size_t>(size));
  if (size <= 0 || size >= LZ4_MAX_INPUT_SIZE) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  if constexpr (std::is_same_v<T, std::string>) {
    uncompressed.resize(initial + uncompressedLength);
  } else if constexpr (std::is_same_v<T,
                                      arangodb::velocypack::Buffer<uint8_t>>) {
    uncompressed.resetTo(initial + uncompressedLength);
  }

  return TRI_ERROR_NO_ERROR;
}

template<typename T>
ErrorCode encoding::gzipCompress(uint8_t const* uncompressed,
                                 size_t uncompressedLength, T& compressed) {
  compressed.clear();

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in =
      const_cast<Bytef*>(reinterpret_cast<Bytef const*>(uncompressed));
  strm.avail_in = static_cast<uInt>(uncompressedLength);

  // from https://stackoverflow.com/a/57699371/3042070:
  //   hard to believe they don't have a macro for gzip encoding, "Add 16" is
  //   the best thing zlib can do: "Add 16 to windowBits to write a simple gzip
  //   header and trailer around the compressed data instead of a zlib wrapper"
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int ret = ::compressStream(strm, compressed);
  deflateEnd(&strm);

  switch (ret) {
    case Z_STREAM_END:
      return TRI_ERROR_NO_ERROR;
    case Z_MEM_ERROR:
      return TRI_ERROR_OUT_OF_MEMORY;
    default:
      return TRI_ERROR_INTERNAL;
  }
}

template<typename T>
ErrorCode encoding::zlibDeflate(uint8_t const* uncompressed,
                                size_t uncompressedLength, T& compressed) {
  compressed.clear();

  if (uncompressedLength == 0) {
    // empty input
    return TRI_ERROR_NO_ERROR;
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in =
      const_cast<Bytef*>(reinterpret_cast<Bytef const*>(uncompressed));
  strm.avail_in = static_cast<uInt>(uncompressedLength);

  if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int ret = ::compressStream(strm, compressed);
  deflateEnd(&strm);

  switch (ret) {
    case Z_STREAM_END:
      return TRI_ERROR_NO_ERROR;
    case Z_MEM_ERROR:
      return TRI_ERROR_OUT_OF_MEMORY;
    default:
      return TRI_ERROR_INTERNAL;
  }
}

template<typename T>
ErrorCode encoding::lz4Compress(uint8_t const* uncompressed,
                                size_t uncompressedLength, T& compressed) {
  TRI_ASSERT(uncompressedLength > 0);

  compressed.clear();
  if (uncompressedLength >= static_cast<size_t>(LZ4_MAX_INPUT_SIZE)) {
    return TRI_ERROR_BAD_PARAMETER;
  }
  int maxLength = LZ4_compressBound(static_cast<int>(uncompressedLength));
  if (maxLength <= 0) {
    return TRI_ERROR_BAD_PARAMETER;
  }
  // store compressed value.
  // the compressed value is prepended by the following header:
  // - byte 0: hard-coded to 0x01 (can be used as a version number later)
  // - byte 1-4: uncompressed size, encoded as a uint32_t in big endian
  // order
  uint32_t originalLength = static_cast<uint32_t>(uncompressedLength);
  originalLength = basics::hostToBig(originalLength);

  if constexpr (std::is_same_v<T, std::string>) {
    compressed.resize(static_cast<size_t>(maxLength) + ::lz4HeaderLength);
    compressed[0] = 0x01U;  // version
    memcpy(compressed.data() + 1, &originalLength, sizeof(originalLength));
  } else if constexpr (std::is_same_v<T,
                                      arangodb::velocypack::Buffer<uint8_t>>) {
    compressed.reserve(static_cast<size_t>(maxLength) + ::lz4HeaderLength);
    compressed.push_back(0x01U);  // version
    memcpy(compressed.data() + 1, &originalLength, sizeof(originalLength));
  } else if constexpr (std::is_same_v<T, arangodb::basics::StringBuffer>) {
    auto res =
        compressed.reserve(static_cast<size_t>(maxLength) + ::lz4HeaderLength);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    compressed.appendChar(0x01U);  // version
    compressed.appendText(reinterpret_cast<char const*>(&originalLength),
                          sizeof(originalLength));
  }

  // compress data into output buffer. writes start at byte 5.
  int compressedLength = LZ4_compress_fast(
      reinterpret_cast<char const*>(uncompressed),
      const_cast<char*>(
          reinterpret_cast<char const*>(compressed.data() + ::lz4HeaderLength)),
      static_cast<int>(uncompressedLength),
      static_cast<int>(maxLength - ::lz4HeaderLength),
      /*accelerationFactor*/ 1);

  if (compressedLength <= 0) {
    return TRI_ERROR_INTERNAL;
  }

  if constexpr (std::is_same_v<T, std::string>) {
    compressed.resize(compressedLength + ::lz4HeaderLength);
  } else if constexpr (std::is_same_v<T,
                                      arangodb::velocypack::Buffer<uint8_t>>) {
    compressed.resetTo(compressedLength + ::lz4HeaderLength);
  } else if constexpr (std::is_same_v<T, arangodb::basics::StringBuffer>) {
    compressed.increaseLength(compressedLength);
  }

  return TRI_ERROR_NO_ERROR;
}

// template instantiations

// uncompress methods
template ErrorCode
encoding::gzipUncompress<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::velocypack::Buffer<uint8_t>& uncompressed);

template ErrorCode encoding::gzipUncompress<arangodb::basics::StringBuffer>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::basics::StringBuffer& uncompressed);

template ErrorCode encoding::gzipUncompress<std::string>(
    uint8_t const* compressed, size_t compressedLength,
    std::string& uncompressed);

template ErrorCode encoding::zlibInflate<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::velocypack::Buffer<uint8_t>& uncompressed);

template ErrorCode encoding::zlibInflate<arangodb::basics::StringBuffer>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::basics::StringBuffer& uncompressed);

template ErrorCode encoding::zlibInflate<std::string>(
    uint8_t const* compressed, size_t compressedLength,
    std::string& uncompressed);

template ErrorCode
encoding::lz4Uncompress<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::velocypack::Buffer<uint8_t>& uncompressed);

template ErrorCode encoding::lz4Uncompress<arangodb::basics::StringBuffer>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::basics::StringBuffer& uncompressed);

// compression methods
template ErrorCode
encoding::gzipCompress<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::velocypack::Buffer<uint8_t>& compressed);

template ErrorCode encoding::gzipCompress<arangodb::basics::StringBuffer>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::basics::StringBuffer& compressed);

template ErrorCode encoding::zlibDeflate<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::velocypack::Buffer<uint8_t>& compressed);

template ErrorCode encoding::zlibDeflate<arangodb::basics::StringBuffer>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::basics::StringBuffer& compressed);

template ErrorCode encoding::zlibDeflate<std::string>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    std::string& compressed);

template ErrorCode encoding::lz4Compress<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::velocypack::Buffer<uint8_t>& compressed);

template ErrorCode encoding::lz4Compress<arangodb::basics::StringBuffer>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::basics::StringBuffer& compressed);

}  // namespace arangodb
