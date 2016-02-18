////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "LinenoiseShell.h"

extern "C" {
#include <linenoise.h>
}

#include "Utilities/Completer.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief active completer
////////////////////////////////////////////////////////////////////////////////

static Completer* COMPLETER = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief completer generator
////////////////////////////////////////////////////////////////////////////////

static void LinenoiseCompletionGenerator(char const* text,
                                         linenoiseCompletions* lc) {
  if (COMPLETER) {
    std::vector<std::string> alternatives = COMPLETER->alternatives(text);
    ShellBase::sortAlternatives(alternatives);

    for (auto& it : alternatives) {
      linenoiseAddCompletion(lc, it.c_str());
    }
  }
}

LinenoiseShell::LinenoiseShell(std::string const& history, Completer* completer)
    : ShellBase(history, completer) {
  COMPLETER = completer;
  linenoiseSetCompletionCallback(LinenoiseCompletionGenerator);
  linenoiseInstallWindowChangeHandler();
}

LinenoiseShell::~LinenoiseShell() { COMPLETER = nullptr; }

bool LinenoiseShell::open(bool) {
  linenoiseHistoryLoad(_historyFilename.c_str());
  _state = STATE_OPENED;
  return true;
}

bool LinenoiseShell::close() {
  // avoid duplicate saving of history
  if (_state != STATE_OPENED) {
    return true;
  }

  _state = STATE_CLOSED;
  auto result = writeHistory();
  linenoiseHistoryFree();
  return result;
}

void LinenoiseShell::addHistory(std::string const& str) {
  if (str.empty()) {
    return;
  }

  linenoiseHistoryAdd(str.c_str());
}

bool LinenoiseShell::writeHistory() {
  linenoiseHistorySave(_historyFilename.c_str());

  return true;
}

std::string LinenoiseShell::getLine(std::string const& prompt, bool& eof) {
  char* line = linenoise(prompt.c_str());

  if (line != nullptr) {
    eof = false;
    std::string const stringValue(line);
    ::free(line);
    return stringValue;
  }

  eof = true;
  return "";
}
