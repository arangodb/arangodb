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

#include "ApplicationFeatures/ConsoleFeature.h"

#include "ApplicationFeatures/ClientFeature.h"
#include "Basics/messages.h"
#include "Basics/shell-colors.h"
#include "Basics/terminal-utils.h"
#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

ConsoleFeature::ConsoleFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "ConsoleFeature"),
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
      _supportsColors(isatty(STDIN_FILENO)),
      _toPager(stdout),
      _toAuditFile(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("LoggerFeature");
}

void ConsoleFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(Section("console", "Configure the console",
                              "console options", false, false));

  options->addOption("--quiet", "silent startup",
                     new BooleanParameter(&_quiet, false));

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
  options->addOption("--console.code-page", "Windows code page to use",
                     new Int16Parameter(&_codePage));
#endif
}

void ConsoleFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

#if _WIN32
  if (getenv("SHELL") != nullptr) {
    _cygwinShell = true;
  }
#endif
}

void ConsoleFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  openLog();
}

void ConsoleFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

  closeLog();
}

// prints a string to stdout, without a newline (Non-Windows only) on
// Windows, we'll print the line and a newline.  No, we cannot use
// std::cout as this doesn't support UTF-8 on Windows.

void ConsoleFeature::printContinuous(std::string const& s) {
#ifdef _WIN32
  // On Windows, we just print the line followed by a newline
  printLine(s, true);
#else
  fprintf(stdout, "%s", s.c_str());
  fflush(stdout);
#endif
}

#ifdef _WIN32
static bool _newLine() {
  COORD pos;
  CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
  if (bufferInfo.dwCursorPosition.Y + 1 >= bufferInfo.dwSize.Y) {
    // when we are at the last visible line of the console
    // the first line of console is deleted (the content of the console
    // is scrolled one line above
    SMALL_RECT srctScrollRect;
    srctScrollRect.Top = 0;
    srctScrollRect.Bottom = bufferInfo.dwCursorPosition.Y + 1;
    srctScrollRect.Left = 0;
    srctScrollRect.Right = bufferInfo.dwSize.X;
    COORD coordDest;
    coordDest.X = 0;
    coordDest.Y = -1;
    CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
    CHAR_INFO chiFill;
    chiFill.Char.AsciiChar = (char)' ';
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                                   &consoleScreenBufferInfo)) {
      chiFill.Attributes = consoleScreenBufferInfo.wAttributes;
    } else {
      // Fill the bottom row with green blanks.
      chiFill.Attributes = BACKGROUND_GREEN | FOREGROUND_RED;
    }
    ScrollConsoleScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE), &srctScrollRect,
                              nullptr, coordDest, &chiFill);
    pos.Y = bufferInfo.dwCursorPosition.Y;
    pos.X = 0;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    return true;
  } else {
    pos.Y = bufferInfo.dwCursorPosition.Y + 1;
    pos.X = 0;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    return false;
  }
}
#endif

#ifdef _WIN32
static void _printLine(std::string const& s) {
  LPWSTR wBuf = (LPWSTR)TRI_Allocate(TRI_CORE_MEM_ZONE,
                                     (sizeof WCHAR) * (s.size() + 1), true);
  int wLen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wBuf,
                                 (int)((sizeof WCHAR) * (s.size() + 1)));

  if (wLen) {
    DWORD n;
    COORD pos;
    CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
    // save old cursor position
    pos = bufferInfo.dwCursorPosition;

    size_t newX = static_cast<size_t>(pos.X) + s.size();
    // size_t oldY = static_cast<size_t>(pos.Y);
    if (newX >= static_cast<size_t>(bufferInfo.dwSize.X)) {
      for (size_t i = 0; i <= newX / bufferInfo.dwSize.X; ++i) {
        // insert as many newlines as we need. this prevents running out of
        // buffer space when printing lines
        // at the end of the console output buffer
        if (_newLine()) {
          pos.Y = pos.Y - 1;
        }
      }
    }

    // save new cursor position
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
    auto newPos = bufferInfo.dwCursorPosition;

    // print the actual string. note: printing does not advance the cursor
    // position
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    WriteConsoleOutputCharacterW(GetStdHandle(STD_OUTPUT_HANDLE), wBuf,
                                 (DWORD)s.size(), pos, &n);

    // finally set the cursor position to where the printing should have
    // stopped
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), newPos);
  } else {
    fprintf(stdout, "window error: '%d' \r\n", GetLastError());
    fprintf(stdout, "%s\r\n", s.c_str());
  }

  if (wBuf) {
    TRI_Free(TRI_CORE_MEM_ZONE, wBuf);
  }
}
#endif

void ConsoleFeature::printLine(std::string const& s, bool forceNewLine) {
#ifdef _WIN32
//#warning do we need forceNewLine

  if (!cygwinShell) {
    // no, we cannot use std::cout as this doesn't support UTF-8 on Windows
    // fprintf(stdout, "%s\r\n", s.c_str());
    TRI_vector_string_t subStrings = TRI_SplitString(s.c_str(), '\n');
    bool hasNewLines = (s.find("\n") != std::string::npos) | forceNewLine;

    if (hasNewLines) {
      for (size_t i = 0; i < subStrings._length; i++) {
        _printLine(subStrings._buffer[i]);
        _newLine();
      }
    } else {
      _printLine(s);
    }
    TRI_DestroyVectorString(&subStrings);
  } else
#endif
  {
    fprintf(stdout, "%s\n", s.c_str());
  }
}

void ConsoleFeature::printErrorLine(std::string const& s) {
#ifdef _WIN32
  // no, we can use std::cerr as this doesn't support UTF-8 on Windows
  printLine(s);
#else
  fprintf(stderr, "%s\n", s.c_str());
#endif
}

std::string ConsoleFeature::readPassword(std::string const& message) {
  std::string password;

  printContinuous(message);

#ifdef TRI_HAVE_TERMIOS_H
  TRI_SetStdinVisibility(false);
  std::getline(std::cin, password);

  TRI_SetStdinVisibility(true);
#else
  std::getline(std::cin, password);
#endif
  ConsoleFeature::printLine("");

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
    printLine("<ctrl-D>");
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
#ifdef _WIN32
    // at moment the formating is ignored in windows
    printLine(message);
#else
    fprintf(_toPager, "%s", message.c_str());
#endif

  } else {
    std::string sanitized = StripBinary(message.c_str());
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
          if (ep.find("tcp://") == 0) {
            ep = ep.substr(6);
          } else if (ep.find("ssl://") == 0) {
            ep = ep.substr(6);
          } else if (ep.find("unix://") == 0) {
            ep = ep.substr(7);
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
      colored = TRI_SHELL_COLOR_BOLD_RED + result + TRI_SHELL_COLOR_RESET;
    } else {
      colored = TRI_SHELL_COLOR_BOLD_GREEN + result + TRI_SHELL_COLOR_RESET;
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
      LOG(ERR) << "popen() for pager failed! Using stdout instead!";
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
