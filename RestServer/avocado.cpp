////////////////////////////////////////////////////////////////////////////////
/// @brief AvocadoDB server
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <Basics/Common.h>

#include <Rest/Initialise.h>

#include "RestServer/AvocadoServer.h"

using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

////////////////////////////////////////////////////////////////////////////////
/// @page StartStop Starting and stopping
///
/// The AvocadoDB has two mode of operations: as server, where it will answer to
/// HTTP request, see @ref HttpInterface, and a debug shell, where you can
/// access the database directly. Using the debug shell always you to issue all
/// command normally available in actions and transactions, see @ref
/// AvocadoScript.
///
/// The following main command-line options are available.
///
/// @copydetails triagens::avocado::AvocadoServer::_databasePath
///
/// @CMDOPT{--shell}
///
/// Opens a debug shell instead of starting the HTTP server.
///
/// @CMDOPT{--log.level @CA{level}}
///
/// Allows the user to choose the level of information which is logged by the
/// server. The arg is specified as a string and can be one of the following
/// values: fatal, error, warning, info, debug, trace.  For more information see
/// @ref CommandLineLogging "here".
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application server
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  TRIAGENS_REST_INITIALISE;
  TRI_InitialiseVocBase();

  // create and start a AvocadoDB server
  AvocadoServer server(argc, argv);

  int res = server.start();

  // shutdown
  TRI_ShutdownVocBase();
  TRIAGENS_REST_SHUTDOWN;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
