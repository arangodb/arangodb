/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertUndefined */

////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lucas Dohmen
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity"),
  preprocess = require("@arangodb/foxx/preprocessor").preprocess,
  Preprocessor = require("@arangodb/foxx/preprocessor").Preprocessor;

// High Level Spec Suite for the preprocess Function
function PreprocessSpec () {
  'use strict';
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
  'use strict';
  var i;
  var testFileWithSingleJSDoc;
  var testFileWithJSDocAndMultiline;
  var processedLineOne;
  var testFileWithMultiline;

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
      var processer = new Preprocessor(testFileWithSingleJSDoc);

      for (i = 0; i < 1; i++) {
        processer.searchNext();
      }

      assertEqual(processer.getCurrentLineNumber(), 1);
    },

    testContinueInJSDoc: function () {
      var processer = new Preprocessor(testFileWithSingleJSDoc);

      for (i = 0; i < 2; i++) {
        processer.searchNext();
      }

      assertEqual(processer.getCurrentLineNumber(), 2);
    },

    testStopAtTheEndOfJSDoc: function () {
      var processer = new Preprocessor(testFileWithSingleJSDoc);

      for (i = 0; i < 4; i++) {
        processer.searchNext();
      }

      assertUndefined(processer.getCurrentLineNumber());
    },

    testDoNotIncludeNormalMultiline: function () {
      var processer = new Preprocessor(testFileWithMultiline);

      for (i = 0; i < 1; i++) {
        processer.searchNext();
      }

      assertUndefined(processer.getCurrentLineNumber());
    },

    testDoNotIncludeNonJSDocComments: function () {
      var processer = new Preprocessor(testFileWithJSDocAndMultiline);

      for (i = 0; i < 4; i++) {
        processer.searchNext();
      }

      assertUndefined(processer.getCurrentLineNumber());
    },

    testConvertLine: function () {
      var processer = new Preprocessor(testFileWithSingleJSDoc);

      processer.searchNext();
      processer.convertLine();

      assertEqual(processer.result(), processedLineOne);
    }
  };
}

jsunity.run(PreprocessSpec);
jsunity.run(PreprocessorSpec);

return jsunity.done();
