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

#include "SHA256Functor.h"

#include "Exceptions.h"

#include <openssl/evp.h>

TRI_SHA256Functor::TRI_SHA256Functor()
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    : _context(EVP_MD_CTX_new()) {
#else
    : _context(EVP_MD_CTX_create()) {
#endif
  auto* context = static_cast<EVP_MD_CTX*>(_context);
  if (context == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 0) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    EVP_MD_CTX_free(context);
#else
    EVP_MD_CTX_destroy(_context);
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to initialize SHA256 processor");
  }
}

TRI_SHA256Functor::~TRI_SHA256Functor() {
  auto* context = static_cast<EVP_MD_CTX*>(_context);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  EVP_MD_CTX_free(context);
#else
  EVP_MD_CTX_destroy(context);
#endif
}

bool TRI_SHA256Functor::operator()(char const* data, size_t size) noexcept {
  auto* context = static_cast<EVP_MD_CTX*>(_context);
  return EVP_DigestUpdate(context, static_cast<void const*>(data), size) == 1;
}

std::string TRI_SHA256Functor::finalize() {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;
  auto* context = static_cast<EVP_MD_CTX*>(_context);
  if (EVP_DigestFinal_ex(context, hash, &lengthOfHash) == 0) {
    TRI_ASSERT(false);
  }
  return arangodb::basics::StringUtils::encodeHex(
      reinterpret_cast<char const*>(&hash[0]), lengthOfHash);
}
