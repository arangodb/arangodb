/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */


const jsunity = require('jsunity');
const internal = require('internal');
const error = internal.errors;
const print = internal.print;

//arango.reconnect(endpoint, db._name(), "test_rw", "testi");

function testSuite() {
  let endpoint = arango.getEndpoint();
  let db = require("@arangodb").db;
  const name = "vimoji"

  return {
    setUp: function() {},
    tearDown: function() {
      arango.DELETE("/_api/analyzer/" + name + "?force=true");
    },

    testAnalyzerGet : function() {
      let result = arango.GET("/_api/analyzer");
      assertEqual(result.error, false);
      assertEqual(result.code, 200);
      assertEqual(result.result[0].name, "identity");
      assertEqual(result.result[0].features, [ "frequency", "norm"]);
    },

    testAnalyzerGetInvalid : function() {
      let result = arango.GET("/_api/analyzer/invalid");
      assertTrue(result.error);
      assertEqual(result.code, 404);
      assertEqual(result.errorNum, error.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
    },


    testAnalyzerCreate : function() {
      let body = JSON.stringify({
        type : "identity",
        name : name,
      });

      let result = arango.POST_RAW("/_api/analyzer", body);

      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateRemove : function() {
      let body = JSON.stringify({
        type : "identity",
        name : name,
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
      result = arango.DELETE("/_api/analyzer/" + name);
      assertFalse(result.error);
      assertEqual(result.code, 200);
      result = arango.DELETE("/_api/analyzer/" + name);
      assertTrue(result.error);
      assertEqual(result.code, 404); // already deleted
    },

    testAnalyzerCreateDouble : function() {
      let body = JSON.stringify({
        type : "identity",
        name : name,
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertEqual(result.code, 201);
      result = arango.POST_RAW("/_api/analyzer", body);
      assertEqual(result.code, 200);
    },


    testAnalyzerCreateText : function() {
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8", ignored_words: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      print(result)

      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateMany : function() {
      // should be last test as we do no clean-up
      const num = 10;
      for (var i = 0; i < num; i++) {
        let body = JSON.stringify({
          type : "text",
          name : name + i,
          properties : { locale: "en.UTF-8", ignored_words: [ ] },
        });

        let result = arango.POST_RAW("/_api/analyzer", body);
        print(result)

        assertFalse(result.error);
        assertEqual(result.code, 201);
      }

      let res = arango.GET("/_api/analyzer");
      assertEqual(res.result.length, num + 1) // vimoji(0..9) + identity
    }
  }; // return
} // end of suite
jsunity.run(testSuite);
return jsunity.done();
