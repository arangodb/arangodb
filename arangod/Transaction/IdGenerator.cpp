////////////////////////////////////////////////////////////////////////////////
/// @brief transaction id generator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "IdGenerator.h"
#include "VocBase/server.h"

using namespace triagens::transaction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an id generator
////////////////////////////////////////////////////////////////////////////////

IdGenerator::IdGenerator (Transaction::IdType id) {
  setLastId(id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an id generator
////////////////////////////////////////////////////////////////////////////////

IdGenerator::~IdGenerator () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief set minimal transaction id
////////////////////////////////////////////////////////////////////////////////

void IdGenerator::setLastId (Transaction::IdType id) {
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction id
////////////////////////////////////////////////////////////////////////////////

Transaction::IdType IdGenerator::next () {
  return static_cast<Transaction::IdType>(TRI_NewTickServer());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
