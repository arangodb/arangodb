/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

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
/// @author Alexey Bakharev
// //////////////////////////////////////////////////////////////////////////////

let arangodb = require("@arangodb");
let analyzers = require("@arangodb/analyzers");
let db = arangodb.db;
let internal = require("internal");
let jsunity = require("jsunity");
let errors = arangodb.errors;
let dbName = "LshTestSuiteCommunity";
let isEnterprise = internal.isEnterprise();

function IResearchLshAqlTestSuiteCommunity() {
  let cleanup = function() {
    db._useDatabase("_system");
    try { db._dropDatabase(dbName); } catch (err) {}
  };

  var checkThatQueryReturnsExpectedError = function(query, expected_error_code){
    try {
      db._query(query);
      assertTrue(false);
    } catch (e) {
      assertEqual(expected_error_code, e.errorNum);
    }
  };

  return {
    setUpAll : function () { 
      cleanup();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      let collection_0 = db._create("collection_0", {computedValues:[{"name":"mh10","expression":"Return 'a'","computeOn":["insert","update","replace"],"overwrite":true,"failOnWarning":false,"keepNull":true},{"name":"mh50","expression":"Return 'b'","computeOn":["insert","update","replace"],"overwrite":true,"failOnWarning":false,"keepNull":true},{"name":"mh100","expression":"Return 'c'","computeOn":["insert","update","replace"],"overwrite":true,"failOnWarning":false,"keepNull":true}],
      replicationFactor:3, 
      writeConcern:1, 
      numberOfShards : 3});

      var analyzers = require("@arangodb/analyzers");
      analyzers.save("delimiter", "delimiter", {"delimiter": "#", "features": []});

      collection_0.insert({ _key: "a"});
      collection_0.insert({ body: "the quick brown fox jumps over the lazy dog", sub: { body: "the quick brown fox jumps over the lazy dog" } });
      collection_0.insert({ body: "the foxx jumps over the fox", sub: { body: "the foxx jumps over the fox" } });
      collection_0.insert({ body: "the fox jumps over the lazy dog", sub: { body: "the fox jumps over the lazy dog" } });
      collection_0.insert({ body: "the dog jumps over the lazy fox", sub: { body: "the dog jumps over the lazy fox" } });
      collection_0.insert({ body: "roses are red, violets are blue, foxes are quick", sub: { body: "roses are red, violets are blue, foxes are quick" } });

      let view = db._createView("view", "arangosearch", {});
      view.properties({
        links: {
          collection_0: {
            fields: {
              mh50: {
                analyzers: [
                  "identity"
                ]
              },
              mh100: {
                analyzers: [
                  "identity"
                ]
              },
              mh10: {
                analyzers: [
                  "identity"
                ]
              }
            }
          }
        }
      });
    },
    tearDownAll : function () { 
      cleanup();
    },

    testMinhashAnalyzer: function() {
      // assert that minhash analyzers are unavailable
      try {
        analyzers.save("myMinHash10", "minhash", {"numHashes": 10, "analyzer": {"type": "delimiter", "properties": {"delimiter": "#", "features": []}}});
        assertTrue(false);
      } catch (e) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, e.errorNum);
      }
    },

    testMinhash: function() {
      checkThatQueryReturnsExpectedError(
        "for d in collection_0 filter minhash(Tokens(d.dataStr, 'delimiter'), 10) == d.mh10 collect with count into c return c",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testMinhashMatch: function() {
      checkThatQueryReturnsExpectedError(
        "for doc in view search minhash_match(doc.dataStr, 'abc', 0.3, 'identity') return doc",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testMinhashCount: function() {
      checkThatQueryReturnsExpectedError(
        "RETURN MINHASH_COUNT(5)",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testMinhashError: function() {
      checkThatQueryReturnsExpectedError(
        "RETURN MINHASH_ERROR(0.05)",
        errors.ERROR_NOT_IMPLEMENTED.code);
    }
  };
}

if (!isEnterprise) {
  jsunity.run(IResearchLshAqlTestSuiteCommunity);
}

return jsunity.done();
