////////////////////////////////////////////////////////////////////////////////
/// @brief rest server template
///
/// @file
/// This file contains the description of the rest model used.
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestServer.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fstream>

#include <Basics/FileUtils.h>
#include <Basics/Initialise.h>
#include <Basics/Logger.h>
#include <Basics/safe_cast.h>
#include <Rest/ApplicationHttpServer.h>
#include <Rest/HttpHandlerFactory.h>
#include <Rest/Initialise.h>
#include <Rest/LibraryLoader.h>
#include <Rest/RestModel.h>
#include <Rest/RestServer.h>
#include <Rest/RestServerOptions.h>

#include <Rest/ApplicationServerDispatcher.h>

#include <Rest/ApplicationHttpsServer.h>

#include "ApplicationServer/ApplicationServerImpl.h"

using namespace std;
using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    int RestServer::main (RestServer* server) {

      // load the model
      bool status = server->loadModel();

      if (! status) {
        return 1;
      }

      // build the application server
      server->buildApplicationServer();

      // parse the arguments
      status = server->checkOptions();

      if (! status) {
        return 1;
      }

      // and start up
      if (server->_model == 0) {
        LOGGER_FATAL << "no " << server->_options->modelDescription() << " has been defined";
        exit(EXIT_FAILURE);
      }

      return server->start();
    }



    int RestServer::main (RestServer* server, RestModel* model) {

      // load the model
      server->useModel(model);

      // build the application server
      server->buildApplicationServer();

      // parse the arguments
      bool status = server->checkOptions();

      if (! status) {
        return 1;
      }

      // and start up
      if (server->_model == 0) {
        LOGGER_FATAL << "no " << server->_options->modelDescription() << " has been defined";
        exit(EXIT_FAILURE);
      }

      return server->start();
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    RestServer::RestServer (RestServerOptions* options, int argc, char** argv)
      : _model(0),
        _options(options),
        _httpServer(0),
        _httpsServer(0),
        _nrDispatcherThreads(1),
        _argc(argc),
        _argv(argv),
        _programName(argv[0]),
        _shortDescription("") {
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    bool RestServer::loadModel () {

      // try to catch a log level as first argument
      TRI_SetLogLevelLogging("info");

      if (_argc >= 3 && (strcmp(_argv[1], "--log.level") == 0 || strcmp(_argv[1], "-l") == 0)) {
        TRI_SetLogLevelLogging(_argv[2]);
      }

      // try to read in internal model
      LibraryLoader::processSelf(_options->modelName(), _options->modelLoader());

      // check if we found an internal model
      bool internal = _options->model() != 0;

      LOGGER_TRACE << "found " << (internal ? "" : "no ") << "internal model";

      // create a shot description
      if (! internal) {
        string d = _options->modelDescription();

        if (! d.empty()) {
          _shortDescription += d + " ";
        }
      }

      {
        string d = _options->serverDescription();

        if (! d.empty()) {
          _shortDescription += "- " + d;
        }
      }

      // loads the model
      if (! internal) {
        if (_argc < 2) {
          cout << "usage: " << _programName << " " << _options->modelDescription() << " [<options>]" << endl;
          return false;
        }

        if (strcasecmp(_argv[1], "--help") == 0) {
          cout << "usage: " << _programName << " " << _options->modelDescription() << " [<options>]" << endl;
          return false;
        }

        LibraryLoader::processFile(_argv[1], _options->modelName(), _options->modelLoader());

        char** nargv = new char* [_argc];
        memcpy(nargv + 1, _argv + 2, sizeof(char*) * (_argc - 2));

        nargv[0] = _argv[0];

        --_argc;
        _argv = nargv;
      }

      // check that we have a model
      _model = _options->model();

      if (_model != 0) {
        return true;
      }
      else {
        cout << "no " << _options->modelDescription() << " found" << endl;
        return false;
      }

      return false;
    }



    void RestServer::useModel (RestModel* model) {
      _model = model;

      // create a shot description
      string d = _options->serverDescription();

      if (! d.empty()) {
        _shortDescription += "- " + d;
      }
    }



    bool RestServer::checkOptions () {

      // additional options from model
      map<string, ProgramOptionsDescription> additional;

      RestModel::Extensions* extensions = 0;

      if (_model != 0) {
        extensions = _model->getExtensions();
      }

      if (extensions != 0) {
        additional = extensions->options();
      }


      // .............................................................................
      // server options
      // .............................................................................

      additional[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
        ("dispatcher.threads", &_nrDispatcherThreads, "number of dispatcher threads")
      ;


      // .............................................................................
      // command line options
      // .............................................................................

      _workingDirectory = "/var/tmp";

      if (_options->allowDaemonMode()) {
        additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
          ("daemon", "run as daemon")
          ("change-directory", &_workingDirectory, "change into working directory")
          ("pid-file", &_pidFile, "pid-file in daemon mode")
        ;
      }

      if (_options->allowSupervisor()) {
        additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
          ("supervisor", "starts a supervisor")
        ;
      }

      // .............................................................................
      // parse the arguments
      // .............................................................................

      if (! _applicationServer->parse(_argc, _argv, additional)) {
        return false;
      }

      if (_applicationServer->programOptions().has("daemon")) {
        _daemonMode = true;
      }

      if (_applicationServer->programOptions().has("supervisor")) {
        _supervisorMode = true;
      }

      if (_daemonMode) {
        if (_pidFile.empty()) {
          LOGGER_FATAL << "no pid-file defined, but daemon mode requested";
          exit(EXIT_FAILURE);
        }
      }

      return true;
    }



    void RestServer::buildApplicationServer () {
      _applicationServer = ApplicationServerDispatcher::create(_shortDescription, _options->serverVersion(_model));

      _applicationHttpServer = ApplicationHttpServer::create(_applicationServer);
      _applicationHttpServer->showPortOptions(_options->showPortOptions(_model));
      _applicationServer->addFeature(_applicationHttpServer);

      _applicationHttpsServer = ApplicationHttpsServer::create(_applicationServer);
      _applicationHttpsServer->showPortOptions(_options->showPortOptions(_model));
      _applicationServer->addFeature(_applicationHttpsServer);

      _applicationServer->allowMultiScheduler(_options->allowMultiScheduler(_model));

      RestModel::Extensions* extensions = _model->getExtensions();

      if (extensions != 0) {
        extensions->buildApplicationServer(_applicationServer);
      }
    }



    void RestServer::prepareServer () {
      AnyServer::prepareServer();

      RestModel::Extensions* extensions = _model->getExtensions();

      // check arguments
      if (extensions != 0) {
        extensions->checkArguments(_applicationServer);
        extensions->prepareServer(_applicationServer);
      }

      // standard setup
      _applicationServer->buildScheduler();
      _applicationServer->buildSchedulerReporter();
      _applicationServer->buildControlCHandler();

      safe_cast<ApplicationServerDispatcher*>(_applicationServer)->buildDispatcher();
      safe_cast<ApplicationServerDispatcher*>(_applicationServer)->buildDispatcherReporter();

      _model->buildQueues(this, safe_cast<ApplicationServerDispatcher*>(_applicationServer));

      HttpHandlerFactory* httpHandlerFactory = 0;

      HttpHandlerFactory* httpsHandlerFactory = 0;

      if (extensions == 0) {
        httpHandlerFactory = new HttpHandlerFactory();
        httpsHandlerFactory = new HttpHandlerFactory();
      }
      else {
        httpHandlerFactory = extensions->buildHandlerFactory(_model);
        httpsHandlerFactory = extensions->buildHandlerFactory(_model);
      }

      if (extensions != 0) {
        _httpServer = extensions->buildHttpServer(_applicationHttpServer, httpHandlerFactory);

        _httpsServer = extensions->buildHttpsServer(_applicationHttpsServer, httpsHandlerFactory);
      }
      else {
        _httpServer = _applicationHttpServer->buildServer(httpHandlerFactory);

        _httpsServer = _applicationHttpsServer->buildServer(httpsHandlerFactory);
      }
    }



    int RestServer::startupServer () {

      // and startup database
      _model->startupServer(_applicationServer);

      // start main event loop and wait
      LOGGER_INFO << "rest server is ready for business";

      _applicationServer->start();
      _applicationServer->wait();

      // and shutdown database
      _model->shutdownServer();

      // that's it
      return 0;
    }
  }
}
