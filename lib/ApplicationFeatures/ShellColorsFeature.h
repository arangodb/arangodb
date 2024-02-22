////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class ShellColorsFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ShellColors"; }

  template<typename Server>
  explicit ShellColorsFeature(Server& server)
      : ApplicationFeature{server, *this}, _initialized(false) {
    setOptional(false);

    // it's admittedly a hack that we already call prepare here...
    // however, setting the colors is one of the first steps we need to do,
    // and we do not want to wait for the application server to have
    // successfully parsed options etc. before we initialize the shell colors
    prepare();
  }

  // cppcheck-suppress virtualCallInConstructor
  void prepare() override final;

 private:
  bool useColors();

 public:
  static char const* SHELL_COLOR_RED;
  static char const* SHELL_COLOR_BOLD_RED;
  static char const* SHELL_COLOR_GREEN;
  static char const* SHELL_COLOR_BOLD_GREEN;
  static char const* SHELL_COLOR_BLUE;
  static char const* SHELL_COLOR_BOLD_BLUE;
  static char const* SHELL_COLOR_YELLOW;
  static char const* SHELL_COLOR_BOLD_YELLOW;
  static char const* SHELL_COLOR_WHITE;
  static char const* SHELL_COLOR_BOLD_WHITE;
  static char const* SHELL_COLOR_BLACK;
  static char const* SHELL_COLOR_BOLD_BLACK;
  static char const* SHELL_COLOR_CYAN;
  static char const* SHELL_COLOR_BOLD_CYAN;
  static char const* SHELL_COLOR_MAGENTA;
  static char const* SHELL_COLOR_BOLD_MAGENTA;
  static char const* SHELL_COLOR_BLINK;
  static char const* SHELL_COLOR_BRIGHT;
  static char const* SHELL_COLOR_RESET;
  static char const* SHELL_COLOR_LINK_START;
  static char const* SHELL_COLOR_LINK_MIDDLE;
  static char const* SHELL_COLOR_LINK_END;

 private:
  bool _initialized;
};

}  // namespace arangodb
