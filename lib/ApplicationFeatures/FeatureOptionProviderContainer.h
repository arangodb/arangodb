////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ProgramOptions/ProgramOptions.h"

#include <tuple>

namespace arangodb::application_features {

namespace {
template<class Provider>
concept HasProcessOptions =
    requires(Provider& provider,
             std::shared_ptr<options::ProgramOptions> programOptions) {
  provider.processOptionsImpl(programOptions,
                              std::declval<typename Provider::Options&>());
};
}  // namespace

template<class... Providers>
class FeatureOptionProviderContainer final {
 public:
  void declareOptions(std::shared_ptr<options::ProgramOptions> programOptions) {
    std::apply(
        [&](auto&... providers) {
          (providers.declareOptions(programOptions), ...);
        },
        _providers);
  }

  void processOptions(std::shared_ptr<options::ProgramOptions> programOptions) {
    std::apply(
        [&](auto&... providers) {
          (processProviderOptions(programOptions, providers), ...);
        },
        _providers);
  }

  void validateOptions(
      std::shared_ptr<options::ProgramOptions> programOptions) {
    std::apply(
        [&](auto&... providers) {
          (providers.validateOptions(programOptions), ...);
        },
        _providers);
  }

  template<typename ProviderType>
  auto const& getOptions() const {
    return std::get<ProviderType>(_providers).options();
  }

 private:
  template<class Provider>
  void processProviderOptions(
      std::shared_ptr<options::ProgramOptions> programOptions,
      Provider& provider) {
    if constexpr (HasProcessOptions<Provider>) {
      provider.processOptions(programOptions);
    }
  }
  std::tuple<Providers...> _providers{};
};
}  // namespace arangodb::application_features
