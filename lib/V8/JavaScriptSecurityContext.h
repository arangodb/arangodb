////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

namespace arangodb {

class JavaScriptSecurityContext {
 public:
  enum class Type {
    Restricted,
    Internal,
    AdminScript,
    Query,
    Task,
    RestAction,
    RestAdminScriptAction
  };

  explicit JavaScriptSecurityContext(Type type) noexcept : _type(type) {}

  ~JavaScriptSecurityContext() = default;

  /// @brief return the context type as a string
  std::string_view typeName() const noexcept;

  /// @brief resets context to most restrictive settings
  void reset() noexcept;

  /// @brief whether or not the context is an internal context
  bool isInternal() const noexcept { return _type == Type::Internal; }

  /// @brief whether or not the context is an admin script
  bool isAdminScript() const noexcept { return _type == Type::AdminScript; }

  /// @brief whether or not the context is an admin script
  bool isRestAdminScript() const noexcept {
    return _type == Type::RestAdminScriptAction;
  }

  /// @brief whether or not db._useDatabase(...) is allowed
  bool canUseDatabase() const noexcept { return _canUseDatabase; }

  /// @brief whether fs read is allowed
  bool canReadFs() const noexcept;

  /// @brief whether fs read is allowed
  bool canWriteFs() const noexcept;

  /// @brief whether or not actions.defineAction(...) is allowed, which will
  /// add REST endpoints
  /// currently only internal operations are allowed to do this
  bool canDefineHttpAction() const noexcept;

  /// @brief whether or not execution or state-modification of external
  /// binaries is allowed.
  bool canControlProcesses() const noexcept;

  /// @brief create a security context that is most restricted
  static JavaScriptSecurityContext createRestrictedContext() noexcept;

  /// @brief create a security context for arangodb-internal
  /// operations, with non-restrictive settings
  static JavaScriptSecurityContext createInternalContext() noexcept;

  /// @brief create a security context for admin script operations,
  /// invoked by `--javascript.execute` or when running in --console mode
  static JavaScriptSecurityContext createAdminScriptContext() noexcept;

  /// @brief create a security context for AQL queries,
  /// with restrictive settings
  static JavaScriptSecurityContext createQueryContext() noexcept;

  /// @brief create a security context for tasks actions
  static JavaScriptSecurityContext createTaskContext(
      bool allowUseDatabase) noexcept;

  /// @brief create a security context for REST actions
  static JavaScriptSecurityContext createRestActionContext(
      bool allowUseDatabase) noexcept;

  /// @brief create a security context for admin script operations running
  /// via POST /_admin/execute
  static JavaScriptSecurityContext createRestAdminScriptActionContext(
      bool allowUseDatabase) noexcept;

 private:
  Type _type;
  bool _canUseDatabase = false;
};

}  // namespace arangodb
