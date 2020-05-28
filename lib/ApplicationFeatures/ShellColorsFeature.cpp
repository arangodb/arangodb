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

#include "ShellColorsFeature.h"

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

using namespace arangodb::basics;

namespace {
static char const* NoColor = "";
}

namespace arangodb {

char const* ShellColorsFeature::SHELL_COLOR_RED = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_RED = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_GREEN = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_GREEN = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BLUE = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_BLUE = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_YELLOW = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_YELLOW = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_WHITE = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_WHITE = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BLACK = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_BLACK = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_CYAN = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_CYAN = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_MAGENTA = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BOLD_MAGENTA = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BLINK = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_BRIGHT = NoColor;
char const* ShellColorsFeature::SHELL_COLOR_RESET = NoColor;

ShellColorsFeature::ShellColorsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ShellColors"), _initialized(false) {
  setOptional(false);

  // it's admittedly a hack that we already call prepare here...
  // however, setting the colors is one of the first steps we need to do,
  // and we do not want to wait for the application server to have successfully
  // parsed options etc. before we initialize the shell colors
  prepare();
}

void ShellColorsFeature::prepare() {
  // prevent duplicate invocation of prepare
  if (_initialized) {
    return;
  }
  _initialized = true;

  if (useColors()) {
    SHELL_COLOR_RED = "\x1b[31m";
    SHELL_COLOR_BOLD_RED = "\x1b[1;31m";
    SHELL_COLOR_GREEN = "\x1b[32m";
    SHELL_COLOR_BOLD_GREEN = "\x1b[1;32m";
    SHELL_COLOR_BLUE = "\x1b[34m";
    SHELL_COLOR_BOLD_BLUE = "\x1b[1;34m";
    SHELL_COLOR_YELLOW = "\x1b[33m";
    SHELL_COLOR_BOLD_YELLOW = "\x1b[1;33m";
    SHELL_COLOR_WHITE = "\x1b[37m";
    SHELL_COLOR_BOLD_WHITE = "\x1b[1;37m";
    SHELL_COLOR_BLACK = "\x1b[30m";
    SHELL_COLOR_BOLD_BLACK = "\x1b[1;30m";
    SHELL_COLOR_CYAN = "\x1b[36m";
    SHELL_COLOR_BOLD_CYAN = "\x1b[1;36m";
    SHELL_COLOR_MAGENTA = "\x1b[35m";
    SHELL_COLOR_BOLD_MAGENTA = "\x1b[1;35m";
    SHELL_COLOR_BLINK = "\x1b[5m";
    SHELL_COLOR_BRIGHT = "\x1b[1m";
    SHELL_COLOR_RESET = "\x1b[0m";
  }
}

bool ShellColorsFeature::useColors() {
#ifdef _WIN32
  if (!prepareConsole()) {
    return false;
  }
  return terminalKnowsANSIColors();
#else
  return true;
#endif
}

#ifdef _WIN32
bool ShellColorsFeature::prepareConsole() {
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hStdout == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD handleMode = 0;
  if (!GetConsoleMode(hStdout, &handleMode)) {
    return false;
  }
  handleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hStdout, handleMode)) {
    return false;
  }
  return true;
}
#endif

}  // namespace arangodb
