/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertLess, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;

// This example was produced by Jan Steeman to reproduce a
// crash in the TraversalExecutor code
const productCollectionName = "RegressionProduct";
const customerCollectionName = "RegressionCustomer";
const materialCollectionName = "RegressionMaterial";
const ownsEdgeCollectionName = "RegressionOwns";
const containsEdgeCollectionName = "RegressionContains";

const expectedResult = [
  {
    name: "Adam",
    lastname: "Smith",
    number: 294,
  },
  {
    name: "Matt",
    lastname: "Smith",
    number: 34,
  }
];

var cleanup = function() {
  db._drop(productCollectionName);
  db._drop(customerCollectionName);
  db._drop(materialCollectionName);
  db._drop(ownsEdgeCollectionName);
  db._drop(containsEdgeCollectionName);
};

var createBaseGraph = function() {
  const customer = internal.db._createDocumentCollection(customerCollectionName);
  const product = internal.db._createDocumentCollection(productCollectionName);
  const material = internal.db._createDocumentCollection(materialCollectionName);
	
  const owns = internal.db._createEdgeCollection(ownsEdgeCollectionName);
  const contains = internal.db._createEdgeCollection(containsEdgeCollectionName);

  const john = customer.save({ name: "John", lastname: "Smith", number: 52 });
  const adam = customer.save({ name: "Adam", lastname: "Smith", number: 294} );
  const matt = customer.save({ name: "Matt", lastname: "Smith", number: 34 });
  const phone = product.save({ name: "phone" });
  const car = product.save({ name: "car"});
  const chair = product.save({ name: "chair"});
  const wood = material.save({name: "wood"});
  const metal = material.save({name: "metal"});
  const glass = material.save({name: "glass"});

  owns.save({_from: john._id, _to: phone._id});
  owns.save({_from: john._id, _to: car._id});
  owns.save({_from: adam._id, _to: chair._id});
  owns.save({_from: matt._id, _to: phone._id});
  owns.save({_from: matt._id, _to: car._id});
  owns.save({_from: matt._id, _to: chair._id});

  contains.save({_from: phone._id, _to: glass._id});
  contains.save({_from: car._id, _to: metal._id});
  contains.save({_from: car._id, _to: glass._id});
  contains.save({_from: chair._id, _to: wood._id});
};

function traversalResetRegression2Suite() {
  return {
    setUpAll: function() {
      cleanup();
      createBaseGraph();
    },

    tearDownAll: function() {
      cleanup();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test object access for path object
    ////////////////////////////////////////////////////////////////////////////////
    testTraversalResetCrashes: function() {
      const query = `
	      WITH @@customer, @@owns, @@contains, @@product, @@material
              FOR e IN @@customer
                 FILTER (FOR e1 IN 1..1 ANY e._id @@owns
                   FILTER (FOR e2 IN 1..1 ANY e1._id @@contains
                     FILTER e2.name == "wood" RETURN 1)[0] == 1 RETURN 1)[0] == 1
                 RETURN {name: e.name, lastname: e.lastname, number: e.number}`;
      const bindVars = { "@customer": customerCollectionName,
	                 "@owns": ownsEdgeCollectionName,
	                 "@contains": containsEdgeCollectionName,
                         "@product": productCollectionName,
                         "@material": materialCollectionName };

      let actual = db._query(query, bindVars);
      assertEqual(actual.toArray().sort(), expectedResult.sort());
    }
  };
}

// Regression test suite for https://github.com/arangodb/arangodb/issues/13099
function subqueryFetchTooMuchRegressionSuite() {
  const count = {fullCount: true};
  return {
    setUpAll: function() {},
    tearDownAll: function() {},

    testSubqueryEndHitShadowRowFirst: function() {
      // This tests needs to fill an AQL itemBlock, s.t. one shadowRow
      // for the subqueryEnd is the start of the next AQLItemBlock
      // This will cause the upstream on subquery to be "DONE" but
      // Bot not on the main query.
      const query = `
        FOR x IN 1..10000
          LET sub = (
            FOR y IN 1..10
              /* We need a condition on X, but want the optimizer to not know it*/
              LET add = x + 10000 * y
              /* mod == x */
              LET mod = add % 10000
              /* on the 100 x we want to have 10 y values*/
              FILTER mod != 1
              RETURN 1
          )
          FILTER LENGTH(sub) > 0
          LIMIT 20
          RETURN 1
      `;
      {
        // Data
        const q = db._query(query);
        const res = q.toArray();
        assertEqual([ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ], res);
      }
      {
        // FullCount
        const stat = db._query(query, {}, count).getExtra().stats;
        assertEqual(9999, stat.fullCount);
        assertEqual(11, stat.filtered);
      }
    },

    testSubqueryEndHitExactEndOfInput: function() {
      // This tests needs to fill an AQL itemBlock, s.t. one shadowRow
      // for the subqueryEnd is the start of the next AQLItemBlock
      // This will cause the upstream on subquery to be "DONE" but
      // Bot not on the main query.
      const query = `
        FOR x IN 1..10000
          LET sub = (
            FOR y IN 1..10
              /* We need a condition on X, but want the optimizer to not know it*/
              LET add = x + 10000 * y
              /* mod == x */
              LET mod = add % 10000
              /* on the 100 x we want to have 10 y values*/
              FILTER y != 10 || mod == 100
              RETURN 1
          )
          FILTER LENGTH(sub) > 0
          LIMIT 20
          RETURN 1
      `;
      {
        // Data
        const q = db._query(query);
        const res = q.toArray();
        assertEqual([ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ], res);
      }
      {
        // FullCount
        const stat = db._query(query, {}, count).getExtra().stats;
        assertEqual(10000, stat.fullCount);
        assertEqual(9999, stat.filtered);
      }
    }
  };
}

jsunity.run(traversalResetRegression2Suite);
jsunity.run(subqueryFetchTooMuchRegressionSuite);

return jsunity.done();
