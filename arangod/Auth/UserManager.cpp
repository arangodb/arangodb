////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "RestServer/DatabaseFeature.h"
#include "RestServer/InitDatabaseFeature.h"
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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

auth::UserManager::UserManager(std::unique_ptr<auth::Handler>&& handler)
    : _outdated(true),
      _queryRegistry(nullptr),
      _authHandler(handler.release()) {}

auth::UserManager::~UserManager() {}

// Parse the users
static auth::UserMap ParseUsers(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());
  auth::UserMap result;
  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    VPackSlice s = authSlice.resolveExternal();
    
    if (s.hasKey("source") && s.get("source").isString() &&
        s.get("source").copyString() == "LDAP") {
      LOG_TOPIC(TRACE, arangodb::Logger::CONFIG)
      << "LDAP: skip user in collection _users: "
      << s.get("user").copyString();
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

static std::shared_ptr<VPackBuilder> QueryAllUsers(
    aql::QueryRegistry* queryRegistry) {
  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
  if (vocbase == nullptr) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "system database is unknown";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  std::string const queryStr("FOR user IN _users RETURN user");
  auto emptyBuilder = std::make_shared<VPackBuilder>();
  arangodb::aql::Query query(false, vocbase,
                             arangodb::aql::QueryString(queryStr), emptyBuilder,
                             emptyBuilder, arangodb::aql::PART_MAIN);

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
      << "starting to load authentication and authorization information";
  auto queryResult = query.execute(queryRegistry);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        queryResult.code, "Error executing user query: " + queryResult.details);
  }

  VPackSlice usersSlice = queryResult.result->slice();

  if (usersSlice.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  } else if (!usersSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot read users from _users collection";
    return std::shared_ptr<VPackBuilder>();
  }

  return queryResult.result;
}

static VPackBuilder QueryUser(aql::QueryRegistry* queryRegistry,
                              std::string const& user) {
  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  if (vocbase == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "_system db is unknown");
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  std::string const queryStr("FOR u IN _users FILTER u.user == @name RETURN u");
  auto emptyBuilder = std::make_shared<VPackBuilder>();

  VPackBuilder binds;
  binds.openObject();
  binds.add("name", VPackValue(user));
  binds.close();  // obj
  arangodb::aql::Query query(false, vocbase,
                             arangodb::aql::QueryString(queryStr),
                             std::make_shared<VPackBuilder>(binds),
                             emptyBuilder, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(queryRegistry);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        queryResult.code, "Error executing user query: " + queryResult.details);
  }

  VPackSlice usersSlice = queryResult.result->slice();

  if (usersSlice.isNone() || !usersSlice.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (usersSlice.length() == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_USER_NOT_FOUND);
  }

  VPackSlice doc = usersSlice.at(0);

  if (doc.isExternal()) {
    doc = doc.resolveExternals();
  }
  VPackBuilder result;
  result.add(doc);
  return result;
}

static void ConvertLegacyFormat(VPackSlice doc, VPackBuilder& result) {
  if (doc.isExternal()) {
    doc = doc.resolveExternals();
  }
  VPackSlice authDataSlice = doc.get("authData");
  VPackObjectBuilder b(&result, true);
  result.add("user", doc.get("user"));
  result.add("active", authDataSlice.get("active"));
  VPackSlice extra = doc.get("userData");
  result.add("extra", extra.isNone() ? VPackSlice::emptyObjectSlice() : extra);
}

// private, will acquire _authInfoLock in write-mode and release it.
// will also aquire _loadFromDBLock and release it
void auth::UserManager::loadFromDB() {
  TRI_ASSERT(_queryRegistry != nullptr);
  TRI_ASSERT(ServerState::instance()->isSingleServerOrCoordinator());
  if (!ServerState::instance()->isSingleServerOrCoordinator()) {
    _outdated = false; // should not get here
    return;
  }

  if (!_outdated) {
    return;
  }
  MUTEX_LOCKER(locker, _loadFromDBLock);
  // double check to be sure after we got the lock
  if (!_outdated) {
    return;
  }

  try {
    std::shared_ptr<VPackBuilder> builder = QueryAllUsers(_queryRegistry);
    if (builder) {
      VPackSlice usersSlice = builder->slice();
      if (usersSlice.length() != 0) {
        UserMap usermap = ParseUsers(usersSlice);
        
        WRITE_LOCKER(writeLocker, _authInfoLock);
        AuthenticationFeature::instance()->tokenCache()->invalidateBasicCache();
        _authInfo.swap(usermap);
        _outdated = false;
      }
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::AUTHENTICATION)
        << "Exception when loading users from db: " << ex.what();
    _outdated = true;
  } catch (...) {
    LOG_TOPIC(TRACE, Logger::AUTHENTICATION)
        << "Exception when loading users from db";
    _outdated = true;
  }
}

