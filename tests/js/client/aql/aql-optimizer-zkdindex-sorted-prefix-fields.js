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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");

const useIndexes = 'use-indexes';
const removeFilterCoveredByIndex = "remove-filter-covered-by-index";
const moveFiltersIntoEnumerate = "move-filters-into-enumerate";

function optimizerRuleZkd2dIndexTestSuite() {
  const colName = 'UnitTestZkdIndexCollection';
  let col;

  return {
    setUpAll: function () {
      col = db._create(colName);
      col.ensureIndex({type: 'zkd', name: 'zkdIndex', fields: ['x', 'y'], fieldValueTypes: 'double', storedValues: ["z"], sortedPrefixValues: ["stringValue"]});

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str} INTO ${col}
      `);
    },

    tearDownAll: function () {
      col.drop();
    },


    testSimplePrefix: function () {

      const res = db._query(aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo"
          return [doc.z, doc.stringValue]
      `);

      const result = res.toArray();
      assertEqual(result.length, 3);
      assertEqual(_.uniq(result.map(([a, b]) => b)), ["foo"]);
      assertEqual(result.map(([a, b]) => a).sort(), [5, 6, 7]);
    }

  };
}

jsunity.run(optimizerRuleZkd2dIndexTestSuite);

return jsunity.done();
