#include "ImportFeatureOptions.h"

#include "Basics/NumberOfCores.h"

namespace arangodb {

ImportFeatureOptions::ImportFeatureOptions() {
  threadCount =
      std::max(threadCount, static_cast<uint32_t>(NumberOfCores::getValue()));
}

}  // namespace arangodb
