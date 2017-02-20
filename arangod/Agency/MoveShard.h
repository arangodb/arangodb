////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_MOVE_SHARD_FROM_FOLLOWER_H
#define ARANGOD_CONSENSUS_MOVE_SHARD_FROM_FOLLOWER_H 1

#include "Job.h"
#include "Supervision.h"

namespace arangodb {
namespace consensus {

struct MoveShard : public Job {
  MoveShard(Node const& snapshot, Agent* agent, std::string const& jobId,
            std::string const& creator, std::string const& prefix,
            std::string const& database = std::string(),
            std::string const& collection = std::string(),
            std::string const& shard = std::string(),
            std::string const& from = std::string(),
            std::string const& to = std::string());

  virtual ~MoveShard();

  virtual JOB_STATUS status() override;
  virtual bool create() override;
  virtual bool start() override;

  std::string _database;
  std::string _collection;
  std::string _shard;
  std::string _from;
  std::string _to;
};
}
}

#endif
