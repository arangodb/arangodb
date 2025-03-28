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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/AgencyDiagnosis.h"

#include "Cluster/Agency.h"
#include "Cluster/AgencyInspection.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Inspection/VPack.h"

#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <utility>

namespace arangodb::agency {

struct CollectionNameDuplicates {
  std::string databaseName;
  std::string collectionName;
  std::vector<std::string> collectionIds;

  CollectionNameDuplicates(std::string dbName, std::string colName,
                           std::vector<std::string> colIds)
      : databaseName(std::move(dbName)),
        collectionName(std::move(colName)),
        collectionIds(std::move(colIds)) {}
};

// Find duplicate collection names within each database
std::vector<CollectionNameDuplicates> findDuplicateCollectionNames(
    const AgencyData& agencyData) {
  std::vector<CollectionNameDuplicates> result;

  // Access the Plan and its Collections
  const auto& plan = agencyData.arango.Plan;
  const auto& collectionsMap = plan.Collections;

  // Iterate through each database
  for (const auto& [dbName, dbCollections] : collectionsMap) {
    // Map to store collection names and their IDs
    std::unordered_map<std::string, std::vector<std::string>> nameToIds;

    // Group collections by name
    for (const auto& [collectionId, collection] : dbCollections) {
      nameToIds[collection.name].push_back(collectionId);
    }

    // Find collections with duplicate names (more than one ID for the same
    // name)
    for (const auto& [name, ids] : nameToIds) {
      if (ids.size() > 1) {
        result.emplace_back(dbName, name, ids);
      }
    }
  }

  return result;
}

// Optional: Print function for the results
void printDuplicateCollections(
    const std::vector<CollectionNameDuplicates>& duplicates,
    std::ostream& out) {
  if (duplicates.empty()) {
    out << "No duplicate collection names found." << std::endl;
    return;
  }

  out << "Found " << duplicates.size()
      << " groups of duplicate collection names:" << std::endl;

  for (const auto& duplicate : duplicates) {
    out << "Database: " << duplicate.databaseName << std::endl;
    out << "  Collection name: " << duplicate.collectionName << std::endl;
    out << "  Collection IDs: ";
    for (size_t i = 0; i < duplicate.collectionIds.size(); ++i) {
      out << duplicate.collectionIds[i];
      if (i < duplicate.collectionIds.size() - 1) {
        out << ", ";
      }
    }
    out << std::endl;
  }
}

struct ShardLeaderDiagnostic {
  std::string database;
  std::string collection;
  std::string shard;
  std::string leaderServer;
  std::string leaderStatus;
  std::vector<std::string> followers;

  ShardLeaderDiagnostic(std::string db, std::string col, std::string shardId,
                        std::string leader, std::string status,
                        std::vector<std::string> followers)
      : database(std::move(db)),
        collection(std::move(col)),
        shard(std::move(shardId)),
        leaderServer(std::move(leader)),
        leaderStatus(std::move(status)),
        followers(std::move(followers)) {}
};

// Find shards where the leader has a non-GOOD health status
std::vector<ShardLeaderDiagnostic> findShardsWithUnhealthyLeaders(
    const AgencyData& agencyData) {
  std::vector<ShardLeaderDiagnostic> result;

  // Get server health statuses
  const auto& health = agencyData.arango.Supervision.Health;

  // Access the Plan and its Collections
  const auto& plan = agencyData.arango.Plan;
  const auto& collectionsMap = plan.Collections;

  // Iterate through each database
  for (const auto& [dbName, dbCollections] : collectionsMap) {
    // Iterate through collections in this database
    for (const auto& [collectionId, collection] : dbCollections) {
      // Iterate through each shard in the collection
      for (const auto& [shardId, servers] : collection.shards) {
        // Skip empty server lists
        if (servers.empty()) {
          continue;
        }

        // The first server in the list is the leader
        const std::string& leaderServer = servers[0];

        // Check if this server has a health entry
        auto healthIt = health.find(leaderServer);
        if (healthIt == health.end()) {
          // Server not found in health data, consider it problematic
          std::vector<std::string> followers(servers.begin() + 1,
                                             servers.end());
          result.emplace_back(dbName, collection.name, shardId, leaderServer,
                              "UNKNOWN", followers);
          continue;
        }

        // Check if the leader's status is not GOOD
        const std::string& status = healthIt->second.Status;
        if (status != "GOOD") {
          // Extract follower servers
          std::vector<std::string> followers(servers.begin() + 1,
                                             servers.end());
          result.emplace_back(dbName, collection.name, shardId, leaderServer,
                              status, followers);
        }
      }
    }
  }

  return result;
}

void printShardsWithUnhealthyLeaders(
    const std::vector<ShardLeaderDiagnostic>& unhealthyLeaders,
    std::ostream& out) {
  if (unhealthyLeaders.empty()) {
    out << "No shards with unhealthy leaders found." << std::endl;
    return;
  }

  out << "Found " << unhealthyLeaders.size()
      << " shards with unhealthy leaders:" << std::endl;

  for (const auto& item : unhealthyLeaders) {
    out << "Database: " << item.database << std::endl;
    out << "  Collection: " << item.collection << std::endl;
    out << "  Shard: " << item.shard << std::endl;
    out << "  Leader Server: " << item.leaderServer << std::endl;
    out << "  Leader Status: " << item.leaderStatus << std::endl;

    if (!item.followers.empty()) {
      out << "  Followers: ";
      for (size_t i = 0; i < item.followers.size(); ++i) {
        out << item.followers[i];
        if (i < item.followers.size() - 1) {
          out << ", ";
        }
      }
      out << std::endl;
    } else {
      out << "  Followers: None" << std::endl;
    }
    out << std::endl;
  }
}

struct DistributeShardsLikeInconsistency {
  std::string database;
  std::string collectionId;
  std::string collectionName;
  std::string distributeShardsLike;
  std::string targetCollectionName;
  std::string inconsistencyType;
  std::string details;

