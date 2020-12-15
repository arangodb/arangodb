/* jshint strict: false, unused: false */
/* global FORMAT_DATETIME, PARSE_DATETIME */

// //////////////////////////////////////////////////////////////////////////////
// / @brief formatter functions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');

// //////////////////////////////////////////////////////////////////////////////
// / @brief parses a number
// //////////////////////////////////////////////////////////////////////////////

exports.number = function (value, info, lang) {
  var error;
  var format;
  var result;

  if (info.hasOwnProperty('format')) {
    format = info.format;

    if (format === '%d') {
      result = value.toFixed(0);
    } else if (format === '%f') {
      result = String(value);
    } else {
      error = new arangodb.ArangoError();
      error.errorNum = arangodb.ERROR_NOT_IMPLEMENTED;
      error.errorMessage = "format '" + format + "' not implemented";

      throw error;
    }
  } else {
    result = value;
  }

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief format a float value
// //////////////////////////////////////////////////////////////////////////////

exports.formatFloat = function (value, args) {
  if (undefined === value || null === value) {
    return null;
  }

  if (undefined === args) {
    args = {};
  }

  var decPlaces = isNaN(args.decPlaces = Math.abs(args.decPlaces)) ? 2 : args.decPlaces;
  var decSeparator =
  args.decSeparator === undefined ? '.' : args.decSeparator;
  var thouSeparator =
  args.thouSeparator === undefined ? ',' : args.thouSeparator;

  var sign = value < 0 ? '-' : '';
  var i = '';
  i += parseInt(value = Math.abs(+value || 0).toFixed(decPlaces), 10);
  var j = i.length;
  j = (j > 3) ? (j % 3) : 0;

  return sign + (j ? i.substr(0, j) + thouSeparator : '') +
    i.substr(j).replace(/(\d{3})(?=\d)/g, '$1' + thouSeparator) +
    (decPlaces ? decSeparator + Math.abs(value - i).toFixed(decPlaces).slice(2) : '');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief format a datetime value
// //////////////////////////////////////////////////////////////////////////////

exports.formatDatetime = function (value, args) {
  if (undefined === value || null === value) {
    return null;
  }

  if (undefined === args) {
    args = {};
  }

  if (undefined === args.pattern) {
    args.pattern = "yyyy-MM-dd'T'HH:mm:ssZ";
  }

  if (undefined === args.timezone) {
    args.timezone = null;
  }

  if (undefined === args.lang) {
    args.lang = null;
  }

  return FORMAT_DATETIME(value, args.pattern, args.timezone, args.lang);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief join array
// //////////////////////////////////////////////////////////////////////////////

exports.joinNumbers = function (value, args) {
  if (undefined === value || null === value) {
    return null;
  }

  return value.join();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief parse a number
// //////////////////////////////////////////////////////////////////////////////

exports.parseFloat = function (value, args) {
  if (undefined === value || null === value) {
    return null;
  }

  if (undefined === args) {
    args = {};
  }

  var decPlaces = isNaN(args.decPlaces = Math.abs(args.decPlaces)) ? 2 : args.decPlaces;
  var decSeparator = args.decSeparator === undefined ? '.' : args.decSeparator;
  var thouSeparator = args.thouSeparator === undefined ? ',' : args.thouSeparator;

  var str = '';
  str += value;
  str = str.replace(thouSeparator, '');

  if ('.' !== decSeparator) {
    str = str.replace(decSeparator, '.');
  }

  if (decPlaces > 0) {
    return parseFloat(str);
  }

  return parseFloat(str);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief format a datetime value
// //////////////////////////////////////////////////////////////////////////////

exports.parseDatetime = function (value, args) {
  if (undefined === value || null === value) {
    return null;
  }

  if (undefined === args) {
    args = {};
  }

  if (undefined === args.pattern) {
    args.pattern = "yyyy-MM-dd'T'HH:mm:ssZ";
  }

  if (undefined === args.timezone) {
    args.timezone = null;
  }

  if (undefined === args.lang) {
    args.lang = null;
  }

  return PARSE_DATETIME(value, args.pattern, args.timezone, args.lang);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief split array
// //////////////////////////////////////////////////////////////////////////////

exports.splitNumbers = function (value, args) {
  var result = [];
  var i;

  if (undefined === value) {
    return null;
  }

  var values = value.split(',');

  for (i = 0; i < values.length; ++i) {
    result[i] = parseFloat(values[i]);
  }

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate >
// //////////////////////////////////////////////////////////////////////////////

exports.validateNotNull = function (value, args) {
  if (undefined === value || null === value) {
    return false;
  }

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate >
// //////////////////////////////////////////////////////////////////////////////

exports.validateGT = function (value, args) {
  if (undefined === value) {
    return false;
  }

  if (undefined === args) {
    args = {};
  }

  var cmpValue = args.compareValue;

  return value > cmpValue;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate >
// //////////////////////////////////////////////////////////////////////////////

exports.validateEQ = function (value, args) {
  if (undefined === value) {
    return false;
  }

  if (undefined === args) {
    args = {};
  }

  var cmpValue = args.compareValue;

  return value === cmpValue;
};
