/* global AQL_EXPLAIN*/
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const analyzers = require("@arangodb/analyzers");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual, assertNotEqual} = jsunity.jsUnity.assertions;
const useIndexes = 'use-indexes';
const removeFilterCoveredByIndex = "remove-filter-covered-by-index";
const moveFiltersIntoEnumerate = "move-filters-into-enumerate";
const sleep = require('internal').sleep;

function optimizerRuleInvertedIndexTestSuite() {
  const colName = 'UnitTestInvertedIndexCollection';
  let col;
  const docs = 10000;
  return {
    setUpAll: function () {
      col = db._create(colName);
      analyzers.save("my_geo", "geojson",{type: 'point'}, ["frequency", "norm", "position"]);
      let indexRes = col.ensureIndex({type: 'inverted',
                                      name: 'InvertedIndexUnsorted',
                                      fields: [{name:'data_field'},
                                               {name:'geo_field', analyzer:'my_geo'},
                                               {name:'custom_field', analyzer:'text_en'}]});
      let data = [];
      for (let i = 0; i < docs; i++) {
        if (i % 10 == 0) {
          data.push({count:i,
                     data_field:'value' + i % 100,
                     custom_field:"quick brown",
                     geo_field:{type: 'Point', coordinates: [37.615895, 55.7039]}});
        } else {
          data.push({count:i,
                     data_field:'value' + i % 100,
                     custom_field: i,
                     geo_field:{type: 'Point', coordinates: [37.615895, 55.7039]}});  
        }
      }
      col.insert(data);
      // wait for index to be in sync
      sleep(1);
      let syncWait = 100;
      const syncQuery = aql`
        FOR d IN ${col} OPTIONS {indexHint: "InvertedIndexUnsorted"}
          FILTER STARTS_WITH(d.data_field, 'value') COLLECT WITH COUNT INTO c  RETURN c`;
      let count  = db._query(syncQuery).toArray()[0];
      while (count < docs && (--syncWait) > 0) {
        sleep(1);
        count  = db._query(syncQuery).toArray()[0];
      }
    },
    tearDownAll: function () {
      col.drop();
    },
    testIndexNotHinted: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER d.data_field == 'value1'
          RETURN d`;
      const res = AQL_EXPLAIN(query.query, query.bindVars);
      const appliedRules = res.plan.rules;
      assertFalse(appliedRules.includes(useIndexes));
      assertFalse(appliedRules.includes(removeFilterCoveredByIndex));
    },
    testIndexHintedUnsorted: function () {
      const query = aql`
        FOR d IN ${col} OPTIONS {indexHint: "InvertedIndexUnsorted"}
          FILTER d.data_field == 'value1'
          RETURN d`;
      const res = AQL_EXPLAIN(query.query, query.bindVars);
      const appliedRules = res.plan.rules;
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      let executeRes = db._query(query.query, query.bindVars);
      assertEqual(docs / 100, executeRes.toArray().length);
    },
    testIndexGeo: function () {
      const query = aql`
        FOR d IN ${col} OPTIONS {indexHint: "InvertedIndexUnsorted"}
          FILTER GEO_INTERSECTS(d.geo_field, {type: 'Point', coordinates: [37.615895, 55.7039]})
          COLLECT WITH COUNT INTO c
          RETURN c`;
      const res = AQL_EXPLAIN(query.query, query.bindVars);
      const appliedRules = res.plan.rules;
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      let executeRes = db._query(query.query, query.bindVars);
      assertEqual(docs, executeRes.toArray()[0]);
    },
  };
}

jsunity.run(optimizerRuleInvertedIndexTestSuite);

return jsunity.done();
