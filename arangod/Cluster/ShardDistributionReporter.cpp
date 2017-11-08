////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShardDistributionReporter.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::cluster;

//////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ShardDistributionReporter>
    arangodb::cluster::ShardDistributionReporter::_theInstance;

ShardDistributionReporter::ShardDistributionReporter(
    std::shared_ptr<ClusterComm> cc, ClusterInfo* ci)
    : _cc(cc), _ci(ci) {
  TRI_ASSERT(_cc != nullptr);
  TRI_ASSERT(_ci != nullptr);
}

ShardDistributionReporter::~ShardDistributionReporter() {}

std::shared_ptr<ShardDistributionReporter>
ShardDistributionReporter::instance() {
  if (_theInstance == nullptr) {
    _theInstance = std::make_shared<ShardDistributionReporter>(
        ClusterComm::instance(), ClusterInfo::instance());
  }
  return _theInstance;
}

void ShardDistributionReporter::getDistributionForDatabase(
    std::string const& dbName, VPackBuilder& result) {
  result.openObject();

  auto aliases = _ci->getServerAliases();
  auto cols = _ci->getCollections(dbName);
  for (auto const& it : cols) {
    auto allShards = it->shardIds();
    result.add(VPackValue(it->name()));

    result.openObject();
    {
      // Add Plan
      result.add(VPackValue("Plan"));
      result.openObject();
      for (auto const& s : *(allShards.get())) {
        result.add(VPackValue(s.first));
        result.openObject();
        // We always have at least the leader
        TRI_ASSERT(!s.second.empty());
        auto respServerIt = s.second.begin();
        auto al = aliases.find(*respServerIt);
        if (al != aliases.end()) {
          result.add("leader", VPackValue(al->second));
        } else {
          result.add("leader", VPackValue(*respServerIt));
        }
        ++respServerIt;

        result.add(VPackValue("followers"));
        result.openArray();
        while (respServerIt != s.second.end()) {
          auto al = aliases.find(*respServerIt);
          if (al != aliases.end()) {
            result.add(VPackValue(al->second));
          } else {
            result.add(VPackValue(*respServerIt));
          }
          ++respServerIt;
        }
        result.close(); // followers
        result.close(); // shard
      }
      result.close();
    }

    {
      // Add Current
      result.add(VPackValue("Current"));
      result.openObject();
      result.close();
    }
    result.close();
  }

  result.close();
}
