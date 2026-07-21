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

  void validateOptions() override {
    ApplicationServer::validateOptions();
    _optionProviders.validateOptions(options());
  }

  // After CLI parse: keep feature loadOptions (Logger early levels), then load
  // .conf via ConfigOptionsProvider. Requires Providers to include
  // ConfigOptionsProvider and VersionOptionsProvider (true for
  // CoreOptionProviders).
  void loadAdditionalOptions() override {
    ApplicationServer::loadAdditionalOptions();
    auto const& versionOptions = getOptions<VersionOptionsProvider>();
    loadConfigAndEarlyLoggerOptions(
        getProvider<LoggerOptionsProvider>(),
        getProvider<ConfigOptionsProvider>(),
        versionOptions.printVersion || versionOptions.printVersionJson,
        options(), getBinaryPath(), _binaryName);
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
