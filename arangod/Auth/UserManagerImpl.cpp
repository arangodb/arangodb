////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "UserManagerImpl.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryString.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the system database or nullptr on error
////////////////////////////////////////////////////////////////////////////////
arangodb::SystemDatabaseFeature::ptr getSystemDatabase(
    arangodb::ArangodServer& server) {
  if (!server.hasFeature<arangodb::SystemDatabaseFeature>()) {
    LOG_TOPIC("607b8", WARN, arangodb::Logger::AUTHENTICATION)
        << "failure to find feature '"
        << arangodb::SystemDatabaseFeature::name()
        << "' while getting the system database";

    return nullptr;
  }
  return server.getFeature<arangodb::SystemDatabaseFeature>().use();
}

}  // namespace

using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

namespace arangodb::auth {

UserManagerImpl::UserManagerImpl(ArangodServer& server)
    : _server(server), _globalVersion(1), _internalVersion(0) {}

UserManagerImpl::~UserManagerImpl() { shutdown(); }

// Parse the users
static UserMap ParseUsers(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());
  UserMap result;
  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    VPackSlice s = authSlice.resolveExternal();

    // we also need to insert inactive users into the cache here
    // otherwise all following update/replace/remove operations on the
    // user will fail
    User user = User::fromDocument(s);
    // intentional copy, as we are about to move-away from user
    std::string username = user.username();
    result.try_emplace(std::move(username), std::move(user));
  }
  return result;
}

static std::shared_ptr<VPackBuilder> QueryAllUsers(ArangodServer& server) {
  auto vocbase = getSystemDatabase(server);

  if (vocbase == nullptr) {
    LOG_TOPIC("b8c47", DEBUG, Logger::AUTHENTICATION)
        << "system database is unknown";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "system database is unknown");
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextSuperuserScope scope;
  std::string const queryStr("FOR user IN _users RETURN user");
  auto origin =
      transaction::OperationOriginInternal{"querying all users from database"};
  auto query = aql::Query::create(
      transaction::StandaloneContext::create(*vocbase, origin),
      aql::QueryString(queryStr), nullptr);

  query->queryOptions().cache = false;
  query->queryOptions().ttl = 30;
  query->queryOptions().maxRuntime = 30;
  query->queryOptions().skipAudit = true;

  LOG_TOPIC("f3eec", DEBUG, Logger::AUTHENTICATION)
      << "starting to load authentication and authorization information";

  aql::QueryResult queryResult = query->executeSync();

  if (queryResult.result.fail()) {
    if (queryResult.result.is(TRI_ERROR_REQUEST_CANCELED) ||
        (queryResult.result.is(TRI_ERROR_QUERY_KILLED))) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        queryResult.result.errorNumber(),
        StringUtils::concatT("Error executing user query: ",
                             queryResult.result.errorMessage()));
  }

  VPackSlice usersSlice = queryResult.data->slice();

  if (usersSlice.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  if (!usersSlice.isArray()) {
    LOG_TOPIC("4b11d", ERR, Logger::AUTHENTICATION)
        << "cannot read users from _users collection";
    return {};
  }

  return queryResult.data;
}

/// Convert documents from _system/_users into the format used in
/// the REST user API and Foxx
static void ConvertLegacyFormat(VPackSlice doc, VPackBuilder& result) {
  doc = doc.resolveExternals();
  VPackSlice authDataSlice = doc.get("authData");
  {
    VPackObjectBuilder b(&result, true);
    result.add("user", doc.get("user"));
    result.add("active", authDataSlice.get("active"));
    VPackSlice extra = doc.get("userData");
    result.add("extra",
               extra.isNone() ? VPackSlice::emptyObjectSlice() : extra);
  }
}

