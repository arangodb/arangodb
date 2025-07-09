////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Common.h"
#include "Rest/RequestContext.h"

#include <memory>
#include <string>

namespace arangodb {
namespace transaction {
class Methods;
}

/// @brief Carries execution context information for the current thread
///
/// This class represents the execution context that carries information about
/// the current user, database, and authorization levels for a thread. It serves
/// as a central place to access authentication and authorization information
/// during request processing. The context should always be accessible from
/// ExecContext::CURRENT and inherits from RequestContext for convenience.
///
/// The execution context manages:
/// - Current user identity and authentication status
/// - Database access permissions and authorization levels
/// - System-level and database-level access rights
/// - Administrative privileges and internal operation flags
///
/// @note This context is thread-local and should be properly managed using
///       scope guards to ensure correct context switching
/// @note Internal contexts bypass normal permission checking
class ExecContext : public RequestContext {
  friend struct ExecContextScope;
  friend struct ExecContextSuperuserScope;

 protected:
  /// @brief Type of execution context
  ///
  /// Distinguishes between regular user contexts and internal system contexts
  /// that bypass normal authentication and authorization checks.
  enum class Type { Default, Internal };
  class ConstructorToken {};

 public:
  /// @brief Construct an execution context with specific parameters
  ///
  /// Creates a new execution context with the specified user, database, and
  /// authorization levels. This constructor is protected by a token pattern
  /// to ensure proper factory usage.
  ///
  /// @param token Constructor access token (prevents direct instantiation)
  /// @param type Context type (Default for users, Internal for system operations)
  /// @param user Username for this context (may be empty for internal contexts)
  /// @param database Database name for this context
  /// @param systemLevel Authorization level for _system database operations
  /// @param dbLevel Authorization level for the specified database
  /// @param isAdminUser Whether this user has administrative privileges
  ///
  /// @note Use factory methods like create() instead of calling directly
  ExecContext(ConstructorToken, ExecContext::Type type, std::string const& user,
              std::string const& database, auth::Level systemLevel,
              auth::Level dbLevel, bool isAdminUser);
  ExecContext(ExecContext const&) = delete;
  ExecContext(ExecContext&&) = delete;

 public:
  virtual ~ExecContext() = default;

  /// @brief Check if authentication is enabled globally
  ///
  /// Provides a convenience method to check if the AuthenticationFeature
  /// is active in the current server configuration.
  ///
  /// @return true if authentication is enabled, false otherwise
  static bool isAuthEnabled();

  /// @brief Get the current thread's execution context
  ///
  /// Returns a reference to the execution context for the current thread.
  /// This should always contain a valid reference during request processing.
  ///
  /// @return Reference to the current execution context
  ///
  /// @note Throws if no context is set for the current thread
  /// @note This is the primary way to access context information
  static ExecContext const& current();

  /// @brief Get the current thread's execution context as shared pointer
  ///
  /// Returns the execution context as a shared pointer, which can be null
  /// if no context is currently set. This version is suitable for setting
  /// CURRENT in another thread context.
  ///
  /// @return Shared pointer to current execution context (may be nullptr)
  static std::shared_ptr<ExecContext const> currentAsShared();

  /// @brief Get the singleton superuser context
  ///
  /// Returns a reference to the internal superuser context that has
  /// unrestricted access to all operations and databases. This is a
  /// singleton instance that should not be deleted.
  ///
  /// @return Reference to the superuser execution context
  ///
  /// @note This context bypasses all authentication and authorization checks
  /// @note Used for internal system operations that require elevated privileges
  static ExecContext const& superuser();

  /// @brief Get the singleton superuser context as shared pointer
  ///
  /// Returns the superuser context as a shared pointer for use in contexts
  /// that require shared ownership semantics.
  ///
  /// @return Shared pointer to superuser execution context
  static std::shared_ptr<ExecContext const> superuserAsShared();

  /// @brief Create a new user execution context
  ///
  /// Factory method to create an execution context for a specific user and
  /// database. This method performs authentication and authorization lookups
  /// to determine the appropriate access levels.
  ///
  /// @param user Username to create context for
  /// @param db Database name for the context
  /// @return Shared pointer to the created execution context
  ///
  /// @note This method queries the UserManager for current permissions
  /// @note Context creation may fail if user doesn't exist or has no access
  static std::shared_ptr<ExecContext> create(std::string const& user,
                                             std::string const& db);

  /// @brief Check if this is an internal system context
  ///
  /// Internal contexts are used for system operations and bypass normal
  /// permission resolution. They are typically used for maintenance tasks,
  /// internal replication, and other system-level operations.
  ///
  /// @return true if this is an internal context, false otherwise
  ///
  /// @note Internal contexts override further permission resolution
  bool isInternal() const noexcept { return _type == Type::Internal; }