// only call from the boostrap feature, must be sure to be the only one
void auth::UserManager::createRootUser() {
  loadFromDB();

  MUTEX_LOCKER(locker, _loadFromDBLock);
  WRITE_LOCKER(writeLocker, _authInfoLock);
  auto it = _authInfo.find("root");
  if (it != _authInfo.end()) {
    LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "Root already exists";
    return;
  }
  TRI_ASSERT(_authInfo.empty());

  try {
    // Attention:
    // the root user needs to have a specific rights grant
    // to the "_system" database, otherwise things break
    auto initDatabaseFeature =
        application_features::ApplicationServer::getFeature<
            InitDatabaseFeature>("InitDatabase");

    TRI_ASSERT(initDatabaseFeature != nullptr);

    auth::User user = auth::User::newUser(
        "root", initDatabaseFeature->defaultPassword(), auth::Source::COLLECTION);
    user.setActive(true);
    user.grantDatabase(StaticStrings::SystemDatabase, auth::Level::RW);
    user.grantDatabase("*", auth::Level::RW);
    user.grantCollection("*", "*", auth::Level::RW);
    storeUserInternal(user, false);
  } catch (...) {
    // No action
  }
}

// private, must be called with _authInfoLock in write mode
// this method can only be called by users with access to the _system collection
Result auth::UserManager::storeUserInternal(auth::User const& entry, bool replace) {
  VPackBuilder data = entry.toVPackBuilder();
  bool hasKey = data.slice().hasKey(StaticStrings::KeyString);
  TRI_ASSERT((replace && hasKey) || (!replace && !hasKey));

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, TRI_COL_NAME_USERS,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();
  if (res.ok()) {
    OperationOptions ops;
    ops.returnNew = true;
    OperationResult result =
        replace ? trx.replace(TRI_COL_NAME_USERS, data.slice(), ops)
                : trx.insert(TRI_COL_NAME_USERS, data.slice(), ops);
    res = trx.finish(result.result);
    if (res.ok()) {
      VPackSlice userDoc = result.slice();
      TRI_ASSERT(userDoc.isObject() && userDoc.hasKey("new"));
      userDoc = userDoc.get("new");
      if (userDoc.isExternal()) {
        userDoc = userDoc.resolveExternal();
      }

      // parse user including document _key
      auth::User created = auth::User::fromDocument(userDoc);
      TRI_ASSERT(!created.key().empty());
      TRI_ASSERT(created.username() == entry.username());
      TRI_ASSERT(created.isActive() == entry.isActive());
      TRI_ASSERT(created.passwordHash() == entry.passwordHash());
      TRI_ASSERT(!replace || created.key() == entry.key());

      if (!_authInfo.emplace(entry.username(), std::move(created)).second) {
        // insertion should always succeed, but...
        _authInfo.erase(entry.username());
        _authInfo.emplace(entry.username(),
                          auth::User::fromDocument(userDoc));
      }
    }
  }
  return res;
}

// -----------------------------------------------------------------------------
// -- SECTION --                                                          public
// -----------------------------------------------------------------------------

VPackBuilder auth::UserManager::allUsers() {
  // will query db directly, no need for _authInfoLock
  std::shared_ptr<VPackBuilder> users;
  {
    TRI_ASSERT(_queryRegistry != nullptr);
    users = QueryAllUsers(_queryRegistry);
  }

  VPackBuilder result;
  VPackArrayBuilder a(&result);
  if (users && !users->isEmpty()) {
    for (VPackSlice const& doc : VPackArrayIterator(users->slice())) {
      ConvertLegacyFormat(doc, result);
    }
  }
  return result;
}

