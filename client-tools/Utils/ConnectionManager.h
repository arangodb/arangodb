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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Utils/ConnectionHandles.h"
#include "Utils/RenewingJwtToken.h"

#include <fuerte/connection.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arangodb {

/**
 * Endpoint, credentials and database to connect with, as the client gave them
 *
 * At most one of jwtToken and jwtSecret is set.
 */
struct ConnectionSettings {
  std::string endpoint;
  std::string username;
  std::string password;
  std::string jwtToken;
  std::string jwtSecret;
  std::string databaseName;
};

/**
 * The connections of a client to its servers, and how they authenticate
 *
 * Authenticates with the given jwt token, with a token for the jwt secret, or
 * with a token obtained by logging in, which it renews as the user who logged
 * in. Keeps a connection per handle, so that switching back to an endpoint and
 * user reuses its connection.
 *
 * Not thread-safe.
 *
 * Example:
 *   ConnectionManager manager{builder, connect, &JwtClock::now};
 *   if (manager.configure(settings, 30s).ok()) {
 *     auto const outcome = manager.connect(false, false);
 *   }
 */
class ConnectionManager {
 public:
  /// opens a connection as configured by the builder
  using Connect = std::function<std::shared_ptr<fuerte::Connection>(
      fuerte::ConnectionBuilder)>;
  using TimeSource = std::function<JwtClock::time_point()>;

  /// base configures what all connections share, such as timeouts
  ConnectionManager(fuerte::ConnectionBuilder base, Connect connect,
                    TimeSource now);

  /**
   * Makes settings the configuration to connect with next
   *
   * A login with username and password happens here, so that a failed one
   * leaves the configuration untouched. The active connection is kept under
   * its handle.
   */
  auto configure(ConnectionSettings const& settings,
                 std::chrono::milliseconds requestTimeout) -> Result;

  struct ServerInfo {
    std::string version;
    std::optional<std::string> mode;
    std::optional<std::string> role;
  };

  struct ConnectOutcome {
    /// null if no connection could be established
    std::shared_ptr<fuerte::Connection> connection;
    /// of the reply to GET /_api/version, or describing why there is none
    unsigned httpCode = 0;
    /// why the connection is not usable, if it is not
    std::optional<std::string> errorMessage;
    /// if the server is an ArangoDB
    std::optional<ServerInfo> server;
  };

  /**
   * Connects as configured, and makes that the active connection if the
   * server answers GET /_api/version
   *
   * Reuses the connection kept for the handle unless bypassCache is set.
   */
  auto connect(bool bypassCache, bool forceJson) -> ConnectOutcome;

  /**
   * Renews the token of the active configuration if due
   *
   * Tokens obtained by logging in are renewed at the server and as the user
   * of the configuration. A configuration with a jwt token option adopts
   * jwtTokenOption. Returns whether the token changed; the active connection
   * keeps the token it was created with until it is replaced.
   */
  auto renewTokenIfDue(JwtClock::duration threshold,
                       std::string const& jwtTokenOption,
                       std::chrono::milliseconds requestTimeout) -> bool;

  /// null if there is none, and possibly closed
  auto activeConnection() const -> std::shared_ptr<fuerte::Connection> const&;
  /// empty if no connection is active
  auto activeHandle() const -> std::string const&;
  /// of the active connection, or of the one to connect with next
  auto settings() const -> ConnectionSettings const&;
  void setDatabaseName(std::string const& databaseName);
  auto protocolType() const -> fuerte::ProtocolType;

  struct HandleInfo {
    std::string handle;
    bool active;
    struct Peer {
      bool connected;
      std::string endpoint;
      std::string localEndpoint;
    };
    // empty if the handle has no connection
    std::optional<Peer> peer;
    std::string username;
    std::string password;
    std::string jwtToken;
  };
  auto handleTable() const -> std::vector<HandleInfo>;

  enum class HandleSwitch { kAlreadyActive, kSwitched, kUnknownHandle };
  /// a switched-to handle brings its own settings and token
  auto switchTo(std::string const& handle) -> HandleSwitch;

  /// false if the handle is unknown
  auto drop(std::string const& handle) -> bool;
  void clear();

 private:
  // how the jwt token of a connection is kept fresh
  struct TokenRenewal {
    enum class Kind {
      // basic auth, no auth, or a token without expiry from the jwt secret
      kNone,
      // a user token from /_open/auth, renewed by logging in again
      kLogin,
      // the jwt token option, renewed by the client
      kJwtTokenOption,
    };
    Kind kind = Kind::kNone;
    // token state of kLogin, which logs in with the credentials of the
    // Configuration
    JwtTokenState token;
  };

  struct Configuration {
    fuerte::ConnectionBuilder builder;
    TokenRenewal renewal;
    // whichever of them the authentication mode uses. the database is not
    // part of the identity: requests name their database, so switching it
    // keeps the connection
    ConnectionSettings settings;
  };

  // Requests a token at the endpoint of target: kLogin logs in via
  // /_open/auth with username and password, kRenew renews the token of target
  // via /_open/auth/renew, over the active connection if that has the token.
  // yields no token if the server issues none (yet)
  auto requestToken(TokenRequest request,
                    fuerte::ConnectionBuilder const& target,
                    std::chrono::milliseconds timeout,
                    std::string const& username = "",
                    std::string const& password = "") -> TokenOutcome;

  // the superuser token for the jwt secret, generated once per secret
  auto internalToken(std::string const& jwtSecret) -> std::string const&;

  Connect _connect;
  TimeSource _now;
  // the last superuser token, and the jwt secret it was generated with
  std::string _internalToken;
  std::string _internalTokenSecret;
  // the config() of the active entry is the one to connect with next while no
  // connection is active; whoever retargets it to another connection has to
  // deactivate() the active connection first
  ConnectionHandles<Configuration> _handles;
};

}  // namespace arangodb
