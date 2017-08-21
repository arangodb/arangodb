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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ConsoleFeature.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/StringUtils.h"
#include "Basics/messages.h"
#include "Basics/terminal-utils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ClientFeature.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

#ifdef _WIN32
static const int FOREGROUND_WHITE =
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
static const int BACKGROUND_WHITE =
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
static const int INTENSITY = FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
#endif

ConsoleFeature::ConsoleFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Console"),
#ifdef _WIN32
      _codePage(-1),
      _cygwinShell(false),
#endif
      _quiet(false),
      _colors(true),
      _autoComplete(true),
      _prettyPrint(true),
      _auditFile(),
      _pager(false),
      _pagerCommand("less -X -R -F -L"),
      _prompt("%E@%d> "),
      _promptError(false),
      _supportsColors(isatty(STDIN_FILENO) != 0),
      _toPager(stdout),
      _toAuditFile(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");

  if (!_supportsColors) {
    _colors = false;
  }

#if _WIN32
  _codePage = GetConsoleOutputCP();

  CONSOLE_SCREEN_BUFFER_INFO info;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);

  _defaultAttribute = info.wAttributes & INTENSITY;
  _defaultColor = info.wAttributes & FOREGROUND_WHITE;
  _defaultBackground = info.wAttributes & BACKGROUND_WHITE;

  _consoleAttribute = _defaultAttribute;
  _consoleColor = _defaultColor | _defaultBackground;
#endif
}

void ConsoleFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--quiet", "silent startup",
                     new BooleanParameter(&_quiet));

  options->addSection("console", "Configure the console");

  options->addOption("--console.colors", "enable color support",
                     new BooleanParameter(&_colors));

  options->addOption("--console.auto-complete", "enable auto completion",
                     new BooleanParameter(&_autoComplete));

  options->addOption("--console.pretty-print", "enable pretty printing",
                     new BooleanParameter(&_prettyPrint));

  options->addOption("--console.audit-file",
                     "audit log file to save commands and results",
                     new StringParameter(&_auditFile));

  options->addOption("--console.pager", "enable paging",
                     new BooleanParameter(&_pager));

  options->addHiddenOption("--console.pager-command", "pager command",
                           new StringParameter(&_pagerCommand));

  options->addOption("--console.prompt", "prompt used in REPL",
                     new StringParameter(&_prompt));

#if _WIN32
  options->addHiddenOption("--console.code-page", "Windows code page to use",
                           new UInt16Parameter(&_codePage));
#endif
}

void ConsoleFeature::prepare() {
#if _WIN32
  if (_is_cyg_tty (STDOUT_FILENO) || getenv("SHELL") != nullptr) {
    _cygwinShell = true;
  }
#endif
}

void ConsoleFeature::start() {
  openLog();

#if _WIN32
  if (_codePage != -1) {
    SetConsoleOutputCP(_codePage);
  }
#endif
}

void ConsoleFeature::unprepare() {
  closeLog();
}

#ifdef _WIN32
static void _newLine() { fprintf(stdout, "\n"); }

void ConsoleFeature::_print2(std::string const& s) {
  size_t sLen = s.size();

  if (sLen == 0) {
    return;
  }

  LPWSTR wBuf = new WCHAR[sLen + 1];
  int wLen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)sLen, wBuf,
                                 (int)((sizeof WCHAR) * (sLen + 1)));

  if (wLen) {
    auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(handle, _consoleAttribute | _consoleColor);

    DWORD n;
    WriteConsoleW(handle, wBuf, (DWORD)wLen, &n, NULL);
  } else {
    fprintf(stdout, "window error: '%d' \r\n", GetLastError());
    fprintf(stdout, "%s\r\n", s.c_str());
  }

  if (wBuf) {
    delete[] wBuf;
  }
}

