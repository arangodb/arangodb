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

#include <string_view>

namespace arangodb::signals {

enum class SignalType { term, core, cont, ign, logrotate, stop, user };

/// @brief find out what impact a signal will have to the process we send it.
SignalType signalType(int signal) noexcept;

/// @brief whether or not the signal is deadly
bool isDeadly(int signal) noexcept;

/// @brief return the name for a signal
std::string_view name(int signal) noexcept;

std::string_view subtypeName(int signal, int subCode) noexcept;

void maskAllSignals();
void maskAllSignalsServer();
void maskAllSignalsClient();
void unmaskAllSignals();

}  // namespace arangodb::signals
