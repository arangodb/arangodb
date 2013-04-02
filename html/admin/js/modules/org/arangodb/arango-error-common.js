module.define("org/arangodb/arango-error-common", function(exports, module) {
/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
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

var arangodb = require("org/arangodb");

var ArangoError = require("org/arangodb/arango-error").ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                       ArangoError
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the object
////////////////////////////////////////////////////////////////////////////////

ArangoError.prototype._PRINT = function (context) {
  context += this.toString();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into a string
////////////////////////////////////////////////////////////////////////////////

ArangoError.prototype.toString = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage;

  return "[ArangoError " + errorNum + ": " + errorMessage + "]";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});
