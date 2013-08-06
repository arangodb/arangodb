require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  transform = require("org/arangodb/foxx/transformer").transform,
  Transformer = require("org/arangodb/foxx/transformer").Transformer;

// High Level Spec Suite for the transform Function
function TransformSpec () {
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
        "appContext.comment(\"long description\");",
        "appContext.comment(\"test\");",
        "appContext.comment(\"\");",
        "  var x = 2;",
        "}());"
      ].join("\n");
    },

    testDoesntChangeFileWithoutJSDocComments: function () {
      assertEqual(transform(testFileWithoutJSDoc), testFileWithoutJSDoc);
    },

    testTransformsFileWithJSDocComments: function () {
      assertEqual(transform(testFileWithJSDoc), testFileWithJSDocTransformed);
    }
  };
}

// Low level Spec Suite for the Transform prototype
function TransformerSpec () {
  var i, result, testFileWithSingleJSDoc, testFileWithJSDocAndMultiline, transformedLineOne;

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

      transformedLineOne = [
        "(function() {",
        "appContext.comment(\"long description\");",
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
      transformer = new Transformer(testFileWithSingleJSDoc);

      for (i = 0; i < 1; i++) {
        transformer.searchNext();
      }

      assertEqual(transformer.getCurrentLineNumber(), 1);
    },

    testContinueInJSDoc: function () {
      transformer = new Transformer(testFileWithSingleJSDoc);

      for (i = 0; i < 2; i++) {
        transformer.searchNext();
      }

      assertEqual(transformer.getCurrentLineNumber(), 2);
    },

    testStopAtTheEndOfJSDoc: function () {
      transformer = new Transformer(testFileWithSingleJSDoc);

      for (i = 0; i < 4; i++) {
        transformer.searchNext();
      }

      assertUndefined(transformer.getCurrentLineNumber());
    },

    testDoNotIncludeNormalMultiline: function () {
      transformer = new Transformer(testFileWithMultiline);

      for (i = 0; i < 1; i++) {
        transformer.searchNext();
      }

      assertUndefined(transformer.getCurrentLineNumber());
    },

    testDoNotIncludeNonJSDocComments: function () {
      transformer = new Transformer(testFileWithJSDocAndMultiline);

      for (i = 0; i < 4; i++) {
        transformer.searchNext();
      }

      assertUndefined(transformer.getCurrentLineNumber());
    },

    testConvertLine: function () {
      transformer = new Transformer(testFileWithSingleJSDoc);

      transformer.searchNext();
      transformer.convertLine();

      assertEqual(transformer.result(), transformedLineOne);
    }
  };
}

jsunity.run(TransformSpec);
jsunity.run(TransformerSpec);

return jsunity.done();
