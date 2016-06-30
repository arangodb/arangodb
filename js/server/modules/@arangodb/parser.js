/* jshint strict: false, unused: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief parser functions
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
      result = parseInt(value, 10);
    } else if (format === '%f') {
      result = parseFloat(value);
    } else if (format === '%x') {
      result = parseInt(value, 16);
    } else if (format === '%o') {
      result = parseInt(value, 8);
    } else {
      error = new arangodb.ArangoError();
      error.errorNum = arangodb.ERROR_NOT_IMPLEMENTED;
      error.errorMessage = "format '" + format + "' not implemented";

      throw error;
    }
  } else {
    result = parseFloat(value);
  }

  if (result === null || result === undefined || isNaN(result)) {
    error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_PARSER_FAILED;
    error.errorMessage = "format '" + format + "' not implemented";

    throw error;
  }

  return result;
};
