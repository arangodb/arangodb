////////////////////////////////////////////////////////////////////////////////
/// @brief arango shell client base
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ArangoClient.h"

#include "BasicsC/logging.h"
#include "BasicsC/messages.h"
#include "BasicsC/terminal-utils.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/ProgramOptions.h"
#include "Logger/Logger.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;
        
double const ArangoClient::DEFAULT_CONNECTION_TIMEOUT = 3.0;
double const ArangoClient::DEFAULT_REQUEST_TIMEOUT    = 300.0;
size_t const ArangoClient::DEFAULT_RETRIES            = 2;

// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoClient
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ArangoClient::ArangoClient () 
  : _configFile(),
    _logLevel("info"),
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
    _logOptions(false),

    _serverOptions(false),
    _disableAuthentication(false),
    _endpointString(),
    _endpointServer(0),
    _username("root"),
    _password(""),
    _hasPassword(false),
    _connectTimeout(DEFAULT_CONNECTION_TIMEOUT),
    _requestTimeout(DEFAULT_REQUEST_TIMEOUT) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the general and logging options
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupGeneral (ProgramOptionsDescription& description) {
  ProgramOptionsDescription loggingOptions("LOGGING options");

  loggingOptions
    ("log.level,l", &_logLevel,  "log level")
  ;

  description
    ("configuration,c", &_configFile, "read configuration file")
    ("help,h", "help message")
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
    ("server.disable-authentication", &_disableAuthentication, "disable authentication")
    ("server.endpoint", &_endpointString, "endpoint to connect to, use 'none' to start without a server")
    ("server.username", &_username, "username to use when connecting")
    ("server.password", &_password, "password to use when connecting (leave empty for prompt)")
    ("server.connect-timeout", &_connectTimeout, "connect timeout in seconds")
    ("server.request-timeout", &_requestTimeout, "request timeout in seconds")
  ;

  description(clientOptions, false);

  _serverOptions = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses command line and config file and prepares logging
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::parse (ProgramOptions& options,
                          ProgramOptionsDescription& description, 
                          int argc,
                          char* argv[],
                          string const& initFilename) {
  if (! options.parse(description, argc, argv)) {
    LOGGER_FATAL_AND_EXIT(options.lastError());
  }

  // check for help
  set<string> help = options.needHelp("help");

  if (! help.empty()) {
    cout << description.usage(help) << endl;
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, NULL);
  }

  // setup the logging
  TRI_SetLogLevelLogging(_logLevel.c_str());
  TRI_CreateLogAppenderFile("-");
  TRI_SetLineNumberLogging(false);
  TRI_SetThreadIdentifierLogging(false);

  // parse config file
  string configFile = "";

  if (! _configFile.empty()) {
    if (StringUtils::tolower(_configFile) == string("none")) {
      LOGGER_INFO("using no init file at all");
    }
    else {
      configFile = _configFile;
    }
  }

#ifdef _SYSCONFDIR_

  else {
    string sysDir = string(_SYSCONFDIR_);
    string systemConfigFile = initFilename;

    if (! sysDir.empty()) {
      if (sysDir[sysDir.size() - 1] != '/') {
        sysDir += "/" + systemConfigFile;
      }
      else {
        sysDir += systemConfigFile;
      }

      if (FileUtils::exists(sysDir)) {
        configFile = sysDir;
      }
      else {
        LOGGER_DEBUG("no system init file '" << sysDir << "'");
      }
    }
  }
  
#endif

  if (! configFile.empty()) {
    LOGGER_DEBUG("using init file '" << configFile << "'");

    if (! options.parse(description, configFile)) {
      LOGGER_FATAL_AND_EXIT("cannot parse config file '" << configFile << "': " << options.lastError());
    }
  }

  // check if have a password
  _hasPassword = options.has("server.password") || 
                 options.has("server.disable-authentication") || 
                 options.has("jslint");

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
      LOGGER_FATAL_AND_EXIT("invalid value for --server.connect-timeout, must be positive");
    }

    if (_requestTimeout <= 0) {
      LOGGER_FATAL_AND_EXIT("invalid value for --server.request-timeout, must be positive");
    }
  
    // must specify a user name
    if (_username.size() == 0) {
      LOGGER_FATAL_AND_EXIT("no value specified for --server.username");
    }

    // no password given on command-line
    if (! _hasPassword) {
      usleep(10 * 1000);
      cout << "Please specify a password: " << flush;

      // now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
      TRI_SetStdinVisibility(false);
      getline(cin, _password);

      TRI_SetStdinVisibility(true);
#else
      getline(cin, _password);
#endif
      cout << "\n";
    }
  }
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
  if (!_usePager || _usePager) {
    return;
  }
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
/// @brief print to pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::internalPrint (const char* format, const char* str) {
  if (str) {
    if (*str == '\x1b') {
      // terminal escape sequence
      if (_pager == stdout) {
        fprintf(_pager, format, str);    
      }
    }
    else {
      // regular string value
      fprintf(_pager, format, str);    
      log(format, str);
    }
  }
  else {
    if (*format == '\x1b') {
      // terminal escape sequence
      if (_pager == stdout) {
        fprintf(_pager, "%s", format);    
      }
    }
    else {
      // regular string value
      fprintf(_pager, "%s", format);    
      log("%s", format);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::openLog () {
  if (! _logFile.empty()) {
    _log = fopen(_logFile.c_str(), "w");
    if (_log == 0) {
      cerr << "Cannot open file '" << _logFile << "' for logging." << endl;
    }
    else {
      cout << "Logging input and output to '" << _logFile << "'." << endl;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::closeLog () {
  if (! _logFile.empty() && _log != 0) {
    fclose(_log);
    _log = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print info message
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printWelcomeInfo () {
  if (_usePager) {
    cout << "Using pager '" << _outputPager << "' for output buffering." << endl;
  }

  if (_prettyPrint) {
    cout << "Pretty printing values." << endl;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print bye-bye
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printByeBye () {
  if (! _quiet) {
    cout << "<ctrl-D>" << endl << TRI_BYE_MESSAGE << endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief log output
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::log (const char* format, const char* str) {
  if (_log) {
    if (*str != '\x1b') {
      // do not print terminal escape sequences into log
      fprintf(_log, format, str); 
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush log output
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
/// @brief shut up arangosh
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::shutup () {
  _quiet = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivate colors
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
