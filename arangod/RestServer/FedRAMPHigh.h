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

#include <string_view>

#include "Basics/Result.h"
#include "Basics/ResultT.h"

namespace arangodb {

struct LoggerOptions;

/// @brief name of the environment variable which enables FedRAMP High mode.
inline constexpr char const* kFedRAMPHighEnvVar = "FEDRAMP_HIGH";

/// @brief interprets the value of the environment variable kFedRAMPHighEnvVar.
/// "on" means enabled, "off" means disabled. Any other value, including the
/// empty string, is an error, so that a mistyped value can never silently
/// select a mode.
[[nodiscard]] ResultT<bool> isFedRAMPHighEnabled(std::string_view value);

/// @brief changes the defaults of the logger options for FedRAMP High mode.
/// must be called before the options are declared, so that the changed
/// defaults are also shown in `--help` and `--dump-options`.
void applyFedRAMPHighLoggerDefaults(LoggerOptions& options) noexcept;

/// @brief checks that the (parsed) logger options satisfy the restrictions of
/// FedRAMP High mode, i.e. that security events are always logged.
[[nodiscard]] Result checkFedRAMPHighLoggerOptions(
    LoggerOptions const& options);

}  // namespace arangodb
