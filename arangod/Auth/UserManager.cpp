////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "UserManager.h"

#include "Agency/AgencyComm.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Auth/Handler.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadUnlocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Random/UniformCharacter.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Ssl/SslInterface.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the system database or nullptr on error
////////////////////////////////////////////////////////////////////////////////
arangodb::SystemDatabaseFeature::ptr getSystemDatabase() {
  auto* feature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();

  if (!feature) {
    LOG_TOPIC(WARN, arangodb::Logger::AUTHENTICATION)
        << "failure to find feature '" << arangodb::SystemDatabaseFeature::name()
        << "' while getting the system database";

    return nullptr;
  }

  return feature->use();
}

}  // namespace

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

static bool inline IsRole(std::string const& name) {
  return StringUtils::isPrefix(name, ":role:");
}

#ifndef USE_ENTERPRISE
auth::UserManager::UserManager()
    : _globalVersion(1), _internalVersion(0), _queryRegistry(nullptr) {}
#else
auth::UserManager::UserManager()
    : _globalVersion(1), _internalVersion(0), _queryRegistry(nullptr), _authHandler(nullptr) {}

auth::UserManager::UserManager(std::unique_ptr<auth::Handler> handler)
    : _globalVersion(1),
      _internalVersion(0),
      _queryRegistry(nullptr),
      _authHandler(std::move(handler)) {}
#endif

// Parse the users
static auth::UserMap ParseUsers(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());
  auth::UserMap result;
  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    VPackSlice s = authSlice.resolveExternal();

    if (s.hasKey("source") && s.get("source").isString() &&
        s.get("source").copyString() == "LDAP") {
      LOG_TOPIC(TRACE, arangodb::Logger::CONFIG)
          << "LDAP: skip user in collection _users: " << s.get("user").copyString();
      continue;
    }

    // we also need to insert inactive users into the cache here
    // otherwise all following update/replace/remove operations on the
    // user will fail
    auth::User user = auth::User::fromDocument(s);
    result.emplace(user.username(), std::move(user));
  }
  return result;
}

static std::shared_ptr<VPackBuilder> QueryAllUsers(aql::QueryRegistry* queryRegistry) {
  auto vocbase = getSystemDatabase();

  if (vocbase == nullptr) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION)
        << "system database is unknown";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  std::string const queryStr("FOR user IN _users RETURN user");
  auto emptyBuilder = std::make_shared<VPackBuilder>();
  arangodb::aql::Query query(false, *vocbase, arangodb::aql::QueryString(queryStr),
                             emptyBuilder, emptyBuilder, arangodb::aql::PART_MAIN);

  query.queryOptions().cache = false;

  LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION)
      << "starting to load authentication and authorization information";

  aql::QueryResult queryResult = query.executeSync(queryRegistry);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code,
                                   "Error executing user query: " + queryResult.details);
  }

  VPackSlice usersSlice = queryResult.result->slice();

  if (usersSlice.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  } else if (!usersSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::AUTHENTICATION)
        << "cannot read users from _users collection";
    return std::shared_ptr<VPackBuilder>();
  }

  return queryResult.result;
}

/// Convert documents from _system/_users into the format used in
/// the REST user API and Foxx
static void ConvertLegacyFormat(VPackSlice doc, VPackBuilder& result) {
  if (doc.isExternal()) {
    doc = doc.resolveExternals();
  }
  VPackSlice authDataSlice = doc.get("authData");
  {
    VPackObjectBuilder b(&result, true);
    result.add("user", doc.get("user"));
    result.add("active", authDataSlice.get("active"));
    VPackSlice extra = doc.get("userData");
    result.add("extra", extra.isNone() ? VPackSlice::emptyObjectSlice() : extra);
  }
}

