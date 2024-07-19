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

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;

function vectorIndexCreateSuite() {
  "use strict";
  var cn = "UnitTestsFulltext";
  var c = null;

  return {
    setUp: function () {
      internal.db._drop(cn);
      c = internal.db._create(cn);
    },
    tearDown: function () {
      internal.db._drop(cn);
    },

    testVectorIndexTest: function () {
      const idx = c.ensureIndex({
        type: "vector",
        fields: ["vec"],
        params: {
          dimensions: 5,
          min: 0,
          max: 10,
          Kparameter: 2,
          Lparameter: 1,
          randomFunctions: [
            { bParam: 3, wParam: 4, vParam: [0, 1, 2, 3, 4] },
            { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
          ],
        },
      });
      const indexes = c.getIndexes();
      assertEqual(2, indexes.length);

      const vectorIndexes = indexes.filter(function (n) {
        return n.type == "vector";
      });
      assertEqual(1, vectorIndexes.length);

      const vectorIndex = vectorIndexes[0];
      assertEqual(idx.id, vectorIndex.id);
      assertEqual(["vec"], vectorIndex.fields);
    },

    testVectorIndexUndefinedParametersTest: function () {
      const vectorIndexFields = ["vec"];
      const invalidIndexes = [
        {
          type: "vector",
          fields: vectorIndexFields,
          unique: true,
          params: {
            dimensions: 5,
            min: 0,
            max: 10,
            Kparameter: 1,
            Lparameter: 1,
            randomFunctions: [
              { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
            ],
          },
        },
        {
          type: "vector",
          fields: vectorIndexFields,
          params: {
            dimensions: 5,
            min: 0,
            max: 10,
            Kparameter: 1,
            Lparameter: 1,
            DParameter: 212,
            randomFunctions: [
              { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
            ],
          },
        },
      ];

      for (let i = 0; i < invalidIndexes.length; ++i) {
        try {
          c.ensureIndex(invalidIndexes[i]);
        } catch (err) {
          assertTrue(err.code != 0);
        }

        // Only primary index remains
        const indexes = c.getIndexes();
        assertEqual(1, indexes.length);
      }
    },

    testVectorIndexWrongParameters: function () {
      const vectorIndexFields = ["vec"];
      const invalidIndexes = [
        {
          type: "vector",
          fields: vectorIndexFields,
          params: {
            dimensions: 5,
            min: 10,
            max: 9,
            Kparameter: 1,
            Lparameter: 1,
            randomFunctions: [
              { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
            ],
          },
        },
        {
          type: "vector",
          fields: vectorIndexFields,
          params: {
            dimensions: 0,
            min: 10,
            max: 9,
            Kparameter: 1,
            Lparameter: 1,
            randomFunctions: [
              { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
            ],
          },
        },
        {
          type: "vector",
          fields: vectorIndexFields,
          params: {
            dimensions: 5,
            min: 10,
            max: 9,
            Kparameter: 0,
            Lparameter: 1,
            randomFunctions: [
              { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
            ],
          },
        },
        {
          type: "vector",
          fields: vectorIndexFields,
          params: {
            dimensions: 5,
            min: 10,
            max: 9,
            Kparameter: 1,
            Lparameter: 0,
            randomFunctions: [
              { bParam: 1, wParam: 2, vParam: [0, 1, 2, 3, 4] },
            ],
          },
        },
      ];

      for (let i = 0; i < invalidIndexes.length; ++i) {
        try {
          c.ensureIndex(invalidIndexes[i]);
        } catch (err) {
          assertTrue(err.code != 0);
        }

        // Only primary index remains
        const indexes = c.getIndexes();
        assertEqual(1, indexes.length);
      }
    },
  };
}
// todo Add more test cases
// invariants, e.g. w not 0

jsunity.run(vectorIndexCreateSuite);

return jsunity.done();
