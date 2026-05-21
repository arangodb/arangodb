/*global assertEqual, assertTrue, fail */

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
const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;
let { versionHas } = require('@arangodb/test-helper');
const isInstr = versionHas('asan') || versionHas('tsan') || versionHas('coverage');

const colNameA = "UnitTestVertexCollectionA";
const colNameB = "UnitTestEdgeCollectionB";
const colNameC = "UnitTestVertexCollectionC";

const tearDownAll = () => {
  db._drop(colNameA);
  db._drop(colNameB);
  db._drop(colNameC);
};
const createCollections = () => {
  db._createDocumentCollection(colNameA);
  db._createDocumentCollection(colNameC);
  db._createEdgeCollection(colNameB);

  var averts = [];
  var cverts = [];
  var bedges = [];

  for(let i=1;i<=4000;i++) {
    averts.push({ _key: `${i}` });
    cverts.push({ _key: `${i}` });
    bedges.push({ _from: `${colNameA}/${i}`, _to: `${colNameC}/${i}` });
  }

  db._collection(colNameA).insert(averts);
  db._collection(colNameC).insert(cverts);
  db._collection(colNameB).insert(bedges);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function asyncPrefetchReuseTestSuite() {
  return {
    setUpAll: function() {
      tearDownAll();
      createCollections();
    },
    tearDownAll,

    // This is a regression test for issue
    // https://github.com/arangodb/arangodb/issues/22158, i.e. that
    // when using async-prefetch, the BuilderLeaser and StringLeaser
    // members of the transaction context were unsafely write-accessed
    // concurrently.
    // In case of a regression, this test will either fail with an
    // obscure AQL error, or tickle the thread sanitizer.
    testTickleReuse: function () {
      const q = `WITH ${colNameA}, ${colNameB}, ${colNameC}
        FOR node IN @@col
        LET x = (
          FOR n,e,p IN 1..1 ANY node._id @@edge_col
            FILTER !IS_NULL(e) && !IS_NULL(e._from) && !IS_NULL(e._to)
            FILTER PARSE_IDENTIFIER(e._from).collection in @_collections
          RETURN n._id)
        FILTER LENGTH(x) > 0
        FOR n, ee, ppp IN 1..1 ANY node._id @@edge_col
          FILTER PARSE_IDENTIFIER(ee._from).collection IN @_collections && PARSE_IDENTIFIER(ee._to).collection IN @_collections
          RETURN ee`;

      const binds =  {
        "@col": colNameA,
        "@edge_col": colNameB,
        "_collections": [ colNameA, colNameB, colNameC ]
      };

      // Running the above query in a loop previously lead to random errors and crashes
      // due to the reuse of the leased Builders and Strings in the transaction
      // context. This test should tickle TSAN if such a problem persists.
      let num_runs = isInstr ? 10 : 50;
      for (let i = 1; i < num_runs; i++) {
        db._query(q, binds);
      }
    },
  };
}

jsunity.run(asyncPrefetchReuseTestSuite);
return jsunity.done();
