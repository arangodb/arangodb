/* jshint strict: false */
global.console = global.console || require('console');

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoShell client API
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
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
// / @author Achim Brandt
// / @author Dr. Frank Celler
// / @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arango = internal.arango;

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a formatted type string for object
// /
// / If the object has an id, it will be included in the string.
// //////////////////////////////////////////////////////////////////////////////

exports.getIdString = function (object, typeName) {
  var result = '[object ' + typeName;

  if (object._id) {
    result += ':' + object._id;
  } else if (object.data && object.data._id) {
    result += ':' + object.data._id;
  }

  result += ']';

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a formatted headline text
// //////////////////////////////////////////////////////////////////////////////

exports.createHelpHeadline = function (text) {
  var i;
  var p = '';
  var x = Math.abs(78 - text.length) / 2;

  for (i = 0; i < x; ++i) {
    p += '-';
  }

  return '\n' + p + ' ' + text + ' ' + p + '\n';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief handles error results
// /
// / throws an exception in case of an an error
// //////////////////////////////////////////////////////////////////////////////

// must came after the export of createHelpHeadline
var arangodb = require('@arangodb');
var ArangoError = arangodb.ArangoError;

exports.checkRequestResult = function (requestResult) {
  if (requestResult === undefined) {
    throw new ArangoError({
      'error': true,
      'code': 500,
      'errorNum': arangodb.ERROR_INTERNAL,
      'errorMessage': 'Unknown error. Request result is empty'
    });
  }

  if (requestResult.hasOwnProperty('error')) {
    if (requestResult.error) {
      if (requestResult.errorNum === arangodb.ERROR_TYPE_ERROR) {
        throw new TypeError(requestResult.errorMessage);
      }

      const error = new ArangoError(requestResult);
      error.message = requestResult.message;
      throw error;
    }

    // remove the property from the original object
    delete requestResult.error;
  }

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief general help
// //////////////////////////////////////////////////////////////////////////////

exports.HELP = exports.createHelpHeadline('Help') +
'Predefined objects:                                                ' + '\n' +
'  arango:                               ArangoConnection           ' + '\n' +
'  db:                                   ArangoDatabase             ' + '\n' +
'  fm:                                   FoxxManager                ' + '\n' +
'Examples:                                                          ' + '\n' +
' > db._collections()                    list all collections       ' + '\n' +
' > db._query(<query>).toArray()         execute an AQL query       ' + '\n' +
' > db._explain(<query>)                 explain an AQL query       ' + '\n' +
' > help                                 show help pages            ' + '\n' +
' > exit                                                            ' + '\n' +
'Note: collection names and statuses may be cached in arangosh.     ' + '\n' +
'To refresh the list of collections and their statuses, issue:      ' + '\n' +
' > db._collections();                                              ' + '\n' +
'                                                                   ' + '\n' +
'To cancel the current prompt, press CTRL + d.                      ' + '\n';

// //////////////////////////////////////////////////////////////////////////////
  // / @brief query help
  // //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
  // / @brief extended help
  // //////////////////////////////////////////////////////////////////////////////

exports.helpExtended = exports.createHelpHeadline('More help') +
  'Pager:                                                              ' + '\n' +
  ' > stop_pager()                         stop the pager output       ' + '\n' +
  ' > start_pager()                        start the pager             ' + '\n' +
  'Pretty printing:                                                    ' + '\n' +
  ' > stop_pretty_print()                  stop pretty printing        ' + '\n' +
  ' > start_pretty_print()                 start pretty printing       ' + '\n' +
  'Color output:                                                       ' + '\n' +
  ' > stop_color_print()                   stop color printing         ' + '\n' +
  ' > start_color_print()                  start color printing        ' + '\n' +
  'Print function:                                                     ' + '\n' +
  ' > print(x)                             std. print function         ' + '\n' +
  ' > print_plain(x)                       print without prettifying   ' + '\n' +
  '                                        and without colors          ' + '\n' +
  ' > clear()                              clear screen                ';