// private, will acquire _userCacheLock in write-mode and release it.
// will also acquire _loadFromDBLock and release it
void auth::UserManager::loadFromDB() {
  TRI_ASSERT(_queryRegistry != nullptr);
  TRI_ASSERT(ServerState::instance()->isSingleServerOrCoordinator());

  if (_internalVersion.load(std::memory_order_acquire) == globalVersion()) {
    return;
  }
  MUTEX_LOCKER(guard, _loadFromDBLock);
  uint64_t tmp = globalVersion();
  if (_internalVersion.load(std::memory_order_acquire) == tmp) {
    return;
  }

  try {
    std::shared_ptr<VPackBuilder> builder = QueryAllUsers(_queryRegistry);
    if (builder) {
      VPackSlice usersSlice = builder->slice();
      if (usersSlice.length() != 0) {
        UserMap usermap = ParseUsers(usersSlice);

        {  // cannot invalidate token cache while holding _userCache write lock
          WRITE_LOCKER(writeGuard, _userCacheLock);  // must be second
          // never delete non-local users
          for (auto pair = _userCache.cbegin(); pair != _userCache.cend();) {
            if (pair->second.source() == auth::Source::Local) {
              pair = _userCache.erase(pair);
            } else {
              pair++;
            }
          }
          _userCache.insert(usermap.begin(), usermap.end());
#ifdef USE_ENTERPRISE
          applyRolesToAllUsers();
#endif
        }

        _internalVersion.store(tmp);
      }
    }
  } catch (basics::Exception const& ex) {
    auto bootstrap =
        application_features::ApplicationServer::lookupFeature<BootstrapFeature>();
    if (ex.code() != TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ||
        (bootstrap != nullptr && bootstrap->isReady())) {
      LOG_TOPIC(WARN, Logger::AUTHENTICATION)
          << "Exception when loading users from db: " << ex.what();
    }
    // suppress log messgage if we get here during the normal course of an
    // agency callback during bootstrapping and carry on
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::AUTHENTICATION)
        << "Exception when loading users from db: " << ex.what();
  } catch (...) {
    LOG_TOPIC(TRACE, Logger::AUTHENTICATION)
        << "Exception when loading users from db";
  }
}

// private, must be called with _userCacheLock in write mode
// this method can only be called by users with access to the _system collection
Result auth::UserManager::storeUserInternal(auth::User const& entry, bool replace) {
  if (entry.source() != auth::Source::Local) {
    return Result(TRI_ERROR_USER_EXTERNAL);
  }
  if (!IsRole(entry.username()) && entry.username() != "root") {
    AuthenticationFeature* af = AuthenticationFeature::instance();
    TRI_ASSERT(af != nullptr);
    if (af != nullptr && !af->localAuthentication()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Local users are disabled");
    }
  }

  VPackBuilder data = entry.toVPackBuilder();
  bool hasKey = data.slice().hasKey(StaticStrings::KeyString);
  bool hasRev = data.slice().hasKey(StaticStrings::RevString);
  TRI_ASSERT((replace && hasKey && hasRev) || (!replace && !hasKey && !hasRev));

  auto vocbase = getSystemDatabase();

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL, "unable to find system database");
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  auto ctx = transaction::StandaloneContext::Create(*vocbase);
  SingleCollectionTransaction trx(ctx, TRI_COL_NAME_USERS, AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (res.ok()) {
    OperationOptions opts;

    opts.returnNew = true;
    opts.ignoreRevs = false;
    opts.mergeObjects = false;

    OperationResult opres =
        replace ? trx.replace(TRI_COL_NAME_USERS, data.slice(), opts)
                : trx.insert(TRI_COL_NAME_USERS, data.slice(), opts);

    res = trx.finish(opres.result);

    if (res.ok()) {
      VPackSlice userDoc = opres.slice();
      TRI_ASSERT(userDoc.isObject() && userDoc.hasKey("new"));
      userDoc = userDoc.get("new");
      if (userDoc.isExternal()) {
        userDoc = userDoc.resolveExternal();
      }

      // parse user including document _key
      auth::User created = auth::User::fromDocument(userDoc);
      TRI_ASSERT(!created.key().empty() && created.rev() != 0);
      TRI_ASSERT(created.username() == entry.username());
      TRI_ASSERT(created.isActive() == entry.isActive());
      TRI_ASSERT(created.passwordHash() == entry.passwordHash());
      TRI_ASSERT(!replace || created.key() == entry.key());

      if (!_userCache.emplace(entry.username(), std::move(created)).second) {
        // insertion should always succeed, but...
        _userCache.erase(entry.username());
        _userCache.emplace(entry.username(), auth::User::fromDocument(userDoc));
      }
#ifdef USE_ENTERPRISE
      if (IsRole(entry.username())) {
        for (UserMap::value_type& pair : _userCache) {
          if (pair.second.source() != auth::Source::Local &&
              pair.second.roles().find(entry.username()) != pair.second.roles().end()) {
            pair.second._dbAccess.clear();
            applyRoles(pair.second);
          }
        }
      }
#endif
    } else if (res.is(TRI_ERROR_ARANGO_CONFLICT)) {  // user was outdated
      // we didn't succeed in updating the user, so we must not remove the
      // user from the cache here. however, we should trigger a reload here
      triggerLocalReload();
      LOG_TOPIC(WARN, Logger::AUTHENTICATION)
          << "Cannot update user due to conflict";
    }
  }
  return res;
}

