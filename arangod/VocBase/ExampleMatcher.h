////////////////////////////////////////////////////////////////////////////////
/// @brief Class to allow simple byExample matching of mptr.
///
/// @file
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
/// @author Michael Hackstein
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_EXAMPLE_MATCHER_H
#define ARANGODB_EXAMPLE_MATCHER_H 1

#include "ShapedJson/json-shaper.h"
#include "document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   Matching Method
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {


    class ExampleMatcher {

      TRI_shaper_t* _shaper;
      std::vector<TRI_shape_pid_t> _pids;
      std::vector<TRI_shaped_json_t*> _values;

      public:

        ExampleMatcher(
          TRI_shaper_t* shaper,
          std::vector<TRI_shape_pid_t>& pids,
          std::vector<TRI_shaped_json_t*>& values
        ) : _shaper(shaper),
          _pids(pids), _values(values) {};

        ~ExampleMatcher() {};

        bool matches (TRI_doc_mptr_t const* mptr);
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
