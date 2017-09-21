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

#include "AuthInfo.h"

#include "Agency/AgencyComm.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"

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

AuthInfo::AuthInfo(std::unique_ptr<AuthenticationHandler>&& handler)
    : _outdated(true),
      _authJwtCache(16384),
      _jwtSecret(""),
      _queryRegistry(nullptr),
      _authenticationHandler(handler.release()) {}

AuthInfo::~AuthInfo() {
  // properly clear structs while using the appropriate locks
  {
    WRITE_LOCKER(readLocker, _authInfoLock);
    _authInfo.clear();
    _authBasicCache.clear();
  }

  {
    WRITE_LOCKER(writeLocker, _authJwtLock);
    _authJwtCache.clear();
  }
}

void AuthInfo::setJwtSecret(std::string const& jwtSecret) {
  WRITE_LOCKER(writeLocker, _authJwtLock);
  _jwtSecret = jwtSecret;
  _authJwtCache.clear();
}

std::string AuthInfo::jwtSecret() {
  READ_LOCKER(writeLocker, _authJwtLock);
  return _jwtSecret;
}

// private, must be called with _authInfoLock in write mode
bool AuthInfo::parseUsers(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  _authInfo.clear();
  _authBasicCache.clear();
  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    VPackSlice s = authSlice.resolveExternal();

    if (s.hasKey("source") && s.get("source").isString() &&
        s.get("source").copyString() == "LDAP") {
      LOG_TOPIC(TRACE, arangodb::Logger::CONFIG)
          << "LDAP: skip user in collection _users: "
          << s.get("user").copyString();
      continue;
    }
    AuthUserEntry auth = AuthUserEntry::fromDocument(s);

    // we also need to insert inactive users into the cache here
    // otherwise all following update/replace/remove operations on the
    // user will fail

    _authInfo.emplace(auth.username(), std::move(auth));
  }

  return true;
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
  ExecContext* oldExe = ExecContext::CURRENT;
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = oldExe);

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
    return std::shared_ptr<VPackBuilder>();
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
  ExecContext* oldExe = ExecContext::CURRENT;
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = oldExe);

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
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, "query error");
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
  return VPackBuilder(doc);
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
void AuthInfo::loadFromDB() {
  if (!_outdated) {
    return;
  }

  MUTEX_LOCKER(locker, _loadFromDBLock);

  // double check to be sure after we got the lock
  if (!_outdated) {
    return;
  }

  auto role = ServerState::instance()->getRole();

  if (role != ServerState::ROLE_SINGLE &&
      role != ServerState::ROLE_COORDINATOR) {
    _outdated = false;
    return;
  }

  TRI_ASSERT(_queryRegistry != nullptr);
  std::shared_ptr<VPackBuilder> builder = QueryAllUsers(_queryRegistry);

  WRITE_LOCKER(writeLocker, _authInfoLock);
  _authBasicCache.clear();

  if (builder) {
    VPackSlice usersSlice = builder->slice();
    if (usersSlice.length() != 0) {
      parseUsers(usersSlice);
    }
  }

  if (_authInfo.empty()) {
    insertInitial();
  }
  _outdated = false;
}

// private, must be called with _authInfoLock in write mode
void AuthInfo::insertInitial() {
  if (!_authInfo.empty()) {
    return;
  }

  try {
    // Attention:
    // the root user needs to have a specific rights grant
    // to the "_system" database, otherwise things break
    auto initDatabaseFeature =
        application_features::ApplicationServer::getFeature<
            InitDatabaseFeature>("InitDatabase");

    AuthUserEntry entry = AuthUserEntry::newUser(
        "root", initDatabaseFeature->defaultPassword(), AuthSource::COLLECTION);
    entry.setActive(true);
    entry.grantDatabase(StaticStrings::SystemDatabase, AuthLevel::RW);
    entry.grantDatabase("*", AuthLevel::RW);
    entry.grantCollection("*", "*", AuthLevel::RW);
    storeUserInternal(entry, false);
  } catch (...) {
    // No action
  }
}