  /// @brief Check if this context has superuser privileges
  ///
  /// A superuser is any internal operation that has read-write access to
  /// both the system database and the current database context.
  ///
  /// @return true if this context has superuser privileges, false otherwise
  ///
  /// @note Only internal contexts can be superusers
  /// @note Superusers have unrestricted access to all operations
  bool isSuperuser() const noexcept {
    return isInternal() && _systemDbAuthLevel == auth::Level::RW &&
           _databaseAuthLevel == auth::Level::RW;
  }

  /// @brief Check if this is an internal read-only context
  ///
  /// Determines if this context is an internal system context with
  /// read-only permissions, typically used for read-only system operations.
  ///
  /// @return true if this is an internal read-only context, false otherwise
  bool isReadOnly() const noexcept {
    return isInternal() && _systemDbAuthLevel == auth::Level::RO;
  }

  /// @brief Check if this user has administrative privileges
  ///
  /// Administrative users can manage other users, create databases, and
  /// perform other administrative operations. This is determined by having
  /// read-write access to the _system database.
  ///
  /// @return true if this user has administrative privileges, false otherwise
  ///
  /// @note Admin status is independent of cluster read-only mode
  bool isAdminUser() const noexcept { return _isAdminUser; }

  /// @brief Check if the current execution has been canceled
  ///
  /// Allows checking whether the current operation should be aborted due to
  /// cancellation. The base implementation always returns false, but
  /// subclasses may override this to provide cancellation support.
  ///
  /// @return true if execution has been canceled, false otherwise
  ///
  /// @note Override in subclasses to provide actual cancellation logic
  virtual bool isCanceled() const { return false; }

  /// @brief Get the current user name
  ///
  /// Returns the username associated with this execution context.
  /// May be empty for internal system contexts.
  ///
  /// @return Username string (may be empty for internal users)
  std::string const& user() const { return _user; }

  /// @brief Get the current database name
  ///
  /// Returns the database name that this execution context is bound to.
  /// This represents the target database for operations in this context.
  ///
  /// @return Database name string
  std::string const& database() const { return _database; }

  /// @brief Get the authentication level for the _system database
  ///
  /// Returns the authorization level this context has for operations on
  /// the _system database. Always RW for superuser contexts.
  ///
  /// @return Authorization level for _system database
  ///
  /// @note _system database access determines administrative privileges
  auth::Level systemAuthLevel() const noexcept { return _systemDbAuthLevel; }

  /// @brief Get the authentication level for the current database
  ///
  /// Returns the authorization level for the database selected in the current
  /// request scope. Should almost always contain something meaningful if the
  /// thread originated from v8 or HTTP request processing.
  ///
  /// @return Authorization level for current database
  auth::Level databaseAuthLevel() const noexcept { return _databaseAuthLevel; }

  /// @brief Check if the current database can be used with requested access level
  ///
  /// Verifies whether the current execution context has sufficient privileges
  /// to access the current database with the specified access level.
  ///
  /// @param requested Minimum required authorization level
  /// @return true if access is allowed, false otherwise
  ///
  /// @note Access is granted if the current level is >= requested level
  bool canUseDatabase(auth::Level requested) const noexcept {
    return requested <= _databaseAuthLevel;
  }

  /// @brief Check if a specific database can be used with requested access level
  ///
  /// Verifies whether the current execution context has sufficient privileges
  /// to access the specified database with the given access level. This may
  /// involve querying the UserManager for permission information.
  ///
  /// @param db Database name to check access for
  /// @param requested Minimum required authorization level
  /// @return true if access is allowed, false otherwise
  ///
  /// @note For internal contexts, uses the internal authorization level
  /// @note For external contexts, queries UserManager for permissions
  bool canUseDatabase(std::string const& db, auth::Level requested) const;

  /// @brief Get authorization level for a specific collection
  ///
  /// Determines the authorization level this context has for operations on
  /// the specified collection within the given database.
  ///
  /// @param dbname Database name containing the collection
  /// @param collection Collection name or identifier
  /// @return Authorization level for the collection
  ///
  /// @note For internal contexts, returns the database authorization level
  /// @note For external contexts, queries UserManager for collection permissions
  /// @note Handles special cases for system collections
  auth::Level collectionAuthLevel(std::string const& dbname,
                                  std::string_view collection) const;

  /// @brief Check if a collection in current database can be used with requested access level
  ///
  /// Convenience method to check collection access within the current database context.
  ///
  /// @param collection Collection name to check access for
  /// @param requested Minimum required authorization level
  /// @return true if access is allowed, false otherwise
  bool canUseCollection(std::string const& collection,
                        auth::Level requested) const {
    return canUseCollection(_database, collection, requested);
  }

  /// @brief Check if a collection in specific database can be used with requested access level
  ///
  /// Verifies whether the current execution context has sufficient privileges
  /// to access the specified collection with the given access level.
  ///
  /// @param db Database name containing the collection
  /// @param coll Collection name to check access for
  /// @param requested Minimum required authorization level
  /// @return true if access is allowed, false otherwise
  bool canUseCollection(std::string const& db, std::string const& coll,
                        auth::Level requested) const {
    return requested <= collectionAuthLevel(db, coll);
  }

