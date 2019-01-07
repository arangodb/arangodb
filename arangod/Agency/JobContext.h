////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_JOB_CONTEXT_H
#define ARANGOD_CONSENSUS_JOB_CONTEXT_H 1

#include "Job.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <string>

namespace arangodb {
namespace consensus {

class JobContext {
 public:
  /// @brief Contextualize arbitrary Job
  JobContext(JOB_STATUS status, std::string id, Node const& snapshot, AgentInterface* agent);

  /// @brief Create job
  void create(std::shared_ptr<VPackBuilder> b = nullptr);

  /// @brief Start job
  void start();

  /// @brief Run job
  void run();

  /// @brief Abort job
  void abort();

 private:
  /// @brief Actual job context
  std::unique_ptr<Job> _job;
};

}  // namespace consensus
}  // namespace arangodb

#endif
