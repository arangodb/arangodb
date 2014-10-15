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

#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
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
/// @brief extracts a string
////////////////////////////////////////////////////////////////////////////////

static char* ExtractStringShapedJson (TRI_shaper_t* shaper,
                                      TRI_shaped_json_t const* document,
                                      char const* path) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t shaped;
  char* result;

  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, path);

  if (pid == 0) {
    return nullptr;
  }

  bool ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &shaped, &shape);

  if (! ok || shape == nullptr) {
    return nullptr;
  }

  TRI_json_t* json = TRI_JsonShapedJson(shaper, &shaped);

  if (json == nullptr) {
    return nullptr;
  }

  if (! TRI_IsStringJson(json)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    return nullptr;
  }

  result = TRI_DuplicateString2(json->_value._string.data,
                                json->_value._string.length - 1);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a boolean
////////////////////////////////////////////////////////////////////////////////

static bool ExtractBooleanShapedJson (TRI_shaper_t* shaper,
                                      TRI_shaped_json_t const* document,
                                      char const* path,
                                      bool* found) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t shaped;

  if (found != nullptr) {
    *found = false;
  }

  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, path);

  if (pid == 0) {
    return false;
  }

  bool ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &shaped, &shape);

  if (! ok || shape == nullptr) {
    return false;
  }

  TRI_json_t* json = TRI_JsonShapedJson(shaper, &shaped);

  if (json == nullptr) {
    return false;
  }

  if (json->_type != TRI_JSON_BOOLEAN) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    return false;
  }

  if (found != nullptr) {
    *found = true;
  }

  bool result = json->_value._boolean;

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the auth information
////////////////////////////////////////////////////////////////////////////////

