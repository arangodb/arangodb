////////////////////////////////////////////////////////////////////////////////
/// @brief console thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_CONSOLE_THREAD_H
#define ARANGODB_REST_SERVER_CONSOLE_THREAD_H 1

#include "Basics/Thread.h"

#include "V8Server/ApplicationV8.h"

struct TRI_vocbase_t;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace arangodb {
    class V8LineEditor;
}

namespace triagens {
  namespace rest {
    class ApplicationServer;
  }

  namespace arango {
    class ApplicationV8;

// -----------------------------------------------------------------------------
// --SECTION--                                               class ConsoleThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ConsoleThread
////////////////////////////////////////////////////////////////////////////////

    class ConsoleThread : public basics::Thread {
      ConsoleThread (const ConsoleThread&) = delete;
      ConsoleThread& operator= (const ConsoleThread&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                           static public variables
// -----------------------------------------------------------------------------

    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief the line editor object for use in debugging
////////////////////////////////////////////////////////////////////////////////

      static arangodb::V8LineEditor* serverConsole;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for console access
////////////////////////////////////////////////////////////////////////////////

      static triagens::basics::Mutex serverConsoleMutex;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ConsoleThread (triagens::rest::ApplicationServer*,
		       ApplicationV8*,
		       TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ConsoleThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the thread
////////////////////////////////////////////////////////////////////////////////

        void run () override;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the console thread is done
////////////////////////////////////////////////////////////////////////////////

        bool done () const {
          return _done;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the user abort flag
////////////////////////////////////////////////////////////////////////////////

        void userAbort () {
          _userAborted = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the thread is chatty on shutdown
////////////////////////////////////////////////////////////////////////////////

        bool isSilent () override {
          return true;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief inner thread loop
////////////////////////////////////////////////////////////////////////////////

        void inner ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief application server
////////////////////////////////////////////////////////////////////////////////

        rest::ApplicationServer* _applicationServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 dealer
////////////////////////////////////////////////////////////////////////////////

        ApplicationV8* _applicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief currently used V8 context
////////////////////////////////////////////////////////////////////////////////

        ApplicationV8::V8Context* _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief done flag
////////////////////////////////////////////////////////////////////////////////

	std::atomic<bool> _done;

////////////////////////////////////////////////////////////////////////////////
/// @brief user aborted flag
////////////////////////////////////////////////////////////////////////////////

        bool _userAborted;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