// -----------------------------------------------------------------------------
// -- SECTION --                                                          public
// -----------------------------------------------------------------------------

// only call from the boostrap feature, must be sure to be the only one
void auth::UserManager::createRootUser() {
  loadFromDB();

  MUTEX_LOCKER(guard, _loadFromDBLock);      // must be first
  WRITE_LOCKER(writeGuard, _userCacheLock);  // must be second
  UserMap::iterator const& it = _userCache.find("root");
  if (it != _userCache.end()) {
    LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "\"root\" already exists";
    return;
  }
  TRI_ASSERT(_userCache.empty());
  LOG_TOPIC(INFO, Logger::AUTHENTICATION) << "Creating user \"root\"";

  try {
    // Attention:
    // the root user needs to have a specific rights grant
    // to the "_system" database, otherwise things break
    auto initDatabaseFeature =
        application_features::ApplicationServer::getFeature<InitDatabaseFeature>(
            "InitDatabase");

    TRI_ASSERT(initDatabaseFeature != nullptr);

    auth::User user = auth::User::newUser("root", initDatabaseFeature->defaultPassword(),
                                          auth::Source::Local);
    user.setActive(true);
    user.grantDatabase(StaticStrings::SystemDatabase, auth::Level::RW);
    user.grantCollection(StaticStrings::SystemDatabase, "*", auth::Level::RW);
    user.grantDatabase("*", auth::Level::RW);
    user.grantCollection("*", "*", auth::Level::RW);
    storeUserInternal(user, false);
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, Logger::AUTHENTICATION)
        << "unable to create user \"root\": " << ex.what();
  } catch (...) {
    // No action
    LOG_TOPIC(ERR, Logger::AUTHENTICATION) << "unable to create user \"root\"";
  }
}

VPackBuilder auth::UserManager::allUsers() {
  // will query db directly, no need for _userCacheLock
  TRI_ASSERT(_queryRegistry != nullptr);
  std::shared_ptr<VPackBuilder> users = QueryAllUsers(_queryRegistry);

  VPackBuilder result;
  {
    VPackArrayBuilder a(&result);
    if (users && !users->isEmpty()) {
      for (VPackSlice const& doc : VPackArrayIterator(users->slice())) {
        ConvertLegacyFormat(doc, result);
      }
    }
  }
  return result;
}

/// Trigger eventual reload, user facing API call
void auth::UserManager::triggerGlobalReload() {
  if (!ServerState::instance()->isCoordinator()) {
    // will reload users on next suitable query
    _globalVersion.fetch_add(1, std::memory_order_release);
    _internalVersion.fetch_add(1, std::memory_order_release);
    return;
  }

  // tell other coordinators to reload as well
  AgencyComm agency;

  AgencyWriteTransaction incrementVersion(
      {AgencyOperation("Sync/UserVersion", AgencySimpleOperationType::INCREMENT_OP)});

  int maxTries = 10;

  while (maxTries-- > 0) {
    AgencyCommResult result = agency.sendTransactionWithFailover(incrementVersion);
    if (result.successful()) {
      _globalVersion.fetch_add(1, std::memory_order_release);
      _internalVersion.fetch_add(1, std::memory_order_release);
      return;
    }
  }

  LOG_TOPIC(WARN, Logger::AUTHENTICATION)
      << "Sync/UserVersion could not be updated";
}

