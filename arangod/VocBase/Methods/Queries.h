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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VocBase/vocbase.h"
#include "Basics/Result.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace methods {

struct Queries {
  /// @brief return the list of slow queries
  static Result listSlow(TRI_vocbase_t& vocbase, velocypack::Builder& out,
                         bool allDatabases, bool fanout);

  /// @brief return the list of currently running queries
  static Result listCurrent(TRI_vocbase_t& vocbase, velocypack::Builder& out,
                            bool allDatabases, bool fanout);

  /// @brief clears the list of slow queries
  static Result clearSlow(TRI_vocbase_t& vocbase, bool allDatabases,
                          bool fanout);

  /// @brief kills the given query
  static Result kill(TRI_vocbase_t& vocbase, TRI_voc_tick_t id,
                     bool allDatabases);
};

}  // namespace methods
}  // namespace arangodb
