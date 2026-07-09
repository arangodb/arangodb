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
#include "ShellConsoleOptionsProvider.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
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

ShellConsoleFeature::ShellConsoleFeature(
    application_features::ApplicationServer& server)
    : ShellConsoleFeature(server, ShellConsoleFeatureOptions{}) {}

ShellConsoleFeature::ShellConsoleFeature(
    application_features::ApplicationServer& server,
    ShellConsoleFeatureOptions options)
    : ApplicationFeature(server, *this),
      _options(std::move(options)),
      _promptError(false),
      _supportsColors(isatty(STDIN_FILENO) != 0),
      _toPager(stdout),
      _toAuditFile(nullptr),
      _lastDuration(0.0),
      _startTime(TRI_microtime()) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();
  if (!_supportsColors) {
    _options.colors = false;
  }
}

void ShellConsoleFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  ShellConsoleOptionsProvider provider;
  provider.declareOptions(options, _options);
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
  if (_options.quiet) {
    return;
  }

  std::ostringstream s;

  if (_options.pager) {
    s << "Using pager '" << _options.pagerCommand << "' for output buffering. ";
  }

  if (_options.useHistory) {
    s << "Command-line history will be persisted when the shell is exited. You "
         "can use `--console.history false` to turn this off";
  } else {
    s << "Command-line history is enabled for this session only and will *not* "
         "be persisted.";
  }

  printLine(s.str());
}

void ShellConsoleFeature::printByeBye() {
  if (!_options.quiet) {
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
  if (!_options.auditFile.empty()) {
    _toAuditFile = TRI_FOPEN(_options.auditFile.c_str(), "w");

    std::ostringstream s;

    if (_toAuditFile == nullptr) {
      s << "Cannot open file '" << _options.auditFile << "' for logging.";
      printErrorLine(s.str());
    } else {
      s << "Logging input and output to '" << _options.auditFile << "'.";
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

  for (char c : _options.prompt) {
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

  if (_supportsColors && _options.colors) {
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
  if (!_options.pager || _options.pagerCommand.empty() ||
      _options.pagerCommand == "stdout" || _options.pagerCommand == "-") {
    _toPager = stdout;
  } else {
    _toPager = popen(_options.pagerCommand.c_str(), "w");

    if (_toPager == nullptr) {
      LOG_TOPIC("25033", ERR, arangodb::Logger::FIXME)
          << "popen() for pager failed! Using stdout instead!";
      _toPager = stdout;
      _options.pager = false;
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
