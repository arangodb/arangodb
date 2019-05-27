/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */


const jsunity = require('jsunity');
const internal = require('internal');
const error = internal.errors;
const print = internal.print;

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


    testAnalyzerCreateIdentity : function() {
      let body = JSON.stringify({
        type : "identity",
        name : name,
      });

      let result = arango.POST_RAW("/_api/analyzer", body);

      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateRemoveIdentiy : function() {
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

    testAnalyzerCreateDoubleIdentity : function() {
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
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateTextMissingProperties1 : function() {
      let body = JSON.stringify({
        type : "text",
        name : name,
        //properties : { locale: "en.UTF-8", ignored_words: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 400);
      assertEqual(result.errorNum, error.ERROR_HTTP_BAD_PARAMETER.code);
    },

    testAnalyzerCreateTextMissingProperties2 : function() {
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8" /*, ignored_words: [ ]*/ },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 400);
      assertEqual(result.errorNum, error.ERROR_HTTP_BAD_PARAMETER.code);
    },

    testAnalyzerCreateDelimited : function() {
      let body = JSON.stringify({
        type : "delimited",
        name : name,
        properties : { delimiter : "❤" } ,
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    // //should work - fix or update docs
    // testAnalyzerCreateDelimited : function() {
    //   let body = JSON.stringify({
    //     type : "delimited",
    //     name : name,
    //     properties : "❤" ,
    //   });

    //   let result = arango.POST_RAW("/_api/analyzer", body);
    //   assertFalse(result.error);
    //   assertEqual(result.code, 201);
    // },

    testAnalyzerCreateNgram : function() {
      let body = JSON.stringify({
        type : "ngram",
        name : name,
        properties : { min : 3, max : 4, preserveOriginal : true } ,
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    // //should work
    // testAnalyzerDifferentDB : function() {
    //   let rv = internal.db._createDatabase("ulf");
    //   let body = JSON.stringify({
    //     type : "identity",
    //     name : name,
    //   });

    //   let result = arango.POST_RAW("_db/ulf/_api/analyzer", body);
    //   assertFalse(result.error);
    //   assertEqual(result.code, 201);
    //   internal.db._dropDatabase("ulf");
    // },

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
        assertFalse(result.error);
        assertEqual(result.code, 201);
      }

      let res = arango.GET("/_api/analyzer");
      assertEqual(res.result.length, num + 1) // vimoji(0..9) + identity
    },

    testAnalyzerRemoveWhileUsed : function() {
      //create
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8", ignored_words: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);

      let query = `LET X=SLEEP(10) RETURN TOKENS('a quick brown fox jumps over the lazy dog', '_system::${name}')`;
      body = JSON.stringify({
        query : query,
      });

      // create an async request
      let headers = {"x-arango-async" : "store"};
      result = arango.POST_RAW("/_api/cursor", body, headers );
      assertEqual(result.code, 202); //accepted
      result = arango.DELETE("/_api/analyzer/" + name);
      assertTrue(result.error); //shold be used
    }


  }; // return
} // end of suite
jsunity.run(testSuite);
return jsunity.done();
