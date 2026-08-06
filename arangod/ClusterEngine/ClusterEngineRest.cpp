#include "ClusterEngine/ClusterEngine.h"

#include "ClusterEngine/ClusterRestHandlers.h"
#include "GeneralServer/RestHandlerFactory.h"

namespace arangodb {

void ClusterEngine::addRestHandlers(
    rest::RestHandlerFactory& handlerFactory) {
  ClusterRestHandlers::registerResources(&handlerFactory);
}

}  // namespace arangodb