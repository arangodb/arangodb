/*jshint strict: false, unused: false, bitwise: false */
/*global require, exports, COMPARE_STRING, AQL_TO_BOOL, AQL_TO_NUMBER, AQL_TO_STRING, AQL_WARNING */

////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, internal query functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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

var INTERNAL = require("internal");
var TRAVERSAL = require("org/arangodb/graph/traversal");
var ArangoError = require("org/arangodb").ArangoError;
var ShapedJson = INTERNAL.ShapedJson;
var isCoordinator = require("org/arangodb/cluster").isCoordinator();
var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cache for compiled regexes
////////////////////////////////////////////////////////////////////////////////

var RegexCache = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief user functions cache
////////////////////////////////////////////////////////////////////////////////

var UserFunctions = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief type weight used for sorting and comparing
////////////////////////////////////////////////////////////////////////////////

var TYPEWEIGHT_NULL      = 0;
var TYPEWEIGHT_BOOL      = 1;
var TYPEWEIGHT_NUMBER    = 2;
var TYPEWEIGHT_STRING    = 4;
var TYPEWEIGHT_LIST      = 8;
var TYPEWEIGHT_DOCUMENT  = 16;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief raise a warning
////////////////////////////////////////////////////////////////////////////////

function WARN (func, error, data) {
  "use strict";

  if (error.code === INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code) {
    AQL_WARNING(error.code, error.message.replace(/%s/, func)); 
  }
  else {
    var prefix = "";
    if (func !== null) {
      prefix = "in function '" + func + "()': ";
    }

    if (typeof data === "string") {
      AQL_WARNING(error.code, prefix + error.message.replace(/%s/, data));
    }
    else {
      AQL_WARNING(error.code, prefix + error.message);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief throw a runtime exception
////////////////////////////////////////////////////////////////////////////////

function THROW (func, error, data) {
  "use strict";
  
  var prefix = "";
  if (func !== null && func !== "") {
    prefix = "in function '" + func + "()': ";
  }

  var err = new ArangoError();

  err.errorNum = error.code;
  if (typeof data === "string") {
    err.errorMessage = prefix + error.message.replace(/%s/, data);
  }
  else {
    err.errorMessage = prefix + error.message;
  }

  throw err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a database-specific function prefix
////////////////////////////////////////////////////////////////////////////////

function DB_PREFIX () {
  return INTERNAL.db._name();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the regex cache
////////////////////////////////////////////////////////////////////////////////

function resetRegexCache () {
  "use strict";

  RegexCache = { 'i' : { }, '' : { } };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the user functions and reload them from the database
////////////////////////////////////////////////////////////////////////////////

function reloadUserFunctions () {
  "use strict";

  var c;

  c = INTERNAL.db._collection("_aqlfunctions");

  if (c === null) {
    return;
  }

  var foundError = false;
  var prefix = DB_PREFIX();

  UserFunctions[prefix] = { };

  c.toArray().forEach(function (f) {
    var code = "(function() { var callback = " + f.code + "; return callback; })();";
    var key = f._key.replace(/:{1,}/g, '::');

    try {
      var res = INTERNAL.executeScript(code, undefined, "(user function " + key + ")");

      UserFunctions[prefix][key.toUpperCase()] = {
        name: key,
        func: res,
        isDeterministic: f.isDeterministic || false
      };

    }
    catch (err) {
      foundError = true;
    }
  });

  if (foundError) {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the query engine
////////////////////////////////////////////////////////////////////////////////

function resetEngine () {
  "use strict";

  resetRegexCache();
  reloadUserFunctions();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a function name
////////////////////////////////////////////////////////////////////////////////

function NORMALIZE_FNAME (functionName) {
  "use strict";

  var p = functionName.indexOf('::');

  if (p === -1) {
    return functionName;
  }

  return functionName.substr(p + 2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter using a list of examples
////////////////////////////////////////////////////////////////////////////////

function FILTER (list, examples) {
  "use strict";

  var result = [ ], i;

  if (examples === undefined || examples === null ||
    (Array.isArray(examples) && examples.length === 0)) {
    return list;
  }

  for (i = 0; i < list.length; ++i) {
    var element = list[i];
    if (AQL_MATCHES(element, examples, false)) {
      result.push(element);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a fulltext index for a certain attribute & collection
////////////////////////////////////////////////////////////////////////////////

function INDEX_FULLTEXT (collection, attribute) {
  "use strict";

  var indexes = collection.getIndexes(), i;

  for (i = 0; i < indexes.length; ++i) {
    var index = indexes[i];
    if (index.type === "fulltext" && index.fields && index.fields[0] === attribute) {
      return index.id;
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an index of a certain type for a collection
////////////////////////////////////////////////////////////////////////////////

function INDEX (collection, indexTypes) {
  "use strict";

  var indexes = collection.getIndexes(), i, j;

  for (i = 0; i < indexes.length; ++i) {
    var index = indexes[i];

    for (j = 0; j < indexTypes.length; ++j) {
      if (index.type === indexTypes[j]) {
        return index;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get access to a collection
////////////////////////////////////////////////////////////////////////////////

function COLLECTION (name) {
  "use strict";

  if (typeof name !== 'string') {
    THROW(null, INTERNAL.errors.ERROR_INTERNAL);
  }

  if (name.substring(0, 1) === '_') {
    // system collections need to be accessed slightly differently as they
    // are not returned by the propertyGetter of db
    return INTERNAL.db._collection(name);
  }

  return INTERNAL.db[name];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms a parameter into a list
////////////////////////////////////////////////////////////////////////////////

function TO_LIST (param, isStringHash) {

  if (! param) {
    param = isStringHash ? [ ] : [ { } ];
  }
  if (typeof param === "string") {
    param = isStringHash ? [ param ] : { _id : param };
  }
  if (! Array.isArray(param)) {
    param = [param];
  }
  return param;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an object
////////////////////////////////////////////////////////////////////////////////

function CLONE (obj) {
  "use strict";

  if (obj === null || typeof(obj) !== "object") {
    return obj;
  }

  var copy;
  if (Array.isArray(obj)) {
    copy = [ ];
    obj.forEach(function (i) {
      copy.push(CLONE(i));
    });
  }
  else if (obj instanceof Object) {
    copy = { };
    Object.keys(obj).forEach(function(k) {
      copy[k] = CLONE(obj[k]);
    });
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief box a value into the AQL datatype system
////////////////////////////////////////////////////////////////////////////////

function FIX_VALUE (value) {
  "use strict";

  if (value instanceof ShapedJson) {
    return value;
  }

  var type = typeof(value);

  if (value === undefined ||
      value === null ||
      (type === 'number' && (isNaN(value) || ! isFinite(value)))) {
    return null;
  }

  if (type === 'boolean' || type === 'string' || type === 'number') {
    return value;
  }

  if (Array.isArray(value)) {
    var i, n = value.length;
    for (i = 0; i < n; ++i) {
      value[i] = FIX_VALUE(value[i]);
    }

    return value;
  }

  if (type === 'object') {
    var result = { };

    Object.keys(value).forEach(function(k) {
      if (typeof value[k] !== 'function') {
        result[k] = FIX_VALUE(value[k]);
      }
    });

    return result;
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the sort type of an operand
////////////////////////////////////////////////////////////////////////////////

function TYPEWEIGHT (value) {
  "use strict";

  if (value !== undefined && value !== null) {
    if (Array.isArray(value)) {
      return TYPEWEIGHT_LIST;
    }

    switch (typeof(value)) {
      case 'boolean':
        return TYPEWEIGHT_BOOL;
      case 'number':
        if (isNaN(value) || ! isFinite(value)) {
          // not a number => null
          return TYPEWEIGHT_NULL;
        }
        return TYPEWEIGHT_NUMBER;
      case 'string':
        return TYPEWEIGHT_STRING;
      case 'object':
        return TYPEWEIGHT_DOCUMENT;
    }
  }

  return TYPEWEIGHT_NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a regex from a string pattern
////////////////////////////////////////////////////////////////////////////////

function COMPILE_REGEX (regex, modifiers) {
  "use strict";

  regex = AQL_TO_STRING(regex);
  var i, n = regex.length;
  var escaped = false;
  var pattern = '';
  var specialChar = /^([.*+?\^=!:${}()|\[\]\/\\])$/;

  for (i = 0; i < n; ++i) {
    var c = regex.charAt(i);

    if (c === '\\') {
      if (escaped) {
        // literal \
        pattern += '\\\\';
      }
      escaped = ! escaped;
    }
    else {
      if (c === '%') {
        if (escaped) {
          // literal %
          pattern += '%';
        }
        else {
          // wildcard
          pattern += '.*';
        }
      }
      else if (c === '_') {
        if (escaped) {
          // literal _
          pattern += '_';
        }
        else {
          // wildcard character
          pattern += '.';
        }
      }
      else if (c.match(specialChar)) {
        // character with special meaning in a regex
        pattern += '\\' + c;
      }
      else {
        if (escaped) {
          // found a backslash followed by no special character
          pattern += '\\\\';
        }

        // literal character
        pattern += c;
      }

      escaped = false;
    }
  }

  return new RegExp('^' + pattern + '$', modifiers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief call a user function
////////////////////////////////////////////////////////////////////////////////

function FCALL_USER (name, parameters) {
  "use strict";

  var reloaded = false;
  var prefix = DB_PREFIX();
  if (! UserFunctions.hasOwnProperty(prefix)) {
    reloadUserFunctions();
    reloaded = true;

    if (! UserFunctions.hasOwnProperty(prefix)) {
      THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, NORMALIZE_FNAME(name));
    }
  }
  
  if (! reloaded && ! UserFunctions[prefix].hasOwnProperty(name)) {
    reloadUserFunctions();
  }

  if (UserFunctions[prefix].hasOwnProperty(name)) {
    try {
      var result = UserFunctions[prefix][name].func.apply(null, parameters);
      return FIX_VALUE(result);
    }
    catch (err) {
      WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err));
      return null;
    }
  }

  THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, NORMALIZE_FNAME(name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the numeric value or undefined if it is out of range
////////////////////////////////////////////////////////////////////////////////

function NUMERIC_VALUE (value) {
  "use strict";

  if (isNaN(value) || ! isFinite(value)) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fix a value for a comparison
////////////////////////////////////////////////////////////////////////////////

function FIX (value) {
  "use strict";

  if (value === undefined) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the values of an object in the order that they are defined
////////////////////////////////////////////////////////////////////////////////

function VALUES (value) {
  "use strict";

  var values = [ ];

  Object.keys(value).forEach(function(k) {
    values.push(value[k]);
  });

  return values;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract key names from an argument list
////////////////////////////////////////////////////////////////////////////////

function EXTRACT_KEYS (args, startArgument, func) {
  "use strict";

  var keys = { }, i, j, key, key2;

  for (i = startArgument; i < args.length; ++i) {
    key = args[i];
    if (typeof key === 'string') {
      keys[key] = true;
    }
    else if (typeof key === 'number') {
      keys[String(key)] = true;
    }
    else if (Array.isArray(key)) {
      for (j = 0; j < key.length; ++j) {
        key2 = key[j];
        if (typeof key2 === 'string') {
          keys[key2] = true;
        }
        else {
          WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
          return null;
        }
      }
    }
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the keys of an array or object in a comparable way
////////////////////////////////////////////////////////////////////////////////

function KEYS (value, doSort) {
  "use strict";

  var keys;

  if (Array.isArray(value)) {
    var n = value.length, i;
    keys = [ ];

    for (i = 0; i < n; ++i) {
      keys.push(i);
    }
  }
  else {
    keys = Object.keys(value);

    if (doSort) {
      // object keys need to be sorted by names
      keys.sort();
    }
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the keys of an array or object in a comparable way
////////////////////////////////////////////////////////////////////////////////

function KEYLIST (lhs, rhs) {
  "use strict";

  if (Array.isArray(lhs)) {
    // lhs & rhs are lists
    return KEYS(lhs.length > rhs.length ? lhs : rhs);
  }

  // lhs & rhs are arrays
  var a, keys = KEYS(lhs);
  for (a in rhs) {
    if (rhs.hasOwnProperty(a) && ! lhs.hasOwnProperty(a)) {
      keys.push(a);
    }
  }

  // object keys need to be sorted by names
  keys.sort();

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an indexed value from an array or document (e.g. users[3])
////////////////////////////////////////////////////////////////////////////////

function GET_INDEX (value, index) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_NULL) {
    return null;
  }

  var result = null;
  if (TYPEWEIGHT(value) === TYPEWEIGHT_DOCUMENT) {
    result = value[String(index)];
  }
  else if (TYPEWEIGHT(value) === TYPEWEIGHT_LIST) {
    var i = parseInt(index, 10);
    if (i < 0) {
      // negative indexes fetch the element from the end, e.g. -1 => value[value.length - 1];
      i = value.length + i;
    }

    if (i >= 0 && i <= value.length - 1) {
      result = value[i];
    }
  }
  else {
    return null;
  }

  if (TYPEWEIGHT(result) === TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize a value for comparison, sorting etc.
////////////////////////////////////////////////////////////////////////////////

function NORMALIZE (value) {
  "use strict";

  if (value === null || value === undefined) {
    return null;
  }

  if (typeof(value) !== "object") {
    return value;
  }

  var result;

  if (Array.isArray(value)) {
    result = [ ];
    value.forEach(function (v) {
      result.push(NORMALIZE(v));
    });
  }
  else {
    result = { };
    KEYS(value, true).forEach(function (a) {
      result[a] = NORMALIZE(value[a]);
    });
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an attribute from a document (e.g. users.name)
////////////////////////////////////////////////////////////////////////////////

function DOCUMENT_MEMBER (value, attributeName) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_DOCUMENT) {
    return null;
  }

  var result = value[attributeName];

  if (TYPEWEIGHT(result) === TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document by its unique id or their unique ids
////////////////////////////////////////////////////////////////////////////////

function DOCUMENT_HANDLE (id) {
  "use strict";

  if (TYPEWEIGHT(id) === TYPEWEIGHT_LIST) {
    var result = [ ], i;
    for (i = 0; i < id.length; ++i) {
      try {
        result.push(INTERNAL.db._document(id[i]));
      }
      catch (e1) {
      }
    }
    return result;
  }

  try {
    return INTERNAL.db._document(id);
  }
  catch (e2) {
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document by its unique id or their unique ids
////////////////////////////////////////////////////////////////////////////////

function AQL_DOCUMENT (collection, id) {
  "use strict";

  // we're polymorphic
  if (id === undefined) {
    // called with a single parameter
    var weight = TYPEWEIGHT(collection);

    if (weight === TYPEWEIGHT_STRING || weight === TYPEWEIGHT_LIST) {
      return DOCUMENT_HANDLE(collection);
    }
  }

  if (TYPEWEIGHT(id) === TYPEWEIGHT_LIST) {
    var c = COLLECTION(collection);

    var result = [ ], i;
    for (i = 0; i < id.length; ++i) {
      try {
        result.push(c.document(id[i]));
      }
      catch (e1) {
      }
    }
    return result;
  }

  try {
    return COLLECTION(collection).document(id);
  }
  catch (e2) {
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents from the specified collection
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS (collection, offset, limit) {
  "use strict";

  if (offset === undefined) {
    offset = 0;
  }
  if (limit === undefined) {
    limit = null;
  }

  if (isCoordinator) {
    return COLLECTION(collection).all().skip(offset).limit(limit).toArray();
  }

  return COLLECTION(collection).ALL(offset, limit).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get names of all collections
////////////////////////////////////////////////////////////////////////////////

function AQL_COLLECTIONS () {
  "use strict";

  var result = [ ];

  INTERNAL.db._collections().forEach(function (c) {
    result.push({
      _id : c._id,
      name : c.name()
    });
  });

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                logical operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute ternary operator
///
/// the condition should be a boolean value, returns either the truepart
/// or the falsepart
////////////////////////////////////////////////////////////////////////////////

function TERNARY_OPERATOR (condition, truePart, falsePart) {
  "use strict";

  if (condition) {
    return truePart();
  }
  return falsePart();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical and
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_AND (lhs, rhs) {
  "use strict";

  var l = lhs();

  if (! l) {
    return l;
  }

  return rhs();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical or
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_OR (lhs, rhs) {
  "use strict";

  var l = lhs();

  if (l) {
    return l;
  }

  return rhs();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical negation
///
/// the operand must be a boolean values, returns a boolean
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_NOT (lhs) {
  "use strict";

  return ! AQL_TO_BOOL(lhs);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             comparison operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform equality check
///
/// returns true if the operands are equal, false otherwise
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_EQUAL (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight !== rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i];
      if (RELATIONAL_EQUAL(lhs[key], rhs[key]) === false) {
        return false;
      }
    }
    return true;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return true;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) === 0;
  }

  return (lhs === rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform inequality check
///
/// returns true if the operands are unequal, false otherwise
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_UNEQUAL (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight !== rightWeight) {
    return true;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i];
      if (RELATIONAL_UNEQUAL(lhs[key], rhs[key]) === true) {
        return true;
      }
    }

    return false;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return false;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) !== 0;
  }

  return (lhs !== rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATER_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_GREATER_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }

    return null;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) > 0;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs > rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check
///
/// returns true if the left operand is greater than the right operand
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATER (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_GREATER_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater equal check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATEREQUAL_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_GREATEREQUAL_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }

    return null;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) >= 0;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs >= rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater equal check
///
/// returns true if the left operand is greater or equal to the right operand
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATEREQUAL (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_GREATEREQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESS_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_LESS_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }

    return null;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) < 0;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs < rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check
///
/// returns true if the left operand is less than the right operand
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESS (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_LESS_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less equal check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESSEQUAL_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_LESSEQUAL_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }

    return null;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) <= 0;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs <= rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less equal check
///
/// returns true if the left operand is less or equal to the right operand
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESSEQUAL (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_LESSEQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform comparison
///
/// returns -1 if the left operand is less than the right operand, 1 if it is
/// greater, 0 if both operands are equal
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_CMP (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight < rightWeight) {
    return -1;
  }
  if (leftWeight > rightWeight) {
    return 1;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_CMP(lhs[key], rhs[key]);
      if (result !== 0) {
        return result;
      }
    }

    return 0;
  }

  // primitive type
  if (leftWeight === TYPEWEIGHT_NULL) {
    return 0;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs);
  }

  if (lhs < rhs) {
    return -1;
  }

  if (lhs > rhs) {
    return 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform in list check
///
/// returns true if the left operand is contained in the right operand
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_IN (lhs, rhs) {
  "use strict";

  var rightWeight = TYPEWEIGHT(rhs);

  if (rightWeight !== TYPEWEIGHT_LIST) {
    WARN(null, INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return false;
  }

  var numRight = rhs.length, i;
  for (i = 0; i < numRight; ++i) {
    if (RELATIONAL_EQUAL(lhs, rhs[i])) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform not-in list check
///
/// returns true if the left operand is not contained in the right operand
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_NOT_IN (lhs, rhs) {
  "use strict";

  return ! RELATIONAL_IN(lhs, rhs);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             arithmetic operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary plus operation
////////////////////////////////////////////////////////////////////////////////

function UNARY_PLUS (value) {
  "use strict";

  value = AQL_TO_NUMBER(value);
  if (value === null) {
    return null;
  } 
  return AQL_TO_NUMBER(+ value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary minus operation
////////////////////////////////////////////////////////////////////////////////

function UNARY_MINUS (value) {
  "use strict";

  value = AQL_TO_NUMBER(value);
  if (value === null) {
    return null;
  }

  return AQL_TO_NUMBER(- value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic plus or string concatenation
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_PLUS (lhs, rhs) {
  "use strict";

  var lWeight = TYPEWEIGHT(lhs);
  var rWeight = TYPEWEIGHT(rhs);

  if (lWeight === TYPEWEIGHT_STRING || 
      lWeight === TYPEWEIGHT_LIST || 
      lWeight === TYPEWEIGHT_DOCUMENT ||
      rWeight === TYPEWEIGHT_STRING ||
      rWeight === TYPEWEIGHT_LIST || 
      rWeight === TYPEWEIGHT_DOCUMENT) {
    // string concatenation
    return AQL_TO_STRING(lhs) + AQL_TO_STRING(rhs);
  }
  
  lhs = AQL_TO_NUMBER(lhs);
  if (lhs === null) {
    return null;
  }

  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === null) {
    return null;
  }

  return AQL_TO_NUMBER(lhs + rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic minus
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MINUS (lhs, rhs) {
  "use strict";
  
  lhs = AQL_TO_NUMBER(lhs);
  if (lhs === null) {
    return null;
  }

  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === null) {
    return null;
  }

  return AQL_TO_NUMBER(lhs - rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic multiplication
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_TIMES (lhs, rhs) {
  "use strict";
  
  lhs = AQL_TO_NUMBER(lhs);
  if (lhs === null) {
    return null;
  }

  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === null) {
    return null;
  }

  return AQL_TO_NUMBER(lhs * rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic division
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_DIVIDE (lhs, rhs) {
  "use strict";

  lhs = AQL_TO_NUMBER(lhs);
  if (lhs === null) {
    return null;
  }

  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === 0 || rhs === null) {
    WARN(null, INTERNAL.errors.ERROR_QUERY_DIVISION_BY_ZERO);
    return null;
  }

  return AQL_TO_NUMBER(lhs / rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic modulus
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MODULUS (lhs, rhs) {
  "use strict";

  lhs = AQL_TO_NUMBER(lhs);
  if (lhs === null) {
    return null;
  }

  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === 0 || rhs === null) {
    WARN(null, INTERNAL.errors.ERROR_QUERY_DIVISION_BY_ZERO);
    return null;
  }

  return AQL_TO_NUMBER(lhs % rhs);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  string functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation
////////////////////////////////////////////////////////////////////////////////

function AQL_CONCAT () {
  "use strict";

  var result = '', i;

  for (i = 0; i < arguments.length; ++i) {
    var element = arguments[i];
    if (TYPEWEIGHT(element) === TYPEWEIGHT_NULL) {
      continue;
    }
    result += AQL_TO_STRING(element);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation using a separator character
////////////////////////////////////////////////////////////////////////////////

function AQL_CONCAT_SEPARATOR () {
  "use strict";

  var separator, found = false, result = '', i;

  for (i = 0; i < arguments.length; ++i) {
    var element = arguments[i];

    if (i > 0 && TYPEWEIGHT(element) === TYPEWEIGHT_NULL) {
      continue;
    }

    if (i === 0) {
      separator = AQL_TO_STRING(element);
      continue;
    }
    else if (found) {
      result += separator;
    }

    found = true;
    result += AQL_TO_STRING(element);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the length of a string in characters (not bytes)
////////////////////////////////////////////////////////////////////////////////

function AQL_CHAR_LENGTH (value) {
  "use strict";

  return AQL_TO_STRING(value).length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
////////////////////////////////////////////////////////////////////////////////

function AQL_LOWER (value) {
  "use strict";

  return AQL_TO_STRING(value).toLowerCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
////////////////////////////////////////////////////////////////////////////////

function AQL_UPPER (value) {
  "use strict";

  return AQL_TO_STRING(value).toUpperCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a substring of the string
////////////////////////////////////////////////////////////////////////////////

function AQL_SUBSTRING (value, offset, count) {
  "use strict";

  if (count !== undefined) {
    count = AQL_TO_NUMBER(count);
  }

  return AQL_TO_STRING(value).substr(AQL_TO_NUMBER(offset), count);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief searches a substring in a string
////////////////////////////////////////////////////////////////////////////////

function AQL_CONTAINS (value, search, returnIndex) {
  "use strict";

  search = AQL_TO_STRING(search);

  var result;
  if (search.length === 0) {
    result = -1;
  }
  else {
    result = AQL_TO_STRING(value).indexOf(search);
  }

  if (AQL_TO_BOOL(returnIndex)) {
    return result;
  }

  return (result !== -1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief searches a substring in a string, using a regex
////////////////////////////////////////////////////////////////////////////////

function AQL_LIKE (value, regex, caseInsensitive) {
  "use strict";

  var modifiers = '';
  if (caseInsensitive) {
    modifiers += 'i';
  }

  regex = AQL_TO_STRING(regex);

  if (RegexCache[modifiers][regex] === undefined) {
    RegexCache[modifiers][regex] = COMPILE_REGEX(regex, modifiers);
  }

  try {
    return RegexCache[modifiers][regex].test(AQL_TO_STRING(value));
  }
  catch (err) {
    WARN("LIKE", INTERNAL.errors.ERROR_QUERY_INVALID_REGEX);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the leftmost parts of a string
////////////////////////////////////////////////////////////////////////////////

function AQL_LEFT (value, length) {
  "use strict";

  return AQL_TO_STRING(value).substr(0, AQL_TO_NUMBER(length));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the rightmost parts of a string
////////////////////////////////////////////////////////////////////////////////

function AQL_RIGHT (value, length) {
  "use strict";

  value = AQL_TO_STRING(value);
  length = AQL_TO_NUMBER(length);

  var left = value.length - length;
  if (left < 0) {
    left = 0;
  }

  return value.substr(left, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a trimmed version of a string
////////////////////////////////////////////////////////////////////////////////

function AQL_TRIM (value, type) {
  "use strict";

  type = AQL_TO_NUMBER(type);

  if (type < 0 || type > 2) { 
    type = 0;
  }

  var result;
  if (type === 1) {
    // TRIM(, 1)
    result = AQL_TO_STRING(value).replace(/^\s+/g, '');
  }
  else if (type === 2) {
    // TRIM(, 2)
    result = AQL_TO_STRING(value).replace(/\s+$/g, '');
  }
  else {
    // TRIM(, 0)
    result = AQL_TO_STRING(value).replace(/(^\s+)|\s+$/g, '');
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds search in value
////////////////////////////////////////////////////////////////////////////////

function AQL_FIND_FIRST (value, search, start, end) {
  "use strict";

  if (start !== undefined && start !== null) {
    start = AQL_TO_NUMBER(start);
    if (start < 0) {
      return -1;
    }
  }
  else {
    start = 0;
  }

  if (end !== undefined && end !== null) {
    end = AQL_TO_NUMBER(end);
    if (end < start || end < 0) {
      return -1;
    }
  }
  else {
    end = undefined;
  }

  if (end !== undefined) {
    return AQL_TO_STRING(value).substr(0, end + 1).indexOf(AQL_TO_STRING(search), start);
  }

  return AQL_TO_STRING(value).indexOf(AQL_TO_STRING(search), start);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds search in value
////////////////////////////////////////////////////////////////////////////////

function AQL_FIND_LAST (value, search, start, end) {
  "use strict";

  if (start !== undefined) {
    start = AQL_TO_NUMBER(start);
  }
  else {
    start = 0;
  }
  if (end !== undefined) {
    end = AQL_TO_NUMBER(end);
    if (end < start || end < 0) {
      return -1;
    }
  }
  else {
    end = undefined;
  }

  if (end !== undefined && end < search.length) {
    return AQL_TO_STRING(value).lastIndexOf(AQL_TO_STRING(search).substr(0, end + 1), start);
  }

  return AQL_TO_STRING(value).lastIndexOf(AQL_TO_STRING(search), start);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                typecast functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a bool
///
/// the operand can have any type, always returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_TO_BOOL (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return false;
    case TYPEWEIGHT_BOOL:
      return value;
    case TYPEWEIGHT_NUMBER:
      return (value !== 0);
    case TYPEWEIGHT_STRING:
      return (value !== '');
    case TYPEWEIGHT_LIST:
    case TYPEWEIGHT_DOCUMENT:
      return true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a number
///
/// the operand can have any type, returns a number or null
////////////////////////////////////////////////////////////////////////////////

function AQL_TO_NUMBER (value) {
  "use strict";

  if (value === undefined) {
    return null;
  }
  if (value === null) {
    return 0;
  }

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      // this covers Infinity and NaN
      return null;
    case TYPEWEIGHT_BOOL:
      return (value ? 1 : 0);
    case TYPEWEIGHT_NUMBER:
      return value;
    case TYPEWEIGHT_STRING:
      var result = NUMERIC_VALUE(Number(value));
      return ((TYPEWEIGHT(result) === TYPEWEIGHT_NUMBER) ? result : null);
    case TYPEWEIGHT_LIST:
      if (value.length === 0) {
        return 0;
      }
      if (value.length === 1) {
        return AQL_TO_NUMBER(value[0]);
      }
      // fallthrough intentional
  }
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a string
///
/// the operand can have any type, always returns a string
////////////////////////////////////////////////////////////////////////////////

function AQL_TO_STRING (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return 'null';
    case TYPEWEIGHT_BOOL:
      return (value ? 'true' : 'false');
    case TYPEWEIGHT_STRING:
      return value;
    case TYPEWEIGHT_NUMBER:
    case TYPEWEIGHT_LIST:
    case TYPEWEIGHT_DOCUMENT:
      return value.toString();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a list
///
/// the operand can have any type, always returns a list
////////////////////////////////////////////////////////////////////////////////

function AQL_TO_LIST (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return [ ];
    case TYPEWEIGHT_BOOL:
    case TYPEWEIGHT_NUMBER:
    case TYPEWEIGHT_STRING:
      return [ value ];
    case TYPEWEIGHT_LIST:
      return value;
    case TYPEWEIGHT_DOCUMENT:
      return VALUES(value);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                               typecheck functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type null
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_NULL (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type bool
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_BOOL (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type number
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_NUMBER (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NUMBER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type string
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_STRING (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_STRING);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type list
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_LIST (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_LIST);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type document
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_DOCUMENT (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_DOCUMENT);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 numeric functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value, not greater than value
////////////////////////////////////////////////////////////////////////////////

function AQL_FLOOR (value) {
  "use strict";

  return NUMERIC_VALUE(Math.floor(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value and not less than value
////////////////////////////////////////////////////////////////////////////////

function AQL_CEIL (value) {
  "use strict";

  return NUMERIC_VALUE(Math.ceil(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value
////////////////////////////////////////////////////////////////////////////////

function AQL_ROUND (value) {
  "use strict";

  return NUMERIC_VALUE(Math.round(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief absolute value
////////////////////////////////////////////////////////////////////////////////

function AQL_ABS (value) {
  "use strict";

  return NUMERIC_VALUE(Math.abs(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a random value between 0 and 1
////////////////////////////////////////////////////////////////////////////////

function AQL_RAND () {
  "use strict";

  return Math.random();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief square root
////////////////////////////////////////////////////////////////////////////////

function AQL_SQRT (value) {
  "use strict";

  return NUMERIC_VALUE(Math.sqrt(AQL_TO_NUMBER(value)));
}

// -----------------------------------------------------------------------------
// --SECTION--                                         list processing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of a list, document or string
////////////////////////////////////////////////////////////////////////////////

function AQL_LENGTH (value) {
  "use strict";

  var result, typeWeight = TYPEWEIGHT(value);

  if (typeWeight === TYPEWEIGHT_LIST) {
    return value.length;
  }
  else if (typeWeight === TYPEWEIGHT_DOCUMENT) {
    return KEYS(value, false).length;
  }

  return AQL_TO_STRING(value).length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the first element of a list
////////////////////////////////////////////////////////////////////////////////

function AQL_FIRST (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_LIST) {
    WARN("FIRST", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  if (value.length === 0) {
    return null;
  }

  return value[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last element of a list
////////////////////////////////////////////////////////////////////////////////

function AQL_LAST (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_LIST) {
    WARN("LAST", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  if (value.length === 0) {
    return null;
  }

  return value[value.length - 1];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the position of an element in a list
////////////////////////////////////////////////////////////////////////////////

function AQL_POSITION (value, search, returnIndex) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_LIST) {
    WARN("POSITION", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  returnIndex = returnIndex || false;

  var i, n = value.length;

  if (n > 0) {
    for (i = 0; i < n; ++i) {
      if (RELATIONAL_EQUAL(value[i], search)) {
        return returnIndex ? i : true;
      }
    }
  }

  return returnIndex ? -1 : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the nth element in a list, or null if the item does not exist
////////////////////////////////////////////////////////////////////////////////

function AQL_NTH (value, position) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_LIST) {
    WARN("NTH", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  position = AQL_TO_NUMBER(position);
  if (position < 0 || position >= value.length) {
    return null;
  }

  return value[position];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reverse the elements in a list or in a string
////////////////////////////////////////////////////////////////////////////////

function AQL_REVERSE (value) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    return value.split("").reverse().join("");
  }

  if (TYPEWEIGHT(value) === TYPEWEIGHT_LIST) {
    return CLONE(value).reverse();
  }
    
  WARN("REVERSE", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a range of values
////////////////////////////////////////////////////////////////////////////////

function AQL_RANGE (from, to, step) {
  "use strict";

  from = AQL_TO_NUMBER(from);
  to = AQL_TO_NUMBER(to);
  
  if (step === undefined) {
    if (from <= to) {
      step = 1;
    }
    else {
      step = -1;
    }
  }
  
  step = AQL_TO_NUMBER(step);

  // check if we would run into an endless loop
  if (step === 0) {
    WARN("RANGE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  if (from < to && step < 0) {
    WARN("RANGE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  if (from > to && step > 0) {
    WARN("RANGE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = [ ], i;
  if (step < 0 && to <= from) {
    for (i = from; i >= to; i += step) {
      result.push(i);
    }
  }
  else {
    for (i = from; i <= to; i += step) {
      result.push(i);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of unique elements from the list
////////////////////////////////////////////////////////////////////////////////

function AQL_UNIQUE (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("UNIQUE", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var keys = { }, result = [ ];

  values.forEach(function (value) {
    var normalized = NORMALIZE(value);
    var key = JSON.stringify(normalized);

    if (! keys.hasOwnProperty(key)) {
      keys[key] = normalized;
    }
  });

  Object.keys(keys).forEach(function(k) {
    result.push(keys[k]);
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union (all) of all arguments
////////////////////////////////////////////////////////////////////////////////

function AQL_UNION () {
  "use strict";

  var result = [ ], i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        WARN("UNION", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        result.push(element[j]);
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union (distinct) of all arguments
////////////////////////////////////////////////////////////////////////////////

function AQL_UNION_DISTINCT () {
  "use strict";

  var keys = { }, i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        WARN("UNION_DISTINCT", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        var normalized = NORMALIZE(element[j]);
        var key = JSON.stringify(normalized);

        if (! keys.hasOwnProperty(key)) {
          keys[key] = normalized;
        }
      }
    }
  }

  var result = [ ];
  Object.keys(keys).forEach(function(k) {
    result.push(keys[k]);
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a slice from a list
////////////////////////////////////////////////////////////////////////////////

function AQL_SLICE (value, from, to) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_LIST) {
    WARN("SLICE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  from = AQL_TO_NUMBER(from);
  to   = AQL_TO_NUMBER(to);

  if (TYPEWEIGHT(to) === TYPEWEIGHT_NULL) {
    to = undefined;
  }
  else {
    if (to >= 0) {
      to += from;
    }
  }

  return value.slice(from, to);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief subtract lists from other lists
////////////////////////////////////////////////////////////////////////////////

function AQL_MINUS () {
  "use strict";

  var keys = { }, i, first = true;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        WARN("MINUS", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        var normalized = NORMALIZE(element[j]);
        var key = JSON.stringify(normalized);
        var contained = keys.hasOwnProperty(key);

        if (first) {
          if (! contained) {
            keys[key] = normalized;
          }
        }
        else if (contained) {
          delete keys[key];
        }
      }

      first = false;
    }
  }

  var result = [ ];
  Object.keys(keys).forEach(function(k) {
    result.push(keys[k]);
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the intersection of all arguments
////////////////////////////////////////////////////////////////////////////////

function AQL_INTERSECTION () {
  "use strict";

  var result = [ ], i, first = true, keys = { };

  var func = function (value) {
    var normalized = NORMALIZE(value);
    keys[JSON.stringify(normalized)] = normalized;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        WARN("INTERSECTION", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      if (first) {
        element.forEach(func);
        first = false;
      }
      else {
        var j, newKeys = { };
        for (j = 0; j < element.length; ++j) {
          var normalized = NORMALIZE(element[j]);
          var key = JSON.stringify(normalized);

          if (keys.hasOwnProperty(key)) {
            newKeys[key] = normalized;
          }
        }
        keys = newKeys;
        newKeys = null;
      }

    }
  }

  Object.keys(keys).forEach(function(k) {
    result.push(keys[k]);
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flatten a list of lists
////////////////////////////////////////////////////////////////////////////////

function AQL_FLATTEN (values, maxDepth, depth) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("FLATTEN", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  maxDepth = AQL_TO_NUMBER(maxDepth);
  if (TYPEWEIGHT(maxDepth) === TYPEWEIGHT_NULL || maxDepth < 1) {
    maxDepth = 1;
  }

  if (TYPEWEIGHT(depth) === TYPEWEIGHT_NULL) {
    depth = 0;
  }

  var value, result = [ ];
  var i, n;
  var p = function(v) {
    result.push(v);
  };

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    if (depth < maxDepth && TYPEWEIGHT(value) === TYPEWEIGHT_LIST) {
      AQL_FLATTEN(value, maxDepth, depth + 1).forEach(p);
    }
    else {
      result.push(value);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_MAX (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("MAX", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var value, result = null;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    if (TYPEWEIGHT(value) !== TYPEWEIGHT_NULL) {
      if (result === null || RELATIONAL_GREATER(value, result)) {
        result = value;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_MIN (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("MIN", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var value, result = null;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    if (TYPEWEIGHT(value) !== TYPEWEIGHT_NULL) {
      if (result === null || RELATIONAL_LESS(value, result)) {
        result = value;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sum of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_SUM (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("SUM", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var i, n;
  var value, typeWeight, result = 0;

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    typeWeight = TYPEWEIGHT(value);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN("SUM", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }
      result += value;
    }
  }

  return NUMERIC_VALUE(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief average of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_AVERAGE (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("AVERAGE", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var current, typeWeight, sum = 0;
  var i, j, n;

  for (i = 0, j = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN("AVERAGE", INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
        return null;
      }

      sum += current;
      j++;
    }
  }

  if (j === 0) {
    return null;
  }

  return NUMERIC_VALUE(sum / j);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief median of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_MEDIAN (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("MEDIAN", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var copy = [ ], current, typeWeight;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN("MEDIAN", INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
        return null;
      }

      copy.push(current);
    }
  }

  if (copy.length === 0) {
    return null;
  }

  copy.sort(RELATIONAL_CMP);

  var midpoint = copy.length / 2;
  if (copy.length % 2 === 0) {
    return NUMERIC_VALUE((copy[midpoint - 1] + copy[midpoint]) / 2);
  }

  return NUMERIC_VALUE(copy[Math.floor(midpoint)]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief variance of all values
////////////////////////////////////////////////////////////////////////////////

function VARIANCE (values) {
  "use strict";

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_LIST) {
    WARN("VARIANCE", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  var mean = 0, m2 = 0, current, typeWeight, delta;
  var i, j, n;

  for (i = 0, j = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN("VARIANCE", INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
        return null;
      }

      delta = current - mean;
      mean += delta / ++j;
      m2 += delta * (current - mean);
    }
  }

  return {
    n: j,
    value: m2
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sample variance of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_VARIANCE_SAMPLE (values) {
  "use strict";

  var result = VARIANCE(values);
  if (result === null || result === undefined) {
    return null;
  }

  if (result.n < 2) {
    return null;
  }

  return NUMERIC_VALUE(result.value / (result.n - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief population variance of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_VARIANCE_POPULATION (values) {
  "use strict";

  var result = VARIANCE(values);
  if (result === null || result === undefined) {
    return null;
  }

  if (result.n < 1) {
    return null;
  }

  return NUMERIC_VALUE(result.value / result.n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief standard deviation of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_STDDEV_SAMPLE (values) {
  "use strict";

  var result = VARIANCE(values);
  if (result === null || result === undefined) {
    return null;
  }

  if (result.n < 2) {
    return null;
  }

  return NUMERIC_VALUE(Math.sqrt(result.value / (result.n - 1)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief standard deviation of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_STDDEV_POPULATION (values) {
  "use strict";

  var result = VARIANCE(values);
  if (result === null || result === undefined) {
    return null;
  }

  if (result.n < 1) {
    return null;
  }

  return NUMERIC_VALUE(Math.sqrt(result.value / result.n));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     geo functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return at most <limit> documents near a certain point
////////////////////////////////////////////////////////////////////////////////

function AQL_NEAR (collection, latitude, longitude, limit, distanceAttribute) {
  "use strict";

  if (limit === null || limit === undefined) {
    // use default value
    limit = 100;
  }
  else {
    limit = AQL_TO_NUMBER(limit);
  }

  var weight = TYPEWEIGHT(distanceAttribute);
  if (weight !== TYPEWEIGHT_NULL && weight !== TYPEWEIGHT_STRING) {
    WARN("NEAR", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }

  if (isCoordinator) {
    var query = COLLECTION(collection).near(latitude, longitude);
    query._distance = distanceAttribute;
    return query.limit(limit).toArray();
  }

  var idx = INDEX(COLLECTION(collection), [ "geo1", "geo2" ]);

  if (idx === null) {
    THROW("NEAR", INTERNAL.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  var result = COLLECTION(collection).NEAR(idx.id, latitude, longitude, limit);

  if (distanceAttribute === null || distanceAttribute === undefined) {
    return result.documents;
  }

  distanceAttribute = AQL_TO_STRING(distanceAttribute);

  // inject distances
  var documents = result.documents;
  var distances = result.distances;
  var n = documents.length, i;
  for (i = 0; i < n; ++i) {
    documents[i][distanceAttribute] = distances[i];
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return documents within <radius> around a certain point
////////////////////////////////////////////////////////////////////////////////

function AQL_WITHIN (collection, latitude, longitude, radius, distanceAttribute) {
  "use strict";

  if (isCoordinator) {
    var query = COLLECTION(collection).within(latitude, longitude, radius);
    query._distance = distanceAttribute;
    return query.toArray();
  }
  
  var weight = TYPEWEIGHT(distanceAttribute);
  if (weight !== TYPEWEIGHT_NULL && weight !== TYPEWEIGHT_STRING) {
    WARN("WITHIN", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }
    
  var idx = INDEX(COLLECTION(collection), [ "geo1", "geo2" ]);

  if (idx === null) {
    THROW("WITHIN", INTERNAL.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  var result = COLLECTION(collection).WITHIN(idx.id, latitude, longitude, radius);

  if (distanceAttribute === null || distanceAttribute === undefined) {
    return result.documents;
  }

  // inject distances
  var documents = result.documents;
  var distances = result.distances;
  var n = documents.length, i;
  for (i = 0; i < n; ++i) {
    documents[i][distanceAttribute] = distances[i];
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return documents within a bounding rectangle 
////////////////////////////////////////////////////////////////////////////////

function AQL_WITHIN_RECTANGLE (collection, latitude1, longitude1, latitude2, longitude2) {
  "use strict";

  if (TYPEWEIGHT(latitude1) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(longitude1) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(latitude2) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(longitude2) !== TYPEWEIGHT_NUMBER) {
    WARN("WITHIN_RECTANGLE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var distanceMeters = function (lat1, lon1, lat2, lon2) {  
    var deltaLat = (lat2 - lat1) * Math.PI / 180;
    var deltaLon = (lon2 - lon1) * Math.PI / 180;
    var a = Math.sin(deltaLat / 2) * Math.sin(deltaLat / 2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(deltaLon / 2) * Math.sin(deltaLon / 2);
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));

    return 6378.137 /* radius of earth in kilometers */  
           * c 
           * 1000; // kilometers to meters;
  };

  var midpoint = [ 
    latitude1 + (latitude2 - latitude1) * 0.5, 
    longitude1 + (longitude2 - longitude1) * 0.5 
  ];

  var idx = INDEX(COLLECTION(collection), [ "geo1", "geo2" ]); 

  if (idx === null) {
    THROW(INTERNAL.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  var diameter = distanceMeters(latitude1, longitude1, latitude2, longitude2);
  var latLower, latUpper, lonLower, lonUpper;

  if (latitude1 < latitude2) {
    latLower = latitude1;
    latUpper = latitude2;
  }
  else {
    latLower = latitude2;
    latUpper = latitude1;
  }

  if (longitude1 < longitude2) {
    lonLower = longitude1;
    lonUpper = longitude2;
  }
  else {
    lonLower = longitude2;
    lonUpper = longitude1;
  }

  var result = COLLECTION(collection).WITHIN(idx.id, midpoint[0], midpoint[1], diameter);

  var documents = [ ];
  if (idx.type === 'geo1') {
    // geo1, we have both coordinates in a list
    var attribute = idx.fields[0];
    if (idx.geoJson) {
      result.documents.forEach(function(doc) {
        // check if within bounding rectangle
        // first list value is longitude, then latitude
        if (doc[attribute][1] >= latLower && doc[attribute][1] <= latUpper &&
            doc[attribute][0] >= lonLower && doc[attribute][0] <= lonUpper) {
          documents.push(doc);
        }
      });
    }
    else {
      result.documents.forEach(function(doc) {
        // check if within bounding rectangle
        // first list value is latitude, then longitude
        if (doc[attribute][0] >= latLower && doc[attribute][0] <= latUpper &&
            doc[attribute][1] >= lonLower && doc[attribute][1] <= lonUpper) {
          documents.push(doc);
        }
      });
    }
  }
  else {
    // geo2, we have dedicated latitude and longitude attributes
    var latAtt = idx.fields[0], lonAtt = idx.fields[1];
    result.documents.forEach(function(doc) {
      // check if within bounding rectangle
      if (doc[latAtt] >= latLower && doc[latAtt] <= latUpper &&
          doc[lonAtt] >= lonLower && doc[lonAtt] <= lonUpper) {
        documents.push(doc);
      }
    });
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return true if a point is contained inside a polygon
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_IN_POLYGON (points, latitude, longitude) {
  "use strict";
  
  if (TYPEWEIGHT(points) !== TYPEWEIGHT_LIST) {
    WARN("POINT_IN_POLYGON", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return false;
  }

  var searchLat, searchLon, pointLat, pointLon, geoJson = false;
  if (TYPEWEIGHT(latitude) === TYPEWEIGHT_LIST) {
    geoJson = AQL_TO_BOOL(longitude);
    if (geoJson) {
      // first list value is longitude, then latitude
      searchLat = latitude[1];
      searchLon = latitude[0];
      pointLat = 1;
      pointLon = 0;
    }
    else {
      // first list value is latitude, then longitude
      searchLat = latitude[0];
      searchLon = latitude[1];
      pointLat = 0;
      pointLon = 1;
    }
  }
  else if (TYPEWEIGHT(latitude) === TYPEWEIGHT_NUMBER &&
           TYPEWEIGHT(longitude) === TYPEWEIGHT_NUMBER) {
    searchLat = latitude;
    searchLon = longitude;
    pointLat = 0;
    pointLon = 1;
  }
  else {
    WARN("POINT_IN_POLYGON", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }
  
  var i, j = points.length - 1;
  var oddNodes = false;

  for (i = 0; i < points.length; ++i) {
    if (TYPEWEIGHT(points[i]) !== TYPEWEIGHT_LIST) {
      continue;
    }

    if (((points[i][pointLat] < searchLat && points[j][pointLat] >= searchLat) || 
         (points[j][pointLat] < searchLat && points[i][pointLat] >= searchLat)) &&
        (points[i][pointLon] <= searchLon || points[j][pointLon] <= searchLon)) {
      oddNodes ^= ((points[i][pointLon] + (searchLat - points[i][pointLat]) / 
                   (points[j][pointLat] - points[i][pointLat]) * 
                    (points[j][pointLon] - points[i][pointLon])) < searchLon);
    }

    j = i;
  }

  if (oddNodes) {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                fulltext functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return documents that match a fulltext query
////////////////////////////////////////////////////////////////////////////////

function AQL_FULLTEXT (collection, attribute, query) {
  "use strict";

  var idx = INDEX_FULLTEXT(COLLECTION(collection), attribute);

  if (idx === null) {
    THROW("FULLTEXT", INTERNAL.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING, collection);
  }

  if (isCoordinator) {
    return COLLECTION(collection).fulltext(attribute, query, idx).toArray();
  }

  return COLLECTION(collection).FULLTEXT(idx, query).documents;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    misc functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first alternative that's not null until there are no more
/// alternatives. if neither of the alternatives is a value other than null,
/// then null will be returned
///
/// the operands can have any type
////////////////////////////////////////////////////////////////////////////////

function AQL_NOT_NULL () {
  "use strict";

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_NULL) {
        return element;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first alternative that's a list until there are no more
/// alternatives. if neither of the alternatives is a list, then null will be
/// returned
///
/// the operands can have any type
////////////////////////////////////////////////////////////////////////////////

function AQL_FIRST_LIST () {
  "use strict";

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) === TYPEWEIGHT_LIST) {
        return element;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first alternative that's a document until there are no
/// more alternatives. if neither of the alternatives is a document, then null
/// will be returned
///
/// the operands can have any type
////////////////////////////////////////////////////////////////////////////////

function AQL_FIRST_DOCUMENT () {
  "use strict";

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) === TYPEWEIGHT_DOCUMENT) {
        return element;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the parts of a document identifier separately
///
/// returns a document with the attributes `collection` and `key` or fails if
/// the individual parts cannot be determined.
////////////////////////////////////////////////////////////////////////////////

function AQL_PARSE_IDENTIFIER (value) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    var parts = value.split('/');
    if (parts.length === 2) {
      return {
        collection: parts[0],
        key: parts[1]
      };
    }
    // fall through intentional
  }
  else if (TYPEWEIGHT(value) === TYPEWEIGHT_DOCUMENT) {
    if (value.hasOwnProperty('_id')) {
      return AQL_PARSE_IDENTIFIER(value._id);
    }
    // fall through intentional
  }

  WARN("PARSE_IDENTIFIER", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief query a skiplist index
///
/// returns a documents from a skiplist index on the specified collection. Only
/// documents that match the specified condition will be returned.
////////////////////////////////////////////////////////////////////////////////

function AQL_SKIPLIST (collection, condition, skip, limit) {
  "use strict";

  var keys = [ ], key, idx;

  for (key in condition) {
    if (condition.hasOwnProperty(key)) {
      keys.push(key);
    }
  }

  var c = COLLECTION(collection);

  if (c === null) {
    THROW("SKIPLIST", INTERNAL.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND, collection);
  }

  idx = c.lookupSkiplist.apply(c, keys);

  if (idx === null) {
    THROW("SKIPLIST", INTERNAL.errors.ERROR_ARANGO_NO_INDEX);
  }

  if (skip === undefined || skip === null) {
    skip = 0;
  }

  if (limit === undefined || limit === null) {
    limit = null;
  }

  try {
    if (isCoordinator) {
      return c.byConditionSkiplist(idx.id, condition).skip(skip).limit(limit).toArray();
    }

    return c.BY_CONDITION_SKIPLIST(idx.id, condition, skip, limit).documents;
  }
  catch (err) {
    THROW("SKIPLIST", INTERNAL.errors.ERROR_ARANGO_NO_INDEX);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a document has a specific attribute
////////////////////////////////////////////////////////////////////////////////

function AQL_HAS (element, name) {
  "use strict";
 
  var weight = TYPEWEIGHT(element);
  if (weight === TYPEWEIGHT_NULL) {
    return false;
  }

  if (weight !== TYPEWEIGHT_DOCUMENT) {
    WARN("HAS", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return;
  }

  return element.hasOwnProperty(AQL_TO_STRING(name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the attribute names of a document as a list
////////////////////////////////////////////////////////////////////////////////

function AQL_ATTRIBUTES (element, removeInternal, sort) {
  "use strict";

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
    WARN("ATTRIBUTES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (removeInternal) {
    var result = [ ];

    Object.keys(element).forEach(function(k) {
      if (k.substring(0, 1) !== '_') {
        result.push(k);
      }
    });

    if (sort) {
      result.sort();
    }

    return result;
  }

  return KEYS(element, sort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unset specific attributes from a document
////////////////////////////////////////////////////////////////////////////////

function AQL_UNSET (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_DOCUMENT) {
    WARN("UNSET", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, "UNSET");
  // copy over all that is left

  Object.keys(value).forEach(function(k) {
    if (keys[k] !== true) {
      result[k] = CLONE(value[k]);
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief keep specific attributes from a document
////////////////////////////////////////////////////////////////////////////////

function AQL_KEEP (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_DOCUMENT) {
    WARN("KEEP", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, "KEEP");

  // copy over all that is left
  Object.keys(keys).forEach(function(k) {
    if (value.hasOwnProperty(k)) {
      result[k] = CLONE(value[k]);
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments
////////////////////////////////////////////////////////////////////////////////

function AQL_MERGE () {
  "use strict";

  var result = { }, i;

  var add = function (element) {
    Object.keys(element).forEach(function(k) {
      result[k] = element[k];
    });
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
        WARN("MERGE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      add(element);

    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments recursively
////////////////////////////////////////////////////////////////////////////////

function AQL_MERGE_RECURSIVE () {
  "use strict";

  var result = { }, i, recurse;

  recurse = function (old, element) {
    var r = CLONE(old);

    Object.keys(element).forEach(function(k) {
      if (r.hasOwnProperty(k) && TYPEWEIGHT(element[k]) === TYPEWEIGHT_DOCUMENT) {
        r[k] = recurse(r[k], element[k]);
      }
      else {
        r[k] = element[k];
      }
    });

    return r;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
        WARN("MERGE_RECURSIVE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      result = recurse(result, element);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a value, using a lookup document
////////////////////////////////////////////////////////////////////////////////

function AQL_TRANSLATE (value, lookup, defaultValue) {
  "use strict";

  if (TYPEWEIGHT(lookup) !== TYPEWEIGHT_DOCUMENT) {
    WARN("TRANSLATE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  
  if (defaultValue === undefined) {
    defaultValue = value;
  }

  var key = AQL_TO_STRING(value);
  if (lookup.hasOwnProperty(key)) {
    return lookup[key];
  }

  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare an object against a list of examples and return whether the
/// object matches any of the examples. returns the example index or a bool,
/// depending on the value of the control flag (3rd) parameter
////////////////////////////////////////////////////////////////////////////////

function AQL_MATCHES (element, examples, returnIndex) {
  "use strict";

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
    return false;
  }

  if (! Array.isArray(examples)) {
    examples = [ examples ];
  }
  if (examples.length === 0) {
    WARN("MATCHES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }

  returnIndex = returnIndex || false;
  var i;
  for (i = 0; i < examples.length; ++i) {
    var example = examples[i];
    var result = true;
    if (TYPEWEIGHT(example) !== TYPEWEIGHT_DOCUMENT) {
      WARN("MATCHES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      continue;
    }

    var keys = KEYS(example), key, j;
    for (j = 0; j < keys.length; ++j) {
      key = keys[j];

      if (! RELATIONAL_EQUAL(element[key], example[key])) {
        result = false;
        break;
      }
    }

    if (result) {
      // 3rd parameter determines whether we return the index or a bool flag
      return (returnIndex ? i : true);
    }
  }

  return (returnIndex ? -1 : false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief passthru the argument
///
/// this function is marked as non-deterministic so its argument withstands
/// query optimisation. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function AQL_PASSTHRU (value) {
  "use strict";

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep
///
/// sleep for the specified duration
////////////////////////////////////////////////////////////////////////////////

function AQL_SLEEP (duration) {
  "use strict";

  duration = AQL_TO_NUMBER(duration);
  if (TYPEWEIGHT(duration) !== TYPEWEIGHT_NUMBER || duration < 0) {
    WARN("SLEEP", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  INTERNAL.sleep(duration);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current user
/// note: this might be null if the query is not executed in a context that
/// has a user
////////////////////////////////////////////////////////////////////////////////

function AQL_CURRENT_USER () {
  "use strict";

  if (INTERNAL.getCurrentRequest) {
    var req = INTERNAL.getCurrentRequest();

    if (typeof req === 'object') {
      return req.user;
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current database name
/// has a user
////////////////////////////////////////////////////////////////////////////////

function AQL_CURRENT_DATABASE () {
  "use strict";

  return INTERNAL.db._name();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief always fail
///
/// this function is non-deterministic so it is not executed at query
/// optimisation time. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function AQL_FAIL (message) {
  "use strict";

  if (TYPEWEIGHT(message) === TYPEWEIGHT_STRING) {
    THROW("FAIL", INTERNAL.errors.ERROR_QUERY_FAIL_CALLED, message);
  }

  THROW("FAIL", INTERNAL.errors.ERROR_QUERY_FAIL_CALLED, "");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    date functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for date creation
////////////////////////////////////////////////////////////////////////////////

function MAKE_DATE (args, func) {
  "use strict";

  var weight;
  var i, n = args.length;

  if (n === 1) {
    // called with one argument only
    weight = TYPEWEIGHT(args[0]);

    if (weight !== TYPEWEIGHT_NUMBER) {
      if (weight !== TYPEWEIGHT_STRING) {
        WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      // argument is a string

      // append zulu time specifier if no other present
      if (! args[0].match(/([zZ]|[+\-]\d+(:\d+)?)$/) ||
          (args[0].match(/-\d+(:\d+)?$/) && ! args[0].match(/[tT ]/))) {
        args[0] += 'Z';
      }
    }

    return new Date(args[0]);
  }

  // called with more than one argument

  if (n < 3) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, func);
    return null;
  }

  for (i = 0; i < n; ++i) {
    weight = TYPEWEIGHT(args[i]);

    if (weight === TYPEWEIGHT_NULL) {
      args[i] = 0;
    }
    else {
      if (weight === TYPEWEIGHT_STRING) {
        args[i] = parseInt(args[i], 10);
      }
      else if (weight !== TYPEWEIGHT_NUMBER) {
        WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      if (args[i] < 0) {
        WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE, func);
        return null;
      }

      if (i === 1) {
        // an exception for handling months: months are 0-based in JavaScript,
        // but 1-based in AQL
        args[i]--;
      }
    }
  }

  return new Date(Date.UTC.apply(null, args));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of milliseconds since the Unix epoch
///
/// this function is evaluated on every call
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_NOW () {
  "use strict";

  return Date.now();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the timestamp of the date passed (in milliseconds)
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_TIMESTAMP () {
  "use strict";

  try {
    return MAKE_DATE(arguments, "DATE_TIMESTAMP").getTime();
  }
  catch (err) {
    WARN("DATE_TIMESTAMP", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the ISO string representation of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_ISO8601 () {
  "use strict";

  try {
    return MAKE_DATE(arguments, "DATE_ISO8601").toISOString();
  }
  catch (err) {
    WARN("DATE_ISO8601", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the weekday of the date passed (0 = Sunday, 1 = Monday etc.)
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAYOFWEEK (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_DAYOFWEEK").getUTCDay();
  }
  catch (err) {
    WARN("DATE_DAYOFWEEK", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the year of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_YEAR (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_YEAR").getUTCFullYear();
  }
  catch (err) {
    WARN("DATE_YEAR", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the month of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_MONTH (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_MONTH").getUTCMonth() + 1;
  }
  catch (err) {
    WARN("DATE_MONTH", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the day of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAY (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_DAY").getUTCDate();
  }
  catch (err) {
    WARN("DATE_DAY", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the hours of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_HOUR (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_HOUR").getUTCHours();
  }
  catch (err) {
    WARN("DATE_HOUR", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the minutes of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_MINUTE (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_MINUTE").getUTCMinutes();
  }
  catch (err) {
    WARN("DATE_MINUTE", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the seconds of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_SECOND (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_SECOND").getUTCSeconds();
  }
  catch (err) {
    WARN("DATE_SECOND", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the milliseconds of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_MILLISECOND (value) {
  "use strict";

  try {
    return MAKE_DATE([ value ], "DATE_MILLISECOND").getUTCMilliseconds();
  }
  catch (err) {
    WARN("DATE_MILLISECOND", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   graph functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph, INTERNAL part called recursively
////////////////////////////////////////////////////////////////////////////////

function GET_SUB_EDGES (edgeCollections, direction, vertexId) {
  if (! Array.isArray(edgeCollections)) {
    edgeCollections = [edgeCollections];
  }

  var result = [];
  edgeCollections.forEach(function (edgeCollection) {
    if (direction === 1) {
      result = result.concat(edgeCollection.outEdges(vertexId));
    }
    else if (direction === 2) {
      result = result.concat(edgeCollection.inEdges(vertexId));
    }
    else if (direction === 3) {
      result = result.concat(edgeCollection.edges(vertexId));
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph, INTERNAL part called recursively
////////////////////////////////////////////////////////////////////////////////

function SUBNODES (searchAttributes, vertexId, visited, edges, vertices, level) {
  "use strict";

  var result = [ ];

  if (level >= searchAttributes.minLength) {
    result.push({
      vertices : vertices,
      edges : edges,
      source : vertices[0],
      destination : vertices[vertices.length - 1]
    });
  }

  if (level + 1 > searchAttributes.maxLength) {
    return result;
  }

  var subEdges = GET_SUB_EDGES(
    searchAttributes.edgeCollection, searchAttributes.direction, vertexId
  );

  var i, j, k;
  for (i = 0; i < subEdges.length; ++i) {
    var subEdge = subEdges[i];
    var targets = [ ];

    if ((searchAttributes.direction === 1 || searchAttributes.direction === 3) &&
        (subEdge._to !== vertexId)) {
      targets.push(subEdge._to);
    }
    if ((searchAttributes.direction === 2 || searchAttributes.direction === 3) &&
        (subEdge._from !== vertexId)) {
      targets.push(subEdge._from);
    }

    for (j = 0; j < targets.length; ++j) {
      var targetId = targets[j];

      if (! searchAttributes.followCycles) {
        if (visited[targetId]) {
          continue;
        }
        visited[targetId] = true;
      }

      var clonedEdges = CLONE(edges);
      var clonedVertices = CLONE(vertices);
      try {
        clonedVertices.push(INTERNAL.db._document(targetId));
        clonedEdges.push(subEdge);
      }
      catch (e) {
        // avoid "document not found error" in case referenced vertices were deleted
      }

      var connected = SUBNODES(searchAttributes,
                                     targetId,
                                     CLONE(visited),
                                     clonedEdges,
                                     clonedVertices,
                                     level + 1);
      for (k = 0; k < connected.length; ++k) {
        result.push(connected[k]);
      }

      if (! searchAttributes.followCycles) {
        delete visited[targetId];
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph
////////////////////////////////////////////////////////////////////////////////

function AQL_PATHS (vertices, edgeCollection, direction, followCycles, minLength, maxLength) {
  "use strict";

  var searchDirection;

  direction      = direction || "outbound";
  followCycles   = followCycles || false;
  minLength      = minLength || 0;
  maxLength      = maxLength !== undefined ? maxLength : 10;

  if (TYPEWEIGHT(vertices) !== TYPEWEIGHT_LIST) {
    WARN("PATHS", INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
    return null;
  }

  // validate arguments
  if (direction === "outbound") {
    searchDirection = 1;
  }
  else if (direction === "inbound") {
    searchDirection = 2;
  }
  else if (direction === "any") {
    searchDirection = 3;
  }
  else {
    WARN("PATHS", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (minLength < 0 || maxLength < 0 || minLength > maxLength) {
    WARN("PATHS", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var searchAttributes = {
    edgeCollection : COLLECTION(edgeCollection),
    minLength : minLength,
    maxLength : maxLength,
    direction : searchDirection,
    followCycles : followCycles
  };

  var result = [ ];
  var n = vertices.length, i, j;
  for (i = 0; i < n; ++i) {
    var vertex = vertices[i];
    var visited = { };

    visited[vertex._id] = true;
    var connected = SUBNODES(searchAttributes, vertex._id, visited, [ ], [ vertex ], 0);
    for (j = 0; j < connected.length; ++j) {
      result.push(connected[j]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_paths
/// The GRAPH\_PATHS function returns all paths of a graph.
///
/// `GRAPH_PATHS (graphName, options)`
///
/// The complexity of this method is **O(n\*n\*m)** with *n* being the amount of vertices in
/// the graph and *m* the average amount of connected edges;
///
/// *Parameters*
///
/// * *graphName*     : The name of the graph as a string.
/// * *options*     : An object containing the following options:
///   * *direction*        : The direction of the edges. Possible values are *any*,
/// *inbound* and *outbound* (default).
///   * *followCycles* (optional) : If set to *true* the query follows cycles in the graph,
/// default is false.
///   * *minLength* (optional)     : Defines the minimal length a path must
/// have to be returned (default is 0).
///   * *maxLength* (optional)     : Defines the maximal length a path must
/// have to be returned (default is 10).
///
/// @EXAMPLES
///
/// Return all paths of the graph "social":
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphPaths}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   db._query("RETURN GRAPH_PATHS('social')").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Return all inbound paths of the graph "social" with a maximal
/// length of 1 and a minimal length of 2:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphPaths2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
/// | db._query(
/// | "RETURN GRAPH_PATHS('social', {direction : 'inbound', minLength : 1, maxLength :  2})"
///   ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_PATHS (graphName, options) {
  "use strict";

  var searchDirection;
  if (! options) {
    options = {};
  }
  var direction      = options.direction || "outbound";
  var followCycles   = options.followCycles || false;
  var minLength      = options.minLength || 0;
  var maxLength      = options.maxLength || 10;

  // check graph exists and load edgeDefintions
  var graph = DOCUMENT_HANDLE("_graphs/" + graphName);
  if (! graph) {
    THROW("GRAPH_PATHS", INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, "GRAPH_PATHS");
  }

  var startCollections = [], edgeCollections = [];

  // validate direction and create edgeCollection array.
  graph.edgeDefinitions.forEach(function (def) {
    if (direction === "outbound") {
      searchDirection = 1;
      def.from.forEach(function (s) {
        if (startCollections.indexOf(s) === -1) {
          startCollections.push(s);
        }
      });
    }
    else if (direction === "inbound") {
      searchDirection = 2;
      def.to.forEach(function (s) {
        if (startCollections.indexOf(s) === -1) {
          startCollections.push(s);
        }
      });
    }
    else if (direction === "any") {
      def.from.forEach(function (s) {
        searchDirection = 3;
        if (startCollections.indexOf(s) === -1) {
          startCollections.push(s);
        }
      });
      def.to.forEach(function (s) {
        if (startCollections.indexOf(s) === -1) {
          startCollections.push(s);
        }
      });
    }
    else {
      WARN("GRAPH_PATHS", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return null;
    }
    if (edgeCollections.indexOf(def.collection) === -1) {
      edgeCollections.push(COLLECTION(def.collection));
    }

  });

  if (minLength < 0 || maxLength < 0 || minLength > maxLength) {
    WARN("GRAPH_PATHS", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = [ ];
  startCollections.forEach(function (startCollection) {

    var searchAttributes = {
      edgeCollection : edgeCollections,
      minLength : minLength,
      maxLength : maxLength,
      direction : searchDirection,
      followCycles : followCycles
    };

    var vertices = GET_DOCUMENTS(startCollection);
    var n = vertices.length, i, j;
    for (i = 0; i < n; ++i) {
      var vertex = vertices[i];
      var visited = { };

      visited[vertex._id] = true;
      var connected = SUBNODES(searchAttributes, vertex._id, visited, [ ], [ vertex ], 0);
      for (j = 0; j < connected.length; ++j) {
        result.push(connected[j]);
      }
    }

  });

  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_VISITOR (config, result, vertex, path) {
  "use strict";

  if (config.trackPaths) {
    result.push(CLONE({ vertex: vertex, path: path }));
  }
  else {
    result.push(CLONE({ vertex: vertex }));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for neighbors
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_NEIGHBOR_VISITOR (config, result, vertex, path) {
  "use strict";

  result.push(CLONE({ vertex: vertex, path: path, startVertex : config.startVertex }));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for tree traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_TREE_VISITOR (config, result, vertex, path) {
  "use strict";

  if (result.length === 0) {
    result.push({ });
  }

  var current = result[0], connector = config.connect, i;

  for (i = 0; i < path.vertices.length; ++i) {
    var v = path.vertices[i];
    if (typeof current[connector] === "undefined") {
      current[connector] = [ ];
    }
    var found = false, j;
    for (j = 0; j < current[connector].length; ++j) {
      if (current[connector][j]._id === v._id) {
        current = current[connector][j];
        found = true;
        break;
      }
    }
    if (! found) {
      current[connector].push(CLONE(v));
      current = current[connector][current[connector].length - 1];
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief expander callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_EDGE_EXAMPLE_FILTER (config, vertex, edge, path) {
  "use strict";
  if (config.edgeCollectionRestriction) {
    if (typeof config.edgeCollectionRestriction === "string" ) {
      config.edgeCollectionRestriction = [config.edgeCollectionRestriction];
    }
    if (config.edgeCollectionRestriction.indexOf(edge._id.split("/")[0]) === -1) {
      return false;
    }
  }
  if (config.expandEdgeExamples) {
    var e = AQL_MATCHES(edge, config.expandEdgeExamples);
    return AQL_MATCHES(edge, config.expandEdgeExamples);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex filter callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_VERTEX_FILTER (config, vertex, path) {
  "use strict";
  if (config.filterVertexExamples && ! AQL_MATCHES(vertex, config.filterVertexExamples)) {
    if (config.filterVertexCollections
      && config.vertexFilterMethod.indexOf("exclude") === -1
      && config.filterVertexCollections.indexOf(vertex._id.split("/")[0]) === -1
    ) {
      if (config.vertexFilterMethod.indexOf("prune") === -1) {
        return ["exclude"];
      }
      return ["prune", "exclude"];
    }
    return config.vertexFilterMethod;
  }
  if (config.filterVertexCollections
    && config.filterVertexCollections.indexOf(vertex._id.split("/")[0]) === -1
  ){
    return ["exclude"];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check typeweights of params.followEdges/params.filterVertices
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS (examples, func) {
  "use strict";

  if (TYPEWEIGHT(examples) === TYPEWEIGHT_STRING) {
    // a callback function was supplied. this is considered valid
    return true;
  }

  if (TYPEWEIGHT(examples) !== TYPEWEIGHT_LIST) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }
  if (examples.length === 0) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }

  var i;
  for (i = 0; i < examples.length; ++i) {
    if (TYPEWEIGHT(examples[i]) !== TYPEWEIGHT_DOCUMENT) {
      WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tranform key to id
////////////////////////////////////////////////////////////////////////////////

function TO_ID (vertex, collection) {
  "use strict";

  if (typeof vertex === 'object' && vertex.hasOwnProperty('_id')) {
    return vertex._id;
  }

  if (typeof vertex === 'string' && vertex.indexOf('/') === -1 && collection) {
    return collection + '/' + vertex;
  }

  return vertex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_FUNC (func,
                         datasource,
                         startVertex,
                         endVertex,
                         direction,
                         params) {
  "use strict";

  if (params === undefined) {
    params = { };
  }

  // check followEdges property
  if (params.followEdges) {
    if (! TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS(params.followEdges, func)) {
      return null;
    }
  }
  // check filterVertices property
  if (params.filterVertices) {
    if (! TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS(params.filterVertices, func)) {
      return null;
    }
  }

  if (typeof params.visitor !== "function") {
    WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (params.maxDepth === undefined) {
    // we need to have at least SOME limit to prevent endless iteration
    params.maxDepth = 256;
  }

  var config = {
    distance: params.distance,
    connect: params.connect,
    datasource: datasource,
    trackPaths: params.paths || false,
    visitor: params.visitor,
    maxDepth: params.maxDepth,
    minDepth: params.minDepth,
    maxIterations: params.maxIterations,
    uniqueness: params.uniqueness,
    expander: direction,
    direction: direction,
    strategy: params.strategy,
    order: params.order,
    itemOrder: params.itemOrder,
    startVertex : startVertex,
    endVertex : endVertex,
    weight : params.weight,
    defaultWeight : params.defaultWeight,
    prefill : params.prefill

  };

  if (params.followEdges) {
    if (typeof params.followEdges === 'string') {
      var f1 = params.followEdges.toUpperCase();
      config.expandFilter = function (config, vertex, edge, path) {
        return FCALL_USER(f1, [ config, vertex, edge, path ]);
      };
    }
    else {
      config.expandFilter = TRAVERSAL_EDGE_EXAMPLE_FILTER;
      config.expandEdgeExamples = params.followEdges;
    }
  }
  if (params.edgeCollectionRestriction) {
    config.expandFilter = TRAVERSAL_EDGE_EXAMPLE_FILTER;
    config.edgeCollectionRestriction = params.edgeCollectionRestriction;
  }

  if (params.filterVertices) {
    if (typeof params.filterVertices === 'string') {
      var f2 = params.filterVertices.toUpperCase();
      config.filter = function (config, vertex, edge, path) {
        return FCALL_USER(f2, [ config, vertex, path ]);
      };
    }
    else {
      config.filter = TRAVERSAL_VERTEX_FILTER;
      config.filterVertexExamples = params.filterVertices;
      config.vertexFilterMethod = params.vertexFilterMethod || ["prune", "exclude"];
    }
  }

  if (params.filterVertexCollections) {
    config.filter = config.filter || TRAVERSAL_VERTEX_FILTER;
    config.vertexFilterMethod = config.vertexFilterMethod || ["prune", "exclude"];
    config.filterVertexCollections = params.filterVertexCollections;
  }
  if (params._sort) {
    config.sort = function (l, r) { return l._key < r._key ? -1 : 1; };
  }

  // start vertex
  var v = null;
  try {
    v = INTERNAL.db._document(startVertex);
  }
  catch (err1) {
  }

  // end vertex
  var e;
  if (endVertex !== undefined) {
    try {
      e = INTERNAL.db._document(endVertex);
    }
    catch (err2) {
    }
  }

  var result = [ ];
  if (v !== null) {
    var traverser = new TRAVERSAL.Traverser(config);
    traverser.traverse(result, v, e);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief CHECK IF RESTRICTION LIST MATCHES
////////////////////////////////////////////////////////////////////////////////

function FILTER_RESTRICTION (list, restrictionList) {
  if (! restrictionList) {
    return list;
  }
  if (typeof restrictionList === "string") {
    restrictionList =  [restrictionList];
  }
  var result = [];
  restrictionList.forEach(function (r) {
    if (list.indexOf(r) !== -1) {
      result.push(r);
    }
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getAllDocsByExample
////////////////////////////////////////////////////////////////////////////////

function DOCUMENTS_BY_EXAMPLE (collectionList, example) {
  var res = [];
  if (example === "null") {
    example = [{}];
  }
  if (!example) {
    example = [{}];
  }
  if (typeof example === "string") {
    example = {_id : example};
  }
  if (!Array.isArray(example)) {
    example = [example];
  }
  var tmp = [];
  example.forEach(function (e) {
    if (typeof e === "string") {
      tmp.push({_id : e});
    } else {
      tmp.push(e);
    }
  });
  collectionList.forEach(function (c) {
    tmp.forEach(function (e) {
      res = res.concat(COLLECTION(c).byExample(e).toArray());
    });
  });
  return res;
}

function RESOLVE_GRAPH_TO_COLLECTIONS(graph, options) {
  var collections = {};
  collections.fromCollections = [];
  collections.toCollection = [];
  collections.edgeCollections = [];
  graph.edgeDefinitions.forEach(function (def) {
    if (options.direction === "outbound") {
      collections.edgeCollections = collections.edgeCollections.concat(
        FILTER_RESTRICTION(def.collection, options.edgeCollectionRestriction)
      );
      collections.fromCollections = collections.fromCollections.concat(
        FILTER_RESTRICTION(def.from, options.startVertexCollectionRestriction)
      );
      collections.toCollection = collections.toCollection.concat(
        FILTER_RESTRICTION(def.to, options.endVertexCollectionRestriction)
      );
    }
    else if (options.direction === "inbound") {
      collections.edgeCollections = collections.edgeCollections.concat(
        FILTER_RESTRICTION(def.collection, options.edgeCollectionRestriction)
      );
      collections.fromCollections = collections.fromCollections.concat(
        FILTER_RESTRICTION(def.to, options.endVertexCollectionRestriction)
      );
      collections.toCollection = collections.toCollection.concat(
        FILTER_RESTRICTION(def.from, options.startVertexCollectionRestriction)
      );
    }
    else if (options.direction === "any") {
      collections.edgeCollections = collections.edgeCollections.concat(
        FILTER_RESTRICTION(def.collection, options.edgeCollectionRestriction)
      );
      collections.fromCollections = collections.fromCollections.concat(
        FILTER_RESTRICTION(def.from.concat(def.to), options.startVertexCollectionRestriction)
      );
      collections.toCollection = collections.toCollection.concat(
        FILTER_RESTRICTION(def.from.concat(def.to), options.endVertexCollectionRestriction)
      );
    }
    else {
      WARN("GRAPH_EDGES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      // TODO: check if we need to return more data here
      return collections;
    }
  });
  collections.orphanCollections = FILTER_RESTRICTION(
    graph.orphanCollections, options.orphanCollectionRestriction
  );
  return collections;
}

function RESOLVE_GRAPH_TO_FROM_VERTICES (graphname, options) {
  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections ;
  if (! graph) {
    THROW("GRAPH_EDGES", INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, "GRAPH_EDGES");
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options);
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };
  if (options.includeOrphans) {
    collections.fromCollections = collections.fromCollections.concat(collections.orphanCollections);
  }
  return DOCUMENTS_BY_EXAMPLE(
    collections.fromCollections.filter(removeDuplicates), options.fromVertexExample
  );
}

function RESOLVE_GRAPH_TO_TO_VERTICES (graphname, options) {
  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections ;
  if (! graph) {
    THROW("GRAPH_EDGES", INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, "GRAPH_EDGES");
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options);
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };

  return DOCUMENTS_BY_EXAMPLE(
    collections.toCollection.filter(removeDuplicates), options.toVertexExample
  );
}

function RESOLVE_GRAPH_TO_EDGES (graphname, options) {
  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections ;
  if (! graph) {
    THROW("GRAPH_EDGES", INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, "GRAPH_EDGES");
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options);
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };

  return DOCUMENTS_BY_EXAMPLE(
    collections.edgeCollections.filter(removeDuplicates), options.edgeExamples
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief GET ALL EDGE and VERTEX COLLECTION ACCORDING TO DIRECTION
////////////////////////////////////////////////////////////////////////////////

function RESOLVE_GRAPH_START_VERTICES (graphName, options) {
  // check graph exists and load edgeDefintions
  var graph = DOCUMENT_HANDLE("_graphs/" + graphName), collections ;
  if (! graph) {
    THROW("GRAPH_EDGES", INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, "GRAPH_EDGES");
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options);
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };
  return DOCUMENTS_BY_EXAMPLE(
    collections.fromCollections.filter(removeDuplicates), options.fromVertexExample
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief GET ALL EDGE and VERTEX COLLECTION ACCORDING TO DIRECTION
////////////////////////////////////////////////////////////////////////////////

function RESOLVE_GRAPH_TO_DOCUMENTS (graphname, options) {
  // check graph exists and load edgeDefintions

  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections ;
  if (! graph) {
    THROW("GRAPH_EDGES", INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, "GRAPH_EDGES");
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options);
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };

  var result =  {
    fromVertices : DOCUMENTS_BY_EXAMPLE(
      collections.fromCollections.filter(removeDuplicates), options.fromVertexExample
    ),
    toVertices : DOCUMENTS_BY_EXAMPLE(
      collections.toCollection.filter(removeDuplicates), options.toVertexExample
    ),
    edges : DOCUMENTS_BY_EXAMPLE(
      collections.edgeCollections.filter(removeDuplicates), options.edgeExamples
    ),
    edgeCollections : collections.edgeCollections,
    fromCollections : collections.fromCollections,
    toCollection : collections.toCollection
  };

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate distance of an edge
////////////////////////////////////////////////////////////////////////////////

function DETERMINE_WEIGHT (edge, weight, defaultWeight) {
  if (! weight) {
    return 1;
  }
  if (typeof edge[weight] === "number") {
    return edge[weight];
  }
  if (defaultWeight) {
    return defaultWeight;
  }
  return Infinity;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_DISTANCE_VISITOR (config, result, vertex, path) {
  "use strict";

  if (config.endVertex && config.endVertex === vertex._id) {
    var dist = 0;
    path.edges.forEach(function (e) {
      if (config.weight) {
        if (typeof e[config.weight] === "number") {
          dist = dist + e[config.weight];
        } else if (config.defaultWeight) {
          dist = dist + config.defaultWeight;
        }
      } else {
        dist++;
      }
    });
    result.push(
      CLONE({ vertex: vertex, distance: dist , path: path , startVertex : config.startVertex})
    );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_DIJSKTRA_VISITOR (config, result, vertex, path) {
  "use strict";
  if (config.endVertex && config.endVertex === vertex._id) {
    path.vertices.forEach(function (from) {
      path.vertices.forEach(function (to) {
        if (config.prefill.indexOf(JSON.stringify({ from : TO_ID(from), to :  TO_ID(to)})) !== -1) {
          return;
        }
        var positionFrom = path.vertices.indexOf(from);
        var positionTo = path.vertices.indexOf(to);
        if (positionFrom > positionTo && config.direction !== 'any') {
          return;
        }
        var startVertex = from._id;
        var vertex = to;

        var distance = 0;
        var pathNew  = { vertices : [ from ], edges : [ ] };
        while (positionFrom !== positionTo) {
          var edgePosition;
          if (positionFrom > positionTo) {
            edgePosition = positionFrom-1;
          } else {
            edgePosition = positionFrom;
          }
          if (positionFrom > positionTo) {
            positionFrom = positionFrom -1;
          } else {
            positionFrom ++;
          }
          pathNew.vertices.push(path.vertices[positionFrom]);
          pathNew.edges.push(path.edges[edgePosition]);
          if (config.weight) {
            if (path.edges[edgePosition][config.weight]  &&
              typeof path.edges[edgePosition][config.weight] === "number") {
              distance = distance + path.edges[edgePosition][config.weight];
            } else if (config.defaultWeight) {
              distance = distance + config.defaultWeight;
            }
          } else {
            distance++;
          }
        }
        result.push(
          CLONE({ vertex: vertex, distance: distance , path: pathNew , startVertex : startVertex})
        );
      });
    });
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to determine parameters for SHORTEST_PATH and
/// GRAPH_SHORTEST_PATH
////////////////////////////////////////////////////////////////////////////////

function SHORTEST_PATH_PARAMS (params) {
  "use strict";

  if (params === undefined) {
    params = { };
  }

  params.strategy = "dijkstra";
  params.itemorder = "forward";
  params.visitor = TRAVERSAL_VISITOR;

  if (typeof params.distance === "string") {
    var name = params.distance.toUpperCase();

    params.distance = function (config, vertex1, vertex2, edge) {
      return FCALL_USER(name, [ config, vertex1, vertex2, edge ]);
    };
  }
  else {
    params.distance = undefined;
  }

  return params;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path algorithm
////////////////////////////////////////////////////////////////////////////////

function AQL_SHORTEST_PATH (vertexCollection,
                            edgeCollection,
                            startVertex,
                            endVertex,
                            direction,
                            params) {
  "use strict";

  params = SHORTEST_PATH_PARAMS(params);

  return TRAVERSAL_FUNC("SHORTEST_PATH",
                        TRAVERSAL.collectionDatasourceFactory(COLLECTION(edgeCollection)),
                        TO_ID(startVertex, vertexCollection),
                        TO_ID(endVertex, vertexCollection),
                        direction,
                        params);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path algorithm
////////////////////////////////////////////////////////////////////////////////

function CALCULATE_SHORTEST_PATHES_WITH_FLOYD_WARSHALL (graphData, options) {
  "use strict";

  var graph = graphData, result = [];

  graph.fromVerticesIDs = {};
  graph.fromVertices.forEach(function (a) {
    graph.fromVerticesIDs[a._id] = a;
  });
  graph.toVerticesIDs = {};
  graph.toVertices.forEach(function (a) {
    graph.toVerticesIDs[a._id] = a;
  });
  graph.docStore = {};

  var paths = {};

  var vertices = {};
  graph.edges.forEach(function(e) {
    if (options.direction === "outbound") {
      if (!paths[e._from]) {
        paths[e._from] = {};
      }
      paths[e._from][e._to] =  {distance : DETERMINE_WEIGHT(e, options.weight,
        options.defaultWeight), paths : [{edges : [e], vertices : [e._from, e._to]}]};
    } else if (options.direction === "inbound") {
      if (!paths[e._to]) {
        paths[e._to] = {};
      }
      paths[e._to][e._from] =  {distance : DETERMINE_WEIGHT(e, options.weight,
        options.defaultWeight), paths : [{edges : [e], vertices : [e._from, e._to]}]};
    } else {
      if (!paths[e._from]) {
        paths[e._from] = {};
      }
      if (!paths[e._to]) {
        paths[e._to] = {};
      }

      if (paths[e._from][e._to]) {
        paths[e._from][e._to].distance =
          Math.min(paths[e._from][e._to].distance, DETERMINE_WEIGHT(e, options.weight,
            options.defaultWeight));
      } else {
        paths[e._from][e._to] = {distance : DETERMINE_WEIGHT(e, options.weight,
          options.defaultWeight), paths : [{edges : [e], vertices : [e._from, e._to]}]};
      }
      if (paths[e._to][e._from]) {
        paths[e._to][e._from].distance =
          Math.min(paths[e._to][e._from].distance, DETERMINE_WEIGHT(e, options.weight,
            options.defaultWeight));
      } else {
        paths[e._to][e._from] = {distance : DETERMINE_WEIGHT(e, options.weight,
          options.defaultWeight), paths : [{edges : [e], vertices : [e._from, e._to]}]};
      }
    }
    if (options.noPaths) {
      try {
        delete paths[e._to][e._from].paths;
        delete paths[e._from][e._to].paths;
      } catch (ignore) {

      }
    }
    vertices[e._to] = 1;
    vertices[e._from] = 1;
  });
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };
  Object.keys(graph.fromVerticesIDs).forEach(function (v) {
    vertices[v] = 1;
  });

  var allVertices = Object.keys(vertices);
  allVertices.forEach(function (k) {
    allVertices.forEach(function (i) {
      allVertices.forEach(function (j) {
        if (i === j ) {
          if (!paths[i]) {
            paths[i] = {};
          }
          paths[i][j] = null;
          return;
        }
        if (paths[i] && paths[i][k] && paths[i][k].distance >=0
          && paths[i][k].distance < Infinity &&
          paths[k] && paths[k][j] && paths[k][j].distance >=0
          && paths[k][j].distance < Infinity &&
          ( !paths[i][j] ||
            paths[i][k].distance + paths[k][j].distance  <= paths[i][j].distance
            )
          ) {
          if (!paths[i][j]) {
            paths[i][j] = {paths : [], distance : paths[i][k].distance + paths[k][j].distance};
          }
          if (paths[i][k].distance + paths[k][j].distance  < paths[i][j].distance) {
            paths[i][j].distance = paths[i][k].distance+paths[k][j].distance;
            paths[i][j].paths = [];
          }
          if (!options.noPaths) {
            paths[i][k].paths.forEach(function (p1) {
              paths[k][j].paths.forEach(function (p2) {
                paths[i][j].paths.push({
                  edges : p1.edges.concat(p2.edges),
                  vertices:  p1.vertices.concat(p2.vertices).filter(removeDuplicates)
                });
              });
            });
          }
        }

      });

    });
  });

  var transformPath = function (paths) {
    paths.forEach(function (p) {
      var vTmp = [];
      p.vertices.forEach(function (v) {
        if (graph.fromVerticesIDs[v]) {
          vTmp.push(graph.fromVerticesIDs[v]);
        } else if (graph.toVerticesIDs[v]) {
          vTmp.push(graph.toVerticesIDs[v]);
        } else if (graph.docStore[v]) {
          vTmp.push(graph.docStore[v]);
        } else {
          graph.docStore[v] = DOCUMENT_HANDLE(v);
          vTmp.push(graph.docStore[v]);
        }
      });
      p.vertices = vTmp;
    });
    return paths;
  };

  Object.keys(paths).forEach(function (from) {
    if (!graph.fromVerticesIDs[from]) {
      return;
    }
    Object.keys(paths[from]).forEach(function (to) {
      if (!graph.toVerticesIDs[to]) {
        return;
      }
      if (from === to) {
        result.push({
          startVertex : from,
          vertex : graph.toVerticesIDs[to],
          paths : options.noPaths ? null :[{edges : [], vertices : []}],
          distance : 0
        });
        return;
      }
      result.push({
        startVertex : from,
        vertex : graph.toVerticesIDs[to],
        paths : options.noPaths ? null : transformPath(paths[from][to].paths),
        distance : paths[from][to].distance
      });
    });
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to determine parameters for TRAVERSAL and
/// GRAPH_TRAVERSAL
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_PARAMS (params) {
  "use strict";

  if (params === undefined) {
    params = { };
  }

  params.visitor = TRAVERSAL_VISITOR;
  return params;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge list of edges with list of examples
////////////////////////////////////////////////////////////////////////////////

function MERGE_EXAMPLES_WITH_EDGES (examples, edges) {
  var result = [], filter;
  if (examples === "null") {
    examples = [{}];
  }
  if (!examples) {
    examples = [{}];
  }
  if (typeof examples === "string") {
    examples = {_id : examples};
  }
  if (!Array.isArray(examples)) {
    examples = [examples];
  }
  if (examples.length === 0 || (
    examples.length === 1 &&
    Object.keys(examples).length === 0)) {
    return edges;
  }
  edges.forEach(function(edge) {
    examples.forEach(function(example) {
      filter = CLONE(example);
      if (!(filter._id || filter._key)) {
        filter._id = edge._id;
      }
      result.push(filter);
    });
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate shortest paths by dijkstra
////////////////////////////////////////////////////////////////////////////////

function CALCULATE_SHORTEST_PATHES_WITH_DIJKSTRA (graphName, options) {
  var params = TRAVERSAL_PARAMS(), factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);
  params.paths = true;
  if (options.edgeExamples) {
    params.followEdges = options.edgeExamples;
  }
  if (options.edgeCollectionRestriction) {
    params.edgeCollectionRestriction = options.edgeCollectionRestriction;
  }
  params.weight = options.weight;
  params.defaultWeight = options.defaultWeight;
  params = SHORTEST_PATH_PARAMS(params);
  params.visitor =  TRAVERSAL_DIJSKTRA_VISITOR;
  var result = [], fromVertices = RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options),
    toVertices = RESOLVE_GRAPH_TO_TO_VERTICES(graphName, options);

  var calculated = {};
  fromVertices.forEach(function (v) {
    toVertices.forEach(function (t) {
      if (calculated[JSON.stringify({ from : TO_ID(v), to :  TO_ID(t)})]) {
        result.push(calculated[JSON.stringify({ from : TO_ID(v), to :  TO_ID(t)})]);
        return;
      }
      params.prefill = Object.keys(calculated);
      var e = TRAVERSAL_FUNC("GRAPH_SHORTEST_PATH",
        factory,
        TO_ID(v),
        TO_ID(t),
        options.direction,
        params);
      e.forEach(function (f) {
        if (TO_ID(v) === f.startVertex &&  TO_ID(t) === f.vertex._id) {
          result.push(f);
        }
        calculated[JSON.stringify({ from : f.startVertex, to : f.vertex._id})] = f;
      });
    });
  });
  result.forEach(function (r) {
    r.paths = [r.path];
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if an example is set
////////////////////////////////////////////////////////////////////////////////

function IS_EXAMPLE_SET (example) {
  return (
    example && (
      (Array.isArray(example) && example.length > 0) ||
      (typeof example === "object" && Object.keys(example) > 0) ||
       typeof example === "string"
      )
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_shortest_paths
/// The GRAPH\_SHORTEST\_PATH function returns all shortest paths of a graph.
///
/// `GRAPH_SHORTEST_PATH (graphName, startVertexExample, endVertexExample, options)`
///
/// This function determines all shortest paths in a graph identified by *graphName*.
/// If one wants to call this function to receive nearly all shortest paths for a
/// graph the option *algorithm* should be set to
/// [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) to
/// increase performance.
/// If no algorithm is provided in the options the function chooses the appropriate
/// one (either [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
///  or [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)) according to its parameters.
/// The length of a path is by default the amount of edges from one start vertex to
/// an end vertex. The option weight allows the user to define an edge attribute
/// representing the length.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *startVertexExample* : An example for the desired start Vertices
/// (see [example](#short_explanation_of_the_example_parameter)).
/// * *endVertexExample*   : An example for the desired
/// end Vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options* (optional) : An object containing the following options:
///   * *direction*                        : The direction of the edges as a string.
///   Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the
///   edges in the shortest paths
///   (see [example](#short_explanation_of_the_example_parameter)).
///   * *algorithm*                        : The algorithm to calculate
///   the shortest paths. If both start and end vertex examples are empty
///   [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
///   used, otherwise the default is [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*                           : The name of the attribute of
///   the edges containing the length as a string.
///   * *defaultWeight*                    : Only used with the option *weight*.
///   If an edge does not have the attribute named as defined in option *weight* this default
///   is used as length.
///   If no default is supplied the default would be positive Infinity so the path could
///   not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, shortest distance from all german to all french cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphShortestPaths1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_SHORTEST_PATH("
/// | + "'routeplanner', {}, {}, {" +
/// | "weight : 'distance', endVertexCollectionRestriction : 'frenchCity', " +
/// | "startVertexCollectionRestriction : 'germanCity'}) RETURN [e.startVertex, e.vertex._id, " +
/// | "e.distance, LENGTH(e.paths)]"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, shortest distance from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphShortestPaths2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_SHORTEST_PATH("
/// | +"'routeplanner', [{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], " +
/// | "'frenchCity/Lyon', " +
/// | "{weight : 'distance'}) RETURN [e.startVertex, e.vertex._id, e.distance, LENGTH(e.paths)]"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_SHORTEST_PATH (graphName,
                                  startVertexExample,
                                  endVertexExample,
                                  options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  options.fromVertexExample = startVertexExample;
  options.toVertexExample = endVertexExample;
  if (! options.direction) {
    options.direction =  'any';
  }

  if (! options.algorithm) {
    if (! IS_EXAMPLE_SET(startVertexExample) && ! IS_EXAMPLE_SET(endVertexExample)) {
      options.algorithm = "Floyd-Warshall";
    }
  }

  if (options.algorithm === "Floyd-Warshall") {
    var graph = RESOLVE_GRAPH_TO_DOCUMENTS(graphName, options);
    return CALCULATE_SHORTEST_PATHES_WITH_FLOYD_WARSHALL(graph, options);
  }

  return CALCULATE_SHORTEST_PATHES_WITH_DIJKSTRA(graphName, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph
////////////////////////////////////////////////////////////////////////////////

function AQL_TRAVERSAL (vertexCollection,
                        edgeCollection,
                        startVertex,
                        direction,
                        params) {
  "use strict";

  params = TRAVERSAL_PARAMS(params);

  return TRAVERSAL_FUNC("TRAVERSAL",
                        TRAVERSAL.collectionDatasourceFactory(COLLECTION(edgeCollection)),
                        TO_ID(startVertex, vertexCollection),
                        undefined,
                        direction,
                        params);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_traversal
/// The GRAPH\_TRAVERSAL function traverses through the graph.
///
/// `GRAPH_TRAVERSAL (graphName, startVertexExample, direction, options)`
///
/// This function performs traversals on the given graph.
///
/// The complexity of this function strongly depends on the usage.
///
/// *Parameters*
/// * *graphName*          : The name of the graph as a string.
/// * *startVertexExample*        : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *direction*          : The direction of the edges as a string. Possible values
/// are *outbound*, *inbound* and *any* (default).
/// * *options*: Object containing optional options.
///
/// *Options*:
///  
/// * *strategy*: determines the visitation strategy. Possible values are 
/// *depthfirst* and *breadthfirst*. Default is *breadthfirst*.
/// * *order*: determines the visitation order. Possible values are 
/// *preorder* and *postorder*.
/// * *itemOrder*: determines the order in which connections returned by the 
/// expander will be processed. Possible values are *forward* and *backward*.
/// * *maxDepth*: if set to a value greater than *0*, this will limit the 
/// traversal to this maximum depth. 
/// * *minDepth*: if set to a value greater than *0*, all vertices found on 
/// a level below the *minDepth* level will not be included in the result.
/// * *maxIterations*: the maximum number of iterations that the traversal is 
/// allowed to perform. It is sensible to set this number so unbounded traversals 
/// will terminate at some point.
/// * *uniqueness*: an object that defines how repeated visitations of vertices should 
/// be handled. The *uniqueness* object can have a sub-attribute *vertices*, and a
/// sub-attribute *edges*. Each sub-attribute can have one of the following values:
///   * *"none"*: no uniqueness constraints
///   * *"path"*: element is excluded if it is already contained in the current path.
///    This setting may be sensible for graphs that contain cycles (e.g. A -> B -> C -> A).
///   * *"global"*: element is excluded if it was already found/visited at any 
///   point during the traversal.
///
/// @EXAMPLES
///
/// A route planner example, start a traversal from Hamburg :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversal1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound') RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, start a traversal from Hamburg with a max depth of 1
/// so only the direct neighbors of Hamburg are returned:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversal2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound', {maxDepth : 1}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_TRAVERSAL (graphName,
                              startVertexExample,
                              direction,
                              options) {
  "use strict";

  var result = [];
  options = TRAVERSAL_PARAMS(options);
  options.fromVertexExample = startVertexExample;
  options.direction =  direction;

  var startVertices = RESOLVE_GRAPH_START_VERTICES(graphName, options);
  var factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);

  startVertices.forEach(function (f) {
    result.push(TRAVERSAL_FUNC("GRAPH_TRAVERSAL",
      factory,
      TO_ID(f),
      undefined,
      direction,
      options));
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to determine parameters for TRAVERSAL_TREE and
/// GRAPH_TRAVERSAL_TREE
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_TREE_PARAMS (params, connectName, func) {
  "use strict";

  if (params === undefined) {
    params = { };
  }

  params.visitor  = TRAVERSAL_TREE_VISITOR;
  params.connect  = AQL_TO_STRING(connectName);

  if (params.connect === "") {
    THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  return params;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph and create a hierarchical result
/// this function uses the same setup as the TRAVERSE() function but will use
/// a different visitor to create the result
////////////////////////////////////////////////////////////////////////////////

function AQL_TRAVERSAL_TREE (vertexCollection,
                             edgeCollection,
                             startVertex,
                             direction,
                             connectName,
                             params) {
  "use strict";

  params = TRAVERSAL_TREE_PARAMS(params, connectName, "TRAVERSAL_TREE");
  if (params === null) {
    return null;
  }

  var result = TRAVERSAL_FUNC("TRAVERSAL_TREE",
                              TRAVERSAL.collectionDatasourceFactory(COLLECTION(edgeCollection)),
                              TO_ID(startVertex, vertexCollection),
                              undefined,
                              direction,
                              params);

  if (result.length === 0) {
    return [ ];
  }

  return [ result[0][params.connect] ];
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_distance
/// The GRAPH\_DISTANCE\_TO function returns all paths and there distance within a graph.
///
/// `GRAPH_DISTANCE_TO (graphName, startVertexExample, endVertexExample, options)`
///
/// This function is a wrapper of [GRAPH\_SHORTEST\_PATH](#graph_shortest_path).
/// It does not return the actual path but only the distance between two vertices.
///
/// @EXAMPLES
///
/// A route planner example, distance from all french to all german cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDistanceTo1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_DISTANCE_TO("
/// | +"'routeplanner', {}, {}, { " +
/// | " weight : 'distance', endVertexCollectionRestriction : 'germanCity', " +
/// | "startVertexCollectionRestriction : 'frenchCity'}) RETURN [e.startVertex, e.vertex._id, " +
/// | "e.distance]"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, distance from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDistanceTo2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_DISTANCE_TO("
/// | + "'routeplanner', [{_id: 'germanCity/Cologne'},{_id: 'germanCity/Hamburg'}], " +
/// | "'frenchCity/Lyon', " +
/// | "{weight : 'distance'}) RETURN [e.startVertex, e.vertex._id, e.distance]"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_DISTANCE_TO (graphName,
                                startVertexExample,
                                endVertexExample,
                                options) {
  "use strict";

  if (! options) {
    options = {};
  }
  options.noPaths = true;
  var res = AQL_GRAPH_SHORTEST_PATH(
    graphName, startVertexExample, endVertexExample, options
  ), result = [];
  res.forEach(function (r) {
    result.push({
      startVertex : r.startVertex,
      distance : r.distance,
      vertex : r.vertex
    });
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_traversal_tree
/// The GRAPH\_TRAVERSAL\_TREE function traverses through the graph.
///
/// `GRAPH_TRAVERSAL_TREE (graphName, startVertexExample, direction, connectName, options)`
/// This function creates a tree format from the result for a better visualization of
/// the path.
/// This function performs traversals on the given graph.
///
/// The complexity of this function strongly depends on the usage.
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *startVertexExample*         : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *direction*          : The direction of the edges as a string.
///  Possible values are *outbound*, *inbound* and *any* (default).
/// * *connectName*        : The result attribute which
///  contains the connection.
/// * *options* (optional) : An object containing options, see
///  [Graph Traversals](../AQL/GraphOperations.html#graph_traversal):
///
/// @EXAMPLES
///
/// A route planner example, start a traversal from Hamburg :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversalTree1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL_TREE('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound', 'connnection') RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, start a traversal from Hamburg with a max depth of 1 so
///  only the direct neighbors of Hamburg are returned:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversalTree2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL_TREE('routeplanner', 'germanCity/Hamburg',"+
/// | " 'outbound', 'connnection', {maxDepth : 1}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_TRAVERSAL_TREE (graphName,
                                   startVertexExample,
                                   direction,
                                   connectName,
                                   options) {
  "use strict";

  var result = [];
  options = TRAVERSAL_TREE_PARAMS(options, connectName, "GRAPH_TRAVERSAL_TREE");
  if (options === null) {
    return null;
  }
  options.fromVertexExample = startVertexExample;
  options.direction =  direction;

  var startVertices = RESOLVE_GRAPH_START_VERTICES(graphName, options);
  var factory = TRAVERSAL.generalGraphDatasourceFactory(graphName), r;

  startVertices.forEach(function (f) {
    r = TRAVERSAL_FUNC("GRAPH_TRAVERSAL_TREE",
      factory,
      TO_ID(f),
      undefined,
      direction,
      options);
    if (r.length > 0) {
      result.push([ r[0][options.connect] ]);
    }
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return connected edges
////////////////////////////////////////////////////////////////////////////////

function AQL_EDGES (edgeCollection,
                    vertex,
                    direction,
                    examples) {
  "use strict";

  var c = COLLECTION(edgeCollection), result;

  // validate arguments
  if (direction === "outbound") {
    result = c.outEdges(vertex);
  }
  else if (direction === "inbound") {
    result = c.inEdges(vertex);
  }
  else if (direction === "any") {
    result = c.edges(vertex);
  }
  else {
    WARN("EDGES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  return FILTER(result, examples);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to filter edges based on examples
////////////////////////////////////////////////////////////////////////////////

function FILTERED_EDGES (edges, vertex, direction, examples) {
  "use strict";

  var result = [ ];

  FILTER(edges, examples).forEach (function (e) {
    var key;

    if (direction === "inbound") {
      key = e._from;
    }
    else if (direction === "outbound") {
      key = e._to;
    }
    else if (direction === "any") {
      key = e._from;
      if (key === vertex) {
        key = e._to;
      }
    }

    if (key === vertex) {
      // do not return the start vertex itself
      return;
    }

    try {
      result.push({ edge: CLONE(e), vertex: DOCUMENT_HANDLE(key) });
    }
    catch (err) {
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return connected neighbors
////////////////////////////////////////////////////////////////////////////////

function AQL_NEIGHBORS (vertexCollection,
                        edgeCollection,
                        vertex,
                        direction,
                        examples) {
  "use strict";

  vertex = TO_ID(vertex, vertexCollection);
  var edges = AQL_EDGES(edgeCollection, vertex, direction);
  return FILTERED_EDGES(edges, vertex, direction, examples);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_neighbors
/// The GRAPH\_NEIGHBORS function returns all neighbors of vertices.
///
/// `GRAPH_NEIGHBORS (graphName, vertexExample, options)`
///
/// By default only the direct neighbors (path length equals 1) are returned, but one can define
/// the range of the path length to the neighbors with the options *minDepth* and *maxDepth*.
/// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
/// parameter vertexExamplex, *m* the average amount of neighbors and *x* the maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options*            : An object containing the following options:
///   * *direction*                        : The direction
///      of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeExamples*                     : A filter example for the edges to
///      the neighbors (see [example](#short_explanation_of_the_example_parameter)).
///   * *neighborExamples*                 : An example for the desired neighbors
///      (see [example](#short_explanation_of_the_example_parameter)).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *vertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be contained in the
///   result. This does not effect vertices on the path.
///   * *minDepth*                         : Defines the minimal
///      depth a path to a neighbor must have to be returned (default is 1).
///   * *maxDepth*                         : Defines the maximal
///      depth a path to a neighbor must have to be returned (default is 1).
///
/// @EXAMPLES
///
/// A route planner example, all neighbors of locations with a distance of either
/// 700 or 600.:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphNeighbors1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_NEIGHBORS("
/// | +"'routeplanner', {}, {edgeExamples : [{distance: 600}, {distance: 700}]}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound neighbors of Hamburg with a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphNeighbors2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_NEIGHBORS("
/// | +"'routeplanner', 'germanCity/Hamburg', {direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_NEIGHBORS (graphName,
                              vertexExample,
                              options) {
  "use strict";
  if (! options) {
    options = {  };
  }

  options.fromVertexExample = vertexExample;
  if (! options.hasOwnProperty("direction")) {
    options.direction =  'any';
  }

  if (options.hasOwnProperty("neighborExamples") && typeof options.neighborExamples === "string") {
    options.neighborExamples = {_id : options.neighborExamples};
  }
  var neighbors = [],
    params = TRAVERSAL_PARAMS(),
    factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);
  params.minDepth = options.minDepth === undefined ? 1 : options.minDepth;
  params.maxDepth = options.maxDepth === undefined ? 1 : options.maxDepth;
  params.paths = true;
  params.visitor = TRAVERSAL_NEIGHBOR_VISITOR;
  var fromVertices = RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options);
  if (options.edgeExamples) {
    params.followEdges = options.edgeExamples;
  }
  if (options.edgeCollectionRestriction) {
    params.edgeCollectionRestriction = options.edgeCollectionRestriction;
  }
  if (options.vertexCollectionRestriction) {
    params.filterVertexCollections = options.vertexCollectionRestriction;
  }
  fromVertices.forEach(function (v) {
    var e = TRAVERSAL_FUNC("GRAPH_NEIGHBORS",
      factory,
      v._id,
      undefined,
      options.direction,
      params);

    neighbors = neighbors.concat(e);
  });
  var result = [];
  neighbors.forEach(function (n) {
    if (FILTER([n.vertex], options.neighborExamples).length > 0) {
      result.push(n);
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_edges
///
/// `GRAPH_EDGES (graphName, vertexExample, options)`
///
/// The GRAPH\_EDGES function returns all edges of the graph connected to the vertices
/// defined by the example.
///
/// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
/// parameter vertexExamplex, *m* the average amount of edges of a vertex and *x* the maximal
/// depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options* (optional) : An object containing the following options:
///   * *direction*                        : The direction
/// of the edges as a string. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example
/// for the edges (see [example](#short_explanation_of_the_example_parameter)).
///   * *neighborExamples*                 : An example for
/// the desired neighbors (see [example](#short_explanation_of_the_example_parameter)).
///   * *minDepth*                         : Defines the minimal length of a path from an edge
///  to a vertex (default is 1, which means only the edges directly connected to a vertex would
///  be returned).
///   * *maxDepth*                         : Defines the maximal length of a path from an edge
///  to a vertex (default is 1, which means only the edges directly connected to a vertex would
///  be returned).
///
/// @EXAMPLES
///
/// A route planner example, all edges to locations with a distance of either 700 or 600.:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | +"'routeplanner', {}, {edgeExamples : [{distance: 600}, {distance: 700}]}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound edges of Hamburg with a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | +"'routeplanner', 'germanCity/Hamburg', {direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_EDGES (graphName,
                          vertexExample,
                          options) {
  "use strict";

  var neighbors = AQL_GRAPH_NEIGHBORS(graphName,
    vertexExample,
    options), result = [],ids = [];

  neighbors.forEach(function (n) {
    n.path.edges.forEach(function (e) {
      if (ids.indexOf(e._id) === -1) {
        result.push(e);
        ids.push(e._id);
      }
    });
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_vertices
/// The GRAPH\_VERTICES function returns all vertices.
///
/// `GRAPH_VERTICES (graphName, vertexExample, options)`
///
/// According to the optional filters it will only return vertices that have
/// outbound, inbound or any (default) edges.
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options* (optional)           : An object containing the following options:
///   * *direction*        : The direction of the edges as a string. Possible values are
///      *outbound*, *inbound* and *any* (default).
///   * *vertexCollectionRestriction*      : One or multiple
/// vertex collections that should be considered.
///
/// @EXAMPLES
///
/// A route planner example, all vertices of the graph
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertices1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_VERTICES("
///   +"'routeplanner', {}) RETURN e").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all vertices from collection *germanCity*.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertices2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_VERTICES("
/// | +"'routeplanner', {}, {direction : 'any', vertexCollectionRestriction" +
///   " : 'germanCity'}) RETURN e").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_VERTICES (graphName,
                             vertexExamples,
                             options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  if (! options.direction) {
    options.direction =  'any';
  }
  if (options.vertexCollectionRestriction) {
    if (options.direction === "inbound") {
      options.endVertexCollectionRestriction = options.vertexCollectionRestriction;
    } else if (options.direction === "outbound")  {
      options.startVertexCollectionRestriction = options.vertexCollectionRestriction;
    } else {
      options.includeOrphans = true;
      options.endVertexCollectionRestriction = options.vertexCollectionRestriction;
      options.startVertexCollectionRestriction = options.vertexCollectionRestriction;
      options.orphanCollectionRestriction = options.vertexCollectionRestriction;
    }
  }

  options.fromVertexExample = vertexExamples;
  return RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return common neighbors of two vertices lists
////////////////////////////////////////////////////////////////////////////////

function TRANSFER_GRAPH_NEIGHBORS_RESULT (result)  {
  var ret = {};
  result.forEach(function (r) {
    var v = JSON.stringify(r.vertex);
    if (! ret[v]) {
      ret[v] = [];
    }
    if (ret[v].indexOf(r.startVertex) === -1) {
      ret[v].push(r.startVertex);
    }
  });
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_common_neighbors
/// *The GRAPH\_COMMON\_NEIGHBORS function returns all common neighbors of the vertices
/// defined by the examples.*
///
/// `GRAPH_COMMON_NEIGHBORS (graphName, vertex1Example, vertex2Examples,
/// optionsVertex1, optionsVertex2)`
///
/// This function returns the intersection of *GRAPH_NEIGHBORS(vertex1Example, optionsVertex1)*
/// and *GRAPH_NEIGHBORS(vertex2Example, optionsVertex2)*.
/// The complexity of this method is **O(n\*m^x)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples, *m* the average amount of neighbors and *x* the
/// maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// For parameter documentation read the documentation of
/// [GRAPH_NEIGHBORS](#graph_neighbors).
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCommonNeighbors1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_NEIGHBORS("
/// | +"'routeplanner', {isCapital : true}, {isCapital : true}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCommonNeighbors2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_NEIGHBORS("
/// | +"'routeplanner', 'germanCity/Hamburg', {}, {direction : 'outbound', maxDepth : 2}, "+
/// | "{direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_COMMON_NEIGHBORS (graphName,
                                     vertex1Examples,
                                     vertex2Examples,
                                     options1,
                                     options2) {
  "use strict";

  var neighbors1 = TRANSFER_GRAPH_NEIGHBORS_RESULT(
    AQL_GRAPH_NEIGHBORS(graphName, vertex1Examples, options1)
  ), neighbors2;
  if (vertex1Examples === vertex2Examples && options1 === options2) {
    neighbors2 = CLONE(neighbors1);
  } else {
    neighbors2 = TRANSFER_GRAPH_NEIGHBORS_RESULT(
      AQL_GRAPH_NEIGHBORS(graphName, vertex2Examples, options2)
    );
  }
  var res = {}, res2 = {}, res3 = [];

  Object.keys(neighbors1).forEach(function (v) {
    if (!neighbors2[v]) {
      return;
    }
    if (!res[v]) {
      res[v] = {n1 : neighbors1[v], n2 : neighbors2[v]};
    }
  });
  Object.keys(res).forEach(function (v) {
    res[v].n1.forEach(function (s1) {
      if (!res2[s1]) {
        res2[s1] = {};
      }
      res[v].n2.forEach(function (s2) {
        if (s1 === s2) {
          return;
        }
        if (!res2[s1][s2]) {
          res2[s1][s2] = [];
        }
        res2[s1][s2].push(JSON.parse(v));
      });
      if (Object.keys(res2[s1]).length === 0) {
        delete res2[s1];
      }
    });
  });
  Object.keys(res2).forEach(function (r) {
    var e = {};
    e[r] = res2[r];
    res3.push(e);
  });
  return res3;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_common_properties
///
/// `GRAPH_COMMON_PROPERTIES (graphName, vertex1Example, vertex2Examples, options)`
///
/// The GRAPH\_COMMON\_PROPERTIES function returns a list of objects which have the id of
/// the vertices defined by *vertex1Example* as keys and a list of vertices defined by
/// *vertex21Example*, that share common properties as value. Notice that only the
/// vertex id and the matching attributes are returned in the result.
///
/// The complexity of this method is **O(n)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples.
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertex1Example*     : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *vertex2Example*     : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options* (optional)    : An object containing the following options:
///   * *vertex1CollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered.
///   * *vertex2CollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered.
///   * *ignoreProperties* : One or multiple
///  attributes of a document that should be ignored, either a string or an array..
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphProperties1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_PROPERTIES("
/// | +"'routeplanner', {}, {}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphProperties2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_PROPERTIES("
/// | +"'routeplanner', {}, {}, {ignoreProperties: 'population'}) RETURN e"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_COMMON_PROPERTIES (graphName,
                                      vertex1Examples,
                                      vertex2Examples,
                                      options) {
  "use strict";

  if (! options) {
    options = { };
  }
  options.fromVertexExample = vertex1Examples;
  options.toVertexExample = vertex2Examples;
  options.direction =  'any';
  options.ignoreProperties = TO_LIST(options.ignoreProperties, true);
  options.startVertexCollectionRestriction = options.vertex1CollectionRestriction;
  options.endVertexCollectionRestriction = options.vertex2CollectionRestriction;

  var g = RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options);
  var g2 = RESOLVE_GRAPH_TO_TO_VERTICES(graphName, options);
  var res = [];
  var t = {};
  g.forEach(function (n1) {
    Object.keys(n1).forEach(function (key) {
      if (key.indexOf("_") === 0 || options.ignoreProperties.indexOf(key) !== -1) {
        return;
      }
      if (!t[JSON.stringify({key : key , value : n1[key]})]) {
        t[JSON.stringify({key : key , value : n1[key]})] = {from : [], to : []};
      }
      t[JSON.stringify({key : key , value : n1[key]})].from.push(n1);
    });
  });

  g2.forEach(function (n1) {
    Object.keys(n1).forEach(function (key) {
      if (key.indexOf("_") === 0) {
        return;
      }
      if (!t[JSON.stringify({key : key , value : n1[key]})]) {
        return;
      }
      t[JSON.stringify({key : key , value : n1[key]})].to.push(n1);
    });
  });

  var tmp = {};
  Object.keys(t).forEach(function (r) {
    t[r].from.forEach(function (f) {
      if (!tmp[f._id]) {
        tmp[f._id] = [];
        tmp[f._id + "|keys"] = [];
      }
      t[r].to.forEach(function (t) {
        if (t._id === f._id) {
          return;
        }
        if (tmp[f._id + "|keys"].indexOf(t._id) === -1) {
          tmp[f._id + "|keys"].push(t._id);
          var obj = {_id : t._id};
          Object.keys(f).forEach(function (fromDoc) {
            if (t[fromDoc] !== undefined && t[fromDoc] === f[fromDoc]) {
              obj[fromDoc] = t[fromDoc];
            }
          });
          tmp[f._id].push(obj);
        }
      });
    });
  });
  Object.keys(tmp).forEach(function (r) {
    if (tmp[r].length === 0 || r.indexOf("|keys") !== -1) {
      return;
    }
    var a = {};
    a[r] = tmp[r];
    res.push(a);

  });

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_absolute_eccentricity
///
/// `GRAPH_ABSOLUTE_ECCENTRICITY (graphName, vertexExample, options)`
///
///  The GRAPH\_ABSOLUTE\_ECCENTRICITY function returns the
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the vertices defined by the examples.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options* (optional)    : An object containing the following options:
///   * *direction*                        : The direction of the edges as a string.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the edges in the
///  shortest paths (see [example](#short_explanation_of_the_example_parameter)).
///   * *algorithm*                        : The algorithm to calculate
///  the shortest paths as a string. If vertex example is empty
///  [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
///  used as default, otherwise the default is
///  [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)
///   * *weight*                           : The name of the attribute of
/// the edges containing the length as a string.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('routeplanner', {})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
///   +"'routeplanner', {}, {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all German cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
/// | + "'routeplanner', {}, {startVertexCollectionRestriction : 'germanCity', " +
///   "direction : 'outbound', weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ABSOLUTE_ECCENTRICITY (graphName, vertexExample, options) {
  "use strict";
  if (! options) {
    options = {  };
  }
  if (! options.direction) {
    options.direction =  'any';
  }

  var distanceMap = AQL_GRAPH_DISTANCE_TO(graphName, vertexExample, {}, options), 
      result = {};
  distanceMap.forEach(function(d) {
    if (!result[d.startVertex]) {
      result[d.startVertex] = d.distance;
    } else {
      result[d.startVertex] = Math.max(result[d.startVertex], d.distance);
    }
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_eccentricity
///
/// `GRAPH_ECCENTRICITY (graphName, options)`
///
/// The GRAPH\_ECCENTRICITY function returns the normalized
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the graphs vertices
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *options* (optional) : An object containing the following options:
///   * *direction*       : The direction of the edges as a string.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*       : The algorithm to calculate the shortest paths as a string. Possible
/// values are [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
///  (default) and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*         : The name of the attribute of the edges containing the length as a string.
///   * *defaultWeight*   : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEccentricity1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ECCENTRICITY('routeplanner')").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEccentricity2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ECCENTRICITY('routeplanner', {weight : 'distance'})"
///   ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ECCENTRICITY (graphName, options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  if (! options.algorithm) {
    options.algorithm = "Floyd-Warshall";
  }

  var result = AQL_GRAPH_ABSOLUTE_ECCENTRICITY(graphName, {}, options), max = 0;
  Object.keys(result).forEach(function (r) {
    result[r] = result[r] === 0 ? 0 : 1 / result[r];
    if (result[r] > max) {
      max = result[r];
    }
  });
  Object.keys(result).forEach(function (r) {
    result[r] = result[r] / max;
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_absolute_closeness
///
/// `GRAPH_ABSOLUTE_CLOSENESS (graphName, vertexExample, options)`
///
/// The GRAPH\_ABSOLUTE\_CLOSENESS function returns the
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of the vertices defined by the examples.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*     : An example for the desired
/// vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options*     : An object containing the following options:
///   * *direction*                        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the
/// edges in the shortest paths (
/// see [example](#short_explanation_of_the_example_parameter)).
///   * *algorithm*                        : The algorithm to calculate
/// the shortest paths. Possible values are
/// [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) (default)
///  and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*                           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS('routeplanner', {})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS("
///   +"'routeplanner', {}, {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all German cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS("
/// | + "'routeplanner', {}, {startVertexCollectionRestriction : 'germanCity', " +
///   "direction : 'outbound', weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ABSOLUTE_CLOSENESS (graphName, vertexExample, options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  if (! options.direction) {
    options.direction =  'any';
  }

  var distanceMap = AQL_GRAPH_DISTANCE_TO(graphName, vertexExample , {}, options), result = {};
  distanceMap.forEach(function(d) {
    if (options.direction !==  'any' && options.calcNormalized) {
      d.distance = d.distance === 0 ?  0 : 1 / d.distance;
    }
    if (!result[d.startVertex]) {
      result[d.startVertex] = d.distance;
    } else {
      result[d.startVertex] = d.distance + result[d.startVertex];
    }
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_closeness
///
/// `GRAPH_CLOSENESS (graphName, options)`
///
/// The GRAPH\_CLOSENESS function returns the normalized
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of graphs vertices.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *options*     : An object containing the following options:
///   * *direction*                        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*                        : The algorithm to calculate
/// the shortest paths. Possible values are
/// [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) (default)
///  and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*                           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCloseness1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_CLOSENESS('routeplanner')").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCloseness2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_CLOSENESS("
///   +"'routeplanner', {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCloseness3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_CLOSENESS("
/// | + "'routeplanner',{direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_CLOSENESS (graphName, options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  options.calcNormalized = true;

  if (! options.algorithm) {
    options.algorithm = "Floyd-Warshall";
  }

  var result = AQL_GRAPH_ABSOLUTE_CLOSENESS(graphName, {}, options), max = 0;
  Object.keys(result).forEach(function (r) {
    if (options.direction ===  'any') {
      result[r] = result[r]  === 0 ? 0 : 1 / result[r];
    }
    if (result[r] > max) {
      max = result[r];
    }
  });
  Object.keys(result).forEach(function (r) {
    result[r] = result[r] / max;
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_absolute_betweenness
///
/// `GRAPH_ABSOLUTE_BETWEENNESS (graphName, vertexExample, options)`
///
/// The GRAPH\_ABSOLUTE\_BETWEENNESS function returns the
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of all vertices in the graph.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
///
/// * *graphName*          : The name of the graph as a string.
/// * *options*            : An object containing the following options:
///   * *direction*                        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *weight*                           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the betweenness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS('routeplanner', {})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS("
///   +"'routeplanner', {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ABSOLUTE_BETWEENNESS (graphName, options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  if (! options.direction) {
    options.direction =  'any';
  }
  options.algorithm = "Floyd-Warshall";

  var distanceMap = AQL_GRAPH_SHORTEST_PATH(graphName, {} , {}, options),
    result = {};
  distanceMap.forEach(function(d) {
    var tmp = {};
    if (!result[d.startVertex]) {
      result[d.startVertex] = 0;
    }
    if (!result[d.vertex._id]) {
      result[d.vertex._id] = 0;
    }
    d.paths.forEach(function (p) {
      p.vertices.forEach(function (v) {
        if (v._id === d.startVertex || v._id === d.vertex._id) {
          return;
        }
        if (!tmp[v._id]) {
          tmp[v._id] = 1;
        } else {
          tmp[v._id]++;
        }
      });
    });
    Object.keys(tmp).forEach(function (t) {
      if (!result[t]) {
        result[t] = 0;
      }
      result[t] =  result[t]  + tmp[t] / d.paths.length;
    });
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_betweenness
///
/// `GRAPH_BETWEENNESS (graphName, options)`
///
/// The GRAPH\_BETWEENNESS function returns the
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of graphs vertices.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *options*     : An object containing the following options:
///   * *direction*                        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *weight*                           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphBetweenness1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_BETWEENNESS('routeplanner')").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphBetweenness2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_BETWEENNESS('routeplanner', {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphBetweenness3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_BETWEENNESS("
///   + "'routeplanner', {direction : 'outbound', weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_BETWEENNESS (graphName, options) {
  "use strict";

  if (! options) {
    options = {  };
  }

  var result = AQL_GRAPH_ABSOLUTE_BETWEENNESS(graphName, options),  max = 0;
  Object.keys(result).forEach(function (r) {
    if (result[r] > max) {
      max = result[r];
    }
  });
  Object.keys(result).forEach(function (r) {
    result[r] = result[r] / max;
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_radius
///
/// `GRAPH_RADIUS (graphName, options)`
///
/// *The GRAPH\_RADIUS function returns the
/// [radius](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
/// of a graph.*
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// * *graphName*       : The name of the graph as a string.
/// * *options*     : An object containing the following options:
///   * *direction*     : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*     : The algorithm to calculate the shortest paths as a string. Possible
/// values are [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
///  (default) and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the radius of the graph.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_RADIUS('routeplanner')").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_RADIUS('routeplanner', {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_RADIUS("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_RADIUS (graphName, options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "Floyd-Warshall";
  }

  var result = AQL_GRAPH_ABSOLUTE_ECCENTRICITY(graphName, {}, options), min = Infinity;
  Object.keys(result).forEach(function (r) {
    if (result[r] === 0) {
      return;
    }
    if (result[r] < min) {
      min = result[r];
    }
  });

  return min;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_ahuacatl_general_graph_diameter
///
/// `GRAPH_DIAMETER (graphName, options)`
///
/// The GRAPH\_DIAMETER function returns the
/// [diameter](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
/// of a graph.
///
/// The complexity of the function is described
/// [here](#the_complexity_of_the_shortest_path_algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *options*     : An object containing the following options:
///   * *direction*        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*        : The algorithm to calculate the shortest paths as a string. Possible
/// values are  [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
///  (default) and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the diameter of the graph.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDiameter1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_DIAMETER('routeplanner')").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDiameter2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_DIAMETER('routeplanner', {weight : 'distance'})").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDiameter3}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_DIAMETER("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_DIAMETER (graphName, options) {
  "use strict";

  if (! options) {
    options = {  };
  }
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "Floyd-Warshall";
  }

  var result = AQL_GRAPH_ABSOLUTE_ECCENTRICITY(graphName, {}, options), max = 0;
  Object.keys(result).forEach(function (r) {
    if (result[r] > max) {
      max = result[r];
    }
  });

  return max;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.FCALL_USER = FCALL_USER;
exports.KEYS = KEYS;
exports.GET_INDEX = GET_INDEX;
exports.DOCUMENT_MEMBER = DOCUMENT_MEMBER;
exports.GET_DOCUMENTS = GET_DOCUMENTS;
exports.TERNARY_OPERATOR = TERNARY_OPERATOR;
exports.LOGICAL_AND = LOGICAL_AND;
exports.LOGICAL_OR = LOGICAL_OR;
exports.LOGICAL_NOT = LOGICAL_NOT;
exports.RELATIONAL_EQUAL = RELATIONAL_EQUAL;
exports.RELATIONAL_UNEQUAL = RELATIONAL_UNEQUAL;
exports.RELATIONAL_GREATER = RELATIONAL_GREATER;
exports.RELATIONAL_GREATEREQUAL = RELATIONAL_GREATEREQUAL;
exports.RELATIONAL_LESS = RELATIONAL_LESS;
exports.RELATIONAL_LESSEQUAL = RELATIONAL_LESSEQUAL;
exports.RELATIONAL_CMP = RELATIONAL_CMP;
exports.RELATIONAL_IN = RELATIONAL_IN;
exports.RELATIONAL_NOT_IN = RELATIONAL_NOT_IN;
exports.UNARY_PLUS = UNARY_PLUS;
exports.UNARY_MINUS = UNARY_MINUS;
exports.ARITHMETIC_PLUS = ARITHMETIC_PLUS;
exports.ARITHMETIC_MINUS = ARITHMETIC_MINUS;
exports.ARITHMETIC_TIMES = ARITHMETIC_TIMES;
exports.ARITHMETIC_DIVIDE = ARITHMETIC_DIVIDE;
exports.ARITHMETIC_MODULUS = ARITHMETIC_MODULUS;

exports.AQL_DOCUMENT = AQL_DOCUMENT;
exports.AQL_COLLECTIONS = AQL_COLLECTIONS;
exports.AQL_CONCAT = AQL_CONCAT;
exports.AQL_CONCAT_SEPARATOR = AQL_CONCAT_SEPARATOR;
exports.AQL_CHAR_LENGTH = AQL_CHAR_LENGTH;
exports.AQL_LOWER = AQL_LOWER;
exports.AQL_UPPER = AQL_UPPER;
exports.AQL_SUBSTRING = AQL_SUBSTRING;
exports.AQL_CONTAINS = AQL_CONTAINS;
exports.AQL_LIKE = AQL_LIKE;
exports.AQL_LEFT = AQL_LEFT;
exports.AQL_RIGHT = AQL_RIGHT;
exports.AQL_TRIM = AQL_TRIM;
exports.AQL_FIND_FIRST = AQL_FIND_FIRST;
exports.AQL_FIND_LAST = AQL_FIND_LAST;
exports.AQL_TO_BOOL = AQL_TO_BOOL;
exports.AQL_TO_NUMBER = AQL_TO_NUMBER;
exports.AQL_TO_STRING = AQL_TO_STRING;
exports.AQL_TO_LIST = AQL_TO_LIST;
exports.AQL_IS_NULL = AQL_IS_NULL;
exports.AQL_IS_BOOL = AQL_IS_BOOL;
exports.AQL_IS_NUMBER = AQL_IS_NUMBER;
exports.AQL_IS_STRING = AQL_IS_STRING;
exports.AQL_IS_LIST = AQL_IS_LIST;
exports.AQL_IS_DOCUMENT = AQL_IS_DOCUMENT;
exports.AQL_FLOOR = AQL_FLOOR;
exports.AQL_CEIL = AQL_CEIL;
exports.AQL_ROUND = AQL_ROUND;
exports.AQL_ABS = AQL_ABS;
exports.AQL_RAND = AQL_RAND;
exports.AQL_SQRT = AQL_SQRT;
exports.AQL_LENGTH = AQL_LENGTH;
exports.AQL_FIRST = AQL_FIRST;
exports.AQL_LAST = AQL_LAST;
exports.AQL_POSITION = AQL_POSITION;
exports.AQL_NTH = AQL_NTH;
exports.AQL_REVERSE = AQL_REVERSE;
exports.AQL_RANGE = AQL_RANGE;
exports.AQL_UNIQUE = AQL_UNIQUE;
exports.AQL_UNION = AQL_UNION;
exports.AQL_UNION_DISTINCT = AQL_UNION_DISTINCT;
exports.AQL_SLICE = AQL_SLICE;
exports.AQL_MINUS = AQL_MINUS;
exports.AQL_INTERSECTION = AQL_INTERSECTION;
exports.AQL_FLATTEN = AQL_FLATTEN;
exports.AQL_MAX = AQL_MAX;
exports.AQL_MIN = AQL_MIN;
exports.AQL_SUM = AQL_SUM;
exports.AQL_AVERAGE = AQL_AVERAGE;
exports.AQL_MEDIAN = AQL_MEDIAN;
exports.AQL_VARIANCE_SAMPLE = AQL_VARIANCE_SAMPLE;
exports.AQL_VARIANCE_POPULATION = AQL_VARIANCE_POPULATION;
exports.AQL_STDDEV_SAMPLE = AQL_STDDEV_SAMPLE;
exports.AQL_STDDEV_POPULATION = AQL_STDDEV_POPULATION;
exports.AQL_NEAR = AQL_NEAR;
exports.AQL_WITHIN = AQL_WITHIN;
exports.AQL_WITHIN_RECTANGLE = AQL_WITHIN_RECTANGLE;
exports.AQL_IS_IN_POLYGON = AQL_IS_IN_POLYGON;
exports.AQL_FULLTEXT = AQL_FULLTEXT;
exports.AQL_PATHS = AQL_PATHS;
exports.AQL_SHORTEST_PATH = AQL_SHORTEST_PATH;
exports.AQL_TRAVERSAL = AQL_TRAVERSAL;
exports.AQL_TRAVERSAL_TREE = AQL_TRAVERSAL_TREE;
exports.AQL_EDGES = AQL_EDGES;
exports.AQL_NEIGHBORS = AQL_NEIGHBORS;
exports.AQL_GRAPH_TRAVERSAL = AQL_GRAPH_TRAVERSAL;
exports.AQL_GRAPH_TRAVERSAL_TREE = AQL_GRAPH_TRAVERSAL_TREE;
exports.AQL_GRAPH_EDGES = AQL_GRAPH_EDGES;
exports.AQL_GRAPH_VERTICES = AQL_GRAPH_VERTICES;
exports.AQL_GRAPH_PATHS = AQL_GRAPH_PATHS;
exports.AQL_GRAPH_SHORTEST_PATH = AQL_GRAPH_SHORTEST_PATH;
exports.AQL_GRAPH_DISTANCE_TO = AQL_GRAPH_DISTANCE_TO;
exports.AQL_GRAPH_NEIGHBORS = AQL_GRAPH_NEIGHBORS;
exports.AQL_GRAPH_COMMON_NEIGHBORS = AQL_GRAPH_COMMON_NEIGHBORS;
exports.AQL_GRAPH_COMMON_PROPERTIES = AQL_GRAPH_COMMON_PROPERTIES;
exports.AQL_GRAPH_ECCENTRICITY = AQL_GRAPH_ECCENTRICITY;
exports.AQL_GRAPH_BETWEENNESS = AQL_GRAPH_BETWEENNESS;
exports.AQL_GRAPH_CLOSENESS = AQL_GRAPH_CLOSENESS;
exports.AQL_GRAPH_ABSOLUTE_ECCENTRICITY = AQL_GRAPH_ABSOLUTE_ECCENTRICITY;
exports.AQL_GRAPH_ABSOLUTE_BETWEENNESS = AQL_GRAPH_ABSOLUTE_BETWEENNESS;
exports.AQL_GRAPH_ABSOLUTE_CLOSENESS = AQL_GRAPH_ABSOLUTE_CLOSENESS;
exports.AQL_GRAPH_DIAMETER = AQL_GRAPH_DIAMETER;
exports.AQL_GRAPH_RADIUS = AQL_GRAPH_RADIUS;
exports.AQL_NOT_NULL = AQL_NOT_NULL;
exports.AQL_FIRST_LIST = AQL_FIRST_LIST;
exports.AQL_FIRST_DOCUMENT = AQL_FIRST_DOCUMENT;
exports.AQL_PARSE_IDENTIFIER = AQL_PARSE_IDENTIFIER;
exports.AQL_SKIPLIST = AQL_SKIPLIST;
exports.AQL_HAS = AQL_HAS;
exports.AQL_ATTRIBUTES = AQL_ATTRIBUTES;
exports.AQL_UNSET = AQL_UNSET;
exports.AQL_KEEP = AQL_KEEP;
exports.AQL_MERGE = AQL_MERGE;
exports.AQL_MERGE_RECURSIVE = AQL_MERGE_RECURSIVE;
exports.AQL_TRANSLATE = AQL_TRANSLATE;
exports.AQL_MATCHES = AQL_MATCHES;
exports.AQL_PASSTHRU = AQL_PASSTHRU;
exports.AQL_SLEEP = AQL_SLEEP;
exports.AQL_CURRENT_DATABASE = AQL_CURRENT_DATABASE;
exports.AQL_CURRENT_USER = AQL_CURRENT_USER;
exports.AQL_FAIL = AQL_FAIL;
exports.AQL_DATE_NOW = AQL_DATE_NOW;
exports.AQL_DATE_TIMESTAMP = AQL_DATE_TIMESTAMP;
exports.AQL_DATE_ISO8601 = AQL_DATE_ISO8601;
exports.AQL_DATE_DAYOFWEEK = AQL_DATE_DAYOFWEEK;
exports.AQL_DATE_YEAR = AQL_DATE_YEAR;
exports.AQL_DATE_MONTH = AQL_DATE_MONTH;
exports.AQL_DATE_DAY = AQL_DATE_DAY;
exports.AQL_DATE_HOUR = AQL_DATE_HOUR;
exports.AQL_DATE_MINUTE = AQL_DATE_MINUTE;
exports.AQL_DATE_SECOND = AQL_DATE_SECOND;
exports.AQL_DATE_MILLISECOND = AQL_DATE_MILLISECOND;

exports.reload = reloadUserFunctions;

// initialise the query engine
resetEngine();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
