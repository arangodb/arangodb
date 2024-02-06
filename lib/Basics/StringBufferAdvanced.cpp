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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "StringBuffer.h"

#include "Basics/EncodingUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "Basics/fpconv.h"
#include "Basics/tri-strings.h"
#include "Zip/zip.h"

#include <openssl/sha.h>

#include <cmath>
#include <memory>

ErrorCode arangodb::basics::StringBuffer::zlibDeflate(bool onlyIfSmaller) {
  arangodb::basics::StringBuffer deflated;

  ErrorCode code = arangodb::encoding::zlibDeflate(
      reinterpret_cast<uint8_t const*>(data()), size(), deflated);

  if (code == TRI_ERROR_NO_ERROR) {
    if (onlyIfSmaller && deflated.size() >= size()) {
      code = TRI_ERROR_DISABLED;
    } else {
      swap(&deflated);
    }
  }
  return code;
}

ErrorCode arangodb::basics::StringBuffer::gzipCompress(bool onlyIfSmaller) {
  arangodb::basics::StringBuffer gzipped;

  ErrorCode code = arangodb::encoding::gzipCompress(
      reinterpret_cast<uint8_t const*>(data()), size(), gzipped);

  if (code == TRI_ERROR_NO_ERROR) {
    if (onlyIfSmaller && gzipped.size() >= size()) {
      code = TRI_ERROR_DISABLED;
    } else {
      swap(&gzipped);
    }
  }
  return code;
}

ErrorCode arangodb::basics::StringBuffer::lz4Compress(bool onlyIfSmaller) {
  arangodb::basics::StringBuffer compressed;

  ErrorCode code = arangodb::encoding::lz4Compress(
      reinterpret_cast<uint8_t const*>(data()), size(), compressed);

  if (code == TRI_ERROR_NO_ERROR) {
    if (onlyIfSmaller && compressed.size() >= size()) {
      code = TRI_ERROR_DISABLED;
    } else {
      swap(&compressed);
    }
  }
  return code;
}

/// @brief uncompress the buffer into StringBuffer out, using zlib-inflate
ErrorCode arangodb::basics::StringBuffer::zlibInflate(
    arangodb::basics::StringBuffer& out, size_t skip) {
  uint8_t const* p = reinterpret_cast<uint8_t const*>(data());
  size_t length = size();

  if (length < skip) {
    length = 0;
  } else {
    p += skip;
    length -= skip;
  }

  return arangodb::encoding::zlibInflate(p, length, out);
}

/// @brief uncompress the buffer into StringBuffer out, using gzip uncompress
ErrorCode arangodb::basics::StringBuffer::gzipUncompress(
    arangodb::basics::StringBuffer& out, size_t skip) {
  uint8_t const* p = reinterpret_cast<uint8_t const*>(data());
  size_t length = size();

  if (length < skip) {
    length = 0;
  } else {
    p += skip;
    length -= skip;
  }

  return arangodb::encoding::gzipUncompress(p, length, out);
}

/// @brief uncompress the buffer into StringBuffer out, using lz4 uncompress
ErrorCode arangodb::basics::StringBuffer::lz4Uncompress(
    arangodb::basics::StringBuffer& out, size_t skip) {
  uint8_t const* p = reinterpret_cast<uint8_t const*>(data());
  size_t length = size();

  if (length < skip) {
    length = 0;
  } else {
    p += skip;
    length -= skip;
  }

  return arangodb::encoding::lz4Uncompress(p, length, out);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256 of a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_SHA256String(char const* source, size_t sourceLen, size_t* dstLen) {
  unsigned char* dst =
      static_cast<unsigned char*>(TRI_Allocate(SHA256_DIGEST_LENGTH));
  if (dst == nullptr) {
    return nullptr;
  }
  *dstLen = SHA256_DIGEST_LENGTH;

  SHA256((unsigned char const*)source, sourceLen, dst);

  return (char*)dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unescapes unicode escape sequences
////////////////////////////////////////////////////////////////////////////////

char* TRI_UnescapeUtf8String(char const* in, size_t inLength, size_t* outLength,
                             bool normalize) {
  char* buffer = static_cast<char*>(TRI_Allocate(inLength + 1));

  if (buffer == nullptr) {
    return nullptr;
  }

  *outLength = TRI_UnescapeUtf8StringInPlace(buffer, in, inLength);
  buffer[*outLength] = '\0';

  if (normalize && *outLength > 0) {
    size_t tmpLength = 0;
    char* utf8_nfc = TRI_normalize_utf8_to_NFC(buffer, *outLength, &tmpLength);

    if (utf8_nfc != nullptr) {
      *outLength = tmpLength;
      TRI_Free(buffer);
      buffer = utf8_nfc;
    }
    // intentionally falls through
  }

  return buffer;
}
