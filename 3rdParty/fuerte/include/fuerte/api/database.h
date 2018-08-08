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
#pragma once
#ifndef ARANGO_CXX_DRIVER_DATABASE
#define ARANGO_CXX_DRIVER_DATABASE

#include <functional>
#include <string>
#include <fuerte/types.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
class Connection;
class Collection;

class Database : public std::enable_shared_from_this<Database> {
  friend class Connection;
  
  Database(std::shared_ptr<Connection>, std::string const& name);
  
 public:
  
  std::shared_ptr<Collection> getCollection(std::string const& name);
  std::shared_ptr<Collection> createCollection(std::string const& name);
  bool deleteCollection(std::string const& name);

 private:
  std::shared_ptr<Connection> _conn;
  std::string _name;
};
}}}  // namespace arangodb::fuerte::v1
#endif
