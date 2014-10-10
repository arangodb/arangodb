////////////////////////////////////////////////////////////////////////////////
/// @brief arango shell client base
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ArangoClient.h"

#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/messages.h"
#include "Basics/tri-strings.h"
#include "Basics/terminal-utils.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/ProgramOptions.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

double const ArangoClient::DEFAULT_CONNECTION_TIMEOUT = 3.0;
double const ArangoClient::DEFAULT_REQUEST_TIMEOUT    = 300.0;
size_t const ArangoClient::DEFAULT_RETRIES            = 2;

namespace {
#ifdef _WIN32
bool _newLine () {
    COORD pos;
    CONSOLE_SCREEN_BUFFER_INFO  bufferInfo;
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
      CONSOLE_SCREEN_BUFFER_INFO  consoleScreenBufferInfo;
      CHAR_INFO chiFill;
      chiFill.Char.AsciiChar = (char)' ';
      if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleScreenBufferInfo)) {
        chiFill.Attributes = consoleScreenBufferInfo.wAttributes;
      }
      else {
        // Fill the bottom row with green blanks.
        chiFill.Attributes = BACKGROUND_GREEN | FOREGROUND_RED;
      }
      ScrollConsoleScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE), &srctScrollRect, NULL, coordDest, &chiFill);
      pos.Y = bufferInfo.dwCursorPosition.Y;
      pos.X = 0;
      SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
      return true;
    }
    else {
      pos.Y = bufferInfo.dwCursorPosition.Y + 1;
      pos.X = 0;
      SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
      return false;
    }
  }

#endif
}
// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoClient
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore sequence used for prompt length calculation (starting point)
///
/// This sequence must be used before any non-visible characters in the prompt.
////////////////////////////////////////////////////////////////////////////////

char const * ArangoClient::PROMPT_IGNORE_START = "\001";

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore sequence used for prompt length calculation (end point)
///
/// This sequence must be used behind any non-visible characters in the prompt.
////////////////////////////////////////////////////////////////////////////////

char const * ArangoClient::PROMPT_IGNORE_END = "\002";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ArangoClient::ArangoClient ()
  : _configFile(),
    _tempPath(),
    _logLevel("info"),
    _logLocalTime(false),
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

    _logFile(""),
    _log(0),
    _logOptions(false),

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

  char* p = TRI_GetTempPath();

  if (p != nullptr) {
    _tempPath = string(p);
    TRI_Free(TRI_CORE_MEM_ZONE, p);
  }
}

ArangoClient::~ArangoClient () {
  if (_endpointServer != nullptr) {
    delete _endpointServer;
  }
}


// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the general and logging options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupGeneral (ProgramOptionsDescription& description) {
  ProgramOptionsDescription loggingOptions("LOGGING options");

  loggingOptions
    ("log.level,l", &_logLevel,  "log level")
    ("log.use-local-time", "log local dates and times in log messages")
  ;

  description
    ("configuration,c", &_configFile, "read configuration file")
    ("help,h", "help message")
    ("temp-path", &_tempPath, "path for temporary files")
    ("quiet,s", "no banner")
    (loggingOptions, false)
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the color options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupColors (ProgramOptionsDescription& description) {
  ProgramOptionsDescription hiddenOptions("HIDDEN options");

  hiddenOptions
    ("colors", "activate color support")
  ;

  description
    ("no-colors", "deactivate color support")
    (hiddenOptions, true)
  ;

  _colorOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the auto-complete options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupAutoComplete (ProgramOptionsDescription& description) {
  ProgramOptionsDescription hiddenOptions("HIDDEN options");

  hiddenOptions
    ("auto-complete", "enable auto completion, use no-auto-complete to disable")
  ;

  description
    ("no-auto-complete", "disable auto completion")
    (hiddenOptions, true)
  ;

  _autoCompleteOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the pretty-printing options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupPrettyPrint (ProgramOptionsDescription& description) {
  ProgramOptionsDescription hiddenOptions("HIDDEN options");

  hiddenOptions
    ("no-pretty-print", "disable pretty printting")
  ;

  description
    ("pretty-print", "pretty print values")
    (hiddenOptions, true)
  ;

  _prettyPrintOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the log options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupLog (ProgramOptionsDescription& description) {
  description
    ("audit-log", &_logFile, "audit log file to save commands and results to")
  ;

  _logOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the pager options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupPager (ProgramOptionsDescription& description) {
  description
    ("pager", &_outputPager, "output pager")
    ("use-pager", "use pager")
  ;

  _pagerOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the server options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupServer (ProgramOptionsDescription& description) {
  ProgramOptionsDescription clientOptions("CLIENT options");

  clientOptions
    ("server.database", &_databaseName, "database name to use when connecting")
    ("server.disable-authentication", &_disableAuthentication, "disable the password prompt and authentication when connecting (note: this doesn't control whether a server requires authentication)")
    ("server.endpoint", &_endpointString, "endpoint to connect to, use 'none' to start without a server")
    ("server.username", &_username, "username to use when connecting")
    ("server.password", &_password, "password to use when connecting. Don't specify this option to be prompted for the password (note: this requires --server.disable-authentication to be 'false')")
    ("server.connect-timeout", &_connectTimeout, "connect timeout in seconds")
    ("server.request-timeout", &_requestTimeout, "request timeout in seconds")
    ("server.ssl-protocol", &_sslProtocol, "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1")
  ;

  description(clientOptions, false);

  _serverOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses command line and config file and prepares logging
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::parse (ProgramOptions& options,
                          ProgramOptionsDescription& description,
                          string const& example,
                          int argc,
                          char* argv[],
                          string const& initFilename) {

  // if options are invalid, exit directly
  if (! options.parse(description, argc, argv)) {
    LOG_FATAL_AND_EXIT("%s", options.lastError().c_str());
  }
  
  if (options.has("log.use-local-time")) {
    _logLocalTime = true;
  }

  // setup the logging
  TRI_SetLogLevelLogging(_logLevel.c_str());
  TRI_SetUseLocalTimeLogging(_logLocalTime);
  TRI_CreateLogAppenderFile("-", 0, TRI_LOG_SEVERITY_UNKNOWN, false);
  TRI_SetLineNumberLogging(false);
  TRI_SetThreadIdentifierLogging(false);

  // parse config file
  string configFile = "";
  bool allowLocal = false;

  if (! _configFile.empty()) {
    if (StringUtils::tolower(_configFile) == string("none")) {
      LOG_DEBUG("using no init file at all");
    }
    else {
      configFile = _configFile;
    }
  }

  else {
    char* d = TRI_LocateConfigDirectory();

    if (d != 0) {
      string sysDir = string(d) + initFilename;
      TRI_FreeString(TRI_CORE_MEM_ZONE, d);

      if (FileUtils::exists(sysDir)) {
        configFile = sysDir;
        allowLocal = true;
      }
      else {
        LOG_DEBUG("no system init file '%s'", sysDir.c_str());
      }
    }
  }

  if (! configFile.empty()) {
    if (allowLocal) {
      string localConfigFile = configFile + ".local";

      if (FileUtils::exists(localConfigFile)) {
        LOG_DEBUG("using init override file '%s'", localConfigFile.c_str());

        if (! options.parse(description, localConfigFile)) {
          LOG_FATAL_AND_EXIT("cannot parse config file '%s': %s", localConfigFile.c_str(), options.lastError().c_str());
        }
      }
    }

    LOG_DEBUG("using init file '%s'", configFile.c_str());

    if (! options.parse(description, configFile)) {
      LOG_FATAL_AND_EXIT("cannot parse config file '%s': %s", configFile.c_str(), options.lastError().c_str());
    }
  }

  // configuration is parsed and valid if we got to this point

  // check for --help
  set<string> help = options.needHelp("help");

  if (! help.empty()) {
    if (! example.empty()) {
      cout << "USAGE: " << argv[0] << " " << example << endl << endl;
    }
    cout << description.usage(help) << endl;

    // --help always returns success
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, NULL);
  }


  // set temp path
  if (options.has("temp-path")) {
    TRI_SetUserTempPath((char*) _tempPath.c_str());
  }

  if (options.has("server.username")) {
    // if a username is specified explicitly, assume authentication is desired
    _disableAuthentication = false;
  }
  
  // check if have a password
  _hasPassword = options.has("server.password")
              || _disableAuthentication
              || options.has("jslint")
              || options.has("javascript.unit-tests");

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
    if (_connectTimeout <= 0) {
      LOG_FATAL_AND_EXIT("invalid value for --server.connect-timeout, must be positive");
    }

    if (_requestTimeout <= 0) {
      LOG_FATAL_AND_EXIT("invalid value for --server.request-timeout, must be positive");
    }

    // must specify a user name
    if (_username.size() == 0) {
      LOG_FATAL_AND_EXIT("no value specified for --server.username");
    }

    // no password given on command-line
    if (! _hasPassword) {
      usleep(10 * 1000);
      printContinuous("Please specify a password: ");

      // now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
      TRI_SetStdinVisibility(false);
      getline(cin, _password);

      TRI_SetStdinVisibility(true);
#else
      getline(cin, _password);
#endif
      printLine("");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string and a newline to stderr
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printErrLine (const string& s) {
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

void ArangoClient::_printLine (const string &s) {
#ifdef _WIN32
  LPWSTR wBuf = (LPWSTR) TRI_Allocate(TRI_CORE_MEM_ZONE, (sizeof WCHAR)* (s.size() + 1), true);
  int wLen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wBuf, (int) ((sizeof WCHAR) * (s.size() + 1)));

  if (wLen) {
    DWORD n;
    COORD pos;
    CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
    // save old cursor position
    pos = bufferInfo.dwCursorPosition;

    auto newX = pos.X + s.size();
    auto oldY = pos.Y;
    if (newX >= bufferInfo.dwSize.X) {
      for (size_t i = 0; i <= newX / bufferInfo.dwSize.X; ++i) {
        // insert as many newlines as we need. this prevents running out of buffer space when printing lines
        // at the end of the console output buffer
        if (_newLine()) {
          pos.Y = pos.Y - 1;
        }
      }
    }

    // save new cursor position
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
    auto newPos = bufferInfo.dwCursorPosition;

    // print the actual string. note: printing does not advance the cursor position
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    WriteConsoleOutputCharacterW(GetStdHandle(STD_OUTPUT_HANDLE), wBuf, (DWORD) s.size(), pos, &n);
  
    // finally set the cursor position to where the printing should have stopped 
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), newPos);
  }
  else {
    fprintf(stdout, "window error: '%d' \r\n", GetLastError());
    fprintf(stdout, "%s\r\n", s.c_str());
  }

  if (wBuf) {
    TRI_Free(TRI_CORE_MEM_ZONE, wBuf);
  }
#endif
}

void ArangoClient::printLine (const string& s, bool forceNewLine) {
#ifdef _WIN32
  // no, we cannot use std::cout as this doesn't support UTF-8 on Windows
  //fprintf(stdout, "%s\r\n", s.c_str());
  TRI_vector_string_t subStrings = TRI_SplitString(s.c_str(), '\n');
  bool hasNewLines = (s.find("\n") != string::npos) | forceNewLine;
  if (hasNewLines) {
    for (size_t i = 0; i < subStrings._length; i++) {
      _printLine(subStrings._buffer[i]);
      _newLine();
    }
  }
  else {
    _printLine(s);
  }
  TRI_DestroyVectorString(&subStrings);
#else
  fprintf(stdout, "%s\n", s.c_str());
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string to stdout, without a newline (Non-Windows only)
/// on Windows, we'll print the line and a newline
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printContinuous (const string& s) {
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

#ifdef _WIN32

void ArangoClient::startPager () {
  // not supported
  if (!_usePager || _usePager) {
    return;
  }
}

#else

void ArangoClient::startPager () {
  if (! _usePager || _outputPager == "" || _outputPager == "stdout" || _outputPager == "-") {
    _pager = stdout;
    return;
  }

  _pager = popen(_outputPager.c_str(), "w");

  if (_pager == 0) {
    printf("popen() failed! Using stdout instead!\n");
    _pager = stdout;
    _usePager = false;
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

void ArangoClient::stopPager () {
  // not supported
}

#else

void ArangoClient::stopPager () {
  if (_pager != stdout) {
    pclose(_pager);
    _pager = stdout;
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief strips binary data from string
/// this is done before sending the string to a pager or writing it to the log
////////////////////////////////////////////////////////////////////////////////

static std::string StripBinary (const char* value) {
  string result;

  char const* p = value;
  bool inBinary = false;

  while (*p) {
    if (inBinary) {
      if (*p == 'm') {
        inBinary = false;
      }
    }
    else {
      if (*p == '\x1b') {
        inBinary = true;
      }
      else {
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

void ArangoClient::internalPrint (const string & str) {

  if (_pager == stdout) {
#ifdef _WIN32
// at moment the formating is ignored in windows
    printLine(str);
#else
    fprintf(_pager, "%s", str.c_str());
#endif
    if (_log) {
      string sanitised = StripBinary(str.c_str());
      log("%s", sanitised.c_str());
    }
  }
  else {
    string sanitised = StripBinary(str.c_str());

    if (! sanitised.empty()) {
      fprintf(_pager, "%s", sanitised.c_str());
      log("%s", sanitised.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::openLog () {
  if (! _logFile.empty()) {
    _log = fopen(_logFile.c_str(), "w");

    ostringstream s;
    if (_log == 0) {
      s << "Cannot open file '" << _logFile << "' for logging.";
      printErrLine(s.str());
    }
    else {
      s << "Logging input and output to '" << _logFile << "'.";
      printLine(s.str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::closeLog () {
  if (! _logFile.empty() && _log != 0) {
    fclose(_log);
    _log = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints info message
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printWelcomeInfo () {
  if (_usePager) {
     ostringstream s;
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

void ArangoClient::printByeBye () {
  if (! _quiet) {
    printLine("<ctrl-D>");
    printLine(TRI_BYE_MESSAGE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs output, without prompt
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::log (const char* format,
                        const char* str) {
  if (_log) {
    string sanitised = StripBinary(str);

    if (! sanitised.empty()) {
      // do not print terminal escape sequences into log
      fprintf(_log, format, sanitised.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs output, with prompt
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::log (const char* format,
                        const char* prompt,
                        const char* str) {
  if (_log) {
    string sanitised = StripBinary(str);

    if (! sanitised.empty()) {
      // do not print terminal escape sequences into log
      fprintf(_log, format, prompt, sanitised.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes log output
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::flushLog () {
  if (_log) {
    fflush(_log);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an new endpoint
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::createEndpoint () {
  createEndpoint(_endpointString);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an new endpoint
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::createEndpoint (string const& definition) {
  // close previous endpoint
  if (_endpointServer != 0) {
    delete _endpointServer;
  }

  _endpointServer = Endpoint::clientFactory(definition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet start
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::quiet () const {
  return _quiet;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts up arangosh
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::shutup () {
  _quiet = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivates colors
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::colors () const {
  return ! _noColors;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the auto completion flag
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::autoComplete () const {
  return ! _noAutoComplete;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::prettyPrint () const {
  return _prettyPrint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the output pager
////////////////////////////////////////////////////////////////////////////////

string const& ArangoClient::outputPager () const {
  return _outputPager;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets use pager
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::usePager () const {
  return _usePager;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets use pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setUsePager (bool value) {
  _usePager = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets endpoint to connect to as string
////////////////////////////////////////////////////////////////////////////////

string const& ArangoClient::endpointString () const {
  return _endpointString;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets endpoint to connect to as string
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setEndpointString (string const& value) {
  _endpointString = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint
////////////////////////////////////////////////////////////////////////////////

Endpoint* ArangoClient::endpointServer() const {
  return _endpointServer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

string const& ArangoClient::databaseName () const {
  return _databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief user to send to endpoint
////////////////////////////////////////////////////////////////////////////////

string const& ArangoClient::username () const {
  return _username;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief password to send to endpoint
////////////////////////////////////////////////////////////////////////////////

string const& ArangoClient::password () const {
  return _password;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets database name
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setDatabaseName (string const& databaseName) {
  _databaseName = databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets username
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setUsername (string const& username) {
  _username = username;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets password
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setPassword (string const& password) {
  _password = password;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in seconds)
////////////////////////////////////////////////////////////////////////////////

double ArangoClient::connectTimeout () const {
  return _connectTimeout;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout (in seconds)
////////////////////////////////////////////////////////////////////////////////

double ArangoClient::requestTimeout () const {
  return _requestTimeout;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl protocol
////////////////////////////////////////////////////////////////////////////////

uint32_t ArangoClient::sslProtocol () const {
  return _sslProtocol;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
