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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ConfigOptionsProvider.h"
#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/VersionOptionsProvider.h"

namespace arangodb {

template<class Providers>
class OptionProvidingServer : public application_features::ApplicationServer {
 public:
  OptionProvidingServer(std::shared_ptr<options::ProgramOptions> options,
                        char const* binaryPath, std::string binaryName,
                        int* ret)
      : ApplicationServer(options, binaryPath),
        _binaryName(std::move(binaryName)),
        _ret(ret) {}

 protected:
  void collectOptions() override {
    ApplicationServer::collectOptions();
    _optionProviders.declareOptions(options());
  }

  void processOptions() override {
    ApplicationServer::processOptions();
    _optionProviders.processOptions(options());
  }

  void validateOptions() override {
    // This inverted order (provider.validateOptions() ->
    // feature.validateOptions()) is intentional. For example,
    // VersionOptionsProvider.validateOptions() has to be called before
    // V8DealerFeature.validateOptions() to ensure `--version` command is
    // printed and the server exits early. Otherwise,
    // V8DealerFeature.validateOptions() will abort (FATAL "no
    // javascript.startup-directory").
    // TODO: Find the right declaration order in the tuple of providers (at
    // least VersionOptionsProvider comes before V8DealerOptionsProvider).
    _optionProviders.validateOptions(options());
    ApplicationServer::validateOptions();
  }

  template<class ProviderType>
  auto& getOptions() {
    return _optionProviders.template getOptions<ProviderType>();
  }

  template<class ProviderType>
  auto const& getOptions() const {
    return _optionProviders.template getOptions<ProviderType>();
  }

  template<class ProviderType>
  ProviderType& getProvider() {
    return _optionProviders.template getProvider<ProviderType>();
  }

  std::string _binaryName;
  int* _ret;

 private:
  Providers _optionProviders;
};

}  // namespace arangodb
