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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SIGNALS_H
#define ARANGODB_BASICS_SIGNALS_H 1

namespace arangodb {
namespace signals {

enum class SignalType {
  term,
  core,
  cont,
  ign,
  logrotate,
  stop,
  user
};

/// @brief find out what impact a signal will have to the process we send it.
#ifndef _WIN32
SignalType signalType(int signal);
#endif

/// @brief whether or not the signal is deadly
bool isDeadly(int signal);

/// @brief return the name for a signal
char const* name(int signal);

void maskAllSignals();
void unmaskAllSignals();
  
}  // namespace signals
}  // namespace arangodb

#endif