// private, must be called with _authInfoLock in write mode
// this method can only be called by users with access to the _system collection
Result AuthInfo::storeUserInternal(AuthUserEntry const& entry, bool replace) {
  VPackBuilder data = entry.toVPackBuilder();
  bool hasKey = data.slice().hasKey(StaticStrings::KeyString);
  TRI_ASSERT((replace && hasKey) || (!replace && !hasKey));

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContext* oldExe = ExecContext::CURRENT;
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = oldExe);

  std::shared_ptr<transaction::Context> ctx(
      new transaction::StandaloneContext(vocbase));
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
    res = trx.finish(result.code);
    if (res.ok()) {
      VPackSlice userDoc = result.slice();
      TRI_ASSERT(userDoc.isObject() && userDoc.hasKey("new"));
      userDoc = userDoc.get("new");
      if (userDoc.isExternal()) {
        userDoc = userDoc.resolveExternal();
      }

      AuthUserEntry created = AuthUserEntry::fromDocument(userDoc);

      TRI_ASSERT(!created.key().empty());
      TRI_ASSERT(created.username() == entry.username());
      TRI_ASSERT(created.isActive() == entry.isActive());
      TRI_ASSERT(created.passwordHash() == entry.passwordHash());
      TRI_ASSERT(!replace || created.key() == entry.key());

      if (!_authInfo.emplace(entry.username(), std::move(created)).second) {
        // insertion should always succeed, but...
        _authInfo.erase(entry.username());
        _authInfo.emplace(entry.username(),
                          AuthUserEntry::fromDocument(userDoc));
      }
    }
  }
  return res;
}

// ================= public ==================

