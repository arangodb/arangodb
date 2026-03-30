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

#include "Can.h"

namespace arangodb::auth {

Can::Can(AuthMode const& authMode) : _authMode(authMode) {}

bool Can::admin(rbac::Category::Any const& rbacAction) const {
  std::abort();  // TODO implement
}

bool Can::readView(std::string_view dbname, std::string_view view) const {
  std::abort();
}

bool Can::createView(std::string_view dbname, std::string_view view) const {
  std::abort();
}

bool Can::createView(std::string_view dbname, std::string_view view,
                     std::span<std::string> linkedCollections) const {
  std::abort();
}

bool Can::modifyView(std::string_view dbname, std::string_view view) const {
  std::abort();
}

bool Can::modifyView(std::string_view dbname, std::string_view view,
                     std::span<std::string> linkedCollections) const {
  std::abort();
}

bool Can::renameView(std::string_view dbname, std::string_view oldViewName,
                     std::string_view newViewName) const {
  std::abort();
}

bool Can::renameView(std::string_view dbname, std::string_view oldViewName,
                     std::string_view newViewName,
                     std::span<std::string> linkedCollections) const {
  std::abort();
}

bool Can::dropView(std::string_view dbname, std::string_view view) const {
  std::abort();
}

bool Can::dropView(std::string_view dbname, std::string_view view,
                   std::span<std::string> linkedCollections) const {
  // TODO at least in Classic, this must check
  //     * RW access to the database
  //     * RO access to each collection
  //     and possibly some access to the view itself, but I haven't looked where
  //     that check currently happens
  std::abort();
}

}  // namespace arangodb::auth
