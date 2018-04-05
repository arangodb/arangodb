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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewCoordinator.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>

namespace arangodb {
namespace iresearch {

/*static*/ std::shared_ptr<LogicalView> IResearchViewCoordinator::make(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    uint64_t planVersion
) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  return std::unique_ptr<IResearchViewCoordinator>(
    new IResearchViewCoordinator(vocbase, info)
  );
}

IResearchViewCoordinator::IResearchViewCoordinator(
    TRI_vocbase_t& vocbase, velocypack::Slice info
) : LogicalView(&vocbase, info), _info(info) {
}

void IResearchViewCoordinator::toVelocyPack(
    velocypack::Builder& result,
    bool includeProperties,
    bool includeSystem
) const {
  result.add(velocypack::ObjectIterator(_info.slice()));
}

arangodb::Result IResearchViewCoordinator::updateProperties(
    velocypack::Slice const& properties,
    bool partialUpdate,
    bool doSync
) {
  // FIXME something like:
  // ClusterInfo* ci = ClusterInfo::instance();
  // ci->setViewPropertiesCoordinator(properties);
  return { TRI_ERROR_NOT_IMPLEMENTED };
}

// FIXME return int???
void IResearchViewCoordinator::drop() {
  ClusterInfo* ci = ClusterInfo::instance();

  std::string errorMsg;

  int const res = ci->dropViewCoordinator(
    vocbase()->name(), // database name
    basics::StringUtils::itoa(id()), // view id
    errorMsg
  );

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "Could not drop view in agency, error: " << errorMsg
      << ", errorCode: " << res;
  }
}
} // iresearch
} // arangodb
