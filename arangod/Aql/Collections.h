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

#ifndef ARANGOD_AQL_COLLECTIONS_H
#define ARANGOD_AQL_COLLECTIONS_H 1

#include "Basics/Common.h"
#include "VocBase/AccessMode.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
struct Collection;

class Collections {
 public:
  Collections& operator=(Collections const& other) = delete;

  explicit Collections(TRI_vocbase_t*);

  ~Collections();

 public:
  Collection* get(std::string const& name) const;

  Collection* add(std::string const& name, AccessMode::Type accessType);

  std::vector<std::string> collectionNames() const;

  std::map<std::string, Collection*>* collections();

  std::map<std::string, Collection*> const* collections() const;

  bool empty() const { return _collections.empty(); }

 private:
  TRI_vocbase_t* _vocbase;

  std::map<std::string, Collection*> _collections;

  static size_t const MaxCollections = 2048;
};
}
}

#endif
