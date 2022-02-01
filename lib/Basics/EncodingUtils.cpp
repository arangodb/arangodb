////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <zconf.h>
#include <zlib.h>

namespace {
constexpr size_t maxUncompressedSize = 512 * 1024 * 1024;

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
    uncompressed.reserve(static_cast<size_t>(strm.total_out));

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

}  // namespace

namespace arangodb {

template<typename T>
ErrorCode encoding::gzipUncompress(uint8_t const* compressed,
                                   size_t compressedLength, T& uncompressed) {
  uncompressed.clear();

  if (compressedLength == 0) {
    // empty input
    return TRI_ERROR_NO_ERROR;
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<Bytef const*>(compressed));
  strm.avail_in = static_cast<uInt>(compressedLength);

  if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int ret = ::uncompressStream(strm, uncompressed);
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

template<typename T>
ErrorCode encoding::gzipInflate(uint8_t const* compressed,
                                size_t compressedLength, T& uncompressed) {
  uncompressed.clear();

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<Bytef const*>(compressed));
  strm.avail_in = (uInt)compressedLength;

  if (inflateInit(&strm) != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int ret = ::uncompressStream(strm, uncompressed);
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

template<typename T>
ErrorCode encoding::gzipDeflate(uint8_t const* uncompressed,
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

// template instantiations
template ErrorCode
encoding::gzipUncompress<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::velocypack::Buffer<uint8_t>& uncompressed);

template ErrorCode encoding::gzipInflate<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::velocypack::Buffer<uint8_t>& uncompressed);

template ErrorCode encoding::gzipInflate<arangodb::basics::StringBuffer>(
    uint8_t const* compressed, size_t compressedLength,
    arangodb::basics::StringBuffer& uncompressed);

template ErrorCode encoding::gzipDeflate<arangodb::velocypack::Buffer<uint8_t>>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::velocypack::Buffer<uint8_t>& compressed);

template ErrorCode encoding::gzipDeflate<arangodb::basics::StringBuffer>(
    uint8_t const* uncompressed, size_t uncompressedLength,
    arangodb::basics::StringBuffer& compressed);

}  // namespace arangodb
