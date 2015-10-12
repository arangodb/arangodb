////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, Index
///
/// @file arangod/Aql/ExecutionNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Index.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of IndexNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index to an output stream
////////////////////////////////////////////////////////////////////////////////
     
std::ostream& operator<< (std::ostream& stream,
                          triagens::aql::Index const* index) {
  stream << index->getInternals()->context();
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<< (std::ostream& stream,
                          triagens::aql::Index const& index) {
  stream << index.getInternals()->context();
  return stream;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

