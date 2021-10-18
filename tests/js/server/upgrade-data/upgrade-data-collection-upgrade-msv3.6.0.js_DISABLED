/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertEqual, assertTypeOf, assertNotEqual, fail */

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
var db = require('internal').db;
const request = require('@arangodb/request');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: upgrade with data from mmfiles instance
////////////////////////////////////////////////////////////////////////////////

function UpgradeData () {
  'use strict';

  const verifyCollectionData = () => {
    const c = db._collection('CollectionUpgrade');
    assertEqual(c.count(), 100);

    // verify documents and contents
    for (let i = 0; i < 100; i++) {
      const doc = c.document({ _key: `key${i}` });
      assertEqual(doc.name, `Name ${i}`);
      assertEqual(doc.num, i);
      assertEqual(doc.num10, i % 10);
      assertEqual(doc.location, { coordinates: [i, i], type: 'Point' });
    }

    // verify indexes
    const indices = c.getIndexes();
    assertEqual(indices.length, 5);

    // primary
    assertEqual(indices[0].type, "primary");
    assertEqual(indices[0].unique, true);
    const pResults = c.all().toArray();
    assertEqual(pResults.length, 100);

    // unique hash
    assertEqual(indices[1].type, "hash");
    assertEqual(indices[1].unique, true);
    assertEqual(indices[1].fields, ["num"]);

    const uhQuery =
      `FOR doc in CollectionUpgrade
          FILTER doc.num == 8 || doc.num == 81
          SORT doc.num ASC
          RETURN doc`;
    const uhExplain = db._createStatement(uhQuery).explain({});
    assertNotEqual(-1, uhExplain.plan.rules.indexOf('use-indexes'));
    const uhResults = db._query(uhQuery).toArray();
    assertEqual(uhResults.length, 2);
    assertEqual(uhResults[0].num, 8);
    assertEqual(uhResults[1].num, 81);

    // non-unique hash
    assertEqual(indices[2].type, "hash");
    assertEqual(indices[2].unique, false);
    assertEqual(indices[2].fields, ["num10"]);

    const nhQuery =
      `FOR doc in CollectionUpgrade
          FILTER doc.num10 == 0
          RETURN doc`;
    const nhExplain = db._createStatement(nhQuery).explain({});
    assertNotEqual(-1, nhExplain.plan.rules.indexOf('use-indexes'));
    const nhResults = db._query(nhQuery).toArray();
    assertEqual(nhResults.length, 10);
    nhResults.forEach((doc) => { assertEqual(doc.num10, 0); });

    // geo
    assertEqual(indices[3].type, "geo");
    assertEqual(indices[3].fields, ["location"]);

    const geoQuery =
      `LET box = GEO_POLYGON([
          [0.5, 0.5],
          [3.5, 0.5],
          [3.5, 3.5],
          [0.5, 3.5],
          [0.5, 0.5]
         ])
         FOR doc in CollectionUpgrade
          FILTER GEO_CONTAINS(box, doc.location)
          SORT doc.name ASC
          RETURN doc`;
    const geoExplain = db._createStatement(geoQuery).explain({});
    assertNotEqual(-1, geoExplain.plan.rules.indexOf('geo-index-optimizer'));
    const geoResults = db._query(geoQuery).toArray();
    assertEqual(geoResults.length, 3);
    assertEqual(geoResults[0].num, 1);
    assertEqual(geoResults[1].num, 2);
    assertEqual(geoResults[2].num, 3);

    // fulltext index
    assertEqual(indices[4].type, "fulltext");
    assertEqual(indices[4].fields, ["name"]);

    const ftQuery =
      `FOR doc in FULLTEXT(CollectionUpgrade, "name", "Name ")
          RETURN doc`;
    const ftExplain = db._createStatement(ftQuery).explain({});
    assertNotEqual(-1, ftExplain.plan.rules.indexOf('replace-function-with-index'));
    const ftResults = db._query(ftQuery).toArray();
    assertEqual(ftResults.length, 100);
  };

  const verifyEdgeCollectionData = () => {
    const c = db._collection('EdgeCollectionUpgrade');
    assertEqual(c.count(), 955);

    // verify indexes
    const indices = c.getIndexes();
    assertEqual(indices.length, 2);

    // primary
    assertEqual(indices[0].type, "primary");
    assertEqual(indices[0].unique, true);
    const pResults = c.all().toArray();
    assertEqual(pResults.length, 955);

    // edge
    assertEqual(indices[1].type, "edge");
    assertEqual(indices[1].unique, false);

    const edgeQuery1 =
      `FOR v in 1..1 OUTBOUND 'CollectionUpgrade/key10' EdgeCollectionUpgrade
          SORT v.num
          RETURN v`;
    const edgeResults1 = db._query(edgeQuery1).toArray();
    assertEqual(edgeResults1.length, 10);
    for (let i = 0; i < 10; i++) {
      assertEqual(edgeResults1[i].num, 10 + i);
    }
    const edgeQuery2 =
      `FOR v in 1..1 INBOUND 'CollectionUpgrade/key95' EdgeCollectionUpgrade
          SORT v.num
          RETURN v`;
    const edgeResults2 = db._query(edgeQuery2).toArray();
    assertEqual(edgeResults2.length, 10);
    for (let i = 0; i < 10; i++) {
      assertEqual(edgeResults2[i].num, 86 + i);
    }
  };

  return {

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (empty)
    ////////////////////////////////////////////////////////////////////////////
    testCollectionUpgrade : function () {
      verifyCollectionData();
      verifyEdgeCollectionData();

      let r = request({
        method: "PUT",
        url: "/_api/collection/CollectionUpgrade/upgrade"
      });
      assertEqual(r.statusCode, 200);
      
      verifyCollectionData();
      verifyEdgeCollectionData();

      const props = db.CollectionUpgrade.properties();
      assertTrue(props.syncByRevision);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UpgradeData);

return jsunity.done();
