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

function QueryLoggerWithCacheSuite() {
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
            collection = db._create(colName);
            let docs = [];
            for (let i = 0; i < 10; ++i) {
                docs.push({
                    val: i
                });
            }
            collection.save(docs);

            let result = cache.properties({
                mode: "on"
            });
            assertEqual("on", result.mode);
        },

        tearDown: function() {
            db._drop(colName);
            db._flushCache();
        },

        testCachingAndLogging: function() {
            const query = aql`FOR d IN ${collection} FILTER d.val > 50 LIMIT 1 RETURN d`;

            let res = db._query(query);
            assertFalse(res._cached);

            res = db._query(query);
            assertTrue(res._cached);
        },

        testCachingAndLoggingWithModification: function() {
            const query = aql`FOR i IN 1..10 INSERT {val: i * 2} INTO ${collection}`;

            let res = db._query(query);
            assertFalse(res._cached);

            res = db._query(query);
            assertFalse(res._cached);
        },
    };
}

jsunity.run(QueryLoggerWithCacheSuite);

return jsunity.done();
