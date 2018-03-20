////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <mutex>

#include "velocypack/StringRef.h"

#include "LogicalDataSource.h"

namespace arangodb {

/*static*/ LogicalDataSource::Type const& LogicalDataSource::Type::emplace(
    arangodb::velocypack::StringRef const& name
) {
  struct Less {
    bool operator()(
        arangodb::velocypack::StringRef const& lhs,
        arangodb::velocypack::StringRef const& rhs
    ) const noexcept {
      return lhs.compare(rhs) < 0;
    }
  };
  static std::mutex mutex;
  static std::map<arangodb::velocypack::StringRef, LogicalDataSource::Type, Less> types;
  std::lock_guard<std::mutex> lock(mutex);
  auto itr = types.emplace(name, Type());

  if (itr.second) {
    const_cast<std::string&>(itr.first->second._name) = name.toString(); // update '_name'
    const_cast<arangodb::velocypack::StringRef&>(itr.first->first) =
      itr.first->second.name(); // point key at value stored in '_name'
  }

  return itr.first->second;
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------