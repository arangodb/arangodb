/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertTypeOf, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the view interface
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
/// @author Dan Larkin-York
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: upgrade with data from mmfiles instance
////////////////////////////////////////////////////////////////////////////////

function UpgradeData() {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (empty)
    ////////////////////////////////////////////////////////////////////////////
    testViewCreationWithExistingCollection: function() {
      const c = db._collection('LargeCollection');
      assertEqual(c.count(), 10000);

      let v;
      try {
        v = db._createView('TestView', 'arangosearch', {});
        const properties = {
          links: {
            'LargeCollection': {
              includeAllFields: true
            }
          }
        };
        v.properties(properties);

        const query =
          `FOR doc in TestView
          SEARCH(doc.name >= "Name 1114" &&  doc.name <= "Name 1117")
          OPTIONS { waitForSync: true }
          SORT doc.name ASC
          RETURN doc`;
        const results = db._query(query).toArray();
        assertEqual(results.length, 4);
        assertEqual(results[0].name, "Name 1114");
        assertEqual(results[1].name, "Name 1115");
        assertEqual(results[2].name, "Name 1116");
        assertEqual(results[3].name, "Name 1117");
      }
      finally {
        db._dropView('TestView');
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UpgradeData);

return jsunity.done();
