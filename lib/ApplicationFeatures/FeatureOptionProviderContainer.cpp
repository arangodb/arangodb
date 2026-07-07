#include "FeatureOptionProviderContainer.h"

namespace arangodb::application_features {

void FeatureOptionProviderContainer::declareOptions(
    std::shared_ptr<options::ProgramOptions> programOptions) {
  std::apply([&](auto& provider) { provider.declareOptions(programOptions); },
             _providers);
}

void FeatureOptionProviderContainer::validateOptions(
    std::shared_ptr<options::ProgramOptions> programOptions) {
  std::apply([&](auto provider) { provider.validateOptions(programOptions); },
             _providers);
}
}  // namespace arangodb::application_features
