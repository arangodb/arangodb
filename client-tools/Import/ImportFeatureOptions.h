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

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

struct ImportFeatureOptions {
  std::string filename;
  bool useBackslash = false;
  bool convert = true;
  bool autoChunkSize = false;
  uint64_t chunkSize = 1024 * 1024 * 4;
  uint32_t threadCount = 2;
  std::string collectionName;
  std::string fromCollectionPrefix;
  std::string toCollectionPrefix;
  bool overwriteCollectionPrefix = false;
  bool createCollection = false;
  bool createDatabase = false;
  std::string createCollectionType = "document";
  std::string typeImport = "auto";
  std::string headersFile;
  std::vector<std::string> translations;
  std::vector<std::string> datatypes;
  std::vector<std::string> removeAttributes;
  bool overwrite = false;
  std::string quote = "\"";
  std::string separator;
  bool progress = true;
  bool ignoreMissing = false;
  std::string onDuplicateAction = "error";
  uint64_t rowsToSkip = 0;
  uint64_t maxErrors = 20;
  bool skipValidation = false;
  bool latencyStats = false;
  std::vector<std::string> mergeAttributes;
};

}  // namespace arangodb
