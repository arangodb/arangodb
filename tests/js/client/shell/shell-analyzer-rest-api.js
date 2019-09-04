/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */


const jsunity = require('jsunity');
const internal = require('internal');
const error = internal.errors;
//const print = internal.print;

function testSuite() {
  const endpoint = arango.getEndpoint();
  const db = require("@arangodb").db;
  const name = "vimoji";

  return {
    setUp: function() {},
    tearDown: function() {
      arango.DELETE("/_api/analyzer/" + name + "?force=true");
    },

    testAnalyzerGet : function() {
      let result = arango.GET("/_api/analyzer");
      assertEqual(result.error, false);
      assertEqual(result.code, 200);
      assertEqual(result.result.length, 13);
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
        properties : { locale: "en.UTF-8", stopwords: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerReplaceInplaceText : function() {
      // try to replace inplace which is forbidden
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8", stopwords: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);

      body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "de.UTF-8", stopwords: [ ] },
      });

      result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 400);
    },

    testAnalyzerCreateTextMissingName : function() {
      let body = JSON.stringify({
        type : "text",
        properties : { locale: "en.UTF-8", stopwords: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 400);
      assertEqual(result.errorNum, error.ERROR_HTTP_BAD_PARAMETER.code);
    },

    testAnalyzerCreateTextMissingProperties1 : function() {
      let body = JSON.stringify({
        type : "text",
        name : name,
        //properties : { locale: "en.UTF-8", stopwords: [ ] },
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
        properties : {  stopwords: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 400);
      assertEqual(result.errorNum, error.ERROR_HTTP_BAD_PARAMETER.code);
    },

    testAnalyzerCreateTextInvalidProperties1 : function() {
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8" , stopwords: { invalid : "property" } },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 400);
      assertEqual(result.errorNum, error.ERROR_HTTP_BAD_PARAMETER.code);
    },

    testAnalyzerCreateDelimited : function() {
      let body = JSON.stringify({
        type : "delimiter",
        name : name,
        properties : { delimiter : "❤" } , // .hsv - heart separated value list :)
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

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

    testAnalyzerCreateNorm: function () {
      let body = JSON.stringify({
        type: "norm",
        name: name,
        properties: { locale: "en.UTF-8", accent:true, stemming: false },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateStem: function () {
      let body = JSON.stringify({
        type: "stem",
        name: name,
        properties: { locale: "en.UTF-8" },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerDifferentDB : function() {
      let rv = db._createDatabase("ulf");
      let body = JSON.stringify({
        type : "identity",
        name : name,
      });

      let result = arango.POST_RAW("/_db/ulf/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
      db._dropDatabase("ulf");
    },

    testAnalyzerCreateMany : function() {
      // should be last test as we do no clean-up
      const num = 10;
      for (var i = 0; i < num; i++) {
        let body = JSON.stringify({
          type : "text",
          name : name + i,
          properties : { locale: "en.UTF-8", stopwords: [ ] },
        });

        let result = arango.POST_RAW("/_api/analyzer", body);
        assertFalse(result.error);
        assertEqual(result.code, 201);
      }

      let res = arango.GET("/_api/analyzer");
      assertEqual(res.result.length, num + 13); // vimoji(0..9) + static

      for (var j = 0; j < num; j++) {
        arango.DELETE("/_api/analyzer/" + name + j);
      }
    },
    
    testAnalyzerLinks : function() {
      try {
        let body = JSON.stringify({
          name : name,
          type : "text",
          properties : { locale: "en.UTF-8", stopwords: [ ] },
        });

        let result = arango.POST_RAW("/_api/analyzer", body);
        assertFalse(result.error);

        let col = db._create("ulfColTestLinks");
        let view = db._createView("ulfViewTestLinks", "arangosearch", {});
        var properties = {
          links : {
            [col.name()] : {
              includeAllFields : true,
              storeValues : "id",
              fields : {
                text : { analyzers : [name] }
              }
            }
          }
        };
        view.properties(properties);

        result = arango.DELETE("/_api/analyzer/" + name);

        assertTrue(result.error);
        assertEqual(result.code, 409); // can not delete -- referencded by link
        assertEqual(result.errorNum, error.ERROR_ARANGO_CONFLICT.code);

        // delete with force - must succeed
        result = arango.DELETE("/_api/analyzer/" + name + "?force=true");
        assertFalse(result.error);
      } finally {
        try {
          db._drop("ulfColTestLinks");
        } catch (_) {}
        try {
          db._dropView("ulfViewTestLinks");
        } catch (_) {}
      }
    }
  }; // return
} // end of suite
jsunity.run(testSuite);
return jsunity.done();
