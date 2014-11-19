////////////////////////////////////////////////////////////////////////////////
/// @brief application server skeleton
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
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "ApplicationServer.h"

#ifdef TRI_HAVE_POSIX_PWD_GRP
#include <pwd.h>
#include <grp.h>
#endif

#include "ApplicationServer/ApplicationFeature.h"
#include "Basics/ConditionLocker.h"
#include "Basics/FileUtils.h"
#include "Basics/RandomGenerator.h"
#include "Basics/StringUtils.h"
#include "Basics/delete_object.h"
#include "Basics/conversions.h"
#include "Basics/logging.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

static string DeprecatedParameter;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Command Line Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_CMDLINE = "Command Line Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Hidden Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_HIDDEN = "Hidden Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Logger Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_LOGGER = "Logging Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief Server Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_SERVER = "Server Options";

////////////////////////////////////////////////////////////////////////////////
/// @brief SSL Options
////////////////////////////////////////////////////////////////////////////////

string const ApplicationServer::OPTIONS_SSL = "SSL Options";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

ApplicationServer::ApplicationServer (std::string const& name, std::string const& title, std::string const& version)
  : _options(),
    _description(),
    _descriptionFile(),
    _arguments(),
    _features(),
    _exitOnParentDeath(false),
    _watchParent(0),
    _stopping(0),
    _name(name),
    _title(title),
    _version(version),
    _configFile(),
    _userConfigFile(),
    _systemConfigFile(),
    _uid(),
    _realUid(0),
    _effectiveUid(0),
    _gid(),
    _realGid(0),
    _effectiveGid(0),
    _logApplicationName("arangod"),
    _logFacility(""),
    _logLevel("info"),
    _logSeverity("human"),
    _logFile("+"),
    _logRequestsFile(""),
    _logPrefix(),
    _logThreadId(false),
    _logLineNumber(false),
    _logLocalTime(false),
    _logSourceFilter(),
    _logContentFilter(),
    _randomGenerator(5),
    _finishedCondition() {
}

#else