  /// @brief Set a new execution context for the current thread
  ///
  /// Replaces the current thread's execution context with the provided one
  /// and returns the previous context. This is used for context switching
  /// during request processing.
  ///
  /// @param ctx New execution context to set
  /// @return Previous execution context (may be nullptr)
  ///
  /// @note Caller is responsible for proper context management
  /// @note Consider using ExecContextScope for automatic cleanup
  static std::shared_ptr<ExecContext const> set(
      std::shared_ptr<ExecContext const> ctx) {
    auto tmp = CURRENT;
    CURRENT = ctx;
    return tmp;
  }

#ifdef USE_ENTERPRISE
  /// @brief Get client address for this execution context
  ///
  /// Returns the client address associated with this execution context.
  /// Enterprise edition feature that may be used for auditing and logging.
  ///
  /// @return Client address string (empty in base implementation)
  virtual std::string clientAddress() const { return ""; }

  /// @brief Get request URL for this execution context
  ///
  /// Returns the request URL associated with this execution context.
  /// Enterprise edition feature that may be used for auditing and logging.
  ///
  /// @return Request URL string (empty in base implementation)
  virtual std::string requestUrl() const { return ""; }

  /// @brief Get authentication method for this execution context
  ///
  /// Returns the authentication method used for this execution context.
  /// Enterprise edition feature that may be used for auditing and logging.
  ///
  /// @return Authentication method string (empty in base implementation)
  virtual std::string authMethod() const { return ""; }
#endif

 protected:
  /// current user, may be empty for internal users
  std::string const _user;
  /// current database to use, superuser db is empty
  std::string const _database;

  Type _type;
  /// Flag if admin user access (not regarding cluster RO mode)
  bool _isAdminUser;
  /// level of system database
  auth::Level _systemDbAuthLevel;
  /// level of current database
  auth::Level _databaseAuthLevel;

 private:
  static std::shared_ptr<ExecContext const> const Superuser;
  static thread_local std::shared_ptr<ExecContext const> CURRENT;
};

/// @brief RAII scope guard for execution context management
///
/// Provides automatic management of execution context switching using RAII
/// principles. When constructed, it sets a new execution context and
/// automatically restores the previous context when destroyed.
///
/// This ensures proper context cleanup even if exceptions are thrown
/// during request processing.
///
/// @note This is the recommended way to manage execution context switching
/// @note The guard maintains the previous context and restores it on destruction
///
/// Example:
/// @code
/// auto newContext = ExecContext::create("user", "database");
/// {
///   ExecContextScope guard(newContext);
///   // Operations here use newContext
///   // ... perform operations ...
/// } // Previous context automatically restored here
/// @endcode
struct ExecContextScope {
  /// @brief Construct scope guard and set new execution context
  ///
  /// Sets the provided execution context as current and stores the
  /// previous context for later restoration.
  ///
  /// @param exe New execution context to set as current
  explicit ExecContextScope(std::shared_ptr<ExecContext const> exe);

  /// @brief Destructor that restores the previous execution context
  ///
  /// Automatically restores the execution context that was active
  /// before this scope guard was created.
  ~ExecContextScope();

 private:
  std::shared_ptr<ExecContext const> _old;
};

/// @brief RAII scope guard for temporary superuser context
///
/// Provides a convenient way to temporarily switch to superuser context
/// for operations that require elevated privileges. The superuser context
/// bypasses all authentication and authorization checks.
///
/// @note Use with caution as superuser context bypasses all security checks
/// @note Automatically restores previous context when destroyed
///
/// Example:
/// @code
/// {
///   ExecContextSuperuserScope guard;
///   // Operations here run with superuser privileges
///   // ... perform privileged operations ...
/// } // Previous context automatically restored here
/// @endcode
struct ExecContextSuperuserScope {
  /// @brief Construct scope guard and set superuser context
  ///
  /// Unconditionally sets the superuser context as current and stores
  /// the previous context for later restoration.
  explicit ExecContextSuperuserScope() : _old(ExecContext::CURRENT) {
    ExecContext::CURRENT = ExecContext::Superuser;
  }

  /// @brief Construct scope guard and conditionally set superuser context
  ///
  /// Sets the superuser context as current only if the condition is true.
  /// If condition is false, the context remains unchanged.
  ///
  /// @param cond Condition that determines whether to switch to superuser context
  explicit ExecContextSuperuserScope(bool cond) : _old(ExecContext::CURRENT) {
    if (cond) {
      ExecContext::CURRENT = ExecContext::Superuser;
    }
  }

  /// @brief Destructor that restores the previous execution context
  ///
  /// Automatically restores the execution context that was active
  /// before this scope guard was created.
  ~ExecContextSuperuserScope() { ExecContext::CURRENT = _old; }

 private:
  std::shared_ptr<ExecContext const> _old;
};

}  // namespace arangodb
