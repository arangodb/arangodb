////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase authentication and authorisation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "auth.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "ShapedJson/shape-accessor.h"
#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  TRI_vocbase_auth_t const* e = element;

  return TRI_FnvHashString(e->_username);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an auth info and a username
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAuthInfo (TRI_associative_pointer_t* array, 
                              void const* key, 
                              void const* element) {
  char const* k = (char const*) key;
  TRI_vocbase_auth_t const* e = element;

  return TRI_EqualString(k, e->_username);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the cache entry
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAuthCache (TRI_associative_pointer_t* array, 
                                      void const* element) {
  TRI_vocbase_auth_cache_t const* e = element;

  return TRI_FnvHashString(e->_hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a auth cache entry and a hash
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAuthCache (TRI_associative_pointer_t* array, 
                               void const* key, 
                               void const* element) {
  char const* k = (char const*) key;
  TRI_vocbase_auth_cache_t const* e = element;

  return TRI_EqualString(k, e->_hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a string
////////////////////////////////////////////////////////////////////////////////

static char* ExtractStringShapedJson (TRI_shaper_t* shaper,
                                      TRI_shaped_json_t const* document,
                                      char const* path) {
  TRI_json_t* json;
  TRI_shape_pid_t pid;
  TRI_shape_t const* shape;
  TRI_shaped_json_t shaped;
  bool ok;
  char* result;

  pid = shaper->findAttributePathByName(shaper, path);

  ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &shaped, &shape);

  if (! ok || shape == NULL) {
    return NULL;
  }

  json = TRI_JsonShapedJson(shaper, &shaped);

  if (json == NULL) {
    return NULL;
  }

  if (! TRI_IsStringJson(json)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    return NULL;
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
  TRI_json_t* json;
  TRI_shape_pid_t pid;
  TRI_shape_t const* shape;
  TRI_shaped_json_t shaped;
  bool result;
  bool ok;

  if (found != NULL) {
    *found = false;
  }

  pid = shaper->findAttributePathByName(shaper, path);
  ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &shaped, &shape);

  if (! ok || shape == NULL) {
    return false;
  }

  json = TRI_JsonShapedJson(shaper, &shaped);

  if (json == NULL) {
    return false;
  }

  if (json->_type != TRI_JSON_BOOLEAN) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    return false;
  }

  if (found != NULL) {
    *found = true;
  }

  result = json->_value._boolean;

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
  if (cached->_hash != NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, cached->_hash);
  }

  if (cached->_username == NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, cached->_username);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, cached);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the auth information
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_auth_t* ConvertAuthInfo (TRI_vocbase_t* vocbase,
                                            TRI_primary_collection_t* primary,
                                            TRI_shaped_json_t const* document) {
  TRI_shaper_t* shaper;
  char* user;
  char* password;
  bool active;
  bool found;
  TRI_vocbase_auth_t* result;

  shaper = primary->_shaper;

  // extract username
  user = ExtractStringShapedJson(shaper, document, "user");

  if (user == NULL) {
    LOG_DEBUG("cannot extract username");
    return NULL;
  }

  // extract password
  password = ExtractStringShapedJson(shaper, document, "password");

  if (password == NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, user);
    LOG_DEBUG("cannot extract password");
    return NULL;
  }

  // extract active flag
  active = ExtractBooleanShapedJson(shaper, document, "active", &found);

  if (! found) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, user);
    TRI_FreeString(TRI_CORE_MEM_ZONE, password);
    LOG_DEBUG("cannot extract active flag");
    return NULL;
  }

  result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_auth_t), true);

  if (result == NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, user);
    TRI_FreeString(TRI_CORE_MEM_ZONE, password);
    LOG_ERROR("couldn't load auth information - out of memory");

    return NULL;
  }

  result->_username = user;
  result->_password = password;
  result->_active = active;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the authentication info
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_InitAssociativePointer(&vocbase->_authInfo,
                             TRI_CORE_MEM_ZONE,
                             HashKey,
                             HashElementAuthInfo,
                             EqualKeyAuthInfo,
                             NULL);
  
  TRI_InitAssociativePointer(&vocbase->_authCache,
                             TRI_CORE_MEM_ZONE,
                             HashKey,
                             HashElementAuthCache,
                             EqualKeyAuthCache,
                             NULL);
  
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
  TRI_vocbase_auth_t* auth;
  TRI_vocbase_auth_cache_t* cached;
  
  auth = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_auth_t), true);

  if (auth == NULL) {
    return false;
  }
    
  cached = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vocbase_auth_cache_t), false);

  if (cached == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, auth);
    return false;
  }

  auth->_username = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "root");
  auth->_password = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "");
  auth->_active = true;
     
  cached->_hash     = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "cm9vdDo="); // base64Encode("root:")
  cached->_username = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "root");

  // insert the authentication info inside a lock
  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);

  TRI_InsertKeyAssociativePointer(&vocbase->_authInfo, auth->_username, auth, false);
  TRI_InsertKeyAssociativePointer(&vocbase->_authCache, cached->_hash, cached, false);
    
  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_vocbase_col_t* collection;
  TRI_primary_collection_t* primary;
  void** beg;
  void** end;
  void** ptr;

  LOG_DEBUG("starting to load authentication and authorisation information");

  collection = TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_USERS);

  if (collection == NULL) {
    LOG_INFO("collection '_users' does not exist, no authentication available");
    return false;
  }

  TRI_UseCollectionVocBase(vocbase, collection);

  primary = collection->_collection;

  if (primary == NULL) {
    LOG_FATAL_AND_EXIT("collection '_users' cannot be loaded");
  }

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->_collection->beginWrite(collection->_collection);

  beg = primary->_primaryIndex._table;
  end = beg + primary->_primaryIndex._nrAlloc;
  ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_t* auth;
      TRI_doc_mptr_t const* d;
      TRI_shaped_json_t shapedJson;

      d = (TRI_doc_mptr_t const*) *ptr;

      TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, d->_data);

      auth = ConvertAuthInfo(vocbase, primary, &shapedJson);

      if (auth != NULL) {
        TRI_vocbase_auth_t* old;

        old = TRI_InsertKeyAssociativePointer(&vocbase->_authInfo, auth->_username, auth, true);

        if (old != NULL) {
          FreeAuthInfo(old);
        }
      }
    }
  }

  collection->_collection->endWrite(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);

  TRI_ReleaseCollectionVocBase(vocbase, collection);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the authentication info
