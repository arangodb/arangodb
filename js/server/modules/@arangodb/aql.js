/* jshint strict: false, unused: true, bitwise: false, esnext: true */
/* global COMPARE_STRING, AQL_WARNING, AQL_QUERY_SLEEP */
/* global OBJECT_HASH */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Aql, internal query functions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var INTERNAL = require('internal');
var ArangoError = require('@arangodb').ArangoError;
var isCoordinator = require('@arangodb/cluster').isCoordinator();

// //////////////////////////////////////////////////////////////////////////////
// / @brief cache for compiled regexes
// //////////////////////////////////////////////////////////////////////////////

var LikeCache = { };
var RegexCache = { };

// //////////////////////////////////////////////////////////////////////////////
// / @brief user functions cache
// //////////////////////////////////////////////////////////////////////////////

var UserFunctions = { };

// //////////////////////////////////////////////////////////////////////////////
// / @brief prefab traversal visitors
// //////////////////////////////////////////////////////////////////////////////

var DefaultVisitors = {
  '_AQL::HASATTRIBUTESVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex, path) {
      if (typeof config.data === 'object' && Array.isArray(config.data.attributes)) {
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
            if (!vertex.hasOwnProperty(config.data.attributes[i])) {
              continue;
            }
            if (!allowNull && vertex[config.data.attributes[i]] === null) {
              continue;
            }

            return CLONE({ vertex: vertex, path: path });
          }

          return;
        }

        for (i = 0; i < config.data.attributes.length; ++i) {
          if (!vertex.hasOwnProperty(config.data.attributes[i])) {
            return;
          }
          if (!allowNull &&
            vertex[config.data.attributes[i]] === null) {
            return;
          }
        }

        return CLONE({ vertex: vertex, path: path });
      }
    }
  },
  '_AQL::PROJECTINGVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      var values = { };
      if (typeof config.data === 'object' && Array.isArray(config.data.attributes)) {
        config.data.attributes.forEach(function (attribute) {
          values[attribute] = vertex[attribute];
        });
      }
      return values;
    }
  },
  '_AQL::IDVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      return vertex._id;
    }
  },
  '_AQL::KEYVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      return vertex._key;
    }
  },
  '_AQL::COUNTINGVISITOR': {
    visitorReturnsResults: false,
    func: function (config, result) {
      if (result.length === 0) {
        result.push(0);
      }
      result[0]++;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief type weight used for sorting and comparing
// //////////////////////////////////////////////////////////////////////////////

var TYPEWEIGHT_NULL = 0;
var TYPEWEIGHT_BOOL = 1;
var TYPEWEIGHT_NUMBER = 2;
var TYPEWEIGHT_STRING = 4;
var TYPEWEIGHT_ARRAY = 8;
var TYPEWEIGHT_OBJECT = 16;

// //////////////////////////////////////////////////////////////////////////////
// / @brief mapping of time unit names to short name, getter and setter
// //////////////////////////////////////////////////////////////////////////////

var unitMapping = {
  y: ['y', 'getUTCFullYear', 'setUTCFullYear'],
  m: ['m', 'getUTCMonth', 'setUTCMonth'],
  w: ['w', null, null],
  d: ['d', 'getUTCDate', 'setUTCDate'],
  h: ['h', 'getUTCHours', 'setUTCHours'],
  i: ['i', 'getUTCMinutes', 'setUTCMinutes'],
  s: ['s', 'getUTCSeconds', 'setUTCSeconds'],
  f: ['f', 'getUTCMilliseconds', 'setUTCMilliseconds']
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

var unitMappingArray = [null, 'y', 'm', 'w', 'd', 'h', 'i', 's', 'f'];

// //////////////////////////////////////////////////////////////////////////////
// / @brief RegExp and cache for ISO duration strings
// //////////////////////////////////////////////////////////////////////////////

// ISODurationRegex.exec("P1Y2M3W4DT5H6M7.890S")
// -> ["P1Y2M3W4DT5H6M7.890S", "1", "2", "3", "4", "5", "6", "7", "890"]

/* jshint -W101 */
var ISODurationRegex = /^P(?:(?:(\d+)Y)?(?:(\d+)M)?(?:(\d+)W)?(?:(\d+)D)?)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)(?:\.(\d+))?S)?)?$/i;
/* jshint +W101 */

var ISODurationCache = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief substring ranges for DATE_COMPARE()
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief offsets for day of year calculation
// //////////////////////////////////////////////////////////////////////////////

var dayOfYearOffsets = [
  0,
  31, // + 31 Jan
  59, // + 28 Feb*
  90, // + 31 Mar
  120, // + 30 Apr
  151, // + 31 May
  181, // + 30 Jun
  212, // + 31 Jul
  243, // + 31 Aug
  273, // + 30 Sep
  304, // + 31 Oct
  334 // + 30 Nov
];

