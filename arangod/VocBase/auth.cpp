////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase authentication and authorization
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "auth.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Indexes/PrimaryIndex.h"
#include "Rest/SslInterface.h"
#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/shape-accessor.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"
#include "Utils/transactions.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a string
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey (TRI_associative_pointer_t* array,
                         void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the auth info
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAuthInfo (TRI_associative_pointer_t* array,
                                     void const* element) {
  VocbaseAuthInfo const* e = static_cast<VocbaseAuthInfo const*>(element);
  return e->hash();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an auth info and a username
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAuthInfo (TRI_associative_pointer_t* array,
                              void const* key,
                              void const* element) {
  char const* k = (char const*) key;
  VocbaseAuthInfo const* e = static_cast<VocbaseAuthInfo const*>(element);
  return e->isEqualName(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the cache entry
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAuthCache (TRI_associative_pointer_t* array,
                                      void const* element) {
  TRI_vocbase_auth_cache_t const* e = static_cast<TRI_vocbase_auth_cache_t const*>(element);

  return TRI_FnvHashString(e->_hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a auth cache entry and a hash
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAuthCache (TRI_associative_pointer_t* array,
                               void const* key,
                               void const* element) {
  char const* k = (char const*) key;
  TRI_vocbase_auth_cache_t const* e = static_cast<TRI_vocbase_auth_cache_t const*>(element);

  return TRI_EqualString(k, e->_hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the cache information
////////////////////////////////////////////////////////////////////////////////

static void FreeAuthCacheInfo (TRI_vocbase_auth_cache_t* cached) {
  if (cached->_hash != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, cached->_hash);
  }

  if (cached->_username != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, cached->_username);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, cached);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs auth information from VelocyPack
////////////////////////////////////////////////////////////////////////////////

static VocbaseAuthInfo* AuthFromVelocyPack (VPackSlice const& slice) {
  if (! slice.isObject()) {
    return nullptr;
  }

  // extract "user" attribute
  VPackSlice const userSlice = slice.get("user");

  if (! userSlice.isString()) {
    LOG_DEBUG("cannot extract username");
    return nullptr;
  }
  VPackSlice const authDataSlice = slice.get("authData");

  if (! authDataSlice.isObject()) {
    LOG_DEBUG("cannot extract authData");
    return nullptr;
  }

  VPackSlice const simpleSlice = authDataSlice.get("simple");

  if (! simpleSlice.isObject()) {
    LOG_DEBUG("cannot extract simple");
    return nullptr;
  }

  VPackSlice const methodSlice = simpleSlice.get("method");
  VPackSlice const saltSlice = simpleSlice.get("salt");
  VPackSlice const hashSlice = simpleSlice.get("hash");

  if (! methodSlice.isString() ||
      ! saltSlice.isString() ||
      ! hashSlice.isString()) {
    LOG_DEBUG("cannot extract password internals");
    return nullptr;
  }

  // extract "active" attribute
  bool active;
  VPackSlice const activeSlice = authDataSlice.get("active");

  if (! activeSlice.isBoolean()) {
    LOG_DEBUG("cannot extract active flag");
    return nullptr;
  }
  active = activeSlice.getBool();

  // extract "changePassword" attribute
  bool mustChange = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "changePassword", false);
  auto result = std::make_unique<VocbaseAuthInfo>(userSlice.copyString(),
                                                  methodSlice.copyString(),
                                                  saltSlice.copyString(),
                                                  hashSlice.copyString(),
                                                  active,
                                                  mustChange); 

  if (result == nullptr) {
    LOG_ERROR("couldn't load auth information - out of memory");

    return nullptr;
  }
  return result.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the auth information
////////////////////////////////////////////////////////////////////////////////

static VocbaseAuthInfo* ConvertAuthInfo (TRI_vocbase_t* vocbase,
                                         TRI_document_collection_t* document,
                                         TRI_doc_mptr_t const* mptr) {
  auto shaper = document->getShaper();  // PROTECTED by trx in caller, checked by RUNTIME

  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, mptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (shapedJson._sid == TRI_SHAPE_ILLEGAL) {
    return nullptr;
  }

  std::unique_ptr<TRI_json_t> json(TRI_JsonShapedJson(shaper, &shapedJson));

  if (json == nullptr) {
    return nullptr;
  }

  std::shared_ptr<VPackBuilder> parsed = triagens::basics::JsonHelper::toVelocyPack(json.get());
  std::unique_ptr<VocbaseAuthInfo> auth(AuthFromVelocyPack(parsed->slice()));
  return auth.release(); // maybe a nullptr
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
///
/// @note the caller must acquire the lock itself
////////////////////////////////////////////////////////////////////////////////

static void ClearAuthInfo (TRI_vocbase_t* vocbase) {
  // clear auth info table
  void** beg = vocbase->_authInfo._table;
  void** end = vocbase->_authInfo._table + vocbase->_authInfo._nrAlloc;
  void** ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      VocbaseAuthInfo* auth = static_cast<VocbaseAuthInfo*>(*ptr);
      delete auth;
      *ptr = nullptr;
    }
  }

  vocbase->_authInfo._nrUsed = 0;

  // clear cache
  beg = vocbase->_authCache._table;
  end = vocbase->_authCache._table + vocbase->_authCache._nrAlloc;
  ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_cache_t* auth = static_cast<TRI_vocbase_auth_cache_t*>(*ptr);

      FreeAuthCacheInfo(auth);
      *ptr = nullptr;
    }
  }

  vocbase->_authCache._nrUsed = 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

uint64_t VocbaseAuthInfo::hash () const {
  return TRI_FnvHashString(_username.c_str());
}

bool VocbaseAuthInfo::isEqualName (char const* compare) const {
  return TRI_EqualString(compare, _username.c_str());
}

bool VocbaseAuthInfo::isEqualPasswordHash (char const* compare) const {
  return TRI_EqualString(_passwordHash.c_str(), compare);
}

char const* VocbaseAuthInfo::username () const {
  return _username.c_str();
}

char const* VocbaseAuthInfo::passwordSalt () const {
  return _passwordSalt.c_str();
}

char const* VocbaseAuthInfo::passwordMethod () const {
  return _passwordMethod.c_str();
}

bool VocbaseAuthInfo::isActive () const {
  return _active;
}

bool VocbaseAuthInfo::mustChange () const {
  return _mustChange;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the authentication info
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_InitAssociativePointer(&vocbase->_authInfo,
                             TRI_CORE_MEM_ZONE,
                             HashKey,
                             HashElementAuthInfo,
                             EqualKeyAuthInfo,
                             nullptr);

  TRI_InitAssociativePointer(&vocbase->_authCache,
                             TRI_CORE_MEM_ZONE,
                             HashKey,
                             HashElementAuthCache,
                             EqualKeyAuthCache,
                             nullptr);

  TRI_InitReadWriteLock(&vocbase->_authInfoLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_ClearAuthInfo(vocbase);

  TRI_DestroyReadWriteLock(&vocbase->_authInfoLock);
  TRI_DestroyAssociativePointer(&vocbase->_authCache);
  TRI_DestroyAssociativePointer(&vocbase->_authInfo);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert initial authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertInitialAuthInfo (TRI_vocbase_t* vocbase) {
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
    char const* hash = "ef74bc6fd59ac713bf5929c5ac2f42233e50d4d58748178132ea46dec433bd5b";
    infoBuilder.add("hash", VPackValue(hash));
    infoBuilder.close(); // simple

    infoBuilder.add("active", VPackValue(true));
    infoBuilder.close(); // authData
    infoBuilder.close(); // The user object
    infoBuilder.close(); // The Array
    return true;
  }
  catch (...) {
    // No action
  }
  // We get here only through error
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo (TRI_vocbase_t* vocbase) {
  LOG_DEBUG("starting to load authentication and authorization information");

  TRI_vocbase_col_t* collection = TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_USERS);

  if (collection == nullptr) {
    LOG_INFO("collection '_users' does not exist, no authentication available");
    return false;
  }


  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(), vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  TRI_document_collection_t* document = trx.documentCollection();

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
  ClearAuthInfo(vocbase);
  auto work = [&](TRI_doc_mptr_t const* ptr) -> void {
    std::unique_ptr<VocbaseAuthInfo> auth(ConvertAuthInfo(vocbase, document, ptr));
    if (auth.get() != nullptr) {
      VocbaseAuthInfo* old = static_cast<VocbaseAuthInfo*>(TRI_InsertKeyAssociativePointer(&vocbase->_authInfo, auth->username(), auth.get(), true));
      auth.release();

      if (old != nullptr) {
        delete old;
      }
    }
  };
  document->primaryIndex()->invokeOnAllElements(work);

  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);

  trx.finish(TRI_ERROR_NO_ERROR);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populate the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_PopulateAuthInfo (TRI_vocbase_t* vocbase,
                           VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
  ClearAuthInfo(vocbase);


  for (VPackSlice const& authSlice: VPackArrayIterator(slice)) {
    std::unique_ptr<VocbaseAuthInfo> auth(AuthFromVelocyPack(authSlice));

    if (auth.get() != nullptr) {
      TRI_InsertKeyAssociativePointer(&vocbase->_authInfo,
                                      auth->username(),
                                      auth.get(),
                                      false);
      auth.release();
    }

  }

  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the authentication info
/// this must be executed after the underlying _users collection is modified
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReloadAuthInfo (TRI_vocbase_t* vocbase) {
  bool result = TRI_LoadAuthInfo(vocbase);

  vocbase->_authInfoLoaded = result;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
  ClearAuthInfo(vocbase);
  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up authentication data in the cache
////////////////////////////////////////////////////////////////////////////////

char* TRI_CheckCacheAuthInfo (TRI_vocbase_t* vocbase,
                              char const* hash,
                              bool* mustChange) {
  char* username = nullptr;

  TRI_ReadLockReadWriteLock(&vocbase->_authInfoLock);
  TRI_vocbase_auth_cache_t* cached = static_cast<TRI_vocbase_auth_cache_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_authCache, hash));

  if (cached != nullptr) {
    username = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, cached->_username);
    *mustChange = cached->_mustChange;
  }

  TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);

  // might be NULL
  return username;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication - note: only checks whether the user exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsAuthenticationAuthInfo (TRI_vocbase_t* vocbase,
                                       char const* username) {
  TRI_ASSERT(vocbase != nullptr);

  // look up username
  TRI_ReadLockReadWriteLock(&vocbase->_authInfoLock);

  // We do not take responsiblity for the data
  VocbaseAuthInfo* auth = static_cast<VocbaseAuthInfo*>(TRI_LookupByKeyAssociativePointer(&vocbase->_authInfo, username));
 
  bool result = (auth != nullptr && auth->isActive());
  TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAuthenticationAuthInfo (TRI_vocbase_t* vocbase,
                                      char const* hash,
                                      char const* username,
                                      char const* password,
                                      bool* mustChange) {
  TRI_ASSERT(vocbase != nullptr);

  // look up username
  TRI_ReadLockReadWriteLock(&vocbase->_authInfoLock);
  // We do not take responsibilty for the data
  VocbaseAuthInfo* auth = static_cast<VocbaseAuthInfo*>(TRI_LookupByKeyAssociativePointer(&vocbase->_authInfo, username));

  if (auth == nullptr || ! auth->isActive()) {
    TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);
    return false;
  }

  *mustChange = auth->mustChange();

  size_t const n = strlen(auth->passwordSalt());
  size_t const p = strlen(password);

  char* salted = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n + p + 1, false));

  if (salted == nullptr) {
    TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);
    return false;
  }

  memcpy(salted, auth->passwordSalt(), n);
  memcpy(salted + n, password, p);
  salted[n + p] = '\0';

  // default value is false
  bool res = false;
  char* crypted = nullptr;
  size_t cryptedLength;

  char const* passwordMethod = auth->passwordMethod();

  TRI_ASSERT(passwordMethod != nullptr);

  try {
    if (strcmp(passwordMethod, "sha1") == 0) {
      triagens::rest::SslInterface::sslSHA1(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(passwordMethod, "sha512") == 0) {
      triagens::rest::SslInterface::sslSHA512(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(passwordMethod, "sha384") == 0) {
      triagens::rest::SslInterface::sslSHA384(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(passwordMethod, "sha256") == 0) {
      triagens::rest::SslInterface::sslSHA256(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(passwordMethod, "sha224") == 0) {
      triagens::rest::SslInterface::sslSHA224(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(passwordMethod, "md5") == 0) {
      triagens::rest::SslInterface::sslMD5(salted, n + p, crypted, cryptedLength);
    }
    else {
      // invalid algorithm...
      res = false;
    }
  }
  catch (...) {
    // SslInterface::ssl....() allocate strings with new, which might throw exceptions
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

  TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);

  if (res && hash != nullptr) {
    // insert item into the cache
    TRI_vocbase_auth_cache_t* cached = static_cast<TRI_vocbase_auth_cache_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vocbase_auth_cache_t), false));

    if (cached != nullptr) {
      cached->_hash       = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, hash);
      cached->_username   = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, username);
      cached->_mustChange = auth->mustChange();

      if (cached->_hash == nullptr || cached->_username == nullptr) {
        FreeAuthCacheInfo(cached);
        return res;
      }

      TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
      void* old = TRI_InsertKeyAssociativePointer(&vocbase->_authCache, cached->_hash, cached, false);

      // duplicate entry?
      if (old != nullptr) {
        FreeAuthCacheInfo(cached);
      }

      TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);
    }
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
