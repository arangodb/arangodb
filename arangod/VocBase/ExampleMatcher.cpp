////////////////////////////////////////////////////////////////////////////////
/// @brief Class to allow simple byExample matching of mptr.
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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

#include "ExampleMatcher.h"
#include "voc-shaper.h"

using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the given mptr matches the example in this class
////////////////////////////////////////////////////////////////////////////////

bool ExampleMatcher::matches (TRI_doc_mptr_t const* mptr) {
  if (mptr == nullptr) {
    return false;
  }
  TRI_shaped_json_t document;
  TRI_shaped_json_t result;
  TRI_shape_t const* shape;

  TRI_EXTRACT_SHAPED_JSON_MARKER(document, mptr->getDataPtr());

  for (size_t i = 0;  i < _values.size();  ++i) {
    TRI_shaped_json_t* example = _values[i];

    bool ok = TRI_ExtractShapedJsonVocShaper(_shaper,
                                             &document,
                                             example->_sid,
                                             _pids[i],
                                             &result,
                                             &shape);

    if (! ok || shape == nullptr) {
      return false;
    }

    if (result._data.length != example->_data.length) {
      // suppress excessive log spam
      // LOG_TRACE("expecting length %lu, got length %lu for path %lu",
      //           (unsigned long) result._data.length,
      //           (unsigned long) example->_data.length,
      //           (unsigned long) pids[i]);

      return false;
    }

    if (memcmp(result._data.data, example->_data.data, example->_data.length) != 0) {
      // suppress excessive log spam
      // LOG_TRACE("data mismatch at path %lu", (unsigned long) pids[i]);
      return false;
    }
  }
  return true;
};
