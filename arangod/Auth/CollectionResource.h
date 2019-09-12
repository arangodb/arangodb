////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/DatabaseResource.h"

#include <string>

namespace arangodb {
namespace auth {
class CollectionResource : public DatabaseResource {
 public:
  CollectionResource(std::string const& database, std::string const& collection)
      : DatabaseResource(database), _collection(collection) {}

  CollectionResource(std::string&& database, std::string&& collection)
      : DatabaseResource(std::move(database)), _collection(std::move(collection)) {}

  CollectionResource(DatabaseResource const& database, std::string const& collection)
      : DatabaseResource(database), _collection(collection) {}

  template <typename D, typename C>
  CollectionResource(D const& database, C const* collection)
      : DatabaseResource(database.name()), _collection(collection->name()) {}

  template <typename D, typename C>
  CollectionResource(D const& database, std::unique_ptr<C> const& collection)
      : DatabaseResource(database.name()), _collection(collection.get()->name()) {}

  template <typename C>
  explicit CollectionResource(C const* collection)
      : DatabaseResource(collection->vocbase().name()),
        _collection(collection->name()) {}

  template <typename C>
  explicit CollectionResource(C const& collection)
      : DatabaseResource(collection.vocbase().name()),
        _collection(collection.name()) {}

  template <typename D>
  CollectionResource(D const& database, std::string const& collection)
      : DatabaseResource(database.name()), _collection(collection) {}

  template <typename D>
  CollectionResource(D const& database, std::string&& collection)
      : DatabaseResource(database.name()), _collection(std::move(collection)) {}

  std::string const _collection;
};
}  // namespace auth
}  // namespace arangodb
