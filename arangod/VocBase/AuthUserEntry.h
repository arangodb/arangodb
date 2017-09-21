////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_AUTH_USER_H
#define ARANGOD_VOC_BASE_AUTH_USER_H 1

#include "Basics/Common.h"
#include "Utils/Authentication.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

/// This class represents a 'user' entry. It contains structures
/// to store the access levels for databases and collections.
/// The user object must be serialized via `toVPackBuilder()` and
/// written to the _users collection after modifying it.
///
class AuthUserEntry {
  friend class AuthInfo;

 public:
  static AuthUserEntry newUser(std::string const& user, std::string const& pass,
                               AuthSource source);
  static AuthUserEntry fromDocument(velocypack::Slice const&);

 public:
  std::string const& key() const { return _key; }
  std::string const& username() const { return _username; }
  std::string const& passwordMethod() const { return _passwordMethod; }
  std::string const& passwordSalt() const { return _passwordSalt; }
  std::string const& passwordHash() const { return _passwordHash; }
  bool isActive() const { return _active; }
  AuthSource source() const { return _source; }

  bool checkPassword(std::string const& password) const;
  void updatePassword(std::string const& password);

  velocypack::Builder toVPackBuilder() const;

  void setActive(bool active) { _active = active; }

  /// grant specific access rights for db. The default "*"
  /// is also a valid database name
  void grantDatabase(std::string const& dbname, AuthLevel level);
  /// Removes the entry.
  void removeDatabase(std::string const& dbname);
  /// Grant collection rights, "*" is a valid parameter for dbname and
  /// collection.
  /// The combination of "*"/"*" is automatically used for the root user
  void grantCollection(std::string const& dbname, std::string const& collection,
                       AuthLevel level);
  void removeCollection(std::string const& dbname,
                        std::string const& collection);

  /// Resolve the access level for this database. Might fall back to
  /// the special '*' entry if the specific database is not found
  AuthLevel databaseAuthLevel(std::string const& dbname) const;
  /// Resolve rights for the specified collection. Falls back to the
  /// special '*' entry if either the database or collection is not
  /// found.
  AuthLevel collectionAuthLevel(std::string const& dbname,
                                std::string const& collectionName) const;

  bool hasSpecificDatabase(std::string const& dbname) const;
  bool hasSpecificCollection(std::string const& dbname,
                             std::string const& collectionName) const;

 private:
  AuthUserEntry() {}

  struct DBAuthContext {
    DBAuthContext(AuthLevel dbLvl,
                  std::unordered_map<std::string, AuthLevel> const& coll)
        : _databaseAuthLevel(dbLvl), _collectionAccess(coll) {}
    
    DBAuthContext(AuthLevel dbLvl,
                  std::unordered_map<std::string, AuthLevel>&& coll)
        : _databaseAuthLevel(dbLvl), _collectionAccess(std::move(coll)) {}

    AuthLevel collectionAuthLevel(std::string const& collectionName, bool& notFound) const;

   public:
    AuthLevel _databaseAuthLevel;
    std::unordered_map<std::string, AuthLevel> _collectionAccess;
  };

 private:
  std::string _key;
  bool _active = true;
  AuthSource _source = AuthSource::COLLECTION;

  std::string _username;
  std::string _passwordMethod;
  std::string _passwordSalt;
  std::string _passwordHash;
  std::unordered_map<std::string, DBAuthContext> _dbAccess;
};
}

#endif
