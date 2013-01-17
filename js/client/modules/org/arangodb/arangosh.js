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

var ArangoError = require("org/arangodb/arango-error").ArangoError;

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
/// @brief handles error results
/// 
/// throws an exception in case of an an error
////////////////////////////////////////////////////////////////////////////////

exports.checkRequestResult = function (requestResult) {
  if (requestResult === undefined) {
    requestResult = {
      "error" : true,
      "code"  : 0,
      "errorNum" : 0,
      "errorMessage" : "Unknown error. Request result is empty"
    };
  }

  if (requestResult.error !== undefined && requestResult.error) {    
    throw new ArangoError(requestResult);
  }
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
  'Predefined objects:                                                 ' + "\n" +
  '  arango:                                ArangoConnection           ' + "\n" +
  '  db:                                    ArangoDatabase             ' + "\n" +
  'Example:                                                            ' + "\n" +
  ' > db._collections();                    list all collections       ' + "\n" +
  ' > db.<coll_name>.all().toArray();       list all documents         ' + "\n" +
  ' > id = db.<coll_name>.save({ ... });    save a document            ' + "\n" +
  ' > db.<coll_name>.remove(<_id>);         delete a document          ' + "\n" +
  ' > db.<coll_name>.document(<_id>);       get a document             ' + "\n" +
  ' > db.<coll_name>.replace(<_id>, {...}); overwrite a document       ' + "\n" +
  ' > db.<coll_name>.update(<_id>, {...});  partially update a document' + "\n" +
  ' > help                                  show help pages            ' + "\n" +
  ' > exit                                                             ' + "\n" +
  'Note: collection names may be cached in arangosh. To refresh them, issue: ' + "\n" +
  ' > db._collections();                                               ' + "\n" +
  (internal.printBrowser ?
  '                                                                          ' + "\n" +
  'Please note that all variables defined with the var keyword will disappear' + "\n" +
  'when the command is finished. To introduce variables that are persisted   ' + "\n" +
  'until the next command, you should omit the var keyword.                  ' + "\n" : '');

////////////////////////////////////////////////////////////////////////////////
/// @brief query help
////////////////////////////////////////////////////////////////////////////////

exports.helpQueries = exports.createHelpHeadline("Select query help") +
  'Create a select query:                                              ' + "\n" +
  ' > st = new ArangoStatement(db, { "query" : "for..." });            ' + "\n" +
  ' > st = db._createStatement({ "query" : "for..." });                ' + "\n" +
  'Set query options:                                                  ' + "\n" +
  ' > st.setBatchSize(<value>);     set the max. number of results     ' + "\n" +
  '                                 to be transferred per roundtrip    ' + "\n" +
  ' > st.setCount(<value>);         set count flag (return number of   ' + "\n" +
  '                                 results in "count" attribute)      ' + "\n" +
  'Get query options:                                                  ' + "\n" +
  ' > st.setBatchSize();            return the max. number of results  ' + "\n" +
  '                                 to be transferred per roundtrip    ' + "\n" +
  ' > st.getCount();                return count flag (return number of' + "\n" +
  '                                 results in "count" attribute)      ' + "\n" +
  ' > st.getQuery();                return query string                ' + "\n" +
  '                                 results in "count" attribute)      ' + "\n" +
  'Bind parameters to a query:                                         ' + "\n" +
  ' > st.bind(<key>, <value>);      bind single variable               ' + "\n" +
  ' > st.bind(<values>);            bind multiple variables            ' + "\n" +
  'Execute query:                                                      ' + "\n" +
  ' > c = st.execute();             returns a cursor                   ' + "\n" +
  'Get all results in an array:                                        ' + "\n" +
  ' > e = c.elements();                                                ' + "\n" +
  'Or loop over the result set:                                        ' + "\n" +
  ' > while (c.hasNext()) { print( c.next() ); }                       ';

////////////////////////////////////////////////////////////////////////////////
/// @brief extended help
////////////////////////////////////////////////////////////////////////////////

exports.helpExtended = exports.createHelpHeadline("More help") +
  'Pager:                                                              ' + "\n" +
  ' > stop_pager()                        stop the pager output        ' + "\n" +
  ' > start_pager()                       start the pager              ' + "\n" +
  'Pretty printing:                                                    ' + "\n" +
  ' > stop_pretty_print()                 stop pretty printing         ' + "\n" +
  ' > start_pretty_print()                start pretty printing        ' + "\n" +
  'Color output:                                                       ' + "\n" +
  ' > stop_color_print()                  stop color printing          ' + "\n" +
  ' > start_color_print()                 start color printing         ' + "\n" +
  ' > start_color_print(COLOR_BLUE)       set color                    ' + "\n" +
  'Print function:                                                     ' + "\n" +
  ' > print(x)                            std. print function          ' + "\n" +
  ' > print_plain(x)                      print without pretty printing' + "\n" +
  '                                       and without colors           ' + "\n" +
  ' > clear()                             clear screen                 ' ;

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
