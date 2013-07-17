////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, fulltext queries
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require("org/arangodb").db;
var jsunity = require("jsunity");
var errors = require("org/arangodb").errors;
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFulltextTestSuite () {
  var cn = "UnitTestsAhuacatlFulltext";
  var fulltext;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);

      fulltext = db._create(cn);
      fulltext.ensureFulltextIndex("text");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fulltext function
////////////////////////////////////////////////////////////////////////////////

    testFulltext1 : function () {
      var actual;

      fulltext.save({ id : 1, text : "some rubbish text" });
      fulltext.save({ id : 2, text : "More rubbish test data. The index should be able to handle all this." });
      fulltext.save({ id : 3, text : "even MORE rubbish. Nevertheless this should be handled well, too." });
     
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'some') SORT d.id RETURN d.id");
      assertEqual([ 1 ], actual);

      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'rubbish') SORT d.id RETURN d.id");
      assertEqual([ 1, 2, 3 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'text') SORT d.id RETURN d.id");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'More') SORT d.id RETURN d.id");
      assertEqual([ 2, 3 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'should') SORT d.id RETURN d.id");
      assertEqual([ 2, 3 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'banana') SORT d.id RETURN d.id");
      assertEqual([ ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fulltext function
////////////////////////////////////////////////////////////////////////////////

    testFulltext2 : function () {
      var texts = [
        "Flötenkröten tröten böse wörter nörgelnd",
        "Krötenbrote grölen stoßen GROßE Römermöter",
        "Löwenschützer möchten mächtige Müller ködern",
        "Differenzenquotienten goutieren gourmante Querulanten, quasi quergestreift",
        "Warum winken wichtige Winzer weinenden Wichten watschelnd winterher?",
        "Warum möchten böse wichte wenig müßige müller meiern?",
        "Loewenschuetzer moechten maechtige Mueller koedern",
        "Moechten boese wichte wenig mueller melken?"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        fulltext.save({ id : (i + 1), text : texts[i] });
      }
     
      var actual;
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'prefix:möchten,müller') SORT d.id RETURN d.id");
      assertEqual([ 3, 6 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'möchten,prefix:müller') SORT d.id RETURN d.id");
      assertEqual([ 3, 6 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'möchten,müller') SORT d.id RETURN d.id");
      assertEqual([ 3, 6 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'Flötenkröten,|goutieren') SORT d.id RETURN d.id");
      assertEqual([ 1, 4 ], actual);
     
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'prefix:quergestreift,|koedern,|prefix:römer,-melken,-quasi') SORT d.id RETURN d.id");
      assertEqual([ 2, 7 ], actual);
      
      actual = getQueryResults("FOR d IN FULLTEXT(" + fulltext.name() + ", 'text', 'prefix:quergestreift,|koedern,|prefix:römer,-melken') SORT d.id RETURN d.id");
      assertEqual([ 2, 4, 7 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test without fulltext index available
////////////////////////////////////////////////////////////////////////////////

    testNonIndexed : function () {
      assertQueryError(errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING.code, "RETURN FULLTEXT(" + fulltext.name() + ", 'bang', 'search')"); 
      assertQueryError(errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING.code, "RETURN FULLTEXT(" + fulltext.name() + ", 'texts', 'foo')"); 
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "RETURN FULLTEXT(NotExistingFooCollection, 'text', 'foo')"); 
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlFulltextTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
