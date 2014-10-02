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

#ifndef ARANGODB_VOC_BASE_AUTH_H
#define ARANGODB_VOC_BASE_AUTH_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_json_t;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief authentication and authorisation
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vocbase_auth_s {
  char* _username;
  char* _passwordMethod;
  char* _passwordSalt;
  char* _passwordHash;
  bool _active;
  bool _mustChange;
}
TRI_vocbase_auth_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief header to username cache
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vocbase_auth_cache_s {
  char* _hash;
  char* _username;
  bool _mustChange;
}
TRI_vocbase_auth_cache_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the authentication info
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAuthInfo (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAuthInfo (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert initial authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertInitialAuthInfo (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief populate the authentication info cache
////////////////////////////////////////////////////////////////////////////////

bool TRI_PopulateAuthInfo (struct TRI_vocbase_s*,
                           struct TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the authentication info
/// this must be executed after the underlying _users collection is modified
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReloadAuthInfo (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearAuthInfo (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up authentication data in the cache
////////////////////////////////////////////////////////////////////////////////

char* TRI_CheckCacheAuthInfo (struct TRI_vocbase_s*,
                              char const* hash,
                              bool* mustChange);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAuthenticationAuthInfo (struct TRI_vocbase_s*,
                                      char const* hash,
                                      char const* username,
                                      char const* password,
                                      bool* mustChange);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
