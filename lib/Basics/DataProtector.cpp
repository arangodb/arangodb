////////////////////////////////////////////////////////////////////////////////
/// @brief Helper class to isolate data protection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2015 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/DataProtector.h"

// TODO: Make this a template again once everybody has gcc >= 4.9.2
// template<int Nr> 
// thread_local int triagens::basics::DataProtector<Nr>::_mySlot = -1;
thread_local int triagens::basics::DataProtector::_mySlot = -1;

/// We need an explicit template initialisation for each value of the
/// template parameter, such that the compiler can allocate a thread local
/// variable for each of them.

// TODO: Reactivate this template instanciation once everybody has gcc >= 4.9.2
// template class triagens::basics::DataProtector<64>;

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