VPackBuilder AuthInfo::allUsers() {
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
void AuthInfo::reloadAllUsers() {
  if (!ServerState::instance()->isCoordinator()) {
    // will reload users on next suitable query
    return;
  }

  // tell other coordinators to reload as well
  AgencyComm agency;
  int maxTries = 10;
  while (maxTries-- > 0) {
    AgencyCommResult commRes = agency.getValues("Sync/UserVersion");
    if (!commRes.successful()) {
      // Error in communication, note that value not found is not an error
      LOG_TOPIC(TRACE, Logger::AUTHENTICATION)
          << "AuthInfo: no agency communication";
      break;
    }
    VPackSlice oldVal = commRes.slice()[0].get(
        {AgencyCommManager::path(), "Sync", "UserVersion"});
    if (!oldVal.isInteger()) {
      LOG_TOPIC(ERR, Logger::AUTHENTICATION)
          << "Sync/UserVersion is not a number";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    VPackBuilder newVal;
    newVal.add(VPackValue(oldVal.getUInt() + 1));
    commRes =
        agency.casValue("Sync/UserVersion", oldVal, newVal.slice(), 0.0,
                        AgencyCommManager::CONNECTION_OPTIONS._requestTimeout);
    if (commRes.successful()) {
      return;
    }
  }
  LOG_TOPIC(WARN, Logger::AUTHENTICATION)
      << "Sync/UserVersion could not be updated";
}

Result AuthInfo::storeUser(bool replace, std::string const& user,
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

  AuthUserEntry entry =
      AuthUserEntry::newUser(user, pass, AuthSource::COLLECTION);
  entry.setActive(active);
  if (replace) {
    TRI_ASSERT(!(it->second.key().empty()));
    entry._key = it->second.key();
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
  ExecContext* oldExe = ExecContext::CURRENT;
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = oldExe);

  std::shared_ptr<transaction::Context> ctx(
      new transaction::StandaloneContext(vocbase));
  SingleCollectionTransaction trx(ctx, TRI_COL_NAME_USERS,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();
  if (res.ok()) {
    OperationResult result =
        trx.update(TRI_COL_NAME_USERS, user, OperationOptions());
    res = trx.finish(result.code);
  }
  return res;
}

Result AuthInfo::enumerateUsers(
    std::function<void(AuthUserEntry&)> const& func) {
  loadFromDB();
  // we require an consisten view on the user object
  {
    WRITE_LOCKER(guard, _authInfoLock);
    for (auto& it : _authInfo) {
      TRI_ASSERT(!it.second.key().empty());
      func(it.second);
      VPackBuilder data = it.second.toVPackBuilder();
      Result r = UpdateUser(data.slice());
      if (!r.ok()) {
        return r;
      }
    }
    _authBasicCache.clear();
  }
  // we need to reload data after the next callback
  reloadAllUsers();
  return TRI_ERROR_NO_ERROR;
}

Result AuthInfo::updateUser(std::string const& user,
                            std::function<void(AuthUserEntry&)> const& func) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }
  loadFromDB();
  Result r;
  VPackBuilder data;
  {  // we require an consisten view on the user object
    WRITE_LOCKER(guard, _authInfoLock);
    auto it = _authInfo.find(user);
    if (it == _authInfo.end()) {
      return Result(TRI_ERROR_USER_NOT_FOUND);
    }
    TRI_ASSERT(!it->second.key().empty());
    func(it->second);
    data = it->second.toVPackBuilder();
    r = UpdateUser(data.slice());
    // must also clear the basic cache here because the secret may be
    // invalid now if the password was changed
    _authBasicCache.clear();
  }

  // we need to reload data after the next callback
  reloadAllUsers();
  return r;
}

Result AuthInfo::accessUser(
    std::string const& user,
    std::function<void(AuthUserEntry const&)> const& func) {
  loadFromDB();
  READ_LOCKER(guard, _authInfoLock);
  auto it = _authInfo.find(user);
  if (it != _authInfo.end()) {
    func(it->second);
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_USER_NOT_FOUND;
}

VPackBuilder AuthInfo::serializeUser(std::string const& user) {
  // loadFromDB();
  // READ_LOCKER(guard, _authInfoLock)
  VPackBuilder doc = QueryUser(_queryRegistry, user);
  VPackBuilder result;
  if (!doc.isEmpty()) {
    ConvertLegacyFormat(doc.slice(), result);
  }
  return result;
}

static Result RemoveUserInternal(AuthUserEntry const& entry) {
  TRI_ASSERT(!entry.key().empty());
  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContext* oldExe = ExecContext::CURRENT;
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = oldExe);

  std::shared_ptr<transaction::Context> ctx(
      new transaction::StandaloneContext(vocbase));
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
    res = trx.finish(result.code);
  }
  return res;
}

Result AuthInfo::removeUser(std::string const& user) {
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

    res = RemoveUserInternal(it->second);
    if (res.ok()) {
      _authInfo.erase(it);
      // must also clear the basic cache here because the secret is invalid now
      _authBasicCache.clear();
    }
  }
  reloadAllUsers();
  return res;
}

Result AuthInfo::removeAllUsers() {
  loadFromDB();
  Result res;
  {
    WRITE_LOCKER(guard, _authInfoLock);
    for (auto const& pair : _authInfo) {
      res = RemoveUserInternal(pair.second);
      if (!res.ok()) {
        break;
      }
    }
    {  // do not get into race conditions with loadFromDB
      MUTEX_LOCKER(locker, _loadFromDBLock);
      _authInfo.clear();
      _authBasicCache.clear();
      _outdated = true;
    }
  }
  reloadAllUsers();
  return res;
}

VPackBuilder AuthInfo::getConfigData(std::string const& username) {
  loadFromDB();
  VPackBuilder bb = QueryUser(_queryRegistry, username);
  return bb.isEmpty() ? bb : VPackBuilder(bb.slice().get("configData"));
}

Result AuthInfo::setConfigData(std::string const& user,
                               velocypack::Slice const& data) {
  loadFromDB();

  READ_LOCKER(guard, _authInfoLock);
  auto it = _authInfo.find(user);
  if (it == _authInfo.end()) {
    return Result(TRI_ERROR_USER_NOT_FOUND);
  }
  TRI_ASSERT(!it->second.key().empty());

  VPackBuilder partial;
  partial.openObject();
  partial.add(StaticStrings::KeyString, VPackValue(it->second.key()));
  partial.add("configData", data);
  partial.close();

  return UpdateUser(partial.slice());
  ;
}

