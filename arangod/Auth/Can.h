////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Rbac/Actions.h"

#include <span>
#include <string>
#include <string_view>

namespace arangodb {
struct AuthMode;
}

namespace arangodb::auth {

// TODO List:
//      * We need a batched API that allows to check multiple different
//        permissions in a single call. Other APIs may stay for convenience, or
//        be removed.
//      * We need to provide a reason, or reasons, for a deny, appropriate for
//        a reply to the user or logging. It should be a Result, or trivially
//        convertible to one.
//      * All APIs must be made asynchronous.
//      * After the structure has settled, revisit whether this should stay
//        as a separate struct, or be merged into ExecContext.

// Collected things we have authorization checks for
struct Can {
  explicit Can(AuthMode const& authMode);

  [[nodiscard]] bool admin(rbac::Category::Any const& rbacAction) const;

  [[nodiscard]] bool accessDatabase(std::string_view dbname) const;

  [[nodiscard]] bool accessCollection(std::string_view dbname,
                                      std::string_view collection) const;
  [[nodiscard]] bool readCollection(std::string_view dbname,
                                    std::string_view collection) const;
  [[nodiscard]] bool writeCollection(std::string_view dbname,
                                     std::string_view collection) const;
  [[nodiscard]] bool createCollection(std::string_view dbname,
                                      std::string_view collection) const;
  [[nodiscard]] bool dropCollection(std::string_view dbname,
                                    std::string_view collection) const;
  [[nodiscard]] bool modifyCollection(std::string_view dbname,
                                      std::string_view collection) const;

  [[nodiscard]] bool accessView(std::string_view dbname,
                                std::string_view view) const;
  [[nodiscard]] bool readView(std::string_view dbname,
                              std::string_view view) const;
  // We currently need two of each view call, to reflect the existing code,
  // because the checks are scattered.
  // Only having the second one should be sufficient, but we probably want to
  // do the check earlier than it currently happens.
  [[nodiscard]] bool createView(std::string_view dbname,
                                std::string_view view) const;
  [[nodiscard]] bool createView(std::string_view dbname, std::string_view view,
                                std::span<std::string> linkedCollections) const;
  [[nodiscard]] bool modifyView(std::string_view dbname,
                                std::string_view view) const;
  [[nodiscard]] bool modifyView(std::string_view dbname, std::string_view view,
                                std::span<std::string> linkedCollections) const;
  [[nodiscard]] bool renameView(std::string_view dbname,
                                std::string_view oldViewName,
                                std::string_view newViewName) const;
  [[nodiscard]] bool renameView(std::string_view dbname,
                                std::string_view oldViewName,
                                std::string_view newViewName,
                                std::span<std::string> linkedCollections) const;
  [[nodiscard]] bool dropView(std::string_view dbname,
                              std::string_view view) const;
  [[nodiscard]] bool dropView(std::string_view dbname, std::string_view view,
                              std::span<std::string> linkedCollections) const;

 private:
  AuthMode const& _authMode;
};

}  // namespace arangodb::auth
