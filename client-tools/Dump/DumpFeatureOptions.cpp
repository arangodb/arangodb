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

#include "DumpFeatureOptions.h"

#include "Basics/error.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"

#include <filesystem>

namespace arangodb {
DumpFeatureOptions::DumpFeatureOptions() {
  using basics::FileUtils::buildFilename;
  std::error_code ec;
  std::filesystem::path const cwd = std::filesystem::current_path(ec);
  if (ec) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_set_errno(TRI_ERROR_SYS_ERROR),
        basics::StringUtils::concatT("cannot get current working directory: ",
                                     ec.message()));
  }
  outputPath = buildFilename(cwd.string(), "dump");
  threadCount =
      std::max(threadCount, static_cast<uint32_t>(NumberOfCores::getValue()));
}
}  // namespace arangodb
