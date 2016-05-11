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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs auth information from VelocyPack
////////////////////////////////////////////////////////////////////////////////

static VocbaseAuthInfo* AuthFromVelocyPack(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return nullptr;
  }

  // extract "user" attribute
  VPackSlice const userSlice = slice.get("user");

  if (!userSlice.isString()) {
    LOG(DEBUG) << "cannot extract username";
    return nullptr;
  }
  VPackSlice const authDataSlice = slice.get("authData");

  if (!authDataSlice.isObject()) {
    LOG(DEBUG) << "cannot extract authData";
    return nullptr;
  }

  VPackSlice const simpleSlice = authDataSlice.get("simple");

  if (!simpleSlice.isObject()) {
    LOG(DEBUG) << "cannot extract simple";
    return nullptr;
  }

  VPackSlice const methodSlice = simpleSlice.get("method");
  VPackSlice const saltSlice = simpleSlice.get("salt");
  VPackSlice const hashSlice = simpleSlice.get("hash");

  if (!methodSlice.isString() || !saltSlice.isString() ||
      !hashSlice.isString()) {
    LOG(DEBUG) << "cannot extract password internals";
    return nullptr;
  }

  // extract "active" attribute
  bool active;
  VPackSlice const activeSlice = authDataSlice.get("active");

  if (!activeSlice.isBoolean()) {
    LOG(DEBUG) << "cannot extract active flag";
    return nullptr;
  }
  active = activeSlice.getBool();

  // extract "changePassword" attribute
  bool mustChange = arangodb::basics::VelocyPackHelper::getBooleanValue(
      slice, "changePassword", false);
  auto result = std::make_unique<VocbaseAuthInfo>(
      userSlice.copyString(), methodSlice.copyString(), saltSlice.copyString(),
      hashSlice.copyString(), active, mustChange);

  if (result == nullptr) {
    LOG(ERR) << "couldn't load auth information - out of memory";

    return nullptr;
  }
  return result.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the auth information
////////////////////////////////////////////////////////////////////////////////

