var jsunity = require("jsunity"),
  preprocess = require("org/arangodb/foxx/preprocessor").preprocess,
  Preprocessor = require("org/arangodb/foxx/preprocessor").Preprocessor;

// High Level Spec Suite for the preprocess Function
function PreprocessSpec () {
  var testFileWithoutJSDoc, testFileWithJSDoc, testFileWithJSDocTransformed;

  return {
    setUp: function () {
      testFileWithoutJSDoc = [
        "(function() {",
        "  // normal comment",
        "  var x = 1;",
        "  /* long description",
        "   * test",
        "   */",
        "}());"
      ].join("\n");

      testFileWithJSDoc = [
        "(function() {",
        "  /** long description",
        "   * test",
        "   */",
        "  var x = 2;",
        "}());"
      ].join("\n");

      testFileWithJSDocTransformed = [
        "(function() {",
        "applicationContext.comment(\"long description\");",
        "applicationContext.comment(\"test\");",
        "applicationContext.comment(\"\");",
        "  var x = 2;",
        "}());"
      ].join("\n");
    },

    testDoesntChangeFileWithoutJSDocComments: function () {
      assertEqual(preprocess(testFileWithoutJSDoc), testFileWithoutJSDoc);
    },

    testTransformsFileWithJSDocComments: function () {
      assertEqual(preprocess(testFileWithJSDoc), testFileWithJSDocTransformed);
    }
  };
}

// Low level Spec Suite for the Transform prototype
function PreprocessorSpec () {
  var i, testFileWithSingleJSDoc, testFileWithJSDocAndMultiline, processedLineOne;

  return {
    setUp: function () {
      testFileWithSingleJSDoc = [
        "(function() {",
        "  /** long description",
        "   * test",
        "   */",
        "  var x = 2;",
        "}());"
      ].join("\n");

      processedLineOne = [
        "(function() {",
        "applicationContext.comment(\"long description\");",
        "   * test",
        "   */",
        "  var x = 2;",
        "}());"
      ].join("\n");

      testFileWithMultiline = [
        "(function() {",
        "  /* long description",
        "   * test",
        "   */",
        "  var x = 2;",
        "}());"
      ].join("\n");

      testFileWithJSDocAndMultiline = [
        "(function() {",
        "  /** long description",
        "   * test",
        "   */",
        "  var x = 2;",
        "  /* long comment",
        "   * test",
        "   */",
        "}());"
      ].join("\n");
    },

    testSearchForFirstJSDocStart: function () {
      processer = new Preprocessor(testFileWithSingleJSDoc);

      for (i = 0; i < 1; i++) {
        processer.searchNext();
      }

      assertEqual(processer.getCurrentLineNumber(), 1);
    },

    testContinueInJSDoc: function () {
      processer = new Preprocessor(testFileWithSingleJSDoc);

      for (i = 0; i < 2; i++) {
        processer.searchNext();
      }

      assertEqual(processer.getCurrentLineNumber(), 2);
    },

    testStopAtTheEndOfJSDoc: function () {
      processer = new Preprocessor(testFileWithSingleJSDoc);

      for (i = 0; i < 4; i++) {
        processer.searchNext();
      }

      assertUndefined(processer.getCurrentLineNumber());
    },

    testDoNotIncludeNormalMultiline: function () {
      processer = new Preprocessor(testFileWithMultiline);

      for (i = 0; i < 1; i++) {
        processer.searchNext();
      }

      assertUndefined(processer.getCurrentLineNumber());
    },

    testDoNotIncludeNonJSDocComments: function () {
      processer = new Preprocessor(testFileWithJSDocAndMultiline);

      for (i = 0; i < 4; i++) {
        processer.searchNext();
      }

      assertUndefined(processer.getCurrentLineNumber());
    },

    testConvertLine: function () {
      processer = new Preprocessor(testFileWithSingleJSDoc);

      processer.searchNext();
      processer.convertLine();

      assertEqual(processer.result(), processedLineOne);
    }
  };
}

jsunity.run(PreprocessSpec);
jsunity.run(PreprocessorSpec);

return jsunity.done();
