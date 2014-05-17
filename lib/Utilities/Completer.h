////////////////////////////////////////////////////////////////////////////////
/// @brief a basis class which defines the methods for determining
///        when an input is "complete"
///       
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Esteban Lombeyda
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILITIES_COMPLETER_H
#define TRIAGENS_UTILITIES_COMPLETER_H 1

#include <string>
#include <vector>

namespace triagens {
  using namespace std;
  class Completer {

     public:
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief                                      public constructors destructors
    ////////////////////////////////////////////////////////////////////////////////

       virtual ~Completer() {};

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check if line is complete
    ////////////////////////////////////////////////////////////////////////////////

    virtual bool isComplete(string const&, size_t lineno, size_t column) = 0;
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief  computes all strings which begins with the given text
    ////////////////////////////////////////////////////////////////////////////////

    virtual void getAlternatives(char const *, vector<string> &) = 0;
      
  };
}
#endif
