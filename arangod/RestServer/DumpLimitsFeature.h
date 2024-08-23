////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RestServer/arangod.h"

#include <cstdint>

namespace arangodb {

struct DumpLimits {
  // per-dump value
  std::uint64_t docsPerBatchLowerBound = 10;
  // per-dump value
  std::uint64_t docsPerBatchUpperBound = 1 * 1000 * 1000;
  // per-dump value
  std::uint64_t batchSizeLowerBound = 4 * 1024;
  // per-dump value
  std::uint64_t batchSizeUpperBound = 1024 * 1024 * 1024;
  // per-dump value
  std::uint64_t parallelismLowerBound = 1;
  // per-dump value
  std::uint64_t parallelismUpperBound = 8;
  // server-global. value will be overridden in the .cpp file.
  std::uint64_t memoryUsage = 512 * 1024 * 1024;
};

class DumpLimitsFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "DumpLimits"; }

  explicit DumpLimitsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  DumpLimits const& limits() const noexcept { return _dumpLimits; }

 private:
  DumpLimits _dumpLimits;
};

}  // namespace arangodb
