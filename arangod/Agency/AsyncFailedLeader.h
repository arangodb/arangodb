////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_ASYNC_FAILED_LEADER_H
#define ARANGOD_CONSENSUS_ASYNC_FAILED_LEADER_H 1

#include "Job.h"
#include "Supervision.h"

namespace arangodb {
namespace consensus {

struct AsyncFailedLeader final : public Job {
  
  AsyncFailedLeader(Node const& snapshot, AgentInterface* agent, std::string const& jobId,
                    std::string const& creator,
                    std::string const& failed);

  AsyncFailedLeader(Node const& snapshot, AgentInterface* agent,
                    JOB_STATUS status, std::string const& jobId);

  virtual ~AsyncFailedLeader();

  virtual JOB_STATUS status() override final;
  virtual void run() override final;
  virtual bool create(std::shared_ptr<VPackBuilder> envelope = nullptr)
    override final;
  virtual bool start() override final;
  virtual Result abort() override final;
  
  /*JOB_STATUS pendingLeader();
  JOB_STATUS pendingFollower();*/
  
private:
  
  std::string findBestFollower(Node const& snapshot);

private:
  std::string _server;
};
}
}

#endif
