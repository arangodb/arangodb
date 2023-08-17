/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {

  var ruleName = "fulltextindex";
  var colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");

  var sortArray = function (l, r) {
    if (l[0] !== r[0]) {
      return l[0] < r[0] ? -1 : 1;
    }
    if (l[1] !== r[1]) {
      return l[1] < r[1] ? -1 : 1;
    }
    return 0;
  };
  var hasNoFilterNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, query + " Has no FilterNode");
  };
  var hasNoIndexNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "IndexNode").length, 0, query + " Has IndexNode, but should not have one");
  };
  var hasIndexNode = function (plan,query) {
    var rn = findExecutionNodes(plan,"IndexNode");
    assertEqual(rn.length, 1, query + " Has no IndexNode, but should have one");
    assertEqual(rn[0].indexes.length, 1);
    var indexType = rn[0].indexes[0].type;
    assertTrue(indexType === "fulltext", indexType + " wrong type");
  };
  var isNodeType = function(node, type, query) {
    assertEqual(node.type, type, query.string + " check whether this node is of type " + type);
  };

  let fulltext;
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var loopto = 10;

      internal.db._drop(colName);
      fulltext = internal.db._create(colName);
      fulltext.ensureIndex({type:"fulltext", fields:["t1"]});
      fulltext.ensureIndex({type:"fulltext", fields:["t2"], minLength: 4});
      fulltext.ensureIndex({type:"fulltext", fields:["t3.e.x"]});
      var texts = [
        "Flötenkröten tröten böse wörter nörgelnd",
        "Krötenbrote grölen stoßen GROßE Römermöter",
        "Löwenschützer möchten mächtige Müller ködern",
        "Differenzenquotienten goutieren gourmante Querulanten, quasi quergestreift",
        "Warum winken wichtige Winzer weinenden Wichten watschelnd winterher?",
        "Warum möchten böse wichte wenig müßige müller meiern?",
        "Loewenschuetzer moechten maechtige Mueller koedern",
        "Moechten boese wichte wenig mueller melken?"
      ];
      for (var i = 0; i < texts.length; ++i) {
        fulltext.save({ id : (i + 1), t1 :texts[i], t2:texts[i], t3:{e:{x:texts[i]}}});
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(colName);
      fulltext = null;
    },

    testRuleNotApplicable : function () {
      let query = "FOR searchValue IN ['prefix:möchten,müller', 'möchten,prefix:müller', 'möchten,müller', 'Flötenkröten,|goutieren', 'prefix:quergestreift,|koedern,|prefix:römer,-melken,-quasi'] RETURN (FOR d IN FULLTEXT(@@coll, @attr, searchValue) SORT d.id RETURN d.id)";

      ["t1", "t2", "t3.e.x"].forEach(field => {
        let bindVars = {'@coll': colName, attr: field};
        let plan = AQL_EXPLAIN(query, bindVars);
        hasIndexNode(plan, query);

        let expected = [ [ 3, 6 ], [ 3, 6 ], [ 3, 6 ], [ 1, 4 ], [ 2, 7 ] ];
        let r = AQL_EXECUTE(query, bindVars);
        assertEqual(r.json, expected, "Invalid fulltext result");
      });
    },

    testRuleBasics : function () {
      fulltext.ensureIndex({ type: "hash", fields: [ "y", "z" ], unique: false });

      let checkQuery = function(q, r) {
        ["t1", "t2", "t3.e.x"].forEach(field => {
          let bindVars = {'@coll': colName, attr: field};

            let plan1 = AQL_EXPLAIN(q, bindVars);
            hasIndexNode(plan1,q);
            hasNoFilterNode(plan1,q);

            let plan2 = AQL_EXPLAIN(q, bindVars, {optimizer: {rules:[ "-all" ]}});
            hasIndexNode(plan2,q);
            hasNoFilterNode(plan2,q);

            let r1 = AQL_EXECUTE(q, bindVars, { optimizer: { rules: [ "-all" ] } });
            let r2 = AQL_EXECUTE(q, bindVars);
            assertEqual(r1.json, r, "Invalid fulltext result");
            assertEqual(r2.json, r, "Invalid fulltext result");
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
      let plan = AQL_EXPLAIN(q, {});
      hasIndexNode(plan,q);
      hasNoFilterNode(plan,q);

      let r = [ 3, 6 ];
      let r1 = AQL_EXECUTE(q, {}, { optimizer: { rules: [ "-all" ] } });
      let r2 = AQL_EXECUTE(q, {});
      assertEqual(r1.json, r, "Invalid fulltext result");
      assertEqual(r2.json, r, "Invalid fulltext result");
    }, // testRuleBasics

    testInvalidQuery : function () {
      let q = "FOR d IN FULLTEXT('" + colName + "', 't1', 'möchten,müller',3) RETURN 1";
      let plan = AQL_EXPLAIN(q, {});
      hasIndexNode(plan,q);

      let r = [ 1, 1 ];
      let r1 = AQL_EXECUTE(q, {}, { optimizer: { rules: [ "-all" ] } });
      let r2 = AQL_EXECUTE(q, {});
      assertEqual(r1.json, r, "Invalid fulltext result");
      assertEqual(r2.json, r, "Invalid fulltext result");
    }

  }; // test dictionary (return)
} // optimizerRuleTestSuite

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