ApplicationServer::ApplicationServer (std::string const& name, std::string const& title, std::string const& version)
  : _options(),
    _description(),
    _descriptionFile(),
    _arguments(),
    _features(),
    _exitOnParentDeath(false),
    _watchParent(0),
    _stopping(0),
    _name(name),
    _title(title),
    _version(version),
    _configFile(),
    _userConfigFile(),
    _systemConfigFile(),
    _uid(),
    _realUid(0),
    _effectiveUid(0),
    _gid(),
    _realGid(0),
    _effectiveGid(0),
    _logApplicationName("arangod"),
    _logFacility(""),
    _logLevel("info"),
    _logSeverity("human"),
    _logFile("+"),
    _logRequestsFile(""),
    _logPrefix(),
    _logThreadId(false),
    _logLineNumber(false),
    _logLocalTime(false),
    _logSourceFilter(),
    _logContentFilter(),
    _randomGenerator(3),
    _finishedCondition() {
  storeRealPrivileges();
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationServer::~ApplicationServer () {
  Random::shutdown();
  for_each(_features.begin(), _features.end(), DeleteObjectAny());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new feature
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::addFeature (ApplicationFeature* feature) {
  _features.push_back(feature);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the system config file
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setSystemConfigFile (std::string const& name) {
  _systemConfigFile = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the user config file
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setUserConfigFile (std::string const& name) {
  _userConfigFile = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of the application
////////////////////////////////////////////////////////////////////////////////

string const& ApplicationServer::getName () const {
  return _name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the logging
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setupLogging (bool threaded, bool daemon) {
  TRI_ShutdownLogging(false);
  TRI_InitialiseLogging(threaded);

  if (_options.has("log.thread")) {
    _logThreadId = true;
  }

  if (_options.has("log.line-number")) {
    _logLineNumber = true;
  }
  
  if (_options.has("log.use-local-time")) {
    _logLocalTime = true;
  }

  if (! _logRequestsFile.empty()) {
    // add this so the user does not need to think about it
    _logSeverity += ",usage";
  }

  TRI_SetUseLocalTimeLogging(_logLocalTime);
  TRI_SetLineNumberLogging(_logLineNumber);

  TRI_SetLogLevelLogging(_logLevel.c_str());
  TRI_SetLogSeverityLogging(_logSeverity.c_str());
  TRI_SetPrefixLogging(_logPrefix.c_str());
  TRI_SetThreadIdentifierLogging(_logThreadId);

  for (auto it = _logSourceFilter.begin();  it != _logSourceFilter.end();  ++it) {
    TRI_SetFileToLog(it->c_str());
  }

  char const* contentFilter = nullptr;

  if (_options.has("log.content-filter")) {
    contentFilter = _logContentFilter.c_str();
  }

  // requests log (must come before the regular logs)
  if (! _logRequestsFile.empty()) {
    string filename = _logRequestsFile;

    if (daemon && filename != "+" && filename != "-") {
      filename = filename + ".daemon";
    }

    // this appender consumes all usage log messages, so they are not propagated to any others
    struct TRI_log_appender_s* appender = TRI_CreateLogAppenderFile(filename.c_str(),
                                                                    0,
                                                                    TRI_LOG_SEVERITY_USAGE,
                                                                    true);

    // the user specified a requests log file to use but it could not be created. bail out
    if (appender == nullptr) {
      LOG_FATAL_AND_EXIT("failed to create requests logfile '%s'. Please check the path and permissions.", filename.c_str());
    }
  }

  // regular log file
  if (! _logFile.empty()) {
    string filename = _logFile;

    if (daemon && filename != "+" && filename != "-") {
      filename = filename + ".daemon";
    }

    struct TRI_log_appender_s* appender = TRI_CreateLogAppenderFile(filename.c_str(),
                                                                    contentFilter,
                                                                    TRI_LOG_SEVERITY_UNKNOWN,
                                                                    false);

    // the user specified a log file to use but it could not be created. bail out
    if (appender == nullptr) {
      LOG_FATAL_AND_EXIT("failed to create logfile '%s'. Please check the path and permissions.", filename.c_str());
    }
  }

#ifdef TRI_ENABLE_SYSLOG
  if (! _logFacility.empty()) {
    TRI_CreateLogAppenderSyslog(_logApplicationName.c_str(),
                                _logFacility.c_str(),
                                contentFilter,
                                TRI_LOG_SEVERITY_UNKNOWN,
                                false);
  }
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
                               std::map<std::string, ProgramOptionsDescription> opts) {

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
    LOG_ERROR("cannot parse command line: %s", _options.lastError().c_str());
    return false;
  }

  // check for help
  set<string> help = _options.needHelp("help");

  if (! help.empty()) {
    // output help, but do not yet exit (we'll exit a little later so we can also
    // check the specified configuration for errors)
    cout << argv[0] << " " << _title << endl << endl << _description.usage(help) << endl;
  }

  // check for version request
  if (_options.has("version")) {
    cout << _version << endl;
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, NULL);
  }

  TRI_SetLogLevelLogging(_logLevel.c_str());

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

  // exit here if --help was specified.
  // this allows us to use --help to run a configuration file check, too, and
  // report errors to the user

  if (! help.empty()) {
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, NULL);
  }

  // .............................................................................
  // UID and GID
  // .............................................................................

  extractPrivileges();
  dropPrivileges();

  // .............................................................................
  // setup logging
  // .............................................................................

  setupLogging(false, false);

  // .............................................................................
  // parse phase 2
  // .............................................................................

  try {
    switch (_randomGenerator) {
      case 1: {
        Random::selectVersion(Random::RAND_MERSENNE);
        break;
      }
      case 2: {
        Random::selectVersion(Random::RAND_RANDOM);
        break;
      }
      case 3: {
        Random::selectVersion(Random::RAND_URANDOM);
        break;
      }
      case 4: {
        Random::selectVersion(Random::RAND_COMBINED);
        break;
      }
      case 5: {
        Random::selectVersion(Random::RAND_WIN32);
        break;
      }
      default: {
        break;
      }
    }
  }
  catch (...) {
    LOG_FATAL_AND_EXIT("cannot select random generator, giving up");
  }


  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ok = (*i)->parsePhase2(_options);

    if (! ok) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server, step one
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::prepare () {

  // prepare all features - why reverse?

  // reason: features might depend on each other. by creating them in reverse order and shutting them
  // down in forward order, we ensure that the features created last are destroyed first, i.e.: LIFO
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOG_DEBUG("preparing server feature '%s'", feature->getName().c_str());

    bool ok = feature->prepare();

    if (! ok) {
      LOG_FATAL_AND_EXIT("failed to prepare server feature '%s'", feature->getName().c_str());
    }

    LOG_TRACE("prepared server feature '%s'", feature->getName().c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server, step two
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::prepare2 () {

  // prepare all features
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOG_DEBUG("preparing(2) server feature '%s'", feature->getName().c_str());

    bool ok = feature->prepare2();

    if (! ok) {
      LOG_FATAL_AND_EXIT("failed to prepare(2) server feature '%s'", feature->getName().c_str());
    }

    LOG_TRACE("prepared(2) server feature '%s'", feature->getName().c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the server
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::start () {
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);
#endif

  raisePrivileges();

  // start all startable features
  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ApplicationFeature* feature = *i;

    bool ok = feature->start();

    if (! ok) {
      LOG_FATAL_AND_EXIT("failed to start server feature '%s'", feature->getName().c_str());
    }

    LOG_DEBUG("started server feature '%s'", feature->getName().c_str());
  }

  // now open all features
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOG_DEBUG("opening server feature '%s'", feature->getName().c_str());

    bool ok = feature->open();

    if (! ok) {
      LOG_FATAL_AND_EXIT("failed to open server feature '%s'", feature->getName().c_str());
    }

    LOG_TRACE("opened server feature '%s'", feature->getName().c_str());
  }

  dropPrivilegesPermanently();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for shutdown
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::wait () {
  // wait until we receive a stop signal
  while (_stopping == 0) {
    // check the parent and wait for a second
    if (! checkParent()) {
      break;
    }

    CONDITION_LOCKER(locker, _finishedCondition);
    locker.wait((uint64_t) (1000 * 1000));
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

  CONDITION_LOCKER(locker, _finishedCondition);
  locker.signal();

  // close all features
  for (vector<ApplicationFeature*>::iterator i = _features.begin();  i != _features.end();  ++i) {
    ApplicationFeature* feature = *i;

    feature->close();

    LOG_TRACE("closed server feature '%s'", feature->getName().c_str());
  }


  // stop all features
  for (vector<ApplicationFeature*>::reverse_iterator i = _features.rbegin();  i != _features.rend();  ++i) {
    ApplicationFeature* feature = *i;

    LOG_DEBUG("shutting down server feature '%s'", feature->getName().c_str());
    feature->stop();
    LOG_TRACE("shut down server feature '%s'", feature->getName().c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief raise the privileges
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::raisePrivileges () {

  // first UID
#ifdef TRI_HAVE_SETUID

  if (_effectiveUid != _realUid) {
    int res = seteuid(_realUid);

    if (res != 0) {
      LOG_FATAL_AND_EXIT("cannot set uid '%s': %s", _uid.c_str(), strerror(errno));
    }
  }

#endif

  // then GID (because we are raising)
#ifdef TRI_HAVE_SETGID

  if (_effectiveGid != _realGid) {
    int res = setegid(_realGid);

    if (res != 0) {
      LOG_FATAL_AND_EXIT("cannot set gid %d: %s", (int) _effectiveGid, strerror(errno));
    }
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the privileges
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::dropPrivileges () {

  // first GID
#ifdef TRI_HAVE_SETGID

  if (_effectiveGid != _realGid) {
    int res = setegid(_effectiveGid);

    if (res != 0) {
      LOG_FATAL_AND_EXIT("cannot set gid %d: %s", (int) _effectiveGid, strerror(errno));
    }
  }

#endif

  // then UID (because we are dropping)
#ifdef TRI_HAVE_SETUID

  if (_effectiveUid != _realUid) {
    int res = seteuid(_effectiveUid);

    if (res != 0) {
      LOG_FATAL_AND_EXIT("cannot set uid %s: %s", _uid.c_str(), strerror(errno));
    }
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the privileges permanently
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::dropPrivilegesPermanently () {
  raisePrivileges();

  // clear all supplementary groups
#if defined(TRI_HAVE_INITGROUPS) && defined(TRI_HAVE_SETGID) && defined(TRI_HAVE_SETUID)

  struct passwd* pwent = getpwuid(_effectiveUid);

  if (pwent != nullptr) {
    initgroups(pwent->pw_name, _effectiveGid);
  }

#endif  

  // first GID
#ifdef TRI_HAVE_SETGID

  if (_effectiveGid != _realGid) {
    LOG_INFO("permanently changing the gid to %d", (int) _effectiveGid);

    int res = setgid(_effectiveGid);

    if (res != 0) {
      LOG_FATAL_AND_EXIT("cannot set gid %d: %s", (int) _effectiveGid, strerror(errno));
    }

    _realGid = _effectiveGid;
  }

#endif

  // then UID (because we are dropping)
#ifdef TRI_HAVE_SETUID

  if (_effectiveUid != _realUid) {
    LOG_INFO("permanently changing the uid to %d", (int) _effectiveUid);

    int res = setuid(_effectiveUid);

    if (res != 0) {
      LOG_FATAL_AND_EXIT("cannot set uid '%s': %s", _uid.c_str(), strerror(errno));
    }

    _realUid = _effectiveUid;
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves the logging privileges
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::storeRealPrivileges () {
#ifdef TRI_HAVE_SETGID
  _realGid = getgid();
#endif

#ifdef TRI_HAVE_SETUID
  _realUid = getuid();
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

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
    ("log.requests-file", &_logRequestsFile, "log requests to file")
    ("log.level,l", &_logLevel, "log level for severity 'human'")
  ;

  options[OPTIONS_LOGGER + ":help-log"]
    ("log.application", &_logApplicationName, "application name for syslog")
    ("log.facility", &_logFacility, "facility name for syslog (OS dependent)")
    ("log.source-filter", &_logSourceFilter, "only debug and trace messages emitted by specific C source file")
    ("log.content-filter", &_logContentFilter, "only log message containing the specified string (case-sensitive)")
    ("log.line-number", "always log file and line number")
    ("log.prefix", &_logPrefix, "prefix log")
    ("log.severity", &_logSeverity, "log severities")
    ("log.thread", "log the thread identifier for severity 'human'")
    ("log.use-local-time", "use local dates and times in log messages")
  ;

  options[OPTIONS_HIDDEN]
    ("log", &_logLevel, "log level for severity 'human'")
    ("log.syslog", &DeprecatedParameter, "use syslog facility (deprecated)")
    ("log.hostname", &DeprecatedParameter, "host name for syslog")
#ifdef TRI_HAVE_SETUID
    ("uid", &_uid, "switch to user-id after reading config files")
#endif
#ifdef TRI_HAVE_SETGID
    ("gid", &_gid, "switch to group-id after reading config files")
#endif
  ;

  // .............................................................................
  // application server options
  // .............................................................................

  options[OPTIONS_SERVER + ":help-extended"]
    ("random.generator", &_randomGenerator, "1 = mersenne, 2 = random, 3 = urandom, 4 = combined")
#ifdef TRI_HAVE_SETUID
    ("server.uid", &_uid, "switch to user-id after reading config files")
#endif
#ifdef TRI_HAVE_SETGID
    ("server.gid", &_gid, "switch to group-id after reading config files")
#endif
  ;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the parent is still alive
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::checkParent () {

  // check our parent, if it died given up
#ifdef TRI_HAVE_GETPPID
  if (_exitOnParentDeath && getppid() == 1) {
    LOG_INFO("parent has died");
    return false;
  }
#endif

// unfortunately even though windows has <signal.h>, there is no
// kill method defined. Notice that the kill below is not to terminate
// the process.
#ifdef TRI_HAVE_SIGNAL_H
  if (_watchParent != 0) {
#ifdef TRI_HAVE_POSIX
    int res = kill(_watchParent, 0);
#else
    int res = -1;
#endif
    if (res != 0) {
      LOG_INFO("parent %d has died", (int) _watchParent);
      return false;
    }
  }
#endif

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
      LOG_INFO("using no init file at all");
      return true;
    }

    LOG_DEBUG("using init file '%s'", _configFile.c_str());

    bool ok = _options.parse(_descriptionFile, _configFile);

    // Observe that this is treated as an error - the configuration file exists
    // but for some reason can not be parsed. Best to report an error.

    if (! ok) {
      LOG_ERROR("cannot parse config file '%s': %s", _configFile.c_str(), _options.lastError().c_str());
    }

    return ok;
  }
  else {
    LOG_DEBUG("no init file has been specified");
  }


  // nothing has been specified on the command line regarding the user's configuration file
  if (! _userConfigFile.empty()) {

    // .........................................................................
    // first attempt to obtain a default configuration file from the users home directory
    // .........................................................................

    // .........................................................................
    // Under windows there is no 'HOME' directory as such so getenv("HOME")
    // may return NULL -- which it does under windows
    // A safer approach below
    // .........................................................................

    string homeDir = FileUtils::homeDirectory(); // TODO homedirectory should either always or never end in "/"

    if (! homeDir.empty()) {
      if (homeDir[homeDir.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
        homeDir += TRI_DIR_SEPARATOR_CHAR + _userConfigFile;
      }
      else {
        homeDir += _userConfigFile;
      }

      // check and see if file exists
      if (FileUtils::exists(homeDir)) {
        LOG_DEBUG("using user init file '%s'", homeDir.c_str());

        bool ok = _options.parse(_descriptionFile, homeDir);

        // Observe that this is treated as an error - the configuration file exists
        // but for some reason can not be parsed. Best to report an error.

        if (! ok) {
          LOG_ERROR("cannot parse config file '%s': %s", homeDir.c_str(), _options.lastError().c_str());
        }

        return ok;
      }
      else {
        LOG_DEBUG("no user init file '%s' found", homeDir.c_str());
      }
    }
    else {
      LOG_DEBUG("no home directory found");
    }
  }


  // try the configuration file in the system directory - if there is one
  if (! _systemConfigFile.empty()) {

    // Please note that the system directory changes depending on
    // where the user installed the application server.

    char* d = TRI_LocateConfigDirectory();

    if (d != 0) {
      string sysDir = string(d) + _systemConfigFile;
      string localSysDir = sysDir + ".local";

      TRI_FreeString(TRI_CORE_MEM_ZONE, d);

      // check and see if a local override file exists
      if (FileUtils::exists(localSysDir)) {
        LOG_DEBUG("using init override file '%s'", localSysDir.c_str());

        bool ok = _options.parse(_descriptionFile, localSysDir);

        // Observe that this is treated as an error - the configuration file exists
        // but for some reason can not be parsed. Best to report an error.
        if (! ok) {
          LOG_ERROR("cannot parse config file '%s': %s", localSysDir.c_str(), _options.lastError().c_str());
          return ok;
        }
      }
      else {
        LOG_DEBUG("no system init override file '%s' found", sysDir.c_str());
      }

      // check and see if file exists
      if (FileUtils::exists(sysDir)) {
        LOG_DEBUG("using init file '%s'", sysDir.c_str());

        bool ok = _options.parse(_descriptionFile, sysDir);

        // Observe that this is treated as an error - the configuration file exists
        // but for some reason can not be parsed. Best to report an error.
        if (! ok) {
          LOG_ERROR("cannot parse config file '%s': %s", sysDir.c_str(), _options.lastError().c_str());
        }

        return ok;
      }
      else {
        LOG_INFO("no system init file '%s' found", sysDir.c_str());
      }
    }
    else {
      LOG_DEBUG("no system init file specified");
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the privileges to use
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::extractPrivileges() {

#ifdef TRI_HAVE_SETGID

  if (_gid.empty()) {
    _effectiveGid = getgid();
  }
  else {
    int gidNumber = TRI_Int32String(_gid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR && gidNumber >= 0) {

#ifdef TRI_HAVE_GETGRGID
      group* g = getgrgid(gidNumber);

      if (g == 0) {
        LOG_FATAL_AND_EXIT("unknown numeric gid '%s'", _gid.c_str());
      }
#endif
    }
    else {
#ifdef TRI_HAVE_GETGRNAM
      string name = _gid;
      group* g = getgrnam(name.c_str());

      if (g != 0) {
        gidNumber = g->gr_gid;
      }
      else {
        LOG_FATAL_AND_EXIT("cannot convert groupname '%s' to numeric gid", _gid.c_str());
      }
#else
      LOG_FATAL_AND_EXIT("cannot convert groupname '%s' to numeric gid", _gid.c_str());
#endif
    }

    _effectiveGid = gidNumber;
  }

#endif

#ifdef TRI_HAVE_SETUID

  if (_uid.empty()) {
    _effectiveUid = getuid();
  }
  else {
    int uidNumber = TRI_Int32String(_uid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR) {

#ifdef TRI_HAVE_GETPWUID
      passwd* p = getpwuid(uidNumber);

      if (p == 0) {
        LOG_FATAL_AND_EXIT("unknown numeric uid '%s'", _uid.c_str());
      }
#endif
    }
    else {
#ifdef TRI_HAVE_GETPWNAM
      string name = _uid;
      passwd* p = getpwnam(name.c_str());

      if (p != 0) {
        uidNumber = p->pw_uid;
      }
      else {
        LOG_FATAL_AND_EXIT("cannot convert username '%s' to numeric uid", _uid.c_str());
      }
#else
      LOG_FATAL_AND_EXIT("cannot convert username '%s' to numeric uid", _uid.c_str());
#endif
    }

    _effectiveUid = uidNumber;
  }

#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
