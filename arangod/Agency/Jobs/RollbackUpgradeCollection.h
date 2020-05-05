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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AGENCY_JOB_ROLLBACK_UPGRADE_COLLECTION_H
#define ARANGOD_AGENCY_JOB_ROLLBACK_UPGRADE_COLLECTION_H 1

#include <velocypack/Builder.h>

#include "Agency/Job.h"
#include "Agency/Supervision.h"

namespace arangodb {
namespace consensus {

struct RollbackUpgradeCollection : public Job {
  RollbackUpgradeCollection(Supervision& supervision, Node const& snapshot,
                            AgentInterface* agent, JOB_STATUS status,
                            std::string const& jobId);

  virtual ~RollbackUpgradeCollection();

  virtual bool create(std::shared_ptr<VPackBuilder> b = nullptr) override final;
  virtual void run(bool&) override final;
  virtual bool start(bool&) override final;
  virtual JOB_STATUS status() override final;
  virtual Result abort(std::string const& reason) override final;

 private:
  std::string jobPrefix() const;
  velocypack::Slice job() const;
  bool writeTransaction(velocypack::Builder const&, std::string const&);
  bool registerError(std::string const&);

  std::string _database;
  std::string _collection;
  std::string _failedId;
  std::chrono::system_clock::time_point _created;
  std::string _error;
  bool _smartChild;
};
}  // namespace consensus
}  // namespace arangodb

#endif