VPackBuilder AuthInfo::getUserData(std::string const& username) {
  loadFromDB();
  VPackBuilder bb = QueryUser(_queryRegistry, username);
  return bb.isEmpty() ? bb : VPackBuilder(bb.slice().get("userData"));
}

Result AuthInfo::setUserData(std::string const& user,
                             velocypack::Slice const& data) {
  loadFromDB();

  READ_LOCKER(guard, _authInfoLock);
  auto it = _authInfo.find(user);
  if (it == _authInfo.end()) {
    return Result(TRI_ERROR_USER_NOT_FOUND);
  }
  TRI_ASSERT(!it->second.key().empty());

  VPackBuilder partial;
  partial.openObject();
  partial.add(StaticStrings::KeyString, VPackValue(it->second.key()));
  partial.add("userData", data);
  partial.close();

  return UpdateUser(partial.slice());
}

AuthResult AuthInfo::checkPassword(std::string const& username,
                                   std::string const& password) {
  loadFromDB();

  READ_LOCKER(readLocker, _authInfoLock);
  AuthResult result(username);
  auto it = _authInfo.find(username);

  if (it == _authInfo.end() || (it->second.source() == AuthSource::LDAP)) {
    TRI_ASSERT(_authenticationHandler);
    AuthenticationResult authResult =
        _authenticationHandler->authenticate(username, password);
    if (!authResult.ok()) {
      return result;
    }

    // user authed, add to _authInfo and _users
    if (authResult.source() == AuthSource::LDAP) {
      AuthUserEntry entry =
          AuthUserEntry::newUser(username, password, AuthSource::LDAP);

      // upgrade read-lock to a write-lock
      readLocker.unlock();
      WRITE_LOCKER(writeLocker, _authInfoLock);

      it = _authInfo.find(username);
      if (it != _authInfo.end()) {
        it->second = entry;
      } else {
        auto r = _authInfo.emplace(username, entry);
        if (!r.second) {
          return result;
        }
        it = r.first;
      }
      AuthUserEntry const& auth = it->second;
      if (auth.isActive()) {
        result._authorized = auth.checkPassword(password);
      }
      return result;
    }  // AuthSource::LDAP
  }

  if (it != _authInfo.end()) {
    AuthUserEntry const& auth = it->second;
    if (auth.isActive()) {
      result._authorized = auth.checkPassword(password);
    }
  }
  return result;
}

// public
AuthLevel AuthInfo::canUseDatabase(std::string const& username,
                                   std::string const& dbname) {
  loadFromDB();
  READ_LOCKER(guard, _authInfoLock);
  auto it = _authInfo.find(username);
  if (it != _authInfo.end()) {
    return it->second.databaseAuthLevel(dbname);
  }
  return AuthLevel::NONE;
}

AuthLevel AuthInfo::canUseCollection(std::string const& username,
                                     std::string const& dbname,
                                     std::string const& coll) {
  loadFromDB();
  READ_LOCKER(guard, _authInfoLock);
  auto it = _authInfo.find(username);
  if (it != _authInfo.end()) {
    return it->second.collectionAuthLevel(dbname, coll);
  }
  return AuthLevel::NONE;
}

// public called from HttpCommTask.cpp and VstCommTask.cpp
AuthResult AuthInfo::checkAuthentication(AuthenticationMethod authType,
                                         std::string const& secret) {
  switch (authType) {
    case AuthenticationMethod::BASIC:
      return checkAuthenticationBasic(secret);

    case AuthenticationMethod::JWT:
      return checkAuthenticationJWT(secret);

    default:
      return AuthResult();
  }
}