/// Trigger eventual reload, user facing API call
void auth::UserManager::reloadAllUsers() {
  if (!ServerState::instance()->isCoordinator()) {
    // will reload users on next suitable query
    return;
  }

  // tell other coordinators to reload as well
  AgencyComm agency;

  AgencyWriteTransaction incrementVersion({
    AgencyOperation("Sync/UserVersion", AgencySimpleOperationType::INCREMENT_OP)
  });

  int maxTries = 10;

  while (maxTries-- > 0) {
    AgencyCommResult result = agency.sendTransactionWithFailover(incrementVersion);
    if (result.successful()) {
      return;
    }
  }

  LOG_TOPIC(WARN, Logger::AUTHENTICATION)
      << "Sync/UserVersion could not be updated";
}

Result auth::UserManager::storeUser(bool replace, std::string const& user,
                           std::string const& pass, bool active) {
  if (user.empty()) {
    return TRI_ERROR_USER_INVALID_NAME;
  }

  loadFromDB();

  WRITE_LOCKER(writeGuard, _authInfoLock);
  auto const& it = _authInfo.find(user);

  if (replace && it == _authInfo.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  } else if (!replace && it != _authInfo.end()) {
    return TRI_ERROR_USER_DUPLICATE;
  }

  std::string oldKey;  // will only be populated during replace

  if (replace) {
    auto const& oldEntry = it->second;
    oldKey = oldEntry.key();
    if (oldEntry.source() == auth::Source::LDAP) {
      return TRI_ERROR_USER_EXTERNAL;
    }
  }

  auth::User entry = auth::User::newUser(user, pass, auth::Source::COLLECTION);
  entry.setActive(active);

  if (replace) {
    TRI_ASSERT(!oldKey.empty());
    entry._key = std::move(oldKey);
  }

  Result r = storeUserInternal(entry, replace);

  if (r.ok()) {
    reloadAllUsers();
  }

  return r;
}

/// Update user document in the dbserver
static Result UpdateUser(VPackSlice const& user) {
  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, TRI_COL_NAME_USERS,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();
  if (res.ok()) {
    OperationOptions options;
    options.mergeObjects = false;
    OperationResult result =
        trx.update(TRI_COL_NAME_USERS, user, options);
    res = trx.finish(result.result);
  }
  return res;
}

Result auth::UserManager::enumerateUsers(
    std::function<void(auth::User&)> const& func) {
  loadFromDB();
  // we require a consistent view on the user object
  {
    WRITE_LOCKER(guard, _authInfoLock);
    for (auto& it : _authInfo) {
      auto const& entry = it.second;

      if (entry.source() == auth::Source::LDAP) {
        continue;
      }

      TRI_ASSERT(!entry.key().empty());

      func(it.second);
      VPackBuilder data = it.second.toVPackBuilder();
      Result r = UpdateUser(data.slice());

      if (!r.ok()) {
        return r;
      }
    }

    // must also clear the basic cache here because the secret may be
    // invalid now if the password was changed
    AuthenticationFeature::instance()->tokenCache()->invalidateBasicCache();
  }

  // we need to reload data after the next callback
  reloadAllUsers();
  return TRI_ERROR_NO_ERROR;
}

Result auth::UserManager::updateUser(std::string const& user,
                                     UserCallback const& func) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  loadFromDB();

  Result r;
  {  // we require a consistent view on the user object
    WRITE_LOCKER(guard, _authInfoLock);
    auto it = _authInfo.find(user);

    if (it == _authInfo.end()) {
      return Result(TRI_ERROR_USER_NOT_FOUND);
    }

    auto& oldEntry = it->second;

    if (oldEntry.source() == auth::Source::LDAP) {
      return TRI_ERROR_USER_EXTERNAL;
    }

    TRI_ASSERT(!oldEntry.key().empty());

    func(oldEntry);
    VPackBuilder data = oldEntry.toVPackBuilder();
    r = UpdateUser(data.slice());

    // must also clear the basic cache here because the secret may be
    // invalid now if the password was changed
    AuthenticationFeature::instance()->tokenCache()->invalidateBasicCache();
  }

  // we need to reload data after the next callback
  reloadAllUsers();
  return r;
}

