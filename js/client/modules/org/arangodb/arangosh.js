/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, regexp: true plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var ArangoError = require("org/arangodb/arango-error").ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "arangosh"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleArangosh
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a formatted type string for object
/// 
/// If the object has an id, it will be included in the string.
////////////////////////////////////////////////////////////////////////////////

exports.getIdString = function (object, typeName) {
  var result = "[object " + typeName;

  if (object._id) {
    result += ":" + object._id;
  }
  else if (object.data && object.data._id) {
    result += ":" + object.data._id;
  }

  result += "]";

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error results
/// 
/// throws an exception in case of an an error
////////////////////////////////////////////////////////////////////////////////

exports.checkRequestResult = function (requestResult) {
  if (requestResult === undefined) {
    requestResult = {
      "error" : true,
      "code"  : 0,
      "errorNum" : 0,
      "errorMessage" : "Unknown error. Request result is empty"
    };
  }

  if (requestResult.error !== undefined && requestResult.error) {    
    throw new ArangoError(requestResult);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted headline text 
////////////////////////////////////////////////////////////////////////////////

exports.createHelpHeadline = function (text) {
  var i;
  var p = "";
  var x = Math.abs(78 - text.length) / 2;

  for (i = 0; i < x; ++i) {
    p += "-";
  }

  return "\n" + p + " " + text + " " + p + "\n";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
