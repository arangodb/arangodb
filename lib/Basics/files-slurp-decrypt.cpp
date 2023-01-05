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

#include "files-slurp-decrypt.h"

#include <fcntl.h>
#include <sys/stat.h>

#include "Basics/files.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Enterprise/Encryption/EncryptionFeature.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief slurps in a file that is encrypted and return unencrypted contents
////////////////////////////////////////////////////////////////////////////////

/// @brief read buffer size (used for bulk file reading)
#define READBUFFER_SIZE 8192
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
