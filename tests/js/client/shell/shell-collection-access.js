/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertMatch, assertNull */

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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;

function CollectionSuite () {
  var cn = "UnitTestsCollection";
  var c = null;

  return {
    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },
    
    tearDown: function () {
      db._drop(cn);
      c = null;
    },

    testAccessByIdForExistingCollection : function () {
      var id = c._id;
      assertMatch(/^\d+$/, id);

      var accessed = db._collection(id);
      assertEqual(cn, accessed.name());
      assertEqual(id, accessed._id);
    },
    
    testAccessByIdForNonExistingCollection : function () {
      assertNull(db._collection("666666666666666666666"));
    }

  };
}

jsunity.run(CollectionSuite);

return jsunity.done();
