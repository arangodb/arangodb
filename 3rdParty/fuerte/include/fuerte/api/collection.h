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
#ifndef ARANGO_CXX_DRIVER_COLLECTION
#define ARANGO_CXX_DRIVER_COLLECTION

#include <memory>
#include <string>
#include <fuerte/types.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
class Database;

class Collection : public std::enable_shared_from_this<Collection> {
  friend class Database;
  typedef std::string Document;  // FIXME

 public:
  bool insert(Document) { return false; }
  void drop(Document) {}
  void update(Document, Document) {}
  void replace(Document, Document) {}
  void dropAll() {}
  void find(Document) {}

 private:
  Collection(std::shared_ptr<Database> const&, std::string const& name);
  std::shared_ptr<Database> _db;
  std::string _name;
};
}}}  // namespace arangodb::fuerte::v1
#endif
