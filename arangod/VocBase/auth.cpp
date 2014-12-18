////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase authentication and authorisation
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

#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/JsonHelper.h"
#include "Rest/SslInterface.h"
#include "ShapedJson/shape-accessor.h"
#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"
#include "Utils/transactions.h"

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
  TRI_vocbase_auth_t const* e = static_cast<TRI_vocbase_auth_t const*>(element);

  return TRI_FnvHashString(e->_username);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an auth info and a username
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAuthInfo (TRI_associative_pointer_t* array,
                              void const* key,
                              void const* element) {
  char const* k = (char const*) key;
  TRI_vocbase_auth_t const* e = static_cast<TRI_vocbase_auth_t const*>(element);

  return TRI_EqualString(k, e->_username);
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
/// @brief frees the auth information
////////////////////////////////////////////////////////////////////////////////

static void FreeAuthInfo (TRI_vocbase_auth_t* auth) {
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_username);
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_passwordMethod);
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_passwordSalt);
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_passwordHash);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, auth);
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
/// @brief constructs auth information from JSON
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_auth_t* AuthFromJson (TRI_json_t const* json) {
  if (! TRI_IsObjectJson(json)) {
    return nullptr;
  }

  // extract "user" attribute
  TRI_json_t const* userJson = TRI_LookupObjectJson(json, "user");

  if (! TRI_IsStringJson(userJson)) {
    LOG_DEBUG("cannot extract username");
    return nullptr;
  }

  TRI_json_t const* authDataJson = TRI_LookupObjectJson(json, "authData");

  if (! TRI_IsObjectJson(authDataJson)) {
    LOG_DEBUG("cannot extract authData");
    return nullptr;
  }

  TRI_json_t const* simpleJson = TRI_LookupObjectJson(authDataJson, "simple");

  if (! TRI_IsObjectJson(simpleJson)) {
    LOG_DEBUG("cannot extract simple");
    return nullptr;
  }

  TRI_json_t const* methodJson = TRI_LookupObjectJson(simpleJson, "method");
  TRI_json_t const* saltJson   = TRI_LookupObjectJson(simpleJson, "salt");
  TRI_json_t const* hashJson   = TRI_LookupObjectJson(simpleJson, "hash");

  if (! TRI_IsStringJson(methodJson) ||
      ! TRI_IsStringJson(saltJson) ||
      ! TRI_IsStringJson(hashJson)) {
    LOG_DEBUG("cannot extract password internals");
    return nullptr;
  }

  // extract "active" attribute
  bool active;
  TRI_json_t const* activeJson = TRI_LookupObjectJson(authDataJson, "active");

  if (! TRI_IsBooleanJson(activeJson)) {
    LOG_DEBUG("cannot extract active flag");
    return nullptr;
  }
  active = activeJson->_value._boolean;

  // extract "changePassword" attribute
  bool mustChange;
  TRI_json_t const* mustChangeJson = TRI_LookupObjectJson(json, "changePassword");

  if (TRI_IsBooleanJson(mustChangeJson)) {
    mustChange = mustChangeJson->_value._boolean;
  }
  else {
    // default value
    mustChange = false;
  }


  TRI_vocbase_auth_t* result = static_cast<TRI_vocbase_auth_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_auth_t), true));

  if (result == nullptr) {
    LOG_ERROR("couldn't load auth information - out of memory");

    return nullptr;
  }

  result->_username        = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, userJson->_value._string.data, userJson->_value._string.length - 1);
  result->_passwordMethod  = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, methodJson->_value._string.data, methodJson->_value._string.length - 1);
  result->_passwordSalt    = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, saltJson->_value._string.data, saltJson->_value._string.length - 1);
  result->_passwordHash    = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, hashJson->_value._string.data, hashJson->_value._string.length - 1);
  result->_active          = active;
  result->_mustChange      = mustChange;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the auth information
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_auth_t* ConvertAuthInfo (TRI_vocbase_t* vocbase,
                                            TRI_document_collection_t* document,
                                            TRI_doc_mptr_t const* mptr) {
  TRI_shaper_t* shaper = document->getShaper();  // PROTECTED by trx in caller, checked by RUNTIME

  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, mptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (shapedJson._sid == TRI_SHAPE_ILLEGAL) {
    return nullptr;
  }

  TRI_json_t* json = TRI_JsonShapedJson(shaper, &shapedJson);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_vocbase_auth_t* auth = AuthFromJson(json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  return auth; // maybe a nullptr
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
      TRI_vocbase_auth_t* auth = static_cast<TRI_vocbase_auth_t*>(*ptr);

      FreeAuthInfo(auth);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the authentication info
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
  TRI_json_t* json = TRI_CreateArray2Json(TRI_UNKNOWN_MEM_ZONE, 1);

  if (json == nullptr) {
    return false;
  }

  TRI_json_t* user = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (user == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return false;
  }

  // username
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE,
                       user,
                       "user",
                       TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "root"));

  TRI_json_t* authData = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (authData != nullptr) {
    // simple
    TRI_json_t* simple = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

    if (simple != nullptr) {
      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE,
                           simple,
                           "method",
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "sha256"));

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE,
                           simple,
                           "salt",
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "c776f5f4"));

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE,
                           simple,
                           "hash",
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "ef74bc6fd59ac713bf5929c5ac2f42233e50d4d58748178132ea46dec433bd5b"));

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, authData, "simple", simple);
    }

    // active
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE,
                         authData,
                         "active",
                         TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));

    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, user, "authData", authData);
  }

  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, user);

  TRI_PopulateAuthInfo(vocbase, json);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo (TRI_vocbase_t* vocbase) {
  LOG_DEBUG("starting to load authentication and authorisation information");

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

  void** beg = document->_primaryIndex._table;
  void** end = beg + document->_primaryIndex._nrAlloc;
  void** ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_t* auth = ConvertAuthInfo(vocbase, document, (TRI_doc_mptr_t const*) *ptr);

      if (auth != nullptr) {
        TRI_vocbase_auth_t* old = static_cast<TRI_vocbase_auth_t*>(TRI_InsertKeyAssociativePointer(&vocbase->_authInfo, auth->_username, auth, true));

        if (old != nullptr) {
          FreeAuthInfo(old);
        }
      }
    }
  }

  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);

  trx.finish(TRI_ERROR_NO_ERROR);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populate the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_PopulateAuthInfo (TRI_vocbase_t* vocbase,
                           TRI_json_t const* json) {
  TRI_ASSERT(TRI_IsArrayJson(json));

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
  ClearAuthInfo(vocbase);

  size_t const n = json->_value._objects._length;
  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_auth_t* auth = AuthFromJson(TRI_LookupArrayJson(json, i));

    if (auth == nullptr) {
      continue;
    }

    TRI_InsertKeyAssociativePointer(&vocbase->_authInfo,
                                    auth->_username,
                                    auth,
                                    false);
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
  TRI_vocbase_auth_t* auth = static_cast<TRI_vocbase_auth_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_authInfo, username));

  if (auth == nullptr || ! auth->_active) {
    TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);
    return false;
  }

  *mustChange = auth->_mustChange;

  size_t const n = strlen(auth->_passwordSalt);
  size_t const p = strlen(password);

  char* salted = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n + p + 1, false));

  if (salted == nullptr) {
    TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);
    return false;
  }

  memcpy(salted, auth->_passwordSalt, n);
  memcpy(salted + n, password, p);
  salted[n + p] = '\0';

  // default value is false
  bool res = false;
  char* crypted = nullptr;
  size_t cryptedLength;

  TRI_ASSERT(auth->_passwordMethod != nullptr);

  try {
    if (strcmp(auth->_passwordMethod, "sha1") == 0) {
      triagens::rest::SslInterface::sslSHA1(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(auth->_passwordMethod, "sha512") == 0) {
      triagens::rest::SslInterface::sslSHA512(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(auth->_passwordMethod, "sha384") == 0) {
      triagens::rest::SslInterface::sslSHA384(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(auth->_passwordMethod, "sha256") == 0) {
      triagens::rest::SslInterface::sslSHA256(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(auth->_passwordMethod, "sha224") == 0) {
      triagens::rest::SslInterface::sslSHA224(salted, n + p, crypted, cryptedLength);
    }
    else if (strcmp(auth->_passwordMethod, "md5") == 0) {
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
      res = TRI_EqualString(auth->_passwordHash, hex);
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
      cached->_mustChange = auth->_mustChange;

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
