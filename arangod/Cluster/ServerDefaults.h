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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "RestServer/arangod.h"

struct TRI_vocbase_t;

namespace arangodb {

struct ServerDefaults {
  uint64_t numberOfShards = 1;
  uint64_t replicationFactor = 1;
  uint64_t writeConcern = 1;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // We use the default constructor for tests,
  // we do not want to use the Server for testing
  // usage of the values. But in production these
  // values have to be taken from the Server.
  ServerDefaults() = default;
#endif
  explicit ServerDefaults(ArangodServer& server);

  explicit ServerDefaults(TRI_vocbase_t const& vocbase);
};

}  // namespace arangodb
