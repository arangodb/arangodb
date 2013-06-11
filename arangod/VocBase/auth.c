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
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default auth info
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_t* DefaultAuthInfo = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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

  if (json->_type != TRI_JSON_STRING) {
    return NULL;
  }

  result = TRI_DuplicateString2(json->_value._string.data, json->_value._string.length);

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
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_vocbase_col_t* collection;
  TRI_primary_collection_t* primary;
  void** beg;
  void** end;
  void** ptr;

  LOG_DEBUG("starting to load authentication and authorisation information");

  collection = TRI_LookupCollectionByNameVocBase(vocbase, "_users");

  if (collection == NULL) {
    LOG_INFO("collection '_users' does not exist, no authentication available");
    return false;
  }

  TRI_UseCollectionVocBase(vocbase, collection);

  primary = collection->_collection;

  if (primary == NULL) {
    LOG_FATAL_AND_EXIT("collection '_users' cannot be loaded");
  }

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._info._type)) {
    TRI_ReleaseCollectionVocBase(vocbase, collection);
    LOG_FATAL_AND_EXIT("collection '_users' has an unknown collection type");
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

  vocbase->_authInfoFlush = true;
  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);

  TRI_ReleaseCollectionVocBase(vocbase, collection);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the default authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DefaultAuthInfo (TRI_vocbase_t* vocbase) {
  DefaultAuthInfo = vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the authentication info
/// this must be executed after the underlying _users collection is modified
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReloadAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_DestroyAuthInfo(vocbase);

  return TRI_LoadAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the default authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAuthInfo (TRI_vocbase_t* vocbase) {
  void** beg;
  void** end;
  void** ptr;

  TRI_WriteLockReadWriteLock(&vocbase->_authInfoLock);

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

  vocbase->_authInfo._nrUsed   = 0;

  TRI_WriteUnlockReadWriteLock(&vocbase->_authInfoLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether some externally cached authentication info 
/// should be flushed, by querying the internal flush flag
/// checking the information may also change the state of the flag
////////////////////////////////////////////////////////////////////////////////

bool TRI_FlushAuthenticationAuthInfo () {
  bool res;

  TRI_ReadLockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
  res = DefaultAuthInfo->_authInfoFlush;
  TRI_ReadUnlockReadWriteLock(&DefaultAuthInfo->_authInfoLock);

  if (res) {
    TRI_WriteLockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
    DefaultAuthInfo->_authInfoFlush = false;
    TRI_WriteUnlockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAuthenticationAuthInfo (char const* username,
                                      char const* password) {
  TRI_vocbase_auth_t* auth;
  bool res;
  char* hex;
  char* sha256;
  size_t hexLen;
  size_t len;
  size_t sha256Len;

  assert(DefaultAuthInfo);

  // look up username
  TRI_ReadLockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
  auth = TRI_LookupByKeyAssociativePointer(&DefaultAuthInfo->_authInfo, username);

  if (auth == NULL || ! auth->_active) {
    TRI_ReadUnlockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
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

  TRI_ReadUnlockReadWriteLock(&DefaultAuthInfo->_authInfoLock);

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
