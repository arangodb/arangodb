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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ActionBase.h"
#include "ActionDescription.h"
#include "Cluster/ClusterTypes.h"

struct TRI_vocbase_t;
namespace arangodb {
class LogicalCollection;
}

namespace arangodb::maintenance {
class DropCollection : public ActionBase, ShardDefinition {
 public:
  DropCollection(MaintenanceFeature&, ActionDescription const&);

  virtual ~DropCollection();

  virtual bool first() override final;
  void setState(ActionState state) override final;

 private:
  static Result dropCollectionReplication2(
      ShardID const& shard, std::shared_ptr<LogicalCollection>& coll);
};
}  // namespace arangodb::maintenance
