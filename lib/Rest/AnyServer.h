////////////////////////////////////////////////////////////////////////////////
/// @brief any server
///
/// @file
/// This file contains a server template.
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_ANY_SERVER_H
#define ARANGODB_REST_ANY_SERVER_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ApplicationServer;

// -----------------------------------------------------------------------------
// --SECTION--                                                   class AnyServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief rest server base
////////////////////////////////////////////////////////////////////////////////

    class AnyServer {
      AnyServer (AnyServer const&);
      AnyServer& operator= (AnyServer const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        AnyServer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~AnyServer ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the server
////////////////////////////////////////////////////////////////////////////////

        int start ();

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown sequence
////////////////////////////////////////////////////////////////////////////////

        void beginShutdown ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the application server
////////////////////////////////////////////////////////////////////////////////

        virtual void buildApplicationServer () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server for startup
////////////////////////////////////////////////////////////////////////////////

        virtual void prepareServer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief start the server using the description
////////////////////////////////////////////////////////////////////////////////

        virtual int startupServer () = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief running in daemon mode
////////////////////////////////////////////////////////////////////////////////

        bool _daemonMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief running in supervisor mode
////////////////////////////////////////////////////////////////////////////////

        bool _supervisorMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief pid file
/// @startDocuBlock pidFile
/// `--pid-file filename`
///
/// The name of the process ID file to use when running the server as a
/// daemon. This parameter must be specified if either the flag *daemon* or
/// *supervisor* is set.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        std::string _pidFile;

////////////////////////////////////////////////////////////////////////////////
/// @brief working directory
////////////////////////////////////////////////////////////////////////////////

        std::string _workingDirectory;

////////////////////////////////////////////////////////////////////////////////
/// @brief application server
////////////////////////////////////////////////////////////////////////////////

        ApplicationServer* _applicationServer;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a supervisor
////////////////////////////////////////////////////////////////////////////////

        int startupSupervisor ();

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a daemon
////////////////////////////////////////////////////////////////////////////////

        int startupDaemon ();
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