Result auth::UserManager::storeUser(bool replace, std::string const& username,
                                    std::string const& pass, bool active, VPackSlice extras) {
  if (username.empty()) {
    return TRI_ERROR_USER_INVALID_NAME;
  }

  loadFromDB();
  WRITE_LOCKER(writeGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(username);

  if (replace && it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  } else if (!replace && it != _userCache.end()) {
    return TRI_ERROR_USER_DUPLICATE;
  }

  std::string oldKey;  // will only be populated during replace
  TRI_voc_rid_t oldRev = 0;
  if (replace) {
    auth::User const& oldEntry = it->second;
    oldKey = oldEntry.key();
    oldRev = oldEntry.rev();
    if (oldEntry.source() != auth::Source::Local) {
      return TRI_ERROR_USER_EXTERNAL;
    }
  }

  auth::User user = auth::User::newUser(username, pass, auth::Source::Local);
  user.setActive(active);
  if (extras.isObject() && !extras.isEmptyObject()) {
    user.setUserData(VPackBuilder(extras));
  }

  if (replace) {
    TRI_ASSERT(!oldKey.empty());
    user._key = std::move(oldKey);
    user._rev = oldRev;
  }

  Result r = storeUserInternal(user, replace);
  if (r.ok()) {
    triggerGlobalReload();
  }
  return r;
}

Result auth::UserManager::enumerateUsers(std::function<bool(auth::User&)>&& func) {
  loadFromDB();

  std::vector<auth::User> toUpdate;
  {  // users are later updated with rev ID for consistency
    READ_LOCKER(readGuard, _userCacheLock);
    for (UserMap::value_type& it : _userCache) {
      if (it.second.source() != auth::Source::Local) {
        continue;
      }
      auth::User user = it.second;  // copy user object
      TRI_ASSERT(!user.key().empty() && user.rev() != 0);
      if (func(user)) {
        toUpdate.emplace_back(std::move(user));
      }
    }
  }
  Result res;
  {
    WRITE_LOCKER(writeGuard, _userCacheLock);
    for (auth::User const& u : toUpdate) {
      res = storeUserInternal(u, true);
      if (res.fail()) {
        break;  // do not return, still need to invalidate token cache
      }
    }
  }

  // cannot hold _userCacheLock while  invalidating token cache
  if (!toUpdate.empty()) {
    triggerGlobalReload();  // trigger auth reload in cluster
  }
  return res;
}

Result auth::UserManager::updateUser(std::string const& name, UserCallback&& func) {
  if (name.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  loadFromDB();

  // we require a consistent view on the user object
  WRITE_LOCKER(writeGuard, _userCacheLock);

  UserMap::iterator it = _userCache.find(name);
  if (it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  } else if (it->second.source() != auth::Source::Local) {
    return TRI_ERROR_USER_EXTERNAL;
  }

  LOG_TOPIC(DEBUG, Logger::AUTHENTICATION) << "Updating user " << name;
  auth::User user = it->second;  // make a copy
  TRI_ASSERT(!user.key().empty() && user.rev() != 0);
  Result r = func(user);
  if (r.fail()) {
    return r;
  }
  r = storeUserInternal(user, /*replace*/ true);

  // cannot hold _userCacheLock while  invalidating token cache
  writeGuard.unlock();
  if (r.ok() || r.is(TRI_ERROR_ARANGO_CONFLICT)) {
    // must also clear the basic cache here because the secret may be
    // invalid now if the password was changed
    triggerGlobalReload();  // trigger auth reload in cluster
  }
  return r;
}

Result auth::UserManager::accessUser(std::string const& user, ConstUserCallback&& func) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  loadFromDB();

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  if (it != _userCache.end()) {
    return func(it->second);
  }
  return TRI_ERROR_USER_NOT_FOUND;
}

bool auth::UserManager::userExists(std::string const& user) {
  if (user.empty()) {
    return false;
  }

  loadFromDB();
  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  return it != _userCache.end();
}

VPackBuilder auth::UserManager::serializeUser(std::string const& user) {
  loadFromDB();

  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it != _userCache.end()) {
    VPackBuilder tmp = it->second.toVPackBuilder();
    if (!tmp.isEmpty() && !tmp.slice().isNone()) {
      VPackBuilder result;
      ConvertLegacyFormat(tmp.slice(), result);
      return result;
    }
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_USER_NOT_FOUND);  // FIXME do not use
}

static Result RemoveUserInternal(auth::User const& entry) {
  TRI_ASSERT(!entry.key().empty());
  auto vocbase = getSystemDatabase();

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL, "unable to find system database");
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(entry.key()));
    // TODO maybe protect with a revision ID?
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  auto ctx = transaction::StandaloneContext::Create(*vocbase);
  SingleCollectionTransaction trx(ctx, TRI_COL_NAME_USERS, AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (res.ok()) {
    OperationResult result =
        trx.remove(TRI_COL_NAME_USERS, builder.slice(), OperationOptions());
    res = trx.finish(result.result);
  }

  return res;
}

