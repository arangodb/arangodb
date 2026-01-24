/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;

function attributeDetectorIndexStoredValuesTestSuite() {
    const cn = "UnitTestsAttributeDetectorIndex";
    const cnStored = "UnitTestsAttributeDetectorIndexStored";
    let collection;
    let storedCollection;

    const explainAbac = function (query, bindVars, options) {
        const explainRes = db._createStatement({
            query: query,
            bindVars: bindVars || {},
            options: options || {}
        }).explain();
        print(JSON.stringify(explainRes, null, 2))
        return explainRes.abacAccesses || [];
    };

    const containsReadAttr = function (access, attrPath) {
        const expected = Array.isArray(attrPath) ? attrPath : [attrPath];
        const attrs = (access && access.read && access.read.attributes) || [];
        return attrs.some(a =>
            Array.isArray(a) &&
            a.length === expected.length &&
            a.every((seg, i) => seg === expected[i])
        );
    }

    return {
        setUp: function () {
            internal.db._drop(cn);
            collection = internal.db._create(cn);

            for (let i = 0; i < 10; ++i) {
                collection.insert({
                    _key: `doc${i}`,
                    value: i,
                    name: `name${i}`,
                    category: `cat${i % 3}`,
                    data: {nested: i, extra: `extra${i}`}
                });
            }

            collection.ensureIndex({
                type: "persistent",
                fields: ["value", "category"]
            });

            internal.db._drop(cnStored);
            storedCollection = internal.db._create(cnStored);

            for (let i = 0; i < 10; ++i) {
                storedCollection.insert({
                    _key: `sdoc${i}`,
                    value: i,
                    category: `cat${i % 2}`,
                    name: `storedName${i}`,
                    price: i * 10,
                    payload: {foo: i}
                });
            }

            storedCollection.ensureIndex({
                type: "persistent",
                fields: ["value", "category"],
                storedValues: ["name", "price"]
            });
        },

        tearDown: function () {
            internal.db._drop(cn);
            internal.db._drop(cnStored);
        },

        testAbac_IndexSimpleProjection: function () {
            const query = `
        FOR doc IN ${cn}
          FILTER doc.category == "cat1" OR doc.value > 5
          RETURN doc.value
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexMultipleAttributes: function () {
            const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN {name: doc.name, category: doc.category}
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        // Using _id/_key explicitly is still attribute access.
        testAbac_IndexReturnIdAndKey: function () {
            const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN {id: doc._id, key: doc._key}
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "_id"));
            assertTrue(containsReadAttr(accesses[0], "_key"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexFullDocumentAccess: function () {
            const query = `
        FOR doc IN ${cn}
          RETURN doc
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexFilterOnAttributeReturnFullDocument: function () {
            const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN doc
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexFilterWithComputation: function () {
            const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5 OR doc.value > 7
          RETURN doc.value
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexFilterAndReturnDifferentAttributes1: function () {
            const query = `
        FOR doc IN ${cn}
          FILTER doc.value IN [1, 5, 7]
          RETURN doc.category
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexFilterAndReturnDifferentAttributes2_SelfJoin: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          FOR q IN ${cn}
            FILTER q.value == p.value
            RETURN q.name
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexFilterAndReturnDifferentAttributes3_SelfJoinReturnDoc: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          FOR q IN ${cn}
            FILTER q.value == p.value
            RETURN q
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexCalculationWithAttributeAccess1: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          LET v2 = p.value * 2
          RETURN { name: p.name, v2 }
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexSortWithAttributes: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          SORT p.value DESC
          RETURN p.name
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexLimitDoesNotChangeProjection: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          LIMIT 2
          RETURN p.name
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexCollectByAttribute: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          COLLECT c = p.category
          RETURN c
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexCollectAggregateUsesAttribute: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          COLLECT AGGREGATE maxV = MAX(p.value)
          RETURN maxV
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_IndexReturnDistinctAttribute: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          RETURN DISTINCT p.category
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_DynamicCollectionAccessWithBindParameters1: function () {
            const query = `
        FOR p IN @@coll
          FILTER p.value == 5
          RETURN p.name
      `;
            const accesses = explainAbac(query, {"@coll": cn});

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_DynamicAttributeAccessWithBindParameters1: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          RETURN p[@attr]
      `;
            const accesses = explainAbac(query, {attr: "name"});

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_DynamicAttributeAccessWithBindParameters2: function () {
            const query = `
        LET a = "category"
        FOR p IN ${cn}
          FILTER p.value == 5
          RETURN p[a]
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_DynamicAttributeAccessWithBindParameters3: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p[@attr] >= 5
          RETURN { name: p.name, category: p.category }
      `;
            const accesses = explainAbac(query, {attr: "value"});

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_MissingCollectionThrowsWithCorrectErrorCode: function () {
            // JS version: explain should throw TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
            let threw = false;
            try {
                explainAbac(`
          FOR p IN UnitTestsAttributeDetectorIndex_Missing
            FILTER p.value == 5
            RETURN p.name
        `);
            } catch (err) {
                threw = true;
                // Error shape differs depending on shell; be permissive but still check code.
                assertTrue(err.errorNum !== undefined);
                assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
            }
            assertTrue(threw);
        },

        testAbac_MissingAttributeStillRecorded1: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          RETURN p.attrNotExist
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "attrNotExist"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_MissingAttributeStillRecorded2: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5 AND p.attrNotExist == 1
          RETURN p.name
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "attrNotExist"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_MissingAttributeStillRecorded3_ReturnDoc: function () {
            const query = `
        FOR p IN ${cn}
          FILTER p.value == 5 AND p.attrNotExist == 1
          RETURN p
      `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_NestedAttribute_NoStoredValues_ForcesDocumentFetch: function () {
            const query = `
    FOR doc IN ${cn}
      FILTER doc.value == 5
      RETURN doc.data.nested
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cn, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], ["data", "nested"]));
            assertFalse(containsReadAttr(accesses[0], "nested"));
            assertFalse(containsReadAttr(accesses[0], "data"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        ///* StoredValues tests *///

        testAbac_Stored_IndexSimpleProjection: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 OR doc.category == "cat1"
      RETURN doc.category
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexMultipleAttributes: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 AND doc.category == "cat1"
      RETURN {name: doc.name, price: doc.price}
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "category"));
            assertTrue(containsReadAttr(accesses[0], "price"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        // Using _id/_key explicitly is still attribute access.
        testAbac_Stored_IndexReturnIdAndKey: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 AND doc.category == "cat1"
      RETURN {id: doc._id, key: doc._key}
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));
            assertTrue(containsReadAttr(accesses[0], "_id"));
            assertTrue(containsReadAttr(accesses[0], "_key"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexFullDocumentAccess: function () {
            const query = `
    FOR doc IN ${cnStored}
      RETURN doc
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexFilterOnAttributeReturnFullDocument: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 AND doc.category == "cat1"
      RETURN doc
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexFilterWithComputation: function () {
            // ###### Index: Condition: adding: value
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 OR doc.value > 7
      RETURN doc.price
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "price"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexFilterAndReturnDifferentAttributes1: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value IN [1, 5, 7]
      RETURN doc.name
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexFilterAndReturnDifferentAttributes2_SelfJoin: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value == 5 AND p.category == "cat1"
      FOR q IN ${cnStored}
        FILTER q.value == p.value AND q.category == p.category
        RETURN q.name
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexFilterAndReturnDifferentAttributes3_SelfJoinReturnDoc: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value == 5 AND p.category == "cat1"
      FOR q IN ${cnStored}
        FILTER q.value == p.value AND q.category == p.category
        RETURN q
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexCalculationWithAttributeAccess1: function () {
            // ###### Index: Condition: adding: value
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value == 5 AND p.category == "cat1"
      LET v2 = p.value * 2
      RETURN { name: p.name, v2 }
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexSortWithAttributes: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value IN [1, 5, 7]
      SORT p.value DESC
      RETURN p.name
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexLimitDoesNotChangeProjection: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value IN [1, 5, 7]
      LIMIT 2
      RETURN p.name
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexCollectByAttribute: function () {
            // ###### Index: Condition: adding: value
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value IN [1, 5, 7]
      COLLECT c = p.category
      RETURN c
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexCollectAggregateUsesAttribute: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value IN [1, 5, 7]
      COLLECT AGGREGATE maxV = MAX(p.value)
      RETURN maxV
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_IndexReturnDistinctAttribute: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value IN [1, 5, 7]
      RETURN DISTINCT p.category
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_DynamicCollectionAccessWithBindParameters1: function () {
            //###### Index: Condition: adding: category
            //###### Index: Condition: adding: value
            const query = `
    FOR p IN @@coll
      FILTER p.value == 5 AND p.category == "cat1"
      RETURN p.name
  `;
            const accesses = explainAbac(query, {"@coll": cnStored});

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_DynamicAttributeAccessWithBindParameters1: function () {
            const query = `
    FOR p IN ${cnStored}
      FILTER p.value == 5 AND p.category == "cat1"
      RETURN p[@attr]
  `;
            const accesses = explainAbac(query, {attr: "name"});

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_DynamicAttributeAccessWithBindParameters2: function () {
            const query = `
    LET a = "price"
    FOR p IN ${cnStored}
      FILTER p.value == 5 AND p.category == "cat1"
      RETURN p[a]
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "price"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_DynamicAttributeAccessWithBindParameters3: function () {
            // ###### Index: Condition: adding: value
            const query = `
    FOR p IN ${cnStored}
      FILTER p[@attr] >= 5
      RETURN { name: p.name, price: p.price }
  `;
            const accesses = explainAbac(query, {attr: "value"});

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "name"));
            assertTrue(containsReadAttr(accesses[0], "price"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Stored_NonStoredAttributeForcesDocumentFetch: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 AND doc.category == "cat1"
      RETURN doc.payload
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "payload"));
            assertTrue(containsReadAttr(accesses[0], "category"));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_NestedAttribute_WithStoredValues_CoveredByIndex: function () {
            const query = `
    FOR doc IN ${cnStored}
      FILTER doc.value == 5 AND doc.category == "cat1"
      RETURN doc.payload.foo
  `;
            const accesses = explainAbac(query);

            assertEqual(1, accesses.length);
            assertEqual(cnStored, accesses[0].collection);

            assertTrue(containsReadAttr(accesses[0], "value"));
            assertTrue(containsReadAttr(accesses[0], "category"));
            assertTrue(containsReadAttr(accesses[0], ["payload", "foo"]));
            assertFalse(containsReadAttr(accesses[0], ["payload"]));
            assertFalse(containsReadAttr(accesses[0], ["foo"]));

            assertFalse(accesses[0].read.requiresAll);
            assertFalse(accesses[0].write.requiresAll);
        },

        testAbac_Subquery_ReadsBothCollections: function () {
            const query = `
    FOR d IN ${cn}
      FILTER d.value == 5
      LET names = (
        FOR s IN ${cnStored}
          FILTER s.value == d.value
          RETURN s.name
      )
      RETURN { outerName: d.name, innerNames: names }
  `;
            const accesses = explainAbac(query);

            assertEqual(2, accesses.length);

            let outer = null;
            let inner = null;
            for (let i = 0; i < accesses.length; ++i) {
                if (accesses[i].collection === cn) {
                    outer = accesses[i];
                } else if (accesses[i].collection === cnStored) {
                    inner = accesses[i];
                }
            }

            assertTrue(outer !== null);
            assertTrue(inner !== null);

            assertTrue(containsReadAttr(outer, "value"));
            assertTrue(containsReadAttr(outer, "name"));
            assertFalse(outer.read.requiresAll);
            assertFalse(outer.write.requiresAll);

            assertTrue(containsReadAttr(inner, "value"));
            assertTrue(containsReadAttr(inner, "name"));
            assertFalse(inner.read.requiresAll);
            assertFalse(inner.write.requiresAll);
        },

        testAbac_Subquery_NonStoredAttributeForcesDocumentFetch: function () {
            const query = `
    FOR d IN ${cn}
      FILTER d.value == 5
      LET payloads = (
        FOR s IN ${cnStored}
          FILTER s.value == d.value AND s.category == "cat1"
          RETURN s.payload
      )
      RETURN { outerName: d.name, innerPayloads: payloads }
  `;
            const accesses = explainAbac(query);

            assertEqual(2, accesses.length);

            let outer = null;
            let inner = null;
            for (let i = 0; i < accesses.length; ++i) {
                if (accesses[i].collection === cn) {
                    outer = accesses[i];
                } else if (accesses[i].collection === cnStored) {
                    inner = accesses[i];
                }
            }

            assertTrue(outer !== null);
            assertTrue(inner !== null);

            assertTrue(containsReadAttr(outer, "value"));
            assertTrue(containsReadAttr(outer, "name"));
            assertFalse(outer.read.requiresAll);
            assertFalse(outer.write.requiresAll);

            assertTrue(containsReadAttr(inner, "value"));
            assertTrue(containsReadAttr(inner, "category"));
            assertTrue(containsReadAttr(inner, "payload"));

            assertFalse(inner.read.requiresAll);
            assertFalse(inner.write.requiresAll);
        },
    };
}

jsunity.run(attributeDetectorIndexStoredValuesTestSuite);

return jsunity.done();