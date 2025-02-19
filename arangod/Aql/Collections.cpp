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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Collections.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Collection.h"
#include "Basics/Exceptions.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::aql;

Collections::Collections(TRI_vocbase_t* vocbase) : _vocbase(vocbase) {}

Collections::~Collections() = default;

Collection* Collections::get(std::string_view name) const {
  auto it = _collections.find(name);

  if (it != _collections.end()) {
    return (*it).second.get();
  }

  return nullptr;
}

Collection* Collections::add(std::string const& name,
                             AccessMode::Type accessType,
                             Collection::Hint hint) {
  TRI_ASSERT(!name.empty());

  // check if collection already is in our map
  auto it = _collections.find(name);

  if (it == _collections.end()) {
    if (_collections.size() >= _vocbase->server()
                                   .getFeature<QueryRegistryFeature>()
                                   .maxCollectionsPerQuery()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS);
    }

    auto collection =
        std::make_unique<Collection>(name, _vocbase, accessType, hint);
    it = _collections.try_emplace(name, std::move(collection)).first;
  } else {
    // note that the collection is used in both read & write ops
    if (AccessMode::isReadWriteChange(accessType, (*it).second->accessType())) {
      (*it).second->isReadWrite(true);
    }

    // change access type from read to write
    if (AccessMode::isWriteOrExclusive(accessType) &&
        (*it).second->accessType() == AccessMode::Type::READ) {
      (*it).second->accessType(accessType);
    } else if (AccessMode::isExclusive(accessType)) {
      // exclusive flag always wins
      (*it).second->accessType(accessType);
    }
  }

  TRI_ASSERT((*it).second != nullptr);
  return (*it).second.get();
}

std::vector<std::string> Collections::collectionNames() const {
  std::vector<std::string> result;
  result.reserve(_collections.size());

  for (auto const& it : _collections) {
    if (!it.first.empty() && it.first[0] >= '0' && it.first[0] <= '9') {
      // numeric collection id. don't return them
      continue;
    }
    result.emplace_back(it.first);
  }
  return result;
}

bool Collections::empty() const { return _collections.empty(); }

void Collections::toVelocyPack(
    velocypack::Builder& builder,
    std::function<bool(std::string const&, Collection const&)> const& filter)
    const {
  builder.openArray();
  for (auto const& c : _collections) {
    if (!filter(c.first, *c.second)) {
      continue;
    }
    builder.openObject();
    builder.add("name", VPackValue(c.first));
    builder.add("type",
                VPackValue(AccessMode::typeString(c.second->accessType())));
    builder.close();
  }
  builder.close();
}

void Collections::visit(
    std::function<bool(std::string const&, Collection&)> const& visitor) const {
  for (auto const& it : _collections) {
    // stop iterating when visitor returns false
    if (!visitor(it.first, *it.second.get())) {
      return;
    }
  }
}
