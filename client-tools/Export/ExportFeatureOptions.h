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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace arangodb {

struct ExportFeatureOptions {
  ExportFeatureOptions();

  std::vector<std::string> collections;
  std::string customQuery;
  std::string customQueryFile;
  std::string customQueryBindVars;
  std::shared_ptr<VPackBuilder> customQueryBindVarsBuilder;
  std::string graphName;
  std::string xgmmlLabelAttribute = "label";
  std::string typeExport = "jsonl";
  std::string csvFieldOptions;
  std::vector<std::string> csvFields;
  std::string outputDirectory;
  double customQueryMaxRuntime = 0.0;
  bool useMaxRuntime = false;
  bool escapeCsvFormulae = true;
  bool xgmmlLabelOnly = false;
  bool overwrite = false;
  bool progress = true;
  bool useGzip = false;
  uint64_t documentsPerBatch = 1000;
};

}  // namespace arangodb