void UserManagerImpl::loadUserCacheAndStartUpdateThread() noexcept {
  TRI_ASSERT(ServerState::instance()->isSingleServerOrCoordinator());

  if (_userCacheUpdateThread.get_id() != std::jthread::id()) {
    // thread already started
    return;
  }

  namespace chrono = std::chrono;
  LOG_TOPIC("ef78c", INFO, Logger::AUTHENTICATION) << "Preloading user cache";
  auto start = chrono::system_clock::now();
  while (loadFromDB() == 0) {
    auto const now = chrono::system_clock::now();
    if ((now - start) > std::chrono::seconds(3)) {
      start = chrono::system_clock::now();
      LOG_TOPIC("ef78e", INFO, Logger::AUTHENTICATION)
          << "Preloading user cache is still in progress.";
    }
  }

  _userCacheUpdateThread =
      std::jthread([this](std::stop_token stpTkn) noexcept {
        uint32_t tries = 0;
        auto start = chrono::system_clock::now();
        while (!stpTkn.stop_requested()) {
          uint64_t const loadedVersion = loadFromDB();
          // In case of an error while loading the _user collection we do not
          // want to retry too often to prevent additional load on the whole
          // server/cluster
          if (loadedVersion == 0) {
            tries++;
            auto const now = chrono::system_clock::now();
            if (tries == 1) {
              LOG_TOPIC("dfba8", INFO, Logger::AUTHENTICATION)
                  << "Loading user cache failed, retrying";
              start = chrono::system_clock::now();
            } else if (now - start > chrono::seconds(2)) {
              LOG_TOPIC("dfbb9", WARN, Logger::AUTHENTICATION)
                  << "Loading user cache is still failing, retrying for the "
                  << tries << " time.";
              start = chrono::system_clock::now();
            }
            // The wait time between retries get longer and longer up until a
            // maximum of ~10sec in between tries.
            uint32_t const multiplier = 1u << std::min(tries, 20u);
            using namespace std::chrono_literals;
            {  // sleep for "10us * multiplier", but interruptible by the stop
              // token.
              auto mutex = std::mutex{};
              auto cv = std::condition_variable{};
              auto lock = std::unique_lock(mutex);
              auto cb = std::stop_callback(stpTkn, [&]() noexcept {
                // We lock and unlock here to make sure that we do not call a
                // notify while the conditional_variable is currently woken-up
                // and checking the predicate. This prevents any sleeping barber
                // scenarios here. If the lock is successful the cv has not yet
                // woken up but stop_requested is already true If the lock is
                // unsuccessful (blocks) we wait here until the cv is finished
                // with checking the predicate, and we can wake it up again
                // after it is sleeping/waiting again.
                mutex.lock();
                mutex.unlock();
                cv.notify_one();
              });
              // The wake-up + lock and the sleep + unlock is guaranteed to
              // happen atomically
              cv.wait_for(lock, 10us * multiplier,
                          [&] { return stpTkn.stop_requested(); });
            }
          } else {
            // load was successful reset tries
            tries = 0;
          }
          _globalVersion.wait(loadedVersion);
        }
      });
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(_userCacheUpdateThread.native_handle(), "UserCacheThread");
#endif
}

// private, will acquire _userCacheLock in write-mode and release it.
uint64_t UserManagerImpl::loadFromDB() noexcept {
  uint64_t const currentGlobalVersion = globalVersion();

  auto const setInternalVersion = [&](uint64_t const version) {
    auto const old =
        _internalVersion.exchange(version, std::memory_order_release);
    TRI_ASSERT(old <= version);
    _internalVersion.notify_all();
  };

  TRI_IF_FAILURE("UserManager::performDBLookup") {
    // Used in GTest. Will simulate a successful loadFromDB.
    setInternalVersion(currentGlobalVersion);
    return currentGlobalVersion;
  }

  try {
    std::shared_ptr<VPackBuilder> builder = QueryAllUsers(_server);
    if (builder) {
      VPackSlice usersSlice = builder->slice();
      if (usersSlice.length() != 0) {
        UserMap userMap = ParseUsers(usersSlice);
        {
          WRITE_LOCKER(writeGuard, _userCacheLock);
          _userCache.swap(userMap);
        }
      }
      setInternalVersion(currentGlobalVersion);
      return currentGlobalVersion;
    }
  } catch (basics::Exception const& ex) {
    LOG_TOPIC("aa45c", WARN, Logger::AUTHENTICATION)
        << "Exception when loading users from db: " << ex.what();
  } catch (std::exception const& ex) {
    LOG_TOPIC("b7342", WARN, Logger::AUTHENTICATION)
        << "Exception when loading users from db: " << ex.what();
  } catch (...) {
    LOG_TOPIC("3f537", TRACE, Logger::AUTHENTICATION)
        << "Exception when loading users from db";
  }
  return 0u;
}

