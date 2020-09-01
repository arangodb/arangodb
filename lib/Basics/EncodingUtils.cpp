////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/velocypack-aliases.h>

#include <zconf.h>
#include <zlib.h>

namespace arangodb {

static constexpr size_t MaxDecompressedSize = 512 * 1024 * 1024;

bool encoding::gzipUncompress(uint8_t* compressed, size_t compressedLength,
                              VPackBuffer<uint8_t>& uncompressed) {
  uncompressed.clear();
  uncompressed.reserve(static_cast<size_t>(compressedLength * 1.25));

  if (compressedLength == 0) {
    /* empty input */
    return true;
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = reinterpret_cast<Bytef*>(compressed);
  strm.avail_in = (uInt)compressedLength;

  if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
    return false;
  }

  int ret;
  char outbuffer[32768];

  do {
    strm.next_out = reinterpret_cast<Bytef*>(outbuffer);
    strm.avail_out = sizeof(outbuffer);

    ret = inflate(&strm, 0);

    if (uncompressed.size() < strm.total_out) {
      uncompressed.append(outbuffer, strm.total_out - uncompressed.size());
    }
    if (uncompressed.size() > MaxDecompressedSize) {
      uncompressed.clear();
      inflateEnd(&strm);
      return false;
    }
  } while (ret == Z_OK);

  inflateEnd(&strm);

  return (ret == Z_STREAM_END);
}

bool encoding::gzipDeflate(uint8_t* compressed, size_t compressedLength,
                           VPackBuffer<uint8_t>& uncompressed) {
  uncompressed.clear();
  uncompressed.reserve(static_cast<size_t>(compressedLength * 1.25));

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = reinterpret_cast<Bytef*>(compressed);
  strm.avail_in = (uInt)compressedLength;

  if (inflateInit(&strm) != Z_OK) {
    return false;
  }

  int ret;
  char outbuffer[32768];

  do {
    strm.next_out = reinterpret_cast<Bytef*>(outbuffer);
    strm.avail_out = sizeof(outbuffer);

    ret = inflate(&strm, 0);

    if (uncompressed.size() < strm.total_out) {
      uncompressed.append(outbuffer, strm.total_out - uncompressed.size());
    }
    if (uncompressed.size() > MaxDecompressedSize) {
      uncompressed.clear();
      inflateEnd(&strm);
      return false;
    }
  } while (ret == Z_OK);

  inflateEnd(&strm);

  return (ret == Z_STREAM_END);
}

}  // namespace arangodb
