////////////////////////////////////////////////////////////////////////////////
/// @brief fundamental types for the optimisation and execution of AQL
///
/// @file 
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_TYPES_H
#define ARANGODB_AQL_TYPES_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief type for register numbers/ids
////////////////////////////////////////////////////////////////////////////////

    typedef unsigned int RegisterId;

////////////////////////////////////////////////////////////////////////////////
/// @brief Range, to hold a range compactly
////////////////////////////////////////////////////////////////////////////////

    struct Range {
      Range () = delete;

      Range (int64_t low, 
             int64_t high) 
        : _low(low), 
          _high(high) {
      }

      size_t size () const {
        if (_low <= _high) {
          // e.g. 1..1, 1..10 etc.
          return _high - _low + 1;
        }
        // e.g. 10..1
        return _low - _high + 1;
      }
      
      int64_t at (size_t position) const {
        if (_low <= _high) {
          // e.g. 1..1, 1..10 etc.
          return _low + static_cast<int64_t>(position);
        }

        // e.g. 10..1
        return _low - static_cast<int64_t>(position);
      }
        
      int64_t const _low;
      int64_t const _high;
    };

  }
} 

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