static VocbaseAuthInfo* ConvertAuthInfo(TRI_vocbase_t* vocbase,
                                        TRI_doc_mptr_t const* mptr) {

  VPackSlice slice(mptr->vpack());
  if (slice.isNone()) {
    return nullptr;
  }
  std::unique_ptr<VocbaseAuthInfo> auth(AuthFromVelocyPack(slice));
  return auth.release();  // maybe a nullptr
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
///
/// @note the caller must acquire the lock itself
////////////////////////////////////////////////////////////////////////////////

static void ClearAuthInfo(TRI_vocbase_t* vocbase) {
  // clear auth info table
  for (auto& it : vocbase->_authInfo) {
    delete it.second;
  }
  vocbase->_authInfo.clear();

  // clear cache
  for (auto& it : vocbase->_authCache) {
    delete it.second;
  }
  vocbase->_authCache.clear();
}

uint64_t VocbaseAuthInfo::hash() const {
  return TRI_FnvHashString(_username.c_str());
}

bool VocbaseAuthInfo::isEqualPasswordHash(char const* compare) const {
  return TRI_EqualString(_passwordHash.c_str(), compare);
}

char const* VocbaseAuthInfo::username() const { return _username.c_str(); }

char const* VocbaseAuthInfo::passwordSalt() const {
  return _passwordSalt.c_str();
}

char const* VocbaseAuthInfo::passwordMethod() const {
  return _passwordMethod.c_str();
}

bool VocbaseAuthInfo::isActive() const { return _active; }

bool VocbaseAuthInfo::mustChange() const { return _mustChange; }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAuthInfo(TRI_vocbase_t* vocbase) {
  TRI_ClearAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert initial authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertInitialAuthInfo(TRI_vocbase_t* vocbase) {
  try {
    VPackBuilder infoBuilder;
    infoBuilder.openArray();
    // The only users object
    infoBuilder.add(VPackValue(VPackValueType::Object));
    // username
    infoBuilder.add("user", VPackValue("root"));
    infoBuilder.add("authData", VPackValue(VPackValueType::Object));

    // Simple auth
    infoBuilder.add("simple", VPackValue(VPackValueType::Object));
    infoBuilder.add("method", VPackValue("sha256"));
    char const* salt = "c776f5f4";
    infoBuilder.add("salt", VPackValue(salt));
    char const* hash =
        "ef74bc6fd59ac713bf5929c5ac2f42233e50d4d58748178132ea46dec433bd5b";
    infoBuilder.add("hash", VPackValue(hash));
    infoBuilder.close();  // simple

    infoBuilder.add("active", VPackValue(true));
    infoBuilder.close();  // authData
    infoBuilder.close();  // The user object
    infoBuilder.close();  // The Array
    return true;
  } catch (...) {
    // No action
  }
  // We get here only through error
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo(TRI_vocbase_t* vocbase) {
  LOG(DEBUG) << "starting to load authentication and authorization information";
  
  WRITE_LOCKER(writeLocker, vocbase->_authInfoLock);

  TRI_vocbase_col_t* collection =
      TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_USERS);

  if (collection == nullptr) {
    LOG(INFO) << "collection '_users' does not exist, no authentication available";
    return false;
  }

  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                  collection->_cid, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  ClearAuthInfo(vocbase);

  auto work = [&](TRI_doc_mptr_t const* ptr) {
    std::unique_ptr<VocbaseAuthInfo> auth(ConvertAuthInfo(vocbase, ptr));

    if (auth != nullptr) {
      auto it = vocbase->_authInfo.find(auth->username());

      if (it != vocbase->_authInfo.end()) {
        delete (*it).second;
        (*it).second = nullptr;
      }

      vocbase->_authInfo[auth->username()] = auth.get();
      auth.release();
    }
    return true;
  };

  trx.invokeOnAllElements(collection->_name, work);

  trx.finish(TRI_ERROR_NO_ERROR);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populate the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_PopulateAuthInfo(TRI_vocbase_t* vocbase, VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  WRITE_LOCKER(writeLocker, vocbase->_authInfoLock);

  ClearAuthInfo(vocbase);

  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    std::unique_ptr<VocbaseAuthInfo> auth(AuthFromVelocyPack(authSlice));

    if (auth != nullptr) {
      vocbase->_authInfo.emplace(auth->username(), auth.get());
      auth.release();
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the authentication info
/// this must be executed after the underlying _users collection is modified
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReloadAuthInfo(TRI_vocbase_t* vocbase) {
  bool result = TRI_LoadAuthInfo(vocbase);

  vocbase->_authInfoLoaded = result;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearAuthInfo(TRI_vocbase_t* vocbase) {
  WRITE_LOCKER(writeLocker, vocbase->_authInfoLock);
  ClearAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up authentication data in the cache
////////////////////////////////////////////////////////////////////////////////

std::string TRI_CheckCacheAuthInfo(TRI_vocbase_t* vocbase, char const* hash,
                                   bool* mustChange) {
  READ_LOCKER(readLocker, vocbase->_authInfoLock);

  auto it = vocbase->_authCache.find(hash);
  if (it != vocbase->_authCache.end()) {
    VocbaseAuthCache* cached = it->second;

    if (cached != nullptr) {
      *mustChange = cached->_mustChange;
      return cached->_username;
    }

    // Sorry not found
  }
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication - note: only checks whether the user exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsAuthenticationAuthInfo(TRI_vocbase_t* vocbase,
                                      char const* username) {
  TRI_ASSERT(vocbase != nullptr);

  // look up username
  READ_LOCKER(readLocker, vocbase->_authInfoLock);

  auto it = vocbase->_authInfo.find(username);

  if (it == vocbase->_authInfo.end()) {
    return false;
  }

  // We do not take responsiblity for the data
  VocbaseAuthInfo* auth = it->second;
  return (auth != nullptr && auth->isActive());
}

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
