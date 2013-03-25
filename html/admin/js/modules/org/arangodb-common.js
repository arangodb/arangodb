module.define("org/arangodb-common", function(exports, module) {
/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript base module
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                 module "arangodb"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief guesses the content type
////////////////////////////////////////////////////////////////////////////////

exports.guessContentType = function (filename) {
  var re = /.*\.([^\.]*)$/;
  var match = re.exec(filename);
  var extension;

  if (match === null) {
    return "text/plain; charset=utf-8";
  }

  extension = match[1];

  if (extension === "html") {
    return "text/html; charset=utf-8";
  }

  if (extension === "xml") {
    return "application/xml; charset=utf-8";
  }

  if (extension === "json") {
    return "application/json; charset=utf-8";
  }

  if (extension === "js") {
    return "application/x-javascript; charset=utf-8";
  }

  return "text/plain; charset=utf-8";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief function "normalizeURL"
////////////////////////////////////////////////////////////////////////////////

exports.normalizeURL = internal.normalizeURL;

////////////////////////////////////////////////////////////////////////////////
/// @brief function "output"
////////////////////////////////////////////////////////////////////////////////

exports.output = function () {
  internal.output.apply(internal.output, arguments);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief function "print"
////////////////////////////////////////////////////////////////////////////////

exports.print = internal.print;

////////////////////////////////////////////////////////////////////////////////
/// @brief function "printf"
////////////////////////////////////////////////////////////////////////////////

exports.printf = internal.printf;

////////////////////////////////////////////////////////////////////////////////
/// @brief function "printObject"
////////////////////////////////////////////////////////////////////////////////

exports.printObject = internal.printObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief error codes
////////////////////////////////////////////////////////////////////////////////

(function () {
  var name;

  for (name in internal.errors) {
    if (internal.errors.hasOwnProperty(name)) {
      exports[name] = internal.errors[name].code;
    }
  }
}());

exports.errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:
});
