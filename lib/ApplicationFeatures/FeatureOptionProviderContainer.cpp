#include "FeatureOptionProviderContainer.h"

namespace arangodb::application_features {

void FeatureOptionProviderContainer::declareOptions(
    std::shared_ptr<options::ProgramOptions> programOptions) {
  std::apply(
      [&](auto&... providers) {
        (providers.declareOptions(programOptions), ...);
      },
      _providers);
}

void FeatureOptionProviderContainer::validateOptions(
    std::shared_ptr<options::ProgramOptions> programOptions) {
  std::apply(
      [&](auto&... providers) {
        (providers.validateOptions(programOptions), ...);
      },
      _providers);
}
}  // namespace arangodb::application_features
