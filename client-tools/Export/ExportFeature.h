////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#pragma once

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/files.h"
#include "Rest/CommonDefines.h"
#include "Utils/ManagedDirectory.h"
#include "V8Client/ArangoClientHelper.h"

namespace arangodb {

namespace httpclient {

class GeneralClientConnection;
class SimpleHttpClient;
class SimpleHttpResult;

}  // namespace httpclient

class ExportFeature final : public application_features::ApplicationFeature,
                            public ArangoClientHelper {
 public:
  ExportFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override final;
  void start() override final;
  std::shared_ptr<VPackBuilder> customQueryBindVars() const { return _customQueryBindVarsBuilder; }

 private:
  void collectionExport(httpclient::SimpleHttpClient* httpClient);
  void queryExport(httpclient::SimpleHttpClient* httpClient);
  void writeFirstLine(ManagedDirectory::File& fd, std::string const& fileName, std::string const& collection);
  void writeBatch(ManagedDirectory::File& fd, VPackArrayIterator it, std::string const& fileName);
  void graphExport(httpclient::SimpleHttpClient* httpClient);
  void writeGraphBatch(ManagedDirectory::File& fd, VPackArrayIterator it, std::string const& fileName);
  void xgmmlWriteOneAtt(ManagedDirectory::File& fd, VPackSlice const& slice,
                        std::string const& name, int deep = 0);

  void writeToFile(ManagedDirectory::File& fd, std::string const& string);
  std::shared_ptr<VPackBuilder> httpCall(httpclient::SimpleHttpClient* httpClient,
                                         std::string const& url, arangodb::rest::RequestType,
                                         std::string postBody = "");

  void appendCsvStringValue(std::string& output, std::string const& value);

  std::vector<std::string> _collections;
  std::string _customQuery;
  std::string _graphName;
  std::string _xgmmlLabelAttribute;
  std::string _typeExport;
  std::string _csvFieldOptions;
  std::vector<std::string> _csvFields;
  std::string _outputDirectory;
  double _customQueryMaxRuntime;
  bool _useMaxRuntime; 
  bool _escapeCsvFormulae;
  bool _xgmmlLabelOnly;
  bool _overwrite;
  bool _progress;
  bool _useGzip;
  bool _firstLine;
  uint64_t _documentsPerBatch;
  uint64_t _skippedDeepNested;
  uint64_t _httpRequestsDone;
  std::string _currentCollection;
  std::string _currentGraph;
  std::string _customQueryBindVars;
  std::shared_ptr<VPackBuilder> _customQueryBindVarsBuilder;
  std::unique_ptr<ManagedDirectory> _directory;

  int* _result;
};

}  // namespace arangodb

