/*jshint globalstrict:false, strict:false, maxlen: 50000 */
/*global assertTrue, assertFalse, assertEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");

function vectorIndexCreateSuite () {
  'use strict';
  var cn = "UnitTestsFulltext";
  var c = null;

  return {
    setUp : function () {
      internal.db._drop(cn);
      c = internal.db._create(cn);
    },
    tearDown : function () {
      internal.db._drop(cn);
    },

    testVectorIndexTest : function () {
      const vectorIndexFields = ["vec"];
      const idx = c.ensureIndex({ 
        type: "vector",
        fields: vectorIndexFields,
        params: { dimensions: 5, min: 0, max: 10, Kparameter: 2, Lparameter: 1, randomFunctions: [ { bParam: 3, wParam: 4, vParam: [0,1,2,3,4] }, { bParam: 1, wParam: 2, vParam: [0,1,2,3,4] } ]} });
      const indexes = c.getIndexes();
      assertEqual(2, indexes.length);

      const vectorIndexes = indexes.filter(function(n) { return n.type == "vector" }); 
      assertEqual(1, vectorIndexes.length);

      const vectorIndex = vectorIndexes[0];
      assertEqual(idx.id, vectorIndex.id);
      assertEqual(vectorIndexFields, vectorIndex.fields);
    },
  };
}

jsunity.run(vectorIndexCreateSuite);

return jsunity.done();