static void FreeAuthInfo (TRI_vocbase_auth_t* auth) {
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_username);
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_password);
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
/// @brief extracts the auth information
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_auth_t* ConvertAuthInfo (TRI_vocbase_t* vocbase,
                                            TRI_document_collection_t* document,
                                            TRI_shaped_json_t const* shapedJson) {
  TRI_shaper_t* shaper = document->getShaper();  // PROTECTED by trx in caller, checked by RUNTIME

  // extract username
  char* user = ExtractStringShapedJson(shaper, shapedJson, "user");

  if (user == nullptr) {
    LOG_DEBUG("cannot extract username");
    return nullptr;
  }

  // extract password
  char* password = ExtractStringShapedJson(shaper, shapedJson, "password");

  if (password == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, user);
    LOG_DEBUG("cannot extract password");
    return nullptr;
  }

  // extract active flag
  bool found;
  bool active = ExtractBooleanShapedJson(shaper, shapedJson, "active", &found);

  if (! found) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, user);
    TRI_FreeString(TRI_CORE_MEM_ZONE, password);
    LOG_DEBUG("cannot extract active flag");
    return nullptr;
  }

  // extract must-change-password flag
  bool mustChange = ExtractBooleanShapedJson(shaper, shapedJson, "changePassword", &found);

  if (! found) {
    mustChange = false;
  }

  TRI_vocbase_auth_t* result = static_cast<TRI_vocbase_auth_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_auth_t), true));

  if (result == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, user);
    TRI_FreeString(TRI_CORE_MEM_ZONE, password);
    LOG_ERROR("couldn't load auth information - out of memory");

    return nullptr;
  }

  result->_username = user;
  result->_password = password;
  result->_active = active;
  result->_mustChange = mustChange;

  return result;
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
  TRI_json_t* json = TRI_CreateList2Json(TRI_UNKNOWN_MEM_ZONE, 1);

  if (json == nullptr) {
    return false;
  }

  TRI_json_t* user = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (user == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return false;
  }

  // username
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                       user,
                       "user",
                       TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "root"));

  // password
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                       user,
                       "password",
                       TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "$1$c776f5f4$ef74bc6fd59ac713bf5929c5ac2f42233e50d4d58748178132ea46dec433bd5b"));

  // active
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                       user,
                       "active",
                       TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));

  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, user);

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


  SingleCollectionReadOnlyTransaction<RestTransactionContext> trx(vocbase, collection->_cid);

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
      TRI_vocbase_auth_t* auth;
      TRI_shaped_json_t shapedJson;

      TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

      TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, d->getDataPtr());  // PROTECTED by trx here

      auth = ConvertAuthInfo(vocbase, document, &shapedJson);

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
  size_t i, n;

  TRI_ASSERT(TRI_IsListJson(json));
  n = json->_value._objects._length;

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
  ClearAuthInfo(vocbase);

  for (i = 0; i < n; ++i) {
    TRI_json_t const* user;
    TRI_json_t const* username;
    TRI_json_t const* password;
    TRI_json_t const* active;

    user = TRI_LookupListJson(json, i);

    if (! TRI_IsArrayJson(user)) {
      continue;
    }

    username = TRI_LookupArrayJson(user, "user");
    password = TRI_LookupArrayJson(user, "password");
    active   = TRI_LookupArrayJson(user, "active");

    if (! TRI_IsStringJson(username) ||
        ! TRI_IsStringJson(password) ||
        ! TRI_IsBooleanJson(active)) {
      continue;
    }

    TRI_vocbase_auth_t* auth = static_cast<TRI_vocbase_auth_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_auth_t), true));

    if (auth == nullptr) {
      continue;
    }

    auth->_username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE,
                                            username->_value._string.data,
                                            username->_value._string.length - 1);

    auth->_password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE,
                                            password->_value._string.data,
                                            password->_value._string.length - 1);

    auth->_active = active->_value._boolean;

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
  bool res;
  char* hex;
  char* sha256;
  size_t hexLen;
  size_t len;
  size_t sha256Len;

  TRI_ASSERT(vocbase != nullptr);

  // look up username
  TRI_ReadLockReadWriteLock(&vocbase->_authInfoLock);
  TRI_vocbase_auth_t* auth = static_cast<TRI_vocbase_auth_t*>(TRI_LookupByKeyAssociativePointer(&vocbase->_authInfo, username));

  if (auth == nullptr || ! auth->_active) {
    TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);
    return false;
  }

  *mustChange = auth->_mustChange;

  // convert password
  res = false;

  // salted password
  if (TRI_IsPrefixString(auth->_password, "$1$")) {
    if (strlen(auth->_password) < 12 || auth->_password[11] != '$') {
      LOG_WARNING("found corrupted password for user '%s'", username);
    }
    else {
      len = 8 + strlen(password);
      char* salted = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, len + 1, false));
      memcpy(salted, auth->_password + 3, 8);
      memcpy(salted + 8, password, len - 8);
      salted[len] = '\0';

      sha256 = TRI_SHA256String(salted, len, &sha256Len);
      TRI_FreeString(TRI_CORE_MEM_ZONE, salted);

      hex = TRI_EncodeHexString(sha256, sha256Len, &hexLen);
      TRI_FreeString(TRI_CORE_MEM_ZONE, sha256);

      LOG_DEBUG("found active user '%s', expecting password '%s', got '%s'",
                username,
                auth->_password + 12,
                hex);

      res = TRI_EqualString(auth->_password + 12, hex);
      TRI_FreeString(TRI_CORE_MEM_ZONE, hex);
    }
  }

  // unsalted password
  else {
    len = strlen(password);
    sha256 = TRI_SHA256String(password, len, &sha256Len);

    hex = TRI_EncodeHexString(sha256, sha256Len, &hexLen);
    TRI_FreeString(TRI_CORE_MEM_ZONE, sha256);

    LOG_DEBUG("found active user '%s', expecting password '%s', got '%s'",
              username,
              auth->_password + 12,
              hex);

    res = TRI_EqualString(auth->_password, hex);
    TRI_FreeString(TRI_CORE_MEM_ZONE, hex);
  }

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