// private
AuthResult AuthInfo::checkAuthenticationBasic(std::string const& secret) {
  auto role = ServerState::instance()->getRole();
  if (role != ServerState::ROLE_SINGLE &&
      role != ServerState::ROLE_COORDINATOR) {
    return AuthResult();
  }

  {
    READ_LOCKER(guard, _authInfoLock);
    auto const& it = _authBasicCache.find(secret);
    if (it != _authBasicCache.end()) {
      return it->second;
    }
  }

  std::string const up = StringUtils::decodeBase64(secret);
  std::string::size_type n = up.find(':', 0);
  if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "invalid authentication data found, cannot extract "
           "username/password";
    return AuthResult();
  }

  std::string username = up.substr(0, n);
  std::string password = up.substr(n + 1);

  AuthResult result = checkPassword(username, password);
  {
    WRITE_LOCKER(guard, _authInfoLock);

    if (result._authorized) {
      if (!_authBasicCache.emplace(secret, result).second) {
        // insertion did not work - probably another thread did insert the
        // same data right now
        // erase it and re-insert our version
        _authBasicCache.erase(secret);
        _authBasicCache.emplace(secret, result);
      }
    } else {
      _authBasicCache.erase(secret);
    }
  }

  return result;
}

AuthResult AuthInfo::checkAuthenticationJWT(std::string const& jwt) {
  try {
    // note that we need the write lock here because it is an LRU
    // cache. reading from it will move the read entry to the start of
    // the cache's linked list. so acquiring just a read-lock is
    // insufficient!!
    WRITE_LOCKER(writeLocker, _authJwtLock);
    // intentionally copy the entry from the cache
    AuthJwtResult result = _authJwtCache.get(jwt);

    if (result._expires) {
      std::chrono::system_clock::time_point now =
          std::chrono::system_clock::now();

      if (now >= result._expireTime) {
        try {
          _authJwtCache.remove(jwt);
        } catch (std::range_error const&) {
        }
        return AuthResult();
      }
    }
    return (AuthResult)result;
  } catch (std::range_error const&) {
    // mop: not found
  }

  std::vector<std::string> const parts = StringUtils::split(jwt, '.');

  if (parts.size() != 3) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Secret contains "
                                              << parts.size() << " parts";
    return AuthResult();
  }

  std::string const& header = parts[0];
  std::string const& body = parts[1];
  std::string const& signature = parts[2];

  if (!validateJwtHeader(header)) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt header " << header;
    return AuthResult();
  }

  AuthJwtResult result = validateJwtBody(body);
  if (!result._authorized) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt body " << body;
    return AuthResult();
  }

  std::string const message = header + "." + body;

  if (!validateJwtHMAC256Signature(message, signature)) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt signature " << signature << " " << _jwtSecret;
    return AuthResult();
  }

  WRITE_LOCKER(writeLocker, _authJwtLock);
  _authJwtCache.put(jwt, result);
  return (AuthResult)result;
}

std::shared_ptr<VPackBuilder> AuthInfo::parseJson(std::string const& str,
                                                  std::string const& hint) {
  std::shared_ptr<VPackBuilder> result;
  VPackParser parser;
  try {
    parser.parse(str);
    result = parser.steal();
  } catch (std::bad_alloc const&) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Out of memory parsing " << hint
                                            << "!";
  } catch (VPackException const& ex) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Couldn't parse " << hint
                                              << ": " << ex.what();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Got unknown exception trying to parse " << hint;
  }

  return result;
}

bool AuthInfo::validateJwtHeader(std::string const& header) {
  std::shared_ptr<VPackBuilder> headerBuilder =
      parseJson(StringUtils::decodeBase64(header), "jwt header");
  if (headerBuilder.get() == nullptr) {
    return false;
  }

  VPackSlice const headerSlice = headerBuilder->slice();
  if (!headerSlice.isObject()) {
    return false;
  }

  VPackSlice const algSlice = headerSlice.get("alg");
  VPackSlice const typSlice = headerSlice.get("typ");

  if (!algSlice.isString()) {
    return false;
  }

  if (!typSlice.isString()) {
    return false;
  }

  if (algSlice.copyString() != "HS256") {
    return false;
  }

  std::string typ = typSlice.copyString();
  if (typ != "JWT") {
    return false;
  }

  return true;
}