void ConsoleFeature::_print(std::string const& s) {
  auto pos = s.find_first_of("\x1b");

  if (pos == std::string::npos) {
    _print2(s);
  } else {
    std::vector<std::string> lines = StringUtils::split(s, '\x1b', '\0');

    int i = 0;

    for (auto line : lines) {
      size_t pos = 0;

      if (i++ != 0 && !line.empty()) {
        char c = line[0];

        if (c == '[') {
          int code = 0;

          for (++pos; pos < line.size(); ++pos) {
            c = line[pos];

            if ('0' <= c && c <= '9') {
              code = code * 10 + (c - '0');
            } else if (c == 'm' || c == ';') {
              switch (code) {
                case 0:
                  _consoleAttribute = _defaultAttribute;
                  _consoleColor = _defaultColor | _defaultBackground;
                  break;

                case 1:  // BOLD
                case 5:  // BLINK
                  _consoleAttribute =
                      (_defaultAttribute ^ FOREGROUND_INTENSITY) & INTENSITY;
                  break;

                case 30:
                  _consoleColor = BACKGROUND_WHITE;
                  break;

                case 31:
                  _consoleColor = FOREGROUND_RED | _defaultBackground;
                  break;

                case 32:
                  _consoleColor = FOREGROUND_GREEN | _defaultBackground;
                  break;

                case 33:
                  _consoleColor =
                      FOREGROUND_RED | FOREGROUND_GREEN | _defaultBackground;
                  break;

                case 34:
                  _consoleColor = FOREGROUND_BLUE | _defaultBackground;
                  break;

                case 35:
                  _consoleColor =
                      FOREGROUND_BLUE | FOREGROUND_RED | _defaultBackground;
                  break;

                case 36:
                  _consoleColor =
                      FOREGROUND_BLUE | FOREGROUND_GREEN | _defaultBackground;
                  break;

                case 37:
                  _consoleColor = FOREGROUND_GREEN | FOREGROUND_RED |
                                  FOREGROUND_BLUE | _defaultBackground;
                  break;
              }

              code = 0;
            }

            if (c == 'm') {
              ++pos;
              break;
            }
          }
        }
      }

      if (line.size() > pos) {
        _print2(line.substr(pos));
      }
    }
  }

  auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(handle, _consoleAttribute | _consoleColor);
}

#endif

// prints a string to stdout, without a newline
void ConsoleFeature::printContinuous(std::string const& s) {
  if (s.empty()) {
    return;
  }

  {
    fprintf(stdout, "%s", s.c_str());
    fflush(stdout);
  }
}

void ConsoleFeature::printLine(std::string const& s) {
  {
    fprintf(stdout, "%s\n", s.c_str());
    fflush(stdout);
  }
}

void ConsoleFeature::printErrorLine(std::string const& s) { printLine(s); }

std::string ConsoleFeature::readPassword(std::string const& message) {
  printContinuous(message);

  std::string password = readPassword();
  ConsoleFeature::printLine("");

  return password;
}

std::string ConsoleFeature::readPassword() {
  TRI_SetStdinVisibility(false);

  TRI_DEFER(TRI_SetStdinVisibility(true));

  std::string password;
  std::getline(std::cin, password);
  return password;
}

void ConsoleFeature::printWelcomeInfo() {
  if (!_quiet) {
    if (_pager) {
      std::ostringstream s;

      s << "Using pager '" << _pagerCommand << "' for output buffering.";

      printLine(s.str());
    }

    if (_prettyPrint) {
      printLine("Pretty printing values.");
    }
  }
}

void ConsoleFeature::printByeBye() {
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

void ConsoleFeature::print(std::string const& message) {
  if (_toPager == stdout) {
    printContinuous(message);
  } else {
    std::string sanitized = StripBinary(message);
    fprintf(_toPager, "%s", sanitized.c_str());
  }

  log(message);
}

void ConsoleFeature::openLog() {
  if (!_auditFile.empty()) {
    _toAuditFile = fopen(_auditFile.c_str(), "w");

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

void ConsoleFeature::closeLog() {
  if (_toAuditFile != nullptr) {
    fclose(_toAuditFile);
    _toAuditFile = nullptr;
  }
}

void ConsoleFeature::log(std::string const& message) {
  if (_toAuditFile != nullptr) {
    std::string sanitized = StripBinary(message);

    if (!sanitized.empty()) {
      // do not print terminal escape sequences into log
      fprintf(_toAuditFile, "%s", sanitized.c_str());
    }
  }
}

void ConsoleFeature::flushLog() {
  if (_toAuditFile) {
    fflush(_toAuditFile);
  }
}

ConsoleFeature::Prompt ConsoleFeature::buildPrompt(ClientFeature* client) {
  std::string result;
  bool esc = false;

  for (char c : _prompt) {
    if (c == '\0') {
      break;
    }

    if (esc) {
      if (c == '%') {
        result.push_back(c);
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
      colored = ShellColorsFeature::SHELL_COLOR_BOLD_RED + result + ShellColorsFeature::SHELL_COLOR_RESET;
    } else {
      colored = ShellColorsFeature::SHELL_COLOR_BOLD_GREEN + result + ShellColorsFeature::SHELL_COLOR_RESET;
    }
  } else {
    colored = result;
  }

  return {result, colored};
}

void ConsoleFeature::startPager() {
#ifndef _WIN32
  if (!_pager || _pagerCommand.empty() || _pagerCommand == "stdout" ||
      _pagerCommand == "-") {
    _toPager = stdout;
  } else {
    _toPager = popen(_pagerCommand.c_str(), "w");

    if (_toPager == nullptr) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "popen() for pager failed! Using stdout instead!";
      _toPager = stdout;
      _pager = false;
    }
  }
#endif
}

void ConsoleFeature::stopPager() {
#ifndef _WIN32
  if (_toPager != stdout) {
    pclose(_toPager);
    _toPager = stdout;
  }
#endif
}
