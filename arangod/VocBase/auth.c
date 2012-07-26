////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase authentication and authorisation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "auth.h"

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "ShapedJson/shape-accessor.h"
#include "VocBase/simple-collection.h"
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

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

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

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the auth information
////////////////////////////////////////////////////////////////////////////////

static void FreeAuthInfo (TRI_vocbase_auth_t* auth) {
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_username);
  TRI_Free(TRI_CORE_MEM_ZONE, auth->_password);
  TRI_Free(TRI_CORE_MEM_ZONE, auth);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the auth information
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_auth_t* ConvertAuthInfo (TRI_vocbase_t* vocbase,
                                            TRI_doc_collection_t* doc,
                                            TRI_shaped_json_t const* document) {
  TRI_shaper_t* shaper;
  char* user;
  char* password;
  bool active;
  bool found;
  TRI_vocbase_auth_t* result;

  shaper = doc->_shaper;

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

  result = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vocbase_auth_t), true);

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

void TRI_LoadAuthInfo (TRI_vocbase_t* vocbase) {
  TRI_vocbase_col_t* collection;
  TRI_doc_collection_t* doc;
  TRI_sim_collection_t* sim;
  void** beg;
  void** end;
  void** ptr;

  LOG_DEBUG("starting to load authentication and authorisation information");

  collection = TRI_LookupCollectionByNameVocBase(vocbase, "_users");

  if (collection == NULL) {
    LOG_INFO("collection '_users' does not exist, no authentication available");
    return;
  }

  TRI_UseCollectionVocBase(vocbase, collection);

  doc = collection->_collection;

  if (doc == NULL) {
    LOG_FATAL("collection '_users' cannot be loaded");
    return;
  }

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_ReleaseCollectionVocBase(vocbase, collection);
    LOG_FATAL("collection '_users' has an unknown collection type");
    return;
  }

  sim = (TRI_sim_collection_t*) doc;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  beg = sim->_primaryIndex._table;
  end = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc;
  ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_t* auth;
      TRI_vocbase_auth_t* old;
      TRI_doc_mptr_t const* d;

      d = (TRI_doc_mptr_t const*) *ptr;

      auth = ConvertAuthInfo(vocbase, doc, &d->_document);

      if (auth != NULL) {
        old = TRI_InsertElementAssociativePointer(&vocbase->_authInfo, auth, true);

        if (old != NULL) {
          FreeAuthInfo(old);
        }
      }
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_ReleaseCollectionVocBase(vocbase, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the default authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DefaultAuthInfo (TRI_vocbase_t* vocbase) {
  DefaultAuthInfo = vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the default authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAuthInfo (struct TRI_vocbase_s* vocbase) {
  void** beg;
  void** end;
  void** ptr;

  if (vocbase == DefaultAuthInfo) {
    DefaultAuthInfo = 0;
  }

  beg = vocbase->_authInfo._table;
  end = vocbase->_authInfo._table + vocbase->_authInfo._nrAlloc;
  ptr = beg;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_vocbase_auth_t* auth = *ptr;

      FreeAuthInfo(auth);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAuthenticationAuthInfo (char const* username,
                                      char const* password) {
  TRI_vocbase_auth_t* auth;
  bool res;

  if (DefaultAuthInfo == 0) {
    return true;
  }

  TRI_ReadLockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
  auth = TRI_LookupByKeyAssociativePointer(&DefaultAuthInfo->_authInfo, username);

  if (auth == 0) {
    TRI_ReadUnlockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
    return false;
  }

  if (! auth->_active) {
    TRI_ReadUnlockReadWriteLock(&DefaultAuthInfo->_authInfoLock);
    return false;
  }

  LOG_DEBUG("found active user '%s', expecting password '%s', got '%s'",
            username,
            auth->_password,
            password);

  res = TRI_EqualString(auth->_password, password);
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
