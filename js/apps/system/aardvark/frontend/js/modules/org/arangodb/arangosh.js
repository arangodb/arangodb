module.define("org/arangodb/arangosh", function(exports, module) {
/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, regexp: true plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "arangosh"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a formatted type string for object
/// 
/// If the object has an id, it will be included in the string.
////////////////////////////////////////////////////////////////////////////////

exports.getIdString = function (object, typeName) {
  var result = "[object " + typeName;

  if (object._id) {
    result += ":" + object._id;
  }
  else if (object.data && object.data._id) {
    result += ":" + object.data._id;
  }

  result += "]";

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted headline text 
////////////////////////////////////////////////////////////////////////////////

exports.createHelpHeadline = function (text) {
  var i;
  var p = "";
  var x = Math.abs(78 - text.length) / 2;

  for (i = 0; i < x; ++i) {
    p += "-";
  }

  return "\n" + p + " " + text + " " + p + "\n";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error results
/// 
/// throws an exception in case of an an error
////////////////////////////////////////////////////////////////////////////////

// must came after the export of createHelpHeadline
var arangodb = require("org/arangodb");
var ArangoError = arangodb.ArangoError;

exports.checkRequestResult = function (requestResult) {
  if (requestResult === undefined) {
    requestResult = {
      "error" : true,
      "code"  : 500,
      "errorNum" : arangodb.ERROR_INTERNAL,
      "errorMessage" : "Unknown error. Request result is empty"
    };
  }

  if (requestResult.error !== undefined && requestResult.error) {    
    if (requestResult.errorNum === arangodb.ERROR_TYPE_ERROR) {
      throw new TypeError(requestResult.errorMessage);
    }

    throw new ArangoError(requestResult);
  }

  var copy = requestResult._shallowCopy;

  delete copy.error;

  return copy;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief general help
////////////////////////////////////////////////////////////////////////////////

exports.HELP = exports.createHelpHeadline("Help") +
  'Predefined objects:                                                ' + "\n" +
  '  arango:                               ArangoConnection           ' + "\n" +
  '  db:                                   ArangoDatabase             ' + "\n" +
  '  fm:                                   FoxxManager                ' + "\n" +
  'Examples:                                                          ' + "\n" +
  ' > db._collections()                    list all collections       ' + "\n" +
  ' > db._create(<name>)                   create a new collection    ' + "\n" +
  ' > db._drop(<name>)                     drop a collection          ' + "\n" +
  ' > db.<name>.toArray()                  list all documents         ' + "\n" +
  ' > id = db.<name>.save({ ... })         save a document            ' + "\n" +
  ' > db.<name>.remove(<_id>)              delete a document          ' + "\n" +
  ' > db.<name>.document(<_id>)            retrieve a document        ' + "\n" +
  ' > db.<name>.replace(<_id>, {...})      overwrite a document       ' + "\n" +
  ' > db.<name>.update(<_id>, {...})       partially update a document' + "\n" +
  ' > db.<name>.exists(<_id>)              check if document exists   ' + "\n" +
  ' > db._query(<query>).toArray()         execute an AQL query       ' + "\n" +
  ' > db._useDatabase(<name>)              switch database            ' + "\n" +
  ' > db._listDatabases()                  list existing databases    ' + "\n" +
  ' > help                                 show help pages            ' + "\n" +
  ' > exit                                                            ' + "\n" +
  'Note: collection names and statuses may be cached in arangosh.     ' + "\n" +
  'To refresh the list of collections and their statuses, issue:      ' + "\n" +
  ' > db._collections();                                              ' + "\n" +
  '                                                                   ' + "\n" +
  (internal.printBrowser ?
  'To cancel the current prompt, press CTRL + z.                      ' + "\n" +
  '                                                                   ' + "\n" +
  'Please note that all variables defined with the var keyword will   ' + "\n" +
  'disapper when the command is finished. To introduce variables that ' + "\n" +
  'are persisting until the next command, omit the var keyword.       ' + "\n" : 
  'To cancel the current prompt, press CTRL + d.                      ' + "\n");

////////////////////////////////////////////////////////////////////////////////
/// @brief query help
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extended help
////////////////////////////////////////////////////////////////////////////////

exports.helpExtended = exports.createHelpHeadline("More help") +
  'Pager:                                                              ' + "\n" +
  ' > stop_pager()                         stop the pager output       ' + "\n" +
  ' > start_pager()                        start the pager             ' + "\n" +
  'Pretty printing:                                                    ' + "\n" +
  ' > stop_pretty_print()                  stop pretty printing        ' + "\n" +
  ' > start_pretty_print()                 start pretty printing       ' + "\n" +
  'Color output:                                                       ' + "\n" +
  ' > stop_color_print()                   stop color printing         ' + "\n" +
  ' > start_color_print()                  start color printing        ' + "\n" +
  'Print function:                                                     ' + "\n" +
  ' > print(x)                             std. print function         ' + "\n" +
  ' > print_plain(x)                       print without prettifying   ' + "\n" +
  '                                        and without colors          ' + "\n" +
  ' > clear()                              clear screen                ' ;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
});
