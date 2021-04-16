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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IMPORT_IMPORT_FEATURE_H
#define ARANGODB_IMPORT_IMPORT_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Shell/ClientFeature.h"
#include "V8Client/ArangoClientHelper.h"

namespace arangodb {

namespace httpclient {

class GeneralClientConnection;
class SimpleHttpClient;
class SimpleHttpResult;

}  // namespace httpclient

class ImportFeature final : public application_features::ApplicationFeature,
                            public ArangoClientHelper {
 public:
  ImportFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) override;
  void start() override;

 private:
  ErrorCode tryCreateDatabase(ClientFeature& client, std::string const& name);

  std::string _filename;
  bool _useBackslash;
  bool _convert;
  bool _autoChunkSize;
  uint64_t _chunkSize;
  uint32_t _threadCount;
  std::string _collectionName;
  std::string _fromCollectionPrefix;
  std::string _toCollectionPrefix;
  bool _createCollection;
  bool _createDatabase;
  std::string _createCollectionType;
  std::string _typeImport;
  std::string _headersFile;
  std::vector<std::string> _translations;
  std::vector<std::string> _removeAttributes;
  bool _overwrite;
  std::string _quote;
  std::string _separator;
  bool _progress;
  bool _ignoreMissing;
  std::string _onDuplicateAction;
  uint64_t _rowsToSkip;
  int* _result;
  bool _skipValidation;
  bool _latencyStats;
};

}  // namespace arangodb

#endif