void UserManagerImpl::checkIfUserDataIsAvailable() const {
  bool const noDataYetLoaded =
      _internalVersion.load(std::memory_order_relaxed) == 0;
  if (noDataYetLoaded) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_STARTING_UP,
                                   "Cannot load users because the _users "
                                   "collection is not yet available");
  }
}

// private, must be called with _userCacheLock in write mode
// this method can only be called by users with access to the _system collection
Result UserManagerImpl::storeUserInternal(User const& entry,
                                          bool const replace) {
  VPackBuilder data = entry.toVPackBuilder();
  bool const hasKey = data.slice().hasKey(StaticStrings::KeyString);
  bool const hasRev = data.slice().hasKey(StaticStrings::RevString);
  TRI_ASSERT((replace && hasKey && hasRev) || (!replace && !hasKey && !hasRev));

  auto vocbase = getSystemDatabase(_server);

  if (vocbase == nullptr) {
    return Result(TRI_ERROR_INTERNAL, "unable to find system database");
  }

  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextSuperuserScope scope;
  auto origin = transaction::OperationOriginInternal{"storing user"};
  auto ctx = transaction::StandaloneContext::create(*vocbase, origin);
  SingleCollectionTransaction trx(ctx, StaticStrings::UsersCollection,
                                  AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (res.ok()) {
    OperationOptions opts;

    opts.returnNew = true;
    opts.ignoreRevs = false;
    opts.mergeObjects = false;

    OperationResult opres =
        replace
            ? trx.replace(StaticStrings::UsersCollection, data.slice(), opts)
            : trx.insert(StaticStrings::UsersCollection, data.slice(), opts);

    res = trx.finish(opres.result);

    if (res.is(TRI_ERROR_ARANGO_CONFLICT)) {
      // user was outdated, we should trigger a reload
      LOG_TOPIC("cf922", DEBUG, Logger::AUTHENTICATION)
          << "Cannot update user : '" << res.errorMessage() << "'";
    }
  }
  return res;
}

// -----------------------------------------------------------------------------
// -- SECTION --                                                          public
// -----------------------------------------------------------------------------

// only call from the boostrap feature, must be sure to be the only one
void UserManagerImpl::createRootUser() {
  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find("root");
  if (it != _userCache.end()) {
    LOG_TOPIC("bbc97", TRACE, Logger::AUTHENTICATION)
        << "\"root\" already exists";
    return;
  }
  TRI_ASSERT(_userCache.empty());
  LOG_TOPIC("857d7", DEBUG, Logger::AUTHENTICATION) << "Creating user \"root\"";

  try {
    // Attention:
    // the root user needs to have a specific rights grant
    // to the "_system" database, otherwise things break
    auto& initDatabaseFeature = _server.getFeature<InitDatabaseFeature>();

    User user = User::newUser("root", initDatabaseFeature.defaultPassword());
    user.setActive(true);
    user.grantDatabase(StaticStrings::SystemDatabase, Level::RW);
    user.grantCollection(StaticStrings::SystemDatabase, "*", Level::RW);
    user.grantDatabase("*", Level::RW);
    user.grantCollection("*", "*", Level::RW);
    storeUserInternal(user, false);
  } catch (std::exception const& ex) {
    LOG_TOPIC("0511c", ERR, Logger::AUTHENTICATION)
        << "unable to create user \"root\": " << ex.what();
  } catch (...) {
    // No action
    LOG_TOPIC("268eb", ERR, Logger::AUTHENTICATION)
        << "unable to create user \"root\"";
  }
  readGuard.unlock();

  triggerGlobalReload();
}

VPackBuilder UserManagerImpl::allUsers() {
  // will query db directly, no need for _userCacheLock
  std::shared_ptr<VPackBuilder> users = QueryAllUsers(_server);

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

void UserManagerImpl::triggerCacheRevalidation() {
  // Internally this function is called after a successful write/update to
  // ensure that the changed data is loaded into the userCache. Before return.
  uint64_t const versionBeforeReload = globalVersion();
  triggerGlobalReload();
  // We triggered a globalReload and can predict that at some point
  // the HeartBeat will lead to an increment of the globalVersion through
  // `setGlobalVersion`. So we just do it manually here.
  setGlobalVersion(versionBeforeReload + 1);

  // After we increased the globalVersion we now have a minimal version that we
  // want the internalVersion to be, so we now we wait.
  uint64_t const versionAfterReload = versionBeforeReload + 1;
  uint64_t currentInternalVersion =
      _internalVersion.load(std::memory_order_acquire);
  while (currentInternalVersion < versionAfterReload) {
    _internalVersion.wait(versionBeforeReload);
    currentInternalVersion = _internalVersion.load(std::memory_order_acquire);
  }

  // So basically:
  // 1. We changed something on a user
  // 2. We trigger a globalReload (Increment in Agency)
  // 3. We overtake the HeartBeat - thread by calling setGlobalVersion manually
  // 4. We wait until the internalVersion is at least the same as the
  //    globalVersion that we called `setGlobalVersion` with.
  // This makes sure that loadFromDB was called sometime after this function
  // was called and before it returns.
}

void UserManagerImpl::setGlobalVersion(uint64_t const version) noexcept {
  uint64_t previous = _globalVersion.load(std::memory_order_relaxed);
  while (version > previous) {
    if (_globalVersion.compare_exchange_strong(previous, version,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
      _globalVersion.notify_all();
      return;
    }
  }
  // we were not able to update the _globalVersion
}

/// @brief used for caching
uint64_t UserManagerImpl::globalVersion() const noexcept {
  return _globalVersion.load(std::memory_order_acquire);
}

/// Trigger eventual reload, user facing API call
void UserManagerImpl::triggerGlobalReload() const {
  if (!ServerState::instance()->isCoordinator()) {
    return;
  }
  // tell other coordinators to reload as well
  AgencyComm agency(_server);
  AgencyWriteTransaction incrementVersion({AgencyOperation(
      "Sync/UserVersion", AgencySimpleOperationType::INCREMENT_OP)});

  AgencyCommResult const result =
      agency.sendTransactionWithFailover(incrementVersion);

  if (!result.successful()) {
    LOG_TOPIC("d2f51", WARN, Logger::AUTHENTICATION)
        << "Sync/UserVersion could not be updated. " << result.errorMessage();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "could not update global version");
  }
}

Result UserManagerImpl::storeUser(bool const replace,
                                  std::string const& username,
                                  std::string const& pass, bool const active,
                                  VPackSlice extras) {
  if (username.empty()) {
    return TRI_ERROR_USER_INVALID_NAME;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(username);

  if (replace && it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  } else if (!replace && it != _userCache.end()) {
    return TRI_ERROR_USER_DUPLICATE;
  }

  std::string oldKey;  // will only be populated during replace
  RevisionId oldRev = RevisionId::none();
  if (replace) {
    User const& oldEntry = it->second;
    oldKey = oldEntry.key();
    oldRev = oldEntry.rev();
  }

  User user = User::newUser(username, pass);
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
  readGuard.unlock();
  if (r.ok()) {
    triggerCacheRevalidation();
  }
  return r;
}

Result UserManagerImpl::enumerateUsers(std::function<bool(User&)>&& func,
                                       bool const retryOnConflict) {
  checkIfUserDataIsAvailable();

  std::vector<User> toUpdate;
  {  // users are later updated with rev ID for consistency
    READ_LOCKER(readGuard, _userCacheLock);
    for (UserMap::value_type& it : _userCache) {
      User user = it.second;  // copy user object
      TRI_ASSERT(!user.key().empty() && user.rev().isSet());
      if (func(user)) {
        toUpdate.emplace_back(std::move(user));
      }
    }
  }

  bool triggerUpdate = !toUpdate.empty();

  Result res;
  do {
    auto it = toUpdate.begin();
    while (it != toUpdate.end()) {
      res = storeUserInternal(*it, /*replace*/ true);
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && retryOnConflict) {
        res.reset();
        // We ran into a conflict, and we have to retry
        // so we either wait for the update thread to re-run/finish, which is
        // blocking anyway so we can also just reload synchronously here
        // directly.
        loadFromDB();
        READ_LOCKER(readGuard, _userCacheLock);
        UserMap::iterator it2 = _userCache.find(it->username());
        if (it2 != _userCache.end()) {
          User user = it2->second;  // copy user object
          func(user);
          *it = std::move(user);
          continue;
        }
      } else if (res.fail()) {
        break;  // do not return, still need to invalidate token cache
      }
      it = toUpdate.erase(it);
    }
  } while (!toUpdate.empty() && res.ok() && !_server.isStopping());

  // cannot hold _userCacheLock while  invalidating token cache
  if (triggerUpdate) {
    triggerCacheRevalidation();
  }
  return res;
}

Result UserManagerImpl::updateUser(std::string const& name,
                                   UserCallback&& func) {
  if (name.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  checkIfUserDataIsAvailable();

  // we require a consistent view on the user object
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator it = _userCache.find(name);
  if (it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  LOG_TOPIC("574c5", DEBUG, Logger::AUTHENTICATION) << "Updating user " << name;
  User user = it->second;  // make a copy
  TRI_ASSERT(!user.key().empty() && user.rev().isSet());
  Result r = func(user);
  if (r.fail()) {
    return r;
  }
  r = storeUserInternal(user, /*replace*/ true);

  // cannot hold _userCacheLock while  invalidating token cache
  readGuard.unlock();
  if (r.ok() || r.is(TRI_ERROR_ARANGO_CONFLICT)) {
    // must also clear the basic cache here because the secret may be
    // invalid now if the password was changed
    triggerCacheRevalidation();
  }
  return r;
}

Result UserManagerImpl::accessUser(std::string const& user,
                                   ConstUserCallback&& func) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  checkIfUserDataIsAvailable();

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  if (it != _userCache.end()) {
    return func(it->second);
  }
  return TRI_ERROR_USER_NOT_FOUND;
}

bool UserManagerImpl::userExists(std::string const& user) {
  if (user.empty()) {
    return false;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  return it != _userCache.end();
}

VPackBuilder UserManagerImpl::serializeUser(std::string const& user) {
  checkIfUserDataIsAvailable();

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

static Result RemoveUserInternal(ArangodServer& server, User const& entry) {
  TRI_ASSERT(!entry.key().empty());
  auto vocbase = getSystemDatabase(server);

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
  ExecContextSuperuserScope scope;
  auto origin = transaction::OperationOriginInternal{"removing user"};
  auto ctx = transaction::StandaloneContext::create(*vocbase, origin);
  SingleCollectionTransaction trx(ctx, StaticStrings::UsersCollection,
                                  AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (res.ok()) {
    OperationResult result = trx.remove(StaticStrings::UsersCollection,
                                        builder.slice(), OperationOptions());
    res = trx.finish(result.result);
  }

  return res;
}

Result UserManagerImpl::removeUser(std::string const& user) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  if (user == "root") {
    return TRI_ERROR_FORBIDDEN;
  }

  checkIfUserDataIsAvailable();

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC("07aaf", TRACE, Logger::AUTHORIZATION)
        << "User not found: " << user;
    return TRI_ERROR_USER_NOT_FOUND;
  }

  User const& oldEntry = it->second;
  Result res = RemoveUserInternal(_server, oldEntry);

  // cannot hold _userCacheLock while  invalidating token cache
  readGuard.unlock();
  triggerCacheRevalidation();

  return res;
}

Result UserManagerImpl::removeAllUsers() {
  checkIfUserDataIsAvailable();

  Result res;
  {
    WRITE_LOCKER(writeGuard, _userCacheLock);

    for (auto pair = _userCache.cbegin(); pair != _userCache.cend();) {
      auto const& oldEntry = pair->second;
#ifdef ARANGODB_USE_GOOGLE_TESTS
      if (oldEntry.key().empty()) {
        // we expect no empty usernames to ever occur, except when
        // called from unit tests
        ++pair;
      } else
#endif
      {
        res = RemoveUserInternal(_server, oldEntry);
        if (!res.ok()) {
          break;  // don't return still need to invalidate token cache
        }
        pair = _userCache.erase(pair);
      }
    }
  }

  triggerCacheRevalidation();
  return res;
}

Result UserManagerImpl::accessTokens(std::string const& user,
                                     velocypack::Builder& builder) {
  Result result = accessUser(
      user, [&](User const& u) { return u.getAccessTokens(builder); });

  return result;
}

Result UserManagerImpl::deleteAccessToken(std::string const& user,
                                          uint64_t id) {
  Result result =
      updateUser(user, [&](User& u) { return u.deleteAccessToken(id); });

  return result;
}

Result UserManagerImpl::createAccessToken(std::string const& user,
                                          std::string const& name,
                                          double validUntil,
                                          velocypack::Builder& builder) {
  Result result = updateUser(user, [&](User& u) {
    return u.createAccessToken(name, validUntil, builder);
  });

  return result;
}

bool UserManagerImpl::checkPassword(std::string const& username,
                                    std::string const& password) {
  if (username.empty()) {
    return false;  // we cannot authenticate during bootstrap
  }

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator it = _userCache.find(username);

  if (it != _userCache.end()) {
    User const& user = it->second;
    if (user.isActive()) {
      return user.checkPassword(password);
    }
  }

  return false;
}

Result UserManagerImpl::extractUsername(std::string const& token,
                                        std::string& username) const {
  if (token.starts_with("v1.")) {
    std::string unhex =
        StringUtils::decodeHex(token.c_str() + 3, token.size() - 3);

    StringBuffer in;
    in.appendText(unhex);

    auto json = VPackParser::fromJson(in.toString());
    VPackSlice at = json->slice();

    if (!at.isObject()) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    VPackSlice user = at.get("u");

    if (!user.isString()) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    username = user.copyString();

    return {TRI_ERROR_NO_ERROR};
  } else {
    return {TRI_ERROR_INCOMPATIBLE_VERSION};
  }
}

bool UserManagerImpl::checkAccessToken(std::string const& username,
                                       std::string const& token,
                                       std::string& un) {
  Result result = extractUsername(token, un);

  if (!result.ok()) {
    return false;
  }

  if (!username.empty() && username != un) {
    return false;
  }

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator it = _userCache.find(un);

  if (it != _userCache.end()) {
    User const& user = it->second;
    if (user.isActive()) {
      return user.checkAccessToken(token);
    }
  }

  return false;
}

bool UserManagerImpl::checkCredentials(std::string const& username,
                                       std::string const& password,
                                       std::string& un) {
  un.clear();
  bool authorized = !username.empty() && checkPassword(username, password);

  if (authorized) {
    un = username;
  } else {
    authorized = checkAccessToken(username, password, un);
  }

  return authorized;
}

Level UserManagerImpl::databaseAuthLevel(std::string const& user,
                                         std::string const& dbname,
                                         bool configured) {
  if (dbname.empty()) {
    return Level::NONE;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC("aa27c", TRACE, Logger::AUTHORIZATION)
        << "User not found: " << user;
    return Level::NONE;
  }

  Level level = it->second.databaseAuthLevel(dbname);
  if (!configured) {
    if (level > Level::RO && ServerState::readOnly()) {
      return Level::RO;
    }
  }
  TRI_ASSERT(level != Level::UNDEFINED);  // not allowed here
  return level;
}

Level UserManagerImpl::collectionAuthLevel(std::string const& user,
                                           std::string const& dbname,
                                           std::string_view coll,
                                           bool configured) {
  if (coll.empty()) {
    return Level::NONE;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC("6d0d4", TRACE, Logger::AUTHORIZATION)
        << "User not found: " << user;
    return Level::NONE;  // no user found
  }

  TRI_ASSERT(!coll.empty());
  Level level;
  if (coll[0] >= '0' && coll[0] <= '9') {
    std::string tmpColl =
        _server.getFeature<DatabaseFeature>().translateCollectionName(dbname,
                                                                      coll);
    level = it->second.collectionAuthLevel(dbname, tmpColl);
  } else {
    level = it->second.collectionAuthLevel(dbname, coll);
  }

  if (!configured) {
    static_assert(Level::RO < Level::RW, "ro < rw");
    if (level > Level::RO && ServerState::readOnly()) {
      return Level::RO;
    }
  }
  TRI_ASSERT(level != Level::UNDEFINED);  // not allowed here
  return level;
}

void UserManagerImpl::shutdown() {
  if (_userCacheUpdateThread.joinable()) {
    _userCacheUpdateThread.request_stop();
    // set global version leads to a wake-up of the update thread
    setGlobalVersion(std::numeric_limits<uint64_t>::max());
    _userCacheUpdateThread.join();
  }
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
/// Only used for testing
void UserManagerImpl::setAuthInfo(UserMap const& newMap) {
  WRITE_LOCKER(writeGuard, _userCacheLock);
  _userCache = newMap;
  writeGuard.unlock();
  auto const currentGlobalVersion = globalVersion();
  setGlobalVersion(currentGlobalVersion + 1);
  _internalVersion.store(currentGlobalVersion + 1);
}

uint64_t UserManagerImpl::internalVersion() const noexcept {
  return _internalVersion.load(std::memory_order_acquire);
}

#endif
}  // namespace arangodb::auth
