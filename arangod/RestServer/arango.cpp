////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "RestServer/ArangoServer.h"
#include "ResultGenerator/InitialiseGenerator.h"

using namespace triagens;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application server
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int res;

#ifdef _WIN32

  // ...........................................................................
  // Call this function to do various initialistions for windows only
  // ...........................................................................
  
  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0); 

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }
  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

#endif

  TRIAGENS_RESULT_GENERATOR_INITIALISE(argc, argv);

  // create and start a ArangoDB server
  ArangoServer server(argc, argv);

  res = server.start();

  // shutdown
  TRIAGENS_RESULT_GENERATOR_SHUTDOWN;

#ifdef _WIN32

  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

#endif  

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
