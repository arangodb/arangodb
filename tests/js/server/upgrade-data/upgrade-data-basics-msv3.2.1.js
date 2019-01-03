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

function UpgradeData () {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (empty)
    ////////////////////////////////////////////////////////////////////////////
    testLargeCollection : function () {
      const c = db._collection('LargeCollection');
      assertEqual(c.count(), 10000);

      // verify documents and contents
      for (let i = 0; i < 10000; i++) {
        const doc = c.document( { _key: `key${i}` } );
        assertEqual(doc.even, ( ( i % 2 ) === 0 ));
        assertEqual(doc.name, `Name ${i}`);
        assertEqual(doc.num, i);
        assertEqual(doc.num100, i % 100);
      }

      // verify indexes
      const indices = c.getIndexes();
      assertEqual(indices.length, 5);

      // primary
      assertEqual(indices[0].type, "primary");
      assertEqual(indices[0].unique, true);

      // unique hash
      assertEqual(indices[1].type, "hash");
      assertEqual(indices[1].unique, true);
      assertEqual(indices[1].fields, [ "num" ]);

      const uhQuery =
        `FOR doc in LargeCollection
          FILTER doc.num == 8 || doc.num == 8001
          SORT doc.num ASC
          RETURN doc`;
      const uhExplain = db._createStatement(uhQuery).explain({});
      assertNotEqual(-1, uhExplain.plan.rules.indexOf('use-indexes'));
      const uhResults = db._query(uhQuery).toArray();
      assertEqual(uhResults.length, 2);
      assertEqual(uhResults[0].num, 8);
      assertEqual(uhResults[1].num, 8001);

      // non-unique hash
      assertEqual(indices[2].type, "hash");
      assertEqual(indices[2].unique, false);
      assertEqual(indices[2].fields, [ "even" ]);

      const nhQuery =
        `FOR doc in LargeCollection
          FILTER doc.even == true
          RETURN doc`;
      const nhExplain = db._createStatement(nhQuery).explain({});
      assertNotEqual(-1, nhExplain.plan.rules.indexOf('use-indexes'));
      const nhResults = db._query(nhQuery).toArray();
      assertEqual(nhResults.length, 5000);
      nhResults.forEach( ( doc ) => { assertTrue(doc.even); } );

      // unique skiplist
      assertEqual(indices[3].type, "skiplist");
      assertEqual(indices[3].unique, true);
      assertEqual(indices[3].fields, [ "name" ]);

      const usQuery =
        `FOR doc in LargeCollection
          FILTER doc.name >= "Name 1114" &&  doc.name <= "Name 1117"
          SORT doc.name ASC
          RETURN doc`;
      const usExplain = db._createStatement(usQuery).explain({});
      assertNotEqual(-1, usExplain.plan.rules.indexOf('use-indexes'));
      const usResults = db._query(usQuery).toArray();
      assertEqual(usResults.length, 4);
      assertEqual(usResults[0].name, "Name 1114");
      assertEqual(usResults[1].name, "Name 1115");
      assertEqual(usResults[2].name, "Name 1116");
      assertEqual(usResults[3].name, "Name 1117");

      // non-unique skiplist
      assertEqual(indices[4].type, "skiplist");
      assertEqual(indices[4].unique, false);
      assertEqual(indices[4].fields, [ "num100" ]);

      const nsQuery =
        `FOR doc in LargeCollection
          FILTER doc.num100 == 57
          RETURN doc`;
      const nsExplain = db._createStatement(nsQuery).explain({});
      assertNotEqual(-1, nsExplain.plan.rules.indexOf('use-indexes'));
      const nsResults = db._query(nsQuery).toArray();
      assertEqual(nsResults.length, 100);
      nsResults.forEach( ( doc ) => { assertEqual(doc.num100, 57); } );
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UpgradeData);

return jsunity.done();
