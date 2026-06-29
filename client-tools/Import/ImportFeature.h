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
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Import/ImportFeatureOptions.h"
#include "Shell/ClientFeature.h"

#include <memory>

namespace arangodb {

namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
}  // namespace httpclient

class ImportFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Import"; }

  ImportFeature(application_features::ApplicationServer& server, int* result);
  ~ImportFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;

 private:
  ErrorCode tryCreateDatabase(ClientFeature& client, std::string const& name);

  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
  ImportFeatureOptions _options;
  int* _result;
};

}  // namespace arangodb