/// this must be executed after the underlying _users collection is modified
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReloadAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_ClearAuthInfo(vocbase);

  return TRI_LoadAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearAuthInfo (TRI_vocbase_t* vocbase) {
  void** beg;
  void** end;
  void** ptr;

  if (vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR) {
    // do not flush coordinator databases
    return;
  }

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);

  // clear auth info table
  beg = vocbase->_authInfo._table;
  end = vocbase->_authInfo._table + vocbase->_authInfo._nrAlloc;
  ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_t* auth = *ptr;

      FreeAuthInfo(auth);
      *ptr = NULL;
    }
  }

  vocbase->_authInfo._nrUsed = 0;
  
  // clear cache
  beg = vocbase->_authCache._table;
  end = vocbase->_authCache._table + vocbase->_authCache._nrAlloc;
  ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_cache_t* auth = *ptr;

      FreeAuthCacheInfo(auth);
      *ptr = NULL;
    }
  }

  vocbase->_authCache._nrUsed = 0;

  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up authentication data in the cache
////////////////////////////////////////////////////////////////////////////////

char* TRI_CheckCacheAuthInfo (TRI_vocbase_t* vocbase,
                              char const* hash) {
  TRI_vocbase_auth_cache_t* cached;
  char* username;

  username = NULL;

  TRI_ReadLockReadWriteLock(&vocbase->_authInfoLock);
  cached = TRI_LookupByKeyAssociativePointer(&vocbase->_authCache, hash);

  if (cached != NULL) {
    username = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, cached->_username);
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
                                      char const* password) {
  TRI_vocbase_auth_t* auth;
  bool res;
  char* hex;
  char* sha256;
  size_t hexLen;
  size_t len;
  size_t sha256Len;

  assert(vocbase != NULL);

  // look up username
  TRI_ReadLockReadWriteLock(&vocbase->_authInfoLock);
  auth = TRI_LookupByKeyAssociativePointer(&vocbase->_authInfo, username);

  if (auth == NULL || ! auth->_active) {
    TRI_ReadUnlockReadWriteLock(&vocbase->_authInfoLock);
    return false;
  }

  // convert password
  res = false;

  if (TRI_IsPrefixString(auth->_password, "$1$")) {
    if (strlen(auth->_password) < 12 || auth->_password[11] != '$') {
      LOG_WARNING("found corrupted password for user '%s'", username);
    }
    else {
      char* salted;

      len = 8 + strlen(password);
      salted = TRI_Allocate(TRI_CORE_MEM_ZONE, len + 1, false);
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

  if (res && hash != NULL) {
    // insert item into the cache
    TRI_vocbase_auth_cache_t* cached;

    cached = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vocbase_auth_cache_t), false);

    if (cached != NULL) {
      void* old;

      cached->_hash     = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, hash);
      cached->_username = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, username);

      if (cached->_hash == NULL || cached->_username == NULL) {
        FreeAuthCacheInfo(cached);
        return res;
      }

      TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);
      old = TRI_InsertKeyAssociativePointer(&vocbase->_authCache, cached->_hash, cached, false);

      // duplicate entry?
      if (old != NULL) {
        FreeAuthCacheInfo(cached);
      }

      TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
