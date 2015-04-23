/*jshint strict: true */
/*global require, assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test attribute naming
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("org/arangodb");

var db = arangodb.db;
var wait = require("internal").wait;

// -----------------------------------------------------------------------------
// --SECTION--                                                        attributes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function AttributesSuite () {
  "use strict";
  var ERRORS = require("internal").errors;
  var cn = "UnitTestsCollectionAttributes";
  var c = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      c.unload();
      c.drop();
      c = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief no attributes
////////////////////////////////////////////////////////////////////////////////

    testNoAttributes : function () {
      var doc = { };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);
      delete d1.error;

      assertEqual(d1, d2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief empty attribute name
////////////////////////////////////////////////////////////////////////////////

    testEmptyAttribute : function () {
      var doc = { "" : "foo" };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);
      delete d1.error;

      var i;
      for (i in d2) {
        if (d2.hasOwnProperty(i)) {
          assertTrue(i !== "");
        }
      }

      assertEqual(d1, d2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief query empty attribute name
////////////////////////////////////////////////////////////////////////////////

    testQueryEmptyAttribute : function () {
      var doc = { "" : "foo" };
      c.save(doc);

      var docs = c.toArray();
      assertEqual(1, docs.length);
      var d = docs[0];

      var i;
      for (i in d) {
        if (d.hasOwnProperty(i)) {
          assertTrue(i !== "");
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief "numeric" attribute names
////////////////////////////////////////////////////////////////////////////////

    testNumericAttributes : function () {
      var doc = { "12345" : 1, "6669" : "foo", "7734" : true };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(1, d2["12345"]);
      assertEqual("foo", d2["6669"]);
      assertEqual(true, d2["7734"]);

      assertEqual(undefined, d2["999"]);

      d2 = c.toArray()[0];
      assertEqual(1, d2["12345"]);
      assertEqual("foo", d2["6669"]);
      assertEqual(true, d2["7734"]);

      assertEqual(undefined, d2["999"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief reserved attribute names
////////////////////////////////////////////////////////////////////////////////

    testReservedAttributes : function () {
      var doc = {
        "_id" : "foo",
        "_rev": "99",
        "_key" : "meow",
        "_from" : "33",
        "_to": "99",
        "_test" : false,
        "_boom" : "bang"
      };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual("meow", d1._key);
      assertEqual("meow", d2._key);
      assertEqual(cn + "/meow", d1._id);
      assertEqual(cn + "/meow", d2._id);
      assertEqual(d1._rev, d2._rev);
      assertFalse(d2._test);
      assertEqual("bang", d2._boom);
      assertFalse(d2.hasOwnProperty("_from"));
      assertFalse(d2.hasOwnProperty("_to"));

      // user specified _rev value must have been ignored
      assertTrue(d1._rev !== "99");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief reserved attribute names
////////////////////////////////////////////////////////////////////////////////

    testEmbeddedReservedAttributes : function () {
      var doc = { "_id" : "foo", "_rev": "99", "_key" : "meow", "_from" : "33", "_to": "99", "_test" : false };

      c.save({ _key: "mydoc", _embedded: doc });
      var d = c.document("mydoc");

      assertEqual(cn + "/mydoc", d._id);
      assertEqual("mydoc", d._key);
      assertEqual("foo", d._embedded._id);
      assertEqual("99", d._embedded._rev);
      assertEqual("meow", d._embedded._key);
      assertEqual("33", d._embedded._from);
      assertEqual("99", d._embedded._to);
      assertFalse(d._embedded._test);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute name with special chars
////////////////////////////////////////////////////////////////////////////////

    testSpecialAttributes : function () {
      var doc = { "-meow-" : 1, "mötör" : 2, " " : 3, "\t" : 4, "\r" : 5, "\n" : 6 };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(1, d2["-meow-"]);
      assertEqual(2, d2["mötör"]);
      assertEqual(3, d2[" "]);
      assertEqual(4, d2["\t"]);
      assertEqual(5, d2["\r"]);
      assertEqual(6, d2["\n"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shared attribute list
////////////////////////////////////////////////////////////////////////////////

    testSharedAttributesList : function () {
      var sub = { name: 1 };
      var doc = { a: [ sub, sub ] };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(sub, d2.a[0]);
      assertEqual(sub, d2.a[1]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shared attribute object
////////////////////////////////////////////////////////////////////////////////

    testSharedAttributesObject : function () {
      var sub = { name: 1 };
      var doc = { a: sub, b: sub };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(sub, d2.a);
      assertEqual(sub, d2.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shared attribute list
////////////////////////////////////////////////////////////////////////////////

    testCyclicAttributesList : function () {
      var sub = {};
      var doc = { a: [ sub ] };

      sub.cycle = doc;

      try {
        c.save(doc);
        fail();
      }
      catch (err) {
        if (err && err.errorNum) {
          // we're on the server
          assertEqual(ERRORS.ERROR_ARANGO_SHAPER_FAILED.code, err.errorNum);
        }
        else {
          // we're on the client, and the JS engine just throws a generic type error
          assertTrue(true);
        }

      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test utf8 handling
////////////////////////////////////////////////////////////////////////////////

    testUtf8AttributeValues : function () {
      var data =[
        {"@language":"af","@value":"Kenia"},
        {"@language":"ak","@value":"Kɛnya"},
        {"@language":"am","@value":"ኬንያ"},
        {"@language":"an","@value":"Kenia"},
        {"@language":"ar","@value":"كينيا"},
        {"@language":"ast","@value":"Keña"},
        {"@language":"az","@value":"Kenya"},
        {"@language":"be","@value":"Кенія"},
        {"@language":"bg","@value":"Кения"},
        {"@language":"bm","@value":"Keniya"},
        {"@language":"bn","@value":"কেনিয়া"},
        {"@language":"bo","@value":"ཁེན་ཉི་ཡ།"},
        {"@language":"br","@value":"Kenya"},
        {"@language":"bs","@value":"Kenija"},
        {"@language":"ca","@value":"Kènia"},
        {"@language":"cs","@value":"Keňa"},
        {"@language":"cy","@value":"Cenia"},
        {"@language":"da","@value":"Kenya"},
        {"@language":"de","@value":"Kenia"},
        {"@language":"dz","@value":"ཀེ་ནི་ཡ"},
        {"@language":"ee","@value":"Kenya nutome"},
        {"@language":"el","@value":"Κένυα"},
        {"@language":"en","@value":"Kenya"},
        {"@language":"eo","@value":"Kenjo"},
        {"@language":"es","@value":"Kenia"},
        {"@language":"et","@value":"Keenia"},
        {"@language":"eu","@value":"Kenia"},
        {"@language":"fa","@value":"کنیا"},
        {"@language":"ff","@value":"Keñaa"},
        {"@language":"fi","@value":"Kenia"},
        {"@language":"fo","@value":"Kenja"},
        {"@language":"fr","@value":"Kenya"},
        {"@language":"frp","@value":"Kenia"},
        {"@language":"ga","@value":"An Chéinia"},
        {"@language":"gd","@value":"A Cheinia"},
        {"@language":"gl","@value":"Quenia"},
        {"@language":"gu","@value":"કેન્યા"},
        {"@language":"ha","@value":"Kenya"},
        {"@language":"hbs","@value":"Kenija"},
        {"@language":"he","@value":"קניה"},
        {"@language":"hi","@value":"केन्या"},
        {"@language":"hr","@value":"Kenija"},
        {"@language":"hsb","@value":"Kenija"},
        {"@language":"hu","@value":"Kenya"},
        {"@language":"hy","@value":"Քենիա"},
        {"@language":"ia","@value":"Kenya"},
        {"@language":"id","@value":"Kenya"},
        {"@language":"io","@value":"Kenia"},
        {"@language":"is","@value":"Kenýa"},
        {"@language":"it","@value":"Kenya"},
        {"@language":"ja","@value":"ケニア共和国"},
        {"@language":"ka","@value":"კენია"},
        {"@language":"ki","@value":"Kenya"},
        {"@language":"kl","@value":"Kenya"},
        {"@language":"km","@value":"កេនយ៉ា"},
        {"@language":"kn","@value":"ಕೀನ್ಯಾ"},
        {"@language":"ko","@value":"케냐"},
        {"@language":"ku","@value":"Kenya"},
        {"@language":"kw","@value":"Kenya"},
        {"@language":"la","@value":"Kenia"},
        {"@language":"lg","@value":"Kenya"},
        {"@language":"li","@value":"Kenia"},
        {"@language":"ln","@value":"Kenya"},
        {"@language":"lo","@value":"ເຄນຢ່າ"},
        {"@language":"lt","@value":"Kenija"},
        {"@language":"lu","@value":"Kenya"},
        {"@language":"lv","@value":"Kenija"},
        {"@language":"mg","@value":"Kenya"},
        {"@language":"mk","@value":"Кенија"},
        {"@language":"ml","@value":"കെനിയ"},
        {"@language":"mr","@value":"केनिया"},
        {"@language":"ms","@value":"Kenya"},
        {"@language":"mt","@value":"Kenja"},
        {"@language":"my","@value":"ကင်ညာ"},
        {"@language":"na","@value":"Keniya"},
        {"@language":"nb","@value":"Kenya"},
        {"@language":"nd","@value":"Khenya"},
        {"@language":"nds","@value":"Kenia"},
        {"@language":"ne","@value":"केन्या"},
        {"@language":"nl","@value":"Kenia"},
        {"@language":"nn","@value":"Kenya"},
        {"@language":"no","@value":"Kenya"},
        {"@language":"oc","@value":"Kenya"},
        {"@language":"om","@value":"Keeniyaa"},
        {"@language":"or","@value":"କେନିୟା"},
        {"@language":"pam","@value":"Kenya"},
        {"@language":"pl","@value":"Kenia"},
        {"@language":"pms","@value":"Chenia"},
        {"@language":"pt","@value":"Quênia"},
        {"@language":"rm","@value":"Kenia"},
        {"@language":"rn","@value":"Kenya"},
        {"@language":"ro","@value":"Kenya"},
        {"@language":"ru","@value":"Кения"},
        {"@language":"sa","@value":"केन्या"},
        {"@language":"se","@value":"Kenia"},
        {"@language":"sg","@value":"Kenyäa"},
        {"@language":"si","@value":"කෙන්යාව"},
        {"@language":"sk","@value":"Keňa"},
        {"@language":"sl","@value":"Kenija"},
        {"@language":"sn","@value":"Kenya"},
        {"@language":"so","@value":"Kiinya"},
        {"@language":"sq","@value":"Kenia"},
        {"@language":"sr","@value":"Кенија"},
        {"@language":"sv","@value":"Kenya"},
        {"@language":"sw","@value":"Kenya"},
        {"@language":"ta","@value":"கென்யா"},
        {"@language":"te","@value":"కెన్యా"},
        {"@language":"tg","@value":"Кения"},
        {"@language":"th","@value":"ประเทศเคนยา"},
        {"@language":"ti","@value":"ኬንያ"},
        {"@language":"tl","@value":"Kenya"},
        {"@language":"to","@value":"Kenia"},
        {"@language":"tr","@value":"Kenya"},
        {"@language":"ug","@value":"كېنىيە"},
        {"@language":"uk","@value":"Кенія"},
        {"@language":"ur","@value":"کینیا"},
        {"@language":"vi","@value":"Kê-ni-a (Kenya)"},
        {"@language":"vo","@value":"Kenyän"},
        {"@language":"yo","@value":"Orílẹ́ède Kenya"},
        {"@language":"zh","@value":"肯尼亚"},
        {"@language":"zu","@value":"i-Kenya"}
      ];

      data.forEach(function(value) {
        c.truncate();
        c.insert(value);

        var doc = c.toArray()[0];
        assertEqual(value["@language"], doc["@language"]);
        assertEqual(value["@value"], doc["@value"]);
      });
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AttributesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
