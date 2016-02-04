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

#include "ArangoClient.h"

#include "Basics/FileUtils.h"
#include "Basics/Logger.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/messages.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

double const ArangoClient::DEFAULT_CONNECTION_TIMEOUT = 5.0;
double const ArangoClient::DEFAULT_REQUEST_TIMEOUT = 1200.0;
size_t const ArangoClient::DEFAULT_RETRIES = 2;

double const ArangoClient::LONG_TIMEOUT = 86400.0;
#ifdef _WIN32
bool cygwinShell = false;
#endif
namespace {
#ifdef _WIN32
bool _newLine() {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore sequence used for prompt length calculation (starting point)
///
/// This sequence must be used before any non-visible characters in the prompt.
////////////////////////////////////////////////////////////////////////////////

char const* ArangoClient::PROMPT_IGNORE_START = "\001";

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore sequence used for prompt length calculation (end point)
///
/// This sequence must be used behind any non-visible characters in the prompt.
////////////////////////////////////////////////////////////////////////////////

char const* ArangoClient::PROMPT_IGNORE_END = "\002";

ArangoClient::ArangoClient(char const* appName)
    : _configFile(),
      _tempPath(),
      _quiet(false),

      _colorOptions(false),
      _noColors(false),

      _autoCompleteOptions(false),
      _noAutoComplete(false),

      _prettyPrintOptions(false),
      _prettyPrint(false),

      _pagerOptions(false),
      _outputPager("less -X -R -F -L"),
      _pager(stdout),
      _usePager(false),

      _auditLog(""),
      _auditFile(nullptr),

      _serverOptions(false),
      _disableAuthentication(false),
      _endpointString(),
      _endpointServer(nullptr),
      _databaseName("_system"),
      _username("root"),
      _password(""),
      _hasPassword(false),
      _connectTimeout(DEFAULT_CONNECTION_TIMEOUT),
      _requestTimeout(DEFAULT_REQUEST_TIMEOUT),
      _sslProtocol(4) {
  TRI_SetApplicationName(appName);
  std::string p = TRI_GetTempPath();

  _logLevel.push_back("info");

  if (p.empty()) {
    _tempPath = p;
  }
}

ArangoClient::~ArangoClient() { delete _endpointServer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a program-specific help message
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupSpecificHelp(std::string const& progname,
                                     std::string const& message) {
  _specificHelp.progname = progname;
  _specificHelp.message = message;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the general and logging options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupGeneral(ProgramOptionsDescription& description) {
  ProgramOptionsDescription loggingOptions("LOGGING options");

  // clang-format off

  loggingOptions
    ("log.level,l", &_logLevel, "log level")
    ("log.use-local-time", "log local dates and times in log messages")
    ("log.output,o", &_logOutput, "log output")
  ;

  description
    ("configuration,c", &_configFile, "read configuration file")
    ("help,h", "help message")
    ("temp-path", &_tempPath, "path for temporary files")
    ("quiet,s", "no banner")
    (loggingOptions, false)
  ;

  // clang-format on
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the color options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupColors(ProgramOptionsDescription& description) {
  ProgramOptionsDescription hiddenOptions("HIDDEN options");

  // clang-format off

  hiddenOptions
    ("colors", "activate color support")
  ;

  description
    ("no-colors", "deactivate color support")
    (hiddenOptions, true)
  ;

  // clang-format on

  _colorOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the auto-complete options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupAutoComplete(ProgramOptionsDescription& description) {
  ProgramOptionsDescription hiddenOptions("HIDDEN options");

  // clang-format off

  hiddenOptions
    ("auto-complete", "enable auto completion, use no-auto-complete to disable")
  ;

  description
    ("no-auto-complete", "disable auto completion")
    (hiddenOptions, true)
  ;

  // clang-format on

  _autoCompleteOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the pretty-printing options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupPrettyPrint(ProgramOptionsDescription& description) {
  ProgramOptionsDescription hiddenOptions("HIDDEN options");

  // clang-format off

  hiddenOptions
    ("no-pretty-print", "disable pretty printting")
  ;

  description
    ("pretty-print", "pretty print values")
    (hiddenOptions, true)
  ;

  // clang-format on

  _prettyPrintOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the log options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupLog(ProgramOptionsDescription& description) {
  // clang-format off

  description
    ("audit-log", &_auditLog, "audit log file to save commands and results to")
  ;

  // clang-format on
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the pager options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupPager(ProgramOptionsDescription& description) {
  // clang-format off

  description
    ("pager", &_outputPager, "output pager")
    ("use-pager", "use pager")
  ;

  // clang-format on

  _pagerOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the server options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupServer(ProgramOptionsDescription& description) {
  ProgramOptionsDescription clientOptions("CLIENT options");

  // clang-format off

  clientOptions
    ("server.database", &_databaseName, "database name to use when connecting")
    ("server.disable-authentication", &_disableAuthentication,
       "disable the password prompt and authentication when connecting (note: "
       "this doesn't control whether a server requires authentication)")
    ("server.endpoint", &_endpointString,
       "endpoint to connect to, use 'none' to start without a server")
    ("server.username", &_username, "username to use when connecting")
    ("server.password", &_password,
       "password to use when connecting. Don't specify this option to be "
       "prompted for the password (note: this requires "
       "--server.disable-authentication to be 'false')")
    ("server.connect-timeout", &_connectTimeout, "connect timeout in seconds")
    ("server.request-timeout", &_requestTimeout, "request timeout in seconds")
    ("server.ssl-protocol", &_sslProtocol,
       "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1")
  ;

  description
    (clientOptions, false)
  ;

  // clang-format on

  _serverOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses command line and config file and prepares logging
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::parse(ProgramOptions& options,
                         ProgramOptionsDescription& description,
                         std::string const& example, int argc, char* argv[],
                         std::string const& initFilename) {
  // if options are invalid, exit directly
  if (!options.parse(description, argc, argv)) {
    LOG(FATAL) << options.lastError();
    FATAL_ERROR_EXIT();
  }

  // setup the logging
  Logger::setLogLevel(_logLevel);
  Logger::setUseLocalTime(options.has("log.use-local-time"));
  Logger::setShowLineNumber(false);
  Logger::setShowThreadIdentifier(false);

  if (_logOutput.empty()) {
    Logger::addAppender("-", true, "");
  } else {
    for (auto& definition : _logOutput) {
      Logger::addAppender(definition, true, "");
    }
  }

  // parse config file
  std::string configFile = "";
  bool allowLocal = false;

  if (!_configFile.empty()) {
    if (StringUtils::tolower(_configFile) == std::string("none")) {
      LOG(DEBUG) << "using no init file at all";
    } else {
      configFile = _configFile;
    }
  }

  else {
    char* d = TRI_LocateConfigDirectory();

    if (d != nullptr) {
      std::string sysDir = std::string(d) + initFilename;
      TRI_FreeString(TRI_CORE_MEM_ZONE, d);

      if (FileUtils::exists(sysDir)) {
        configFile = sysDir;
        allowLocal = true;
      } else {
        LOG(DEBUG) << "no system init file '" << sysDir << "'";
      }
    }
  }

  if (!configFile.empty()) {
    if (allowLocal) {
      std::string localConfigFile = configFile + ".local";

      if (FileUtils::exists(localConfigFile)) {
        LOG(DEBUG) << "using init override file '" << localConfigFile
                   << "'";

        if (!options.parse(description, localConfigFile)) {
          LOG(FATAL) << "cannot parse config file '" << localConfigFile
                     << "': " << options.lastError();
          FATAL_ERROR_EXIT();
        }
      }
    }

    LOG(DEBUG) << "using init file '" << configFile << "'";

    if (!options.parse(description, configFile)) {
      LOG(FATAL) << "cannot parse config file '" << configFile
                 << "': " << options.lastError();
      FATAL_ERROR_EXIT();
    }
  }

  // configuration is parsed and valid if we got to this point

  // check for --help
  std::set<std::string> help = options.needHelp("help");

  if (!help.empty()) {
    if (!example.empty()) {
      std::cout << "USAGE:  " << argv[0] << " " << example << std::endl
                << std::endl;
    }
    std::cout << description.usage(help) << std::endl;

    // check for program-specific help
    std::string const progname(argv[0]);
    if (!_specificHelp.progname.empty() &&
        progname.size() >= _specificHelp.progname.size() &&
        progname.substr(progname.size() - _specificHelp.progname.size(),
                        _specificHelp.progname.size()) ==
            _specificHelp.progname) {
      // found a program-specific help
      std::cout << _specificHelp.message << std::endl;
    }

    // --help always returns success
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // set temp path
  if (options.has("temp-path")) {
    TRI_SetUserTempPath((char*)_tempPath.c_str());
  }

  if (options.has("server.username")) {
    // if a username is specified explicitly, assume authentication is desired
    _disableAuthentication = false;
  }

  // check if have a password
  _hasPassword = options.has("server.password") || _disableAuthentication ||
                 options.has("jslint") || options.has("javascript.unit-tests");

  // set colors
  if (options.has("colors")) {
    _noColors = false;
  }

  if (options.has("no-colors")) {
    _noColors = true;
  }

  // set auto-completion
  if (options.has("auto-complete")) {
    _noAutoComplete = false;
  }

  if (options.has("no-auto-complete")) {
    _noAutoComplete = true;
  }

  // set pretty print
  if (options.has("pretty-print")) {
    _prettyPrint = true;
  }

  if (options.has("no-pretty-print")) {
    _prettyPrint = false;
  }

  // set pager
  if (options.has("use-pager")) {
    _usePager = true;
  }

  // set quiet
  if (options.has("quiet")) {
    _quiet = true;
  }

  // .............................................................................
  // server options
  // .............................................................................

  if (_serverOptions) {
    // check connection args
    if (_connectTimeout < 0.0) {
      LOG(FATAL) << "invalid value for --server.connect-timeout, must be >= 0";
      FATAL_ERROR_EXIT();
    } else if (_connectTimeout == 0.0) {
      _connectTimeout = LONG_TIMEOUT;
    }

    if (_requestTimeout < 0.0) {
      LOG(FATAL)
          << "invalid value for --server.request-timeout, must be positive";
      FATAL_ERROR_EXIT();
    } else if (_requestTimeout == 0.0) {
      _requestTimeout = LONG_TIMEOUT;
    }

    // must specify a user name
    if (_username.size() == 0) {
      LOG(FATAL) << "no value specified for --server.username";
      FATAL_ERROR_EXIT();
    }

    // no password given on command-line
    if (!_hasPassword) {
      usleep(10 * 1000);
      printContinuous("Please specify a password: ");

// now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
      TRI_SetStdinVisibility(false);
      getline(std::cin, _password);

      TRI_SetStdinVisibility(true);
#else
      getline(std::cin, _password);
#endif
      printLine("");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string and a newline to stderr
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printErrLine(std::string const& s) {
#ifdef _WIN32
  // no, we can use std::cerr as this doesn't support UTF-8 on Windows
  printLine(s);
#else
  fprintf(stderr, "%s\n", s.c_str());
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string and a newline to stdout
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::_printLine(std::string const& s) {
#ifdef _WIN32
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

    // finally set the cursor position to where the printing should have stopped
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), newPos);
  } else {
    fprintf(stdout, "window error: '%d' \r\n", GetLastError());
    fprintf(stdout, "%s\r\n", s.c_str());
  }

  if (wBuf) {
    TRI_Free(TRI_CORE_MEM_ZONE, wBuf);
  }
#endif
}

void ArangoClient::printLine(std::string const& s, bool forceNewLine) {
#if _WIN32
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
    fprintf(stdout, "%s\n", s.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string to stdout, without a newline (Non-Windows only)
/// on Windows, we'll print the line and a newline
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printContinuous(std::string const& s) {
// no, we cannot use std::cout as this doesn't support UTF-8 on Windows
#ifdef _WIN32
  // On Windows, we just print the line followed by a newline
  printLine(s, true);
#else
  fprintf(stdout, "%s", s.c_str());
  fflush(stdout);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::startPager() {
#ifndef _WIN32
  if (!_usePager || _outputPager == "" || _outputPager == "stdout" ||
      _outputPager == "-") {
    _pager = stdout;
    return;
  }

  _pager = popen(_outputPager.c_str(), "w");

  if (_pager == nullptr) {
    printf("popen() failed! Using stdout instead!\n");
    _pager = stdout;
    _usePager = false;
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::stopPager() {
#ifndef _WIN32
  if (_pager != stdout) {
    pclose(_pager);
    _pager = stdout;
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strips binary data from string
/// this is done before sending the string to a pager or writing it to the log
////////////////////////////////////////////////////////////////////////////////

static std::string StripBinary(char const* value) {
  std::string result;

  char const* p = value;
  bool inBinary = false;

  while (*p) {
    if (inBinary) {
      if (*p == 'm') {
        inBinary = false;
      }
    } else {
      if (*p == '\x1b') {
        inBinary = true;
      } else {
        result.push_back(*p);
      }
    }
    ++p;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints to pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::internalPrint(std::string const& str) {
  if (_pager == stdout) {
#ifdef _WIN32
    // at moment the formating is ignored in windows
    printLine(str);
#else
    fprintf(_pager, "%s", str.c_str());
#endif
    if (_auditFile) {
      std::string sanitized = StripBinary(str.c_str());
      log("%s", sanitized.c_str());
    }
  } else {
    std::string sanitized = StripBinary(str.c_str());

    if (!sanitized.empty()) {
      fprintf(_pager, "%s", sanitized.c_str());
      log("%s", sanitized.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::openLog() {
  if (!_auditLog.empty()) {
    _auditFile = fopen(_auditLog.c_str(), "w");

    std::ostringstream s;
    if (_auditFile == nullptr) {
      s << "Cannot open file '" << _auditLog << "' for logging.";
      printErrLine(s.str());
    } else {
      s << "Logging input and output to '" << _auditLog << "'.";
      printLine(s.str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::closeLog() {
  if (!_auditLog.empty() && _auditFile != nullptr) {
    fclose(_auditFile);
    _auditFile = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints info message
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printWelcomeInfo() {
  if (_usePager) {
    std::ostringstream s;
    s << "Using pager '" << _outputPager << "' for output buffering.";

    printLine(s.str());
  }

  if (_prettyPrint) {
    printLine("Pretty printing values.");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints bye-bye
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printByeBye() {
  if (!_quiet) {
    printLine("<ctrl-D>");
    printLine(TRI_BYE_MESSAGE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs output, without prompt
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::log(char const* format, char const* str) {
  if (_auditFile) {
    std::string sanitized = StripBinary(str);

    if (!sanitized.empty()) {
      // do not print terminal escape sequences into log
      fprintf(_auditFile, format, sanitized.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs output, with prompt
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::log(std::string const& format, std::string const& prompt,
                       std::string const& str) {
  if (_auditFile) {
    std::string sanitized = StripBinary(str.c_str());

    if (!sanitized.empty()) {
      // do not print terminal escape sequences into log
      fprintf(_auditFile, format.c_str(), prompt.c_str(), sanitized.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes log output
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::flushLog() {
  if (_auditFile) {
    fflush(_auditFile);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new endpoint
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::createEndpoint() { createEndpoint(_endpointString); }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new endpoint
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::createEndpoint(std::string const& definition) {
  // close previous endpoint
  if (_endpointServer != nullptr) {
    delete _endpointServer;
    _endpointServer = nullptr;
  }

  _endpointServer = Endpoint::clientFactory(definition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet start
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::quiet() const { return _quiet; }

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts up arangosh
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::shutup() { _quiet = true; }

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivates colors
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::colors() const {
  return (!_noColors && isatty(STDIN_FILENO));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the auto completion flag
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::autoComplete() const { return !_noAutoComplete; }

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::prettyPrint() const { return _prettyPrint; }

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the output pager
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::outputPager() const { return _outputPager; }

////////////////////////////////////////////////////////////////////////////////
/// @brief gets use pager
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::usePager() const { return _usePager; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets use pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setUsePager(bool value) { _usePager = value; }

////////////////////////////////////////////////////////////////////////////////
/// @brief gets endpoint to connect to as string
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::endpointString() const {
  return _endpointString;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets endpoint to connect to as string
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setEndpointString(std::string const& value) {
  _endpointString = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint
////////////////////////////////////////////////////////////////////////////////

Endpoint* ArangoClient::endpointServer() const { return _endpointServer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::databaseName() const { return _databaseName; }

////////////////////////////////////////////////////////////////////////////////
/// @brief user to send to endpoint
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::username() const { return _username; }

////////////////////////////////////////////////////////////////////////////////
/// @brief password to send to endpoint
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::password() const { return _password; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets database name
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setDatabaseName(std::string const& databaseName) {
  _databaseName = databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets username
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setUsername(std::string const& username) {
  _username = username;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets password
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setPassword(std::string const& password) {
  _password = password;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in seconds)
////////////////////////////////////////////////////////////////////////////////

double ArangoClient::connectTimeout() const { return _connectTimeout; }

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout (in seconds)
////////////////////////////////////////////////////////////////////////////////

double ArangoClient::requestTimeout() const { return _requestTimeout; }

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl protocol
////////////////////////////////////////////////////////////////////////////////

uint32_t ArangoClient::sslProtocol() const { return _sslProtocol; }
