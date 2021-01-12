////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AUTHENTICATION_USER_H
#define ARANGOD_AUTHENTICATION_USER_H 1

#include <set>

#include "Auth/Common.h"
#include "VocBase/Identifiers/RevisionId.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace auth {

/// @brief Represents a 'user' entry.
/// It contains structures to store the access
/// levels for databases and collections.  The user
/// object must be serialized via `toVPackBuilder()` and
/// written to the _users collection after modifying it.
class User {
  friend class UserManager;

 public:
  static User newUser(std::string const& user, std::string const& pass, auth::Source source);
  static User fromDocument(velocypack::Slice const&);

 private:
  static void fromDocumentDatabases(auth::User&, velocypack::Slice const& databases,
                                    velocypack::Slice const& user);

 public:
  std::string const& key() const { return _key; }
  RevisionId rev() const { return _rev; }
  // updates the user's _loaded attribute
  void touch();

  std::string const& username() const { return _username; }
  std::string const& passwordMethod() const { return _passwordMethod; }
  std::string const& passwordSalt() const { return _passwordSalt; }
  std::string const& passwordHash() const { return _passwordHash; }
  bool isActive() const { return _active; }
  auth::Source source() const { return _source; }

  bool checkPassword(std::string const& password) const;
  void updatePassword(std::string const& password);

  velocypack::Builder toVPackBuilder() const;

  void setActive(bool active) { _active = active; }

  /// grant specific access rights for db. The default "*" is also a
  /// valid database name
  void grantDatabase(std::string const& dbname, auth::Level level);

  /// Removes the entry, returns true if entry existed
  bool removeDatabase(std::string const& dbname);

  /// Grant collection rights, "*" is a valid parameter for dbname and
  /// collection.  The combination of "*"/"*" is automatically used for
  /// the root
  void grantCollection(std::string const& dbname, std::string const& cname, auth::Level level);

  /// Removes the collection right, returns true if entry existed
  bool removeCollection(std::string const& dbname, std::string const& collection);

  // Resolve the access level for this database.
  auth::Level configuredDBAuthLevel(std::string const& dbname) const;
  // Resolve rights for the specified collection.
  auth::Level configuredCollectionAuthLevel(std::string const& dbname,
                                            std::string const& cname) const;

  // Resolve the access level for this database. Might fall back to
  // the special '*' entry if the specific database is not found
  auth::Level databaseAuthLevel(std::string const& dbname) const;

  // Resolve rights for the specified collection. Falls back to the
  // special '*' entry if either the database or collection is not
  // found.
  auth::Level collectionAuthLevel(std::string const& dbname, std::string const& cname) const;

  /// Content of `userData` or `extra` fields
  velocypack::Slice userData() const { return _userData.slice(); }

  /// Set content of `userData` or `extra` fields
  void setUserData(velocypack::Builder&& b) { _userData = std::move(b); }

  /// Content of internal `configData` field, used by the WebUI
  velocypack::Slice configData() const { return _configData.slice(); }

  /// Set content of internal `configData` field, used by the WebUI
  void setConfigData(velocypack::Builder&& b) { _configData = std::move(b); }

  /// Time in seconds (since epoch) when user was loaded
  double loaded() const { return _loaded; }

#ifdef USE_ENTERPRISE
  std::set<std::string> const& roles() const { return _roles; }
  void setRoles(std::set<std::string> const& roles) { _roles = roles; }
#endif

 private:
  User(std::string&& key, RevisionId rid);
  typedef std::unordered_map<std::string, auth::Level> CollLevelMap;

  struct DBAuthContext {
    DBAuthContext(auth::Level dbLvl, CollLevelMap const& coll)
        : _databaseAuthLevel(dbLvl), _collectionAccess(coll) {}

    DBAuthContext(auth::Level dbLvl, CollLevelMap&& coll)
        : _databaseAuthLevel(dbLvl), _collectionAccess(std::move(coll)) {}

   public:
    auth::Level _databaseAuthLevel;
    CollLevelMap _collectionAccess;
  };

 private:
  std::string _key;
  RevisionId _rev;
  bool _active = true;
  auth::Source _source = auth::Source::Local;

  std::string _username;
  std::string _passwordMethod;
  std::string _passwordSalt;
  std::string _passwordHash;
  std::unordered_map<std::string, DBAuthContext> _dbAccess;

  velocypack::Builder _userData;
  velocypack::Builder _configData;

  /// Time when user was loaded from DB / LDAP
  double _loaded;

#ifdef USE_ENTERPRISE
  /// @brief roles this user has
  std::set<std::string> _roles;
#endif
};
}  // namespace auth
}  // namespace arangodb

#endif
