////////////////////////////////////////////////////////////////////////////////
/// @brief arithmetic helpers
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

#ifndef ARANGODB_AQL_ARITHMETIC_H
#define ARANGODB_AQL_ARITHMETIC_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

    template<typename T>
    bool IsUnsafeAddition (T l, T r) {
      return ((r > 0 && l > std::numeric_limits<T>::max() - r) ||
              (r < 0 && l < std::numeric_limits<T>::min() - r));
    }

    template<typename T>
    bool IsUnsafeSubtraction (T l, T r) {
      return ((r > 0 && l < std::numeric_limits<T>::min() + r) || (r < 0 && l > std::numeric_limits<T>::max() + r));
    }

    template<typename T>
    bool IsUnsafeMultiplication (T l, T r) {
      if (l > 0) {  
        if (r > 0) { 
          if (l > (std::numeric_limits<T>::max() / r)) {
            return true;
          }
        } 
        else { 
          if (r < (std::numeric_limits<T>::min() / l)) {
            return true;
          }
        } 
      } 
      else { 
        if (r > 0) { 
          if (l < (std::numeric_limits<T>::min() / r)) {
            return true;
          }
        } 
        else { 
          if ( (l != 0) && (r < (std::numeric_limits<T>::max() / l))) {
            return true;
          }
        } 
      }

      return false;
    }

    template<typename T>
    bool IsUnsafeDivision (T l, T r) {
      return (l == std::numeric_limits<T>::min() && r == -1);
    } 

  }
} 

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

