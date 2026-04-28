/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertFalse, assertTrue */

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
/// @author Michael Hackstein
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const cn = "UnitTestEdgeCollection";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: index creation
////////////////////////////////////////////////////////////////////////////////

function vertexCentricIndexSuite() {
  var collection = null;

  return {

    setUp: () => {
      internal.db._drop(cn);
      collection = internal.db._createEdgeCollection(cn, { waitForSync : false });
    },

    tearDown: () => {
      collection.drop();
      collection = null;
      internal.wait(0.0);
    },

    testCreateDefault : () => {
      let before = collection.indexes();
      let idx = collection.ensureIndex({ type: "persistent", fields: ["_from", "label"] });

      let after = collection.indexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("persistent", idx.type);
      assertEqual(["_from", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);
    },

    testCreateMultiFields : () => {
      let before = collection.indexes();
      let idx = collection.ensureIndex({ type: "persistent", fields: ["_from", "label", "type"] });

      let after = collection.indexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("persistent", idx.type);
      assertEqual(["_from", "label", "type"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);
    },

    testCreateInbound : () => {
      let before = collection.indexes();
      let idx = collection.ensureIndex({ type: "persistent", fields: ["_to", "label"] });

      let after = collection.indexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("persistent", idx.type);
      assertEqual(["_to", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);
    },
  };
}

jsunity.run(vertexCentricIndexSuite);

return jsunity.done();
