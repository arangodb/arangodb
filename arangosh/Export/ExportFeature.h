////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_EXPORT_EXPORT_FEATURE_H
#define ARANGODB_EXPORT_EXPORT_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "V8Client/ArangoClientHelper.h"
#include "lib/Rest/CommonDefines.h"
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
class SimpleHttpResult;
}

class ExportFeature final : public application_features::ApplicationFeature,
                               public ArangoClientHelper {
 public:
  ExportFeature(application_features::ApplicationServer* server,
                   int* result);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override final;
  void start() override final;

 private:
  void collectionExport(httpclient::SimpleHttpClient* httpClient);
  void queryExport(httpclient::SimpleHttpClient* httpClient);
  void writeFirstLine(int fd, std::string const& fileName, std::string const& collection);
  void writeBatch(int fd, VPackArrayIterator it, std::string const& fileName);
  void graphExport(httpclient::SimpleHttpClient* httpClient);
  void writeGraphBatch(int fd, VPackArrayIterator it, std::string const& fileName);
  void xgmmlWriteOneAtt(int fd, std::string const& fileName, VPackSlice const& slice, std::string const& name, int deep = 0);

  void writeToFile(int fd, std::string const& string, std::string const& fileName);
  std::shared_ptr<VPackBuilder> httpCall(httpclient::SimpleHttpClient* httpClient, std::string const& url, arangodb::rest::RequestType, std::string postBody = "");

 private:
  std::vector<std::string> _collections;
  std::string _query;
  std::string _graphName;
  std::string _xgmmlLabelAttribute;
  std::string _typeExport;
  std::string _csvFieldOptions;
  std::vector<std::string> _csvFields;
  bool        _xgmmlLabelOnly;

  std::string _outputDirectory;
  bool _overwrite;
  bool _progress;

  bool _firstLine;
  uint64_t _skippedDeepNested;
  uint64_t _httpRequestsDone;
  std::string _currentCollection;
  std::string _currentGraph;

  int* _result;
};
}

#endif
