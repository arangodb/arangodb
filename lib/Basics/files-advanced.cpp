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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include "Basics/operating-system.h"

#ifdef _WIN32
#include <Shlwapi.h>
#include <tchar.h>
#include <windows.h>
#endif

#ifndef _WIN32
#include <sys/statvfs.h>
#endif

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <zlib.h>

#include "files.h"

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/Utf8Helper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "Basics/directories.h"
#include "Basics/error.h"
#include "Basics/hashes.h"
#include "Basics/memory.h"
#include "Basics/threads.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <absl/crc/crc32c.h>

using namespace arangodb::basics;
using namespace arangodb;

TRI_SHA256Functor::TRI_SHA256Functor()
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    : _context(EVP_MD_CTX_new()) {
#else
    : _context(EVP_MD_CTX_create()) {
#endif
  if (_context == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  if (EVP_DigestInit_ex(_context, EVP_sha256(), nullptr) == 0) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    EVP_MD_CTX_free(_context);
#else
    EVP_MD_CTX_destroy(_context);
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to initialize SHA256 processor");
  }
}

TRI_SHA256Functor::~TRI_SHA256Functor() {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  EVP_MD_CTX_free(_context);
#else
  EVP_MD_CTX_destroy(_context);
#endif
}

bool TRI_SHA256Functor::operator()(char const* data, size_t size) noexcept {
  return EVP_DigestUpdate(_context, static_cast<void const*>(data), size) == 1;
}

std::string TRI_SHA256Functor::finalize() {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;
  if (EVP_DigestFinal_ex(_context, hash, &lengthOfHash) == 0) {
    TRI_ASSERT(false);
  }
  return arangodb::basics::StringUtils::encodeHex(
      reinterpret_cast<char const*>(&hash[0]), lengthOfHash);
}

/// @brief read buffer size (used for bulk file reading)
#define READBUFFER_SIZE 8192

////////////////////////////////////////////////////////////////////////////////
/// @brief slurps in a file that is compressed and return uncompressed contents
////////////////////////////////////////////////////////////////////////////////

char* TRI_SlurpGzipFile(char const* filename, size_t* length) {
  TRI_set_errno(TRI_ERROR_NO_ERROR);
  gzFile gzFd = gzopen(filename, "rb");
  auto fdGuard = arangodb::scopeGuard([&gzFd]() noexcept {
    if (nullptr != gzFd) {
      gzclose(gzFd);
    }
  });

  char* retPtr = nullptr;

  if (nullptr != gzFd) {
    TRI_string_buffer_t result;
    TRI_InitStringBuffer(&result, false);

    while (true) {
      auto res = TRI_ReserveStringBuffer(&result, READBUFFER_SIZE);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_AnnihilateStringBuffer(&result);

        TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }

      TRI_read_return_t n =
          gzread(gzFd, (void*)TRI_EndStringBuffer(&result), READBUFFER_SIZE);

      if (n == 0) {
        break;
      }

      if (n < 0) {
        TRI_AnnihilateStringBuffer(&result);

        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        return nullptr;
      }

      TRI_IncreaseLengthStringBuffer(&result, (size_t)n);
    }  // while

    if (length != nullptr) {
      *length = TRI_LengthStringBuffer(&result);
    }

    retPtr = result._buffer;
  }  // if

  return retPtr;
}  // TRI_SlurpGzipFile

#ifdef USE_ENTERPRISE
////////////////////////////////////////////////////////////////////////////////
/// @brief slurps in a file that is encrypted and return unencrypted contents
////////////////////////////////////////////////////////////////////////////////

char* TRI_SlurpDecryptFile(EncryptionFeature& encryptionFeature,
                           char const* filename, char const* keyfile,
                           size_t* length) {
  TRI_set_errno(TRI_ERROR_NO_ERROR);

  encryptionFeature.setKeyFile(keyfile);
  auto keyGuard = arangodb::scopeGuard(
      [&encryptionFeature]() noexcept { encryptionFeature.clearKey(); });

  int fd = TRI_OPEN(filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return nullptr;
  }

  std::unique_ptr<EncryptionFeature::Context> context;
  context = encryptionFeature.beginDecryption(fd);

  if (nullptr == context.get() || !context->status().ok()) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return nullptr;
  }

  TRI_string_buffer_t result;
  TRI_InitStringBuffer(&result, false);

  while (true) {
    auto res = TRI_ReserveStringBuffer(&result, READBUFFER_SIZE);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE(fd);
      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    TRI_read_return_t n = encryptionFeature.readData(
        *context, (void*)TRI_EndStringBuffer(&result), READBUFFER_SIZE);

    if (n == 0) {
      break;
    }

    if (n < 0) {
      TRI_CLOSE(fd);

      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      return nullptr;
    }

    TRI_IncreaseLengthStringBuffer(&result, (size_t)n);
  }

  if (length != nullptr) {
    *length = TRI_LengthStringBuffer(&result);
  }

  TRI_CLOSE(fd);
  return result._buffer;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the crc32 checksum of a file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_Crc32File(char const* path, uint32_t* crc) {
  FILE* fin = TRI_FOPEN(path, "rb");

  if (fin == nullptr) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  char buffer[4096];

  auto res = TRI_ERROR_NO_ERROR;
  *crc = 0;

  while (true) {
    size_t sizeRead = fread(&buffer[0], 1, sizeof(buffer), fin);

    if (sizeRead < sizeof(buffer)) {
      if (feof(fin) == 0) {
        res = TRI_ERROR_FAILED;
        break;
      }
    }

    if (sizeRead > 0) {
      *crc = static_cast<uint32_t>(absl::ExtendCrc32c(
          absl::ToCrc32c(*crc), std::string_view{&buffer[0], sizeRead}));
    } else /* if (sizeRead <= 0) */ {
      break;
    }
  }

  if (0 != fclose(fin)) {
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);
    // otherwise keep original error
  }

  return res;
}
