/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail */

const internal = require("internal");
const errors = internal.errors;
const jsunity = require("jsunity");
const db = require("@arangodb").db;


function resourceMonitorMergeCollectTestSuite () {
    'use strict';

    return {

        testMergeMemoryLimit : function () {
            const query = `
        LET arr = (FOR i IN 1..200 RETURN { ['key' || TO_STRING(i)]: i, value: REPEAT('A', 1024) })
        RETURN MERGE(arr)
      `;
            // a lot of memory to make it work
            const resOk = db._query(query, null, { memoryLimit: 10 * 1000 * 1000 }).toArray();
            assertEqual(1, resOk.length);

            // less memory to make it crash
            try {
                db._query(query, null, { memoryLimit: 40 * 1000 });
                fail();
            } catch (err) {
                assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
            }
        },

        testCollectMemoryLimit : function () {
            const query = `
        FOR i IN 1..5000
        COLLECT remainders = i % 50 INTO arr
        RETURN { remainders, values: arr }
      `;
            // enough for all
            let actual = db._query(query, null, { memoryLimit: 20 * 1000 * 1000 }).toArray();
            assertEqual(50, actual.length);
            //enough for the input but not for intermediate usage
            try {
                db._query(query, null, { memoryLimit: 60 * 1000 });
                fail();
            } catch (err) {
                assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
            }
        },

        testMergeMemoryLimitInsideLet: function () {
            const query = `
        LET iterations = (FOR i IN 1..200 RETURN { ['key' || TO_STRING(i)]: i, value: REPEAT('A', 1024) })
        LET merged = MERGE(iterations)
        RETURN merged
      `;
            const resOk = db._query(query, null, { memoryLimit: 10 * 1000 * 1000 }).toArray();
            assertEqual(1, resOk.length);

            try {
                db._query(query, null, { memoryLimit: 60 * 1000 });
                fail();
            } catch (err) {
                assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
            }
        },

        testMergeRecursiveMemoryLimitInsideLet: function () {
            const query = `
        LET iterations = (FOR i IN 1..150 RETURN { ['a' || TO_STRING(i)]: { val: REPEAT('B', 1024) } })
        LET merged = MERGE_RECURSIVE(iterations)
        RETURN merged
      `;

            const resOk = db._query(query, null, { memoryLimit: 15 * 1000 * 1000 }).toArray();
            assertEqual(1, resOk.length);

            try {
                db._query(query, null, { memoryLimit: 80 * 1000 });
                fail();
            } catch (err) {
                assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
            }
        },

    };

}


jsunity.run(resourceMonitorMergeCollectTestSuite);
return jsunity.done();

