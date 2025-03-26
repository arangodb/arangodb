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

std::string diagnoseAgency(VPackSlice agency_vpack) {
  // Now parse the complete agency dump into a typed structure:
  AgencyData agency;
  try {
    arangodb::velocypack::deserialize(agency_vpack, agency);
  } catch (std::exception const& e) {
    LOG_DEVEL << "Caught exception when parsing agency data: " << e.what();
    return absl::StrCat("Could not parse agency data: ", e.what());
  }

  auto duplicates = findDuplicateCollectionNames(agency);
  std::stringstream out;
  printDuplicateCollections(duplicates, out);

  return out.str();
}

std::string diagnoseAgency(arangodb::ArangodServer& server) {
  AgencyCache& ac = server.getFeature<ClusterFeature>().agencyCache();
  auto [agency_vpack, index] = ac.read(std::vector<std::string>({"/"}));
  TRI_ASSERT(agency_vpack->slice().isArray() &&
             agency_vpack->slice().length() == 1);
  return diagnoseAgency(agency_vpack->slice().at(0));
}

}  // namespace arangodb::agency
