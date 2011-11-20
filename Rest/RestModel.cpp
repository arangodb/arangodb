////////////////////////////////////////////////////////////////////////////////
/// @brief abstract rest model
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

#include "RestModel.h"

#include <Rest/ApplicationHttpServer.h>
#include <Rest/RestHandlerFactory.h>
#include <Rest/RestServer.h>

#include <Rest/ApplicationHttpsServer.h>

#include <Rest/ApplicationServerDispatcher.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // Extensions: public methods
    // -----------------------------------------------------------------------------

    map<string, ProgramOptionsDescription> RestModel::Extensions::options () {
      map<string, ProgramOptionsDescription> result;
      return result;
    }



    vector<RestModel::HandlerDescription> RestModel::Extensions::handlers () {
      vector<HandlerDescription> result;
      return result;
    }



    pair<size_t, size_t> RestModel::Extensions::sizeRestrictions () {
      static size_t m = (size_t) -1;

      return make_pair(m, m);
    }



    HttpHandlerFactory* RestModel::Extensions::buildHandlerFactory (RestModel* model) {
      HttpHandlerFactory* httpHandlerFactory = new RestHandlerFactory(model);
      vector<RestModel::HandlerDescription> handlers = this->handlers();

      for (vector<RestModel::HandlerDescription>::iterator i = handlers.begin();
           i != handlers.end();
           ++i) {
        RestModel::HandlerDescription const& desc = *i;

        if (desc.isPrefix) {
          httpHandlerFactory->addPrefixHandler(desc.path, desc.method, desc.data);
        }
        else {
          httpHandlerFactory->addHandler(desc.path, desc.method, desc.data);
        }
      }

      return httpHandlerFactory;
    }



    void RestModel::Extensions::checkArguments (ApplicationServer*) {
    }



    void RestModel::Extensions::prepareServer (ApplicationServer*) {
    }



    void RestModel::Extensions::buildApplicationServer (ApplicationServer*) {
    }



    HttpServer* RestModel::Extensions::buildHttpServer (ApplicationHttpServer* server, HttpHandlerFactory* httpHandlerFactory) {
      return server->buildServer(httpHandlerFactory);
    }




    HttpsServer* RestModel::Extensions::buildHttpsServer (ApplicationHttpsServer* server, HttpHandlerFactory* httpHandlerFactory) {
      return server->buildServer(httpHandlerFactory);
    }


    // -----------------------------------------------------------------------------
    // RestModel: constructors and destructors
    // -----------------------------------------------------------------------------

    RestModel::RestModel ()
      : extensions(0) {
    }



    RestModel::~RestModel () {
      if (extensions != 0) {
        delete extensions;
      }
    }

    // -----------------------------------------------------------------------------
    // RestModel: public methods
    // -----------------------------------------------------------------------------


    void RestModel::buildQueues (RestServer* restServer, ApplicationServerDispatcher* application) {
      application->buildStandardQueue(restServer->getNumberDispatcherThreads());
    }




    void RestModel::startupServer (ApplicationServer*) {
    }



    void RestModel::shutdownServer () {
    }
  }
}

