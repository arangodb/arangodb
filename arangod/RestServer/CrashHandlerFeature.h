////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "CrashHandler/DumpManager.h"
#include "RestServer/CrashHandlerFeatureOptions.h"

namespace arangodb {

/// @brief Feature to control crash dump logging to the database directory.
/// The CrashHandler itself always runs for crash handling, but this feature
/// controls whether additional crash information is written to disk.
class CrashHandlerFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "CrashHandler"; }

  explicit CrashHandlerFeature(
      application_features::ApplicationServer& server,
      std::shared_ptr<crash_handler::DumpManager> dumpManager);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  /// @brief returns true if crash dump logging is enabled
  bool isEnabled() const noexcept { return _options.enabled; }

  void start() override final;

  std::shared_ptr<crash_handler::DumpManager> getDumpManager() const;

 private:
  bool canAccessCrashesDirectory() const noexcept;

  /// @brief pointer to the Crash Handler Dumper
  std::shared_ptr<crash_handler::DumpManager> _dumpManager;

  CrashHandlerFeatureOptions _options;
};

}  // namespace arangodb
