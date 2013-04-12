/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief AQL user functions management
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal"); 
var arangodb = require("org/arangodb");

var db = arangodb.db;
var ArangoError = arangodb.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                               module "org/arangodb/aql/functions"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a function name
////////////////////////////////////////////////////////////////////////////////

var validateName = function (name) {
  if (typeof name !== 'string' || 
      ! name.match(/^[a-zA-Z0-9_]+(:[a-zA-Z0-9_]+)+$/) ||
      name.substr(0, 1) === "_") {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.message;

    throw err;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate user function code
////////////////////////////////////////////////////////////////////////////////

var stringifyFunction = function (code, name) {
  if (typeof code === 'function') {
    code = String(code);
  }
  
  if (typeof code === 'string') {
    code = "(" + code + ")";

    if (! internal.parse) {
      // no parsing possible. assume always valid
      return code;
    }
 
    try { 
      if (internal.parse(code, name)) {
        // parsing successful
        return code;
      }
    }
    catch (e) {
    }
  }

  // fall-through intentional

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
  err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message;
    
  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the _aqlfunctions collection
////////////////////////////////////////////////////////////////////////////////

var getStorage = function () {
  var functions = db._collection("_aqlfunctions");

  if (functions === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = "collection _aqlfunctions not found";

    throw err;
  }

  return functions;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief delete an existing AQL user function
////////////////////////////////////////////////////////////////////////////////
  
var unregisterFunction = function (name) {
  var func = null;

  validateName(name);
  
  try {
    func = getStorage().document(name.toUpperCase());
  }
  catch (err1) {
  }

  if (func === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.message;

    throw err;
  }
  
  getStorage().remove(func._id);
  internal.reloadAqlFunctions();

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief register an AQL user function
////////////////////////////////////////////////////////////////////////////////
  
var registerFunction = function (name, code, isDeterministic) {
  // validate input
  validateName(name);
  code = stringifyFunction(code, name);
    
  try {
    unregisterFunction(name);
  }
  catch (err) {
  }

  var data = {
    _key: name.toUpperCase(),
    name: name,
    code: code,
    isDeterministic: isDeterministic || false
  };
    
  getStorage().save(data);
  internal.reloadAqlFunctions();

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the AQL user functons
////////////////////////////////////////////////////////////////////////////////

var reloadFunctions = function () {
  throw "cannot use abstract reload function";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.register   = registerFunction;
exports.unregister = unregisterFunction;
exports.reload     = reloadFunctions;

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

