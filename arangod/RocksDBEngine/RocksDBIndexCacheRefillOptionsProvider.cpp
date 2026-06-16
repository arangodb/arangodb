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

#include "RocksDBIndexCacheRefillOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void RocksDBIndexCacheRefillOptionsProvider::declareOptions(
    std::shared_ptr<options::ProgramOptions> options,
    RocksDBIndexCacheRefillFeatureOptions& opts) {
  options
      ->addOption("--rocksdb.auto-fill-index-caches-on-startup",
                  "Whether to automatically fill the in-memory index caches "
                  "with entries from edge indexes and cache-enabled persistent "
                  "indexes on server startup.",
                  new options::BooleanParameter(&opts.fillOnStartup),
                  arangodb::options::makeFlags(
                      options::Flags::DefaultNoComponents,
                      options::Flags::OnDBServer, options::Flags::OnSingle))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(Enabling this option may cause additional CPU and
I/O load. You can limit how many index filling operations can execute
concurrently with the `--rocksdb.max-concurrent-index-fill-tasks` startup
option.)");

  options
      ->addOption("--rocksdb.auto-refill-index-caches-on-modify",
                  "Whether to automatically (re-)fill the in-memory index "
                  "caches with entries from edge indexes and cache-enabled "
                  "persistent indexes on insert/update/replace/remove "
                  "operations by default.",
                  new options::BooleanParameter(&opts.autoRefill),
                  arangodb::options::makeFlags(
                      options::Flags::DefaultNoComponents,
                      options::Flags::OnDBServer, options::Flags::OnSingle))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(When documents are added, modified, or removed,
these changes are tracked and a background thread tries to update the index
caches accordingly if the feature is enabled, by adding new, updating existing,
or deleting and refilling cache entries.

You can enable the feature for individual `INSERT`, `UPDATE`, `REPLACE`,  and
`REMOVE` operations in AQL queries, for individual document API requests that
insert, update, replace, or remove single or multiple documents, as well
as enable it by default using this startup option.

The background refilling is done on a best-effort basis and not guaranteed to
succeed, for example, if there is no memory available for the cache subsystem,
or during cache grow/shrink operations. A background thread is used so that
foreground write operations are not slowed down by a lot. It may still cause
additional I/O activity to look up data from the storage engine to repopulate
the cache.)");

  options
      ->addOption(
          "--rocksdb.auto-refill-index-caches-queue-capacity",
          "How many changes can be queued at most for automatically refilling "
          "the index caches.",
          new options::SizeTParameter(&opts.maxCapacity),
          options::makeFlags(options::Flags::DefaultNoComponents,
                             options::Flags::OnDBServer,
                             options::Flags::OnSingle))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(This option restricts how many cache entries
the background thread for (re-)filling the in-memory index caches can queue at
most. This limits the memory usage for the case of the background thread being
slower than other operations that invalidate cache entries of edge indexes
or cache-enabled persistent indexes.)");

  options
      ->addOption(
          "--rocksdb.max-concurrent-index-fill-tasks",
          "The maximum number of index fill tasks that can run concurrently on "
          "server startup.",
          new options::SizeTParameter(&opts.maxConcurrentIndexFillTasks,
                                      /*minValue*/ 1),
          options::makeFlags(options::Flags::DefaultNoComponents,
                             options::Flags::OnDBServer,
                             options::Flags::OnSingle, options::Flags::Dynamic))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(The lower this number, the lower the impact of the
index cache filling, but the longer it takes to complete.)");

  options
      ->addOption(
          "--rocksdb.auto-refill-index-caches-on-followers",
          "Whether or not to automatically (re-)fill the in-memory index "
          "caches on followers as well.",
          new options::BooleanParameter(&opts.autoRefillOnFollowers),
          arangodb::options::makeFlags(options::Flags::DefaultNoComponents,
                                       options::Flags::OnDBServer,
                                       options::Flags::OnSingle))
      .setIntroducedIn(31005)
      .setLongDescription(R"(Set this to `false` to only (re-)fill in-memory
index caches on leaders and save memory on followers.
Note that the value of this option should be identical for all DBServers.)");
}

}  // namespace arangodb
