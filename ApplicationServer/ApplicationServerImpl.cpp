////////////////////////////////////////////////////////////////////////////////
/// @brief application server implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationServerImpl.h"

#include "build.h"

#include <Basics/FileUtils.h>
#include <Basics/Random.h>
#include <Basics/StringUtils.h>
#include <Basics/delete_object.h>
#include <BasicsC/conversions.h>
#include <Logger/Logger.h>

#include "ApplicationServer/ApplicationFeature.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationServerImpl::ApplicationServerImpl (string const& title, string const& version)
      : _exitOnParentDeath(false),
        _watchParent(0),
        title(title),
        version(version),
        initFile(),
        userConfigFile(),
        systemConfigFile(),
        systemConfigPath(),
        uid(),
        gid(),
        logApplicationName("triagens"),
        logHostName("-"),
        logFacility("-"),
        logLevel("info"),
        logFormat(),
        logSeverity("human"),
        logFile("+"),
        logPrefix(),
        logSyslog(),
        logThreadId(false),
        logLineNumber(false),
        randomGenerator(3) {
    }



    ApplicationServerImpl::~ApplicationServerImpl () {
      for_each(features.begin(), features.end(), DeleteObject());
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ApplicationServerImpl::addFeature (ApplicationFeature* feature) {
      features.push_back(feature);
    }



    bool ApplicationServerImpl::parse (int argc,
                                       char* argv[],
                                       map<string, ProgramOptionsDescription> opts) {

      // setup the options
      setupOptions(opts);

      for (vector<ApplicationFeature*>::iterator i = features.begin();  i != features.end();  ++i) {
        (*i)->setupOptions(opts);
      }

      // construct options description
      for (map<string, ProgramOptionsDescription>::iterator i = opts.begin();  i != opts.end();  ++i) {
        string name = i->first;

        ProgramOptionsDescription sectionDescription = i->second;
        sectionDescription.setName(name);

        // and add to the global options
        description(sectionDescription, name == OPTIONS_HIDDEN);

        if (name != OPTIONS_CMDLINE) {
          descriptionFile(sectionDescription, name == OPTIONS_HIDDEN);
        }
      }

      description.arguments(&arguments);

      // parse command line
      bool ok = options.parse(description, argc, argv);

      if (! ok) {
        cout << options.lastError() << endl;
        return false;
      }


      // check for help
      set<string> help = options.needHelp("help");

      if (! help.empty()) {
        cout << argv[0] << " " << title << "\n\n" << description.usage(help) << endl;
        exit(EXIT_SUCCESS);
      }


      // setup logging
      storeLoggingPrivileges();
      setupLogging();


      // check for version request
      if (options.has("version")) {
        cout << version << endl;
        exit(EXIT_SUCCESS);
      }


      // check for phase 1 in subclasses
      ok = parsePhase1();

      if (! ok) {
        return false;
      }

      for (vector<ApplicationFeature*>::iterator i = features.begin();  i != features.end();  ++i) {
        ok = (*i)->parsePhase1(options);

        if (! ok) {
          return false;
        }
      }

      // check configuration file
      ok = readConfigurationFile();

      if (! ok) {
        return false;
      }


      // re-set logging using the additional config file entries
      setupLogging();


      // check for phase 2 in subclasses
      ok = parsePhase2();

      if (! ok) {
        return false;
      }

      for (vector<ApplicationFeature*>::iterator i = features.begin();  i != features.end();  ++i) {
        ok = (*i)->parsePhase2(options);

        if (! ok) {
          return false;
        }
      }


      // no drop all privilegs
      dropPriviliges();


      // done
      return true;
    }



    void ApplicationServerImpl::start () {
      LOGGER_DEBUG << "VOC version " << TRIAGENS_VERSION;

      sigset_t all;
      sigfillset(&all);
      pthread_sigmask(SIG_SETMASK, &all, 0);
    }



    void ApplicationServerImpl::wait () {
    }



    void ApplicationServerImpl::beginShutdown () {
      Random::shutdown();
    }



    void ApplicationServerImpl::shutdown () {
      beginShutdown();
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    void ApplicationServerImpl::setupOptions (map<string, ProgramOptionsDescription>& options) {

      // .............................................................................
      // command line options
      // .............................................................................

      options[OPTIONS_CMDLINE]
        ("version,v", "print version string and exit")
        ("help,h", "produce a usage message and exit")
        ("configuration,c", &initFile, "read configuration file")
      ;

#if defined(TRI_HAVE_SETUID) || defined(TRI_HAVE_SETGID)

      options[OPTIONS_CMDLINE + ":help-extended"]
#ifdef TRI_HAVE_SETUID
        ("uid", &uid, "switch to user-id after reading config files")
#endif
#ifdef TRI_HAVE_SETGID
        ("gid", &gid, "switch to group-id after reading config files")
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
        ("log.level,l", &logLevel, "log level for severity 'human'")
        ("log.file", &logFile, "log to file")
      ;

      options[OPTIONS_LOGGER + ":help-log"]
        ("log.thread", "log the thread identifier for severity 'human'")
        ("log.severity", &logSeverity, "log severities")
        ("log.format", &logFormat, "log format")
        ("log.application", &logApplicationName, "application name")
        ("log.facility", &logFacility, "facility name")
        ("log.hostname", &logHostName, "host name")
        ("log.prefix", &logPrefix, "prefix log")
        ("log.syslog", &logSyslog, "use syslog facility")
        ("log.line-number", "always log file and line number")
      ;

      options[OPTIONS_HIDDEN]
        ("log", &logLevel, "log level for severity 'human'")
      ;

      // .............................................................................
      // application server options
      // .............................................................................

      options[OPTIONS_SERVER + ":help-extended"]
        ("random.no-seed", "do not seed the random generator")
        ("random.generator", &randomGenerator, "1 = mersenne, 2 = random, 3 = urandom, 4 = combined")
      ;
    }



    bool ApplicationServerImpl::parsePhase1 () {
      return true;
    }



    bool ApplicationServerImpl::parsePhase2 () {
      if (! options.has("random.no-seed")) {
        Random::seed();
      }

      try {
        switch (randomGenerator) {
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

      return true;
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // setup logging
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    void ApplicationServerImpl::setupLogging () {
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

      Logger::setApplicationName(logApplicationName);
      Logger::setHostName(logHostName);
      Logger::setFacility(logFacility);

      if (! logFormat.empty()) {
        Logger::setLogFormat(logFormat);
      }

      if (options.has("log.thread")) {
        logThreadId = true;
      }

      if (options.has("log.line-number")) {
        logLineNumber = true;
      }

      TRI_SetLineNumberLogging(logLineNumber);
      TRI_SetLogLevelLogging(logLevel);
      TRI_SetLogSeverityLogging(logSeverity);
      TRI_SetPrefixLogging(logPrefix);
      TRI_SetThreadIdentifierLogging(logThreadId);

      TRI_InitialiseLogging(threaded);

      TRI_CreateLogAppenderFile(logFile);
      TRI_CreateLogAppenderSyslog(logPrefix, logSyslog);

#ifdef TRI_HAVE_SETGID
      setegid(gid);
#endif

#ifdef TRI_HAVE_SETUID
      seteuid(uid);
#endif

    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // read configuration file
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    bool ApplicationServerImpl::readConfigurationFile () {

      // something has been specified on the command line regarding configuration file
      if (! initFile.empty()) {

        // do not use init files
        if (StringUtils::tolower(initFile) == string("none")) {
          LOGGER_INFO << "using no init file at all";
          return true;
        }

        LOGGER_INFO << "using init file '" << initFile << "'";

        bool ok = options.parse(descriptionFile, initFile);

        // observe that this is treated as an error - the configuration file exists
        // but for some reason can not be parsed. Best to report an error
        if (! ok) {
          cout << options.lastError() << endl;
        }

        return ok;
      }
      else {
        LOGGER_DEBUG << "no init file has been specified";
      }

      // nothing has been specified on the command line regarding configuration file
      if (! userConfigFile.empty()) {

        // first attempt to obtain a default configuration file from the users home directory
        string homeDir = string(getenv("HOME"));

        if (! homeDir.empty()) {
          if (homeDir[homeDir.size() - 1] != '/') {
            homeDir += "/" + userConfigFile;
          }
          else {
            homeDir += userConfigFile;
          }

          // check and see if file exists
          if (FileUtils::exists(homeDir)) {
            LOGGER_INFO << "using user init file '" << homeDir << "'";

            bool ok = options.parse(descriptionFile, homeDir);

            // observe that this is treated as an error - the configuration file exists
            // but for some reason can not be parsed. Best to report an error
            if (! ok) {
              cout << options.lastError() << endl;
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

      if (systemConfigPath.empty()) {

#ifdef _SYSCONFDIR_

        // try the configuration file in the system directory - if there is one

        // Please note that the system directory changes depending on
        // where the user installed the application server.

        if (! systemConfigFile.empty()) {
          string sysDir = string(_SYSCONFDIR_);
          
          if (! sysDir.empty()) {
            if (sysDir[sysDir.size() - 1] != '/') {
              sysDir += "/" + systemConfigFile;
            }
            else {
              sysDir += systemConfigFile;
            }
            
            // check and see if file exists
            if (FileUtils::exists(sysDir)) {
              LOGGER_INFO << "using init file '" << sysDir << "'";
              
              bool ok = options.parse(descriptionFile, sysDir);
              
              // observe that this is treated as an error - the configuration file exists
              // but for some reason can not be parsed. Best to report an error
              if (! ok) {
                cout << options.lastError() << endl;
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
        if (! systemConfigFile.empty()) {
          string sysDir = systemConfigPath + "/" + systemConfigFile;
            
          // check and see if file exists
          if (FileUtils::exists(sysDir)) {
            LOGGER_INFO << "using init file '" << sysDir << "'";
              
            bool ok = options.parse(descriptionFile, sysDir);
              
            // observe that this is treated as an error - the configuration file exists
            // but for some reason can not be parsed. Best to report an error
            if (! ok) {
              cout << options.lastError() << endl;
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

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // drop privileges
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    void ApplicationServerImpl::storeLoggingPrivileges () {
#ifdef TRI_HAVE_SETGID
      _loggingGid = getegid();
#endif

#ifdef TRI_HAVE_SETUID
      _loggingUid = geteuid();
#endif

    }


    void ApplicationServerImpl::dropPriviliges () {

#ifdef TRI_HAVE_SETGID

      if (! gid.empty()) {
        LOGGER_TRACE << "trying to switch to group '" << gid << "'";

        int gidNumber = TRI_Int32String(gid.c_str());

        if (TRI_errno() == TRI_ERROR_NO_ERROR) {
          LOGGER_TRACE << "trying to switch to numeric gid '" << gidNumber << "' for '" << gid << "'";

#ifdef TRI_HAVE_GETGRGID
          group* g = getgrgid(gidNumber);

          if (g == 0) {
            LOGGER_FATAL << "unknown numeric gid '" << gid << "'";
            TRI_ShutdownLogging();
            exit(EXIT_FAILURE);
          }
#endif
        }
        else {
#ifdef TRI_HAVE_GETGRNAM
          string name = gid;
          group* g = getgrnam(name.c_str());

          if (g != 0) {
            gidNumber = g->gr_gid;
            LOGGER_TRACE << "trying to switch to numeric gid '" << gidNumber << "'";
          }
          else {
            LOGGER_FATAL << "cannot convert groupname '" << gid << "' to numeric gid";
            TRI_ShutdownLogging();
            exit(EXIT_FAILURE);
          }
#else
          LOGGER_FATAL << "cannot convert groupname '" << gid << "' to numeric gid";
          TRI_ShutdownLogging();
          exit(EXIT_FAILURE);
#endif
        }

        LOGGER_INFO << "changing gid to '" << gidNumber << "'";

        int res = setegid(gidNumber);

        if (res != 0) {
          LOGGER_FATAL << "cannot set gid '" << gid << "', because " << strerror(errno);
          TRI_ShutdownLogging();
          exit(EXIT_FAILURE);
        }
      }

#endif

#ifdef TRI_HAVE_SETUID

      if (! uid.empty()) {
        LOGGER_TRACE << "trying to switch to user '" << uid << "'";

        int uidNumber = TRI_Int32String(uid.c_str());

        if (TRI_errno() == TRI_ERROR_NO_ERROR) {
          LOGGER_TRACE << "trying to switch to numeric uid '" << uidNumber << "' for '" << uid << "'";

#ifdef TRI_HAVE_GETPWUID
          passwd* p = getpwuid(uidNumber);

          if (p == 0) {
            LOGGER_FATAL << "unknown numeric uid '" << uid << "'";
            TRI_ShutdownLogging();
            exit(EXIT_FAILURE);
          }
#endif
        }
        else {
#ifdef TRI_HAVE_GETPWNAM
          string name = uid;
          passwd* p = getpwnam(name.c_str());

          if (p != 0) {
            uidNumber = p->pw_uid;
            LOGGER_TRACE << "trying to switch to numeric uid '" << uidNumber << "'";
          }
          else {
            LOGGER_FATAL << "cannot convert username '" << uid << "' to numeric uid";
            TRI_ShutdownLogging();
            exit(EXIT_FAILURE);
          }
#else
          LOGGER_FATAL << "cannot convert username '" << uid << "' to numeric uid";
          TRI_ShutdownLogging();
          exit(EXIT_FAILURE);
#endif
        }

        LOGGER_INFO << "changing uid to '" << uidNumber << "'";

        int res = seteuid(uidNumber);

        if (res != 0) {
          LOGGER_FATAL << "cannot set uid '" << uid << "', because " << strerror(errno);
          TRI_ShutdownLogging();
          exit(EXIT_FAILURE);
        }
      }

#endif
    }
  }
}
