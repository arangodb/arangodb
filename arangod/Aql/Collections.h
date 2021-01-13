////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_COLLECTIONS_H
#define ARANGOD_AQL_COLLECTIONS_H 1

#include "Aql/types.h"
#include "Aql/Collection.h"
#include "VocBase/AccessMode.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace aql {

class Collections {
 public:
  Collections(Collections const& other) = delete;
  Collections& operator=(Collections const& other) = delete;

  explicit Collections(TRI_vocbase_t*);

  ~Collections();

 public:
  Collection* get(std::string_view name) const;

  Collection* add(std::string const& name, AccessMode::Type accessType, Collection::Hint hint);

  std::vector<std::string> collectionNames() const;

  bool empty() const;

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  
  void visit(std::function<bool(std::string const&, Collection&)> const& visitor) const;

 private:
  TRI_vocbase_t* _vocbase;

  std::map<std::string, std::unique_ptr<aql::Collection>, std::less<>> _collections;

  static size_t const MaxCollections = 2048;
};
}  // namespace aql
}  // namespace arangodb

#endif
