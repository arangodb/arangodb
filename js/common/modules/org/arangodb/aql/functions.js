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
/// @brief apply a prefix filter on the functions
////////////////////////////////////////////////////////////////////////////////

var getFiltered = function (group) {
  var result = [ ];

  if (group !== null && group !== undefined && group.length > 0) {
    var prefix = group.toUpperCase();

    if (group.substr(group.length - 1, 1) !== ':') {
      prefix += ':';
    }
   
    getStorage().toArray().forEach(function (f) {
      if (f.name.toUpperCase().substr(0, prefix.length) === prefix) {
        result.push(f);
      }
    });
  }
  else {
    result = getStorage().toArray();
  }

  return result;
};

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
    err.errorMessage = "collection '_aqlfunctions' not found";

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
/// @fn JSF_aqlfunctions_unregister
/// @brief delete an existing AQL user function
///
/// @FUN{aqlfunctions.unregister(@FA{name})}
///
/// Unregisters an existing AQL user function, identified by the fully qualified
/// function name.
///
/// Trying to unregister a function that does not exist will result in an 
/// exception.
///
/// @EXAMPLES
///
/// @code
/// arangosh> require("org/arangodb/aql/functions").unregister("myfunctions:temperature:celsiustofahrenheit");
/// @endcode
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
    err.errorMessage = internal.sprintf(arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.message, name);

    throw err;
  }
  
  getStorage().remove(func._id);
  internal.reloadAqlFunctions();

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_aqlfunctions_unregister_group
/// @brief delete a group of AQL user functions
///
/// @FUN{aqlfunctions.unregisterGroup(@FA{prefix})}
///
/// Unregisters a group of AQL user function, identified by a common function 
/// group prefix.
///
/// This will return the number of functions unregistered.
///
/// @EXAMPLES
///
/// @code
/// arangosh> require("org/arangodb/aql/functions").unregisterGroup("myfunctions:temperature");
///
/// arangosh> require("org/arangodb/aql/functions").unregisterGroup("myfunctions");
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  
var unregisterFunctionsGroup = function (group) {
  if (group.length === 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message;
  }

  var deleted = 0;
  
  getFiltered(group).forEach(function (f) {
    getStorage().remove(f._id);
    deleted++;
  });
  
  if (deleted > 0) {
    internal.reloadAqlFunctions();
  }

  return deleted;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_aqlfunctions_register
/// @brief register an AQL user function
///
/// @FUN{aqlfunctions.register(@FA{name}, @FA{code})}
///
/// Registers an AQL user function, identified by a fully qualified function 
/// name. The function code in @FA{code} must be specified as a Javascript
/// function or a string representation of a Javascript function. 
///
/// If a function identified by @FA{name} already exists, the previous function
/// definition will be updated.
///
/// @EXAMPLES
///
/// @code
/// arangosh> require("org/arangodb/aql/functions").register("myfunctions:temperature:celsiustofahrenheit", 
///                   function (celsius) {
///                     return celsius * 1.8 + 32; 
///                   });
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  
var registerFunction = function (name, code, isDeterministic) {
  // validate input
  validateName(name);
  code = stringifyFunction(code, name);

  var exists = false;
    
  try {
    unregisterFunction(name);
    exists = true;
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

  return exists;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_aqlfunctions_toArray
/// @brief list all AQL user functions
///
/// @FUN{aqlfunctions.toArray()}
///
/// Returns all previously registered AQL user functions, with their fully 
/// qualified names and function code.
/// 
/// The result may optionally be restricted to a specified group of functions
/// by specifying a group prefix:
///
/// @FUN{aqlfunctions.toArray(@FA{prefix})}
///
/// @EXAMPLES
///
/// To list all available user functions:
///
/// @code
/// arangosh> require("org/arangodb/aql/functions").toArray();
/// @endcode
///
/// To list all available user functions in the `myfunctions` namespace:
///
/// @code
/// arangosh> require("org/arangodb/aql/functions").toArray("myfunctions");
/// @endcode
///
/// To list all available user functions in the `myfunctions:temperature` namespace:
///
/// @code
/// arangosh> require("org/arangodb/aql/functions").toArray("myfunctions:temperature");
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  
var toArrayFunctions = function (group) {
  var result = [ ];

  getFiltered(group).forEach(function (f) {
    result.push({ name: f.name, code: f.code });
  });

  return result;
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

exports.unregister      = unregisterFunction;
exports.unregisterGroup = unregisterFunctionsGroup;
exports.register        = registerFunction;
exports.toArray         = toArrayFunctions;

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

