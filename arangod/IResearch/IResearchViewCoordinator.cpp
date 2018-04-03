#include "IResearchViewCoordinator.h"

namespace arangodb {
namespace iresearch {

/*static*/ std::shared_ptr<LogicalView> IResearchViewCoordinator::make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew 
) {
  // FIXME implement
  return nullptr;
}


} // iresearch
} // arangodb
