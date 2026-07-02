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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/files.h"
#include "Export/ExportFeatureOptions.h"
#include "Rest/CommonDefines.h"
#include "Utils/ManagedDirectory.h"

#include <memory>

namespace arangodb {

namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
}  // namespace httpclient

class ExportFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Export"; }

  ExportFeature(application_features::ApplicationServer& server, int* result,
                ExportFeatureOptions options);
  ExportFeature(application_features::ApplicationServer& server, int* result);
  ~ExportFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override final;
  void start() override final;
  std::shared_ptr<VPackBuilder> customQueryBindVars() const {
    return _options.customQueryBindVarsBuilder;
  }

 private:
  void collectionExport(httpclient::SimpleHttpClient* httpClient);
  void queryExport(httpclient::SimpleHttpClient* httpClient);
  void writeFirstLine(ManagedDirectory::File& fd, std::string const& fileName,
                      std::string const& collection);
  void writeBatch(ManagedDirectory::File& fd, VPackArrayIterator it,
                  std::string const& fileName);
  void graphExport(httpclient::SimpleHttpClient* httpClient);
  void writeGraphBatch(ManagedDirectory::File& fd, VPackArrayIterator it,
                       std::string const& fileName);
  void xgmmlWriteOneAtt(ManagedDirectory::File& fd, VPackSlice const& slice,
                        std::string const& name, int deep = 0);

  void writeToFile(ManagedDirectory::File& fd, std::string const& string);
  std::shared_ptr<VPackBuilder> httpCall(
      httpclient::SimpleHttpClient* httpClient, std::string const& url,
      arangodb::rest::RequestType, std::string postBody = "");

  void appendCsvStringValue(std::string& output, std::string const& value);

  ExportFeatureOptions _options;

  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
  bool _firstLine = true;
  uint64_t _skippedDeepNested = 0;
  uint64_t _httpRequestsDone = 0;
  std::string _currentCollection;
  std::string _currentGraph;
  std::unique_ptr<ManagedDirectory> _directory;

  int* _result;
};

}  // namespace arangodb
