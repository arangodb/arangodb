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

#include "ApplicationServer.h"

#include <iostream>

#ifdef TRI_HAVE_POSIX_PWD_GRP
#include <pwd.h>
#include <grp.h>
#endif

#include "ApplicationServer/ApplicationFeature.h"
#include "Basics/ConditionLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/Logger.h"
#include "Basics/RandomGenerator.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

static std::string DeprecatedParameter;

////////////////////////////////////////////////////////////////////////////////
/// @brief Hidden Options
////////////////////////////////////////////////////////////////////////////////

namespace {
std::string const OPTIONS_HIDDEN = "Hidden Options";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Command Line Options
////////////////////////////////////////////////////////////////////////////////

namespace {
std::string const OPTIONS_CMDLINE = "General Options";
}

ApplicationServer::ApplicationServer(std::string const& name,
                                     std::string const& title,
                                     std::string const& version)
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
      _numericUid(0),
      _gid(),
      _numericGid(0),
      _logApplicationName("arangod"),
      _logFacility(""),
      _logFile("+"),
      _logTty("+"),
      _logRequestsFile(""),
      _logPrefix(),
      _logThreadId(false),
      _logLineNumber(false),
      _logLocalTime(false),
      _logContentFilter(),
#ifdef _WIN32
      _randomGenerator(5),
#else
      _randomGenerator(3),
#endif
      _finishedCondition() {
  _logLevel.push_back("info");
}

ApplicationServer::~ApplicationServer() {
  Random::shutdown();

  for (auto& it : _features) {
    delete it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new feature
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::addFeature(ApplicationFeature* feature) {
  _features.push_back(feature);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the system config file
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setSystemConfigFile(std::string const& name) {
  _systemConfigFile = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the name of the user config file
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setUserConfigFile(std::string const& name) {
  _userConfigFile = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of the application
////////////////////////////////////////////////////////////////////////////////

std::string const& ApplicationServer::getName() const { return _name; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the logging
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::setupLogging(bool threaded, bool daemon,
                                     bool backgrounded) {
  Logger::shutdown(false);
  Logger::initialize(threaded);

  std::string severity("human");

  if (_options.has("log.thread")) {
    _logThreadId = true;
  }

  if (_options.has("log.line-number")) {
    _logLineNumber = true;
  }

  if (_options.has("log.use-local-time")) {
    _logLocalTime = true;
  }

  if (_options.has("log.performance")) {
    severity += ",performance";
  }

  Logger::setUseLocalTime(_logLocalTime);
  Logger::setShowLineNumber(_logLineNumber);
  Logger::setOutputPrefix(_logPrefix);
  Logger::setShowThreadIdentifier(_logThreadId);

  char const* contentFilter = nullptr;

  if (_options.has("log.content-filter")) {
    contentFilter = _logContentFilter.c_str();
  }

  std::vector<std::string> levels;
  std::vector<std::string> outputs;

  // map deprecated option "log.requests-files" to "log.output"
  if (!_logRequestsFile.empty()) {
    std::string const& filename = _logRequestsFile;
    std::string definition;

    if (filename == "+" || filename == "-") {
      definition = filename;
    } else if (daemon) {
      definition = "file://" + filename + ".daemon";
    } else {
      definition = "file://" + filename;
    }

    levels.push_back("requests=info");
    outputs.push_back("requests=" + definition);
  }

  // map "log.file" to "log.output"
  if (!_logFile.empty()) {
    std::string const& filename = _logFile;
    std::string definition;

    if (filename == "+" || filename == "-") {
      definition = filename;
    } else if (daemon) {
      definition = "file://" + filename + ".daemon";
    } else {
      definition = "file://" + filename;
    }

    outputs.push_back(definition);
  }

  // additional log file in case of tty
  bool ttyLogger = false;

  if (!backgrounded && isatty(STDIN_FILENO) != 0 && !_logTty.empty()) {
    bool ttyOut = (_logTty == "+" || _logTty == "-");

    if (!ttyOut) {
      LOG(ERR) << "'log.tty' must either be '+' or '-', ignoring value '"
               << _logTty << "'";
    } else {
      bool regularOut = false;

      for (auto definition : _logOutput) {
        regularOut = regularOut || definition == "+" || definition == "-";
      }

      for (auto definition : outputs) {
        regularOut = regularOut || definition == "+" || definition == "-";
      }

      if (!regularOut) {
        outputs.push_back(_logTty);
        ttyLogger = true;
      }
    }
  }

  // create all output definitions
  levels.insert(levels.end(), _logLevel.begin(), _logLevel.end());
  outputs.insert(outputs.end(), _logOutput.begin(), _logOutput.end());

  Logger::setLogLevel(levels);

  for (auto definition : outputs) {
    Logger::addAppender(definition, !ttyLogger, _logContentFilter);
  }

// TODO(FC) fixme
#if 0
#ifdef TRI_ENABLE_SYSLOG
  if (!_logFacility.empty()) {
    TRI_CreateLogAppenderSyslog(_logApplicationName.c_str(),
                                _logFacility.c_str(), contentFilter,
                                TRI_LOG_SEVERITY_UNKNOWN, false);
  }
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the command line options
////////////////////////////////////////////////////////////////////////////////

ProgramOptions& ApplicationServer::programOptions() { return _options; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the command line arguments
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ApplicationServer::programArguments() {
  return _arguments;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the arguments with empty options description
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::parse(int argc, char* argv[]) {
  std::map<std::string, ProgramOptionsDescription> none;

  return parse(argc, argv, none);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the arguments
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::parse(
    int argc, char* argv[],
    std::map<std::string, ProgramOptionsDescription> opts) {
  // .............................................................................
  // setup the options
  // .............................................................................

  setupOptions(opts);

  for (std::vector<ApplicationFeature*>::iterator i = _features.begin();
       i != _features.end(); ++i) {
    (*i)->setupOptions(opts);
  }

  // construct options description
  for (std::map<std::string, ProgramOptionsDescription>::iterator i =
           opts.begin();
       i != opts.end(); ++i) {
    std::string name = i->first;

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

  if (!ok) {
    LOG(ERR) << "cannot parse command line: " << _options.lastError().c_str();
    return false;
  }

  // check for version request
  if (_options.has("version")) {
    std::cout << _version << std::endl;
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // check for help
  std::set<std::string> help = _options.needHelp("help");

  if (!help.empty()) {
    // output help, but do not yet exit (we'll exit a little later so we can
    // also
    // check the specified configuration for errors)
    std::cout << argv[0] << " " << _title << std::endl
              << std::endl
              << _description.usage(help) << std::endl;
  }

  Logger::setLogLevel(_logLevel);

  // .............................................................................
  // check configuration file
  // .............................................................................

  ok = readConfigurationFile();

  if (!ok) {
    return false;
  }

  // .............................................................................
  // parse phase 1
  // .............................................................................

  for (std::vector<ApplicationFeature*>::iterator i = _features.begin();
       i != _features.end(); ++i) {
    ok = (*i)->afterOptionParsing(_options);

    if (!ok) {
      return false;
    }
  }

  // exit here if --help was specified.
  // this allows us to use --help to run a configuration file check, too, and
  // report errors to the user

  if (!help.empty()) {
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // .............................................................................
  // UID and GID
  // .............................................................................

  extractPrivileges();
  dropPrivilegesPermanently();

  // .............................................................................
  // setup logging
  // .............................................................................

  setupLogging(false, false, false);

  // .............................................................................
  // select random generate
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
      default: { break; }
    }
  } catch (...) {
    LOG(FATAL) << "cannot select random generator, giving up";
    FATAL_ERROR_EXIT();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server, step one
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::prepare() {
  // prepare all features - why reverse?

  // reason: features might depend on each other. by creating them in reverse
  // order and shutting them
  // down in forward order, we ensure that the features created last are
  // destroyed first, i.e.: LIFO
  for (std::vector<ApplicationFeature*>::reverse_iterator i =
           _features.rbegin();
       i != _features.rend(); ++i) {
    ApplicationFeature* feature = *i;

    LOG(DEBUG) << "preparing server feature '" << feature->getName().c_str()
               << "'";

    bool ok = feature->prepare();

    if (!ok) {
      LOG(FATAL) << "failed to prepare server feature '"
                 << feature->getName().c_str() << "'";
      FATAL_ERROR_EXIT();
    }

    LOG(TRACE) << "prepared server feature '" << feature->getName().c_str()
               << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server, step two
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::prepare2() {
  // prepare all features
  for (std::vector<ApplicationFeature*>::reverse_iterator i =
           _features.rbegin();
       i != _features.rend(); ++i) {
    ApplicationFeature* feature = *i;

    LOG(DEBUG) << "preparing(2) server feature '" << feature->getName().c_str()
               << "'";

    bool ok = feature->prepare2();

    if (!ok) {
      LOG(FATAL) << "failed to prepare(2) server feature '"
                 << feature->getName().c_str() << "'";
      FATAL_ERROR_EXIT();
    }

    LOG(TRACE) << "prepared(2) server feature '" << feature->getName().c_str()
               << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the server
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::start() {
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);
#endif

  // start all startable features
  for (std::vector<ApplicationFeature*>::iterator i = _features.begin();
       i != _features.end(); ++i) {
    ApplicationFeature* feature = *i;

    bool ok = feature->start();

    if (!ok) {
      LOG(FATAL) << "failed to start server feature '"
                 << feature->getName().c_str() << "'";
      FATAL_ERROR_EXIT();
    }

    LOG(DEBUG) << "started server feature '" << feature->getName().c_str()
               << "'";
  }

  // now open all features
  for (std::vector<ApplicationFeature*>::reverse_iterator i =
           _features.rbegin();
       i != _features.rend(); ++i) {
    ApplicationFeature* feature = *i;

    LOG(DEBUG) << "opening server feature '" << feature->getName().c_str()
               << "'";

    bool ok = feature->open();

    if (!ok) {
      LOG(FATAL) << "failed to open server feature '"
                 << feature->getName().c_str() << "'";
      FATAL_ERROR_EXIT();
    }

    LOG(TRACE) << "opened server feature '" << feature->getName().c_str()
               << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for shutdown
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::wait() {
  // wait until we receive a stop signal
  while (_stopping == 0) {
    // check the parent and wait for a second
    if (!checkParent()) {
      break;
    }

    CONDITION_LOCKER(locker, _finishedCondition);
    locker.wait((uint64_t)(1000 * 1000));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::beginShutdown() {
  if (_stopping != 0) {
    return;
  }
  _stopping = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops everything
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::stop() {
  beginShutdown();

  CONDITION_LOCKER(locker, _finishedCondition);
  locker.signal();

  // close all features
  for (std::vector<ApplicationFeature*>::iterator i = _features.begin();
       i != _features.end(); ++i) {
    ApplicationFeature* feature = *i;

    feature->close();

    LOG(TRACE) << "closed server feature '" << feature->getName().c_str()
               << "'";
  }

  // stop all features
  for (std::vector<ApplicationFeature*>::reverse_iterator i =
           _features.rbegin();
       i != _features.rend(); ++i) {
    ApplicationFeature* feature = *i;

    LOG(DEBUG) << "shutting down server feature '" << feature->getName().c_str()
               << "'";
    feature->stop();
    LOG(TRACE) << "shut down server feature '" << feature->getName().c_str()
               << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the privileges to use
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::extractPrivileges() {
#ifdef TRI_HAVE_SETGID

  if (_gid.empty()) {
    _numericGid = getgid();
  } else {
    int gidNumber = TRI_Int32String(_gid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR && gidNumber >= 0) {
#ifdef TRI_HAVE_GETGRGID
      group* g = getgrgid(gidNumber);

      if (g == 0) {
        LOG(FATAL) << "unknown numeric gid '" << _gid.c_str() << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef TRI_HAVE_GETGRNAM
      std::string name = _gid;
      group* g = getgrnam(name.c_str());

      if (g != 0) {
        gidNumber = g->gr_gid;
      } else {
        LOG(FATAL) << "cannot convert groupname '" << _gid.c_str()
                   << "' to numeric gid";
        FATAL_ERROR_EXIT();
      }
#else
      LOG(FATAL) << "cannot convert groupname '" << _gid.c_str()
                 << "' to numeric gid";
      FATAL_ERROR_EXIT();
#endif
    }

    _numericGid = gidNumber;
  }

#endif

#ifdef TRI_HAVE_SETUID

  if (_uid.empty()) {
    _numericUid = getuid();
  } else {
    int uidNumber = TRI_Int32String(_uid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR) {
#ifdef TRI_HAVE_GETPWUID
      passwd* p = getpwuid(uidNumber);

      if (p == 0) {
        LOG(FATAL) << "unknown numeric uid '" << _uid.c_str() << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef TRI_HAVE_GETPWNAM
      std::string name = _uid;
      passwd* p = getpwnam(name.c_str());

      if (p != 0) {
        uidNumber = p->pw_uid;
      } else {
        LOG(FATAL) << "cannot convert username '" << _uid.c_str()
                   << "' to numeric uid";
        FATAL_ERROR_EXIT();
      }
#else
      LOG(FATAL) << "cannot convert username '" << _uid.c_str()
                 << "' to numeric uid";
      FATAL_ERROR_EXIT();
#endif
    }

    _numericUid = uidNumber;
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the privileges permanently
////////////////////////////////////////////////////////////////////////////////

void ApplicationServer::dropPrivilegesPermanently() {
// clear all supplementary groups
#if defined(TRI_HAVE_INITGROUPS) && defined(TRI_HAVE_SETGID) && \
    defined(TRI_HAVE_SETUID)

  if (!_gid.empty() && !_uid.empty()) {
    struct passwd* pwent = getpwuid(_numericUid);

    if (pwent != nullptr) {
      initgroups(pwent->pw_name, _numericGid);
    }
  }

#endif

// first GID
#ifdef TRI_HAVE_SETGID

  if (!_gid.empty()) {
    LOG(DEBUG) << "permanently changing the gid to " << _numericGid;

    int res = setgid(_numericGid);

    if (res != 0) {
      LOG(FATAL) << "cannot set gid " << _numericGid << ": " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }

#endif

// then UID (because we are dropping)
#ifdef TRI_HAVE_SETUID

  if (!_uid.empty()) {
    LOG(DEBUG) << "permanently changing the uid to " << _numericUid;

    int res = setuid(_numericUid);

    if (res != 0) {
      LOG(FATAL) << "cannot set uid '" << _uid.c_str()
                 << "': " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }

#endif
}

void ApplicationServer::setupOptions(
    std::map<std::string, ProgramOptionsDescription>& options) {
  // .............................................................................
  // command line options
  // .............................................................................

  options["General Options:help-default"]("version,v",
                                          "print version string and exit")(
      "help,h", "produce a usage message and exit")(
      "configuration,c", &_configFile, "read configuration file");

#if defined(TRI_HAVE_SETUID) || defined(TRI_HAVE_SETGID)

  options["General Options:help-admin"]
#ifdef TRI_HAVE_GETPPID
      ("exit-on-parent-death", &_exitOnParentDeath, "exit if parent dies")
#endif
          ("watch-process", &_watchParent,
           "exit if process with given PID dies");

#endif

  // .............................................................................
  // logger options
  // .............................................................................

  // clang-format off

  options["Logging Options:help-default:help-log"]
    ("log.file", &_logFile, "log to file")
    ("log.level,l", &_logLevel, "log level")
    ("log.output,o", &_logOutput, "log output")
  ;

  options["Logging Options:help-log"]
    ("log.application", &_logApplicationName, "application name for syslog")
    ("log.facility", &_logFacility, "facility name for syslog (OS dependent)")
    ("log.content-filter", &_logContentFilter,
       "only log message containing the specified string (case-sensitive)")
    ("log.line-number", "always log file and line number")
    ("log.performance", "log performance indicators")
    ("log.prefix", &_logPrefix, "prefix log")
    ("log.thread", "log the thread identifier")
    ("log.use-local-time", "use local dates and times in log messages")
    ("log.tty", &_logTty, "additional log file if started on tty")
  ;

  options["Hidden Options"]
    ("log", &_logLevel, "log level")
    ("log.requests-file", &_logRequestsFile, "log requests to file (deprecated)")
    ("log.syslog", &DeprecatedParameter, "use syslog facility (deprecated)")
    ("log.hostname", &DeprecatedParameter, "host name for syslog")
    ("log.severity", &DeprecatedParameter, "log severities")
#ifdef TRI_HAVE_SETUID
    ("uid", &_uid, "switch to user-id after reading config files")
#endif
#ifdef TRI_HAVE_SETGID
    ("gid", &_gid, "switch to group-id after reading config files")
#endif
  ;

  // clang-format on

  // .............................................................................
  // application server options
  // .............................................................................

  options["Server Options:help-admin"](
      "random.generator", &_randomGenerator,
      "1 = mersenne, 2 = random, 3 = urandom, 4 = combined")
#ifdef TRI_HAVE_SETUID
      ("server.uid", &_uid, "switch to user-id after reading config files")
#endif
#ifdef TRI_HAVE_SETGID
          ("server.gid", &_gid, "switch to group-id after reading config files")
#endif
              ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the parent is still alive
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::checkParent() {
// check our parent, if it died given up
#ifdef TRI_HAVE_GETPPID
  if (_exitOnParentDeath && getppid() == 1) {
    LOG(INFO) << "parent has died";
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
      LOG(INFO) << "parent " << _watchParent << " has died";
      return false;
    }
  }
#endif

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads the configuration files
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::readConfigurationFile() {
  // something has been specified on the command line regarding configuration
  // file
  if (!_configFile.empty()) {
    // do not use init files
    if (StringUtils::tolower(_configFile) == std::string("none")) {
      LOG(INFO) << "using no init file at all";
      return true;
    }

    LOG(DEBUG) << "using init file '" << _configFile.c_str() << "'";

    bool ok = _options.parse(_descriptionFile, _configFile);

    // Observe that this is treated as an error - the configuration file exists
    // but for some reason can not be parsed. Best to report an error.

    if (!ok) {
      LOG(ERR) << "cannot parse config file '" << _configFile.c_str()
               << "': " << _options.lastError().c_str();
    }

    return ok;
  } else {
    LOG(DEBUG) << "no init file has been specified";
  }

  // nothing has been specified on the command line regarding the user's
  // configuration file
  if (!_userConfigFile.empty()) {
    // .........................................................................
    // first attempt to obtain a default configuration file from the users home
    // directory
    // .........................................................................

    // .........................................................................
    // Under windows there is no 'HOME' directory as such so getenv("HOME")
    // may return nullptr -- which it does under windows
    // A safer approach below
    // .........................................................................

    std::string homeDir = FileUtils::homeDirectory();

    if (!homeDir.empty()) {
      if (homeDir[homeDir.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
        homeDir += TRI_DIR_SEPARATOR_CHAR + _userConfigFile;
      } else {
        homeDir += _userConfigFile;
      }

      // check and see if file exists
      if (FileUtils::exists(homeDir)) {
        LOG(DEBUG) << "using user init file '" << homeDir.c_str() << "'";

        bool ok = _options.parse(_descriptionFile, homeDir);

        // Observe that this is treated as an error - the configuration file
        // exists
        // but for some reason can not be parsed. Best to report an error.

        if (!ok) {
          LOG(ERR) << "cannot parse config file '" << homeDir.c_str()
                   << "': " << _options.lastError().c_str();
        }

        return ok;
      } else {
        LOG(DEBUG) << "no user init file '" << homeDir.c_str() << "' found";
      }
    } else {
      LOG(DEBUG) << "no home directory found";
    }
  }

  // try the configuration file in the system directory - if there is one
  if (!_systemConfigFile.empty()) {
    // Please note that the system directory changes depending on
    // where the user installed the application server.

    char* d = TRI_LocateConfigDirectory();

    if (d != 0) {
      std::string sysDir = std::string(d);

      if ((sysDir.length() > 0) &&
          (sysDir[sysDir.length() - 1] != TRI_DIR_SEPARATOR_CHAR)) {
        sysDir += TRI_DIR_SEPARATOR_CHAR;
      }
      sysDir += _systemConfigFile;

      std::string localSysDir = sysDir + ".local";

      TRI_FreeString(TRI_CORE_MEM_ZONE, d);

      // check and see if a local override file exists
      if (FileUtils::exists(localSysDir)) {
        LOG(DEBUG) << "using init override file '" << localSysDir.c_str()
                   << "'";

        bool ok = _options.parse(_descriptionFile, localSysDir);

        // Observe that this is treated as an error - the configuration file
        // exists
        // but for some reason can not be parsed. Best to report an error.
        if (!ok) {
          LOG(ERR) << "cannot parse config file '" << localSysDir.c_str()
                   << "': " << _options.lastError().c_str();
          return ok;
        }
      } else {
        LOG(DEBUG) << "no system init override file '" << localSysDir.c_str()
                   << "' found";
      }

      // check and see if file exists
      if (FileUtils::exists(sysDir)) {
        LOG(DEBUG) << "using init file '" << sysDir.c_str() << "'";

        bool ok = _options.parse(_descriptionFile, sysDir);

        // Observe that this is treated as an error - the configuration file
        // exists
        // but for some reason can not be parsed. Best to report an error.
        if (!ok) {
          LOG(ERR) << "cannot parse config file '" << sysDir.c_str()
                   << "': " << _options.lastError().c_str();
        }

        return ok;
      } else {
        LOG(INFO) << "no system init file '" << sysDir.c_str() << "' found";
      }
    } else {
      LOG(DEBUG) << "no system init file specified";
    }
  }

  return true;
}
