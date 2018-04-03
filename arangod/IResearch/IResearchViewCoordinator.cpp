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
    bool /*isNew*/
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
  // ???
  result.add(_info.slice());
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
