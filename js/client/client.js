/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
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
/// @author Achim Brandt
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief start paging
////////////////////////////////////////////////////////////////////////////////

function start_pager () {
  var internal = require("internal");

  internal.startPager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop paging
////////////////////////////////////////////////////////////////////////////////

function stop_pager () {
  var internal = require("internal");
  
  internal.stopPager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing with optional color
////////////////////////////////////////////////////////////////////////////////

function start_color_print (color) {
  require("internal").startColorPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_color_print () {
  require("internal").stopColorPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the overall help
////////////////////////////////////////////////////////////////////////////////

function help () {
  var internal = require("internal");
  var client = require("org/arangodb/arangosh");

  internal.print(client.HELP);
  internal.print(client.helpQueries);
  internal.print(client.helpArangoDatabase);
  internal.print(client.helpArangoCollection);
  internal.print(client.helpArangoStatement);
  internal.print(client.helpArangoQueryCursor);
  internal.print(client.helpExtended);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      initialisers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");
  var client = require("org/arangodb/arangosh");

////////////////////////////////////////////////////////////////////////////////
/// @brief general help
////////////////////////////////////////////////////////////////////////////////

  client.HELP = client.createHelpHeadline("Help") +
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

  client.helpQueries = client.createHelpHeadline("Select query help") +
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

  client.helpExtended = client.createHelpHeadline("More help") +
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
/// @brief create the global db object and load the collections
////////////////////////////////////////////////////////////////////////////////

  try {
    clear = function () {
      for (var i = 0; i < 100; ++i) {
        print('\n');
      }
    };

    if (typeof arango !== 'undefined') {
      
      // load simple queries
      require("simple-query");

      // prepopulate collection data arrays
      internal.db._collections();
      internal.edges._collections();

      if (! internal.ARANGO_QUIET) {
        internal.print(client.HELP);
      }
    }
  }
  catch (err) {
    internal.print("Caught startup error: ", String(err));
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
