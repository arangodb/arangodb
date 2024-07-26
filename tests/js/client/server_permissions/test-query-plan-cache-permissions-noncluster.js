/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global getOptions, arango, assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined, assertNotUndefined  */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
  };
}

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const users = require("@arangodb/users");
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;

const cn1 = "UnitTestsQueryPlanCache1";
const cn2 = "UnitTestsQueryPlanCache2";

const assertNotCached = (query, bindVars = {}, options = {}) => {
  options.optimizePlanForCaching = true;
  // execute query once
  let res = db._query(query, bindVars, options);
  assertFalse(res.hasOwnProperty("planCacheKey"));
  
  // execute query again
  res = db._query(query, bindVars, options);
  // should still not have the planCacheKey attribute
  assertFalse(res.hasOwnProperty("planCacheKey"));
};

const assertCached = (query, bindVars = {}, options = {}) => {
  options.optimizePlanForCaching = true;
  // execute query once
  let res = db._query(query, bindVars, options);
  assertFalse(res.hasOwnProperty("planCacheKey"));
  
  // execute query again
  res = db._query(query, bindVars, options);
  // should now have the planCacheKey attribute
  assertTrue(res.hasOwnProperty("planCacheKey"));
};

let id = 1;
const uniqid = () => {
  return `/* query-${id++} */ `;
};

function QueryPlanCacheTestSuite () {
  let c1, c2;

  return {
    setUp : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);
    },

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },
    
    testPermissionsChange : function () {
      const endpoint = arango.getEndpoint();
      users.save("test_user", "testi");

      try {
        users.grantDatabase("test_user", "_system", "rw");
        users.grantCollection("test_user", "_system", cn1, "rw");
        users.grantCollection("test_user", "_system", cn2, "rw");

        arango.reconnect(endpoint, db._name(), "test_user", "testi");

        // read queries
        const query1 = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == 123 RETURN doc.value`;
        const query2 = `${uniqid()} FOR doc IN ${cn2} FILTER doc.value == 123 RETURN doc.value`;
       
        // write queries
        const query3 = `${uniqid()} FOR i IN 1..10 INSERT {} INTO ${cn1}`;
        const query4 = `${uniqid()} FOR i IN 1..10 INSERT {} INTO ${cn2}`;
        
        assertCached(query1);
        assertCached(query2);
        assertCached(query3);
        assertCached(query4);
        
        // revoke write permissions
        users.grantCollection("test_user", "_system", cn1, "ro");
        users.grantCollection("test_user", "_system", cn2, "ro");

        // plans should still be cached for queries 1 and 2
        const options = { optimizePlanForCaching: true };
        let res = db._query(query1, {}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));
        
        res = db._query(query2, {}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));

        // queries 3 and 4 must fail
        try {
          db._query(query3, {}, options);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_READ_ONLY.code, err.errorNum);
        }
        
        try {
          db._query(query4, {}, options);
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_READ_ONLY.code, err.errorNum);
        }
        
        // revoke read permissions
        users.grantCollection("test_user", "_system", cn1, "none");
        try {
          db._query(query1, {}, options);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_FORBIDDEN.code, err.errorNum);
        }

        // restore all permissions
        users.grantCollection("test_user", "_system", cn1, "rw");
        users.grantCollection("test_user", "_system", cn2, "rw");

        res = db._query(query1, {}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));
        
        res = db._query(query2, {}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));
        
        res = db._query(query3, {}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));
        
        res = db._query(query4, {}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));
      } finally {
        arango.reconnect(endpoint, db._name(), "root", "");
        users.remove("test_user");
      }
    },
    
  };
}

jsunity.run(QueryPlanCacheTestSuite);
return jsunity.done();
