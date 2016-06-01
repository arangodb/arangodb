////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "auth.h"

#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Ssl/SslInterface.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAuthenticationAuthInfo(TRI_vocbase_t* vocbase, char const* hash,
                                     char const* username, char const* password,
                                     bool* mustChange) {
  TRI_ASSERT(vocbase != nullptr);
  bool res = false;
  VocbaseAuthInfo* auth = nullptr;

  {
    // look up username
    READ_LOCKER(readLocker, vocbase->_authInfoLock);

    auto it = vocbase->_authInfo.find(username);
    if (it == vocbase->_authInfo.end()) {
      return false;
    }

    // We do not take responsiblity for the data
    auth = it->second;

    if (auth == nullptr || !auth->isActive()) {
      return false;
    }

    *mustChange = auth->mustChange();

    size_t const n = strlen(auth->passwordSalt());
    size_t const p = strlen(password);

    char* salted = static_cast<char*>(
        TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n + p + 1, false));

    if (salted == nullptr) {
      return false;
    }

    memcpy(salted, auth->passwordSalt(), n);
    memcpy(salted + n, password, p);
    salted[n + p] = '\0';

    // default value is false
    char* crypted = nullptr;
    size_t cryptedLength;

    char const* passwordMethod = auth->passwordMethod();

    TRI_ASSERT(passwordMethod != nullptr);

    try {
      if (strcmp(passwordMethod, "sha1") == 0) {
        arangodb::rest::SslInterface::sslSHA1(salted, n + p, crypted,
                                              cryptedLength);
      } else if (strcmp(passwordMethod, "sha512") == 0) {
        arangodb::rest::SslInterface::sslSHA512(salted, n + p, crypted,
                                                cryptedLength);
      } else if (strcmp(passwordMethod, "sha384") == 0) {
        arangodb::rest::SslInterface::sslSHA384(salted, n + p, crypted,
                                                cryptedLength);
      } else if (strcmp(passwordMethod, "sha256") == 0) {
        arangodb::rest::SslInterface::sslSHA256(salted, n + p, crypted,
                                                cryptedLength);
      } else if (strcmp(passwordMethod, "sha224") == 0) {
        arangodb::rest::SslInterface::sslSHA224(salted, n + p, crypted,
                                                cryptedLength);
      } else if (strcmp(passwordMethod, "md5") == 0) {
        arangodb::rest::SslInterface::sslMD5(salted, n + p, crypted,
                                             cryptedLength);
      } else {
        // invalid algorithm...
        res = false;
      }
    } catch (...) {
      // SslInterface::ssl....() allocate strings with new, which might throw
      // exceptions
      // if we get one, we can ignore it because res is set to false anyway
    }

    if (crypted != nullptr) {
      TRI_ASSERT(cryptedLength > 0);

      size_t hexLen;
      char* hex = TRI_EncodeHexString(crypted, cryptedLength, &hexLen);

      if (hex != nullptr) {
        res = auth->isEqualPasswordHash(hex);
        TRI_FreeString(TRI_CORE_MEM_ZONE, hex);
      }

      delete[] crypted;
    }

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, salted);
  }

  if (res && hash != nullptr) {
    // insert item into the cache
    auto cached = std::make_unique<VocbaseAuthCache>();

    cached->_hash = std::string(hash);
    cached->_username = std::string(username);
    cached->_mustChange = auth->mustChange();

    if (cached->_hash.empty() || cached->_username.empty()) {
      return res;
    }

    WRITE_LOCKER(writeLocker, vocbase->_authInfoLock);

    auto it = vocbase->_authCache.find(cached->_hash);

    if (it != vocbase->_authCache.end()) {
      delete (*it).second;
      (*it).second = nullptr;
    }

    vocbase->_authCache[cached->_hash] = cached.get();
    cached.release();
  }

  return res;
}
