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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ShellConsoleFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/messages.h"
#include "Basics/operating-system.h"
#include "Basics/system-functions.h"
#include "Basics/terminal-utils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ClientFeature.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <iomanip>
#include <iostream>

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

ShellConsoleFeature::ShellConsoleFeature(Server& server)
    : ArangoshFeature(server, *this),
      _quiet(false),
      _colors(true),
      _useHistory(true),
      _autoComplete(true),
      _prettyPrint(true),
      _auditFile(),
      _pager(false),
      _pagerCommand("less -X -R -F -L"),
      _prompt("%E@%d> "),
      _promptError(false),
      _supportsColors(isatty(STDIN_FILENO) != 0),
      _toPager(stdout),
      _toAuditFile(nullptr),
      _lastDuration(0.0),
      _startTime(TRI_microtime()) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();
  if (!_supportsColors) {
    _colors = false;
  }
}

void ShellConsoleFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption("--quiet", "Silent startup.",
                     new BooleanParameter(&_quiet));

  options->addSection("console", "console");

  options->addOption(
      "--console.colors", "Enable color support.",
      new BooleanParameter(&_colors),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--console.auto-complete", "Enable auto-completion.",
                     new BooleanParameter(&_autoComplete));

  options->addOption("--console.pretty-print", "Enable pretty-printing.",
                     new BooleanParameter(&_prettyPrint));

  options->addOption("--console.audit-file",
                     "The audit log file to save commands and results to.",
                     new StringParameter(&_auditFile));

  options->addOption("--console.history",
                     "Whether to load and persist command-line history.",
                     new BooleanParameter(&_useHistory));

  options->addOption("--console.pager", "Enable paging.",
                     new BooleanParameter(&_pager));

  options->addOption(
      "--console.pager-command", "The pager command.",
      new StringParameter(&_pagerCommand),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--console.prompt",
      "The prompt used in REPL (placeholders: %t = the current time as "
      "timestamp, %p = the duration of last command in seconds, %d = the name "
      "of the current database, %e = the current endpoint, %E = the current "
      "endpoint without the protocol, %u = the current user",
      new StringParameter(&_prompt));
}

void ShellConsoleFeature::start() { openLog(); }

void ShellConsoleFeature::unprepare() { closeLog(); }

// prints a string to stdout, without a newline
void ShellConsoleFeature::printContinuous(std::string const& s) {
  if (s.empty()) {
    return;
  }

  {
    fprintf(stdout, "%s", s.c_str());
    fflush(stdout);
  }
}

void ShellConsoleFeature::printLine(std::string const& s) {
  {
    fprintf(stdout, "%s\n", s.c_str());
    fflush(stdout);
  }
}

void ShellConsoleFeature::printErrorLine(std::string const& s) { printLine(s); }

std::string ShellConsoleFeature::readPassword(std::string const& message) {
  printContinuous(message);

  std::string password = readPassword();
  ShellConsoleFeature::printLine("");

  return password;
}

std::string ShellConsoleFeature::readPassword() {
  terminal_utils::setStdinVisibility(false);

  auto sg = arangodb::scopeGuard(
      [&]() noexcept { terminal_utils::setStdinVisibility(true); });

  std::string password;

  std::getline(std::cin, password);
  return password;
}

void ShellConsoleFeature::printWelcomeInfo() {
  if (_quiet) {
    return;
  }

  std::ostringstream s;

  if (_pager) {
    s << "Using pager '" << _pagerCommand << "' for output buffering. ";
  }

  if (_useHistory) {
    s << "Command-line history will be persisted when the shell is exited. You "
         "can use `--console.history false` to turn this off";
  } else {
    s << "Command-line history is enabled for this session only and will *not* "
         "be persisted.";
  }

  printLine(s.str());
}

void ShellConsoleFeature::printByeBye() {
  if (!_quiet) {
    printLine(TRI_BYE_MESSAGE);
  }
}

static std::string StripBinary(std::string const& value) {
  std::string result;

  bool inBinary = false;

  for (char c : value) {
    if (inBinary) {
      if (c == 'm') {
        inBinary = false;
      }
    } else {
      if (c == '\x1b') {
        inBinary = true;
      } else {
        result.push_back(c);
      }
    }
  }

  return result;
}

void ShellConsoleFeature::print(std::string const& message) {
  if (_toPager == stdout) {
    printContinuous(message);
  } else {
    std::string sanitized = StripBinary(message);
    fprintf(_toPager, "%s", sanitized.c_str());
  }

  log(message);
}