AuthJwtResult AuthInfo::validateJwtBody(std::string const& body) {
  std::shared_ptr<VPackBuilder> bodyBuilder =
      parseJson(StringUtils::decodeBase64(body), "jwt body");
  AuthJwtResult authResult;
  if (bodyBuilder.get() == nullptr) {
    return authResult;
  }

  VPackSlice const bodySlice = bodyBuilder->slice();
  if (!bodySlice.isObject()) {
    return authResult;
  }

  VPackSlice const issSlice = bodySlice.get("iss");
  if (!issSlice.isString()) {
    return authResult;
  }

  if (issSlice.copyString() != "arangodb") {
    return authResult;
  }

  if (bodySlice.hasKey("preferred_username")) {
    VPackSlice const usernameSlice = bodySlice.get("preferred_username");
    if (!usernameSlice.isString()) {
      return authResult;
    }
    authResult._username = usernameSlice.copyString();
  } else if (bodySlice.hasKey("server_id")) {
    // mop: hmm...nothing to do here :D
    // authResult._username = "root";
  } else {
    return authResult;
  }

  // mop: optional exp (cluster currently uses non expiring jwts)
  if (bodySlice.hasKey("exp")) {
    VPackSlice const expSlice = bodySlice.get("exp");

    if (!expSlice.isNumber()) {
      return authResult;
    }

    std::chrono::system_clock::time_point expires(
        std::chrono::seconds(expSlice.getNumber<uint64_t>()));
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    if (now >= expires) {
      return authResult;
    }
    authResult._expires = true;
    authResult._expireTime = expires;
  }

  authResult._authorized = true;
  return authResult;
}

bool AuthInfo::validateJwtHMAC256Signature(std::string const& message,
                                           std::string const& signature) {
  std::string decodedSignature = StringUtils::decodeBase64U(signature);

  return verifyHMAC(_jwtSecret.c_str(), _jwtSecret.length(), message.c_str(),
                    message.length(), decodedSignature.c_str(),
                    decodedSignature.length(),
                    SslInterface::Algorithm::ALGORITHM_SHA256);
}

std::string AuthInfo::generateRawJwt(VPackBuilder const& bodyBuilder) {
  VPackBuilder headerBuilder;
  {
    VPackObjectBuilder h(&headerBuilder);
    headerBuilder.add("alg", VPackValue("HS256"));
    headerBuilder.add("typ", VPackValue("JWT"));
  }

  std::string fullMessage(StringUtils::encodeBase64(headerBuilder.toJson()) +
                          "." +
                          StringUtils::encodeBase64(bodyBuilder.toJson()));

  std::string signature =
      sslHMAC(_jwtSecret.c_str(), _jwtSecret.length(), fullMessage.c_str(),
              fullMessage.length(), SslInterface::Algorithm::ALGORITHM_SHA256);

  return fullMessage + "." + StringUtils::encodeBase64U(signature);
}

std::string AuthInfo::generateJwt(VPackBuilder const& payload) {
  if (!payload.slice().isObject()) {
    std::string error = "Need an object to generate a JWT. Got: ";
    error += payload.slice().typeName();
    throw std::runtime_error(error);
  }
  bool hasIss = payload.slice().hasKey("iss");
  bool hasIat = payload.slice().hasKey("iat");
  VPackBuilder bodyBuilder;
  if (hasIss && hasIat) {
    bodyBuilder = payload;
  } else {
    VPackObjectBuilder p(&bodyBuilder);
    if (!hasIss) {
      bodyBuilder.add("iss", VPackValue("arangodb"));
    }
    if (!hasIat) {
      bodyBuilder.add("iat", VPackValue(TRI_microtime() / 1000));
    }
    for (auto const& obj : VPackObjectIterator(payload.slice())) {
      bodyBuilder.add(obj.key.copyString(), obj.value);
    }
  }
  return generateRawJwt(bodyBuilder);
}