  DistributeShardsLikeInconsistency(std::string db, std::string colId,
                                    std::string colName, std::string dsLike,
                                    std::string targetColName, std::string type,
                                    std::string dtls)
      : database(std::move(db)),
        collectionId(std::move(colId)),
        collectionName(std::move(colName)),
        distributeShardsLike(std::move(dsLike)),
        targetCollectionName(std::move(targetColName)),
        inconsistencyType(std::move(type)),
        details(std::move(dtls)) {}
};

// Find collections with inconsistent shard distribution compared to their
// distributeShardsLike reference
std::vector<DistributeShardsLikeInconsistency>
findDistributeShardsLikeInconsistencies(const AgencyData& agencyData) {
  std::vector<DistributeShardsLikeInconsistency> result;

  // Access the Plan and its Collections
  const auto& plan = agencyData.arango.Plan;
  const auto& collectionsMap = plan.Collections;

  // Function to extract number from shard ID (e.g., "s12345" -> 12345)
  auto extractShardNumber = [](const std::string& shardId) -> uint64_t {
    if (shardId.empty() || shardId[0] != 's') {
      return 0;  // Invalid format, return 0
    }
    try {
      return std::stoull(shardId.substr(1));
    } catch (...) {
      return 0;  // Conversion error, return 0
    }
  };

  // Sort shards by their numerical ID
  auto sortShardsByNumber =
      [&extractShardNumber](
          const std::unordered_map<std::string, std::vector<std::string>>&
              shards)
      -> std::vector<std::pair<std::string, std::vector<std::string>>> {
    std::vector<std::pair<std::string, std::vector<std::string>>> result;

    // First convert to vector for sorting
    for (const auto& [shardId, servers] : shards) {
      result.emplace_back(shardId, servers);
    }

    // Sort by numerical value of shard ID
    std::sort(result.begin(), result.end(),
              [&extractShardNumber](const auto& a, const auto& b) {
                return extractShardNumber(a.first) <
                       extractShardNumber(b.first);
              });

    return result;
  };

  // Iterate through each database
  for (const auto& [dbName, dbCollections] : collectionsMap) {
    // First pass: build a map from collectionId to collection for quick lookups
    std::unordered_map<std::string, const Collection*> idToCollection;
    for (const auto& [collectionId, collection] : dbCollections) {
      idToCollection[collectionId] = &collection;
    }

    // Second pass: check each collection with distributeShardsLike
    for (const auto& [collectionId, collection] : dbCollections) {
      // Skip collections without distributeShardsLike
      if (!collection.distributeShardsLike.has_value()) {
        continue;
      }

      const std::string& targetId = collection.distributeShardsLike.value();
      // Skip if the distributeShardsLike value is empty
      if (targetId.empty()) {
        continue;
      }

      // Find the target collection
      auto targetIt = idToCollection.find(targetId);
      if (targetIt == idToCollection.end()) {
        // Target collection doesn't exist
        result.emplace_back(dbName, collectionId, collection.name, targetId,
                            "MISSING", "MissingTargetCollection",
                            "The target collection specified in "
                            "distributeShardsLike does not exist");
        continue;
      }

      const Collection* targetCollection = targetIt->second;

      // Check if the number of shards is the same
      if (collection.shards.size() != targetCollection->shards.size()) {
        result.emplace_back(
            dbName, collectionId, collection.name, targetId,
            targetCollection->name, "DifferentNumberOfShards",
            "Collection has " + std::to_string(collection.shards.size()) +
                " shards, but target collection has " +
                std::to_string(targetCollection->shards.size()) + " shards");
        continue;
      }

      // Sort both collections' shards numerically
      auto sortedCollectionShards = sortShardsByNumber(collection.shards);
      auto sortedTargetShards = sortShardsByNumber(targetCollection->shards);

      // Compare each pair of shards in order
      for (size_t i = 0; i < sortedCollectionShards.size(); ++i) {
        const auto& [shardId, servers] = sortedCollectionShards[i];
        const auto& [targetShardId, targetServers] = sortedTargetShards[i];

        // If servers don't match, report inconsistency
        if (servers != targetServers) {
          std::stringstream serversStr, targetServersStr;
          for (size_t j = 0; j < servers.size(); ++j) {
            serversStr << servers[j];
            if (j < servers.size() - 1) {
              serversStr << ", ";
            }
          }
          for (size_t j = 0; j < targetServers.size(); ++j) {
            targetServersStr << targetServers[j];
            if (j < targetServers.size() - 1) {
              targetServersStr << ", ";
            }
          }

          std::string details =
              "Shard " + shardId + " has servers [" + serversStr.str() +
              "] which does not match the corresponding shard " +
              targetShardId + " in the target collection: [" +
              targetServersStr.str() + "]";

          result.emplace_back(dbName, collectionId, collection.name, targetId,
                              targetCollection->name,
                              "InconsistentShardDistribution", details);
        }
      }
    }
  }

  return result;
}

// Print function for the results
void printDistributeShardsLikeInconsistencies(
    const std::vector<DistributeShardsLikeInconsistency>& inconsistencies,
    std::ostream& out) {
  if (inconsistencies.empty()) {
    out << "No inconsistencies in distributeShardsLike configurations found."
        << std::endl;
    return;
  }

  out << "Found " << inconsistencies.size()
      << " collections with inconsistent distributeShardsLike configurations:"
      << std::endl;

  for (const auto& item : inconsistencies) {
    out << "Database: " << item.database << std::endl;
    out << "  Collection: " << item.collectionName
        << " (ID: " << item.collectionId << ")" << std::endl;
    out << "  Distributes shards like: " << item.distributeShardsLike;
    if (!item.targetCollectionName.empty()) {
      out << " (Name: " << item.targetCollectionName << ")";
    }
    out << std::endl;
    out << "  Inconsistency type: " << item.inconsistencyType << std::endl;
    out << "  Details: " << item.details << std::endl;
    out << std::endl;
  }
}

VPackBuilder diagnoseAgency(VPackSlice agency_vpack) {
  // Now parse the complete agency dump into a typed structure:
  VPackBuilder builder;
  AgencyData agency;
  try {
    arangodb::velocypack::deserialize(agency_vpack, agency);
  } catch (std::exception const& e) {
    LOG_TOPIC("76252", WARN, Logger::AGENCY)
        << "Caught exception when parsing agency data: " << e.what();
    {
      VPackObjectBuilder guard(&builder);
      builder.add("error", VPackValue(true));
      builder.add(
          "errorMessage",
          VPackValue(absl::StrCat("Could not parse agency data: ", e.what())));
      builder.add("diagnosis", VPackSlice::nullSlice());
    }
    return builder;
  }

  {
    VPackObjectBuilder guard(&builder);
    builder.add(VPackValue("diagnosis"));
    {
      VPackObjectBuilder guard2(&builder);

      std::vector<std::string> goodTests;
      std::vector<std::string> badTests;

      std::string testName;

      testName = "DuplicateCollectionNames";

      auto duplicates = findDuplicateCollectionNames(agency);
      if (duplicates.empty()) {
        goodTests.emplace_back(testName);
      } else {
        badTests.emplace_back(testName);
        std::stringstream out;
        printDuplicateCollections(duplicates, out);
        builder.add("duplicateCollectionNames", VPackValue(out.str()));
      }

      // Add the new test for unhealthy shard leaders
      testName = "UnhealthyShardLeaders";

      auto unhealthyLeaders = findShardsWithUnhealthyLeaders(agency);
      if (unhealthyLeaders.empty()) {
        goodTests.emplace_back(testName);
      } else {
        badTests.emplace_back(testName);
        std::stringstream out;
        printShardsWithUnhealthyLeaders(unhealthyLeaders, out);
        builder.add("unhealthyShardLeaders", VPackValue(out.str()));
      }

      // Add the new test for distributeShardsLike inconsistencies
      testName = "DistributeShardsLikeInconsistencies";

      auto inconsistencies = findDistributeShardsLikeInconsistencies(agency);
      if (inconsistencies.empty()) {
        goodTests.emplace_back(testName);
      } else {
        badTests.emplace_back(testName);
        std::stringstream out;
        printDistributeShardsLikeInconsistencies(inconsistencies, out);
        builder.add("distributeShardsLikeInconsistencies",
                    VPackValue(out.str()));
      }

      builder.add(VPackValue("goodTests"));
      arangodb::velocypack::serialize(builder, goodTests);
      builder.add(VPackValue("badTests"));
      arangodb::velocypack::serialize(builder, badTests);
    }
    builder.add("error", VPackValue(false));
    builder.add("errorMessage", VPackValue(""));
  }
  return builder;
}

VPackBuilder diagnoseAgency(arangodb::ArangodServer& server) {
  AgencyCache& ac = server.getFeature<ClusterFeature>().agencyCache();
  auto [agency_vpack, index] = ac.read(std::vector<std::string>({"/"}));
  TRI_ASSERT(agency_vpack->slice().isArray() &&
             agency_vpack->slice().length() == 1);
  return diagnoseAgency(agency_vpack->slice().at(0));
}

}  // namespace arangodb::agency
