////////////////////////////////////////////////////////////////////////////////
/// @brief application server skeleton
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
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationServer.h"

#include <pwd.h>
#include <grp.h>

#include "ApplicationServer/ApplicationFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/Random.h"
#include "Basics/StringUtils.h"
#include "Basics/delete_object.h"
#include "BasicsC/conversions.h"
#include "Logger/Logger.h"

#include "build.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Command Line Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_CMDLINE = "Command Line Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Hidden Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_HIDDEN = "Hidden Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Limit Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_LIMITS = "Limit Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Logger Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_LOGGER = "Logging Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Server Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_SERVER = "Server Options";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationServer::ApplicationServer (string const& title, string const& version)
  : _options(),
    _description(),
    _descriptionFile(),
    _arguments(),
    _features(),
    _exitOnParentDeath(false),
    _watchParent(0),
    _stopping(0),
    _title(title),
    _version(version),
    _configFile(),
    _userConfigFile(),
    _systemConfigFile(),
    _systemConfigPath(),
    _uid(),
    _loggingUid(0),
    _gid(),
    _loggingGid(0),
    _logApplicationName("triagens"),
    _logHostName("-"),
    _logFacility("-"),
    _logLevel("info"),
    _logFormat(),
    _logSeverity("human"),
    _logFile("+"),
    _logPrefix(),
    _logSyslog(),
    _logThreadId(false),
    _logLineNumber(false),
    _randomGenerator(3) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationServer::~ApplicationServer () {
  Random::shutdown();
  for_each(_features.begin(), _features.end(), DeleteObject());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new feature
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::addFeature (ApplicationFeature* feature) {
  _features.push_back(feature);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the system config file with a path
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setSystemConfigFile (string const& name, string const& path) {
  _systemConfigFile = name;
  _systemConfigPath = path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the system config file without a path
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setSystemConfigFile (string const& name) {
  return setSystemConfigFile(name, "");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the user config file
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setUserConfigFile (string const& name) {
  _userConfigFile = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the logging
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setupLogging () {
#ifdef TRI_HAVE_SETGID
  gid_t gid;
#endif

#ifdef TRI_HAVE_SETUID
  uid_t uid;
#endif

#ifdef TRI_HAVE_SETGID
  gid = getegid();
  setegid(_loggingGid);
#endif

#ifdef TRI_HAVE_SETUID
  uid = geteuid();
  seteuid(_loggingUid);
#endif

  bool threaded = TRI_ShutdownLogging();

  TRI_InitialiseLogging(threaded);

  Logger::setApplicationName(_logApplicationName);
  Logger::setHostName(_logHostName);
  Logger::setFacility(_logFacility);

  if (! _logFormat.empty()) {
    Logger::setLogFormat(_logFormat);
  }

  if (_options.has("log.thread")) {
    _logThreadId = true;
  }

  if (_options.has("log.line-number")) {
    _logLineNumber = true;
  }

  TRI_SetLineNumberLogging(_logLineNumber);
  TRI_SetLogLevelLogging(_logLevel);
  TRI_SetLogSeverityLogging(_logSeverity);
  TRI_SetPrefixLogging(_logPrefix);
  TRI_SetThreadIdentifierLogging(_logThreadId);

  for (vector<string>::iterator i = _logFilter.begin();  i != _logFilter.end();  ++i) {
    TRI_SetFileToLog(i->c_str());
  }

  if (NULL == TRI_CreateLogAppenderFile(_logFile)) {
    if (_logFile.length() > 0) {
      // the user specified a log file to use but it could not be created. bail out
      std::cerr << "failed to create logfile " << _logFile << std::endl;
      exit(EXIT_FAILURE);      

    }
  }
  TRI_CreateLogAppenderSyslog(_logPrefix, _logSyslog);

#ifdef TRI_HAVE_SETGID
  setegid(gid);
#endif

#ifdef TRI_HAVE_SETUID
  seteuid(uid);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the command line options
////////////////////////////////////////////////////////////////////////////////

ProgramOptions& ApplicationServer::programOptions () {
  return _options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the command line arguments
////////////////////////////////////////////////////////////////////////////////

vector<string> ApplicationServer::programArguments () {
  return _arguments;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the arguments with empty options description
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::parse (int argc, char* argv[]) {
  map<string, ProgramOptionsDescription> none;

  return parse(argc, argv, none);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the arguments
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::parse (int argc,
                               char* argv[],
                               map<string, ProgramOptionsDescription> opts) {

  // .............................................................................
  // setup the options
  // .............................................................................

  setupOptions(opts);

  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    (*i)->setupOptions(opts);
  }

  // construct options description
  for (map<string, ProgramOptionsDescription>::iterator i = opts.begin();  i != opts.end();  ++i) {
    string name = i->first;

    ProgramOptionsDescription sectionDescription = i->second;
    sectionDescription.setName(name);

    // and add to the global options
    _description(sectionDescription, name == OPTIONS_HIDDEN);

    if (name != OPTIONS_CMDLINE) {
      _descriptionFile(sectionDescription, name == OPTIONS_HIDDEN);
    }
  }

  _description.arguments(&_arguments);

  // .............................................................................
  // parse command line
  // .............................................................................

  bool ok = _options.parse(_description, argc, argv);

  if (! ok) {
    cout << _options.lastError() << endl;
    return false;
  }

  // check for help
  set<string> help = _options.needHelp("help");

  if (! help.empty()) {
    cout << argv[0] << " " << _title << "\n\n" << _description.usage(help) << endl;
    exit(EXIT_SUCCESS);
  }

  // check for version request
  if (_options.has("version")) {
    cout << _version << endl;
    exit(EXIT_SUCCESS);
  }

  // setup logging
  storeLoggingPrivileges();
  setupLogging();

  // .............................................................................
  // parse phase 1
  // .............................................................................

  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ok = (*i)->parsePhase1(_options);

    if (! ok) {
      return false;
    }
  }

  // .............................................................................
  // check configuration file
  // .............................................................................

  ok = readConfigurationFile();

  if (! ok) {
    return false;
  }

  // re-set logging using the additional config file entries
  setupLogging();


  // .............................................................................
  // parse phase 2
  // .............................................................................

  if (! _options.has("random.no-seed")) {
    Random::seed();
  }

  try {
    switch (_randomGenerator) {
      case 1: Random::selectVersion(Random::RAND_MERSENNE);  break;
      case 2: Random::selectVersion(Random::RAND_RANDOM);  break;
      case 3: Random::selectVersion(Random::RAND_URANDOM);  break;
      case 4: Random::selectVersion(Random::RAND_COMBINED);  break;
      default: break;
    }
  }
  catch (...) {
    LOGGER_FATAL << "cannot select random generator, giving up";
    TRI_ShutdownLogging();
    exit(EXIT_FAILURE);
  }

  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ok = (*i)->parsePhase2(_options);

    if (! ok) {
      return false;
    }
  }

  // .............................................................................
  // now drop all privilegs
  // .............................................................................

  dropPriviliges();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::prepare () {

  // prepare all features
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOGGER_DEBUG << "preparing server feature '" << feature->getName() << "'";

    bool ok = feature->prepare();

    if (! ok) {
      LOGGER_FATAL << "failed to prepare server feature '" << feature->getName() <<"'";
      exit(EXIT_FAILURE);      
    }

    LOGGER_TRACE << "prepared server feature '" << feature->getName() << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the server
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::start () {
  LOGGER_DEBUG << "ApplicationServer version " << TRIAGENS_VERSION;

  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);

  // start all startable features
  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ApplicationFeature* feature = *i;

    bool ok = feature->start();

    if (! ok) {
      LOGGER_FATAL << "failed to start server feature '" << feature->getName() <<"'";
      exit(EXIT_FAILURE);      
    }

    LOGGER_DEBUG << "started server feature '" << feature->getName() << "'";
  }

  // now open all features
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOGGER_DEBUG << "opening server feature '" << feature->getName() << "'";

    bool ok = feature->open();

    if (! ok) {
      LOGGER_FATAL << "failed to open server feature '" << feature->getName() <<"'";
      exit(EXIT_FAILURE);      
    }

    LOGGER_TRACE << "opened server feature '" << feature->getName() << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for shutdown
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::wait () {
  bool running = true;

  // wait until we receive a stop signal
  while (running && _stopping == 0) {

    // check the parent and wait for a second
    if (! checkParent()) {
      running = false;
      break;
    }

    sleep(1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::beginShutdown () {
  if (_stopping != 0) {
    return;
  }

  _stopping = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops everything
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::stop () {
  beginShutdown();

  // close all features
  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ApplicationFeature* feature = *i;

    feature->close();

    LOGGER_TRACE << "closed server feature '" << feature->getName() << "'";
  }

  // stop all features
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOGGER_DEBUG << "shutting down server feature '" << feature->getName() << "'";

    feature->stop();

    LOGGER_TRACE << "shut down server feature '" << feature->getName() << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setupOptions (map<string, ProgramOptionsDescription>& options) {

  // .............................................................................
  // command line options
  // .............................................................................

  options[OPTIONS_CMDLINE]
    ("version,v", "print version string and exit")
    ("help,h", "produce a usage message and exit")
    ("configuration,c", &_configFile, "read configuration file")
  ;

#if defined(TRI_HAVE_SETUID) || defined(TRI_HAVE_SETGID)

  options[OPTIONS_CMDLINE + ":help-extended"]
#ifdef TRI_HAVE_SETUID
    ("uid", &_uid, "switch to user-id after reading config files")
#endif
#ifdef TRI_HAVE_SETGID
    ("gid", &_gid, "switch to group-id after reading config files")
#endif
#ifdef TRI_HAVE_GETPPID
    ("exit-on-parent-death", &_exitOnParentDeath, "exit if parent dies")
#endif
    ("watch-process", &_watchParent, "exit if process with given PID dies")
  ;

#endif

  // .............................................................................
  // logger options
  // .............................................................................

  options[OPTIONS_LOGGER]
    ("log.file", &_logFile, "log to file")
    ("log.level,l", &_logLevel, "log level for severity 'human'")
  ;

  options[OPTIONS_LOGGER + ":help-log"]
    ("log.application", &_logApplicationName, "application name")
    ("log.facility", &_logFacility, "facility name")
    ("log.filter", &_logFilter, "filter for debug and trace")
    ("log.format", &_logFormat, "log format")
    ("log.hostname", &_logHostName, "host name")
    ("log.line-number", "always log file and line number")
    ("log.prefix", &_logPrefix, "prefix log")
    ("log.severity", &_logSeverity, "log severities")
    ("log.syslog", &_logSyslog, "use syslog facility")
    ("log.thread", "log the thread identifier for severity 'human'")
  ;

  options[OPTIONS_HIDDEN]
    ("log", &_logLevel, "log level for severity 'human'")
  ;

  // .............................................................................
  // application server options
  // .............................................................................

  options[OPTIONS_SERVER + ":help-extended"]
    ("random.no-seed", "do not seed the random generator")
    ("random.generator", &_randomGenerator, "1 = mersenne, 2 = random, 3 = urandom, 4 = combined")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the parent is still alive
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::checkParent () {

  // check our parent, if it died given up
#ifdef TRI_HAVE_GETPPID
  if (_exitOnParentDeath && getppid() == 1) {
    LOGGER_INFO << "parent has died";
    return false;
  }
#endif

  if (_watchParent != 0) {
    int res = kill(_watchParent, 0);

    if (res != 0) {
      LOGGER_INFO << "parent " << _watchParent << " has died";
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads the configuration files
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::readConfigurationFile () {

  // something has been specified on the command line regarding configuration file
  if (! _configFile.empty()) {

    // do not use init files
    if (StringUtils::tolower(_configFile) == string("none")) {
      LOGGER_INFO << "using no init file at all";
      return true;
    }

    LOGGER_INFO << "using init file '" << _configFile << "'";

    bool ok = _options.parse(_descriptionFile, _configFile);

    // Observe that this is treated as an error - the configuration file exists
    // but for some reason can not be parsed. Best to report an error.

    if (! ok) {
      cout << _options.lastError() << endl;
    }

    return ok;
  }
  else {
    LOGGER_DEBUG << "no init file has been specified";
  }

  // nothing has been specified on the command line regarding configuration file
  if (! _userConfigFile.empty()) {

    // first attempt to obtain a default configuration file from the users home directory
    string homeDir = string(getenv("HOME"));

    if (! homeDir.empty()) {
      if (homeDir[homeDir.size() - 1] != '/') {
        homeDir += "/" + _userConfigFile;
      }
      else {
        homeDir += _userConfigFile;
      }

      // check and see if file exists
      if (FileUtils::exists(homeDir)) {
        LOGGER_INFO << "using user init file '" << homeDir << "'";

        bool ok = _options.parse(_descriptionFile, homeDir);

        // Observe that this is treated as an error - the configuration file exists
        // but for some reason can not be parsed. Best to report an error.

        if (! ok) {
          cout << _options.lastError() << endl;
        }

        return ok;
      }
      else {
        LOGGER_INFO << "no user init file '" << homeDir << "' found";
      }
    }
    else {
      LOGGER_DEBUG << "no user init file, $HOME is empty";
    }
  }

  if (_systemConfigPath.empty()) {

#ifdef _SYSCONFDIR_

    // try the configuration file in the system directory - if there is one

    // Please note that the system directory changes depending on
    // where the user installed the application server.

    if (! _systemConfigFile.empty()) {
      string sysDir = string(_SYSCONFDIR_);

      if (! sysDir.empty()) {
        if (sysDir[sysDir.size() - 1] != '/') {
          sysDir += "/" + _systemConfigFile;
        }
        else {
          sysDir += _systemConfigFile;
        }

        // check and see if file exists
        if (FileUtils::exists(sysDir)) {
          LOGGER_INFO << "using init file '" << sysDir << "'";

          bool ok = _options.parse(_descriptionFile, sysDir);

          // Observe that this is treated as an error - the configuration file exists
          // but for some reason can not be parsed. Best to report an error.

          if (! ok) {
            cout << _options.lastError() << endl;
          }

          return ok;
        }
        else {
          LOGGER_INFO << "no system init file '" << sysDir << "' found";
        }
      }
      else {
        LOGGER_DEBUG << "no system init file, not system directory is known";
      }
    }

#endif
  }
  else {
    if (! _systemConfigFile.empty()) {
      string sysDir = _systemConfigPath + "/" + _systemConfigFile;

      // check and see if file exists
      if (FileUtils::exists(sysDir)) {
        LOGGER_INFO << "using init file '" << sysDir << "'";

        bool ok = _options.parse(_descriptionFile, sysDir);

        // Observe that this is treated as an error - the configuration file exists
        // but for some reason can not be parsed. Best to report an error.

        if (! ok) {
          cout << _options.lastError() << endl;
        }

        return ok;
      }
      else {
        LOGGER_INFO << "no system init file '" << sysDir << "' found";
      }
    }
    else {
      LOGGER_DEBUG << "no system init file specified";
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves the logging privileges
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::storeLoggingPrivileges () {
#ifdef TRI_HAVE_SETGID
  _loggingGid = getegid();
#endif

#ifdef TRI_HAVE_SETUID
  _loggingUid = geteuid();
#endif
}


////////////////////////////////////////////////////////////////////////////////
/// @brief drops the privileges
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::dropPriviliges () {

#ifdef TRI_HAVE_SETGID

  if (! _gid.empty()) {
    LOGGER_TRACE << "trying to switch to group '" << _gid << "'";

    int gidNumber = TRI_Int32String(_gid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR) {
      LOGGER_TRACE << "trying to switch to numeric gid '" << gidNumber << "' for '" << _gid << "'";

#ifdef TRI_HAVE_GETGRGID
      group* g = getgrgid(gidNumber);

      if (g == 0) {
        LOGGER_FATAL << "unknown numeric gid '" << _gid << "'";
        TRI_ShutdownLogging();
        exit(EXIT_FAILURE);
      }
#endif
    }
    else {
#ifdef TRI_HAVE_GETGRNAM
      string name = _gid;
      group* g = getgrnam(name.c_str());

      if (g != 0) {
        gidNumber = g->gr_gid;
        LOGGER_TRACE << "trying to switch to numeric gid '" << gidNumber << "'";
      }
      else {
        LOGGER_FATAL << "cannot convert groupname '" << _gid << "' to numeric gid";
        TRI_ShutdownLogging();
        exit(EXIT_FAILURE);
      }
#else
      LOGGER_FATAL << "cannot convert groupname '" << _gid << "' to numeric gid";
      TRI_ShutdownLogging();
      exit(EXIT_FAILURE);
#endif
    }

    LOGGER_INFO << "changing gid to '" << gidNumber << "'";

    int res = setegid(gidNumber);

    if (res != 0) {
      LOGGER_FATAL << "cannot set gid '" << _gid << "', because " << strerror(errno);
      TRI_ShutdownLogging();
      exit(EXIT_FAILURE);
    }
  }

#endif

#ifdef TRI_HAVE_SETUID

  if (! _uid.empty()) {
    LOGGER_TRACE << "trying to switch to user '" << _uid << "'";

    int uidNumber = TRI_Int32String(_uid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR) {
      LOGGER_TRACE << "trying to switch to numeric uid '" << uidNumber << "' for '" << _uid << "'";

#ifdef TRI_HAVE_GETPWUID
      passwd* p = getpwuid(uidNumber);

      if (p == 0) {
        LOGGER_FATAL << "unknown numeric uid '" << _uid << "'";
        TRI_ShutdownLogging();
        exit(EXIT_FAILURE);
      }
#endif
    }
    else {
#ifdef TRI_HAVE_GETPWNAM
      string name = _uid;
      passwd* p = getpwnam(name.c_str());

      if (p != 0) {
        uidNumber = p->pw_uid;
        LOGGER_TRACE << "trying to switch to numeric uid '" << uidNumber << "'";
      }
      else {
        LOGGER_FATAL << "cannot convert username '" << _uid << "' to numeric uid";
        TRI_ShutdownLogging();
        exit(EXIT_FAILURE);
      }
#else
      LOGGER_FATAL << "cannot convert username '" << _uid << "' to numeric uid";
      TRI_ShutdownLogging();
      exit(EXIT_FAILURE);
#endif
    }

    LOGGER_INFO << "changing uid to '" << uidNumber << "'";

    int res = seteuid(uidNumber);

    if (res != 0) {
      LOGGER_FATAL << "cannot set uid '" << _uid << "', because " << strerror(errno);
      TRI_ShutdownLogging();
      exit(EXIT_FAILURE);
    }
  }

#endif
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
