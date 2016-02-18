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

#include "ShellBase.h"

#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Utilities/Completer.h"
#include "Utilities/LinenoiseShell.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shell
////////////////////////////////////////////////////////////////////////////////

ShellBase* ShellBase::buildShell(std::string const& history,
                                 Completer* completer) {
  return new LinenoiseShell(history, completer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort the alternatives results vector
////////////////////////////////////////////////////////////////////////////////

void ShellBase::sortAlternatives(std::vector<std::string>& completions) {
  std::sort(completions.begin(), completions.end(),
            [](std::string const& l, std::string const& r) -> bool {
              int res = strcasecmp(l.c_str(), r.c_str());
              return (res < 0);
            });
}

ShellBase::ShellBase(std::string const& history, Completer* completer)
    : _current(),
      _historyFilename(),
      _state(STATE_NONE),
      _completer(completer) {
  // construct the complete history path
  std::string path;
  char* p = TRI_HomeDirectory();

  if (p != nullptr) {
    path.append(p);
    TRI_Free(TRI_CORE_MEM_ZONE, p);

    if (!path.empty() && path[path.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
      path.push_back(TRI_DIR_SEPARATOR_CHAR);
    }
  }

  path.append(history);

  _historyFilename = path;
}

ShellBase::~ShellBase() { delete _completer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a signal
////////////////////////////////////////////////////////////////////////////////

void ShellBase::signal() {
  // do nothing special
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shell prompt
////////////////////////////////////////////////////////////////////////////////

std::string ShellBase::prompt(std::string const& prompt,
                              std::string const& plain, bool& eof) {
  size_t lineno = 0;
  std::string dotdot = "...> ";
  std::string p = prompt;
  std::string sep = "";
  std::string line;

  eof = false;

  while (true) {
    // calling concrete implementation of the shell
    line = getLine(p, eof);
    p = dotdot;

    if (eof) {
      // give up, if the user pressed control-D on the top-most level
      if (_current.empty()) {
        return "";
      }

      // otherwise clear current content
      _current.clear();
      eof = false;
      break;
    }

    _current += sep;
    sep = "\n";
    ++lineno;

    // remove any prompt at the beginning of the line
    size_t pos = std::string::npos;

    if (StringUtils::isPrefix(line, plain)) {
      pos = line.find('>');
    } else if (StringUtils::isPrefix(line, "...")) {
      pos = line.find('>');
    }

    if (pos != std::string::npos) {
      pos = line.find_first_not_of(" \t", pos + 1);

      if (pos != std::string::npos) {
        line = line.substr(pos);
      } else {
        line.clear();
      }
    }

    // extend line and check
    _current += line;

    // stop if line is complete
    bool ok = _completer->isComplete(_current, lineno);

    if (ok) {
      break;
    }
  }

  // clear current line
  line = _current;
  _current.clear();

  return line;
}
