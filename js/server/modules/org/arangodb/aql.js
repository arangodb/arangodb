/*jshint strict: false, unused: false, bitwise: false, esnext: true */
/*global COMPARE_STRING, AQL_TO_BOOL, AQL_TO_NUMBER, AQL_TO_STRING, AQL_WARNING, AQL_QUERY_SLEEP */
/*global CPP_SHORTEST_PATH, CPP_NEIGHBORS, Set */

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
var underscore = require("underscore");
var graphModule = require("org/arangodb/general-graph");

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
/// @brief prefab traversal visitors
////////////////////////////////////////////////////////////////////////////////

var DefaultVisitors = {
  "_AQL::HASATTRIBUTESVISITOR" : {
    visitorReturnsResults: true,
    func: function (config, result, vertex, path) {
      if (typeof config.data === "object" && Array.isArray(config.data.attributes)) {
        if (config.data.attributes.length === 0) {
          return;
        }

        var allowNull = true; // default value
        if (config.data.hasOwnProperty('allowNull')) {
          allowNull = config.data.allowNull;
        }
        var i;
        if (config.data.type === 'any') {
          for (i = 0; i < config.data.attributes.length; ++i) {
            if (! vertex.hasOwnProperty(config.data.attributes[i])) {
              continue;
            }
            if (! allowNull && vertex[config.data.attributes[i]] === null) {
              continue;
            }
          
            return CLONE({ vertex: vertex, path: path });
          }

          return;
        }
          
        for (i = 0; i < config.data.attributes.length; ++i) {
          if (! vertex.hasOwnProperty(config.data.attributes[i])) {
            return;
          }
          if (! allowNull &&
              vertex[config.data.attributes[i]] === null) {
            return;
          }
        }

        return CLONE({ vertex: vertex, path: path });
      }
    }
  },
  "_AQL::PROJECTINGVISITOR" : {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      var values = { };
      if (typeof config.data === "object" && Array.isArray(config.data.attributes)) {
        config.data.attributes.forEach(function (attribute) {
          values[attribute] = vertex[attribute];
        });
      }
      return values;
    }
  },
  "_AQL::IDVISITOR" : {
    visitorReturnsResults: true,
    func:  function (config, result, vertex) {
      return vertex._id;
    }
  },
  "_AQL::KEYVISITOR" : {
    visitorReturnsResults: true,
    func:  function (config, result, vertex) {
      return vertex._key;
    }
  },
  "_AQL::COUNTINGVISITOR" : {
    visitorReturnsResults: false,
    func: function (config, result) {
      if (result.length === 0) {
        result.push(0);
      }
      result[0]++;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief type weight used for sorting and comparing
////////////////////////////////////////////////////////////////////////////////

var TYPEWEIGHT_NULL      = 0;
var TYPEWEIGHT_BOOL      = 1;
var TYPEWEIGHT_NUMBER    = 2;
var TYPEWEIGHT_STRING    = 4;
var TYPEWEIGHT_ARRAY     = 8;
var TYPEWEIGHT_OBJECT    = 16;

////////////////////////////////////////////////////////////////////////////////
/// @brief mapping of time unit names to short name, getter and setter
////////////////////////////////////////////////////////////////////////////////

var unitMapping = {
  y: ["y", "getUTCFullYear", "setUTCFullYear"],
  m: ["m", "getUTCMonth", "setUTCMonth"],
  w: ["w", null, null],
  d: ["d", "getUTCDate", "setUTCDate"],
  h: ["h", "getUTCHours", "setUTCHours"],
  i: ["i", "getUTCMinutes", "setUTCMinutes"],
  s: ["s", "getUTCSeconds", "setUTCSeconds"],
  f: ["f", "getUTCMilliseconds", "setUTCMilliseconds"]
};

// aliases
unitMapping.years = unitMapping.y;
unitMapping.year = unitMapping.y;
unitMapping.months = unitMapping.m;
unitMapping.month = unitMapping.m;
unitMapping.week = unitMapping.w;
unitMapping.weeks = unitMapping.w;
unitMapping.days = unitMapping.d;
unitMapping.day = unitMapping.d;
unitMapping.hours = unitMapping.h;
unitMapping.hour = unitMapping.h;
unitMapping.minutes = unitMapping.i;
unitMapping.minute = unitMapping.i;
unitMapping.seconds = unitMapping.s;
unitMapping.second = unitMapping.s;
unitMapping.milliseconds = unitMapping.f;
unitMapping.millisecond = unitMapping.f;
unitMapping.ms = unitMapping.f;

var unitMappingArray = [null, "y", "m", "w", "d", "h", "i", "s", "f"];

////////////////////////////////////////////////////////////////////////////////
/// @brief RegExp and cache for ISO duration strings
////////////////////////////////////////////////////////////////////////////////

// ISODurationRegex.exec("P1Y2M3W4DT5H6M7.890S")
// -> ["P1Y2M3W4DT5H6M7.890S", "1", "2", "3", "4", "5", "6", "7", "890"]

/* jshint -W101 */
var ISODurationRegex = /^P(?:(?:(\d+)Y)?(?:(\d+)M)?(?:(\d+)W)?(?:(\d+)D)?)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)(?:\.(\d+))?S)?)?$/i;
/* jshint +W101 */

var ISODurationCache = {}; // TODO: clear cache for every new AQL query to avoid memory leak

////////////////////////////////////////////////////////////////////////////////
/// @brief substring ranges for DATE_COMPARE()
////////////////////////////////////////////////////////////////////////////////

// 0123_56_89_12_45_78_012_
var unitStrRanges = {
  y: [0, 4],
  m: [5, 7],
  d: [8, 10],
  h: [11, 13],
  i: [14, 16],
  s: [17, 19],
  f: [20, 23]
};
unitStrRanges.years = unitStrRanges.y;
unitStrRanges.year = unitStrRanges.y;
unitStrRanges.months = unitStrRanges.m;
unitStrRanges.month = unitStrRanges.m;
unitStrRanges.days = unitStrRanges.d;
unitStrRanges.day = unitStrRanges.d;
unitStrRanges.hours = unitStrRanges.h;
unitStrRanges.hour = unitStrRanges.h;
unitStrRanges.minutes = unitStrRanges.i;
unitStrRanges.minute = unitStrRanges.i;
unitStrRanges.seconds = unitStrRanges.s;
unitStrRanges.second = unitStrRanges.s;
unitStrRanges.milliseconds = unitStrRanges.f;
unitStrRanges.millisecond = unitStrRanges.f;
unitStrRanges.ms = unitStrRanges.f;

////////////////////////////////////////////////////////////////////////////////
/// @brief offsets for day of year calculation
////////////////////////////////////////////////////////////////////////////////

var dayOfYearOffsets = [
  0,
  31,  // + 31 Jan
  59,  // + 28 Feb*
  90,  // + 31 Mar
  120, // + 30 Apr
  151, // + 31 May
  181, // + 30 Jun
  212, // + 31 Jul
  243, // + 31 Aug
  273, // + 30 Sep
  304, // + 31 Oct
  334  // + 30 Nov
];

var dayOfLeapYearOffsets = [
  0,
  31,  // + 31 Jan
  59,  // + 29 Feb*
  91,  // + 31 Mar
  121, // + 30 Apr
  152, // + 31 May
  182, // + 30 Jun
  213, // + 31 Jul
  244, // + 31 Aug
  274, // + 30 Sep
  305, // + 31 Oct
  335  // + 30 Nov
];

////////////////////////////////////////////////////////////////////////////////
/// @brief constants for date difference function
////////////////////////////////////////////////////////////////////////////////

// milliseconds per month, counting February as 28 days (compensating later)
var msPerMonth = [
  26784e5, 24192e5, 26784e5, 2592e6, 26784e5, 2592e6,
  26784e5, 26784e5, 2592e6, 26784e5, 2592e6, 26784e5
];

var msPerUnit = {
  f:      1,
  s:    1e3, // 1000
  i:    6e4, // 1000 * 60
  h:   36e5, // 1000 * 60 * 60
  d:  864e5, // 1000 * 60 * 60 * 24
  w: 6048e5, // 1000 * 60 * 60 * 24 * 7
  m: 0, // evaluates to false
  y: -1 // evaluates to true to distinguish it from months
};

// Aliases
msPerUnit.milliseconds = msPerUnit.f;
msPerUnit.millisecond = msPerUnit.f;
msPerUnit.ms = msPerUnit.f;
msPerUnit.seconds = msPerUnit.s;
msPerUnit.second = msPerUnit.s;
msPerUnit.minutes = msPerUnit.i;
msPerUnit.minute = msPerUnit.i;
msPerUnit.hours = msPerUnit.h;
msPerUnit.hour = msPerUnit.h;
msPerUnit.days = msPerUnit.d;
msPerUnit.day = msPerUnit.d;
msPerUnit.weeks = msPerUnit.w;
msPerUnit.week = msPerUnit.w;
msPerUnit.months = msPerUnit.m;
msPerUnit.month = msPerUnit.m;
msPerUnit.years = msPerUnit.y;
msPerUnit.year = msPerUnit.y;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add zeros for a total length of width chars (left padding by default)
////////////////////////////////////////////////////////////////////////////////