Result auth::UserManager::accessUser(std::string const& user,
                                     ConstUserCallback const& func) {
  loadFromDB();
  READ_LOCKER(guard, _authInfoLock);
  auto it = _authInfo.find(user);
  if (it != _authInfo.end()) {
    func(it->second);
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_USER_NOT_FOUND;
}

VPackBuilder auth::UserManager::serializeUser(std::string const& user) {
  loadFromDB();
  // will query db directly, no need for _authInfoLock
  VPackBuilder doc = QueryUser(_queryRegistry, user);
  VPackBuilder result;
  if (!doc.isEmpty()) {
    ConvertLegacyFormat(doc.slice(), result);
  }
  return result;
}

static Result RemoveUserInternal(auth::User const& entry) {
  TRI_ASSERT(!entry.key().empty());
  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextScope scope(ExecContext::superuser());
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, TRI_COL_NAME_USERS,
                                  AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (res.ok()) {
    VPackBuilder builder;
    {
      VPackObjectBuilder guard(&builder);
      builder.add(StaticStrings::KeyString, VPackValue(entry.key()));
      // TODO maybe protect with a revision ID?
    }

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

  Result res;
  {
    WRITE_LOCKER(guard, _authInfoLock);
    auto const& it = _authInfo.find(user);

    if (it == _authInfo.end()) {
      return TRI_ERROR_USER_NOT_FOUND;
    }

    auto const& oldEntry = it->second;

    if (oldEntry.source() == auth::Source::LDAP) {
      return TRI_ERROR_USER_EXTERNAL;
    }

    res = RemoveUserInternal(oldEntry);

    if (res.ok()) {
      _authInfo.erase(it);
      // must also clear the basic cache here because the secret is invalid now
      AuthenticationFeature::instance()->tokenCache()->invalidateBasicCache();
    }
  }

  reloadAllUsers();
  return res;
}

Result auth::UserManager::removeAllUsers() {
  loadFromDB();

  Result res;

  {
    MUTEX_LOCKER(locker, _loadFromDBLock);
    WRITE_LOCKER(guard, _authInfoLock);

    for (auto const& pair : _authInfo) {
      auto const& oldEntry = pair.second;

      if (oldEntry.source() == auth::Source::LDAP) {
        continue;
      }

      res = RemoveUserInternal(oldEntry);

      if (!res.ok()) {
        break;
      }
    }

    // do not get into race conditions with loadFromDB
    {
      _authInfo.clear();
      AuthenticationFeature::instance()->tokenCache()->invalidateBasicCache();
      _outdated = true;
    }
  }

  reloadAllUsers();
  return res;
}

VPackBuilder auth::UserManager::getConfigData(std::string const& username) {
  loadFromDB();
  VPackBuilder bb = QueryUser(_queryRegistry, username);

  return bb.isEmpty() ? bb : VPackBuilder(bb.slice().get("configData"));
}

Result auth::UserManager::setConfigData(std::string const& user,
                               velocypack::Slice const& data) {
  loadFromDB();

  READ_LOCKER(guard, _authInfoLock);

  auto it = _authInfo.find(user);

  if (it == _authInfo.end()) {
    return Result(TRI_ERROR_USER_NOT_FOUND);
  }

  auto const& oldEntry = it->second;

  if (oldEntry.source() == auth::Source::LDAP) {
    return TRI_ERROR_USER_EXTERNAL;
  }

  TRI_ASSERT(!oldEntry.key().empty());

  VPackBuilder partial;
  partial.openObject();
  partial.add(StaticStrings::KeyString, VPackValue(oldEntry.key()));
  partial.add("configData", data);
  partial.close();

  return UpdateUser(partial.slice());
}

VPackBuilder auth::UserManager::getUserData(std::string const& username) {
  loadFromDB();
  VPackBuilder bb = QueryUser(_queryRegistry, username);
  return bb.isEmpty() ? bb : VPackBuilder(bb.slice().get("userData"));
}

Result auth::UserManager::setUserData(std::string const& user,
                             velocypack::Slice const& data) {
  loadFromDB();

  READ_LOCKER(guard, _authInfoLock);

  auto it = _authInfo.find(user);

  if (it == _authInfo.end()) {
    return Result(TRI_ERROR_USER_NOT_FOUND);
  }

  auto const& oldEntry = it->second;

  if (oldEntry.source() == auth::Source::LDAP) {
    return TRI_ERROR_USER_EXTERNAL;
  }

  TRI_ASSERT(!oldEntry.key().empty());

  VPackBuilder partial;
  partial.openObject();
  partial.add(StaticStrings::KeyString, VPackValue(oldEntry.key()));
  partial.add("userData", data);
  partial.close();

  return UpdateUser(partial.slice());
}

bool auth::UserManager::checkPassword(std::string const& username,
                                   std::string const& password) {
  //AuthResult result(username);
  if (username.empty() || StringUtils::isPrefix(username, ":role:")) {
    return false;
  }

  loadFromDB();

  READ_LOCKER(readLocker, _authInfoLock);

  auto it = _authInfo.find(username);
  AuthenticationFeature* af = AuthenticationFeature::instance();

  // using local users might be forbidden
  if (it != _authInfo.end() &&
      (it->second.source() == auth::Source::COLLECTION) && af != nullptr &&
      !af->localAuthentication()) {
    return false;
  }
  // handle LDAP based authentication
  if (it == _authInfo.end() || (it->second.source() == auth::Source::LDAP)) {
    TRI_ASSERT(_authHandler != nullptr);
    auth::HandlerResult authResult = _authHandler->authenticate(username, password);

    if (!authResult.ok()) {
      return false;
    }

    // user authed, add to _authInfo and _users
    if (authResult.source() == auth::Source::LDAP) {
      auth::User user = auth::User::newUser(username, password, auth::Source::LDAP);
      user.setRoles(authResult.roles());
      for (auto const& al : authResult.permissions()) {
        user.grantDatabase(al.first, al.second);
      }

      // upgrade read-lock to a write-lock
      readLocker.unlock();
      WRITE_LOCKER(writeLocker, _authInfoLock);

      it = _authInfo.find(username);

      if (it != _authInfo.end()) {
        it->second = user;
      } else {
        auto r = _authInfo.emplace(username, user);
        if (!r.second) { // insertion failed ?
          return false;
        }
        it = r.first;
      }

      auth::User const& auth = it->second;
      if (auth.isActive()) {
        return auth.checkPassword(password);
      }
      return false; // inactive user
    }
  }

  if (it != _authInfo.end()) {
    auth::User const& auth = it->second;
    if (auth.isActive()) {
      return auth.checkPassword(password);
    }
  }

  return false;
}

// worker function for configuredDatabaseAuthLevel
// must only be called with the read-lock on _authInfoLock being held
auth::Level auth::UserManager::configuredDatabaseAuthLevelInternal(std::string const& username,
                                                        std::string const& dbname,
                                                        size_t depth) const {
  auto it = _authInfo.find(username);

  if (it == _authInfo.end()) {
    return auth::Level::NONE;
  }

  auto const& entry = it->second;
  auth::Level level = entry.databaseAuthLevel(dbname);

#ifdef USE_ENTERPRISE
  // check all roles and use the highest permission from them
  for (auto const& role : entry.roles()) {
    if (level == auth::Level::RW) {
      // we already have highest permission
      break;
    }

    // recurse into function, but only one level deep.
    // this allows us to avoid endless recursion without major overhead
    if (depth == 0) {
      auth::Level roleLevel = configuredDatabaseAuthLevelInternal(role, dbname, depth + 1);

      if (level == auth::Level::NONE) {
        // use the permission of the role we just found
        level = roleLevel;
      }
    }
  }
#endif
  return level;
}

auth::Level auth::UserManager::configuredDatabaseAuthLevel(std::string const& username,
                                                         std::string const& dbname) {
  loadFromDB();
  READ_LOCKER(guard, _authInfoLock);
  return configuredDatabaseAuthLevelInternal(username, dbname, 0);
}

auth::Level auth::UserManager::canUseDatabase(std::string const& username,
                                            std::string const& dbname) {
  auth::Level level = configuredDatabaseAuthLevel(username, dbname);
  static_assert(auth::Level::RO < auth::Level::RW, "ro < rw");
  if (level > auth::Level::RO && !ServerState::writeOpsEnabled()) {
    return auth::Level::RO;
  }
  return level;
}

auth::Level auth::UserManager::canUseDatabaseNoLock(std::string const& username,
                                         std::string const& dbname) {
  auth::Level level = configuredDatabaseAuthLevelInternal(username, dbname, 0);
  static_assert(auth::Level::RO < auth::Level::RW, "ro < rw");
  if (level > auth::Level::RO && !ServerState::writeOpsEnabled()) {
    return auth::Level::RO;
  }
  return level;
}


// internal method called by configuredCollectionAuthLevel
// asserts that collection name is non-empty and already translated
// from collection id to name
auth::Level auth::UserManager::configuredCollectionAuthLevelInternal(std::string const& username,
                                                          std::string const& dbname,
                                                          std::string const& coll,
                                                          size_t depth) const {

  // we must have got a non-empty collection name when we get here
  TRI_ASSERT(coll[0] < '0' || coll[0] > '9');

  auto it = _authInfo.find(username);
  if (it == _authInfo.end()) {
    return auth::Level::NONE;
  }

  auto const& entry = it->second;
  auth::Level level = entry.collectionAuthLevel(dbname, coll);

#ifdef USE_ENTERPRISE
  for (auto const& role : entry.roles()) {
    if (level == auth::Level::RW) {
      // we already have highest permission
      return level;
    }

    // recurse into function, but only one level deep.
    // this allows us to avoid endless recursion without major overhead
    if (depth == 0) {
      auth::Level roleLevel = configuredCollectionAuthLevelInternal(role, dbname, coll, depth + 1);

      if (level == auth::Level::NONE) {
        // use the permission of the role we just found
        level = roleLevel;
      }
    }
  }
#endif
  return level;
}

auth::Level auth::UserManager::configuredCollectionAuthLevel(std::string const& username,
                                                  std::string const& dbname,
                                                  std::string coll) {
  if (coll.empty()) {
    // no collection name given
    return auth::Level::NONE;
  }
  if (coll[0] >= '0' && coll[0] <= '9') {
    coll = DatabaseFeature::DATABASE->translateCollectionName(dbname, coll);
  }

  loadFromDB();
  READ_LOCKER(guard, _authInfoLock);

  return configuredCollectionAuthLevelInternal(username, dbname, coll, 0);
}

auth::Level auth::UserManager::canUseCollection(std::string const& username,
                                     std::string const& dbname,
                                     std::string const& coll) {
  if (coll.empty()) {
    // no collection name given
    return auth::Level::NONE;
  }

  auth::Level level = configuredCollectionAuthLevel(username, dbname, coll);
  static_assert(auth::Level::RO < auth::Level::RW, "ro < rw");
  if (level > auth::Level::RO && !ServerState::writeOpsEnabled()) {
    return auth::Level::RO;
  }
  return level;
}

auth::Level auth::UserManager::canUseCollectionNoLock(std::string const& username,
                                           std::string const& dbname,
                                           std::string const& coll) {
  if (coll.empty()) {
    // no collection name given
    return auth::Level::NONE;
  }

  auth::Level level;
  if (coll[0] >= '0' && coll[0] <= '9') {
    std::string tmpColl = DatabaseFeature::DATABASE->translateCollectionName(dbname, coll);
    level = configuredCollectionAuthLevelInternal(username, dbname, tmpColl, 0);
  } else {
    level = configuredCollectionAuthLevelInternal(username, dbname, coll, 0);
  }

  static_assert(auth::Level::RO < auth::Level::RW, "ro < rw");
  if (level > auth::Level::RO && !ServerState::writeOpsEnabled()) {
    return auth::Level::RO;
  }
  return level;
}

/// Only used for testing
void auth::UserManager::setAuthInfo(auth::UserMap const& newMap) {
  MUTEX_LOCKER(guard, _loadFromDBLock);
  WRITE_LOCKER(writeLocker, _authInfoLock);
  _authInfo = newMap;
  _outdated = false;
}
