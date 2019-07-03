////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/connection.h>
#include <fuerte/message.h>

#include <fuerte/api/database.h>
#include <fuerte/api/collection.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
using namespace arangodb::fuerte::detail;

Database::Database(std::shared_ptr<Connection> conn, std::string const& name)
    : _conn(conn), _name(name) {}

std::shared_ptr<Collection> Database::getCollection(std::string const& name) {
  return std::shared_ptr<Collection>(new Collection(shared_from_this(), name));
}

std::shared_ptr<Collection> Database::createCollection(
    std::string const& name) {
  return std::shared_ptr<Collection>(new Collection(shared_from_this(), name));
}
bool Database::deleteCollection(std::string const& name) { return false; }
}}}  // namespace arangodb::fuerte::v1