var dayOfLeapYearOffsets = [
  0,
  31, // + 31 Jan
  59, // + 29 Feb*
  91, // + 31 Mar
  121, // + 30 Apr
  152, // + 31 May
  182, // + 30 Jun
  213, // + 31 Jul
  244, // + 31 Aug
  274, // + 30 Sep
  305, // + 31 Oct
  335 // + 30 Nov
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup array for days in month calculation (leap year aware)
// //////////////////////////////////////////////////////////////////////////////

var daysInMonth = [
  29, // Feb (in leap year)
  31, // Jan
  28, // Feb (in non-leap year)
  31, // Mar
  30, // Apr
  31, // May
  30, // Jun
  31, // Jul
  31, // Aug
  30, // Sep
  31, // Oct
  30, // Nov
  31 // Dec
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief English month names (1-based)
// //////////////////////////////////////////////////////////////////////////////

var monthNames = [
  'January',
  'February',
  'March',
  'April',
  'May',
  'June',
  'July',
  'August',
  'September',
  'October',
  'November',
  'December'
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief English weekday names
// //////////////////////////////////////////////////////////////////////////////

var weekdayNames = [
  'Sunday',
  'Monday',
  'Tuesday',
  'Wednesday',
  'Thursday',
  'Friday',
  'Saturday'
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief constants for date difference function
// //////////////////////////////////////////////////////////////////////////////

// milliseconds per month, counting February as 28 days (compensating later)
var msPerMonth = [
  26784e5, 24192e5, 26784e5, 2592e6, 26784e5, 2592e6,
  26784e5, 26784e5, 2592e6, 26784e5, 2592e6, 26784e5
];

var msPerUnit = {
  f: 1,
  s: 1e3, // 1000
  i: 6e4, // 1000 * 60
  h: 36e5, // 1000 * 60 * 60
  d: 864e5, // 1000 * 60 * 60 * 24
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief clear caches
// //////////////////////////////////////////////////////////////////////////////

function clearCaches () {
  'use strict';

  RegexCache = { 'gi': { }, 'g': { }, 'i': { }, '': { } };
  LikeCache = { 'i': { }, '': { } };
  ISODurationCache = { };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief add zeros for a total length of width chars (left padding by default)
// //////////////////////////////////////////////////////////////////////////////

function zeropad (n, width, padRight) {
  'use strict';

  padRight = padRight || false;
  n = '' + n;
  if (padRight) {
    return n.length >= width ? n : n + new Array(width - n.length + 1).join('0');
  } else {
    return n.length >= width ? n : new Array(width - n.length + 1).join('0') + n;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief raise a warning
// //////////////////////////////////////////////////////////////////////////////

function WARN (func, error, data) {
  'use strict';

  if (error.code === INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code) {
    AQL_WARNING(error.code, error.message.replace(/%s/, func));
  } else {
    var prefix = '';
    if (func !== null) {
      prefix = "in function '" + func + "()': ";
    }

    if (typeof data === 'string') {
      AQL_WARNING(error.code, prefix + error.message.replace(/%s/, data));
    } else {
      AQL_WARNING(error.code, prefix + error.message);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief throw a runtime exception
// //////////////////////////////////////////////////////////////////////////////

function THROW (func, error, data, moreMessage) {
  'use strict';

  var prefix = '';
  if (func !== null && func !== '') {
    prefix = "in function '" + func + "()': ";
  }

  var err = new ArangoError();

  err.errorNum = error.code;
  if (typeof data === 'string') {
    err.errorMessage = prefix + error.message.replace(/%s/, data);
  } else {
    err.errorMessage = prefix + error.message;
  }
  if (moreMessage !== undefined) {
    err.errorMessage += '; ' + moreMessage;
  }

  throw err;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a database-specific function prefix
// //////////////////////////////////////////////////////////////////////////////

function DB_PREFIX () {
  return INTERNAL.db._name();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief reset the user functions and reload them from the database
// //////////////////////////////////////////////////////////////////////////////

function reloadUserFunctions () {
  'use strict';

  var prefix = DB_PREFIX();
  var c = INTERNAL.db._collection('_aqlfunctions');

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
    } else {
      code = '(function() { var callback = ' + f.code + ';\n return callback; })();';
    }

    try {
      var res = INTERNAL.executeScript(code, undefined, '(user function ' + key + ')');
      if (typeof res !== 'function') {
        foundError = true;
      }

      functions[key.toUpperCase()] = {
        name: key,
        func: res,
        isDeterministic: f.isDeterministic || false
      };
    } catch (err) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a user-function by name
// //////////////////////////////////////////////////////////////////////////////

function GET_USERFUNCTION (name, config) {
  var prefix = DB_PREFIX(), reloaded = false;
  var key = name.toUpperCase();

  var func;

  if (DefaultVisitors.hasOwnProperty(key)) {
    var visitor = DefaultVisitors[key];
    func = visitor.func;
    config.visitorReturnsResults = visitor.visitorReturnsResults;
  } else {
    if (!UserFunctions.hasOwnProperty(prefix)) {
      reloadUserFunctions();
      reloaded = true;
    }

    if (!UserFunctions[prefix].hasOwnProperty(key) && !reloaded) {
      // last chance
      reloadUserFunctions();
    }

    if (!UserFunctions[prefix].hasOwnProperty(key)) {
      THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
    }

    func = UserFunctions[prefix][key].func;
  }

  if (typeof func !== 'function') {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  return func;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief normalize a function name
// //////////////////////////////////////////////////////////////////////////////

function NORMALIZE_FNAME (functionName) {
  'use strict';

  var p = functionName.indexOf('::');

  if (p === -1) {
    return functionName;
  }

  return functionName.substr(p + 2);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief find a fulltext index for a certain attribute & collection
// //////////////////////////////////////////////////////////////////////////////

function INDEX_FULLTEXT (collection, attribute) {
  'use strict';

  var indexes = collection.getIndexes(), i;

  for (i = 0; i < indexes.length; ++i) {
    var index = indexes[i];
    if (index.type === 'fulltext' && index.fields && index.fields[0] === attribute) {
      return index.id;
    }
  }

  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get access to a collection
// //////////////////////////////////////////////////////////////////////////////

function COLLECTION (name, func) {
  'use strict';

  if (typeof name !== 'string') {
    THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func);
  }

  var c;
  if (name.substring(0, 1) === '_') {
    // system collections need to be accessed slightly differently as they
    // are not returned by the propertyGetter of db
    c = INTERNAL.db._collection(name);
  } else {
    c = INTERNAL.db[name];
  }

  if (c === null || c === undefined) {
    THROW(func, INTERNAL.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND, String(name));
  }
  return c;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief clone an object
// //////////////////////////////////////////////////////////////////////////////

function CLONE (obj) {
  'use strict';

  if (obj === null || typeof (obj) !== 'object') {
    return obj;
  }

  var copy;
  if (Array.isArray(obj)) {
    copy = [];
    obj.forEach(function (i) {
      copy.push(CLONE(i));
    });
  } else if (obj instanceof Object) {
    copy = { };
    Object.keys(obj).forEach(function (k) {
      copy[k] = CLONE(obj[k]);
    });
  }

  return copy;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief box a value into the AQL datatype system
// //////////////////////////////////////////////////////////////////////////////

function FIX_VALUE (value) {
  'use strict';

  var type = typeof (value);

  if (value === undefined ||
    value === null ||
    (type === 'number' && (isNaN(value) || !isFinite(value)))) {
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

    Object.keys(value).forEach(function (k) {
      if (typeof value[k] !== 'function') {
        result[k] = FIX_VALUE(value[k]);
      }
    });

    return result;
  }

  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the sort type of an operand
// //////////////////////////////////////////////////////////////////////////////

function TYPEWEIGHT (value) {
  'use strict';

  if (value !== undefined && value !== null) {
    if (Array.isArray(value)) {
      return TYPEWEIGHT_ARRAY;
    }

    switch (typeof (value)) {
      case 'boolean':
        return TYPEWEIGHT_BOOL;
      case 'number':
        if (isNaN(value) || !isFinite(value)) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief compile a regex from a string pattern
// //////////////////////////////////////////////////////////////////////////////

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
    } else if (c === '\t') {
      pattern += '\\t';
    } else if (c === '\r') {
      pattern += '\\r';
    } else if (c === '\n') {
      pattern += '\\n';
    } else if (c === '\b') {
      pattern += '\\b';
    } else if (c === '\f') {
      pattern += '\\f';
    } else {
      pattern += c;
    }
  }

  return pattern;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief compile a regex from a string pattern
// //////////////////////////////////////////////////////////////////////////////

function COMPILE_REGEX (regex, modifiers) {
  'use strict';

  return new RegExp(AQL_TO_STRING(regex), modifiers);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief compile a regex from a string pattern
// //////////////////////////////////////////////////////////////////////////////

function COMPILE_LIKE (regex, modifiers) {
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
      escaped = !escaped;
    } else {
      if (c === '%') {
        if (escaped) {
          // literal %
          pattern += '%';
        } else {
          // wildcard
          pattern += '(.|[\r\n])*';
        }
      } else if (c === '_') {
        if (escaped) {
          // literal _
          pattern += '_';
        } else {
          // wildcard character
          pattern += '(.|[\r\n])';
        }
      } else if (c.match(specialChar)) {
        // character with special meaning in a regex
        pattern += '\\' + c;
      } else {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief call a user function
// //////////////////////////////////////////////////////////////////////////////

function FCALL_USER (name, parameters) {
  'use strict';

  var prefix = DB_PREFIX(), reloaded = false;
  if (!UserFunctions.hasOwnProperty(prefix)) {
    reloadUserFunctions();
    reloaded = true;
  }

  if (!UserFunctions[prefix].hasOwnProperty(name) && !reloaded) {
    // last chance
    reloadUserFunctions();
  }

  if (!UserFunctions[prefix].hasOwnProperty(name)) {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  try {
    return FIX_VALUE(UserFunctions[prefix][name].func.apply({ name: name }, parameters));
  } catch (err) {
    WARN(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err.stack || String(err)));
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief dynamically call a function
// //////////////////////////////////////////////////////////////////////////////

function FCALL_DYNAMIC (func, applyDirect, values, name, args) {
  var toCall;

  name = AQL_TO_STRING(name).toUpperCase();
  if (name.indexOf('::') > 0) {
    // user-defined function
    var prefix = DB_PREFIX(), reloaded = false;
    if (!UserFunctions.hasOwnProperty(prefix)) {
      reloadUserFunctions();
      reloaded = true;
    }

    if (!UserFunctions[prefix].hasOwnProperty(name) && !reloaded) {
      // last chance
      reloadUserFunctions();
    }

    if (!UserFunctions[prefix].hasOwnProperty(name)) {
      THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
    }

    toCall = UserFunctions[prefix][name].func;
  } else {
    // built-in function
    if (name === 'CALL' || name === 'APPLY') {
      THROW(func, INTERNAL.errors.ERROR_QUERY_DISALLOWED_DYNAMIC_CALL, NORMALIZE_FNAME(name));
    }

    if (!exports.hasOwnProperty('AQL_' + name)) {
      THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, NORMALIZE_FNAME(name));
    }

    toCall = exports['AQL_' + name];
  }

  if (applyDirect) {
    try {
      return FIX_VALUE(toCall.apply({ name: name }, args));
    } catch (err) {
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
        result[i] = FIX_VALUE(toCall.apply({ name: name }, args));
      }
    }
    return result;
  } else if (type === TYPEWEIGHT_ARRAY) {
    result = [];
    for (i = 0; i < values.length; ++i) {
      args[0] = values[i];
      result[i] = FIX_VALUE(toCall.apply({ name: name }, args));
    }
    return result;
  }

  WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the numeric value or undefined if it is out of range
// //////////////////////////////////////////////////////////////////////////////

function NUMERIC_VALUE (value, nullify) {
  'use strict';

  if (value === null || isNaN(value) || !isFinite(value)) {
    if (nullify) {
      return null;
    }
    return 0;
  }

  return value;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the values of an object in the order that they are defined
// //////////////////////////////////////////////////////////////////////////////

function VALUES (value) {
  'use strict';

  var values = [];

  Object.keys(value).forEach(function (k) {
    values.push(value[k]);
  });

  return values;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief extract key names from an argument list
// //////////////////////////////////////////////////////////////////////////////

function EXTRACT_KEYS (args, startArgument, func) {
  'use strict';

  var keys = { }, i, j, key, key2;

  for (i = startArgument; i < args.length; ++i) {
    key = args[i];
    if (typeof key === 'string') {
      keys[key] = true;
    } else if (typeof key === 'number') {
      keys[String(key)] = true;
    } else if (Array.isArray(key)) {
      for (j = 0; j < key.length; ++j) {
        key2 = key[j];
        if (typeof key2 === 'string') {
          keys[key2] = true;
        } else {
          WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
          return null;
        }
      }
    }
  }

  return keys;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the keys of an array or object in a comparable way
// //////////////////////////////////////////////////////////////////////////////

function KEYS (value, doSort) {
  'use strict';

  var keys;

  if (Array.isArray(value)) {
    var n = value.length, i;
    keys = [];

    for (i = 0; i < n; ++i) {
      keys.push(i);
    }
  } else {
    keys = Object.keys(value);

    if (doSort) {
      // object keys need to be sorted by names
      keys.sort();
    }
  }

  return keys;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the keys of an array or object in a comparable way
// //////////////////////////////////////////////////////////////////////////////

function KEYLIST (lhs, rhs) {
  'use strict';

  if (Array.isArray(lhs)) {
    // lhs & rhs are lists
    return KEYS(lhs.length > rhs.length ? lhs : rhs);
  }

  // lhs & rhs are arrays
  var a, keys = KEYS(lhs);
  for (a in rhs) {
    if (rhs.hasOwnProperty(a) && !lhs.hasOwnProperty(a)) {
      keys.push(a);
    }
  }

  // object keys need to be sorted by names
  keys.sort();

  return keys;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get an indexed value from an array or document (e.g. users[3])
// //////////////////////////////////////////////////////////////////////////////

function GET_INDEX (value, index) {
  'use strict';

  if (TYPEWEIGHT(value) === TYPEWEIGHT_NULL) {
    return null;
  }

  var result = null;
  if (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT) {
    result = value[String(index)];
  } else if (TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY) {
    var i = parseInt(index, 10);
    if (i < 0) {
      // negative indexes fetch the element from the end, e.g. -1 => value[value.length - 1]
      i = value.length + i;
    }

    if (i >= 0 && i <= value.length - 1) {
      result = value[i];
    }
  } else {
    return null;
  }

  if (TYPEWEIGHT(result) === TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief normalize a value for comparison, sorting etc.
// //////////////////////////////////////////////////////////////////////////////

function NORMALIZE (value) {
  'use strict';

  if (value === null || value === undefined) {
    return null;
  }

  if (typeof (value) !== 'object') {
    return value;
  }

  var result;

  if (Array.isArray(value)) {
    result = [];
    value.forEach(function (v) {
      result.push(NORMALIZE(v));
    });
  } else {
    result = { };
    KEYS(value, true).forEach(function (a) {
      result[a] = NORMALIZE(value[a]);
    });
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get an attribute from a document (e.g. users.name)
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a document by its unique id or their unique ids
// //////////////////////////////////////////////////////////////////////////////

function DOCUMENT_HANDLE (id) {
  'use strict';

  if (TYPEWEIGHT(id) === TYPEWEIGHT_ARRAY) {
    var result = [], i;
    for (i = 0; i < id.length; ++i) {
      try {
        result.push(INTERNAL.db._document(id[i]));
      } catch (e1) {}
    }
    return result;
  }

  try {
    return INTERNAL.db._document(id);
  } catch (e2) {
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a document by its unique id or their unique ids
// //////////////////////////////////////////////////////////////////////////////

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
    var c = COLLECTION(collection, 'DOCUMENT');

    var result = [], i;
    for (i = 0; i < id.length; ++i) {
      try {
        result.push(c.document(id[i]));
      } catch (e1) {}
    }
    return result;
  }

  try {
    if (TYPEWEIGHT(collection) !== TYPEWEIGHT_STRING) {
      WARN('DOCUMENT', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return null;
    }
    return COLLECTION(collection, 'DOCUMENT').document(id);
  } catch (e2) {
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get all documents from the specified collection
// //////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS (collection, func) {
  'use strict';

  WARN(null, INTERNAL.errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION, AQL_TO_STRING(collection));

  if (isCoordinator) {
    return COLLECTION(collection, func).all().toArray();
  }

  return COLLECTION(collection, func).ALL(0, null).documents;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get names of all collections
// //////////////////////////////////////////////////////////////////////////////

function AQL_COLLECTIONS () {
  'use strict';

  var result = [];

  INTERNAL.db._collections().forEach(function (c) {
    result.push({
      _id: c._id,
      name: c.name()
    });
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the number of documents in a collection
// / this is an internal function that is not exposed to end users
// //////////////////////////////////////////////////////////////////////////////

function AQL_COLLECTION_COUNT (name) {
  'use strict';

  if (typeof name !== 'string') {
    THROW('COLLECTION_COUNT', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, 'COLLECTION_COUNT');
  }

  var c = INTERNAL.db._collection(name);
  if (c === null || c === undefined) {
    THROW('COLLECTION_COUNT', INTERNAL.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND, String(name));
  }
  return c.count();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief execute ternary operator
// /
// / the condition should be a boolean value, returns either the truepart
// / or the falsepart
// //////////////////////////////////////////////////////////////////////////////

function TERNARY_OPERATOR (condition, truePart, falsePart) {
  'use strict';

  if (condition) {
    return truePart();
  }
  return falsePart();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform logical and
// /
// / both operands must be boolean values, returns a boolean, uses short-circuit
// / evaluation
// //////////////////////////////////////////////////////////////////////////////

function LOGICAL_AND (lhs, rhs) {
  'use strict';

  var l = lhs();

  if (!l) {
    return l;
  }

  return rhs();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform logical or
// /
// / both operands must be boolean values, returns a boolean, uses short-circuit
// / evaluation
// //////////////////////////////////////////////////////////////////////////////

function LOGICAL_OR (lhs, rhs) {
  'use strict';

  var l = lhs();

  if (l) {
    return l;
  }

  return rhs();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform logical negation
// /
// / the operand must be a boolean values, returns a boolean
// //////////////////////////////////////////////////////////////////////////////

function LOGICAL_NOT (lhs) {
  'use strict';

  return !AQL_TO_BOOL(lhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform equality check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_FUNC (lhs, rhs, quantifier, func) {
  'use strict';

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_ARRAY) {
    return false;
  }

  var n = lhs.length, min, max;
  if (quantifier === 1) {
    // NONE
    if (n === 0) {
      return true;
    }
    min = max = 0;
  } else if (quantifier === 2) {
    // ALL
    if (n === 0) {
      return true;
    }
    min = max = n;
  } else if (quantifier === 3) {
    // ANY
    if (n === 0) {
      return false;
    }
    min = (n === 0 ? 0 : 1);
    max = n;
  }

  var left = n, matches = 0;
  for (var i = 0; i < n; ++i) {
    var result = func(lhs[i], rhs);
    --left;

    if (result) {
      ++matches;
      if (matches > max) {
        // too many matches
        return false;
      }
      if (matches >= min && matches + left <= max) {
        // enough matches
        return true;
      }
    } else {
      if (matches + left < min) {
        // too few matches
        return false;
      }
    }
  }

  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform equality check
// /
// / returns true if the operands are equal, false otherwise
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform inequality check
// /
// / returns true if the operands are unequal, false otherwise
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform greater than check (inner function)
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform greater than check
// /
// / returns true if the left operand is greater than the right operand
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATER (lhs, rhs) {
  'use strict';

  var result = RELATIONAL_GREATER_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform greater equal check (inner function)
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform greater equal check
// /
// / returns true if the left operand is greater or equal to the right operand
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATEREQUAL (lhs, rhs) {
  'use strict';

  var result = RELATIONAL_GREATEREQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform less than check (inner function)
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform less than check
// /
// / returns true if the left operand is less than the right operand
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESS (lhs, rhs) {
  'use strict';

  var result = RELATIONAL_LESS_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform less equal check (inner function)
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform less equal check
// /
// / returns true if the left operand is less or equal to the right operand
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESSEQUAL (lhs, rhs) {
  'use strict';

  var result = RELATIONAL_LESSEQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform comparison
// /
// / returns -1 if the left operand is less than the right operand, 1 if it is
// / greater, 0 if both operands are equal
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform in list check
// /
// / returns true if the left operand is contained in the right operand
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform not-in list check
// /
// / returns true if the left operand is not contained in the right operand
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_NOT_IN (lhs, rhs) {
  'use strict';

  return !RELATIONAL_IN(lhs, rhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform equality check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_EQUAL (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_EQUAL);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform unequality check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_UNEQUAL (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_UNEQUAL);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform greater check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_GREATER (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_GREATER);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform greater equal check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_GREATEREQUAL (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_GREATEREQUAL);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform less check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_LESS (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_LESS);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform less equal check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_LESSEQUAL (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_LESSEQUAL);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform in check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_IN (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_IN);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform in check for arrays
// //////////////////////////////////////////////////////////////////////////////

function RELATIONAL_ARRAY_NOT_IN (lhs, rhs, quantifier) {
  'use strict';

  return RELATIONAL_ARRAY_FUNC(lhs, rhs, quantifier, RELATIONAL_NOT_IN);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform unary plus operation
// //////////////////////////////////////////////////////////////////////////////

function UNARY_PLUS (value) {
  'use strict';

  value = AQL_TO_NUMBER(value);
  return AQL_TO_NUMBER(+ value);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform unary minus operation
// //////////////////////////////////////////////////////////////////////////////

function UNARY_MINUS (value) {
  'use strict';

  value = AQL_TO_NUMBER(value);
  return AQL_TO_NUMBER(- value);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform arithmetic plus or string concatenation
// //////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_PLUS (lhs, rhs) {
  'use strict';

  lhs = AQL_TO_NUMBER(lhs);
  rhs = AQL_TO_NUMBER(rhs);
  return AQL_TO_NUMBER(lhs + rhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform arithmetic minus
// //////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MINUS (lhs, rhs) {
  'use strict';

  lhs = AQL_TO_NUMBER(lhs);
  rhs = AQL_TO_NUMBER(rhs);
  return AQL_TO_NUMBER(lhs - rhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform arithmetic multiplication
// //////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_TIMES (lhs, rhs) {
  'use strict';

  lhs = AQL_TO_NUMBER(lhs);
  rhs = AQL_TO_NUMBER(rhs);
  return AQL_TO_NUMBER(lhs * rhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform arithmetic division
// //////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_DIVIDE (lhs, rhs) {
  'use strict';

  lhs = AQL_TO_NUMBER(lhs);
  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === 0 || rhs === null || isNaN(rhs) || !isFinite(rhs)) {
    WARN(null, INTERNAL.errors.ERROR_QUERY_DIVISION_BY_ZERO);
    return null;
  }

  return AQL_TO_NUMBER(lhs / rhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform arithmetic modulus
// //////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MODULUS (lhs, rhs) {
  'use strict';

  lhs = AQL_TO_NUMBER(lhs);
  rhs = AQL_TO_NUMBER(rhs);
  if (rhs === 0 || rhs === null || isNaN(rhs) || !isFinite(rhs)) {
    WARN(null, INTERNAL.errors.ERROR_QUERY_DIVISION_BY_ZERO);
    return null;
  }

  return AQL_TO_NUMBER(lhs % rhs);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform string concatenation
// //////////////////////////////////////////////////////////////////////////////

function AQL_CONCAT () {
  'use strict';

  var result = '', what = arguments;
  if (what.length === 1 && Array.isArray(what[0])) {
    what = arguments[0];
  }

  var i, n = what.length;

  for (i = 0; i < n; ++i) {
    var element = what[i];
    var weight = TYPEWEIGHT(element);
    if (weight === TYPEWEIGHT_NULL) {
      continue;
    }
    result += AQL_TO_STRING(element);
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief perform string concatenation using a separator character
// //////////////////////////////////////////////////////////////////////////////

function AQL_CONCAT_SEPARATOR () {
  'use strict';

  var separator, found = false, result = '', i, element;

  if (arguments.length === 2 && Array.isArray(arguments[1])) {
    separator = AQL_TO_STRING(arguments[0]);
    for (i = 0; i < arguments[1].length; ++i) {
      element = arguments[1][i];
      if (TYPEWEIGHT(element) === TYPEWEIGHT_NULL) {
        continue;
      }
      if (found) {
        result += separator;
      }
      result += AQL_TO_STRING(element);
      found = true;
    }
    return result;
  }

  for (i = 0; i < arguments.length; ++i) {
    element = arguments[i];

    if (i > 0 && TYPEWEIGHT(element) === TYPEWEIGHT_NULL) {
      continue;
    }
    if (i === 0) {
      separator = AQL_TO_STRING(element);
    } else {
      if (found) {
        result += separator;
      }
      result += AQL_TO_STRING(element);
      found = true;
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the length of a string in characters (not bytes)
// //////////////////////////////////////////////////////////////////////////////

function AQL_CHAR_LENGTH (value) {
  'use strict';

  // https://mathiasbynens.be/notes/javascript-unicode
  return [...AQL_TO_STRING(value)].length;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief convert a string to lower case
// //////////////////////////////////////////////////////////////////////////////

function AQL_LOWER (value) {
  'use strict';

  return AQL_TO_STRING(value).toLowerCase();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief convert a string to upper case
// //////////////////////////////////////////////////////////////////////////////

function AQL_UPPER (value) {
  'use strict';

  return AQL_TO_STRING(value).toUpperCase();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a substring of the string
// //////////////////////////////////////////////////////////////////////////////

function AQL_SUBSTRING (value, offset, count) {
  'use strict';

  if (TYPEWEIGHT(count) !== TYPEWEIGHT_NULL) {
    count = AQL_TO_NUMBER(count);
  }

  return AQL_TO_STRING(value).substr(AQL_TO_NUMBER(offset), count);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief searches a substring in a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_CONTAINS (value, search, returnIndex) {
  'use strict';

  search = AQL_TO_STRING(search);

  var result;
  if (search.length === 0) {
    result = -1;
  } else {
    result = AQL_TO_STRING(value).indexOf(search);
  }

  if (AQL_TO_BOOL(returnIndex)) {
    return result;
  }

  return (result !== -1);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief searches a substring in a string, using a regex
// //////////////////////////////////////////////////////////////////////////////

function AQL_LIKE (value, regex, caseInsensitive) {
  'use strict';

  var modifiers = '';
  if (caseInsensitive) {
    modifiers += 'i';
  }

  regex = AQL_TO_STRING(regex);

  if (LikeCache[modifiers][regex] === undefined) {
    LikeCache[modifiers][regex] = COMPILE_LIKE(regex, modifiers);
  }

  try {
    return LikeCache[modifiers][regex].test(AQL_TO_STRING(value));
  } catch (err) {
    WARN('LIKE', INTERNAL.errors.ERROR_QUERY_INVALID_REGEX);
    return false;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief searches a substring in a string, using a regex
// //////////////////////////////////////////////////////////////////////////////

function AQL_REGEX_TEST (value, regex, caseInsensitive) {
  'use strict';

  var modifiers = '';
  if (caseInsensitive) {
    modifiers += 'i';
  }

  regex = AQL_TO_STRING(regex);

  try {
    if (RegexCache[modifiers][regex] === undefined) {
      RegexCache[modifiers][regex] = COMPILE_REGEX(regex, modifiers);
    }

    return RegexCache[modifiers][regex].test(AQL_TO_STRING(value));
  } catch (err) {
    WARN('REGEX_TEST', INTERNAL.errors.ERROR_QUERY_INVALID_REGEX);
    return false;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief replaces a substring in a string, using a regex
// //////////////////////////////////////////////////////////////////////////////

function AQL_REGEX_REPLACE (value, regex, replacement, caseInsensitive) {
  'use strict';

  var modifiers = 'g';
  if (caseInsensitive) {
    modifiers += 'i';
  }

  regex = AQL_TO_STRING(regex);

  try {
    if (RegexCache[modifiers][regex] === undefined) {
      RegexCache[modifiers][regex] = COMPILE_REGEX(regex, modifiers);
    }

    return AQL_TO_STRING(value).replace(RegexCache[modifiers][regex], AQL_TO_STRING(replacement));
  } catch (err) {
    WARN('REGEX_REPLACE', INTERNAL.errors.ERROR_QUERY_INVALID_REGEX);
    return false;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the leftmost parts of a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_LEFT (value, length) {
  'use strict';

  return AQL_TO_STRING(value).substr(0, AQL_TO_NUMBER(length));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the rightmost parts of a string
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns a trimmed version of a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_TRIM (value, chars) {
  'use strict';

  if (chars === 1) {
    return AQL_LTRIM(value);
  } else if (chars === 2) {
    return AQL_RTRIM(value);
  } else if (chars === null || chars === undefined || chars === 0) {
    return AQL_TO_STRING(value).replace(new RegExp('(^\\s+|\\s+$)', 'g'), '');
  }

  var pattern = CREATE_REGEX_PATTERN(chars);
  return AQL_TO_STRING(value).replace(new RegExp('(^[' + pattern + ']+|[' + pattern + ']+$)', 'g'), '');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief trim a value from the left
// //////////////////////////////////////////////////////////////////////////////

function AQL_LTRIM (value, chars) {
  'use strict';

  if (chars === null || chars === undefined) {
    chars = '^\\s+';
  } else {
    chars = '^[' + CREATE_REGEX_PATTERN(chars) + ']+';
  }

  return AQL_TO_STRING(value).replace(new RegExp(chars, 'g'), '');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief trim a value from the right
// //////////////////////////////////////////////////////////////////////////////

function AQL_RTRIM (value, chars) {
  'use strict';

  if (chars === null || chars === undefined) {
    chars = '\\s+$';
  } else {
    chars = '[' + CREATE_REGEX_PATTERN(chars) + ']+$';
  }

  return AQL_TO_STRING(value).replace(new RegExp(chars, 'g'), '');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief split a string using a separator
// //////////////////////////////////////////////////////////////////////////////

function AQL_SPLIT (value, separator, limit) {
  'use strict';

  if (separator === null || separator === undefined) {
    return [ AQL_TO_STRING(value) ];
  }

  if (TYPEWEIGHT(limit) === TYPEWEIGHT_NULL) {
    limit = undefined;
  } else {
    limit = AQL_TO_NUMBER(limit);
  }

  if (limit < 0) {
    WARN('SPLIT', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (TYPEWEIGHT(separator) === TYPEWEIGHT_ARRAY) {
    var patterns = [];
    separator.forEach(function (s) {
      patterns.push(CREATE_REGEX_PATTERN(AQL_TO_STRING(s)));
    });

    return AQL_TO_STRING(value).split(new RegExp(patterns.join('|'), 'g'), limit);
  }

  return AQL_TO_STRING(value).split(AQL_TO_STRING(separator), limit);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief replace a search value inside a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_SUBSTITUTE (value, search, replace, limit) {
  'use strict';

  var pattern = '', patterns, replacements = { }, sWeight = TYPEWEIGHT(search);
  value = AQL_TO_STRING(value);

  if (sWeight === TYPEWEIGHT_OBJECT) {
    patterns = [];
    KEYS(search, false).forEach(function (k) {
      patterns.push(CREATE_REGEX_PATTERN(k));
      replacements[k] = AQL_TO_STRING(search[k]);
    });
    pattern = patterns.join('|');
    limit = replace;
  } else if (sWeight === TYPEWEIGHT_STRING) {
    pattern = CREATE_REGEX_PATTERN(search);
    if (TYPEWEIGHT(replace) === TYPEWEIGHT_NULL) {
      replacements[search] = '';
    } else {
      replacements[search] = AQL_TO_STRING(replace);
    }
  } else if (sWeight === TYPEWEIGHT_ARRAY) {
    if (search.length === 0) {
      // empty list
      WARN('SUBSTITUTE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return value;
    }

    patterns = [];

    if (TYPEWEIGHT(replace) === TYPEWEIGHT_ARRAY) {
      // replace each occurrence with a member from the second list
      search.forEach(function (k, i) {
        k = AQL_TO_STRING(k);
        patterns.push(CREATE_REGEX_PATTERN(k));
        if (i < replace.length) {
          replacements[k] = AQL_TO_STRING(replace[i]);
        } else {
          replacements[k] = '';
        }
      });
    } else {
      // replace all occurrences with a constant string
      if (TYPEWEIGHT(replace) === TYPEWEIGHT_NULL) {
        replace = '';
      } else {
        replace = AQL_TO_STRING(replace);
      }
      search.forEach(function (k) {
        k = AQL_TO_STRING(k);
        patterns.push(CREATE_REGEX_PATTERN(k));
        replacements[k] = replace;
      });
    }
    pattern = patterns.join('|');
  }

  if (pattern === '') {
    return value;
  }

  if (TYPEWEIGHT(limit) === TYPEWEIGHT_NULL) {
    limit = undefined;
  } else {
    limit = AQL_TO_NUMBER(limit);
  }

  if (limit < 0) {
    WARN('SUBSTITUTE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  return AQL_TO_STRING(value).replace(new RegExp(pattern, 'g'), function (match) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief generates the MD5 value for a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_MD5 (value) {
  'use strict';

  return INTERNAL.md5(AQL_TO_STRING(value));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief generates the SHA1 value for a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_SHA1 (value) {
  'use strict';

  return INTERNAL.sha1(AQL_TO_STRING(value));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief generates a hash value for an object
// //////////////////////////////////////////////////////////////////////////////

function AQL_HASH (value) {
  'use strict';

  return OBJECT_HASH(value);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the typename for an object
// //////////////////////////////////////////////////////////////////////////////

function AQL_TYPENAME (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_BOOL:
      return 'bool';
    case TYPEWEIGHT_NUMBER:
      return 'number';
    case TYPEWEIGHT_STRING:
      return 'string';
    case TYPEWEIGHT_ARRAY:
      return 'array';
    case TYPEWEIGHT_OBJECT:
      return 'object';
  }

  return 'null';
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief generates a random token of the specified length
// //////////////////////////////////////////////////////////////////////////////

function AQL_RANDOM_TOKEN (length) {
  'use strict';

  length = AQL_TO_NUMBER(length);

  if (length <= 0 || length > 65536) {
    THROW('RANDOM_TOKEN', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, 'RANDOM_TOKEN');
  }

  return INTERNAL.genRandomAlphaNumbers(length);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief finds search in value
// //////////////////////////////////////////////////////////////////////////////

function AQL_FIND_FIRST (value, search, start, end) {
  'use strict';

  if (TYPEWEIGHT(start) !== TYPEWEIGHT_NULL) {
    start = AQL_TO_NUMBER(start);
    if (start < 0) {
      return -1;
    }
  } else {
    start = 0;
  }

  if (TYPEWEIGHT(end) !== TYPEWEIGHT_NULL) {
    end = AQL_TO_NUMBER(end);
    if (end < start || end < 0) {
      return -1;
    }
  } else {
    end = undefined;
  }

  if (end !== undefined) {
    return AQL_TO_STRING(value).substr(0, end + 1).indexOf(AQL_TO_STRING(search), start);
  }

  return AQL_TO_STRING(value).indexOf(AQL_TO_STRING(search), start);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief finds search in value
// //////////////////////////////////////////////////////////////////////////////

function AQL_FIND_LAST (value, search, start, end) {
  'use strict';

  if (TYPEWEIGHT(start) !== TYPEWEIGHT_NULL) {
    start = AQL_TO_NUMBER(start);
  } else {
    start = undefined;
  }

  if (TYPEWEIGHT(end) !== TYPEWEIGHT_NULL) {
    end = AQL_TO_NUMBER(end);
    if (end < start || end < 0) {
      return -1;
    }
  } else {
    end = undefined;
  }

  var result;
  if (start > 0 || end !== undefined) {
    if (end === undefined) {
      result = AQL_TO_STRING(value).substr(start).lastIndexOf(AQL_TO_STRING(search));
    } else {
      result = AQL_TO_STRING(value).substr(start, end - start + 1).lastIndexOf(AQL_TO_STRING(search));
    }
    if (result !== -1) {
      result += start;
    }
  } else {
    result = AQL_TO_STRING(value).lastIndexOf(AQL_TO_STRING(search));
  }
  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief cast to a bool
// /
// / the operand can have any type, always returns a bool
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief cast to a number
// /
// / the operand can have any type, returns a number or null
// //////////////////////////////////////////////////////////////////////////////

function AQL_TO_NUMBER (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      // this covers Infinity and NaN
      return 0;
    case TYPEWEIGHT_BOOL:
      return (value ? 1 : 0);
    case TYPEWEIGHT_NUMBER:
      return value;
    case TYPEWEIGHT_STRING:
      var result = NUMERIC_VALUE(Number(value), false);
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
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief cast to a string
// /
// / the operand can have any type, always returns a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_TO_STRING (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return '';
    case TYPEWEIGHT_BOOL:
      return (value ? 'true' : 'false');
    case TYPEWEIGHT_STRING:
      return value;
    case TYPEWEIGHT_NUMBER:
      return value.toString();
    case TYPEWEIGHT_ARRAY:
    case TYPEWEIGHT_OBJECT:
      return JSON.stringify(value);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief cast to an array
// /
// / the operand can have any type, always returns a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_TO_ARRAY (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return [];
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the array as is, or an empty array
// //////////////////////////////////////////////////////////////////////////////

function AQL_ARRAYIZE (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    return [];
  }
  return value;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of type null
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_NULL (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NULL);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of type bool
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_BOOL (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_BOOL);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of type number
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_NUMBER (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NUMBER);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of type string
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_STRING (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_STRING);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of type array
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_ARRAY (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of type object
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_OBJECT (value) {
  'use strict';

  return (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if value is of a valid datestring
// /
// / returns a bool
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_DATESTRING (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_STRING) {
    return false;
  }

  // argument is a string

  // detect invalid dates ("foo" -> "fooZ" -> getTime() == NaN)
  var date = new Date(value);
  if (isNaN(date)) {
    return false;
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief integer closest to value, not greater than value
// //////////////////////////////////////////////////////////////////////////////

function AQL_FLOOR (value) {
  'use strict';

  return NUMERIC_VALUE(Math.floor(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief integer closest to value and not less than value
// //////////////////////////////////////////////////////////////////////////////

function AQL_CEIL (value) {
  'use strict';

  return NUMERIC_VALUE(Math.ceil(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief integer closest to value
// //////////////////////////////////////////////////////////////////////////////

function AQL_ROUND (value) {
  'use strict';

  return NUMERIC_VALUE(Math.round(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief absolute value
// //////////////////////////////////////////////////////////////////////////////

function AQL_ABS (value) {
  'use strict';

  return NUMERIC_VALUE(Math.abs(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief a random value between 0 and 1
// //////////////////////////////////////////////////////////////////////////////

function AQL_RAND () {
  'use strict';

  return Math.random();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief square root
// //////////////////////////////////////////////////////////////////////////////

function AQL_SQRT (value) {
  'use strict';

  return NUMERIC_VALUE(Math.sqrt(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief exponentation
// //////////////////////////////////////////////////////////////////////////////

function AQL_POW (base, exp) {
  'use strict';

  return NUMERIC_VALUE(Math.pow(AQL_TO_NUMBER(base), AQL_TO_NUMBER(exp)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief log(n)
// //////////////////////////////////////////////////////////////////////////////

function AQL_LOG (value) {
  'use strict';

  return NUMERIC_VALUE(Math.log(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief log(2)
// //////////////////////////////////////////////////////////////////////////////

function AQL_LOG2 (value) {
  'use strict';

  return NUMERIC_VALUE(Math.log2(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief log(10)
// //////////////////////////////////////////////////////////////////////////////

function AQL_LOG10 (value) {
  'use strict';

  return NUMERIC_VALUE(Math.log10(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief exp(n)
// //////////////////////////////////////////////////////////////////////////////

function AQL_EXP (value) {
  'use strict';

  return NUMERIC_VALUE(Math.exp(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief exp(2)
// //////////////////////////////////////////////////////////////////////////////

function AQL_EXP2 (value) {
  'use strict';

  return NUMERIC_VALUE(Math.pow(2, AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief sin
// //////////////////////////////////////////////////////////////////////////////

function AQL_SIN (value) {
  'use strict';

  return NUMERIC_VALUE(Math.sin(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief cos
// //////////////////////////////////////////////////////////////////////////////

function AQL_COS (value) {
  'use strict';

  return NUMERIC_VALUE(Math.cos(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief tan
// //////////////////////////////////////////////////////////////////////////////

function AQL_TAN (value) {
  'use strict';

  return NUMERIC_VALUE(Math.tan(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief asin
// //////////////////////////////////////////////////////////////////////////////

function AQL_ASIN (value) {
  'use strict';

  return NUMERIC_VALUE(Math.asin(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief acos
// //////////////////////////////////////////////////////////////////////////////

function AQL_ACOS (value) {
  'use strict';

  return NUMERIC_VALUE(Math.acos(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief atan
// //////////////////////////////////////////////////////////////////////////////

function AQL_ATAN (value) {
  'use strict';

  return NUMERIC_VALUE(Math.atan(AQL_TO_NUMBER(value)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief atan2
// //////////////////////////////////////////////////////////////////////////////

function AQL_ATAN2 (value1, value2) {
  'use strict';

  return NUMERIC_VALUE(Math.atan2(AQL_TO_NUMBER(value1), AQL_TO_NUMBER(value2)), true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief radians
// //////////////////////////////////////////////////////////////////////////////

function AQL_RADIANS (value) {
  'use strict';

  return NUMERIC_VALUE(value * (Math.PI / 180));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief degrees
// //////////////////////////////////////////////////////////////////////////////

function AQL_DEGREES (value) {
  'use strict';

  return NUMERIC_VALUE(value * (180 / Math.PI));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief pi
// //////////////////////////////////////////////////////////////////////////////

function AQL_PI () {
  'use strict';

  return Math.PI;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the length of a list, document or string
// //////////////////////////////////////////////////////////////////////////////

function AQL_LENGTH (value) {
  'use strict';

  var typeWeight = TYPEWEIGHT(value);

  if (typeWeight === TYPEWEIGHT_ARRAY) {
    return value.length;
  } else if (typeWeight === TYPEWEIGHT_OBJECT) {
    return KEYS(value, false).length;
  } else if (typeWeight === TYPEWEIGHT_NULL) {
    return 0;
  } else if (typeWeight === TYPEWEIGHT_BOOL) {
    return value ? 1 : 0;
  }

  // https://mathiasbynens.be/notes/javascript-unicode
  return [...AQL_TO_STRING(value)].length;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the first element of a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_FIRST (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN('FIRST', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  if (value.length === 0) {
    return null;
  }

  return value[0];
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the last element of a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_LAST (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN('LAST', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  if (value.length === 0) {
    return null;
  }

  return value[value.length - 1];
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the position of an element in a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_POSITION (value, search, returnIndex) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN('POSITION', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the nth element in a list, or null if the item does not exist
// //////////////////////////////////////////////////////////////////////////////

function AQL_NTH (value, position) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN('NTH', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  position = AQL_TO_NUMBER(position);
  if (position < 0 || position >= value.length) {
    return null;
  }

  return value[position];
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief reverse the elements in a list or in a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_REVERSE (value) {
  'use strict';

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    return value.split('').reverse().join('');
  }

  if (TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY) {
    return CLONE(value).reverse();
  }

  WARN('REVERSE', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a range of values
// //////////////////////////////////////////////////////////////////////////////

function AQL_RANGE (from, to, step) {
  'use strict';

  from = AQL_TO_NUMBER(from) || 0;
  to = AQL_TO_NUMBER(to) || 0;

  if (step === undefined || step === null) {
    if (from <= to) {
      step = 1;
    } else {
      step = -1;
    }
  }

  step = AQL_TO_NUMBER(step);

  // check if we would run into an endless loop
  if (step === 0 || step === null) {
    WARN('RANGE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  if (from < to && step < 0) {
    WARN('RANGE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
  if (from > to && step > 0) {
    WARN('RANGE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = [], i;
  if (step < 0 && to <= from) {
    for (i = from; i >= to; i += step) {
      result.push(i);
    }
  } else {
    for (i = from; i <= to; i += step) {
      result.push(i);
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a list of unique elements from the array
// //////////////////////////////////////////////////////////////////////////////

function AQL_UNIQUE (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('UNIQUE', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  var keys = { }, result = [];

  values.forEach(function (value) {
    var normalized = NORMALIZE(value);
    var key = JSON.stringify(normalized);

    if (!keys.hasOwnProperty(key)) {
      keys[key] = normalized;
      result.push(value);
    }
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a list of unique elements from the array
// //////////////////////////////////////////////////////////////////////////////

function AQL_SORTED_UNIQUE (values) {
  'use strict';

  var unique = AQL_UNIQUE(values);

  if (TYPEWEIGHT(unique) !== TYPEWEIGHT_ARRAY) {
    return null;
  }

  unique.sort(RELATIONAL_CMP);
  return unique;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief create the union (all) of all arguments
// //////////////////////////////////////////////////////////////////////////////

function AQL_UNION () {
  'use strict';

  var result = [], i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
        WARN('UNION', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief create the union (distinct) of all arguments
// //////////////////////////////////////////////////////////////////////////////

function AQL_UNION_DISTINCT () {
  'use strict';

  var keys = { }, i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
        WARN('UNION_DISTINCT', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        var normalized = NORMALIZE(element[j]);
        var key = JSON.stringify(normalized);

        if (!keys.hasOwnProperty(key)) {
          keys[key] = normalized;
        }
      }
    }
  }

  var result = [];
  Object.keys(keys).forEach(function (k) {
    result.push(keys[k]);
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief call a function for each element in the input list
// //////////////////////////////////////////////////////////////////////////////

function AQL_CALL (name) {
  'use strict';

  var args = [], i;
  for (i = 1; i < arguments.length; ++i) {
    args.push(arguments[i]);
  }

  return FCALL_DYNAMIC('CALL', true, null, name, args);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief call a function for each element in the input list
// //////////////////////////////////////////////////////////////////////////////

function AQL_APPLY (name, parameters) {
  'use strict';

  var args = [];
  if (Array.isArray(parameters)) {
    args = args.concat(parameters);
  }

  return FCALL_DYNAMIC('APPLY', true, null, name, args);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief removes elements from a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_REMOVE_VALUES (list, values) {
  'use strict';

  var type = TYPEWEIGHT(values);
  if (type === TYPEWEIGHT_NULL) {
    return list;
  } else if (type !== TYPEWEIGHT_ARRAY) {
    WARN('REMOVE_VALUES', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [];
  } else if (type === TYPEWEIGHT_ARRAY) {
    var copy = [], i;
    for (i = 0; i < list.length; ++i) {
      if (RELATIONAL_IN(list[i], values)) {
        continue;
      }
      copy.push(CLONE(list[i]));
    }
    return copy;
  }

  WARN('REMOVE_VALUES', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief removes an element from a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_REMOVE_VALUE (list, value, limit) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [];
  } else if (type === TYPEWEIGHT_ARRAY) {
    if (TYPEWEIGHT(limit) === TYPEWEIGHT_NULL) {
      limit = -1;
    }

    var copy = [], i;
    for (i = 0; i < list.length; ++i) {
      if (limit === -1 && RELATIONAL_CMP(list[i], value) === 0) {
        continue;
      } else if (limit > 0 && RELATIONAL_CMP(list[i], value) === 0) {
        --limit;
        continue;
      }
      copy.push(CLONE(list[i]));
    }
    return copy;
  }

  WARN('REMOVE_VALUE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief removes an element from a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_REMOVE_NTH (list, position) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [];
  } else if (type === TYPEWEIGHT_ARRAY) {
    position = AQL_TO_NUMBER(position);
    if (position >= list.length || position < - list.length) {
      return list;
    }
    if (position === 0) {
      return list.slice(1);
    } else if (position === - list.length) {
      return list.slice(position + 1);
    } else if (position === list.length - 1) {
      return list.slice(0, position);
    } else if (position < 0) {
      return list.slice(0, list.length + position).concat(list.slice(list.length + position + 1));
    }

    return list.slice(0, position).concat(list.slice(position + 1));
  }

  WARN('REMOVE_NTH', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief adds an element to a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_PUSH (list, value, unique) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ value ];
  } else if (type === TYPEWEIGHT_ARRAY) {
    if (AQL_TO_BOOL(unique)) {
      if (RELATIONAL_IN(value, list)) {
        return list;
      }
    }

    var copy = CLONE(list);
    copy.push(value);
    return copy;
  }

  WARN('PUSH', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief adds elements to a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_APPEND (list, values, unique) {
  'use strict';

  var type = TYPEWEIGHT(values);
  if (type === TYPEWEIGHT_NULL) {
    return list;
  } else if (type !== TYPEWEIGHT_ARRAY) {
    values = [ values ];
  }

  if (unique) {
    list = AQL_UNIQUE(list);
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
  } else if (type === TYPEWEIGHT_ARRAY) {
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

  WARN('APPEND', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief pops an element from a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_POP (list) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return null;
  } else if (type === TYPEWEIGHT_ARRAY) {
    if (list.length === 0) {
      return [];
    }
    var copy = CLONE(list);
    copy.pop();

    return copy;
  }

  WARN('POP', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief insert an element into a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_UNSHIFT (list, value, unique) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return [ value ];
  } else if (type === TYPEWEIGHT_ARRAY) {
    if (unique) {
      if (RELATIONAL_IN(value, list)) {
        return list;
      }
    }

    var copy = CLONE(list);
    copy.unshift(value);
    return copy;
  }

  WARN('UNSHIFT', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief pops an element from a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_SHIFT (list) {
  'use strict';

  var type = TYPEWEIGHT(list);
  if (type === TYPEWEIGHT_NULL) {
    return null;
  } else if (type === TYPEWEIGHT_ARRAY) {
    if (list.length === 0) {
      return [];
    }
    var copy = CLONE(list);
    copy.shift();

    return copy;
  }

  WARN('SHIFT', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief extract a slice from an array
// //////////////////////////////////////////////////////////////////////////////

function AQL_SLICE (value, from, to, nonNegative) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_ARRAY) {
    WARN('SLICE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  from = AQL_TO_NUMBER(from);
  if (from < 0) {
    from = value.length + from;
    if (from < 0) {
      from = 0;
    }
  }

  if (TYPEWEIGHT(to) !== TYPEWEIGHT_NULL) {
    to = AQL_TO_NUMBER(to);
  }

  if (nonNegative && (from < 0 || to < 0)) {
    return [];
  }

  if (TYPEWEIGHT(to) === TYPEWEIGHT_NULL) {
    to = undefined;
  } else {
    if (to >= 0) {
      to += from;
    } else {
      to = value.length + to;
      if (to < 0) {
        to = 0;
      }
    }
  }

  return value.slice(from, to);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief subtract lists from other lists
// //////////////////////////////////////////////////////////////////////////////

function AQL_MINUS () {
  'use strict';

  var keys = { }, i, first = true;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
        WARN('MINUS', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        var normalized = NORMALIZE(element[j]);
        var key = JSON.stringify(normalized);
        var contained = keys.hasOwnProperty(key);

        if (first) {
          if (!contained) {
            keys[key] = normalized;
          }
        } else if (contained) {
          delete keys[key];
        }
      }

      first = false;
    }
  }

  var result = [];
  Object.keys(keys).forEach(function (k) {
    result.push(keys[k]);
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief create the intersection of all arguments
// //////////////////////////////////////////////////////////////////////////////

function AQL_INTERSECTION () {
  'use strict';

  var result = [], i, first = true, keys = { };

  var func = function (value) {
    var normalized = NORMALIZE(value);
    keys[JSON.stringify(normalized)] = normalized;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
        WARN('INTERSECTION', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      if (first) {
        element.forEach(func);
        first = false;
      } else {
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

  Object.keys(keys).forEach(function (k) {
    result.push(keys[k]);
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief create the difference of all arguments, i.e. all unique elements
// / in the arrays
// //////////////////////////////////////////////////////////////////////////////

function AQL_OUTERSECTION () {
  'use strict';

  var i, keys = { };

  var func = function (value) {
    var normalized = NORMALIZE(value);
    var k = JSON.stringify(normalized);
    if (keys.hasOwnProperty(k)) {
      ++keys[k][1];
    } else {
      keys[k] = [value, 1];
    }
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
        WARN('OUTERSECTION', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      element.forEach(func);
    }
  }

  var result = [];
  Object.keys(keys).forEach(function (k) {
    if (keys[k][1] === 1) {
      result.push(keys[k][0]);
    }
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief flatten a list of lists
// //////////////////////////////////////////////////////////////////////////////

function AQL_FLATTEN (values, maxDepth, depth) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('FLATTEN', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  maxDepth = AQL_TO_NUMBER(maxDepth);
  if (TYPEWEIGHT(maxDepth) === TYPEWEIGHT_NULL || maxDepth < 1) {
    maxDepth = 1;
  }

  if (TYPEWEIGHT(depth) === TYPEWEIGHT_NULL) {
    depth = 0;
  }

  var value, result = [];
  var i, n;
  var p = function (v) {
    result.push(v);
  };

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    if (depth < maxDepth && TYPEWEIGHT(value) === TYPEWEIGHT_ARRAY) {
      AQL_FLATTEN(value, maxDepth, depth + 1).forEach(p);
    } else {
      result.push(value);
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief maximum of all values
// //////////////////////////////////////////////////////////////////////////////

function AQL_MAX (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('MAX', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief minimum of all values
// //////////////////////////////////////////////////////////////////////////////

function AQL_MIN (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('MIN', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief sum of all values
// //////////////////////////////////////////////////////////////////////////////

function AQL_SUM (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('SUM', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  var i, n;
  var value, typeWeight, result = 0;

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    typeWeight = TYPEWEIGHT(value);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN('SUM', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }
      result += value;
    }
  }

  return NUMERIC_VALUE(result);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief average of all values
// //////////////////////////////////////////////////////////////////////////////

function AQL_AVERAGE (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('AVERAGE', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  var current, typeWeight, sum = 0;
  var i, j, n;

  for (i = 0, j = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN('AVERAGE', INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief median of all values
// //////////////////////////////////////////////////////////////////////////////

function AQL_MEDIAN (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('MEDIAN', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  var copy = [], current, typeWeight;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN('MEDIAN', INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the pth percentile of all values
// //////////////////////////////////////////////////////////////////////////////

function AQL_PERCENTILE (values, p, method) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('PERCENTILE', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  if (TYPEWEIGHT(p) !== TYPEWEIGHT_NUMBER) {
    WARN('PERCENTILE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (p <= 0 || p > 100) {
    WARN('PERCENTILE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (method === null || method === undefined) {
    method = 'rank';
  }

  if (method !== 'interpolation' && method !== 'rank') {
    WARN('PERCENTILE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var copy = [], current, typeWeight;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN('PERCENTILE', INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
        return null;
      }

      copy.push(current);
    }
  }

  if (copy.length === 0) {
    return null;
  } else if (copy.length === 1) {
    return copy[0];
  }

  copy.sort(RELATIONAL_CMP);

  var idx, pos;
  if (method === 'interpolation') {
    // interpolation method
    idx = p * (copy.length + 1) / 100;
    pos = Math.floor(idx);
    var delta = idx - pos;

    if (pos >= copy.length) {
      return NUMERIC_VALUE(copy[copy.length - 1]);
    }
    return NUMERIC_VALUE(delta * (copy[pos] - copy[pos - 1]) + copy[pos - 1]);
  } else {
    // rank method
    idx = p * (copy.length) / 100;
    pos = Math.ceil(idx);

    if (pos >= copy.length) {
      return NUMERIC_VALUE(copy[copy.length - 1]);
    }
    return NUMERIC_VALUE(copy[pos - 1]);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief variance of all values
// //////////////////////////////////////////////////////////////////////////////

function VARIANCE (values) {
  'use strict';

  if (TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY) {
    WARN('VARIANCE', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
    return null;
  }

  var mean = 0, m2 = 0, current, typeWeight, delta;
  var i, j, n;

  for (i = 0, j = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        WARN('VARIANCE', INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief sample variance of all values
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief population variance of all values
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief standard deviation of all values
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief standard deviation of all values
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return at most <limit> documents near a certain point
// //////////////////////////////////////////////////////////////////////////////

function AQL_NEAR (collection, latitude, longitude, limit, distanceAttribute) {
  'use strict';

  if (TYPEWEIGHT(limit) === TYPEWEIGHT_NULL) {
    // use default value
    limit = 100;
  } else {
    if (TYPEWEIGHT(limit) !== TYPEWEIGHT_NUMBER) {
      THROW('NEAR', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    }
    limit = AQL_TO_NUMBER(limit);
  }

  var weight = TYPEWEIGHT(distanceAttribute);
  if (weight !== TYPEWEIGHT_NULL && weight !== TYPEWEIGHT_STRING) {
    THROW('NEAR', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }

  var query = COLLECTION(collection, 'NEAR').near(latitude, longitude);
  query._distance = distanceAttribute;
  return query.limit(limit).toArray();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return documents within <radius> around a certain point
// //////////////////////////////////////////////////////////////////////////////

function AQL_WITHIN (collection, latitude, longitude, radius, distanceAttribute) {
  'use strict';

  var weight = TYPEWEIGHT(distanceAttribute);
  if (weight !== TYPEWEIGHT_NULL && weight !== TYPEWEIGHT_STRING) {
    THROW('WITHIN', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }

  weight = TYPEWEIGHT(radius);
  if (weight !== TYPEWEIGHT_NULL && weight !== TYPEWEIGHT_NUMBER) {
    THROW('WITHIN', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }
  radius = AQL_TO_NUMBER(radius);

  // Just start a simple query
  var query = COLLECTION(collection, 'WITHIN').within(latitude, longitude, radius);
  query._distance = distanceAttribute;
  return query.toArray();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return documents within a bounding rectangle 
// //////////////////////////////////////////////////////////////////////////////

function AQL_WITHIN_RECTANGLE (collection, latitude1, longitude1, latitude2, longitude2) {
  'use strict';

  if (TYPEWEIGHT(latitude1) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(longitude1) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(latitude2) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(longitude2) !== TYPEWEIGHT_NUMBER) {
    WARN('WITHIN_RECTANGLE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  return COLLECTION(collection, 'WITHIN_RECTANGLE').withinRectangle(
    latitude1,
    longitude1,
    latitude2,
    longitude2
  ).toArray();
}

function AQL_DISTANCE (latitude1, longitude1, latitude2, longitude2) {
  if (TYPEWEIGHT(latitude1) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(longitude1) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(latitude2) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(longitude2) !== TYPEWEIGHT_NUMBER) {
    WARN('DISTANCE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var p1 = AQL_TO_NUMBER(latitude1) * (Math.PI / 180.0);
  var p2 = AQL_TO_NUMBER(latitude2) * (Math.PI / 180.0);
  var d1 = AQL_TO_NUMBER(latitude2 - latitude1) * (Math.PI / 180.0);
  var d2 = AQL_TO_NUMBER(longitude2 - longitude1) * (Math.PI / 180.0);

  var a = Math.sin(d1 / 2.0) * Math.sin(d1 / 2.0) +
          Math.cos(p1) * Math.cos(p2) *
          Math.sin(d2 / 2.0) * Math.sin(d2 / 2.0);
  var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1.0 - a));

  return AQL_TO_NUMBER(6371e3 * c);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return true if a point is contained inside a polygon
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_IN_POLYGON (points, latitude, longitude) {
  'use strict';

  if (TYPEWEIGHT(points) !== TYPEWEIGHT_ARRAY) {
    WARN('POINT_IN_POLYGON', INTERNAL.errors.ERROR_QUERY_ARRAY_EXPECTED);
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
    } else {
      // first list value is latitude, then longitude
      searchLat = latitude[0];
      searchLon = latitude[1];
      pointLat = 0;
      pointLon = 1;
    }
  } else if (TYPEWEIGHT(latitude) === TYPEWEIGHT_NUMBER &&
    TYPEWEIGHT(longitude) === TYPEWEIGHT_NUMBER) {
    searchLat = latitude;
    searchLon = longitude;
    pointLat = 0;
    pointLon = 1;
  } else {
    WARN('POINT_IN_POLYGON', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return documents that match a fulltext query
// //////////////////////////////////////////////////////////////////////////////

function AQL_FULLTEXT (collection, attribute, query, limit) {
  'use strict';

  var idx = INDEX_FULLTEXT(COLLECTION(collection, 'FULLTEXT'), attribute);

  if (idx === null) {
    THROW('FULLTEXT', INTERNAL.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING, collection);
  }

  // Just start a simple query
  if (limit !== undefined && limit !== null && limit > 0) {
    return COLLECTION(collection, 'FULLTEXT').fulltext(attribute, query, idx).limit(limit).toArray();
  }
  return COLLECTION(collection, 'FULLTEXT').fulltext(attribute, query, idx).toArray();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the first alternative that's not null until there are no more
// / alternatives. if neither of the alternatives is a value other than null,
// / then null will be returned
// /
// / the operands can have any type
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the first alternative that's a list until there are no more
// / alternatives. if neither of the alternatives is a list, then null will be
// / returned
// /
// / the operands can have any type
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the first alternative that's a document until there are no
// / more alternatives. if neither of the alternatives is a document, then null
// / will be returned
// /
// / the operands can have any type
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the parts of a document identifier separately
// /
// / returns a document with the attributes `collection` and `key` or fails if
// / the individual parts cannot be determined.
// //////////////////////////////////////////////////////////////////////////////

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
  } else if (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT) {
    if (value.hasOwnProperty('_id')) {
      return AQL_PARSE_IDENTIFIER(value._id);
    }
  // fall through intentional
  }

  WARN('PARSE_IDENTIFIER', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief validates if a document or object is from the specified collection
// //////////////////////////////////////////////////////////////////////////////

function AQL_IS_SAME_COLLECTION (collection, value) {
  'use strict';

  if (TYPEWEIGHT(value) === TYPEWEIGHT_OBJECT) {
    if (value.hasOwnProperty('_id')) {
      value = value._id;
    }
  }

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    var pos = value.indexOf('/');
    if (pos !== -1) {
      return value.substr(0, pos) === collection;
    }
  // fall through intentional
  }

  WARN('AQL_IS_SAME_COLLECTION', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief check whether a document has a specific attribute
// //////////////////////////////////////////////////////////////////////////////

function AQL_HAS (element, name) {
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    return false;
  }

  return element.hasOwnProperty(AQL_TO_STRING(name));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the attribute names of a document as a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_ATTRIBUTES (element, removeInternal, sort) {
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    WARN('ATTRIBUTES', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  if (removeInternal) {
    var result = [];

    Object.keys(element).forEach(function (k) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the attribute values of a document as a list
// //////////////////////////////////////////////////////////////////////////////

function AQL_VALUES (element, removeInternal) {
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    WARN('VALUES', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = [], a;

  for (a in element) {
    if (element.hasOwnProperty(a)) {
      if (a[0] !== '_' || !removeInternal) {
        result.push(element[a]);
      }
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assemble a document from two lists
// //////////////////////////////////////////////////////////////////////////////

function AQL_ZIP (keys, values) {
  'use strict';

  if (TYPEWEIGHT(keys) !== TYPEWEIGHT_ARRAY ||
    TYPEWEIGHT(values) !== TYPEWEIGHT_ARRAY ||
    keys.length !== values.length) {
    WARN('ZIP', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, i, n = keys.length;

  for (i = 0; i < n; ++i) {
    result[AQL_TO_STRING(keys[i])] = values[i];
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief JSON.stringify
// //////////////////////////////////////////////////////////////////////////////

function AQL_JSON_STRINGIFY (value) {
  'use strict';

  return JSON.stringify(value);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief JSON.parse
// //////////////////////////////////////////////////////////////////////////////

function AQL_JSON_PARSE (value) {
  'use strict';

  try {
    if (typeof value !== 'string') {
      WARN('JSON_PARSE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return null;
    }
    return JSON.parse(value);
  } catch (err) {
    WARN('JSON_PARSE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief unset specific attributes from a document
// //////////////////////////////////////////////////////////////////////////////

function AQL_UNSET (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_OBJECT) {
    WARN('UNSET', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, 'UNSET');
  // copy over all that is left

  for (var k in value) {
    if (value.hasOwnProperty(k) && !keys.hasOwnProperty(k)) {
      result[k] = CLONE(value[k]);
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief unset specific attributes from a document, recursively
// //////////////////////////////////////////////////////////////////////////////

function AQL_UNSET_RECURSIVE (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_OBJECT) {
    WARN('UNSET', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var keys = EXTRACT_KEYS(arguments, 1, 'UNSET');
  // copy over all that is left

  var func = function (value) {
    var result = { };
    for (var k in value) {
      if (value.hasOwnProperty(k) && !keys.hasOwnProperty(k)) {
        if (TYPEWEIGHT(value[k]) === TYPEWEIGHT_OBJECT) {
          result[k] = func(value[k], keys);
        } else {
          result[k] = CLONE(value[k]);
        }
      }
    }
    return result;
  };

  return func(value);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief keep specific attributes from a document
// //////////////////////////////////////////////////////////////////////////////

function AQL_KEEP (value) {
  'use strict';

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_OBJECT) {
    WARN('KEEP', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, 'KEEP');

  // copy over all that is left
  for (var k in value) {
    if (value.hasOwnProperty(k) && keys.hasOwnProperty(k)) {
      result[k] = CLONE(value[k]);
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief merge all arguments
// //////////////////////////////////////////////////////////////////////////////

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

  var i, element;

  if (arguments.length === 1) {
    element = arguments[0];
    if (TYPEWEIGHT(element) !== TYPEWEIGHT_ARRAY) {
      WARN('MERGE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return null;
    }

    for (i = 0; i < element.length; ++i) {
      if (TYPEWEIGHT(element[i]) !== TYPEWEIGHT_OBJECT) {
        WARN('MERGE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      add(element[i]);
    }

    return result;
  }

  var j = 0;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
        WARN('MERGE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      if (j === 0) {
        result = element;
      } else {
        add(element);
      }
      ++j;
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief merge all arguments recursively
// //////////////////////////////////////////////////////////////////////////////

function AQL_MERGE_RECURSIVE () {
  'use strict';

  var result = { }, i, recurse;

  recurse = function (old, element) {
    var r = CLONE(old);

    Object.keys(element).forEach(function (k) {
      if (r.hasOwnProperty(k) && TYPEWEIGHT(element[k]) === TYPEWEIGHT_OBJECT) {
        r[k] = recurse(r[k], element[k]);
      } else {
        r[k] = element[k];
      }
    });

    return r;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
        WARN('MERGE_RECURSIVE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }

      result = recurse(result, element);
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief translate a value, using a lookup document
// //////////////////////////////////////////////////////////////////////////////

function AQL_TRANSLATE (value, lookup, defaultValue) {
  'use strict';

  if (TYPEWEIGHT(lookup) !== TYPEWEIGHT_OBJECT) {
    WARN('TRANSLATE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief compare an object against a list of examples and return whether the
// / object matches any of the examples. returns the example index or a bool,
// / depending on the value of the control flag (3rd) parameter
// //////////////////////////////////////////////////////////////////////////////

function AQL_MATCHES (element, examples, returnIndex) {
  'use strict';

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_OBJECT) {
    return false;
  }

  if (!Array.isArray(examples)) {
    examples = [ examples ];
  }
  if (examples.length === 0) {
    WARN('MATCHES', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return false;
  }

  returnIndex = returnIndex || false;
  var i;
  for (i = 0; i < examples.length; ++i) {
    var example = examples[i];
    var result = true;
    if (TYPEWEIGHT(example) !== TYPEWEIGHT_OBJECT) {
      WARN('MATCHES', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      continue;
    }

    var keys = KEYS(example), key, j;
    for (j = 0; j < keys.length; ++j) {
      key = keys[j];

      if (!RELATIONAL_EQUAL(element[key], example[key])) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief passthru the argument
// /
// / this function is marked as non-deterministic so its argument withstands
// / query optimisation. this function can be used for testing
// //////////////////////////////////////////////////////////////////////////////

function AQL_PASSTHRU (value) {
  'use strict';

  return value;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test helper function
// / this is no actual function the end user should call
// //////////////////////////////////////////////////////////////////////////////

function AQL_TEST_INTERNAL (test, what) {
  'use strict';
  if (test === 'MODIFY_ARRAY') {
    what[0] = 1;
    what[1] = 42;
    what[2] = [ 1, 2 ];
    what[3].push([ 1, 2 ]);
    what[4] = { a: 9, b: 2 };
    what.push('foo');
    what.push('bar');
    what.pop();
  } else if (test === 'MODIFY_OBJECT') {
    what.a = 1;
    what.b = 3;
    what.c = [ 1, 2 ];
    what.d.push([ 1, 2 ]);
    what.e.f = { a: 1, b: 2 };
    delete what.f;
    what.g = 'foo';
  } else if (test === 'DEADLOCK') {
    var err = new ArangoError();
    err.errorNum = INTERNAL.errors.ERROR_DEADLOCK.code;
    err.errorMessage = INTERNAL.errors.ERROR_DEADLOCK.message;
    throw err;
  }
  return what;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief sleep
// /
// / sleep for the specified duration
// //////////////////////////////////////////////////////////////////////////////

function AQL_SLEEP (duration) {
  'use strict';

  duration = AQL_TO_NUMBER(duration);
  if (TYPEWEIGHT(duration) !== TYPEWEIGHT_NUMBER || duration < 0) {
    WARN('SLEEP', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  AQL_QUERY_SLEEP(duration);
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the current user
// / note: this might be null if the query is not executed in a context that
// / has a user
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the current database name
// / has a user
// //////////////////////////////////////////////////////////////////////////////

function AQL_CURRENT_DATABASE () {
  'use strict';

  return INTERNAL.db._name();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief always fail
// /
// / this function is non-deterministic so it is not executed at query
// / optimisation time. this function can be used for testing
// //////////////////////////////////////////////////////////////////////////////

function AQL_FAIL (message) {
  'use strict';

  if (TYPEWEIGHT(message) === TYPEWEIGHT_STRING) {
    THROW('FAIL', INTERNAL.errors.ERROR_QUERY_FAIL_CALLED, message);
  }

  THROW('FAIL', INTERNAL.errors.ERROR_QUERY_FAIL_CALLED, '');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief helper function for date creation
// //////////////////////////////////////////////////////////////////////////////

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
      if (!args[0].match(/([zZ]|[+\-]\d+(:\d+)?)$/) ||
        (args[0].match(/-\d+(:\d+)?$/) && !args[0].match(/[tT ]/))) {
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
    } else {
      if (weight === TYPEWEIGHT_STRING) {
        args[i] = parseInt(args[i], 10);
      } else if (weight !== TYPEWEIGHT_NUMBER) {
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

  var result = new Date(Date.UTC.apply(null, args));

  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NULL) {
    return result;
  }

  // avoid returning NaN here
  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the number of milliseconds since the Unix epoch
// /
// / this function is evaluated on every call
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_NOW () {
  'use strict';

  return Date.now();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the timestamp of the date passed (in milliseconds)
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_TIMESTAMP () {
  'use strict';

  try {
    return MAKE_DATE(arguments, 'DATE_TIMESTAMP').getTime();
  } catch (err) {
    WARN('DATE_TIMESTAMP', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the ISO string representation of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_ISO8601 () {
  'use strict';

  try {
    var dt = MAKE_DATE(arguments, 'DATE_ISO8601');
    if (dt === null) {
      return dt;
    }
    return dt.toISOString();
  } catch (err) {
    WARN('DATE_ISO8601', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the weekday of the date passed (0 = Sunday, 1 = Monday etc.)
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAYOFWEEK (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_DAYOFWEEK').getUTCDay();
  } catch (err) {
    WARN('DATE_DAYOFWEEK', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the year of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_YEAR (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_YEAR').getUTCFullYear();
  } catch (err) {
    WARN('DATE_YEAR', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the month of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_MONTH (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_MONTH').getUTCMonth() + 1;
  } catch (err) {
    WARN('DATE_MONTH', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the day of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAY (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_DAY').getUTCDate();
  } catch (err) {
    WARN('DATE_DAY', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the hours of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_HOUR (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_HOUR').getUTCHours();
  } catch (err) {
    WARN('DATE_HOUR', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the minutes of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_MINUTE (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_MINUTE').getUTCMinutes();
  } catch (err) {
    WARN('DATE_MINUTE', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the seconds of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_SECOND (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_SECOND').getUTCSeconds();
  } catch (err) {
    WARN('DATE_SECOND', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the milliseconds of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_MILLISECOND (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_MILLISECOND').getUTCMilliseconds();
  } catch (err) {
    WARN('DATE_MILLISECOND', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the day of the year of the date passed
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAYOFYEAR (value) {
  'use strict';

  try {
    var date = MAKE_DATE([ value ], 'DATE_DAYOFYEAR');
    var m = date.getUTCMonth();
    var d = date.getUTCDate();
    var ly = AQL_DATE_LEAPYEAR(date.getTime());
    // we could duplicate the leap year code here to avoid an extra MAKE_DATE() call...
    // var yr = date.getUTCFullYear()
    // var ly = !((yr % 4) || (!(yr % 100) && (yr % 400)))
    return (ly ? (dayOfLeapYearOffsets[m] + d) : (dayOfYearOffsets[m] + d));
  } catch (err) {
    WARN('DATE_DAYOFYEAR', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the ISO week date of the date passed (1..53)
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_ISOWEEK (value) {
  'use strict';

  try {
    var date = MAKE_DATE([ value ], 'DATE_ISOWEEK');
    date.setUTCHours(0, 0, 0, 0);
    date.setUTCDate(date.getUTCDate() + 4 - (date.getUTCDay() || 7));
    return Math.ceil((((date - Date.UTC(date.getUTCFullYear(), 0, 1)) / 864e5) + 1) / 7);
  } catch (err) {
    WARN('DATE_ISOWEEK', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return if year of the date passed is a leap year
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_LEAPYEAR (value) {
  'use strict';

  try {
    var yr = MAKE_DATE([ value ], 'DATE_LEAPYEAR').getUTCFullYear();
    return ((yr % 4 === 0) && (yr % 100 !== 0)) || (yr % 400 === 0);
  } catch (err) {
    WARN('DATE_LEAPYEAR', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the ISO week date of the date passed (1..53)
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_QUARTER (value) {
  'use strict';

  try {
    return MAKE_DATE([ value ], 'DATE_QUARTER').getUTCMonth() / 3 + 1 | 0;
  } catch (err) {
    WARN('DATE_QUARTER', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return number of days in month of date passed (leap year aware)
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DAYS_IN_MONTH (value) {
  'use strict';

  try {
    var date = MAKE_DATE([ value ], 'DATE_DAYS_IN_MONTH');
    var month = date.getUTCMonth() + 1;
    var ly = AQL_DATE_LEAPYEAR(date.getTime());
    return daysInMonth[month === 2 && ly ? 0 : month];
  } catch (err) {
    WARN('DATE_DAYS_IN_MONTH', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal function to add to or subtract from given date
// //////////////////////////////////////////////////////////////////////////////

function DATE_CALC (value, amount, unit, func) {
  'use strict';

  try {
    // TODO: check if isNaN? If called by AQL FOR-loop, should query throw
    // and terminate immediately, or return a bunch of 'null's? If it shall
    // stop, then best handled in MAKE_DATE() itself I guess.
    var date = MAKE_DATE([ value ], func);

    if (date === null) {
      WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
      return null;
    }

    var sign = (func === 'DATE_ADD' || func === undefined) ? 1 : -1;
    var m;

    // if amount is not a number, then it must be an ISO duration string
    if (TYPEWEIGHT(unit) === TYPEWEIGHT_NULL) {
      if (TYPEWEIGHT(amount) !== TYPEWEIGHT_STRING) {
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
      for (var d = duration.length - 1; d >= 1; d--) {
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
      if (TYPEWEIGHT(unit) !== TYPEWEIGHT_STRING) {
        WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }
      if (TYPEWEIGHT(amount) !== TYPEWEIGHT_NUMBER) {
        WARN(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return null;
      }
      m = unitMapping[unit.toLowerCase()]; // we're sure unit is a string here
      if (m === 'undefined') {
        WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
        return null;
      } else if (m[0] === 'w') {
        m = unitMapping.d;
        date[m[2]](date[m[1]]() + amount * 7 * sign);
      } else {
        date[m[2]](date[m[1]]() + amount * sign);
      }
      return date.toISOString();
    }
  } catch (err) {
    WARN(func, INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return date passed with added amount of time units
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_ADD (value, amount, unit) {
  'use strict';
  return DATE_CALC(value, amount, unit, 'DATE_ADD');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return date passed with subtracted amount of time units
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_SUBTRACT (value, amount, unit) {
  'use strict';
  return DATE_CALC(value, amount, unit, 'DATE_SUBTRACT');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return date difference in given unit, optionally with fractions
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_DIFF (value1, value2, unit, asFloat) {
  'use strict';

  var date1 = MAKE_DATE([ value1 ], 'DATE_DIFF');
  var date2 = MAKE_DATE([ value2 ], 'DATE_DIFF');
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
      WARN('DATE_DIFF', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
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
  } catch (err) {
    WARN('DATE_DIFF', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief compare two partial dates, using a substring of the dates
// //////////////////////////////////////////////////////////////////////////////

function AQL_DATE_COMPARE (value1, value2, unitRangeStart, unitRangeEnd) {
  try {
    // TODO: Should we handle leap years, so leapling birthdays occur every year?
    // It may result in unexpected behavior if it's used for something else but
    // birthday checking however. Probably best to leave compensation up to user query.
    var date1 = MAKE_DATE([ value1 ], 'DATE_COMPARE');
    var date2 = MAKE_DATE([ value2 ], 'DATE_COMPARE');
    if (TYPEWEIGHT(date1) === TYPEWEIGHT_NULL || TYPEWEIGHT(date2) === TYPEWEIGHT_NULL) {
      return null;
    }
    if (unitRangeEnd === undefined) {
      unitRangeEnd = unitRangeStart;
    }
    var start = unitStrRanges[unitRangeStart][0];
    var end = unitStrRanges[unitRangeEnd][1];
    if (start === undefined || end === undefined) {
      WARN('DATE_COMPARE', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
      return null;
    }
    var yr1 = date1.getUTCFullYear();
    if ((yr1 < 0 || yr1 > 9999) && start !== 0) {
      start += 3;
    }
    var yr2 = date2.getUTCFullYear();
    if (yr2 < 0 || yr2 > 9999) {
      end += 3;
    }
    var substr1 = date1.toISOString().slice(start, end);
    var substr2 = date2.toISOString().slice(start, end);
    // if unitRangeEnd > unitRangeStart, substrings will be empty
    if (!substr1 || !substr2) {
      WARN('DATE_COMPARE', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
      return null;
    }
    return substr1 === substr2;
  } catch (err) {
    WARN('DATE_COMPARE', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}
        
        
// //////////////////////////////////////////////////////////////////////////////
// / @brief return at most <limit> documents near a certain point
// //////////////////////////////////////////////////////////////////////////////

function AQL_PREGEL_RESULT (executionNr) {
  'use strict';

  if (isCoordinator) {
      return INTERNAL.db._pregelAqlResult(executionNr);
  } else {
    THROW('PREGEL_RESULT', INTERNAL.errors.ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief format a date (numerical values only)
// //////////////////////////////////////////////////////////////////////////////

// special escape sequence first, rest ordered by length
var dateMapRegExp = [
  '%&', '%yyyyyy', '%yyyy', '%mmmm', '%wwww', '%mmm', '%www', '%fff', '%xxx',
  '%yy', '%mm', '%dd', '%hh', '%ii', '%ss', '%kk', '%t', '%z', '%w', '%y', '%m',
  '%d', '%h', '%i', '%s', '%f', '%x', '%k', '%l', '%q', '%a', '%%', '%'
].join('|');

function AQL_DATE_FORMAT (value, format) {
  'use strict';
  try {
    var date = MAKE_DATE([ value ], 'DATE_FORMAT');
    var dateStr = date.toISOString();
    var yr = date.getUTCFullYear();
    var offset = yr < 0 || yr > 9999 ? 3 : 0;
    var dateMap = {
      '%t': function () { return date.getTime(); },
      '%z': function () { return dateStr; },
      '%w': function () { return AQL_DATE_DAYOFWEEK(dateStr); },
      '%y': function () { return date.getUTCFullYear(); },
      // there's no really sensible way to handle negative years, but better not drop the sign
      '%yy': function () { return (yr < 0 ? '-' : '') + dateStr.slice(2 + offset, 4 + offset); },
      // preserves full negative years (-000753 is not reduced to -753 or -0753)
      '%yyyy': function () { return dateStr.slice(0, 4 + offset); },
      // zero-pad 4 digit years to length of 6 and add "+" prefix, keep negative as-is
      '%yyyyyy': function () {
        return (yr >= 0 && yr <= 9999)
          ? '+' + zeropad(dateStr.slice(0, 4 + offset), 6)
          : dateStr.slice(0, 7);
      },
      '%m': function () { return date.getUTCMonth() + 1; },
      '%mm': function () { return dateStr.slice(5 + offset, 7 + offset); },
      '%d': function () { return date.getUTCDate(); },
      '%dd': function () { return dateStr.slice(8 + offset, 10 + offset); },
      '%h': function () { return date.getUTCHours(); },
      '%hh': function () { return dateStr.slice(11 + offset, 13 + offset); },
      '%i': function () { return date.getUTCMinutes(); },
      '%ii': function () { return dateStr.slice(14 + offset, 16 + offset); },
      '%s': function () { return date.getUTCSeconds(); },
      '%ss': function () { return dateStr.slice(17 + offset, 19 + offset); },
      '%f': function () { return date.getUTCMilliseconds(); },
      '%fff': function () { return dateStr.slice(20 + offset, 23 + offset); },
      '%x': function () { return AQL_DATE_DAYOFYEAR(dateStr); },
      '%xxx': function () { return zeropad(AQL_DATE_DAYOFYEAR(dateStr), 3); },
      '%k': function () { return AQL_DATE_ISOWEEK(dateStr); },
      '%kk': function () { return zeropad(AQL_DATE_ISOWEEK(dateStr), 2); },
      '%l': function () { return +AQL_DATE_LEAPYEAR(dateStr); },
      '%q': function () { return AQL_DATE_QUARTER(dateStr); },
      '%a': function () { return AQL_DATE_DAYS_IN_MONTH(dateStr); },
      '%mmm': function () { return monthNames[date.getUTCMonth()].substring(0, 3); },
      '%mmmm': function () { return monthNames[date.getUTCMonth()]; },
      '%www': function () { return weekdayNames[AQL_DATE_DAYOFWEEK(dateStr)].substring(0, 3); },
      '%wwww': function () { return weekdayNames[AQL_DATE_DAYOFWEEK(dateStr)]; },
      '%&': function () { return ''; }, // Allow for literal "m" after "%m" ("%mm" -> %m%&m)
      '%%': function () { return '%'; }, // Allow for literal "%y" using "%%y"
      '%': function () { return ''; }
    };
    var exp = new RegExp(dateMapRegExp, 'gi');
    format = format.replace(exp, function (match) {
      return dateMap[match.toLowerCase()]();
    });
    return format;
  } catch (err) {
    WARN('DATE_FORMAT', INTERNAL.errors.ERROR_QUERY_INVALID_DATE_VALUE);
    return null;
  }
}

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
exports.RELATIONAL_ARRAY_IN = RELATIONAL_ARRAY_IN;
exports.RELATIONAL_ARRAY_NOT_IN = RELATIONAL_ARRAY_NOT_IN;
exports.RELATIONAL_ARRAY_EQUAL = RELATIONAL_ARRAY_EQUAL;
exports.RELATIONAL_ARRAY_UNEQUAL = RELATIONAL_ARRAY_UNEQUAL;
exports.RELATIONAL_ARRAY_GREATER = RELATIONAL_ARRAY_GREATER;
exports.RELATIONAL_ARRAY_GREATEREQUAL = RELATIONAL_ARRAY_GREATEREQUAL;
exports.RELATIONAL_ARRAY_LESS = RELATIONAL_ARRAY_LESS;
exports.RELATIONAL_ARRAY_LESSEQUAL = RELATIONAL_ARRAY_LESSEQUAL;
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
exports.AQL_COLLECTION_COUNT = AQL_COLLECTION_COUNT;
exports.AQL_CONCAT = AQL_CONCAT;
exports.AQL_CONCAT_SEPARATOR = AQL_CONCAT_SEPARATOR;
exports.AQL_CHAR_LENGTH = AQL_CHAR_LENGTH;
exports.AQL_LOWER = AQL_LOWER;
exports.AQL_UPPER = AQL_UPPER;
exports.AQL_SUBSTRING = AQL_SUBSTRING;
exports.AQL_CONTAINS = AQL_CONTAINS;
exports.AQL_LIKE = AQL_LIKE;
exports.AQL_REGEX_TEST = AQL_REGEX_TEST;
exports.AQL_REGEX_REPLACE = AQL_REGEX_REPLACE;
exports.AQL_LEFT = AQL_LEFT;
exports.AQL_RIGHT = AQL_RIGHT;
exports.AQL_TRIM = AQL_TRIM;
exports.AQL_LTRIM = AQL_LTRIM;
exports.AQL_RTRIM = AQL_RTRIM;
exports.AQL_SPLIT = AQL_SPLIT;
exports.AQL_SUBSTITUTE = AQL_SUBSTITUTE;
exports.AQL_MD5 = AQL_MD5;
exports.AQL_SHA1 = AQL_SHA1;
exports.AQL_HASH = AQL_HASH;
exports.AQL_TYPENAME = AQL_TYPENAME;
exports.AQL_RANDOM_TOKEN = AQL_RANDOM_TOKEN;
exports.AQL_FIND_FIRST = AQL_FIND_FIRST;
exports.AQL_FIND_LAST = AQL_FIND_LAST;
exports.AQL_TO_BOOL = AQL_TO_BOOL;
exports.AQL_TO_NUMBER = AQL_TO_NUMBER;
exports.AQL_TO_STRING = AQL_TO_STRING;
exports.AQL_TO_ARRAY = AQL_TO_ARRAY;
exports.AQL_ARRAYIZE = AQL_ARRAYIZE;
exports.AQL_TO_LIST = AQL_TO_ARRAY; // alias
exports.AQL_IS_NULL = AQL_IS_NULL;
exports.AQL_IS_BOOL = AQL_IS_BOOL;
exports.AQL_IS_NUMBER = AQL_IS_NUMBER;
exports.AQL_IS_STRING = AQL_IS_STRING;
exports.AQL_IS_ARRAY = AQL_IS_ARRAY;
exports.AQL_IS_LIST = AQL_IS_ARRAY; // alias
exports.AQL_IS_OBJECT = AQL_IS_OBJECT;
exports.AQL_IS_DOCUMENT = AQL_IS_OBJECT; // alias
exports.AQL_IS_DATESTRING = AQL_IS_DATESTRING;
exports.AQL_FLOOR = AQL_FLOOR;
exports.AQL_CEIL = AQL_CEIL;
exports.AQL_ROUND = AQL_ROUND;
exports.AQL_ABS = AQL_ABS;
exports.AQL_RAND = AQL_RAND;
exports.AQL_SQRT = AQL_SQRT;
exports.AQL_POW = AQL_POW;
exports.AQL_LOG = AQL_LOG;
exports.AQL_LOG2 = AQL_LOG2;
exports.AQL_LOG10 = AQL_LOG10;
exports.AQL_EXP = AQL_EXP;
exports.AQL_EXP2 = AQL_EXP2;
exports.AQL_SIN = AQL_SIN;
exports.AQL_COS = AQL_COS;
exports.AQL_TAN = AQL_TAN;
exports.AQL_ASIN = AQL_ASIN;
exports.AQL_ACOS = AQL_ACOS;
exports.AQL_ATAN = AQL_ATAN;
exports.AQL_ATAN2 = AQL_ATAN2;
exports.AQL_RADIANS = AQL_RADIANS;
exports.AQL_DEGREES = AQL_DEGREES;
exports.AQL_PI = AQL_PI;
exports.AQL_LENGTH = AQL_LENGTH;
exports.AQL_FIRST = AQL_FIRST;
exports.AQL_LAST = AQL_LAST;
exports.AQL_POSITION = AQL_POSITION;
exports.AQL_NTH = AQL_NTH;
exports.AQL_REVERSE = AQL_REVERSE;
exports.AQL_RANGE = AQL_RANGE;
exports.AQL_UNIQUE = AQL_UNIQUE;
exports.AQL_SORTED_UNIQUE = AQL_SORTED_UNIQUE;
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
exports.AQL_OUTERSECTION = AQL_OUTERSECTION;
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
exports.AQL_DISTANCE = AQL_DISTANCE;
exports.AQL_IS_IN_POLYGON = AQL_IS_IN_POLYGON;
exports.AQL_FULLTEXT = AQL_FULLTEXT;
exports.AQL_NOT_NULL = AQL_NOT_NULL;
exports.AQL_FIRST_LIST = AQL_FIRST_LIST;
exports.AQL_FIRST_DOCUMENT = AQL_FIRST_DOCUMENT;
exports.AQL_PARSE_IDENTIFIER = AQL_PARSE_IDENTIFIER;
exports.AQL_IS_SAME_COLLECTION = AQL_IS_SAME_COLLECTION;
exports.AQL_HAS = AQL_HAS;
exports.AQL_ATTRIBUTES = AQL_ATTRIBUTES;
exports.AQL_VALUES = AQL_VALUES;
exports.AQL_ZIP = AQL_ZIP;
exports.AQL_JSON_STRINGIFY = AQL_JSON_STRINGIFY;
exports.AQL_JSON_PARSE = AQL_JSON_PARSE;
exports.AQL_UNSET = AQL_UNSET;
exports.AQL_UNSET_RECURSIVE = AQL_UNSET_RECURSIVE;
exports.AQL_KEEP = AQL_KEEP;
exports.AQL_MERGE = AQL_MERGE;
exports.AQL_MERGE_RECURSIVE = AQL_MERGE_RECURSIVE;
exports.AQL_TRANSLATE = AQL_TRANSLATE;
exports.AQL_MATCHES = AQL_MATCHES;
exports.AQL_PASSTHRU = AQL_PASSTHRU;
exports.AQL_TEST_INTERNAL = AQL_TEST_INTERNAL;
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
exports.AQL_DATE_DAYS_IN_MONTH = AQL_DATE_DAYS_IN_MONTH;
exports.AQL_DATE_ADD = AQL_DATE_ADD;
exports.AQL_DATE_SUBTRACT = AQL_DATE_SUBTRACT;
exports.AQL_DATE_DIFF = AQL_DATE_DIFF;
exports.AQL_DATE_COMPARE = AQL_DATE_COMPARE;
exports.AQL_DATE_FORMAT = AQL_DATE_FORMAT;
exports.AQL_PREGEL_RESULT = AQL_PREGEL_RESULT;

exports.reload = reloadUserFunctions;
exports.clearCaches = clearCaches;
exports.lookupFunction = GET_USERFUNCTION;
exports.throwFromFunction = THROW;
exports.fixValue = FIX_VALUE;

// initialize the query engine
exports.clearCaches();
