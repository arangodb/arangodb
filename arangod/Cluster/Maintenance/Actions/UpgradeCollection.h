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

#ifndef ARANGODB_CLUSTER_MAINTENANCE_ACTIONS_UPGRADE_COLLECTION_H
#define ARANGODB_CLUSTER_MAINTENANCE_ACTIONS_UPGRADE_COLLECTION_H

#include "Cluster/Maintenance/ActionBase.h"
#include "Cluster/Maintenance/ActionDescription.h"
#include "Network/Methods.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::maintenance {

class UpgradeCollection : public ActionBase,
                          public std::enable_shared_from_this<UpgradeCollection> {
 public:
  UpgradeCollection(MaintenanceFeature&, ActionDescription const&);

  virtual ~UpgradeCollection() = default;

  virtual bool first() override final;

  virtual bool next() override final;

 private:
  bool hasMore(LogicalCollection::UpgradeStatus::State) const;

  futures::Future<Result> sendRequest(LogicalCollection&, std::string const&,
                                      LogicalCollection::UpgradeStatus::State);

  std::function<Result(network::Response&&)> handleResponse(std::string const&,
                                                            LogicalCollection::UpgradeStatus::State);

  bool processPhase(std::unordered_map<LogicalCollection::UpgradeStatus::State, std::vector<std::string>> servers,
                    LogicalCollection::UpgradeStatus::State searchPhase,
                    LogicalCollection::UpgradeStatus::State targetPhase,
                    LogicalCollection& collection, std::unique_lock<std::mutex>& lock);

  bool refreshPlanStatus();

  void setError(LogicalCollection&, std::string const&);
  void setError(LogicalCollection&, std::string const&, std::unique_lock<std::mutex>&);

  void wait() const;

 private:
  velocypack::Builder _planStatus;
  std::size_t _timeout;
  bool _isSmartChild;
  std::atomic<bool> _inRollback;
  std::unique_ptr<transaction::Methods> _trx;
  std::unordered_map<std::string, std::pair<LogicalCollection::UpgradeStatus::State, futures::Future<Result>>> _futures;
};

}  // namespace arangodb::maintenance

#endif
