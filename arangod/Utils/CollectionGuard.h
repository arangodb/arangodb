////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "VocBase/vocbase.h"

#include <memory>

namespace arangodb {
class LogicalCollection;

class CollectionGuard {
 public:
  CollectionGuard(CollectionGuard const&) = delete;
  CollectionGuard& operator=(CollectionGuard const&) = delete;

  CollectionGuard(CollectionGuard&& other)
      : _vocbase(other._vocbase),
        _collection(std::move(other._collection)),
        _lockId(other._lockId) {
    other._collection.reset();
    other._vocbase = nullptr;
  }

  /// @brief create the guard, using a collection id
  CollectionGuard(TRI_vocbase_t* vocbase, DataSourceId cid,
                  bool checkPermissions = true)
      : _vocbase(vocbase) {
    std::tie(_collection, _lockId) =
        _vocbase->useCollection(cid, checkPermissions);
    // useCollection will throw if the collection does not exist
    TRI_ASSERT(_collection != nullptr);
  }

  /// @brief create the guard, using a collection name
  CollectionGuard(TRI_vocbase_t* vocbase, std::string const& name,
                  bool checkPermissions = true)
      : _vocbase(vocbase), _collection(nullptr) {
    if (!name.empty() && name[0] >= '0' && name[0] <= '9') {
      DataSourceId id{NumberUtils::atoi_zero<DataSourceId::BaseType>(
          name.data(), name.data() + name.size())};
      std::tie(_collection, _lockId) =
          _vocbase->useCollection(id, checkPermissions);
    } else {
      std::tie(_collection, _lockId) =
          _vocbase->useCollection(name, checkPermissions);
    }
    // useCollection will throw if the collection does not exist
    TRI_ASSERT(_collection != nullptr);
  }

  /// @brief destroy the guard
  ~CollectionGuard() {
    if (_collection != nullptr) {
      _vocbase->releaseCollection(_collection.get(), _lockId);
    }
  }

 public:
  /// @brief return the collection pointer
  arangodb::LogicalCollection* collection() const { return _collection.get(); }

 private:
  /// @brief pointer to vocbase
  TRI_vocbase_t* _vocbase;

  /// @brief pointer to collection
  std::shared_ptr<arangodb::LogicalCollection> _collection;
  size_t _lockId;
};
}  // namespace arangodb
