////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Collections.h"
#include "Aql/Collection.h"
#include "Basics/Exceptions.h"

using namespace arangodb::aql;

Collections::Collections(TRI_vocbase_t* vocbase)
    : _vocbase(vocbase), _collections() {}

Collections::~Collections() {
  for (auto& it : _collections) {
    delete it.second;
  }
}

Collection* Collections::get(std::string const& name) const {
  auto it = _collections.find(name);

  if (it != _collections.end()) {
    return (*it).second;
  }

  return nullptr;
}

Collection* Collections::add(std::string const& name,
                             TRI_transaction_type_e accessType) {
  // check if collection already is in our map
  TRI_ASSERT(!name.empty());
  auto it = _collections.find(name);

  if (it == _collections.end()) {
    if (_collections.size() >= MaxCollections) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS);
    }

    auto collection = std::make_unique<Collection>(name, _vocbase, accessType);
    _collections.emplace(name, collection.get());

    return collection.release();
  } else {
    // note that the collection is used in both read & write ops
    if (accessType != (*it).second->accessType) {
      (*it).second->isReadWrite = true;
    }

    // change access type from read to write
    if (accessType == TRI_TRANSACTION_WRITE &&
        (*it).second->accessType == TRI_TRANSACTION_READ) {
      (*it).second->accessType = TRI_TRANSACTION_WRITE;
    }
  }
  return (*it).second;
}

std::vector<std::string> Collections::collectionNames() const {
  std::vector<std::string> result;
  result.reserve(_collections.size());

  for (auto const& it : _collections) {
    result.emplace_back(it.first);
  }
  return result;
}

std::map<std::string, Collection*>* Collections::collections() {
  return &_collections;
}

std::map<std::string, Collection*> const* Collections::collections() const {
  return &_collections;
}
