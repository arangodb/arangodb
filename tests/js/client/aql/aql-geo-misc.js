/*global assertEqual, assertTrue, assertFalse, print */
"use strict";
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
const jsunity = require("jsunity");
const {db} = require("@arangodb");

const database = "myGeoDatabase";

function testGeoMisc() {
  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      db._create("col");
      db.col.ensureIndex({type: "geo", fields: [`geoloc.lat`, `geoloc.lon`]});
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testResetExecutorBts1950: function () {
      const query = `
        LET polygon = [
          [
            [
                55.2793527748355,
                -20.915228550665972
            ],
            [
                55.27008417364911,
                -20.956699600097522
            ],
            [
                55.272781834306045,
                -20.990067818924132
            ],
        [
            55.2793527748355,
            -20.915228550665972
        ]
          ]
        ]
        FOR x IN 1..10
          LIMIT 6
          FOR doc IN col
            FILTER GEO_CONTAINS(GEO_POLYGON(polygon), [ doc.geoloc.lon, doc.geoloc.lat ])
            RETURN {
              id: doc._key,
              lat: doc.geoloc.lat,
              lon: doc.geoloc.lon
            }`;
      // This test is about whether the query finishes or not.
      const result = db._query(query).toArray();
      assertEqual(result.length, 0);
    }
  };
}

jsunity.run(testGeoMisc);
return jsunity.done();
