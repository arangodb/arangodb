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
/// @author Esteban Lombeyda
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShellBase.h"

namespace arangodb {
class Completer;

////////////////////////////////////////////////////////////////////////////////
/// @brief LinenoiseShell
////////////////////////////////////////////////////////////////////////////////

class LinenoiseShell final : public ShellBase {
 public:
  LinenoiseShell(std::string const& history, Completer*);

  ~LinenoiseShell();

 public:
  bool open(bool autoComplete) override final;

  bool close() override final;

  void addHistory(std::string const&) override final;

  bool writeHistory() override final;

  std::string getLine(std::string const& prompt, EofType& eof) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the shell implementation supports colors
  //////////////////////////////////////////////////////////////////////////////

  bool supportsColors() const override final { return true; }
};
}  // namespace arangodb
