/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual */
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
/// @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const findExecutionNodes = helper.findExecutionNodes;
const db = require('internal').db;

function optimizerRuleTestSuite() {
  const ruleName = "fulltextindex";
  const colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");

  const hasNoFilterNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, query + " Has no FilterNode");
  };
  const hasIndexNode = function (plan,query) {
    let rn = findExecutionNodes(plan,"IndexNode");
    assertEqual(rn.length, 1, query + " Has no IndexNode, but should have one");
    assertEqual(rn[0].indexes.length, 1);
    let indexType = rn[0].indexes[0].type;
    assertTrue(indexType === "fulltext", indexType + " wrong type");
  };

  let fulltext;

  return {
    setUp : function () {
      internal.db._drop(colName);
      fulltext = internal.db._create(colName);
      fulltext.ensureIndex({type:"fulltext", fields:["t1"]});
      fulltext.ensureIndex({type:"fulltext", fields:["t2"], minLength: 4});
      fulltext.ensureIndex({type:"fulltext", fields:["t3.e.x"]});
      const texts = [
        "Flötenkröten tröten böse wörter nörgelnd",
        "Krötenbrote grölen stoßen GROßE Römermöter",
        "Löwenschützer möchten mächtige Müller ködern",
        "Differenzenquotienten goutieren gourmante Querulanten, quasi quergestreift",
        "Warum winken wichtige Winzer weinenden Wichten watschelnd winterher?",
        "Warum möchten böse wichte wenig müßige müller meiern?",
        "Loewenschuetzer moechten maechtige Mueller koedern",
        "Moechten boese wichte wenig mueller melken?"
      ];
      for (let i = 0; i < texts.length; ++i) {
        fulltext.save({ id : (i + 1), t1 :texts[i], t2:texts[i], t3:{e:{x:texts[i]}}});
      }
    },

    tearDown : function () {
      internal.db._drop(colName);
      fulltext = null;
    },

    testRuleNotApplicable : function () {
      let query = "FOR searchValue IN ['prefix:möchten,müller', 'möchten,prefix:müller', 'möchten,müller', 'Flötenkröten,|goutieren', 'prefix:quergestreift,|koedern,|prefix:römer,-melken,-quasi'] RETURN (FOR d IN FULLTEXT(@@coll, @attr, searchValue) SORT d.id RETURN d.id)";

      ["t1", "t2", "t3.e.x"].forEach(field => {
        let bindVars = {'@coll': colName, attr: field};
        let plan = db._createStatement({query, bindVars}).explain();
        hasIndexNode(plan, query);

        let expected = [ [ 3, 6 ], [ 3, 6 ], [ 3, 6 ], [ 1, 4 ], [ 2, 7 ] ];
        let r = db._query(query, bindVars);
        assertEqual(r.toArray(), expected, "Invalid fulltext result");
      });
    },

    testRuleBasics : function () {
      fulltext.ensureIndex({ type: "hash", fields: [ "y", "z" ], unique: false });

      let checkQuery = function(q, r) {
        ["t1", "t2", "t3.e.x"].forEach(field => {
          let bindVars = {'@coll': colName, attr: field};

            let plan1 = db._createStatement({query: q, bindVars: bindVars}).explain();
            hasIndexNode(plan1,q);
            hasNoFilterNode(plan1,q);

            let plan2 = db._createStatement({query: q, bindVars: bindVars, options: {optimizer: {rules:[ "-all" ]}}}).explain();
            hasIndexNode(plan2,q);
            hasNoFilterNode(plan2,q);

            let r1 = db._query(q, bindVars, { optimizer: { rules: [ "-all" ] } });
            let r2 = db._query(q, bindVars);
            assertEqual(r1.toArray(), r, "Invalid fulltext result");
            assertEqual(r2.toArray(), r, "Invalid fulltext result");
        });
      };

      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'prefix:möchten,müller') SORT d.id RETURN d.id", [ 3, 6 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'möchten,prefix:müller') SORT d.id RETURN d.id", [ 3, 6 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'möchten,müller') SORT d.id RETURN d.id", [ 3, 6 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'Flötenkröten,|goutieren') SORT d.id RETURN d.id", [ 1, 4 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'prefix:quergestreift,|koedern,|prefix:römer,-melken,-quasi') SORT d.id RETURN d.id", [ 2, 7 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'prefix:quergestreift,|koedern,|prefix:römer,-melken') SORT d.id RETURN d.id", [ 2, 4, 7 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'prefix:quergestreift,|koedern,|prefix:römer,-melken', 2) SORT d.id RETURN d.id", [ 2, 4 ]);
      checkQuery("FOR d IN FULLTEXT(@@coll, @attr, 'prefix:quergestreift,|koedern,|prefix:römer,-melken', 2) LIMIT 1 SORT d.id RETURN d.id", [ 2]);
    }, // testRuleBasics

    testRuleStringCollection : function() {
      // collection is not known to query before optimizer rule is applied
      let q = "FOR d IN FULLTEXT('" + colName + "', 't1', 'möchten,müller') RETURN d.id";
      let plan = db._createStatement(q, {}).explain();
      hasIndexNode(plan,q);
      hasNoFilterNode(plan,q);

      let r = [ 3, 6 ];
      let r1 = db._query(q, {}, { optimizer: { rules: [ "-all" ] } });
      let r2 = db._query(q, {});
      assertEqual(r1.toArray(), r, "Invalid fulltext result");
      assertEqual(r2.toArray(), r, "Invalid fulltext result");
    }, // testRuleBasics

    testInvalidQuery : function () {
      let q = "FOR d IN FULLTEXT('" + colName + "', 't1', 'möchten,müller',3) RETURN 1";
      let plan = db._createStatement(q, {}).explain();
      hasIndexNode(plan,q);

      let r = [ 1, 1 ];
      let r1 = db._query(q, {}, { optimizer: { rules: [ "-all" ] } });
      let r2 = db._query(q, {});
      assertEqual(r1.toArray(), r, "Invalid fulltext result");
      assertEqual(r2.toArray(), r, "Invalid fulltext result");
    }

  };
} 

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