function zeropad(n, width, padRight) {
  padRight = padRight || false;
  n = "" + n;
  if (padRight) {
    return n.length >= width ? n : n + new Array(width - n.length + 1).join("0");
  } else {
    return n.length >= width ? n : new Array(width - n.length + 1).join("0") + n;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief raise a warning
////////////////////////////////////////////////////////////////////////////////

function WARN (func, error, data) {
  'use strict';

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
  'use strict';
  
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
  'use strict';

  RegexCache = { 'i' : { }, '' : { } };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the user functions and reload them from the database
////////////////////////////////////////////////////////////////////////////////

function reloadUserFunctions () {
  'use strict';

  var prefix = DB_PREFIX();
  var c = INTERNAL.db._collection("_aqlfunctions");

  if (c === null) {
    // collection not found. now reset all user functions
    UserFunctions = { };
    UserFunctions[prefix] = { };
    return;
  }

  var foundError = false;
  var functions = { };

  c.toArray().forEach(function (f) {
    var key = f._key.replace(/:{1,}/g, '::');
    var code;

    if (f.code.match(/^\(?function\s+\(/)) {
      code = f.code;
    }
    else {
      code = "(function() { var callback = " + f.code + ";\n return callback; })();";
    }

    try {
      var res = INTERNAL.executeScript(code, undefined, "(user function " + key + ")");
      if (typeof res !== "function") {
        foundError = true;
      }

      functions[key.toUpperCase()] = {
        name: key,
        func: res,
        isDeterministic: f.isDeterministic || false
      };

    }
    catch (err) {
      // in case a single function is broken, we still continue with the other ones
      // so that at least some functions remain usable
      foundError = true;
    }
  });
  
  // now reset the functions for all databases
  // this ensures that functions of other databases will be reloaded next 
  // time (the reload does not necessarily need to be carried out in the
  // database in which the function is registered)
  UserFunctions = { }; 
  UserFunctions[prefix] = functions;

  if (foundError) {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a user-function by name
////////////////////////////////////////////////////////////////////////////////

function GET_USERFUNCTION (name, config) { 
  var prefix = DB_PREFIX(), reloaded = false;
  var key = name.toUpperCase();

  var func;

  if (DefaultVisitors.hasOwnProperty(key)) {
    var visitor = DefaultVisitors[key];
    func = visitor.func;
    config.visitorReturnsResults = visitor.visitorReturnsResults;
  }
  else {
    if (! UserFunctions.hasOwnProperty(prefix)) {
      reloadUserFunctions();
      reloaded = true;
    }
  
    if (! UserFunctions[prefix].hasOwnProperty(key) && ! reloaded) {
      // last chance
      reloadUserFunctions();
    }
   
    if (! UserFunctions[prefix].hasOwnProperty(key)) {
      THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
    }

    func = UserFunctions[prefix][key].func;
  }

  if (typeof func !== "function") {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  return func;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a user-defined visitor from a function name
////////////////////////////////////////////////////////////////////////////////

function GET_VISITOR (name, config) {
  var func = GET_USERFUNCTION(name, config);

  return function (config, result, vertex, path) {
    try {
      if (config.visitorReturnsResults) {
        var r = func.apply(null, arguments);
        if (r !== undefined && r !== null) {
          result.push(CLONE(FIX_VALUE(r)));
        }
      }
      else {
        func.apply(null, arguments);
      }
    }
    catch (err) {
      WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err));
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a user-defined filter from a function name
////////////////////////////////////////////////////////////////////////////////

function GET_FILTER (name, config) {
  var func = GET_USERFUNCTION(name, config);

  return function (config, vertex, path) {
    try {
      var filterResult = func.apply(null, arguments);
      return FIX_VALUE(filterResult);
    }
    catch (err) {
      WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err));
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a user-defined expand filter from a function name
////////////////////////////////////////////////////////////////////////////////

function GET_EXPANDFILTER (name, config) {
  var func = GET_USERFUNCTION(name, config);

  return function (config, vertex, edge, path) {
    try {
      var filterResult = func.apply(null, arguments);
      return FIX_VALUE(filterResult);
    }
    catch (err) {
      WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err));
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a function name
////////////////////////////////////////////////////////////////////////////////

function NORMALIZE_FNAME (functionName) {
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

  if (obj === null || typeof(obj) !== "object" || obj instanceof ShapedJson) {
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
  'use strict';

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
  'use strict';

  if (value !== undefined && value !== null) {
    if (Array.isArray(value)) {
      return TYPEWEIGHT_ARRAY;
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
        return TYPEWEIGHT_OBJECT;
    }
  }

  return TYPEWEIGHT_NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a regex from a string pattern
////////////////////////////////////////////////////////////////////////////////
  
function CREATE_REGEX_PATTERN (chars) {
  'use strict';

  chars = AQL_TO_STRING(chars);
  var i, n = chars.length, pattern = '';
  var specialChar = /^([.*+?\^=!:${}()|\[\]\/\\])$/;

  for (i = 0; i < n; ++i) {
    var c = chars.charAt(i);
    if (c.match(specialChar)) {
      // character with special meaning in a regex
      pattern += '\\' + c;
    }
    else if (c === '\t') {
      pattern += '\\t';
    }
    else if (c === '\r') {
      pattern += '\\r';
    }
    else if (c === '\n') {
      pattern += '\\n';
    }
    else if (c === '\b') {
      pattern += '\\b';
    }
    else if (c === '\f') {
      pattern += '\\f';
    }
    else {
      pattern += c;
    }
  }

  return pattern;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a regex from a string pattern
////////////////////////////////////////////////////////////////////////////////

function COMPILE_REGEX (regex, modifiers) {
  'use strict';

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
  'use strict';

  var prefix = DB_PREFIX(), reloaded = false;
  if (! UserFunctions.hasOwnProperty(prefix)) {
    reloadUserFunctions();
    reloaded = true;
  }
  
  if (! UserFunctions[prefix].hasOwnProperty(name) && ! reloaded) {
    // last chance
    reloadUserFunctions();
  }
  
  if (! UserFunctions[prefix].hasOwnProperty(name)) {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  try {
    return FIX_VALUE(UserFunctions[prefix][name].func.apply(null, parameters));
  }
  catch (err) {
    WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err.stack || String(err)));
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dynamically call a function
////////////////////////////////////////////////////////////////////////////////

function FCALL_DYNAMIC (func, applyDirect, values, name, args) {
  var toCall;

  name = AQL_TO_STRING(name).toUpperCase();
  if (name.indexOf('::') > 0) {
    // user-defined function
    var prefix = DB_PREFIX(), reloaded = false;
    if (! UserFunctions.hasOwnProperty(prefix)) {
      reloadUserFunctions();
      reloaded = true;
    }

    if (! UserFunctions[prefix].hasOwnProperty(name) && ! reloaded) {
      // last chance
      reloadUserFunctions();
    }

    if (! UserFunctions[prefix].hasOwnProperty(name)) {
      THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
    }

    toCall = UserFunctions[prefix][name].func;
  }
  else {
    // built-in function
    if (name === "CALL" || name === "APPLY") {
      THROW(func, INTERNAL.errors.ERROR_QUERY_DISALLOWED_DYNAMIC_CALL, NORMALIZE_FNAME(name));
    }

    if (! exports.hasOwnProperty("AQL_" + name)) {
      THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, NORMALIZE_FNAME(name));
    }

    toCall = exports["AQL_" + name];
  }

  if (applyDirect) {
    try {
      return FIX_VALUE(toCall.apply(null, args));
    }
    catch (err) {
      WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err));
      return null;
    }
  }

  var type = TYPEWEIGHT(values), result, i;

  if (type === TYPEWEIGHT_OBJECT) {
    result = { };
    for (i in values) {
      if (values.hasOwnProperty(i)) {
        args[0] = values[i];
        result[i] = FIX_VALUE(toCall.apply(null, args));
      }
    }
    return result;
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    result = [ ];
    for (i = 0; i < values.length; ++i) {
      args[0] = values[i];
      result[i] = FIX_VALUE(toCall.apply(null, args));
    }
    return result;
  }
    
  WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the numeric value or undefined if it is out of range
////////////////////////////////////////////////////////////////////////////////

function NUMERIC_VALUE (value) {
  'use strict';

  if (isNaN(value) || ! isFinite(value)) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fix a value for a comparison
////////////////////////////////////////////////////////////////////////////////

function FIX (value) {
  'use strict';

  if (value === undefined) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the values of an object in the order that they are defined
////////////////////////////////////////////////////////////////////////////////

function VALUES (value) {
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

  if (TYPEWEIGHT(value) === TYPEWEIGHT_NULL) {
    return null;
  }

  var result = null;
  if (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT) {
    result = value[String(index)];
  }
  else if (TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY) {
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
  'use strict';

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
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_OBJECT) {
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
  'use strict';

  if (TYPEWEIGHT(id) === TYPEWEIGHT_ARRAY) {
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
  'use strict';

  // we're polymorphic
  if (id === undefined) {
    // called with a single parameter
    var weight = TYPEWEIGHT(collection);

    if (weight === TYPEWEIGHT_STRING || weight === TYPEWEIGHT_ARRAY) {
      return DOCUMENT_HANDLE(collection);
    }
  }

  if (TYPEWEIGHT(id) === TYPEWEIGHT_ARRAY) {
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

function GET_DOCUMENTS (collection) {
  'use strict';

  WARN(null, INTERNAL.errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION, AQL_TO_STRING(collection));

  if (isCoordinator) {
    return COLLECTION(collection).all().toArray();
  }

  return COLLECTION(collection).ALL(0, null).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get names of all collections
////////////////////////////////////////////////////////////////////////////////

function AQL_COLLECTIONS () {
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight !== rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight !== rightWeight) {
    return true;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

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
  'use strict';

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight < rightWeight) {
    return -1;
  }
  if (leftWeight > rightWeight) {
    return 1;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_ARRAY) {
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
  'use strict';

  var rightWeight = TYPEWEIGHT(rhs);

  if (rightWeight !== TYPEWEIGHT_ARRAY) {
    WARN(null, INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  return ! RELATIONAL_IN(lhs, rhs);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             arithmetic operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary plus operation
////////////////////////////////////////////////////////////////////////////////

function UNARY_PLUS (value) {
  'use strict';

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
  'use strict';

  value = AQL_TO_NUMBER(value);
  if (value === null) {
    return null;
  }

  return AQL_TO_NUMBER(- value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform arithmetic plus or string concatenation
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_PLUS (lhs, rhs) {
  'use strict';

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
/// @brief perform arithmetic minus
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MINUS (lhs, rhs) {
  'use strict';
  
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
/// @brief perform arithmetic multiplication
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_TIMES (lhs, rhs) {
  'use strict';
  
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
/// @brief perform arithmetic division
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_DIVIDE (lhs, rhs) {
  'use strict';

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
/// @brief perform arithmetic modulus
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MODULUS (lhs, rhs) {
  'use strict';

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
  'use strict';

  var result = '', i, j;

  for (i = 0; i < arguments.length; ++i) {
    var element = arguments[i];
    var weight = TYPEWEIGHT(element);
    if (weight === TYPEWEIGHT_NULL) {
      continue;
    }
    else if (weight === TYPEWEIGHT_ARRAY) {
      for (j = 0; j < element.length; ++j) {
        if (TYPEWEIGHT(element[j]) !== TYPEWEIGHT_NULL) {
          result += AQL_TO_STRING(element[j]);
        }
      } 
    }
    else {
      result += AQL_TO_STRING(element);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation using a separator character
////////////////////////////////////////////////////////////////////////////////

function AQL_CONCAT_SEPARATOR () {
  'use strict';

  var separator, found = false, result = '', i, j;

  for (i = 0; i < arguments.length; ++i) {
    var element = arguments[i];
    var weight = TYPEWEIGHT(element);
 
    if (i > 0 && weight === TYPEWEIGHT_NULL) {
      continue;
    }

    if (i === 0) {
      separator = AQL_TO_STRING(element);
      continue;
    }
    else if (found) {
      result += separator;
    }

    if (weight === TYPEWEIGHT_ARRAY) {
      found = false;
      for (j = 0; j < element.length; ++j) {
        if (TYPEWEIGHT(element[j]) !== TYPEWEIGHT_NULL) {
          if (found) {
            result += separator;
          }
          result += AQL_TO_STRING(element[j]);
          found = true;
        }
      } 
    }
    else {
      result += AQL_TO_STRING(element);
      found = true;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the length of a string in characters (not bytes)
////////////////////////////////////////////////////////////////////////////////

function AQL_CHAR_LENGTH (value) {
  'use strict';

  return AQL_TO_STRING(value).length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
////////////////////////////////////////////////////////////////////////////////

function AQL_LOWER (value) {
  'use strict';

  return AQL_TO_STRING(value).toLowerCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
////////////////////////////////////////////////////////////////////////////////

function AQL_UPPER (value) {
  'use strict';

  return AQL_TO_STRING(value).toUpperCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a substring of the string
////////////////////////////////////////////////////////////////////////////////

function AQL_SUBSTRING (value, offset, count) {
  'use strict';

  if (count !== undefined) {
    count = AQL_TO_NUMBER(count);
  }

  return AQL_TO_STRING(value).substr(AQL_TO_NUMBER(offset), count);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief searches a substring in a string
////////////////////////////////////////////////////////////////////////////////

function AQL_CONTAINS (value, search, returnIndex) {
  'use strict';

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
  'use strict';

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
  'use strict';

  return AQL_TO_STRING(value).substr(0, AQL_TO_NUMBER(length));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the rightmost parts of a string
////////////////////////////////////////////////////////////////////////////////

function AQL_RIGHT (value, length) {
  'use strict';

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

function AQL_TRIM (value, chars) {
  'use strict';

  if (chars === 1) {
    return AQL_LTRIM(value);
  }
  else if (chars === 2) {
    return AQL_RTRIM(value);
  }
  else if (chars === null || chars === undefined || chars === 0) {
    return AQL_TO_STRING(value).replace(new RegExp("(^\\s+|\\s+$)", 'g'), '');
  }

  var pattern = CREATE_REGEX_PATTERN(chars);
  return AQL_TO_STRING(value).replace(new RegExp("(^[" + pattern + "]+|[" + pattern + "]+$)", 'g'), '');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief trim a value from the left
////////////////////////////////////////////////////////////////////////////////

function AQL_LTRIM (value, chars) {
  'use strict';

  if (chars === null || chars === undefined) {
    chars = "^\\s+";
  }
  else {
    chars = "^[" + CREATE_REGEX_PATTERN(chars) + "]+";
  }

  return AQL_TO_STRING(value).replace(new RegExp(chars, 'g'), '');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief trim a value from the right
////////////////////////////////////////////////////////////////////////////////

function AQL_RTRIM (value, chars) {
  'use strict';

  if (chars === null || chars === undefined) {
    chars = "\\s+$";
  }
  else {
    chars = "[" + CREATE_REGEX_PATTERN(chars) + "]+$";
  }

  return AQL_TO_STRING(value).replace(new RegExp(chars, 'g'), '');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief split a string using a separator
////////////////////////////////////////////////////////////////////////////////

function AQL_SPLIT (value, separator, limit) {
  'use strict';

  if (separator === null || separator === undefined) {
    return [ AQL_TO_STRING(value) ];
  }

  if (limit === null || limit === undefined) {
    limit = undefined;
  }
  else {
    limit = AQL_TO_NUMBER(limit);
  }

  if (limit < 0) {
    WARN("SPLIT", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (TYPEWEIGHT(separator) === TYPEWEIGHT_ARRAY) {
    var patterns = [];
    separator.forEach(function(s) {
      patterns.push(CREATE_REGEX_PATTERN(AQL_TO_STRING(s)));
    });

    return AQL_TO_STRING(value).split(new RegExp(patterns.join("|"), "g"), limit);
  }

  return AQL_TO_STRING(value).split(AQL_TO_STRING(separator), limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a search value inside a string
////////////////////////////////////////////////////////////////////////////////

function AQL_SUBSTITUTE (value, search, replace, limit) {
  'use strict';

  var pattern, patterns, replacements = { }, sWeight = TYPEWEIGHT(search);
  value = AQL_TO_STRING(value);

  if (sWeight === TYPEWEIGHT_OBJECT) {
    patterns = [ ];
    KEYS(search, false).forEach(function(k) {
      patterns.push(CREATE_REGEX_PATTERN(k));
      replacements[k] = AQL_TO_STRING(search[k]);
    });
    pattern = patterns.join('|');
    limit = replace;
  }
  else if (sWeight === TYPEWEIGHT_STRING) {
    pattern = CREATE_REGEX_PATTERN(search);
    if (TYPEWEIGHT(replace) === TYPEWEIGHT_NULL) {
      replacements[search] = "";
    }
    else {
      replacements[search] = AQL_TO_STRING(replace);
    }
  }
  else if (sWeight === TYPEWEIGHT_ARRAY) {
    if (search.length === 0) {
      // empty list
      WARN("SUBSTITUTE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return value;
    }

    patterns = [ ];

    if (TYPEWEIGHT(replace) === TYPEWEIGHT_ARRAY) {
      // replace each occurrence with a member from the second list
      search.forEach(function(k, i) {
        k = AQL_TO_STRING(k);
        patterns.push(CREATE_REGEX_PATTERN(k));
        if (i < replace.length) {
          replacements[k] = AQL_TO_STRING(replace[i]);
        }
        else {
          replacements[k] = "";
        }
      });
    }
    else {
      // replace all occurrences with a constant string
      if (TYPEWEIGHT(replace) === TYPEWEIGHT_NULL) {
        replace = "";
      }
      else {
        replace = AQL_TO_STRING(replace);
      }
      search.forEach(function(k, i) {
        k = AQL_TO_STRING(k);
        patterns.push(CREATE_REGEX_PATTERN(k));
        replacements[k] = replace;
      });
    }
    pattern = patterns.join('|');
  }

  if (limit === null || limit === undefined) {
    limit = undefined;
  }
  else {
    limit = AQL_TO_NUMBER(limit);
  }

  if (limit < 0) {
    WARN("SUBSTITUTE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  return AQL_TO_STRING(value).replace(new RegExp(pattern, 'g'), function(match) {
    if (limit === undefined) {
      return replacements[match];
    } 
    if (limit > 0) {
      --limit;
      return replacements[match];
    }
    return match;
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates the MD5 value for a string
////////////////////////////////////////////////////////////////////////////////

function AQL_MD5 (value) {
  'use strict';

  return INTERNAL.md5(AQL_TO_STRING(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates the SHA1 value for a string
////////////////////////////////////////////////////////////////////////////////

function AQL_SHA1 (value) {
  'use strict';

  return INTERNAL.sha1(AQL_TO_STRING(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a random token of the specified length
////////////////////////////////////////////////////////////////////////////////

function AQL_RANDOM_TOKEN (length) {
  'use strict';

  length = AQL_TO_NUMBER(length);

  if (length <= 0 || length > 65536) {
    THROW("RANDOM_TOKEN", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANDOM_TOKEN");
  }

  return INTERNAL.genRandomAlphaNumbers(AQL_TO_NUMBER(length));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds search in value
////////////////////////////////////////////////////////////////////////////////

function AQL_FIND_FIRST (value, search, start, end) {
  'use strict';

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
  'use strict';

  if (start !== undefined && start !== null) {
    start = AQL_TO_NUMBER(start);
  }
  else {
    start = undefined;
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

  var result;
  if (start > 0 || end !== undefined) {
    if (end === undefined) {
      result = AQL_TO_STRING(value).substr(start).lastIndexOf(AQL_TO_STRING(search));
    }
    else {
      result = AQL_TO_STRING(value).substr(start, end - start + 1).lastIndexOf(AQL_TO_STRING(search));
    }
    if (result !== -1) {
      result += start;
    }
  }
  else {
    result = AQL_TO_STRING(value).lastIndexOf(AQL_TO_STRING(search));
  }
  return result;
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
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return false;
    case TYPEWEIGHT_BOOL:
      return value;
    case TYPEWEIGHT_NUMBER:
      return (value !== 0);
    case TYPEWEIGHT_STRING:
      return (value !== '');
    case TYPEWEIGHT_ARRAY:
    case TYPEWEIGHT_OBJECT:
      return true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a number
///
/// the operand can have any type, returns a number or null
////////////////////////////////////////////////////////////////////////////////

function AQL_TO_NUMBER (value) {
  'use strict';

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
    case TYPEWEIGHT_ARRAY:
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
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return 'null';
    case TYPEWEIGHT_BOOL:
      return (value ? 'true' : 'false');
    case TYPEWEIGHT_STRING:
      return value;
    case TYPEWEIGHT_NUMBER:
    case TYPEWEIGHT_ARRAY:
    case TYPEWEIGHT_OBJECT:
      return value.toString();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to an array
///
/// the operand can have any type, always returns a list
////////////////////////////////////////////////////////////////////////////////

function AQL_TO_ARRAY (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return [ ];
    case TYPEWEIGHT_BOOL:
    case TYPEWEIGHT_NUMBER:
    case TYPEWEIGHT_STRING:
      return [ value ];
    case TYPEWEIGHT_ARRAY:
      return value;
    case TYPEWEIGHT_OBJECT:
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
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type bool
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_BOOL (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type number
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_NUMBER (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NUMBER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type string
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_STRING (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_STRING);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type array
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_ARRAY (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type object
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_OBJECT (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 numeric functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value, not greater than value
////////////////////////////////////////////////////////////////////////////////

function AQL_FLOOR (value) {
  'use strict';

  return NUMERIC_VALUE(Math.floor(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value and not less than value
////////////////////////////////////////////////////////////////////////////////

function AQL_CEIL (value) {
  'use strict';

  return NUMERIC_VALUE(Math.ceil(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value
////////////////////////////////////////////////////////////////////////////////

function AQL_ROUND (value) {
  'use strict';

  return NUMERIC_VALUE(Math.round(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief absolute value
////////////////////////////////////////////////////////////////////////////////

function AQL_ABS (value) {
  'use strict';

  return NUMERIC_VALUE(Math.abs(AQL_TO_NUMBER(value)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a random value between 0 and 1
////////////////////////////////////////////////////////////////////////////////

function AQL_RAND () {
  'use strict';

  return Math.random();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief square root
////////////////////////////////////////////////////////////////////////////////

function AQL_SQRT (value) {
  'use strict';

  return NUMERIC_VALUE(Math.sqrt(AQL_TO_NUMBER(value)));
}

// -----------------------------------------------------------------------------
// --SECTION--                                         list processing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of a list, document or string
////////////////////////////////////////////////////////////////////////////////

function AQL_LENGTH (value) {
  'use strict';

  var typeWeight = TYPEWEIGHT(value);

  if (typeWeight === TYPEWEIGHT_ARRAY) {
    return value.length;
  }
  else if (typeWeight === TYPEWEIGHT_OBJECT) {
    return KEYS(value, false).length;
  }
  else if (typeWeight === TYPEWEIGHT_NULL) {
    return 0;
  }
  else if (typeWeight === TYPEWEIGHT_BOOL) {
    return value ? 1 : 0;
  }

  return AQL_TO_STRING(value).length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the first element of a list
////////////////////////////////////////////////////////////////////////////////

function AQL_FIRST (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN("FIRST", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN("LAST", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN("POSITION", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN("NTH", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    return value.split("").reverse().join("");
  }

  if (TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY) {
    return CLONE(value).reverse();
  }
    
  WARN("REVERSE", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a range of values
////////////////////////////////////////////////////////////////////////////////

function AQL_RANGE (from, to, step) {
  'use strict';

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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("UNIQUE", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  var result = [ ], i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
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
  'use strict';

  var keys = { }, i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
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
/// @brief call a function for each element in the input list
////////////////////////////////////////////////////////////////////////////////

function AQL_CALL (name) {
  'use strict';

  var args = [ ], i;
  for (i = 1; i < arguments.length; ++i) {
    args.push(arguments[i]);
  }

  return FCALL_DYNAMIC("CALL", true, null, name, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief call a function for each element in the input list
////////////////////////////////////////////////////////////////////////////////

function AQL_APPLY (name, parameters) {
  'use strict';

  var args = [ ], i;
  if (Array.isArray(parameters)) {
    args = args.concat(parameters);
  }

  return FCALL_DYNAMIC("APPLY", true, null, name, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes elements from a list
////////////////////////////////////////////////////////////////////////////////

function AQL_REMOVE_VALUES (list, values) {
  'use strict';

  var type = TYPEWEIGHT(values);
  if (type === TYPEWEIGHT_NULL) {
    return list;
  }
  else if (type !== TYPEWEIGHT_ARRAY) {
    WARN("REMOVE_VALUES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ ];
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    var copy = [ ], i;
    for (i = 0; i < list.length; ++i) {
      if (RELATIONAL_IN(list[i], values)) {
        continue;
      }
      copy.push(CLONE(list[i]));
    }
    return copy;
  }

  WARN("REMOVE_VALUES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from a list
////////////////////////////////////////////////////////////////////////////////

function AQL_REMOVE_VALUE (list, value, limit) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ ];
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    if (TYPEWEIGHT(limit) === TYPEWEIGHT_NULL) {
      limit = -1;
    }

    var copy = [ ], i;
    for (i = 0; i < list.length; ++i) {
      if (limit === -1 && RELATIONAL_CMP(list[i], value) === 0) {
        continue;
      }
      else if (limit > 0 && RELATIONAL_CMP(list[i], value) === 0) {
        --limit;
        continue;
      }
      copy.push(CLONE(list[i]));
    }
    return copy;
  }

  WARN("REMOVE_VALUE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from a list
////////////////////////////////////////////////////////////////////////////////

function AQL_REMOVE_NTH (list, position) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ ];
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    position = AQL_TO_NUMBER(position);
    if (position >= list.length || position < - list.length) {
      return list;
    }
    if (position === 0) {
      return list.slice(1);
    }
    else if (position === - list.length) {
      return list.slice(position + 1);
    }
    else if (position === list.length - 1) {
      return list.slice(0, position);
    }
    else if (position < 0) {
      return list.slice(0, list.length + position).concat(list.slice(list.length + position + 1));
    }

    return list.slice(0, position).concat(list.slice(position + 1));
  }

  WARN("REMOVE_NTH", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to a list
////////////////////////////////////////////////////////////////////////////////

function AQL_PUSH (list, value, unique) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ value ];
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    if (AQL_TO_BOOL(unique)) {
      if (RELATIONAL_IN(value, list)) {
        return list;
      } 
    }

    var copy = CLONE(list);
    copy.push(value);
    return copy;
  }

  WARN("PUSH", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds elements to a list
////////////////////////////////////////////////////////////////////////////////

function AQL_APPEND (list, values, unique) {
  'use strict';

  var type = TYPEWEIGHT(values);
  if (type === TYPEWEIGHT_NULL) {
    return list;
  }
  else if (type !== TYPEWEIGHT_ARRAY) {
    values = [ values ];
  }

  if (values.length === 0) {
    return list;
  }

  unique = AQL_TO_BOOL(unique);
  if (values.length > 1 && unique) {
    // make values unique themselves
    values = AQL_UNIQUE(values);
  }

  type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return values;
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    var copy = CLONE(list);
    if (unique) {
      var i;
      for (i = 0; i < values.length; ++i) {
        if (RELATIONAL_IN(values[i], list)) {
          continue;
        }
        copy.push(values[i]);
      }
      return copy;
    }
    return copy.concat(values);
  }

  WARN("APPEND", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops an element from a list
////////////////////////////////////////////////////////////////////////////////

function AQL_POP (list) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return null;
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    if (list.length === 0) {
      return [ ];
    }
    var copy = CLONE(list);
    copy.pop();

    return copy;
  }

  WARN("POP", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an element into a list
////////////////////////////////////////////////////////////////////////////////

function AQL_UNSHIFT (list, value, unique) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ value ];
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    if (unique) {
      if (RELATIONAL_IN(value, list)) {
        return list;
      } 
    }

    var copy = CLONE(list);
    copy.unshift(value);
    return copy;
  }

  WARN("UNSHIFT", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops an element from a list
////////////////////////////////////////////////////////////////////////////////

function AQL_SHIFT (list) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return null;
  }
  else if (type === TYPEWEIGHT_ARRAY) {
    if (list.length === 0) {
      return [ ];
    }
    var copy = CLONE(list);
    copy.shift();

    return copy;
  }

  WARN("SHIFT", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a slice from an array
////////////////////////////////////////////////////////////////////////////////

function AQL_SLICE (value, from, to, nonNegative) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN("SLICE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  from = AQL_TO_NUMBER(from);
  to   = AQL_TO_NUMBER(to);

  if (nonNegative && (from < 0 || to < 0)) {
    return [ ];
  }

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
  'use strict';

  var keys = { }, i, first = true;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
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
  'use strict';

  var result = [ ], i, first = true, keys = { };

  var func = function (value) {
    var normalized = NORMALIZE(value);
    keys[JSON.stringify(normalized)] = normalized;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("FLATTEN", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
    if (depth < maxDepth && TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY) {
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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("MAX", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("MIN", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("SUM", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("AVERAGE", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("MEDIAN", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
/// @brief returns the pth percentile of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_PERCENTILE (values, p, method) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("PERCENTILE", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  if (TYPEWEIGHT(p) !== TYPEWEIGHT_NUMBER) {
    WARN("PERCENTILE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (p <= 0 || p > 100) {
    WARN("PERCENTILE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
 
  if (method === null || method === undefined) {
    method = "rank";
  }
   
  if (method !== "interpolation" && method !== "rank") {
    WARN("PERCENTILE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var copy = [ ], current, typeWeight;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN("PERCENTILE", INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
        return null;
      }

      copy.push(current);
    }
  }

  if (copy.length === 0) {
    return null;
  }
  else if (copy.length === 1) {
    return copy[0];
  }

  copy.sort(RELATIONAL_CMP);

  var idx, pos;
  if (method === "interpolation") {
    // interpolation method
    idx = p * (copy.length + 1) / 100;
    pos = Math.floor(idx);
    var delta = idx - pos;

    if (pos >= copy.length) {
      return NUMERIC_VALUE(copy[copy.length - 1]);
    }
    return NUMERIC_VALUE(delta * (copy[pos] - copy[pos - 1]) + copy[pos - 1]);
  }
  else {
    // rank method
    idx = p * (copy.length) / 100;
    pos = Math.ceil(idx);

    if (pos >= copy.length) {
      return NUMERIC_VALUE(copy[copy.length - 1]);
    }
    return NUMERIC_VALUE(copy[pos - 1]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief variance of all values
////////////////////////////////////////////////////////////////////////////////

function VARIANCE (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN("VARIANCE", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

  var weight = TYPEWEIGHT(distanceAttribute);
  if (weight !== TYPEWEIGHT_NULL && weight !== TYPEWEIGHT_STRING) {
    WARN("WITHIN", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }
  
  if (isCoordinator) {
    var query = COLLECTION(collection).within(latitude, longitude, radius);
    query._distance = distanceAttribute;
    return query.toArray();
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
  'use strict';

  if (TYPEWEIGHT(latitude1) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(longitude1) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(latitude2) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(longitude2) !== TYPEWEIGHT_NUMBER) {
    WARN("WITHIN_RECTANGLE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  
  return COLLECTION(collection).withinRectangle(latitude1, longitude1, latitude2, longitude2).toArray();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return true if a point is contained inside a polygon
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_IN_POLYGON (points, latitude, longitude) {
  'use strict';
  
  if (TYPEWEIGHT(points) !== TYPEWEIGHT_ARRAY) {
    WARN("POINT_IN_POLYGON", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return false;
  }

  var searchLat, searchLon, pointLat, pointLon, geoJson = false;
  if (TYPEWEIGHT(latitude) === TYPEWEIGHT_ARRAY) {
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
    if (TYPEWEIGHT(points[i]) !== TYPEWEIGHT_ARRAY) {
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

function AQL_FULLTEXT (collection, attribute, query, limit) {
  'use strict';

  var idx = INDEX_FULLTEXT(COLLECTION(collection), attribute);

  if (idx === null) {
    THROW("FULLTEXT", INTERNAL.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING, collection);
  }

  if (isCoordinator) {
    if (limit !== undefined && limit !== null && limit > 0) {
      return COLLECTION(collection).fulltext(attribute, query, idx).limit(limit).toArray();
    }
    return COLLECTION(collection).fulltext(attribute, query, idx).toArray();
  }

  return COLLECTION(collection).FULLTEXT(idx, query, limit).documents;
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
  'use strict';

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
  'use strict';

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) === TYPEWEIGHT_ARRAY) {
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
  'use strict';

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) === TYPEWEIGHT_OBJECT) {
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
  'use strict';

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
  else if (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT) {
    if (value.hasOwnProperty('_id')) {
      return AQL_PARSE_IDENTIFIER(value._id);
    }
    // fall through intentional
  }

  WARN("PARSE_IDENTIFIER", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a document has a specific attribute
////////////////////////////////////////////////////////////////////////////////

function AQL_HAS (element, name) {
  'use strict';
 
  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    return false;
  }

  return element.hasOwnProperty(AQL_TO_STRING(name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the attribute names of a document as a list
////////////////////////////////////////////////////////////////////////////////

function AQL_ATTRIBUTES (element, removeInternal, sort) {
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    WARN("ATTRIBUTES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (removeInternal) {
    var result = [ ];

    Object.keys(element).forEach(function(k) {
      if (k[0] !== '_') {
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
/// @brief return the attribute values of a document as a list
////////////////////////////////////////////////////////////////////////////////

function AQL_VALUES (element, removeInternal) {
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    WARN("VALUES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = [ ], a;

  for (a in element) {
    if (element.hasOwnProperty(a)) {
      if (a[0] !== '_' || ! removeInternal) {
        result.push(element[a]);
      } 
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document from two lists
////////////////////////////////////////////////////////////////////////////////

function AQL_ZIP (keys, values) {
  'use strict';

  if (TYPEWEIGHT(keys) !== TYPEWEIGHT_ARRAY ||
      TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY ||
      keys.length !== values.length) {
    WARN("ZIP", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, i, n = keys.length;

  for (i = 0; i < n; ++i) {
    result[AQL_TO_STRING(keys[i])] = values[i];
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unset specific attributes from a document
////////////////////////////////////////////////////////////////////////////////

function AQL_UNSET (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_OBJECT) {
    WARN("UNSET", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, "UNSET");
  // copy over all that is left

  for (var k in value) {
    if (value.hasOwnProperty(k) && ! keys.hasOwnProperty(k)) {
      result[k] = CLONE(value[k]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief keep specific attributes from a document
////////////////////////////////////////////////////////////////////////////////

function AQL_KEEP (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_OBJECT) {
    WARN("KEEP", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, "KEEP");

  // copy over all that is left
  for (var k in value) {
    if (value.hasOwnProperty(k) && keys.hasOwnProperty(k)) {
      result[k] = CLONE(value[k]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments
////////////////////////////////////////////////////////////////////////////////

function AQL_MERGE () {
  'use strict';

  var result = { };

  var add = function (element) {
    for (var k in element) {
      if (element.hasOwnProperty(k)) {
        result[k] = element[k];
      }
    }
  };

  var j = 0;
  for (var i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
        WARN("MERGE", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      if (j === 0) {
        result = element;
      }
      else {
        add(element);
      }
      ++j;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments recursively
////////////////////////////////////////////////////////////////////////////////

function AQL_MERGE_RECURSIVE () {
  'use strict';

  var result = { }, i, recurse;

  recurse = function (old, element) {
    var r = CLONE(old);

    Object.keys(element).forEach(function(k) {
      if (r.hasOwnProperty(k) && TYPEWEIGHT(element[k]) === TYPEWEIGHT_OBJECT) {
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

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
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
  'use strict';

  if (TYPEWEIGHT(lookup) !== TYPEWEIGHT_OBJECT) {
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
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
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
    if (TYPEWEIGHT(example) !== TYPEWEIGHT_OBJECT) {
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
  'use strict';

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test helper function
/// this is no actual function the end user should call
////////////////////////////////////////////////////////////////////////////////

function AQL_TEST_MODIFY (test, what) {
  'use strict';
  if (test === 'MODIFY_ARRAY') {
    what[0] = 1; 
    what[1] = 42; 
    what[2] = [ 1, 2 ]; 
    what[3].push([ 1, 2 ]); 
    what[4] = { a: 9, b: 2 };
    what.push("foo");
    what.push("bar");
    what.pop();
  }
  else if (test === 'MODIFY_OBJECT') {
    what.a = 1; 
    what.b = 3; 
    what.c = [ 1, 2 ]; 
    what.d.push([ 1, 2 ]); 
    what.e.f = { a: 1, b: 2 };
    delete what.f; 
    what.g = "foo";
  }
  return what; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep
///
/// sleep for the specified duration
////////////////////////////////////////////////////////////////////////////////

function AQL_SLEEP (duration) {
  'use strict';

  duration = AQL_TO_NUMBER(duration);
  if (TYPEWEIGHT(duration) !== TYPEWEIGHT_NUMBER || duration < 0) {
    WARN("SLEEP", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  AQL_QUERY_SLEEP(duration);
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current user
/// note: this might be null if the query is not executed in a context that
/// has a user
////////////////////////////////////////////////////////////////////////////////

function AQL_CURRENT_USER () {
  'use strict';

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
  'use strict';

  return INTERNAL.db._name();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief always fail
///
/// this function is non-deterministic so it is not executed at query
/// optimisation time. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function AQL_FAIL (message) {
  'use strict';

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
  'use strict';

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

    // detect invalid dates ("foo" -> "fooZ" -> getTime() == NaN)
    var date = new Date(args[0]);
    if (isNaN(date)) {
      WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE, func);
      return null;
    }
    return date;
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

  // TODO: add check if Date is NaN? Note: avoid duplicate warnings!
  return new Date(Date.UTC.apply(null, args));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of milliseconds since the Unix epoch
///
/// this function is evaluated on every call
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_NOW () {
  'use strict';

  return Date.now();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the timestamp of the date passed (in milliseconds)
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_TIMESTAMP () {
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

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
  'use strict';

  try {
    return MAKE_DATE([ value ], "DATE_MILLISECOND").getUTCMilliseconds();
  }
  catch (err) {
    WARN("DATE_MILLISECOND", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the day of the year of the date passed
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAYOFYEAR (value) {
  'use strict';

  try {
    var date = MAKE_DATE([ value ], "DATE_DAYOFYEAR");
    var m = date.getUTCMonth();
    var d = date.getUTCDate();
    var ly = AQL_DATE_LEAPYEAR(date.getTime());
    // we could duplicate the leap year code here to avoid an extra MAKE_DATE() call...
    //var yr = date.getUTCFullYear();
    //var ly = !((yr % 4) || (!(yr % 100) && (yr % 400)));
    return (ly ? (dayOfLeapYearOffsets[m] + d) : (dayOfYearOffsets[m] + d));
  }
  catch (err) {
    WARN("DATE_DAYOFYEAR", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the ISO week date of the date passed (1..53)
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_ISOWEEK (value) {
  'use strict';

  try {
    var date = MAKE_DATE([ value ], "DATE_ISOWEEK");
    date.setUTCHours(0, 0, 0, 0);
    date.setUTCDate(date.getUTCDate() + 4 - (date.getUTCDay() || 7));
    return Math.ceil((((date - Date.UTC(date.getUTCFullYear(), 0, 1)) / 864e5) + 1) / 7);
  }
  catch (err) {
    WARN("DATE_ISOWEEK", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if year of the date passed is a leap year
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_LEAPYEAR (value) {
  'use strict';

  try {
    var yr = MAKE_DATE([ value ], "DATE_LEAPYEAR").getUTCFullYear();
    return ((yr % 4 === 0) && (yr % 100 !== 0)) || (yr % 400 === 0);
  }
  catch (err) {
    WARN("DATE_LEAPYEAR", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the ISO week date of the date passed (1..53)
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_QUARTER (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], "DATE_QUARTER").getUTCMonth() / 3 + 1 | 0;
  }
  catch (err) {
    WARN("DATE_QUARTER", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to add or subtract from date
////////////////////////////////////////////////////////////////////////////////

function DATE_CALC(value, amount, unit, func) {
  'use strict';

  try {
    // TODO: check if isNaN? If called by AQL FOR-loop, should query throw
    // and terminate immediately, or return a bunch of 'null's? If it shall
    // stop, then best handled in MAKE_DATE() itself I guess.
    var date = MAKE_DATE([ value ], func);
    var sign = (func === "DATE_ADD" || func === undefined) ? 1 : -1;
    var m;
    
    // if amount is not a number, than it must be an ISO duration string
    if (isNaN(amount)) {
      if (unit !== undefined) {
        WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }
      // ISO duration cache
      var duration;
      if (ISODurationCache[amount] === undefined) {
        duration = ISODurationRegex.exec(amount);
        if (!duration) {
          WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
          return null;
        }
        ISODurationCache[amount] = duration;
      } else {
        duration = ISODurationCache[amount];
      }
      // ensure ms has a length of 3
      if (duration[8] && duration[8].length !== 3) {
        duration[8] = zeropad(duration[8], 3, true).substring(0, 3);
      }
      // add or subtract component by component, from ms to year
      for (var d=duration.length-1; d>=1; d--) {
        if (duration[d]) {
          // convert weeks to days
          if (d === 3) {
            m = unitMapping[unitMappingArray[4]];
            date[m[2]](date[m[1]]() + duration[d] * sign * 7);
          } else {
            m = unitMapping[unitMappingArray[d]];
            date[m[2]](date[m[1]]() + duration[d] * sign);
          }
        }
      }
      return date.toISOString(); 
    } else {
      if (unit === undefined || typeof unit !== "string") {
        WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }
      m = unitMapping[unit.toLowerCase()]; // TODO: AQL_TO_STRING?
      if (m === "undefined") {
        WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
        return null;
      } else if (m[0] === "w") {
        m = unitMapping.d;
        date[m[2]](date[m[1]]() + amount * 7 * sign);
      } else {
        date[m[2]](date[m[1]]() + amount * sign);
      }
      return date.toISOString();
    }
  }
  catch (err) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return date passed with added amount of time units
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_ADD (value, amount, unit) {
  'use strict';
  return DATE_CALC(value, amount, unit, "DATE_ADD");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return date passed with subtracted amount of time units
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_SUBTRACT (value, amount, unit) {
  'use strict';
  return DATE_CALC(value, amount, unit, "DATE_SUBTRACT");
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return date difference in given unit, optionally with fractions
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DIFF (value1, value2, unit, asFloat) {
  'use strict';

  var date1 = MAKE_DATE([ value1 ], "DATE_DIFF");
  var date2 = MAKE_DATE([ value2 ], "DATE_DIFF");
  // Don't return any number if either or both dates are NaN.
  if (date1 === null || date2 === null) {
    // warning issued by MAKE_DATE() already (duplicate warnings in other date function calls???)
    return null;
  }

  asFloat = (asFloat === undefined) ? false : asFloat;
  if (date1 === date2) {
    return 0;
  }

  try {
    var divisor = msPerUnit[unit.toLowerCase()];
    if (divisor === undefined) {
      WARN("DATE_DIFF", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
      return null;
    }

    // simple calculation if not month or year
    if (divisor > 0) {
      return (asFloat) ? ((date2 - date1) / divisor) : (~~((date2 - date1) / divisor));
    }

    var year1 = date1.getUTCFullYear();
    var month1 = date1.getUTCMonth();
    var year2 = date2.getUTCFullYear();
    var month2 = date2.getUTCMonth();

    // ms in given month, leap year compensated if February and leap year
    var month1ms = msPerMonth[month1] + ((month1 === 1 && AQL_DATE_LEAPYEAR(date1.getTime())) ? msPerUnit.d : 0);
    // ms since beginning of month
    var month1msOffset = date1 - Date.UTC(year1, month1);
    var month2ms = msPerMonth[month2] + ((month2 === 1 && AQL_DATE_LEAPYEAR(date2.getTime())) ? msPerUnit.d : 0);
    var month2msOffset = date2 - Date.UTC(year2, month2);

    // Diff between YYYY-MM parts only.
    var diff = (year2 * 12 + month2) - (year1 * 12 + month1);

    // Add diff between month offsets relative to the mean of ms in both months
    diff += ((month2msOffset - month1msOffset) / ((month1ms + month2ms) / 2));

    // return 1/12th of month if unit is year
    if (asFloat) {
      return divisor ? diff / 12 : diff;
    } else {
      // round towards zero, regardless of sign
      return divisor ? ~~(diff / 12) : ~~diff;
    }
  }
  catch (err) {
    WARN("DATE_DIFF", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two partial dates, using a substring of the dates
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_COMPARE (value1, value2, unitRangeStart, unitRangeEnd) {
  try {
    // TODO: Should we handle leap years, so leapling birthdays occur every year?
    // It may result in unexpected behavior if it's used for something else but
    // birthday checking however. Probably best to leave compensation up to user query.
    var date1 = MAKE_DATE([ value1 ], "DATE_COMPARE");
    var date2 = MAKE_DATE([ value2 ], "DATE_COMPARE");
    if (isNaN(date1) || isNaN(date2)) {
      return null;
    }
    if (unitRangeEnd === undefined) {
      unitRangeEnd = unitRangeStart;
    }
    var start = unitStrRanges[unitRangeStart][0];
    var end = unitStrRanges[unitRangeEnd][1];
    if (start === undefined || end === undefined) {
      WARN("DATE_COMPARE", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
      return null;
    }
    if (date1.getUTCFullYear() < 0 && start !== 0) {
      start += 3;
    }
    if (date2.getUTCFullYear() < 0) {
      end += 3;
    }
    var substr1 = date1.toISOString().slice(start, end);
    var substr2 = date2.toISOString().slice(start, end);
    // if unitRangeEnd > unitRangeStart, substrings will be empty
    if (!substr1 || !substr2) {
      WARN("DATE_COMPARE", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
      return null;
    }
    return substr1 === substr2;
  } catch (err) {
    WARN("DATE_COMPARE", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief format a date (numerical values only)
////////////////////////////////////////////////////////////////////////////////

function AQL_DATE_FORMAT (value, format) {
  'use strict';
  try {
    var date = MAKE_DATE([ value ], "DATE_FORMAT");
    var dateStr = date.toISOString();
    var offset = date.getUTCFullYear() < 0 ? 3 : 0;
    var dateMap = {
      "%t": date.getTime(),
      "%o": dateStr,
      "%w": AQL_DATE_DAYOFWEEK(dateStr),
      "%y": dateStr.slice(0, 4 + offset),
      "%m": dateStr.slice(5 + offset, 7 + offset),
      "%d": dateStr.slice(8 + offset, 10 + offset),
      "%h": dateStr.slice(11 + offset, 13 + offset),
      "%i": dateStr.slice(14 + offset, 16 + offset),
      "%s": dateStr.slice(17 + offset, 19 + offset),
      "%f": dateStr.slice(20 + offset, 23 + offset),
      "%x": zeropad(AQL_DATE_DAYOFYEAR(dateStr), 3),
      "%k": zeropad(AQL_DATE_ISOWEEK(dateStr), 2),
      "%l": +AQL_DATE_LEAPYEAR(dateStr),
      "%q": AQL_DATE_QUARTER(dateStr),
      "%%": "%" // Allow for literal "%Y" using "%%Y"
      //"%": "" // Not reliable, because Object.keys() does not guarantee order
    };
    var exp = new RegExp(Object.keys(dateMap).join("|"), "gi"); 
    format = format.replace(exp, function(match){
      return dateMap[match.toLowerCase()];
    });
    return format;
  } catch (err) {
    WARN("DATE_FORMAT", INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
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
  'use strict';

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

function AQL_PATHS (vertices, edgeCollection, direction, options) {
  'use strict';

  direction      = direction || "outbound";
  followCycles   = followCycles || false;

  if (typeof options === "boolean") {
    options = { followCycles : options };
  }
  else if (typeof options !== "object" || Array.isArray(options)) {
    options = { };
  }

  var followCycles   = options.followCycles || false;
  var minLength      = options.minLength || 0;
  var maxLength      = options.maxLength || 10;

  if (TYPEWEIGHT(vertices) !== TYPEWEIGHT_ARRAY) {
    WARN("PATHS", INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  // validate arguments
  var searchDirection;
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   db._query("RETURN GRAPH_PATHS('social')").toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Return all inbound paths of the graph "social" with a maximal
/// length of 1 and a minimal length of 2:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphPaths2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
/// | db._query(
/// | "RETURN GRAPH_PATHS('social', {direction : 'inbound', minLength : 1, maxLength :  2})"
///   ).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_PATHS (graphName, options) {
  'use strict';

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
  'use strict';

  if (config.trackPaths) {
    result.push(CLONE({ vertex: vertex, path: path }));
  }
  else {
    result.push(CLONE({ vertex: vertex }));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function SHORTEST_PATH_VISITOR (config, result, vertex, path) {
  'use strict';

  if (this.includeData) {
    result.push({
      vertices: CLONE(path.vertices),
      edges: CLONE(path.edges),
      distance: path.edges.length
    });
  } else {
    result.push({
      vertices: underscore.pluck(path.vertices, "_id"),
      edges: underscore.pluck(path.edges, "_id"),
      distance: path.edges.length
    });
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for neighbors
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_NEIGHBOR_VISITOR (config, result, vertex, path) {
  'use strict';
  // The this has to be bound explicitly!
  // It contains additional options.
  if (path.edges.length >= this.minDepth) {
    result.push(CLONE(vertex));
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for edges
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_EDGE_VISITOR (config, result, vertex, path) {
  'use strict';
  // The this has to be bound explicitly!
  // It contains additional options.
  if (this.hasOwnProperty("minDepth") && path.edges.length < this.minDepth) {
    return;
  }
  if (this.hasOwnProperty("neighborExamples") && !AQL_MATCHES(vertex, this.neighborExamples, false)) {
    return;
  }
  result.push(CLONE(path.edges[path.edges.length - 1]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for tree traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_TREE_VISITOR (config, result, vertex, path) {
  'use strict';

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
  'use strict';
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
  'use strict';
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
  ) {
    return ["exclude"];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check typeweights of params.followEdges/params.filterVertices
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS (examples, func) {
  'use strict';

  if (TYPEWEIGHT(examples) === TYPEWEIGHT_STRING) {
    // a callback function was supplied. this is considered valid
    return true;
  }

  if (TYPEWEIGHT(examples) !== TYPEWEIGHT_ARRAY) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }
  if (examples.length === 0) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }

  var i;
  for (i = 0; i < examples.length; ++i) {
    if (TYPEWEIGHT(examples[i]) !== TYPEWEIGHT_OBJECT) {
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
  'use strict';

  if (typeof vertex === 'object' && vertex !== null && vertex.hasOwnProperty('_id')) {
    return vertex._id;
  }

  if (typeof vertex === 'string' && vertex.indexOf('/') === -1 && collection) {
    return collection + '/' + vertex;
  }

  return vertex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create basic traversal config
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_CONFIG (func, datasource, startVertex, endVertex, direction, params) {
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
    trackPaths: params.paths || false,
    visitor: params.visitor,
    visitorReturnsResults: params.visitorReturnsResults || false,
    maxDepth: params.maxDepth,
    minDepth: params.minDepth,
    maxIterations: params.maxIterations,
    uniqueness: params.uniqueness,
    strategy: params.strategy,
    order: params.order,
    itemOrder: params.itemOrder,
    weight : params.weight,
    defaultWeight : params.defaultWeight,
    prefill : params.prefill,
    data: params.data,
    datasource: datasource,
    expander: direction,
    direction: direction,
    startVertex: startVertex,
    endVertex: endVertex
  };

  if (typeof params.filter === "function") {
    config.filter = params.filter;
  } 

  if (params.followEdges) {
    if (typeof params.followEdges === 'string') {
      config.expandFilter = GET_EXPANDFILTER(params.followEdges, params);
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
      config.filter = GET_FILTER(params.filterVertices, params);
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
    if (typeof endVertex === "string") {
      try {
        e = INTERNAL.db._document(endVertex);
      }
      catch (err2) {
      }
    } else {
      e = endVertex;
    }
  }

  return {
    config: config,
    endVertex: e,
    vertex: v
  };
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
  'use strict';

  var info = TRAVERSAL_CONFIG(func, datasource, startVertex, endVertex, direction, params);
  var result = [ ];
  if (info.vertex !== null) {
    var traverser = new TRAVERSAL.Traverser(info.config);
    traverser.traverse(result, info.vertex, info.endVertex);
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
/// @brief get all document _ids matching the given examples
////////////////////////////////////////////////////////////////////////////////

function DOCUMENT_IDS_BY_EXAMPLE (collectionList, example) {
  var res = [ ];
  if (example === "null" || example === null || ! example) {
    collectionList.forEach(function (c) {
      res = res.concat(COLLECTION(c).toArray().map(function(t) { return t._id; }));
    });
    return res;
  }
  if (typeof example === "string") {
    // Assume it is an _id. Has to fail later on
    return [ example ];
  }
  if (! Array.isArray(example)) {
    example = [ example ];
  }
  var tmp = [ ];
  example.forEach(function (e) {
    if (typeof e === "string") {
      // We have an _id already
      res.push(e);
    } 
    else if (e !== null) {
      tmp.push(e);
    }
  });
  collectionList.forEach(function (c) {
    tmp.forEach(function (e) {
      res = res.concat(COLLECTION(c).byExample(e).toArray().map(function(t) {
        return t._id;
      }));
    });
  });
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getAllDocsByExample
////////////////////////////////////////////////////////////////////////////////

function DOCUMENTS_BY_EXAMPLE (collectionList, example) {
  var res = [ ];
  if (example === "null" || example === null || ! example) {
    collectionList.forEach(function (c) {
      res = res.concat(COLLECTION(c).toArray());
    });
    return res;
  }
  if (typeof example === "string") {
    example = [ { _id : example } ];
  }
  if (! Array.isArray(example)) {
    example = [ example ];
  }
  var tmp = [ ];
  example.forEach(function (e) {
    if (typeof e === "string") {
      tmp.push({ _id : e });
    } 
    else if (e !== null) {
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

function RESOLVE_GRAPH_TO_COLLECTIONS (graph, options, funcname) {
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
      WARN(funcname, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      // TODO: check if we need to return more data here
      return collections;
    }
  });
  collections.orphanCollections = FILTER_RESTRICTION(
    graph.orphanCollections, options.orphanCollectionRestriction
  );
  return collections;
}

function RESOLVE_GRAPH_TO_FROM_VERTICES (graphname, options, funcname) {
  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections;
  if (! graph) {
    THROW(funcname, INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, funcname);
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options, funcname);
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

function RESOLVE_GRAPH_TO_TO_VERTICES (graphname, options, funcname) {
  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections ;
  if (! graph) {
    THROW(funcname, INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, funcname);
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options, funcname);
  var removeDuplicates = function(elem, pos, self) {
    return self.indexOf(elem) === pos;
  };

  return DOCUMENTS_BY_EXAMPLE(
    collections.toCollection.filter(removeDuplicates), options.toVertexExample
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief GET ALL EDGE and VERTEX COLLECTION ACCORDING TO DIRECTION
////////////////////////////////////////////////////////////////////////////////

function RESOLVE_GRAPH_START_VERTICES (graphName, options, funcname) {
  // check graph exists and load edgeDefintions
  var graph = DOCUMENT_HANDLE("_graphs/" + graphName), collections ;
  if (! graph) {
    THROW(funcname, INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, funcname);
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options, funcname);
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

function RESOLVE_GRAPH_TO_DOCUMENTS (graphname, options, funcname) {
  // check graph exists and load edgeDefintions

  var graph = DOCUMENT_HANDLE("_graphs/" + graphname), collections ;
  if (! graph) {
    THROW(funcname, INTERNAL.errors.ERROR_GRAPH_INVALID_GRAPH, funcname);
  }

  collections = RESOLVE_GRAPH_TO_COLLECTIONS(graph, options, funcname);
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
  'use strict';

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

// NOTE Inject includeData property in this.
function TRAVERSAL_DIJKSTRA_VISITOR (config, result, node, path) {
  'use strict';
  var vertex = node.vertex;
  var res;
  if (this.hasOwnProperty("includeData")
    && this.includeData === true) {
    res = {
      vertices: path.vertices,
      edges: path.edges,
      distance: node.dist
    };
  } else {
    res = {
      vertices: underscore.pluck(path.vertices, "_id"),
      edges: underscore.pluck(path.edges, "_id"),
      distance: node.dist
    };
  }
  result.push(CLONE(res));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to determine parameters for SHORTEST_PATH and
/// GRAPH_SHORTEST_PATH
////////////////////////////////////////////////////////////////////////////////

function SHORTEST_PATH_PARAMS (params) {
  'use strict';

  if (params === undefined) {
    params = { };
  }
  else {
    params = CLONE(params);
  }

  params.strategy = "dijkstra";
  params.itemorder = "forward";

  // add user-defined visitor, if specified
  if (params.hasOwnProperty("visitor")) {
    if (typeof params.visitor === "string") {
      params.visitor = GET_VISITOR(params.visitor, params);
    }
  } else {
    // params.visitor = TRAVERSAL_VISITOR;
    params.visitor = SHORTEST_PATH_VISITOR.bind({
      includeData: params.includeData || false
    });
  }

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
  'use strict';
  var opts = {
    direction: direction
  };
  params = params || {};
  var vertexCollections = [vertexCollection];
  var edgeCollections = [edgeCollection];
  // JS Fallback cases. CPP implementation not working here
  if (
      params.hasOwnProperty("distance") ||
      params.hasOwnProperty("filterVertices") ||
      params.hasOwnProperty("followEdges") ||
      isCoordinator
  ) {
    params = SHORTEST_PATH_PARAMS(params);
    var a = TRAVERSAL_FUNC("SHORTEST_PATH",
                           TRAVERSAL.collectionDatasourceFactory(COLLECTION(edgeCollection)),
                           TO_ID(startVertex, vertexCollection),
                           TO_ID(endVertex, vertexCollection),
                           direction,
                           params);
    // Did not want to modify traversal dijkstra search.
    // Might have unforseen side effects.
    // Internal function visits all vertices on the path.
    // So we reset the path on every step again.
    return a[a.length - 1];
  }
  // Fall through to new CPP version.
  if (params.hasOwnProperty("weight") && params.hasOwnProperty("defaultWeight")) {
    opts.weight = params.weight;
    opts.defaultWeight = params.defaultWeight;
  }
  if (params.hasOwnProperty("includeData")) {
    opts.includeData = params.includeData;
  }
  if (params.hasOwnProperty("followEdges")) {
    opts.followEdges = params.followEdges;
  }
  if (params.hasOwnProperty("filterVertices")) {
    opts.filterVertices = params.filterVertices;
  }
  var i, c;
  if (params.hasOwnProperty("vertexCollections") && Array.isArray(params.vertexCollections)) {
    for (i = 0; i < params.vertexCollections.length; ++i) {
      c = params.vertexCollections[i];
      if (typeof c === "string") {
        vertexCollections.push(c);
      }
    }
  }
  if (params.hasOwnProperty("edgeCollections") && Array.isArray(params.edgeCollections)) {
    for (i = 0; i < params.edgeCollections.length; ++i) {
      c = params.edgeCollections[i];
      if (typeof c === "string") {
        edgeCollections.push(c);
      }
    }
  }

  // newRes has the format: { vertices: [Doc], edges: [Doc], distance: Number}
  // oldResult had the format: [ {vertex: Doc} ]
  return CPP_SHORTEST_PATH(vertexCollections, edgeCollections,
    TO_ID(startVertex, vertexCollection),
    TO_ID(endVertex, vertexCollection),
    opts
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path algorithm
////////////////////////////////////////////////////////////////////////////////

function CALCULATE_SHORTEST_PATHES_WITH_FLOYD_WARSHALL (graphData, options) {
  'use strict';

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
        options.defaultWeight), paths : [{edges : [e], vertices : [e._to, e._from]}]};
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
          options.defaultWeight), paths : [{edges : [e], vertices : [e._to, e._from]}]};
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
        return;
      }
      result.push({
        vertices: paths[from][to].paths[0].vertices,
        edges: underscore.pluck(paths[from][to].paths[0].edges, "_id"),
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

function TRAVERSAL_PARAMS (params, defaultVisitor) {
  'use strict';

  if (params === undefined) {
    params = { };
  }
  params = CLONE(params);

  // add user-defined visitor, if specified
  if (params.hasOwnProperty("visitor")) {
    if (typeof params.visitor === "string") {
      params.visitor = GET_VISITOR(params.visitor, params);
    }
  } else {
    params.visitor = defaultVisitor || TRAVERSAL_VISITOR;
  }
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
/// @brief Creates parameters for a dijkstra based shortest path traversal
////////////////////////////////////////////////////////////////////////////////

function CREATE_DIJKSTRA_PARAMS (graphName, options) {
  var params = TRAVERSAL_PARAMS(options,
    TRAVERSAL_DIJKSTRA_VISITOR.bind({
      includeData: options.includeData || false
    })
  ),
    factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);
  params.paths = true;
  if (options.edgeExamples) {
    params.followEdges = options.edgeExamples;
    if (! Array.isArray(params.followEdges)) {
      params.followEdges = [ params.followEdges ];
    }
  }
  if (options.edgeCollectionRestriction) {
    params.edgeCollectionRestriction = options.edgeCollectionRestriction;
  }
  params.weight = options.weight;
  params.defaultWeight = options.defaultWeight;

  params = SHORTEST_PATH_PARAMS(params);

  params.strategy = "dijkstramulti";

  // merge other options
  for (var att in options) {
    if (options.hasOwnProperty(att) &&
      !params.hasOwnProperty(att)) {
      params[att] = options[att];
    }
  }
  
  var toVertices = RESOLVE_GRAPH_TO_TO_VERTICES(graphName, options);
  var toVerticesObject = {}; 

  var i;
  for (i = 0; i < toVertices.length; ++i) {
    toVerticesObject[TO_ID(toVertices[i])] = false;
  }

  return {
    params: params,
    factory: factory,
    fromVertices: RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options),
    toVertices: toVerticesObject
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate shortest paths by dijkstra
////////////////////////////////////////////////////////////////////////////////

function CALCULATE_SHORTEST_PATHES_WITH_DIJKSTRA (graphName, options) {
  var info = CREATE_DIJKSTRA_PARAMS(graphName, options);
  var result = [];
  info.fromVertices.forEach(function (v) {
    var e = TRAVERSAL_FUNC("GRAPH_SHORTEST_PATH",
      info.factory,
      TO_ID(v),
      JSON.parse(JSON.stringify(info.toVertices)),
      options.direction,
      info.params
    );
    e = underscore.filter(e, function(x) {
      return x.edges.length > 0;
    });
    result = result.concat(e);
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
///   * *stopAtFirstMatch*                 : Only useful if targetVertices is an example that matches 
///                                          to more than one vertex. If so only the shortest path to
///                                          the closest of these target vertices is returned.
///                                          This flag is of special use if you have target pattern and
///                                          you want to know which vertex with this pattern is matched first.
///   * *includeData*                      : Triggers if only *_id*'s are returned (*false*, default)
///                                          or if data is included for all objects as well (*true*)
///                                          This will modify the content of *vertex*, *path.vertices*
///                                          and *path.edges*. 
///
/// NOTE: Since version 2.6 we have included a new optional parameter *includeData*.
/// This parameter triggers if the result contains the real data object *true* or
/// it just includes the *_id* values *false*.
/// The default value is *false* as it yields performance benefits.
///
/// @EXAMPLES
///
/// A route planner example, shortest distance from all german to all french cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphShortestPaths1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_SHORTEST_PATH("
/// | + "'routeplanner', {}, {}, {" +
/// | "weight : 'distance', endVertexCollectionRestriction : 'frenchCity', " +
/// | "includeData: true, " +
/// | "startVertexCollectionRestriction : 'germanCity'}) RETURN [e.startVertex, e.vertex._id, " +
/// | "e.distance, LENGTH(e.paths)]"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, shortest distance from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphShortestPaths2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_SHORTEST_PATH("
/// | +"'routeplanner', [{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], " +
/// | "'frenchCity/Lyon', " +
/// | "{weight : 'distance'}) RETURN [e.startVertex, e.vertex, e.distance, LENGTH(e.paths)]"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_SHORTEST_PATH (graphName,
                                  startVertexExample,
                                  endVertexExample,
                                  options) {
  'use strict';

  if (! options) {
    options = {  };
  }

  if (! options.direction) {
    options.direction =  'any';
  }

  if (isCoordinator) {
    // Fallback to old JS function if we are in the cluster.
    options.fromVertexExample = startVertexExample;
    options.toVertexExample = endVertexExample;

    if (! options.algorithm) {
      if (! IS_EXAMPLE_SET(startVertexExample) && ! IS_EXAMPLE_SET(endVertexExample)) {
        options.algorithm = "Floyd-Warshall";
      }
    }

    if (options.algorithm === "Floyd-Warshall") {
      return CALCULATE_SHORTEST_PATHES_WITH_FLOYD_WARSHALL(
        RESOLVE_GRAPH_TO_DOCUMENTS(graphName, options, "GRAPH_SHORTEST_PATH"),
        options);
    }
    let tmp = CALCULATE_SHORTEST_PATHES_WITH_DIJKSTRA(graphName, options);
    if (options.stopAtFirstMatch && options.stopAtFirstMatch === true && tmp.length > 0) {
      let tmpDist = Infinity;
      let tmpRes = {};
      for (let i = 0; i < tmp.length; ++i) {
        if (tmp[i].distance < tmpDist) {
          tmpDist = tmp[i].distance;
          tmpRes = tmp[i];
        }
      }
      return [tmpRes];
    }
    return tmp;
  }

  if (options.hasOwnProperty('edgeExamples')) {
    // these two are the same (edgeExamples & filterEdges)...
    options.filterEdges = options.edgeExamples;
  }

  let graph = graphModule._graph(graphName);
  let edgeCollections = graph._edgeCollections().map(function (c) { return c.name();});
  if (options.hasOwnProperty("edgeCollectionRestriction")) {
    if (!Array.isArray(options.edgeCollectionRestriction)) {
      if (typeof options.edgeCollectionRestriction === "string") {
        if (!underscore.contains(edgeCollections, options.edgeCollectionRestriction)) {
          // Short circut collection not in graph, cannot find results.
          return [];
        }
        edgeCollections = [options.edgeCollectionRestriction];
      }
    } else {
      edgeCollections = underscore.intersection(edgeCollections, options.edgeCollectionRestriction);
    }
  }
  let vertexCollections = graph._vertexCollections().map(function (c) { return c.name();});

  let startVertices;
  if (options.hasOwnProperty("startVertexCollectionRestriction")
    && Array.isArray(options.startVertexCollectionRestriction)) {
    startVertices = DOCUMENT_IDS_BY_EXAMPLE(options.startVertexCollectionRestriction, startVertexExample);
  } 
  else if (options.hasOwnProperty("startVertexCollectionRestriction") 
    && typeof options.startVertexCollectionRestriction === 'string') {
    startVertices = DOCUMENT_IDS_BY_EXAMPLE([ options.startVertexCollectionRestriction ], startVertexExample);
  }
  else {
    startVertices = DOCUMENT_IDS_BY_EXAMPLE(vertexCollections, startVertexExample);
  }
  if (startVertices.length === 0) {
    return [];
  }

  let endVertices;
  if (options.hasOwnProperty("endVertexCollectionRestriction")
    && Array.isArray(options.endVertexCollectionRestriction)) {
    endVertices = DOCUMENT_IDS_BY_EXAMPLE(options.endVertexCollectionRestriction, endVertexExample);
  } 
  else if (options.hasOwnProperty("endVertexCollectionRestriction")
    && typeof options.endVertexCollectionRestriction === 'string') {
    endVertices = DOCUMENT_IDS_BY_EXAMPLE([ options.endVertexCollectionRestriction ], endVertexExample);
  } 
  else {
    endVertices = DOCUMENT_IDS_BY_EXAMPLE(vertexCollections, endVertexExample);
  }
  if (endVertices.length === 0) {
    return [];
  }

  let res = [];
  // Compute all shortest_path pairs.
  for (let i = 0; i < startVertices.length; ++i) {
    for (let j = 0; j < endVertices.length; ++j) {
      if (startVertices[i] !== endVertices[j]) {
        let p = CPP_SHORTEST_PATH(vertexCollections, edgeCollections,
          startVertices[i], endVertices[j], options
        );
        if (p !== null) {
          res.push(p);
        }
      }
    }
  }
  if (options.stopAtFirstMatch && options.stopAtFirstMatch === true && res.length > 0) {
    let tmpDist = Infinity;
    let tmpRes = {};
    for (let i = 0; i < res.length; ++i) {
      if (res[i].distance < tmpDist) {
        tmpDist = res[i].distance;
        tmpRes = res[i];
      }
    }
    return [tmpRes];
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph
////////////////////////////////////////////////////////////////////////////////

function AQL_TRAVERSAL (vertexCollection,
                        edgeCollection,
                        startVertex,
                        direction,
                        params) {
  'use strict';

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
/// * *filterVertices*  An optional array of example vertex documents that the traversal will treat specially.
///     If no examples are given, the traversal will handle all encountered vertices equally.
///     If one or many vertex examples are given, the traversal will exclude any non-matching vertex from the
///     result and/or not descend into it. Optionally, filterVertices can contain a string containing the name
///     of a user-defined AQL function that should be responsible for filtering.
///     If so, the AQL function is expected to have the following signature:
/// 
///     `function (config, vertex, path)`
///
///     If a custom AQL function is used for filterVertices, it is expected to return one of the following values:
///
///     * [ ]: Include the vertex in the result and descend into its connected edges
///     * [ "prune" ]: Will include the vertex in the result but not descend into its connected edges
///     * [ "exclude" ]: Will not include the vertex in the result but descend into its connected edges
///     * [ "prune", "exclude" ]: Will completely ignore the vertex and its connected edges
///
/// * *vertexFilterMethod:* Only useful in conjunction with filterVertices and if no user-defined AQL function is used.
///     If specified, it will influence how vertices are handled that don't match the examples in filterVertices:
///
///    * [ "prune" ]: Will include non-matching vertices in the result but not descend into them
///    * [ "exclude" ]: Will not include non-matching vertices in the result but descend into them
///    * [ "prune", "exclude" ]: Will completely ignore the vertex and its connected edges
///
/// @EXAMPLES
///
/// A route planner example, start a traversal from Hamburg :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversal1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound') RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, start a traversal from Hamburg with a max depth of 1
/// so only the direct neighbors of Hamburg are returned:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversal2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound', {maxDepth : 1}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_TRAVERSAL (graphName,
                              startVertexExample,
                              direction,
                              options) {
  'use strict';

  var result = [];
  options = TRAVERSAL_PARAMS(options);
  options.fromVertexExample = startVertexExample;
  options.direction =  direction;

  var startVertices = RESOLVE_GRAPH_START_VERTICES(graphName, options, "GRAPH_TRAVERSAL");
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
  'use strict';

  if (params === undefined) {
    params = { };
  }
  else {
    params = CLONE(params);
  }

  // add user-defined visitor, if specified
  if (typeof params.visitor === "string") {
    params.visitor = GET_VISITOR(params.visitor, params);
  }
  else {
    params.visitor = TRAVERSAL_TREE_VISITOR;
  }
  
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
  'use strict';

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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_DISTANCE_TO("
/// | +"'routeplanner', {}, {}, { " +
/// | " weight : 'distance', endVertexCollectionRestriction : 'germanCity', " +
/// | "startVertexCollectionRestriction : 'frenchCity'}) RETURN [e.startVertex, e.vertex, " +
/// | "e.distance]"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, distance from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDistanceTo2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_DISTANCE_TO("
/// | + "'routeplanner', [{_id: 'germanCity/Cologne'},{_id: 'germanCity/Hamburg'}], " +
/// | "'frenchCity/Lyon', " +
/// | "{weight : 'distance'}) RETURN [e.startVertex, e.vertex, e.distance]"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_DISTANCE_TO (graphName,
                                startVertexExample,
                                endVertexExample,
                                options) {
  'use strict';

  if (! options) {
    options = {};
  }
  else {
    options = CLONE(options);
  }
  options.includeData = false;
  if (! options.algorithm) {
    options.algorithm = "dijkstra";
  }
  let paths = AQL_GRAPH_SHORTEST_PATH(
    graphName, startVertexExample, endVertexExample, options
  );
  let result = [];
  for (let i = 0; i < paths.length; ++i) {
    let p = paths[i];
    result.push({
      startVertex: p.vertices[0],
      vertex: p.vertices[p.vertices.length - 1],
      distance: p.distance
    });
  }
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
///  [Graph Traversals](../Aql/GraphOperations.html#graph_traversal):
///
/// @EXAMPLES
///
/// A route planner example, start a traversal from Hamburg :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversalTree1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL_TREE('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound', 'connnection') RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, start a traversal from Hamburg with a max depth of 1 so
///  only the direct neighbors of Hamburg are returned:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversalTree2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL_TREE('routeplanner', 'germanCity/Hamburg',"+
/// | " 'outbound', 'connnection', {maxDepth : 1}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_TRAVERSAL_TREE (graphName,
                                   startVertexExample,
                                   direction,
                                   connectName,
                                   options) {
  'use strict';

  var result = [];
  options = TRAVERSAL_TREE_PARAMS(options, connectName, "GRAPH_TRAVERSAL_TREE");
  if (options === null) {
    return null;
  }
  options.fromVertexExample = startVertexExample;
  options.direction = direction;

  var startVertices = RESOLVE_GRAPH_START_VERTICES(graphName, options, "GRAPH_TRAVERSAL_TREE");
  var factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);

  startVertices.forEach(function (f) {
    var r = TRAVERSAL_FUNC("GRAPH_TRAVERSAL_TREE",
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
                    examples,
                    options) {
  'use strict';

  var c = COLLECTION(edgeCollection), result;

  // validate arguments
  if (direction === "outbound") {
    result = FILTER(c.outEdges(vertex), examples);
    if (options && options.includeVertices) {
      for (let i = 0; i < result.length; ++i) {
        try {
          result[i] = { edge: CLONE(result[i]), vertex: DOCUMENT_HANDLE(result[i]._from) };
        }
        catch (err) {
        }
      }
    }
  }
  else if (direction === "inbound") {
    result = FILTER(c.inEdges(vertex), examples);
    if (options && options.includeVertices) {
      for (let i = 0; i < result.length; ++i) {
        try {
          result[i] = { edge: CLONE(result[i]), vertex: DOCUMENT_HANDLE(result[i]._to) };
        }
        catch (err) {
        }
      }
    }
  }
  else if (direction === "any") {
    if (options && options.includeVertices) {
      if (Array.isArray(vertex)) {
        result = [];
        let tmp;
        for (let h = 0; h < vertex.length; ++h) {
          tmp = FILTER(c.inEdges(vertex), examples);
          for (let i = 0; i < tmp.length; ++i) {
            try {
              tmp[i] = { edge: CLONE(tmp[i]), vertex: DOCUMENT_HANDLE(tmp[i]._from) };
            }
            catch (err) {
            }
          }
          result = result.concat(tmp);
          tmp = FILTER(c.outEdges(vertex), examples);
          for (let i = 0; i < tmp.length; ++i) {
            try {
              tmp[i] = { edge: CLONE(tmp[i]), vertex: DOCUMENT_HANDLE(tmp[i]._to) };
            }
            catch (err) {
            }
          }
          result = result.concat(tmp);
        }
      } else {
        result = [];
        let tmp = FILTER(c.inEdges(vertex), examples);
        for (let i = 0; i < tmp.length; ++i) {
          try {
            tmp[i] = { edge: CLONE(tmp[i]), vertex: DOCUMENT_HANDLE(tmp[i]._from) };
          }
          catch (err) {
          }
        }
        result = result.concat(tmp);
        tmp = FILTER(c.outEdges(vertex), examples);
        for (let i = 0; i < tmp.length; ++i) {
          try {
            tmp[i] = { edge: CLONE(tmp[i]), vertex: DOCUMENT_HANDLE(tmp[i]._to) };
          }
          catch (err) {
          }
        }
        result = result.concat(tmp);
      }
    } else {
      result = FILTER(c.edges(vertex), examples);
    }
  }
  else {
    WARN("EDGES", INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to filter edges based on examples
////////////////////////////////////////////////////////////////////////////////

function FILTERED_EDGES (edges, vertex, direction, examples) {
  'use strict';

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
                        examples,
                        options) {
  'use strict';

  vertex = TO_ID(vertex, vertexCollection);
  options = CLONE(options) || {};
  // Fallback to JS if we are in the cluster
  // Improve the examples. LocalServer can match String -> _id
  if (examples !== undefined) {
    if (typeof examples === "string") {
      examples = [{_id: examples}];
    }
    if (Array.isArray(examples)) {
      examples = examples.map(function(e) {
        if (typeof e === "string") {
          return {_id: e};
        }
        return e;
      });
    }
  }
  let edges = AQL_EDGES(edgeCollection, vertex, direction);
  let tmp = FILTERED_EDGES(edges, vertex, direction, examples);
  let vertices = [];
  let distinct = new Set();
  for (let i = 0; i < tmp.length; ++i) {
    let v = tmp[i].vertex;
    if (!distinct.has(v._id)) {
      distinct.add(v._id);
      if (options.includeData) {
        vertices.push(v);
      } else {
        vertices.push(v._id);
      }
    }
  }
  distinct.clear();
  return vertices;
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
///                          vertices (see [example](#short_explanation_of_the_example_parameter)).
/// * *options*            : An object containing the following options:
///   * *direction*                        : The direction
///     of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeExamples*                     : A filter example for the edges to
///     the neighbors (see [example](#short_explanation_of_the_example_parameter)).
///   * *neighborExamples*                 : An example for the desired neighbors
///     (see [example](#short_explanation_of_the_example_parameter)).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *vertexCollectionRestriction* : One or multiple vertex
///     collection names. Only vertices from these collections will be contained in the
///   result. This does not effect vertices on the path.
///   * *minDepth*                         : Defines the minimal
///     depth a path to a neighbor must have to be returned (default is 1).
///   * *maxDepth*                         : Defines the maximal
///     depth a path to a neighbor must have to be returned (default is 1).
///   * *maxIterations*: the maximum number of iterations that the traversal is
///     allowed to perform. It is sensible to set this number so unbounded traversals
///     will terminate at some point.
///   * *includeData* is a boolean value to define if the returned documents should be extracted 
///     instead of returning their ids only. The default is *false*.
///
/// Note: in ArangoDB versions prior to 2.6 *NEIGHBORS* returned the array of neighbor vertices with 
/// all attributes and not just the vertex ids. To return to the same behavior, set the *includeData*
/// option to *true* in 2.6 and above.
///
/// @EXAMPLES
///
/// A route planner example, all neighbors of locations with a distance of either
/// 700 or 600.:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphNeighbors1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_NEIGHBORS("
/// | +"'routeplanner', {}, {edgeExamples : [{distance: 600}, {distance: 700}]}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound neighbors of Hamburg with a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphNeighbors2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_NEIGHBORS("
/// | +"'routeplanner', 'germanCity/Hamburg', {direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_NEIGHBORS (graphName,
                              vertexExample,
                              options) {
  'use strict';
  options = CLONE(options) || {};
  if (! options.hasOwnProperty("direction")) {
    options.direction =  'any';
  }
  var params = {};

  if (isCoordinator) {
    // JS Fallback for features not yet included in CPP Neighbor version
    options.fromVertexExample = vertexExample;

    if (options.hasOwnProperty("neighborExamples") && typeof options.neighborExamples === "string") {
      options.neighborExamples = {_id : options.neighborExamples};
    }
    var neighbors = [],
      factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);
    params = TRAVERSAL_PARAMS();
    params.paths = true;
    params.uniqueness = {
      vertices: "global",
      edges: "global"
    };
    params.minDepth = 1; // Make sure we exclude from depth 1
    params.maxDepth = options.maxDepth === undefined ? 1 : options.maxDepth;
    params.maxIterations = options.maxIterations;
    // add user-defined visitor, if specified
    // (NOT SUPPORTED RIGHT NOW, return format will break)
    /*
    if (typeof options.visitor === "string") {
      params.visitor = GET_VISITOR(options.visitor, options);
      params.visitorReturnsResults = options.visitorReturnsResults || false;
    }
    else {
    */
   // Injecting additional options into the Visitor
     params.visitor = TRAVERSAL_NEIGHBOR_VISITOR.bind({
       minDepth: options.minDepth === undefined ? 1 : options.minDepth,
       includeData: options.includeData || false
     });
     // }
    
    var fromVertices = RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options, "GRAPH_NEIGHBORS");
    if (options.edgeExamples) {
      params.followEdges = options.edgeExamples;
    }
    if (options.edgeCollectionRestriction) {
      params.edgeCollectionRestriction = options.edgeCollectionRestriction;
    }
    if (options.vertexCollectionRestriction) {
      params.filterVertexCollections = options.vertexCollectionRestriction;
    }
    if (options.endVertexCollectionRestriction) {
      params.filterVertexCollections = options.endVertexCollectionRestriction;
    }
    // merge other options
    Object.keys(options).forEach(function (att) {
      if (! params.hasOwnProperty(att)) {
        params[att] = options[att];
      }
    });
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
    let uniqueIds = new Set();
    neighbors.forEach(function (n) {
      if (!uniqueIds.has(n._id)) {
        uniqueIds.add(n._id);
        if (FILTER([n], options.neighborExamples).length > 0) {
          if (options.includeData) {
            result.push(n);
          } else {
            result.push(n._id);
          }
        }
      }
    });
    uniqueIds.clear();
    return result;
  }


  params.direction = options.direction;
  if (options.hasOwnProperty("edgeExamples")) {
    if (!Array.isArray(options.edgeExamples)) {
      params.filterEdges = [options.edgeExamples];
    } else {
      params.filterEdges = options.edgeExamples;
    }
  }

  if (options.hasOwnProperty("neighborExamples")) {
    if (!Array.isArray(options.neighborExamples)) {
      params.filterVertices = [options.neighborExamples];
    } else {
      params.filterVertices = options.neighborExamples;
    }
  }

  let graph = graphModule._graph(graphName);
  let edgeCollections = graph._edgeCollections().map(function (c) { return c.name();});
  if (options.hasOwnProperty("edgeCollectionRestriction")) {
    if (!Array.isArray(options.edgeCollectionRestriction)) {
      if (typeof options.edgeCollectionRestriction === "string") {
        if (!underscore.contains(edgeCollections, options.edgeCollectionRestriction)) {
          // Short circut collection not in graph, cannot find results.
          return [];
        }
        edgeCollections = [options.edgeCollectionRestriction];
      }
    } else {
      edgeCollections = underscore.intersection(edgeCollections, options.edgeCollectionRestriction);
    }
  }
  let vertexCollections = graph._vertexCollections().map(function (c) { return c.name();});
  let startVertices = DOCUMENT_IDS_BY_EXAMPLE(vertexCollections, vertexExample);
  if (startVertices.length === 0) {
    return [];
  }
  if (options.hasOwnProperty("vertexCollectionRestriction")) {
    if (!Array.isArray(options.vertexCollectionRestriction)) {
      if (typeof options.vertexCollectionRestriction === "string") {
        if (!underscore.contains(vertexCollections, options.vertexCollectionRestriction)) {
          // Short circut collection not in graph, cannot find results.
          return [];
        }
        vertexCollections = [options.vertexCollectionRestriction];
      }
    } else {
      vertexCollections = underscore.intersection(vertexCollections, options.vertexCollectionRestriction);
    }
  }
  if (options.hasOwnProperty("minDepth")) {
    params.minDepth = options.minDepth;
  }
  if (options.hasOwnProperty("maxDepth")) {
    params.maxDepth = options.maxDepth;
  }
  if (options.hasOwnProperty("includeData")) {
    params.includeData = options.includeData;
  }

  return CPP_NEIGHBORS(vertexCollections, edgeCollections, startVertices, params);
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
///   * *minDepth*                         : Defines the minimal length of a path from an edge
///  to a vertex (default is 1, which means only the edges directly connected to a vertex would
///  be returned).
///   * *maxDepth*                         : Defines the maximal length of a path from an edge
///  to a vertex (default is 1, which means only the edges directly connected to a vertex would
///  be returned).
///   * *maxIterations*: the maximum number of iterations that the traversal is
///      allowed to perform. It is sensible to set this number so unbounded traversals
///      will terminate.
///   * *includeData*: Defines if the result should contain only ids (false) or if all documents
///     should be fully extracted (true). By default this parameter is set to false, so only ids
///     will be returned.
///
/// @EXAMPLES
///
/// A route planner example, all edges to locations with a distance of either 700 or 600.:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | +"'routeplanner', {}, {edgeExamples : [{distance: 600}, {distance: 700}]}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound edges of Hamburg with a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | +"'routeplanner', 'germanCity/Hamburg', {direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Including the data:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | + "'routeplanner', 'germanCity/Hamburg', {direction : 'outbound',"
/// | + "maxDepth : 2, includeData: true}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_EDGES (graphName,
                          vertexExample,
                          options) {
  'use strict';
  options = CLONE(options) || {};
  if (! options.hasOwnProperty("direction")) {
    options.direction =  'any';
  }
  var params = {};

  // JS Fallback for features not yet included in CPP Neighbor version
  options.fromVertexExample = vertexExample;

  if (options.hasOwnProperty("neighborExamples") && typeof options.neighborExamples === "string") {
    options.neighborExamples = {_id : options.neighborExamples};
  }
  var edges = [],
    factory = TRAVERSAL.generalGraphDatasourceFactory(graphName);
  params = TRAVERSAL_PARAMS();
  params.paths = true;
  params.uniqueness = {
    vertices: "none",
    edges: "global"
  };
  params.minDepth = 1; // Make sure we exclude from depth 1
  params.maxDepth = options.maxDepth === undefined ? 1 : options.maxDepth;
  params.maxIterations = options.maxIterations;
  // add user-defined visitor, if specified
  // (NOT SUPPORTED RIGHT NOW, return format will break)
  /*
  if (typeof options.visitor === "string") {
    params.visitor = GET_VISITOR(options.visitor, options);
    params.visitorReturnsResults = options.visitorReturnsResults || false;
  }
  else {
  */
 // Injecting additional options into the Visitor
  var visitorConfig = {
    minDepth: options.minDepth === undefined ? 1 : options.minDepth
  };
  if (options.hasOwnProperty("neighborExamples")) {
    visitorConfig.neighborExamples = options.neighborExamples;
  }
  params.visitor = TRAVERSAL_EDGE_VISITOR.bind(visitorConfig);
  
  var fromVertices = RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options, "GRAPH_EDGES");
  if (options.edgeExamples) {
    params.followEdges = options.edgeExamples;
  }
  if (options.edgeCollectionRestriction) {
    params.edgeCollectionRestriction = options.edgeCollectionRestriction;
  }
  if (options.vertexCollectionRestriction) {
    params.filterVertexCollections = options.vertexCollectionRestriction;
  }
  if (options.endVertexCollectionRestriction) {
    params.filterVertexCollections = options.endVertexCollectionRestriction;
  }
  // merge other options
  Object.keys(options).forEach(function (att) {
    if (! params.hasOwnProperty(att)) {
      params[att] = options[att];
    }
  });
  fromVertices.forEach(function (v) {
    var e = TRAVERSAL_FUNC("GRAPH_EDGES",
      factory,
      v._id,
      undefined,
      options.direction,
      params);

    edges = edges.concat(e);
  });
  var result = [];
  let uniqueIds = new Set();
  edges.forEach(function (n) {
    if (!uniqueIds.has(n._id)) {
      uniqueIds.add(n._id);
      if (options.includeData) {
        result.push(n);
      } else {
        result.push(n._id);
      }
    }
  });
  uniqueIds.clear();
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_VERTICES("
///   +"'routeplanner', {}) RETURN e").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all vertices from collection *germanCity*.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertices2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_VERTICES("
/// | +"'routeplanner', {}, {direction : 'any', vertexCollectionRestriction" +
///   " : 'germanCity'}) RETURN e").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_VERTICES (graphName,
                             vertexExamples,
                             options) {
  'use strict';

  options = CLONE(options) || {};
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
  return RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options, "GRAPH_VERTICES");
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_NEIGHBORS("
/// | +"'routeplanner', {isCapital : true}, {isCapital : true}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCommonNeighbors2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_NEIGHBORS("
/// | +"'routeplanner', 'germanCity/Hamburg', {}, {direction : 'outbound', maxDepth : 2}, "+
/// | "{direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_COMMON_NEIGHBORS (graphName,
                                     vertex1Examples,
                                     vertex2Examples,
                                     options1,
                                     options2) {
  'use strict';

  options1 = options1 || {};
  options2 = options2 || {};
  if (options1.includeData) {
    options2.includeData = true;
  }
  else if (options2.includeData) {
    options1.includeData = true;
  }

  let graph = graphModule._graph(graphName);
  let vertexCollections = graph._vertexCollections().map(function (c) { return c.name();});
  let vertices1 = DOCUMENT_IDS_BY_EXAMPLE(vertexCollections, vertex1Examples);
  let vertices2;
  if (vertex1Examples === vertex2Examples) {
    vertices2 = vertices1;
  } 
  else {
    vertices2 = DOCUMENT_IDS_BY_EXAMPLE(vertexCollections, vertex2Examples);
  }
  // Use ES6 Map. Higher performance then Object.
  let tmpNeighborsLeft = new Map();
  let tmpNeighborsRight = new Map();
  let result = [];

  // Iterate over left side vertex list as left.
  // Calculate its neighbors as ln
  // For each entry iterate over right side vertex list as right.
  // Calculate their neighbors as rn
  // For each store one entry in result {left: left, right: right, neighbors: intersection(ln, rn)}
  // All Neighbors are cached in tmpNeighborsLeft and tmpNeighborsRight resp.
  for (let i = 0; i < vertices1.length; ++i) {
    let left = vertices1[i];
    let itemNeighbors;
    if(tmpNeighborsLeft.has(left)) {
      itemNeighbors = tmpNeighborsLeft.get(left);
    } 
    else {
      itemNeighbors = AQL_GRAPH_NEIGHBORS(graphName, left, options1);
      tmpNeighborsLeft.set(left, itemNeighbors);
    }
    for (let j = 0; j < vertices2.length; ++j) {
      let right = vertices2[j];
      if (left === right) {
        continue;
      }
      let rNeighbors;
      if(tmpNeighborsRight.has(right)) {
        rNeighbors = tmpNeighborsRight.get(right);
      } 
      else {
        rNeighbors = AQL_GRAPH_NEIGHBORS(graphName, right, options2);
        tmpNeighborsRight.set(right, rNeighbors);
      }
      let neighbors;
      if (! options1.includeData) {
        neighbors = underscore.intersection(itemNeighbors, rNeighbors);
      }
      else {
        // create a quick lookup table for left hand side
        let lKeys = { };
        for (let i = 0; i < itemNeighbors.length; ++i) {
          lKeys[itemNeighbors[i]._id] = true;
        }
        // check which elements of the right-hand side are also present in the left hand side lookup  map
        neighbors = [];
        for (let i = 0; i < rNeighbors.length; ++i) {
          if (lKeys.hasOwnProperty(rNeighbors[i]._id)) {
            neighbors.push(rNeighbors[i]);
          }
        }
      }
      if (neighbors.length > 0) {
        result.push({left, right, neighbors});
      }
    }
  }
  // Legacy Format
  // result.push(tmpRes);
  tmpNeighborsLeft.clear();
  tmpNeighborsRight.clear();
  return result;
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_PROPERTIES("
/// | +"'routeplanner', {}, {}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphProperties2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_PROPERTIES("
/// | +"'routeplanner', {}, {}, {ignoreProperties: 'population'}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_COMMON_PROPERTIES (graphName,
                                      vertex1Examples,
                                      vertex2Examples,
                                      options) {
  'use strict';

  options = CLONE(options) || {};
  options.fromVertexExample = vertex1Examples;
  options.toVertexExample = vertex2Examples;
  options.direction =  'any';
  options.ignoreProperties = TO_LIST(options.ignoreProperties, true);
  options.startVertexCollectionRestriction = options.vertex1CollectionRestriction;
  options.endVertexCollectionRestriction = options.vertex2CollectionRestriction;

  var g = RESOLVE_GRAPH_TO_FROM_VERTICES(graphName, options, "GRAPH_COMMON_PROPERTIES");
  var g2 = RESOLVE_GRAPH_TO_TO_VERTICES(graphName, options, "GRAPH_COMMON_PROPERTIES");
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
/// @brief Prepares and executes a dijkstra search with predefined result object
///        The result object will be handed over in each traversal step
////////////////////////////////////////////////////////////////////////////////

function RUN_DIJKSTRA_WITH_RESULT_HANDLE (func, graphName, options, result, afterEach) {
  result = result || [];
  var dijkstraParams = CREATE_DIJKSTRA_PARAMS(graphName, options);
  dijkstraParams.fromVertices.forEach(function (v) {
    var info = TRAVERSAL_CONFIG(
      func,
      dijkstraParams.factory,
      TO_ID(v),
      JSON.parse(JSON.stringify(dijkstraParams.toVertices)),
      options.direction,
      dijkstraParams.params
    );
    if (info.vertex !== null) {
      var traverser = new TRAVERSAL.Traverser(info.config);
      traverser.traverse(result, info.vertex, info.endVertex);
      if (typeof afterEach === "function") {
        afterEach(result, info.vertex);
      }
    }
  });
  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for absolute eccentricity traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_ABSOLUTE_ECCENTRICITY_VISITOR (config, result, node, path) {
  'use strict';
  result[path.vertices[0]._id] = Math.max(node.dist, result[path.vertices[0]._id] || 0);
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('routeplanner', {})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
///   +"'routeplanner', {}, {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all German cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
/// | + "'routeplanner', {}, {startVertexCollectionRestriction : 'germanCity', " +
///   "direction : 'outbound', weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ABSOLUTE_ECCENTRICITY (graphName, vertexExample, options) {
  'use strict';
  options = CLONE(options) || {};
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "dijkstra";
  }
  options.fromVertexExample = vertexExample;
  options.toVertexExample = {};

  options.visitor = TRAVERSAL_ABSOLUTE_ECCENTRICITY_VISITOR;
  return RUN_DIJKSTRA_WITH_RESULT_HANDLE(
    "ABSOLUTE_ECCENTRICITY",
    graphName,
    options,
    {}
  );
}


////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for absolute eccentricity traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_ECCENTRICITY_VISITOR (config, result, node, path) {
  'use strict';
  if (path.edges.length > 0) {
    result.current = Math.min(1 / node.dist, result.current || 1);
  }
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ECCENTRICITY('routeplanner')").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEccentricity2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ECCENTRICITY('routeplanner', {weight : 'distance'})"
///   ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ECCENTRICITY (graphName, options) {
  'use strict';

  options = CLONE(options) || {};
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "dijkstra";
  }
  options.fromVertexExample = {};
  options.toVertexExample = {};
  options.visitor = TRAVERSAL_ECCENTRICITY_VISITOR;
  var result = RUN_DIJKSTRA_WITH_RESULT_HANDLE(
    "ECCENTRICITY",
    graphName,
    options,
    {
      vertices: {},
      max: 0
    },
    function (result, vertex) {
      if (result.current === undefined) {
        result.vertices[vertex._id] = 0;
      } else {
        if (result.current > result.max) {
          result.max = result.current;
        }
        result.vertices[vertex._id] = result.current;
        delete result.current;
      }
    }
  );
  var list = result.vertices;
  Object.keys(list).forEach(function (r) {
    list[r] /= result.max;
  });
  return list;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for absolute closeness traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_ABSOLUTE_CLOSENESS_VISITOR (config, result, node, path) {
  'use strict';
  result[path.vertices[0]._id] = result[path.vertices[0]._id] || 0;
  result[path.vertices[0]._id] += node.dist;
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS('routeplanner', {})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS("
///   +"'routeplanner', {}, {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all German cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS("
/// | + "'routeplanner', {}, {startVertexCollectionRestriction : 'germanCity', " +
///   "direction : 'outbound', weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

/*
function AQL_GRAPH_ABSOLUTE_CLOSENESS (graphName, vertexExample, options) {
 'use strict';
 if (! options) {
   options = { };
 }
 if (! options.direction) {
   options.direction = 'any';
 }
 var distanceMap = AQL_GRAPH_DISTANCE_TO(graphName, vertexExample , {}, options), result = {};
 distanceMap.forEach(function(d) {
   if (options.direction !== 'any' && options.calcNormalized) {
     d.distance = d.distance === 0 ? 0 : 1 / d.distance;
   }
   if (!result[d.startVertex]) {
     result[d.startVertex] = d.distance;
   } else {
     result[d.startVertex] = d.distance + result[d.startVertex];
   }
 });
 return result;
}

function AQL_GRAPH_CLOSENESS (graphName, options) {
 'use strict';
 if (! options) {
   options = { };
 }
 options.calcNormalized = true;
 if (! options.algorithm) {
   options.algorithm = "Floyd-Warshall";
 }
 var result = AQL_GRAPH_ABSOLUTE_CLOSENESS(graphName, {}, options), max = 0;
 Object.keys(result).forEach(function (r) {
   if (options.direction === 'any') {
     result[r] = result[r] === 0 ? 0 : 1 / result[r];
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
*/

function AQL_GRAPH_ABSOLUTE_CLOSENESS (graphName, vertexExample, options) {
  'use strict';
  options = CLONE(options) || {};
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "dijkstra";
  }
  options.fromVertexExample = vertexExample;
  options.toVertexExample = {};

  options.visitor = TRAVERSAL_ABSOLUTE_CLOSENESS_VISITOR;
  return RUN_DIJKSTRA_WITH_RESULT_HANDLE(
    "ABSOLUTE_CLOSENESS",
    graphName,
    options,
    {}
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for absolute closeness traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_CLOSENESS_VISITOR (config, result, node, path) {
  'use strict';
  result.current += node.dist;
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_CLOSENESS('routeplanner')").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCloseness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_CLOSENESS("
///   +"'routeplanner', {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCloseness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_CLOSENESS("
/// | + "'routeplanner',{direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_CLOSENESS (graphName, options) {
  'use strict';

  options = CLONE(options) || {};
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "dijkstra";
  }
  options.fromVertexExample = {};
  options.toVertexExample = {};
  options.visitor = TRAVERSAL_CLOSENESS_VISITOR;

  var result = RUN_DIJKSTRA_WITH_RESULT_HANDLE(
    "CLOSENESS",
    graphName,
    options,
    {
      vertices: {},
      current: 0,
      max: 0
    },
    function(result, vertex) {
      if (result.current !== 0) {
        result.current = 1 / result.current;
      }
      if (result.current > result.max) {
        result.max = result.current;
      }
      result.vertices[vertex._id] = result.current;
      result.current = 0;
    }
  );
  var list = result.vertices;
  Object.keys(list).forEach(function (r) {
    list[r] /= result.max;
  });
  return list;
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS('routeplanner', {})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS("
///   +"'routeplanner', {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_ABSOLUTE_BETWEENNESS (graphName, options) {
  'use strict';

  options = CLONE(options) || {};
  if (! options.direction) {
    options.direction =  'any';
  }
  options.algorithm = "Floyd-Warshall";

  // Make sure we ONLY extract _ids
  options.includeData = false;
  let graph = graphModule._graph(graphName);
  let vertexCollections = graph._vertexCollections().map(function (c) { return c.name();});
  let vertexIds = DOCUMENT_IDS_BY_EXAMPLE(vertexCollections, {});
  let result = {};
  let distanceMap = AQL_GRAPH_SHORTEST_PATH(graphName, vertexIds , vertexIds, options);
  for (let k = 0; k < vertexIds.length; k++) {
    result[vertexIds[k]] = 0;
  }
  distanceMap.forEach(function(d) {
    let startVertex = d.vertices[0];
    let l = d.vertices.length;
    let targetVertex = d.vertices[l - 1];
    let val = 1 / l;
    d.vertices.forEach(function (v) {
      if (v === startVertex || v === targetVertex) {
        return;
      }
      result[v] += val;
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_BETWEENNESS('routeplanner')").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphBetweenness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_BETWEENNESS('routeplanner', {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphBetweenness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_BETWEENNESS("
///   + "'routeplanner', {direction : 'outbound', weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_BETWEENNESS (graphName, options) {
  'use strict';

  options = CLONE(options) || {};

  var result = AQL_GRAPH_ABSOLUTE_BETWEENNESS(graphName, options),  max = 0;
  Object.keys(result).forEach(function (r) {
    if (result[r] > max) {
      max = result[r];
    }
  });
  if (max === 0) {
    return result;
  }
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_RADIUS('routeplanner')").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_RADIUS('routeplanner', {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_RADIUS("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_RADIUS (graphName, options) {
  'use strict';

  options = CLONE(options) || {};
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
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_DIAMETER('routeplanner')").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDiameter2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_DIAMETER('routeplanner', {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDiameter3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_DIAMETER("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function AQL_GRAPH_DIAMETER (graphName, options) {
  'use strict';

  options = CLONE(options) || {};
  if (! options.direction) {
    options.direction =  'any';
  }
  if (! options.algorithm) {
    options.algorithm = "Floyd-Warshall";
  }

  var result = AQL_GRAPH_ABSOLUTE_ECCENTRICITY(graphName, {}, options),
      max = 0;
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
exports.AQL_LTRIM = AQL_LTRIM;
exports.AQL_RTRIM = AQL_RTRIM;
exports.AQL_SPLIT = AQL_SPLIT;
exports.AQL_SUBSTITUTE = AQL_SUBSTITUTE;
exports.AQL_MD5 = AQL_MD5;
exports.AQL_SHA1 = AQL_SHA1;
exports.AQL_RANDOM_TOKEN = AQL_RANDOM_TOKEN;
exports.AQL_FIND_FIRST = AQL_FIND_FIRST;
exports.AQL_FIND_LAST = AQL_FIND_LAST;
exports.AQL_TO_BOOL = AQL_TO_BOOL;
exports.AQL_TO_NUMBER = AQL_TO_NUMBER;
exports.AQL_TO_STRING = AQL_TO_STRING;
exports.AQL_TO_ARRAY = AQL_TO_ARRAY;
exports.AQL_TO_LIST = AQL_TO_ARRAY; // alias
exports.AQL_IS_NULL = AQL_IS_NULL;
exports.AQL_IS_BOOL = AQL_IS_BOOL;
exports.AQL_IS_NUMBER = AQL_IS_NUMBER;
exports.AQL_IS_STRING = AQL_IS_STRING;
exports.AQL_IS_ARRAY = AQL_IS_ARRAY;
exports.AQL_IS_LIST = AQL_IS_ARRAY; // alias
exports.AQL_IS_OBJECT = AQL_IS_OBJECT;
exports.AQL_IS_DOCUMENT = AQL_IS_OBJECT; // alias
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
exports.AQL_CALL = AQL_CALL;
exports.AQL_APPLY = AQL_APPLY;
exports.AQL_REMOVE_VALUE = AQL_REMOVE_VALUE; 
exports.AQL_REMOVE_VALUES = AQL_REMOVE_VALUES; 
exports.AQL_REMOVE_NTH = AQL_REMOVE_NTH; 
exports.AQL_PUSH = AQL_PUSH;
exports.AQL_APPEND = AQL_APPEND;
exports.AQL_POP = AQL_POP;
exports.AQL_SHIFT = AQL_SHIFT;
exports.AQL_UNSHIFT = AQL_UNSHIFT;
exports.AQL_SLICE = AQL_SLICE;
exports.AQL_MINUS = AQL_MINUS;
exports.AQL_INTERSECTION = AQL_INTERSECTION;
exports.AQL_FLATTEN = AQL_FLATTEN;
exports.AQL_MAX = AQL_MAX;
exports.AQL_MIN = AQL_MIN;
exports.AQL_SUM = AQL_SUM;
exports.AQL_AVERAGE = AQL_AVERAGE;
exports.AQL_MEDIAN = AQL_MEDIAN;
exports.AQL_PERCENTILE = AQL_PERCENTILE;
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
exports.AQL_HAS = AQL_HAS;
exports.AQL_ATTRIBUTES = AQL_ATTRIBUTES;
exports.AQL_VALUES = AQL_VALUES;
exports.AQL_ZIP = AQL_ZIP;
exports.AQL_UNSET = AQL_UNSET;
exports.AQL_KEEP = AQL_KEEP;
exports.AQL_MERGE = AQL_MERGE;
exports.AQL_MERGE_RECURSIVE = AQL_MERGE_RECURSIVE;
exports.AQL_TRANSLATE = AQL_TRANSLATE;
exports.AQL_MATCHES = AQL_MATCHES;
exports.AQL_PASSTHRU = AQL_PASSTHRU;
exports.AQL_TEST_MODIFY = AQL_TEST_MODIFY;
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
exports.AQL_DATE_DAYOFYEAR = AQL_DATE_DAYOFYEAR;
exports.AQL_DATE_ISOWEEK = AQL_DATE_ISOWEEK;
exports.AQL_DATE_LEAPYEAR = AQL_DATE_LEAPYEAR;
exports.AQL_DATE_QUARTER = AQL_DATE_QUARTER;
exports.AQL_DATE_ADD = AQL_DATE_ADD;
exports.AQL_DATE_SUBTRACT = AQL_DATE_SUBTRACT;
exports.AQL_DATE_DIFF = AQL_DATE_DIFF;
exports.AQL_DATE_COMPARE = AQL_DATE_COMPARE;
exports.AQL_DATE_FORMAT = AQL_DATE_FORMAT;

exports.reload = reloadUserFunctions;

// initialize the query engine
resetRegexCache();
//reloadUserFunctions();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
