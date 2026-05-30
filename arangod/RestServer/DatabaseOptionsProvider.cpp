////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "DatabaseOptionsProvider.h"

#include "Basics/FeatureFlags.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Replication2/Version.h"

namespace arangodb {

using namespace arangodb::options;

void DatabaseOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> opts, DatabaseFeatureOptions& options) {
  opts->addSection("database", "database options");

  auto static allowedReplicationVersions = [] {
    using namespace arangodb::replication;
    using namespace arangodb::replication2;

    auto result = std::unordered_set<std::string>{};
    result.emplace(versionToString(Version::ONE));
    if (EnableReplication2) {
      result.emplace(versionToString(Version::TWO));
    }
    return result;
  }();

  opts->addOption(
          "--database.default-replication-version",
          "The replication version to use unless overwritten "
          "when creating a new database.",
          new DiscreteValuesParameter<StringParameter>(
              &options.defaultReplicationVersion, allowedReplicationVersions),
          makeDefaultFlags(Flags::Uncommon, Flags::Experimental))
      .setIntroducedIn(31200);

  opts->addOption(
      "--database.wait-for-sync",
      "The default waitForSync behavior. Can be overwritten when creating a "
      "collection.",
      new BooleanParameter(&options.defaultWaitForSync),
      makeDefaultFlags(Flags::Uncommon));

  // the following option was obsoleted in 3.9
  opts->addObsoleteOption(
      "--database.force-sync-properties",
      "Force syncing of collection properties to disk after creating a "
      "collection or updating its properties. Otherwise, let the waitForSync "
      "property of each collection determine it.",
      false);

  opts->addOption("--database.ignore-datafile-errors",
                  "Load collections even if datafiles may contain errors.",
                  new BooleanParameter(&options.ignoreDatafileErrors),
                  makeDefaultFlags(Flags::Uncommon));

  opts->addOption("--database.extended-names",
                  "Allow most UTF-8 characters in the names of databases, "
                  "collections, Views, and indexes. Once in use, "
                  "this option cannot be turned off again.",
                  new BooleanParameter(&options.extendedNames),
                  makeDefaultFlags(Flags::Uncommon, Flags::Experimental))
      .setIntroducedIn(30900);

  opts->addOldOption("database.extended-names-databases",
                     "database.extended-names");

  opts->addOption("--database.io-heartbeat",
                  "Perform I/O heartbeat to test the underlying volume.",
                  new BooleanParameter(&options.performIOHeartbeat),
                  makeDefaultFlags(Flags::Uncommon))
      .setIntroducedIn(30807)
      .setIntroducedIn(30902);

  opts->addOption("--database.max-databases",
                  "The maximum number of databases that can exist in parallel.",
                  new SizeTParameter(&options.maxDatabases))
      .setLongDescription(R"(If the maximum number of databases is reached, no
additional databases can be created in the deployment. In order to create additional
databases, other databases need to be removed first.")")
      .setIntroducedIn(31200);

  // the following option was obsoleted in 3.9
  opts->addObsoleteOption(
      "--database.old-system-collections",
      "Create and use deprecated system collection (_modules, _fishbowl).",
      false);

  // the following option was obsoleted in 3.8
  opts->addObsoleteOption(
      "--database.throw-collection-not-loaded-error",
      "throw an error when accessing a collection that is still loading",
      false);

  // the following option was removed in 3.7
  opts->addObsoleteOption(
      "--database.maximal-journal-size",
      "default maximal journal size, can be overwritten when "
      "creating a collection",
      true);

  // the following option was removed in 3.2
  opts->addObsoleteOption(
      "--database.index-threads",
      "threads to start for parallel background index creation", true);

  // the following hidden option was removed in 3.4
  opts->addObsoleteOption(
      "--database.check-30-revisions",
      "check for revision values from ArangoDB 3.0 databases", true);

  // the following options were removed in 3.2
  opts->addObsoleteOption(
      "--database.revision-cache-chunk-size",
      "chunk size (in bytes) for the document revisions cache", true);
  opts->addObsoleteOption(
      "--database.revision-cache-target-size",
      "total target size (in bytes) for the document revisions cache", true);

  opts->addObsoleteOption("--server.storage-engine", "The storage engine type",
                          true);
}

}  // namespace arangodb