Result auth::UserManager::removeUser(std::string const& user) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  if (user == "root") {
    return TRI_ERROR_FORBIDDEN;
  }

  loadFromDB();

  WRITE_LOCKER(writeGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User not found: " << user;
    return TRI_ERROR_USER_NOT_FOUND;
  }

  auth::User const& oldEntry = it->second;
  if (oldEntry.source() != auth::Source::Local) {
    return TRI_ERROR_USER_EXTERNAL;
  }
  Result res = RemoveUserInternal(oldEntry);
  if (res.ok()) {
    _userCache.erase(it);
  }

  // cannot hold _userCacheLock while  invalidating token cache
  writeGuard.unlock();
  triggerGlobalReload();  // trigger auth reload in cluster

  return res;
}

Result auth::UserManager::removeAllUsers() {
  loadFromDB();

  Result res;
  {
    // do not get into race conditions with loadFromDB
    MUTEX_LOCKER(guard, _loadFromDBLock);      // must be first
    WRITE_LOCKER(writeGuard, _userCacheLock);  // must be second

    for (auto pair = _userCache.cbegin(); pair != _userCache.cend();) {
      auto const& oldEntry = pair->second;
      if (oldEntry.source() == auth::Source::Local) {
        res = RemoveUserInternal(oldEntry);
        if (!res.ok()) {
          break;  // don't return still need to invalidate token cache
        }
        pair = _userCache.erase(pair);
      } else {
        pair++;
      }
    }
  }

  triggerGlobalReload();
  return res;
}

bool auth::UserManager::checkPassword(std::string const& username, std::string const& password) {
  if (username.empty() || IsRole(username)) {
    return false;  // we cannot authenticate during bootstrap
  }

  loadFromDB();

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator it = _userCache.find(username);

  // using local users might be forbidden
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (it != _userCache.end() && (it->second.source() == auth::Source::Local)) {
    if (af != nullptr && !af->localAuthentication()) {
      LOG_TOPIC(DEBUG, Logger::AUTHENTICATION) << "Local users are forbidden";
      return false;
    }
    auth::User const& user = it->second;
    if (user.isActive()) {
      return user.checkPassword(password);
    }
  }

#ifdef USE_ENTERPRISE
  bool userCached = it != _userCache.end();
  if (!userCached && _authHandler == nullptr) {
    // nothing more to do here
    return false;
  }
  // handle authentication with external system
  if (!userCached || (it->second.source() != auth::Source::Local)) {
    return checkPasswordExt(username, password, userCached, readGuard);
  }
#endif

  return false;
}

auth::Level auth::UserManager::databaseAuthLevel(std::string const& user,
                                                 std::string const& dbname, bool configured) {
  if (dbname.empty()) {
    return auth::Level::NONE;
  }

  loadFromDB();
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User not found: " << user;
    return auth::Level::NONE;
  }

  auth::Level level = it->second.databaseAuthLevel(dbname);
  if (!configured) {
    if (level > auth::Level::RO && ServerState::readOnly()) {
      return auth::Level::RO;
    }
  }
  TRI_ASSERT(level != auth::Level::UNDEFINED);  // not allowed here
  return level;
}

auth::Level auth::UserManager::collectionAuthLevel(std::string const& user,
                                                   std::string const& dbname,
                                                   std::string const& coll, bool configured) {
  if (coll.empty()) {
    return auth::Level::NONE;
  }

  loadFromDB();
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User not found: " << user;
    return auth::Level::NONE;  // no user found
  }

  auth::Level level;
  if (isdigit(coll[0])) {
    std::string tmpColl = DatabaseFeature::DATABASE->translateCollectionName(dbname, coll);
    level = it->second.collectionAuthLevel(dbname, tmpColl);
  } else {
    level = it->second.collectionAuthLevel(dbname, coll);
  }

  if (!configured) {
    static_assert(auth::Level::RO < auth::Level::RW, "ro < rw");
    if (level > auth::Level::RO && ServerState::readOnly()) {
      return auth::Level::RO;
    }
  }
  TRI_ASSERT(level != auth::Level::UNDEFINED);  // not allowed here
  return level;
}

/// Only used for testing
void auth::UserManager::setAuthInfo(auth::UserMap const& newMap) {
  MUTEX_LOCKER(guard, _loadFromDBLock);      // must be first
  WRITE_LOCKER(writeGuard, _userCacheLock);  // must be second
  _userCache = newMap;
  _internalVersion.store(_globalVersion.load());
}
