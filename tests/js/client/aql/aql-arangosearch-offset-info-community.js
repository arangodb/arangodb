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
/// @author Andrey Abramov
// //////////////////////////////////////////////////////////////////////////////

let arangodb = require("@arangodb");
let analyzers = require("@arangodb/analyzers");
let db = arangodb.db;
let internal = require("internal");
let jsunity = require("jsunity");
let errors = arangodb.errors;
let dbName = "OffsetInfoAqlTestSuiteCommunity";
let isEnterprise = internal.isEnterprise();

function IResearchOffsetInfoAqlTestSuiteCommunity() {
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
      let col = db._create("docs");
      analyzers.save("offsets", "delimiter", { delimiter: " " }, [ "frequency", "position", "offset"]);
      col.insert({ _key: "a"});
      col.insert({ body: "the quick brown fox jumps over the lazy dog", sub: { body: "the quick brown fox jumps over the lazy dog" } });
      col.insert({ body: "the foxx jumps over the fox", sub: { body: "the foxx jumps over the fox" } });
      col.insert({ body: "the fox jumps over the lazy dog", sub: { body: "the fox jumps over the lazy dog" } });
      col.insert({ body: "the dog jumps over the lazy fox", sub: { body: "the dog jumps over the lazy fox" } });
      col.insert({ body: "roses are red, violets are blue, foxes are quick", sub: { body: "roses are red, violets are blue, foxes are quick" } });
      db._createView("docsView", "arangosearch",
          { links: { docs: { includeAllFields : true, analyzers: ["offsets"] } } });
      db._query("FOR d IN docsView OPTIONS {waitForSync:true} LIMIT 1 RETURN 1");
    },
    tearDownAll : function () { 
      cleanup();
    },

    testSimpleOffset: function() {
      checkThatQueryReturnsExpectedError(
        "FOR d IN docsView SEARCH d._key == 'a' RETURN OFFSET_INFO(d, '_key')",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testOffsetWithAnalyzer: function() {
      checkThatQueryReturnsExpectedError(
        "FOR doc IN docsView SEARCH ANALYZER(doc.body == 'brown', 'offsets') RETURN OFFSET_INFO(doc, 'body')",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testOffsetWithSubField: function() {
      checkThatQueryReturnsExpectedError(
        "FOR doc IN docsView SEARCH ANALYZER(doc.sub.body == 'brown', 'offsets') RETURN OFFSET_INFO(doc, 'sub.body')",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testOffsetWithNonExistentField: function() {
      checkThatQueryReturnsExpectedError(
        "FOR doc IN docsView SEARCH ANALYZER(doc.doesNotExist == 'brown', 'offsets') RETURN OFFSET_INFO(doc, 'doesNotExist')",
        errors.ERROR_NOT_IMPLEMENTED.code);
    },

    testOffsetWithNonExistentSubField: function() {
      checkThatQueryReturnsExpectedError(
        "FOR doc IN docsView SEARCH ANALYZER(doc.sub.doesNotExist == 'brown', 'offsets') RETURN OFFSET_INFO(doc, 'sub.body.doesNotExist')",
        errors.ERROR_NOT_IMPLEMENTED.code);
    }
  };
}

if (!isEnterprise) {
  jsunity.run(IResearchOffsetInfoAqlTestSuiteCommunity);
}

return jsunity.done();