void ShellConsoleFeature::openLog() {
  if (!_auditFile.empty()) {
    _toAuditFile = TRI_FOPEN(_auditFile.c_str(), "w");

    std::ostringstream s;

    if (_toAuditFile == nullptr) {
      s << "Cannot open file '" << _auditFile << "' for logging.";
      printErrorLine(s.str());
    } else {
      s << "Logging input and output to '" << _auditFile << "'.";
      printLine(s.str());
    }
  }
}

void ShellConsoleFeature::closeLog() {
  if (_toAuditFile != nullptr) {
    fclose(_toAuditFile);
    _toAuditFile = nullptr;
  }
}

void ShellConsoleFeature::log(std::string const& message) {
  if (_toAuditFile != nullptr) {
    std::string sanitized = StripBinary(message);

    if (!sanitized.empty()) {
      // do not print terminal escape sequences into log
      fprintf(_toAuditFile, "%s", sanitized.c_str());
    }
  }
}

void ShellConsoleFeature::flushLog() {
  if (_toAuditFile) {
    fflush(_toAuditFile);
  }
}

ShellConsoleFeature::Prompt ShellConsoleFeature::buildPrompt(
    ClientFeature* client) {
  std::string result;
  bool esc = false;

  for (char c : _prompt) {
    if (c == '\0') {
      break;
    }

    if (esc) {
      if (c == '%') {
        result.push_back(c);
      } else if (c == 't') {
        std::ostringstream tmp;
        tmp << std::setprecision(6) << std::fixed << TRI_microtime();
        result.append(tmp.str());
      } else if (c == 'a') {
        std::ostringstream tmp;
        tmp << std::setprecision(6) << std::fixed
            << (TRI_microtime() - _startTime);
        result.append(tmp.str());
      } else if (c == 'p') {
        std::ostringstream tmp;
        tmp << std::setprecision(6) << std::fixed << _lastDuration;
        result.append(tmp.str());
      } else if (c == 'd') {
        if (client != nullptr) {
          result.append(client->databaseName());
        } else {
          result.append("[database]");
        }
      } else if (c == 'e' || c == 'E') {
        std::string ep;

        if (client == nullptr) {
          ep = "none";
        } else {
          ep = client->endpoint();
        }

        if (c == 'E') {
          // replace protocol
          if (ep.compare(0, strlen("tcp://"), "tcp://") == 0) {
            ep = ep.substr(strlen("tcp://"));
          } else if (ep.compare(0, strlen("http+tcp://"), "http+tcp://") == 0) {
            ep = ep.substr(strlen("http+tcp://"));
          } else if (ep.compare(0, strlen("ssl://"), "ssl://") == 0) {
            ep = ep.substr(strlen("ssl://"));
          } else if (ep.compare(0, strlen("unix://"), "unix://") == 0) {
            ep = ep.substr(strlen("unix://"));
          }
        }

        result.append(ep);
      } else if (c == 'u') {
        if (client == nullptr) {
          result.append("[user]");
        } else {
          result.append(client->username());
        }
      }

      esc = false;
    } else {
      if (c == '%') {
        esc = true;
      } else {
        result.push_back(c);
      }
    }
  }

  std::string colored;

  if (_supportsColors && _colors) {
    if (_promptError) {
      colored = ShellColorsFeature::SHELL_COLOR_BOLD_RED + result +
                ShellColorsFeature::SHELL_COLOR_RESET;
    } else {
      colored = ShellColorsFeature::SHELL_COLOR_BOLD_GREEN + result +
                ShellColorsFeature::SHELL_COLOR_RESET;
    }
  } else {
    colored = result;
  }

  return {result, colored};
}

void ShellConsoleFeature::startPager() {
  if (!_pager || _pagerCommand.empty() || _pagerCommand == "stdout" ||
      _pagerCommand == "-") {
    _toPager = stdout;
  } else {
    _toPager = popen(_pagerCommand.c_str(), "w");

    if (_toPager == nullptr) {
      LOG_TOPIC("25033", ERR, arangodb::Logger::FIXME)
          << "popen() for pager failed! Using stdout instead!";
      _toPager = stdout;
      _pager = false;
    }
  }
}

void ShellConsoleFeature::stopPager() {
  if (_toPager != stdout) {
    pclose(_toPager);
    _toPager = stdout;
  }
}

}  // namespace arangodb
