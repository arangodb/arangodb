////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include <fuerte/connection.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace arangodb {

/**
 * The connections of a client, at most one per handle
 *
 * A handle identifies a connection configuration, such as endpoint and user.
 * One connection is active; the others are kept for reuse when the client
 * switches back to their handle. A later connection for a handle replaces an
 * earlier one, which is cancelled. Closed connections are not kept.
 *
 * Config describes how a connection was configured. The Config of the active
 * entry also serves as the configuration to connect with next while no
 * connection is active, so it outlives deactivate().
 *
 * Not thread-safe.
 *
 * Example:
 *   ConnectionHandles<std::string> handles;
 *   handles.activate("a", {connectionA, "config a"});
 *   handles.activate("b", {connectionB, "config b"});  // keeps "a"
 *   auto a = handles.take("a");  // "a" is no longer kept
 */
template<typename Config>
class ConnectionHandles {
 public:
  struct Entry {
    std::shared_ptr<fuerte::Connection> connection;
    Config config;
  };

  /// the active connection; null if none, and possibly closed
  auto connection() -> std::shared_ptr<fuerte::Connection>& {
    return _active.connection;
  }
  auto connection() const -> std::shared_ptr<fuerte::Connection> const& {
    return _active.connection;
  }

  /// the configuration of the active connection, or the one to connect with
  /// next while there is none
  auto config() -> Config& { return _active.config; }
  auto config() const -> Config const& { return _active.config; }

  /// the handle of the active connection; empty if there is none
  auto activeHandle() const -> std::string const& { return _activeHandle; }

  /// the kept, inactive connections by handle
  auto inactive() const -> std::unordered_map<std::string, Entry> const& {
    return _inactive;
  }

  /**
   * Keeps an inactive connection for reuse
   *
   * Null connections are ignored. Closed connections and connections without
   * a handle are not kept, but cancelled.
   */
  void keep(std::string const& handle, Entry entry) {
    if (entry.connection == nullptr) {
      return;
    }
    if (handle.empty() ||
        entry.connection->state() == fuerte::Connection::State::Closed) {
      entry.connection->cancel();
      return;
    }
    auto it = _inactive.find(handle);
    if (it == _inactive.end()) {
      _inactive.emplace(handle, std::move(entry));
      return;
    }
    // a later connection for the handle replaces the earlier one
    if (it->second.connection != entry.connection) {
      it->second.connection->cancel();
    }
    it->second = std::move(entry);
  }

  /**
   * Removes the kept connection of the handle and returns it
   */
  auto take(std::string const& handle) -> std::optional<Entry> {
    auto it = _inactive.find(handle);
    if (it == _inactive.end()) {
      return std::nullopt;
    }
    auto entry = std::move(it->second);
    _inactive.erase(it);
    return entry;
  }

  /**
   * Makes entry the active connection
   *
   * The previously active connection is kept, unless it has the same handle:
   * then entry supersedes it, and it is cancelled.
   */
  void activate(std::string const& handle, Entry entry) {
    if (_active.connection != nullptr &&
        _active.connection != entry.connection) {
      if (_activeHandle == handle) {
        _active.connection->cancel();
      } else {
        deactivate();
      }
    }
    _active = std::move(entry);
    _activeHandle = handle;
  }

  /**
   * Keeps the active connection as an inactive one
   *
   * Afterwards no connection is active, but config() is unchanged.
   */
  void deactivate() {
    keep(_activeHandle, Entry{std::move(_active.connection), _active.config});
    _active.connection.reset();
    _activeHandle.clear();
  }

  /**
   * Forgets the active connection without keeping it
   */
  void dropActive() {
    _active.connection.reset();
    _activeHandle.clear();
  }

  /**
   * Forgets all connections; config() is unchanged
   */
  void clear() {
    _inactive.clear();
    dropActive();
  }

 private:
  Entry _active;
  std::string _activeHandle;
  std::unordered_map<std::string, Entry> _inactive;
};

}  // namespace arangodb
