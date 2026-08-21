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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

namespace arangodb {
/// Options that apply to a single create request. None of them describes the
/// collection, so none of them is stored with it.
struct CollectionCreateOptions {
  // Parsed from the request body.
  // Not documented, actually this is an option, not a configuration parameter
  std::vector<std::string> avoidServers = {};

  // Chosen by the caller, not part of the body.
  bool waitForSyncReplication = true;
  bool enforceReplicationFactor = true;
  bool isNewDatabase = false;
  bool allowEnterpriseCollectionsOnSingleServer = false;
  bool isRestore = false;

  bool operator==(CollectionCreateOptions const& other) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionCreateOptions& props) {
  return f.object(props).fields(
      f.field("avoidServers", props.avoidServers).fallback(f.keep()));
}

}  // namespace arangodb
