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
    THROW(func, INTERNAL.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, String(name));
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
// / @brief call a user function
// //////////////////////////////////////////////////////////////////////////////

function FCALL_USER (name, parameters, func) {
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
    THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  try {
    return FIX_VALUE(UserFunctions[prefix][name].func.apply({ name: name }, parameters));
  } catch (err) {
    THROW(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err.stack || String(err)));
    return null;
  }
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert an expression
// //////////////////////////////////////////////////////////////////////////////

function AQL_ASSERT (expression, message) {
  'use strict';
  if(!AQL_TO_BOOL(expression)) {
    THROW('', INTERNAL.errors.ERROR_QUERY_USER_ASSERT, message);
  }
  return true;
}


exports.FCALL_USER = FCALL_USER;

exports.AQL_NEAR = AQL_NEAR;
exports.AQL_WITHIN = AQL_WITHIN;
exports.AQL_WITHIN_RECTANGLE = AQL_WITHIN_RECTANGLE;
exports.AQL_FULLTEXT = AQL_FULLTEXT;
exports.AQL_V8 = AQL_PASSTHRU;
exports.AQL_CURRENT_USER = AQL_CURRENT_USER;
exports.AQL_DATE_FORMAT = AQL_DATE_FORMAT;
exports.AQL_PREGEL_RESULT = AQL_PREGEL_RESULT;

exports.reload = reloadUserFunctions;
exports.fixValue = FIX_VALUE;
