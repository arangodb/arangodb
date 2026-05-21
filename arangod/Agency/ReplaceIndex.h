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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Job.h"
#include "Supervision.h"

namespace arangodb::consensus {

// Performs a hot-swap of an index (only vector index for now) by adding a
// sibling "shadow" entry to Plan/Collections/{db}/{coll}/indexes that carries
// a `replaces: <oldId>` marker. DBServers build the shadow through the
// standard maintenance loop; this job watches Current, then either commits
// (removes the old entry, strips the marker) or aborts (drops the shadow).
struct ReplaceIndex : public Job {
  ReplaceIndex(Node const& snapshot, AgentInterface* agent,
               std::string const& jobId, std::string const& creator,
               std::string const& database, std::string const& collection,
               std::string const& oldIndexId, std::string const& newIndexId,
               std::shared_ptr<velocypack::Builder> newDefinition);

  // Used by JobContext to rehydrate the job from an agency snapshot after a
  // coordinator restart or between supervision passes.
  ReplaceIndex(Node const& snapshot, AgentInterface* agent, JOB_STATUS status,
               std::string const& jobId);

  ~ReplaceIndex() override;

  JOB_STATUS status() override final;
  bool create(
      std::shared_ptr<velocypack::Builder> envelope = nullptr) override final;
  void run(bool&) override final;
  bool start(bool&) override final;
  Result abort(std::string const& reason) override final;

  // Add the ReplaceIndex job fields into an already-open object in `builder`.
  // The REST handler and `create()` both call this so writer and reader stay
  // in sync on the schema.
  static void appendJobPayload(
      velocypack::Builder& builder, std::string const& jobId,
      std::string const& creator, std::string const& database,
      std::string const& collection, std::string const& oldIndexId,
      std::string const& newIndexId, velocypack::Slice newDefinition);

  std::string _database;
  std::string _collection;
  std::string _oldIndexId;
  std::string _newIndexId;
  std::string _timeCreated;
  std::shared_ptr<velocypack::Builder> _newDefinition;
};

}  // namespace arangodb::consensus
