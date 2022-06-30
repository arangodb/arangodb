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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "Agency/Helpers.h"

#include "Basics/StaticStrings.h"
#include "Replication2/Version.h"

namespace arangodb::consensus {

bool isReplicationTwoDB(Node::Children const& databases,
                        std::string const& dbName) {
  auto it = databases.find(dbName);
  if (it == databases.end()) {
    // this should actually never happen, but if it does we simply claim that
    // this is an old replication 1 DB.
    return false;
  }

  if (auto v = it->second->hasAsString(StaticStrings::ReplicationVersion); v) {
    auto res = replication::parseVersion(v.value());
    return res.ok() && res.get() == replication::Version::TWO;
  }
  return false;
}

}  // namespace arangodb::consensus
