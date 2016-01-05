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

#ifndef ARANGODB_VOC_BASE_AUTH_H
#define ARANGODB_VOC_BASE_AUTH_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_vocbase_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief authentication and authorization
////////////////////////////////////////////////////////////////////////////////

class VocbaseAuthInfo {
  private:
    std::string const _username;
    std::string const _passwordMethod;
    std::string const _passwordSalt;
    std::string const _passwordHash;
    bool const _active;
    bool const _mustChange;

  public:
    VocbaseAuthInfo (std::string const& username,
                     std::string const& passwordMethod,
                     std::string const& passwordSalt,
                     std::string const& passwordHash,
                     bool active,
                     bool mustChange)
    : _username(username),
      _passwordMethod(passwordMethod),
      _passwordSalt(passwordSalt),
      _passwordHash(passwordHash),
      _active(active), 
      _mustChange(mustChange) {
    }

    ~VocbaseAuthInfo () {}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the auth info
////////////////////////////////////////////////////////////////////////////////

    uint64_t hash () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the given string is equal to the username
////////////////////////////////////////////////////////////////////////////////

    bool isEqualName (char const*) const;

    bool isEqualPasswordHash (char const*) const;

    char const* username () const;

    char const* passwordSalt () const;

    char const* passwordMethod () const;

    bool isActive () const;

    bool mustChange () const;
};

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
/// @brief initializes the authentication info
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAuthInfo (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAuthInfo (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert initial authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertInitialAuthInfo (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the authentication info
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadAuthInfo (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief populate the authentication info cache
////////////////////////////////////////////////////////////////////////////////

bool TRI_PopulateAuthInfo (TRI_vocbase_t*,
                           VPackSlice const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the authentication info
/// this must be executed after the underlying _users collection is modified
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReloadAuthInfo (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the authentication info
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearAuthInfo (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up authentication data in the cache
////////////////////////////////////////////////////////////////////////////////

char* TRI_CheckCacheAuthInfo (TRI_vocbase_t*,
                              char const* hash,
                              bool* mustChange);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication - note: only checks whether the user exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsAuthenticationAuthInfo (TRI_vocbase_t*,
                                       char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAuthenticationAuthInfo (TRI_vocbase_t*,
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
