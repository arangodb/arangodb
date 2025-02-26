/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, assertEqual, assertFalse, assertTrue, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const internal = require('internal');
const cache = require("@arangodb/aql/cache");
const errors = internal.errors;
const planCache = require("@arangodb/aql/plan-cache");
const aql = arangodb.aql;

if (getOptions === true) {
    return {
        'query.collection-logger-enabled': 'true',
        'query.collection-logger-include-system-database': 'true',
        'query.collection-logger-all-slow-queries': 'true',
        'query.collection-logger-probability': '100',
        'query.collection-logger-push-interval': '250',
    };
}

function QueryLoggerWithPlanCacheSuite() {
    let collection;
    const dbName = "queryLoggingAndCachingDB";
    const colName = "col";

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        setUp: function() {
            db._useDatabase(dbName);
            collection = db._create(colName, {numberOfShards: 3});
            let docs = [];
            for (let i = 0; i < 10; ++i) {
                docs.push({
                    val: i
                });
            }
            collection.save(docs);

            planCache.clear();
        },

        tearDown: function() {
            db._drop(colName);
        },

        testPlanCachingAndLogging: function() {
            let query = aql`FOR d IN ${collection} FILTER d.val > 5 RETURN d`;
            query.options = {
                optimizePlanForCaching: true,
                usePlanCache: true,
                cache: false
            };

            let res = db._query(query);
            assertFalse(res.hasOwnProperty("planCacheKey"));

            res = db._query(query);
            assertTrue(res.hasOwnProperty("planCacheKey"));
        },

        testPlanCachingAndLoggingWithModification: function() {
            let query = aql`FOR i IN 10..20 INSERT {val: i} INTO ${collection}`;
            query.options = {
                optimizePlanForCaching: true,
                usePlanCache: true,
                cache: false
            };

            let res = db._query(query);
            assertFalse(res.hasOwnProperty("planCacheKey"));

            res = db._query(query);
            assertTrue(res.hasOwnProperty("planCacheKey"));
        },

        testPlanCachingAndLoggingWithExplain: function() {
            let query = aql`FOR d IN ${collection} FILTER d.val > 50 LIMIT 1 RETURN d`;
            query.options = {
                optimizePlanForCaching: true,
                usePlanCache: true,
                cache: false
            };

            let stmt = db._createStatement(query);
            let res = stmt.explain();
            assertFalse(res.hasOwnProperty("planCacheKey"));
            assertNotUndefined(res.plan, res);

            stmt = db._createStatement(query);
            res = stmt.explain();
            assertTrue(res.hasOwnProperty("planCacheKey"));
            assertNotUndefined(res.plan, res);
        },
    };
}

jsunity.run(QueryLoggerWithPlanCacheSuite);

return jsunity.done();
