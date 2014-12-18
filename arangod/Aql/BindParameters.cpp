////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, bind parameters
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/BindParameters.h"
#include "Basics/json.h"
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parameters
////////////////////////////////////////////////////////////////////////////////

BindParameters::BindParameters (TRI_json_t* json)
  : _json(json),
    _parameters(),
    _processed(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the parameters
////////////////////////////////////////////////////////////////////////////////

BindParameters::~BindParameters () {
  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief process the parameters
////////////////////////////////////////////////////////////////////////////////

void BindParameters::process () {
  if (_processed || _json == nullptr) {
    return;
  }

  if (! TRI_IsObjectJson(_json)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID);
  }
  
  size_t const n = _json->_value._objects._length;
  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t const* key = static_cast<TRI_json_t const*>(TRI_AtVector(&_json->_value._objects, i));

    if (! TRI_IsStringJson(key)) {
      // no string, should not happen
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE);
    }

    std::string const k(key->_value._string.data, key->_value._string.length - 1);

    TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&_json->_value._objects, i + 1));

    if (value == nullptr) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, k.c_str()); 
    }

    if (k[0] == '@' && ! TRI_IsStringJson(value)) {
      // collection bind parameter
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, k.c_str()); 
    }

    _parameters.emplace(std::make_pair(k, std::make_pair(value, false)));
  }
  
  _processed = true;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
