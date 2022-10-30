////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>

#include <yaclib/fwd.hpp>

struct TRI_vocbase_t;

namespace arangodb {

template<typename T>
class ResultT;

namespace velocypack {
class SharedSlice;
}

namespace replication2 {
struct LogIndex;
class LogId;

/**
 * Abstraction used by the RestHandler to access the DocumentState.
 */
struct DocumentStateMethods {
  virtual ~DocumentStateMethods() = default;

  [[nodiscard]] static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<DocumentStateMethods>;

  [[nodiscard]] virtual auto getSnapshot(LogId logId,
                                         LogIndex waitForIndex) const
      -> yaclib::Future<ResultT<velocypack::SharedSlice>> = 0;
};
}  // namespace replication2
}  // namespace arangodb
