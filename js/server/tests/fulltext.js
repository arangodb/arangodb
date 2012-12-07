////////////////////////////////////////////////////////////////////////////////
/// @brief tests for fulltext
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

var internal = require("internal");
var jsunity = require("jsunity");

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext queries
////////////////////////////////////////////////////////////////////////////////

function fulltextQuerySuite () {
  var cn = "UnitTestsFulltext";
  var collection = null;
  var idx = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      idx = collection.ensureFulltextIndex("text", false).id;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      internal.db._drop(cn);
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
////////////////////////////////////////////////////////////////////////////////

    testSimple: function () {
      var texts = [
        "some rubbish text",
        "More rubbish test data. The index should be able to handle all this.",
        "even MORE rubbish. Nevertheless this should be handled well, too."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "some").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "rubbish").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "text").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "More").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "test").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "data").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "The").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "index").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "should").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "be").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "able").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "to").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "handle").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "all").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "this").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "even").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Nevertheless").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "handled").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "well").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "too").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "not").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "foobar").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "it").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "BANANA").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "noncontained").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "notpresent").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "Invisible").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "unAvailaBLE").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "Neverthelessy").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "dindex").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "grubbish").documents.length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief test updates
////////////////////////////////////////////////////////////////////////////////
    
    testUpdates: function () {
      var d1 = collection.save({ text: "Some people like bananas, but others like tomatoes" });
      var d2 = collection.save({ text: "Several people hate oranges, but many like them" });

      assertEqual(1, collection.FULLTEXT(idx, "like,bananas").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "like,tomatoes").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "oranges,hate").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "people").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "but").documents.length);

      collection.update(d1, { text: "bananas are delicious" });
      assertEqual(1, collection.FULLTEXT(idx, "bananas").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "delicious").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "like,bananas").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "like,tomatoes").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "oranges,hate").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "people").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "but").documents.length);
      
      collection.update(d2, { text: "seems that some where still left!" });
      assertEqual(1, collection.FULLTEXT(idx, "bananas").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "bananas,delicious").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "bananas,like").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "seems,left").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "delicious").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "oranges").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "people").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deletes
////////////////////////////////////////////////////////////////////////////////
    
    testDeletes: function () {
      var d1 = collection.save({ text: "Some people like bananas, but others like tomatoes" });
      var d2 = collection.save({ text: "Several people hate oranges, but many like them" });
      var d3 = collection.save({ text: "A totally unrelated text is inserted into the index, too" });

      assertEqual(1, collection.FULLTEXT(idx, "like,bananas").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "like,tomatoes").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "oranges,hate").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "people").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "but").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "totally,unrelated").documents.length);

      collection.remove(d1);
      assertEqual(0, collection.FULLTEXT(idx, "bananas").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "like,bananas").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "like").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "oranges,hate").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "people").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "unrelated,text,index").documents.length);
      
      collection.remove(d3);
      assertEqual(0, collection.FULLTEXT(idx, "bananas").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "like").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "oranges,hate").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "people").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "unrelated,text,index").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "unrelated").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "text").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "index").documents.length);
      
      collection.remove(d2);
      assertEqual(0, collection.FULLTEXT(idx, "bananas").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "like").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "oranges").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "hat").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "people").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "unrelated").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "text").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "index").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief case sensivity
////////////////////////////////////////////////////////////////////////////////

    testCaseSensivity: function () {
      var texts = [
        "sOMe rubbiSH texT",
        "MoRe rubbish test Data. The indeX should be able to handle alL this.",
        "eveN MORE RUBbish. neverThELeSs ThIs should be hanDLEd well, ToO."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "sOmE").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "RUBBISH").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "texT").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "MORE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "tEsT").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Data").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "tHE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "inDex").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "shoULd").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "bE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "ablE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "TO").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "hAnDlE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "AlL").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "THis").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "eVen").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "nEvertheless").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "HandleD").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "welL").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "ToO").documents.length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief long words
////////////////////////////////////////////////////////////////////////////////

    testLongWords: function () {
      var texts = [
        // German is ideal for this
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattinsfreundinnenbesucheranlassversammlungsortausschilderungsherstellungsfabrikationsanlagenbetreiberliebhaberliebhaber",
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Reliefpfeiler feilen reihenweise Pfeilreifen"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "AUTOTUERENDELLENentfernungsfirmenmitarbeiterverguetungsbewerter").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin").documents.length); // significance!

      assertEqual(1, collection.FULLTEXT(idx, "reliefpfeiler").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "feilen").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "reihenweise").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "pfeilreifen").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "pfeil").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "feile").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "feiler").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "reliefpfeilen").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "foo").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "bar").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "baz").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "pfeil").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "auto").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "it").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "not").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "somethingisreallywrongwiththislongwordsyouknowbetternotputthemintheindexyouneverknowwhathappensiftheresenoughmemoryforalltheindividualcharactersinthemletssee").documents.length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief search for multiple words
////////////////////////////////////////////////////////////////////////////////

    testMultipleWords: function () {
      var texts = [
        "sOMe rubbiSH texT",
        "MoRe rubbish test Data. The indeX should be able to handle alL this.",
        "eveN MORE RUBbish. neverThELeSs ThIs should be hanDLEd well, ToO."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "some,rubbish,text").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "some,rubbish").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "SOME,TEXT,RUBBISH").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "ruBBisH,TEXT,some").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "rubbish,text").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "some,text").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "more,rubbish,test,data,the,index,should,be,able,to,handle,all,this").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "even,more,rubbish,nevertheless,this,should,be,handled,well,too").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "even,too").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "even,rubbish,should,be,handled").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "more,well").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "well,this,rubbish,more").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "nevertheless,well").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "should,this,be").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "should,this").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "should,be").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "this,be").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "this,be,should").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "THIS,should").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "THIS,should,BE").documents.length);

      assertEqual(0, collection.FULLTEXT(idx, "some,data").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "nevertheless,text").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "some,rubbish,data").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "more,nevertheless,index").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "tomato,text").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "banana,too").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "apple").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesHorizontal: function () {
      collection.save({ text: "the the the the the the the their theirs them them themselves" });
      collection.save({ text: "apple banana tomato peach" });
     
      assertEqual(1, collection.FULLTEXT(idx, "the").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "them").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "themselves").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "the,their,theirs,them,themselves").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "she").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "thus").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "these,those").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesHorizontalMany: function () {
      var text = "";
      for (var i = 0; i < 1000; ++i) {
        text += "some random text,";
      }

      collection.save({ text: text });
      collection.save({ text: text });
     
      assertEqual(2, collection.FULLTEXT(idx, "text,random").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:rando").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "banana").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesQuery: function () {
      collection.save({ text: "the quick brown dog jumped over the lazy fox" });
      collection.save({ text: "apple banana tomato peach" });
      
      assertEqual(1, collection.FULLTEXT(idx, "the,the,the,the,the,the,the,the").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "apple,apple,apple,apple,apple,apple,apple,apple,APPLE,aPpLE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "apple,apple,apple,apple,apple,apple,apple,apple,APPLE,aPpLE,peach").documents.length);

      assertEqual(0, collection.FULLTEXT(idx, "apple,apple,apple,apple,apple,apple,apple,apple,APPLE,aPpLE,peach,fox").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesVertical: function () {
      var rubbish = "the quick brown fox jumped over the lazy dog";
      
      for (var i = 0; i < 100; ++i) {
        collection.save({ text: rubbish });
      }
      
      assertEqual(100, collection.FULLTEXT(idx, "the").documents.length);
      assertEqual(100, collection.FULLTEXT(idx, "the,dog").documents.length);
      assertEqual(100, collection.FULLTEXT(idx, "quick,brown,dog").documents.length);
      assertEqual(100, collection.FULLTEXT(idx, "the,quick,brown,fox,jumped,over,the,lazy,dog").documents.length);
      assertEqual(100, collection.FULLTEXT(idx, "dog,lazy,the,over,jumped,fox,brown,quick,the").documents.length);
      assertEqual(100, collection.FULLTEXT(idx, "fox,over,dog").documents.length);

      assertEqual(0, collection.FULLTEXT(idx, "the,frog").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "no,cats,allowed").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "banana").documents.length);
    },
/*
////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesMatrix: function () {
      var text = "";
      for (var i = 0; i < 1000; ++i) {
        text += "some random text,";
      }

      for (var i = 0; i < 1000; ++i) {
        collection.save({ text: text });
      }
     
      assertEqual(1000, collection.FULLTEXT(idx, "text,random").documents.length);
      assertEqual(1000, collection.FULLTEXT(idx, "prefix:rando").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "banana").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesDocuments: function () {
      var text1 = "this is a short document text";
      var text2 = "Some longer document text is put in here just to validate whats going on";

      for (var i = 0; i < 10000; ++i) {
        collection.save({ text: text1 });
        collection.save({ text: text2 });
      }
     
      assertEqual(20000, collection.FULLTEXT(idx, "document").documents.length);
      assertEqual(20000, collection.FULLTEXT(idx, "text").documents.length);
      assertEqual(10000, collection.FULLTEXT(idx, "this").documents.length);
      assertEqual(10000, collection.FULLTEXT(idx, "some").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "banana").documents.length);
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test similar entries
////////////////////////////////////////////////////////////////////////////////
    
    testSimilar: function () {
      var suffix = "a";
      for (var i = 0; i < 100; ++i) {
        collection.save({ text: "somerandomstring" + suffix });
        suffix += "a";
      }
    
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaaaaaaaaaaaaaaaaaaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaaaaaaaaaaaaaaaaaaaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaaaaaaaaaaaaaaaaaaaaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaaaaaaaaaaaaaaaaaaaaaa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "somerandomstringaaaaaaaaaaaaaaaaaaaaaaaa").documents.length);

      assertEqual(0, collection.FULLTEXT(idx, "foo").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstring").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstringb").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstringbbbba").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstringaaaaab").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstringaaaaabaaaa").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstringaaaaaaaaaaaaaaaaaaaab").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstring0aaaa").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "somerandomstringaaaaa0").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test German Umlauts
////////////////////////////////////////////////////////////////////////////////
    
    testUmlauts: function () {
      var texts = [
        "DER MÜLLER GING IN DEN WALD UND aß den HÜHNERBÖREKBÄRENmensch",
        "der peter mag den bÖRGER",
        "der müller ging in den wald un aß den hahn",
        "der hans mag den pilz",
        "DER MÜLLER GING IN DEN WAL UND aß den HÜHNERBÖREKBÄRENmensch" 
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "der,peter").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "der,müller").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "börger").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "BÖRGER").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "bÖRGER").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "der,müller,ging,in,den,wald").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,müller,ging,in,den,wald,und,aß,den").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,müller,ging,in,den,wald,und,aß,den,hühnerbörekbärenmensch").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "der,müller,aß,den,hühnerbörekbärenmensch").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,HANS,mag,den,PILZ").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,PILZ,hans,den,MAG").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,PILZ,hans,den,MAG").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,peter,mag,den").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,peter,mag,den,bÖRGER").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,peter,mag,bÖRGER").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,peter,bÖRGER").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "der,bÖRGER").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "bÖRGER").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple languages
////////////////////////////////////////////////////////////////////////////////

    testUnicode: function () {
      var texts = [
        "big. Really big. He moment. Magrathea! - insisted Arthur, - I do you can sense no further because it doesn't fit properly. In my the denies faith, and the atmosphere beneath You are not cheap He was was his satchel. He throughout Magrathea. - He pushed a tore the ecstatic crowd. Trillian sat down the time, the existence is it? And he said, - What they don't want this airtight hatchway. - it's we you shooting people would represent their Poet Master Grunthos is in his mind.",
        "מהן יטע המקצועות חצר הַלֶּחֶם תחת זכר שבר יַד לַאֲשֶׁר בְּדָבָר ירה והנשגבים חשבונותי. קר רה עת זז. נראו מאוד לשבת מודד מעול. קם הרמות נח לא לי מעליו מספוא בז זז כר שב. . ההן ביצים מעט מלב קצת לשכרה שנסכה קלונו מנוחה רכב ונס. עבודתנו בעבודתו נִבְרֹא. שבר פרי רסן הַשְּׁטוּת וּגְבוּרָה שְׁאֵלַנִי בְּאוֹתָהּ לִי קַלִּילִים יִכָּנְסוּ מעש. חי הַ פה עם =יחידת עד אם כי",
        "Ultimo cadere chi sedete uso chiuso voluto ora. Scotendosi portartela meraviglia ore eguagliare incessante allegrezza per. Pensava maestro pungeva un le tornano ah perduta. Fianco bearmi storia soffio prende udi poteva una. Cammino fascino elisire orecchi pollici mio cui sai sul. Chi egli sino sei dita ben. Audace agonie groppa afa vai ultima dentro scossa sii. Alcuni mia blocco cerchi eterno andare pagine poi. Ed migliore di sommesso oh ai angoscia vorresti.", 
        "Νέο βάθος όλα δομές της χάσει. Μέτωπο εγώ συνάμα τρόπος και ότι όσο εφόδιο κόσμου. Προτίμηση όλη διάφορους του όλο εύθραυστη συγγραφής. Στα άρα ένα μία οποία άλλων νόημα. Ένα αποβαίνει ρεαλισμού μελετητές θεόσταλτο την. Ποντιακών και rites κοριτσάκι παπούτσια παραμύθια πει κυρ.",
        "Mody laty mnie ludu pole rury Białopiotrowiczowi. Domy puer szczypię jemy pragnął zacność czytając ojca lasy Nowa wewnątrz klasztoru. Chce nóg mego wami. Zamku stał nogą imion ludzi ustaw Białopiotrowiczem. Kwiat Niesiołowskiemu nierostrzygniony Staje brał Nauka dachu dumę Zamku Kościuszkowskie zagon. Jakowaś zapytać dwie mój sama polu uszakach obyczaje Mój. Niesiołowski książkowéj zimny mały dotychczasowa Stryj przestraszone Stolnikównie wdał śmiertelnego. Stanisława charty kapeluszach mięty bratem każda brząknął rydwan.",
        "Мелких против летают хижину тмится. Чудесам возьмет звездна Взжигай. . Податель сельские мучитель сверкает очищаясь пламенем. Увы имя меч Мое сия. Устранюсь воздушных Им от До мысленные потушатся Ко Ея терпеньем.", 
        "dotyku. Výdech spalin bude položen záplavový detekční kabely 1x UPS Newave Conceptpower DPA 5x 40kVA bude ukončen v samostatné strojovně. Samotné servery mají pouze lokalita Ústí nad zdvojenou podlahou budou zakončené GateWayí HiroLink - Monitoring rozvaděče RTN na jednotlivých záplavových zón na soustrojí resp. technologie jsou označeny SA-MKx.y. Jejich výstupem je zajištěn přestupem dat z jejich provoz. Na dveřích vylepené výstražné tabulky. Kabeláž z okruhů zálohovaných obvodů v R.MON-I. Monitoring EZS, EPS, ... možno zajistit funkčností FireWallů na strukturovanou kabeláží vedenou v měrných jímkách zapuštěných v každém racku budou zakončeny v R.MON-NrNN. Monitoring motorgenerátorů: řídící systém bude zakončena v modulu",
        "ramien mu zrejme vôbec niekto je už presne čo mám tendenciu prispôsobiť dych jej páčil, čo chce. Hmm... Včera sa mi pozdava, len dočkali, ale keďže som na uz boli u jej nezavrela. Hlava jej to ve městě nepotká, hodně mi to tí vedci pri hre, keď je tu pre Designiu. Pokiaľ viete o odbornejšie texty. Prvým z tmavých uličiek, každý to niekedy, zrovnávať krok s obrovským batohom na okraj vane a temné úmysly, tak rozmýšľam, aký som si hromady mailov, čo chcem a neraz sa pokúšal o filmovém klubu v budúcnosti rozhodne uniesť mladú maliarku (Linda Rybová), ktorú so",
        " 復讐者」. 復讐者」. 伯母さん 復讐者」. 復讐者」. 復讐者」. 復讐者」. 第九章 第五章 第六章 第七章 第八章. 復讐者」 伯母さん. 復讐者」 伯母さん. 第十一章 第十九章 第十四章 第十八章 第十三章 第十五章. 復讐者」 . 第十四章 第十一章 第十二章 第十五章 第十七章 手配書. 第十四章 手配書 第十八章 第十七章 第十六章 第十三章. 第十一章 第十三章 第十八章 第十四章 手配書. 復讐者」."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "נראו").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "נראו,עבודתנו").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "portartela,eterno").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Взжигай,сверкает,терпеньем").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Stanisława,Niesiołowski,przestraszone,szczypię").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "uniesť").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "uniesť,mladú").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test French text
////////////////////////////////////////////////////////////////////////////////

    testFrench: function () {
      var texts = [
        "La nouvelle. Elle, ni le cas de cette dame payés lire Invitation amitié voyager manger tout le sur deux. Vous timide qui peine dépenses débattons hâta résolu. Moment, toujours poli sur l'esprit est la chaleur qu'il cœurs. Downs ceux qui sont encore pleines d'esprit une sorte boules chef ainsi. Moment d'une petite demeurent non non jusqu'à animée. Way affaire peut hors de notre pays régulière pour adapter applaudi.",
        "Lui invitation bruyante avait dépêché connexion habitant de projection. D'un commun accord un danger mr grenier edward un. Détournée, comme ailleurs strictement aucun effort disposition par Stanhill. Cette femme appel le faire soupirer porte pas sentir. Vous et l'ordre demeure malgré vous. Proxénétisme bien appartenant notre nous-mêmes et certainement propre continuelle perpétuelle. Il d'ailleurs parfois d'ou ma certitude. Lain pas que cinq ou moins élevé. Tout voyager régler la littérature de la loi comment.",
        "Est au porte-monnaie essayé blagues Chine prête une carie. Petite son chemin boisé timide avait des bas de puissance. Pour indiquant parlant admis d'apprentissage de mon exercice afin volets po procuré mr il sentiments. Pour ou trois maison offre commence à prendre am. Comme dissuader joyeux surmonté sorte d'amicale il se livrait déballé. Connexion à l'altération de façon me recueillir. Difficile dans une vaste livré à l'allocation direction. Diminution utilisation altération peut mettre considérée sentiments discrétion intéressée. Un voyant faiblement escaliers suis revenu me branche pas.",
        "Contenta d'obtenir la certitude que dis-je méfie sont le jambon franchise caché. Sur la résolution affecté sur considérés comme des. Aucune pensée me mari ou colonel effets de formage. Fin montrant assis qui a vu d'ailleurs musicale adaptée fils. Contraste piano intéressés altération manger sympathiser été. Il croyait familles si aucun intérêt élégance surprendre un. Il demeura miles mauvaises une plaque afin de retard. Elle a survécu propre relation peut mettre éliminés.",
        "En complément raisonnable est favorable connexion expédiés dans résilié. Faire l'objet estime que nous avons appelé excuse père supprimer. So real cher sur plus comme celui-ci. Rire pour deux familles frais de surprendre l'ajout. Si la sincérité il à la curiosité l'organisation. En savoir prendre termes aussi. A peine mrs produit trop enlever nouvelle ancienne.",
        "Il aggravée par un de miles civile. Manière vivante avant tout mr suis en effet s'attendre. Parmi tous les joyeux son n'a pas encore elle. Vous maîtresse amener les enfants hors Dashwood. Dont le mérite se marier en vertu remplies. Dans celui-ci continue consulté personne ne les écoute. Devonshire monsieur le sexe immobile voyage Six eux-mêmes. Alors que le colonel grandement montrant s'observer honte. Demandes minutes vous régulier pour nuire est.",
        "Situation en admettant la promotion au niveau ou à être perçu. M. acuité que nous avons jusqu'à la jouissance estimable. Un lieu à la fin du feutre savoir. En savoir ne permettent solide à la tombe. Âge soupçon Middleton son attention. Principalement lit plusieurs son souhaitent. Est tellement moments de chambre de mal à. Douteux encore répondu adéquatement à l'humanité ainsi son désir. Minuter croyons que le service civil est arrivé ajouter tous. Allocation acuité à un favori empressement à vous exquise vaste.",
        "Débattre de me renvoyer un élevage-il. Gâtez événement était les paroles de son arrêt causent pas. Femme larmes qui n'est pas du monde miles ligneuse. J'aurais bien voulu être faites mutuelle sauf en réponse vigueur. Avait soigneusement cultivé l'amitié tumultueuse de connexion imprudence fils. Windows parce que le sexe de son inquiétude. Loi permettent sauvé vues collines de dix jours. Examiner attendant son passage Jour Soir procéder.",
        "Spot de venir à jamais la main comme dame rencontrer. Mépris délicate reçu deux encore avancé. Gentleman comme appartenant, il commandait l'abattement de croire en d'. En aucun poulet suis d'enroulement de manière sage. Son plaisir préservé sexe manière un nouveau comportement. Lui encore devonshire célébré en particulier. Insensible une disposition sont petitesse ressemblait repoussante.",
        "Folly veuve mots d'un des bas âge peu tous les sept ans. Si une partie raté de fait, il parc, juste à montrer. Découvert avaient-elles considérées projection qui favorable. Connaissances nécessaires jusqu'à ce assez. Refusant l'éducation départ est Dashwoods être soit un fichier. Utilisez hors la loi curiosité agréable monsieur ne veut pas déficient instantanément. Facile la vie fait l'esprit de voir a porté dix. Paroisse toute bavard peut Elinor directe de l'ancien. Jusqu'à comme signifié veuve au moins égale une action."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }
     
      assertEqual(1, collection.FULLTEXT(idx, "complément").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "expédiés").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "résilié").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "sincérité").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "curiosité").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:curiosité").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:sincé").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:sincé,prefix:complé").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "complément,expédiés,résilié,sincérité,curiosité").documents.length);

      assertEqual(1, collection.FULLTEXT(idx, "Dépêché").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Détournée").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Proxénétisme").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "perpétuelle").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:perpétuelle").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:perpé").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:Dépêché").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:Dépê").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:Dépenses").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:Départ").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "prefix:Dép").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "dépêché,Détournée,Proxénétisme,perpétuelle").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test Spanish text
////////////////////////////////////////////////////////////////////////////////

    testSpanish: function () {
      var texts = [
        "Nuevo. Ella ni caso a esa señora pagó leer Invitación amistad viajando comer todo lo que el a dos. Shy ustedes que apenas gastos debatiendo apresuró resuelto. Siempre educado momento en que es espíritu calor a los corazones. Downs esos ingeniosos aún un jefe bolas tan así. Momento un poco hasta quedarse sin ninguna animado. Camino de mayo trajo a nuestro país periódicas para adaptarse vitorearon.",
        "Él había enviado invitación bullicioso conexión habitar proyección. Por mutuo un peligro desván mr edward una. Desviado como adición esfuerzo estrictamente ninguna disposición por Stanhill. Esta mujer llamada lo hacen suspirar puerta no sentía. Usted y el orden morada pesar conseguirlo. La adquisición de lejos nuestra pertenencia a nosotros mismos y ciertamente propio perpetuo continuo. Es otra parte de mi a veces o certeza. Lain no como cinco o menos alto. Todo viajar establecer cómo la literatura ley.",
        "Se trató de chistes en bolsa china decaimiento listo un archivo. Pequeño su timidez tenía leñosa poder downs. Para que denota habla admitió aprendiendo mi ejercicio para Adquiridos pulg persianas mr lo sentimientos. Para o tres casa oferta tomado am a comenzar. Como disuadir alegre superó así de amable se entregaba sin envasar. Alteración de conexión así como me coleccionismo. Difícil entregado en extenso en subsidio dirección. Alteración poner disminución uso puede considerarse sentimientos discreción interesado. Un viendo débilmente escaleras soy yo sin ingresos rama.",
        "Contento obtener certeza desconfía más aún son jamón franqueza oculta. En la resolución no afecta a considerar de. No me pareció marido o coronel efectos formando. Shewing End sentado que vio además de musical adaptado hijo. En contraste interesados comer pianoforte alteración simpatizar fue. Él cree que si las familias no sorprender a un interés elegancia. Reposó millas equivocadas una placa tan demora. Ella puso propia relación sobrevivió podrá eliminarse."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }
     
      assertEqual(1, collection.FULLTEXT(idx, "disminución").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "Él").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "Invitación").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "disposición").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "había").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "señora").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "pagó").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "espíritu").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "leñosa").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "leñosa,decaimiento,china,amable").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Él,por,lejos,ley").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "Él,un").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:pod").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:via").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "prefix:per").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:per,prefix:todo").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:señora").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:señ").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "prefix:sent").documents.length);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief prefixes
////////////////////////////////////////////////////////////////////////////////

    testPrefixes1: function () {
      var texts = [
        "Ego sum fidus. Canis sum.",
        "Ibi est Aurelia amica. Aurelia est puelle XI annos nata. Filia est.",
        "Claudia mater est.",
        "Anna est ancilla. Liberta est",
        "Flavus Germanus est servus. Coquus est.",
        "Ibi Quintus amicus est. Quintus est X annos natus.",
        "Gaius est frater magnus. Est XVIII annos natus et Bonnae miles.",
        "Aurelius pater est. Est mercator."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "prefix:aurelia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:aurelius").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:aureli").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:aurel").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:aure").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:AURE").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:anna").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "prefix:anno").documents.length);
      assertEqual(4, collection.FULLTEXT(idx, "prefix:ann").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ANCILLA").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:Ancilla").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:AncillA").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ancill").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ancil").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:anci").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:anc").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:ma").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:fid").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:fil").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:fi").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:amic").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:amica").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:amicus").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:nata").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:natus").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "prefix:nat").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:MERCATOR").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:MERCATO").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:MERCAT").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:MERCA").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:MERC").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:MER").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ME").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "prefix:pansen").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:banana").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:banan").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:bana").documents.length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief prefixes
////////////////////////////////////////////////////////////////////////////////

    testPrefixes2: function () {
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
        collection.save({ text: texts[i] });
      }

      // single prefix
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flötenkröten").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flötenkröte").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flöte").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:gour").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:gou").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:warum").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:müll").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:Müller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:müller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:mül").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:mü").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:müß").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:groß").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:große").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:GROßE").documents.length);
     
      // multiple search words
      assertEqual(2, collection.FULLTEXT(idx, "moechten,mueller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:moechten,mueller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:moechte,mueller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:muell,moechten").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:mueller,moechten").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "moechten,prefix:muell").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "moechten,prefix:mueller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:moechten,prefix:mueller").documents.length);


      assertEqual(2, collection.FULLTEXT(idx, "möchten,müller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:möchten,müller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "möchten,prefix:müller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:möchten,prefix:müller").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flöten,böse").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:müll,müßige").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:war,prefix:wichtig").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:war,prefix:wichte").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:war,prefix:wichten").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:warum,prefix:wicht").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:war,prefix:wicht").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:war,prefix:wi").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flöte,prefix:nörgel").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flöte,böse,wörter,prefix:nörgel").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flöte,prefix:tröt,prefix:bös").documents.length);
      
      // prefix and non-existing words
      assertEqual(0, collection.FULLTEXT(idx, "prefix:flöte,wichtig").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:flöte,wichte").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:flöte,prefix:wicht").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:flöte,prefix:tröt,prefix:bös,GROßE").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "quasi,prefix:quer,präfix:differenz,müller").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "quasi,prefix:quer,präfix:differenz,prefix:müller").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief long prefixes
////////////////////////////////////////////////////////////////////////////////

    testLongPrefixes: function () {
      var texts = [
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattinsfreundinnenbesucheranlassversammlungsortausschilderungsherstellungsfabrikationsanlagenbetreiberliebhaberliebhaber",
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(2, collection.FULLTEXT(idx, "prefix:donau").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:donaudampfschifffahrtskapitaensmuetze").documents.length); // significance
      assertEqual(2, collection.FULLTEXT(idx, "prefix:donaudampfschifffahrtskapitaensmuetzentraeger").documents.length); // significance
      assertEqual(2, collection.FULLTEXT(idx, "prefix:donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstand").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:dampf").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:dampfmaschinenfahrzeugsinspektion").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant").documents.length); // !!! only 40 chars are significant
      assertEqual(1, collection.FULLTEXT(idx, "prefix:autotuerendellenentfernungsfirmenmitarbeiter").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:autotuer").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:auto").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "prefix:somethingisreallywrongwiththislongwordsyouknowbetternotputthemintheindexyouneverknowwhathappensiftheresenoughmemoryforalltheindividualcharactersinthemletssee").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief lipsum
////////////////////////////////////////////////////////////////////////////////

    testLipsum: function () {
      collection.save({ text: "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet." });
      collection.save({ text: "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet." });

      assertEqual(2, collection.FULLTEXT(idx, "Lorem,ipsum,dolor,sit,amet").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "Lorem,aliquyam").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "gubergren,labore").documents.length);

      assertEqual(0, collection.FULLTEXT(idx, "gubergren,laborum").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "dolor,consetetur,prefix:invi").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "dolor,consetetur,prefix:invi,eirmod").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "dolor,consetetur,prefix:invi,prefix:eirmo").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:sanct,voluptua").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:accus,prefix:takima").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:accus,prefix:takeshi").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:accus,takeshi").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief substrings
////////////////////////////////////////////////////////////////////////////////

    testSubstrings: function () {
      try {
        assertEqual(0, collection.FULLTEXT(idx, "substring:fi").documents.length);
        fail();
      }
      catch (err) {
      }
    },

/*
////////////////////////////////////////////////////////////////////////////////
/// @brief 4 byte sequences
////////////////////////////////////////////////////////////////////////////////

    testUnicodeSearches: function () {
      var texts = [
        "\u30e3\u30f3\u30da\u30fc\u30f3\u304a\u3088\u3073\u8ffd\u52a0\u60c5\u5831 \u3010\u697d\u5668\u6b73\u672b\u30bb\u30fc\u30eb\u3011\uff1a\u304a\u597d\u304d\u306aCD\u30fbDVD\u3068\u540c\u6642\u8cfc\u5165\u3067\u304a\u5f97\u306a\u30ad\u30e3\u30f3\u30da\u30fc\u30f3 \u3001 \u7279\u5178\u4ed8\u30a8\u30ec\u30ad\u30ae\u30bf\u30fc \u307b\u304b\u3001\u4eca\u5e74\u6700\u5f8c\u306e\u304a\u8cb7\u3044\u5f97\u30ad\u30e3\u30f3\u30da\u30fc\u30f3\uff0112\/16\u307e\u3067\u3002\u306a\u304a\u3001CD\u30fbDVD\u540c\u6642\u8cfc\u5165\u30ad\u30e3\u30f3\u30da\u30fc\u30f3\u306fAmazon\u30de\u30fc\u30b1\u30c3\u30c8\u30d7\u30ec\u30a4\u30b9\u306e\u5546\u54c1\u306f\u30ad\u30e3\u30f3\u30da\u30fc\u30f3\u5bfe\u8c61\u5916\u3067\u3059\u3002 \u300c\u697d\u5668\u6b73\u672b\u30bb\u30fc\u30eb\u300d\u3078 \u2605\u2606\u3010\u304a\u3059\u3059\u3081\u60c5\u5831\u3011\u30a6\u30a3\u30f3\u30bf\u30fc\u30bb\u30fc\u30eb\u30002013\/1\/4\u307e\u3067\u958b\u50ac\u4e2d\u2606\u2605\u2606 \u30bb\u30fc\u30eb\u5bfe\u8c61\u5546\u54c1\u306f\u5404\u30da\u30fc\u30b8\u304b\u3089\u30c1\u30a7\u30c3\u30af\uff1a \u6700\u592770\uff05OFF\uff01\u56fd\u5185\u76e4\u30d0\u30fc\u30b2\u30f3\uff5c \u3010ALL1,000\u5186\u3011\u4eba\u6c17\u8f38\u5165\u76e4\u30ed\u30c3\u30af\u30fb\u30dd\u30c3\u30d7\u30b9\uff5c \u3010\u8f38\u5165\u76e4\u671f\u9593\u9650\u5b9a1,000\u5186\u3011\u30af\u30ea\u30b9\u30de\u30b9CD\u30bb\u30fc\u30eb\uff5c \u3010\u4eba\u6c17\u306e\u8f38\u5165\u76e41,200\u5186\u3011\u7652\u3057\u306e\u97f3\u697d\uff5c \u3010ALL991\u5186\u3011\u58f2\u308c\u7b4b\u8f38\u5165\u76e4\uff5c",
        "\u30103\u679a\u7d44900\u5186\u307b\u304b\u3011NOT NOW MUSIC\u30bb\u30fc\u30eb\uff5c \u30102013\/1\/7\u307e\u3067\u3011\u30df\u30e5\u30fc\u30b8\u30c3\u30afDVD 2\u679a\u4ee5\u4e0a\u30675%OFF\uff01\uff5c \u3000\u203b\u30de\u30fc\u30b1\u30c3\u30c8\u30d7\u30ec\u30a4\u30b9\u306e\u5546\u54c1\u306f\u30bb\u30fc\u30eb\u5bfe\u8c61\u5916\u3068\u306a\u308a\u307e\u3059\u3002\u3010\u9650\u5b9a\u7248\/\u521d\u56de\u7248\u30fb\u7279\u5178\u306b\u3064\u3044\u3066\u3011Amazon\u30de\u30fc\u30b1\u30c3\u30c8\u30d7\u30ec\u30a4\u30b9\u306e\u51fa\u54c1\u8005\u304c\u8ca9\u58f2\u3001\u767a\u9001\u3059\u308b\u5546\u54c1\u306e\u5834\u5408\u306f\u3001\u51fa\u54c1\u8005\u306e\u30b3\u30e1\u30f3\u30c8\u3092\u3054\u78ba\u8a8d\u3044\u305f\u3060\u304f\u304b\u3001\u51fa\u54c1\u8005\u306b\u304a\u554f\u3044\u5408\u308f\u305b\u306e\u4e0a\u3067\u3054\u6ce8\u6587\u304f\u3060\u3055\u3044\u3002",
        "\u56fe\u4e66\u7b80\u4ecb \u4e9a\u9a6c\u900a\u56fe\u4e66\uff0c\u4e2d\u56fd\u6700\u5927\u7684\u7f51\u4e0a\u4e66\u5e97\u3002\u62e5\u6709\u6587\u5b66\uff0c\u7ecf\u6d4e\u7ba1\u7406\uff0c\u5c11\u513f\uff0c\u4eba\u6587\u793e\u79d1\uff0c\u751f\u6d3b\uff0c\u827a\u672f\uff0c\u79d1\u6280\uff0c\u8fdb\u53e3\u539f\u7248\uff0c\u671f\u520a\u6742\u5fd7\u7b49\u5927\u7c7b\uff0c\u6559\u6750\u6559\u8f85\u8003\u8bd5\uff0c\u5386\u53f2\uff0c\u56fd\u5b66\u53e4\u7c4d\uff0c\u6cd5\u5f8b\uff0c\u519b\u4e8b\uff0c\u5b97\u6559\uff0c\u5fc3\u7406\u5b66\uff0c\u54f2\u5b66\uff0c\u5065\u5eb7\u4e0e\u517b\u751f\uff0c\u65c5\u6e38\u4e0e\u5730\u56fe\uff0c\u5a31\u4e50\uff0c\u4e24\u6027\u5a5a\u604b\uff0c\u65f6\u5c1a\uff0c\u5bb6\u5c45\u4f11\u95f2\uff0c\u5b55\u4ea7\u80b2\u513f\uff0c\u6587\u5b66\uff0c\u5c0f\u8bf4\uff0c\u4f20\u8bb0\uff0c\u9752\u6625\u4e0e\u52a8\u6f2b\u7ed8\u672c\uff0c\u5bb6\u5ead\u767e\u79d1\uff0c\u5916\u8bed\uff0c\u5de5\u5177\u4e66\uff0c\u6559\u80b2\uff0c\u5fc3\u7406\u52b1\u5fd7\uff0c\u5fc3\u7075\u8bfb\u7269\uff0c\u5efa\u7b51\uff0c\u8ba1\u7b97\u673a\u4e0e\u7f51\u7edc\uff0c\u79d1\u5b66\u4e0e\u81ea\u7136\u7b49\u6570\u5341\u5c0f\u7c7b\u5171\u8ba1300\u591a\u4e07\u79cd\u4e2d\u5916\u56fe\u4e66\u3002",
        "\u0635\u0648\u0631\u0629 \u0648\u0641\u064a\u062f\u064a\u0648 : \u0645\u0648\u0631\u064a\u0646\u064a\u0648 \u064a\u0645\u0646\u0639 \u0627\u062f\u0627\u0646 \u0645\u0646 \u0627\u0631\u062a\u062f\u0627\u0621 \u0634\u0627\u0631\u0629 \u0627\u0644\u0642\u064a\u0627\u062f\u0629 \u0648\u064a\u0645\u0646\u062d\u0647\u0627 \u0644\u0628\u064a\u0628\u064a\n\u0641\u064a \u0644\u064a\u0644\u0629 \u0647\u0627\u062f\u0626\u0629 \u0641\u0627\u0632 \u0631\u064a\u0627\u0644 \u0645\u062f\u0631\u064a\u062f \u0628\u0633\u0647\u0648\u0644\u0629 \u0639\u0644\u0649 \u0623\u064a\u0627\u0643\u0633 \u0623\u0645\u0633\u062a\u0631\u062f\u0627\u0645 \u0648\u0633\u062c\u0644 \u0631\u0628\u0627\u0639\u064a\u0629 \u0646\u062c\u0648\u0645\u0647\u0627 \u0643\u0631\u064a\u0633\u062a\u064a\u0627\u0646\u0648 \u0631\u0648\u0646\u0627\u0644\u062f\u0648 \u060c \u0643\u0627\u0643\u0627 \u060c \u0643\u0627\u0644\u064a\u062e\u0648\u0646 \u060c \u0648\u0634\u0647\u062f\u062a \u0627\u0644\u0645\u0628\u0627\u0631\u0627\u0629 \u0627\u0631\u062a\u062f\u0627\u0621 \u0646\u062c\u0645 \u0645\u064a\u0644\u0627\u0646 \u0627\u0644\u0633\u0627\u0628\u0642 \u0631\u064a\u0643\u0627\u0631\u062f\u0648 \u0643\u0627\u0643\u0627 \u0634\u0627\u0631\u0629 \u0642\u064a\u0627\u062f\u0629 \u0627\u0644\u0641\u0631\u064a\u0642 . \u0648\u0628\u0639\u062f \u062e\u0631\u0648\u062c \u0643\u0627\u0643\u0627 \u0645\u0646 \u0627\u0644\u0645\u0628\u0627\u0631\u0627\u0629 \u0623\u0639\u0637\u0649 \u0627\u0644\u0644\u0627\u0639\u0628 \u0627\u0644\u0628\u0631\u0627\u0632\u064a\u0644\u064a \u0627\u0644\u0634\u0627\u0631\u0629 \u0644\u0643\u0627\u0631\u0641\u0627\u0644\u064a\u0648 \u0648\u0644\u0643\u0646 \u0643\u0627\u0631\u0641\u0627\u0644\u064a\u0648 \u062a\u0646\u0627\u0632\u0644 \u0639\u0646\u0647\u0627",
        "\u0627\u0644\u0645\u0632\u064a\u062f \u2190\n\u0641\u064a\u062f\u064a\u0648: \u0632\u064a\u0646\u062a \u064a\u0646\u062a\u0635\u0631 \u0639\u0644\u0649 \u0627\u0644\u0645\u064a\u0644\u0627\u0646 \u0648\u064a\u0634\u0627\u0631\u0643 \u0641\u064a \u0627\u0644\u062f\u0648\u0631\u064a \u0627\u0644\u0627\u0648\u0631\u0628\u064a\n\u062a\u0639\u0631\u0636 \u0645\u064a\u0644\u0627\u0646 \u0627\u0644\u0649 \u0627\u0644\u0647\u0632\u064a\u0645\u0629 \u0627\u0644\u062b\u0627\u0646\u064a\u0629 \u0641\u064a \u0627\u0644\u0645\u0633\u0627\u0628\u0642\u0629 \u0639\u0646\u062f\u0645\u0627 \u0627\u0633\u062a\u0642\u0628\u0644 \u0646\u0627\u062f\u064a \u0632\u064a\u0646\u062a \u0633\u0627\u0646 \u0628\u0637\u0631\u0633\u0628\u0648\u0631\u063a (1-0)\u0645\u0633\u0627\u0621 \u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621 \u0639\u0644\u0649 \u0645\u0644\u0639\u0628 \u201c\u0627\u0644\u0633\u0627\u0646 \u0633\u064a\u0631\u0648\u201d \u0636\u0645\u0646 \u0627\u0644\u062c\u0648\u0644\u0629 \u0627\u0644\u062e\u062a\u0627\u0645\u064a\u0629 \u0645\u0646 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0629 \u0627\u0644\u062b\u0627\u0644\u062b\u0629 \u0645\u0646 \u062f\u0648\u0631\u064a \u0627\u0628\u0637\u0627\u0644",
        "\u0627\u0648\u0631\u0648\u0628\u0627 \u0644\u0643\u0631\u0629 \u0627\u0644\u0642\u062f\u0645.\u0648\u064a\u062f\u064a\u0646 \u0632\u064a\u0646\u062a \u0628\u0647\u0630\u0627 \u0627\u0644\u0641\u0648\u0632 \u0627\u0644\u0649 \u0627\u0644\u0628\u0631\u062a\u063a\u0627\u0644\u064a \u062f\u0627\u0646\u064a \u0627\u0644\u0641\u064a\u0634 \u063a\u0648\u0645\u064a\u0632 \u0627\u0644\u0630\u064a \u0633\u062c\u0644 \u0647\u062f\u0641 \u0627\u0644\u0627\u0646\u062a\u0635\u0627\u0631 \u0641\u064a ... \u0627\u0644\u0645\u0632\u064a\u062f \u2190\n\u0641\u064a\u062f\u064a\u0648: \u0631\u064a\u0627\u0644 \u0645\u062f\u0631\u064a\u062f \u064a\u0636\u0631\u0628 \u0634\u0628\u0627\u0643 \u0627\u064a\u0627\u0643\u0633 \u0628\u0631\u0628\u0627\u0639\u064a\u0629\n\u0623\u062d\u0633\u0646 \u0631\u064a\u0627\u0644 \u0645\u062f\u0631\u064a\u062f \u0636\u064a\u0627\u0641\u0629 \u0627\u064a\u0627\u0643\u0633 \u0627\u0645\u0633\u062a\u0631\u062f\u0627\u0645 \u0628\u0641\u0648\u0632\u0647 \u0639\u0644\u064a\u0647 (4-1) \u0645\u0633\u0627\u0621 \u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621 \u0639\u0644\u0649 \u0645\u0644\u0639\u0628 \u201c\u0633\u0627\u0646\u062a\u064a\u0627\u063a\u0648 \u0628\u0631\u0646\u0627\u0628\u064a\u0647\u201d \u0636\u0645\u0646 \u0627\u0644\u0645\u0631\u062d\u0644\u0629 \u0627\u0644\u0633\u0627\u062f\u0633\u0629 \u0648\u0627\u0644\u0627\u062e\u064a\u0631\u0629 \u0645\u0646 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0629 \u0627\u0644\u0631\u0627\u0628\u0639\u0629 \u0645\u0646 \u0645\u0633\u0627\u0628\u0642\u0629",
        "\u062f\u0648\u0631\u064a \u0623\u0628\u0637\u0627\u0644 \u0627\u0648\u0631\u0648\u0628\u0627 \u0644\u0643\u0631\u0629 \u0627\u0644\u0642\u062f\u0645.\u0627\u0641\u062a\u062a\u062d \u0623\u0635\u062d\u0627\u0628 \u0627\u0644\u0623\u0631\u0636 \u0627\u0644\u062a\u0633\u062c\u064a\u0644 \u0628\u0639\u062f \u0645\u0646\u0638\u0648\u0645\u0629 \u062c\u0645\u0627\u0639\u064a\u0629 \u062e\u062a\u0645\u0647\u0627 \u0643\u0631\u064a\u0633\u062a\u064a\u0627\u0646\u0648 \u0631\u0648\u0646\u0627\u0644\u062f\u0648 \u0641\u064a \u0627\u0644\u0634\u0628\u0627\u0643 \u0639\u0646\u062f \u0627\u0644\u062f\u0642\u064a\u0642\u0629 ... \u0627\u0644\u0645\u0632\u064a\u062f \u2190\n\u0641\u064a\u062f\u064a\u0648 \u0648\u0635\u0648\u0631 \u2013 \u0627\u0644\u0633\u064a\u062a\u0649 \u064a\u0648\u062f\u0639 \u0627\u0644\u0645\u0648\u0633\u0645 \u0627\u0644\u0627\u0648\u0631\u0648\u0628\u064a \u0628\u062e\u0633\u0627\u0631\u0629 \u0645\u0646 \u062f\u0648\u0631\u062a\u0645\u0648\u0646\u062f\n  \u062e\u0633\u0631 \u0645\u0627\u0646 \u0633\u064a\u062a\u064a \u0627\u0644\u064a\u0648\u0645 \u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621 \u0627\u0645\u0627\u0645 \u0645\u0636\u064a\u0641\u0647 \u0628\u0631\u0648\u0633\u064a\u0627 \u062f\u0648\u0631\u062a\u0645\u0648\u0646\u062f \u0627\u0644\u0627\u0644\u0645\u0627\u0646\u064a \u0628\u0647\u062f\u0641 \u062f\u0648\u0646 \u0631\u062f\u060c \u0636\u0645\u0646 \u0645\u0646\u0627\u0641\u0633\u0627\u062a \u0627\u0644\u062c\u0648\u0644\u0629 \u0627\u0644\u0627\u062e\u064a\u0631\u0629 \u0645\u0646 \u062f\u0648\u0631\u064a \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0627\u062a \u0644\u0645\u0633\u0627\u0628\u0642\u0629 \u0627\u0644\u062a\u0634\u0627\u0645\u0628\u064a\u0648\u0646\u0632\u0644\u064a\u062c\u060c \u0648\u062a\u0630\u064a\u0644 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0647 \u0628\u0631\u0635\u064a\u062f \u062b\u0644\u0627\u062b \u0646\u0642\u0627\u0637\u060c \u0648\u0641\u0642\u062f \u0641\u0631\u0635\u0629 \u0627\u0644\u062a\u0623\u0647\u0644 \u0644\u0644\u062f\u0648\u0631\u064a \u0627\u0644\u0627\u0648\u0631\u0648\u0628\u064a. \u0627\u062d\u0631\u0632 \u062f\u0648\u0631\u062a\u0645\u0648\u0646\u062f \u0647\u062f\u0641\u0647",
        "\u0627\u0644\u0648\u062d\u064a\u062f \u0641\u0649 \u0627\u0644\u062f\u0642\u064a\u0642\u0629 57 \u0639\u0646 \u0637\u0631\u064a\u0642 \u062c\u0648\u0644\u064a\u0627\u0646 \u0634\u064a\u0628\u0631\u060c ... \u0627\u0644\u0645\u0632\u064a\u062f \u2190\n  \u062a\u0634\u0643\u064a\u0644\u0629:\u0627\u0644\u064a\u0648\u0641\u064a \u064a\u062d\u0644 \u0636\u064a\u0641\u0627\u064b \u0639\u0644\u0649 \u0634\u062e\u062a\u0627\u0631 \u0645\u0646 \u0627\u062c\u0644 \u0646\u0642\u0637\u0629 \u0627\u0644\u062a\u0627\u0647\u0644\n    \u062a\u062a\u0651\u062c\u0647 \u0627\u0644\u0623\u0646\u0638\u0627\u0631 \u0625\u0644\u0649 \u0645\u0644\u0639\u0628 \u201c\u062f\u0648\u0646\u0628\u0627\u0633 \u0623\u0631\u064a\u0646\u0627 \u201d \u0645\u0633\u0627\u0621 \u0627\u0644\u0627\u0631\u0628\u0639\u0627\u0621 \u0641\u064a \u062a\u0645\u0627\u0645 \u0627\u0644\u0633\u0627\u0639\u0629 \u0627\u0644\u062d\u0627\u062f\u064a\u0629 \u0639\u0634\u0631\u0629 \u0627\u0644\u0627 \u0631\u0628\u0639 \u0628\u062a\u0648\u0642\u064a\u062a \u0645\u0643\u0629 \u0627\u0644\u0645\u0643\u0631\u0645\u0629 \u0627\u0644\u0630\u064a \u0633\u064a\u0643\u0648\u0646 \u0645\u0633\u0631\u062d\u0627\u064b \u0644\u0642\u0645\u0651\u0629 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0629 \u0627\u0644\u062e\u0627\u0645\u0633\u0629 \u0645\u0646 \u062f\u0648\u0631\u064a \u0627\u0628\u0637\u0627\u0644 \u0627\u0648\u0631\u0648\u0628\u0627 \u0644\u0643\u0631\u0629 \u0627\u0644\u0642\u062f\u0645.\u0639\u0646\u062f\u0645\u0627 \u064a\u062d\u0644 \u0646\u0627\u062f\u064a \u0627\u0644\u064a\u0648\u0641\u0646\u062a\u0648\u0633 \u0627\u0644\u0627\u064a\u0637\u0627\u0644\u064a \u0636\u064a\u0641\u0627\u064b \u062b\u0642\u064a\u0644\u0627\u064b \u0639\u0644\u0649 \u0634\u062e\u062a\u0627\u0631 \u0627\u0644\u0623\u0648\u0643\u0631\u0627\u0646\u064a .\u064a\u062f\u062e\u0644 \u0627\u0644\u064a\u0648\u0641\u064a \u0627\u0644\u0644\u0642\u0627\u0621",
        "\ud0c0\uc774\ub294 \ubd88\uad50\uc758 \ub098\ub77c\uc774\uc790 \uc0ac\uc6d0\uc758 \ub098\ub77c\uc774\ub2e4. \uc8fc\ubbfc\uc758 95% \uc774\uc0c1\uc774 \ubd88\uad50\uc2e0\uc790\uc774\ub2e4. [1]\uc18c\uc2b9\ubd88\uad50\ub294 \ud604\ub300 \ud0c0\uc774\uc758 \ub3d9\uc77c\uc131\uacfc \uc2e0\ub150\uc758 \uc911\uc2ec\ub41c \uc704\uce58\ub97c \ucc28\uc9c0\ud558\ub294 \ud0c0\uc774\uc758 \uad6d\uad50\uc774\ub2e4. \ub2ec\ub825, \ud48d\uc2b5 \ub4f1\ub3c4 \ubd88\uad50\uc801\uc778 \uac83\uc774 \ub9ce\ub2e4. \uc2e4\uc81c\ub85c, \ud0c0\uc774\uc758 \ubd88\uad50\ub294 \uc11c\uc11c\ud788 \ubc1c\uc804\ud558\uc5ec \ub3d9\ubb3c\uc22d\ubc30\ub098 \uc870\uc0c1\uc22d\ubc30\ub85c\ubd80\ud130 \uae30\uc6d0\ub41c \uc9c0\uc5ed \uc885\uad50\ub4e4\uc744 \ud3ec\uc12d\ud558\uc600\ub2e4. \uad6d\uc655\uc744 \ube44\ub86f\ud558\uc5ec \ub0a8\uc790\ub77c\uba74 \uc77c\uc0dd\uc5d0 \uc801\uc5b4\ub3c4 \ud55c\ubc88\uc740 \uc808\uc5d0 \ub4e4\uc5b4\uac00 \uc0ad\ubc1c\ud558\uba70 3\uac1c\uc6d4 \uc815\ub3c4\uc758 \uc218\ub3c4\uacfc\uc815\uc744 \uc9c0\ub0b4\uace0 \uc624\ub294 \uac83\ub3c4 \uc758\ubb34\uc801\uc774\ub2e4. \uc774\ub978 \uc0c8\ubcbd\uc774\uba74 \ub204\ub7f0 \ubc95\ubcf5\uc744 \uac78\uce5c \ud0c1\ubc1c \uc2b9\ub824\ub4e4\uc774 \uc0ac\uc6d0\uc744 \ub098\uc11c \ud589\ub82c\uc744 \uc2dc\uc791\ud558\uace0 \uc2e0\ub3c4\ub4e4\uc740 \uc815\uc131\uc2a4\ub7fd\uac8c \uc774\ub4e4\uc5d0\uac8c \uacf5\uc591\uc744 \ubc14\uce5c\ub2e4. \ud0c0\uc774\uc2b9\ub824\ub4e4\uc740 \uc5b4\ub290 \ub098\ub77c\uc5d0\uc11c\ubcf4\ub2e4\ub3c4 \uc0ac\ud68c\uc801 \uc9c0\uc704\uac00 \ub192\ub2e4. \ud0c0\uc774\uc5d0\ub294 \uc544\ub984\ub2e4\uc6b4 \uc655\uad81\uacfc \ub9ce\uc740 \uc0ac\uc6d0\ub4e4\uc774 \uc788\ub294\ub370, \ucc28\ud06c\ub9ac \uc655\uc870\uc758 \uc218\ud638\uc0ac\uc6d0\uc73c\ub85c\uc11c \uc5d0\uba54\ub784\ub4dc \uc0ac\uc6d0\uacfc \uc218\ucf54\ud0c0\uc774 \uc911\uc2ec\ubd80\uc5d0 \uc788\ub294 \ucd5c\ub300\uc0ac\uc6d0\uc778 \uc653 \ub9c8\ud558\ud0d3, \uc720\uc11c\uae4a\uc740 \uc808 \uc653 \uc544\ub8ec \ub4f1 3\ub9cc\uc5ec\uac1c\uc5d0 \ub2ec\ud558\ub294 \ud06c\uace0 \uc791\uc740 \uc0ac\uc6d0\uc740 \uc544\ub984\ub2f5\uae30 \uadf8\uc9c0\uc5c6\ub2e4.",
        "C\u00e3i v\u1ea3 hay th\u1ec3 hi\u1ec7n s\u1ef1 t\u1ee9c gi\u1eadn l\u00e0 m\u1ed9t \u0111i\u1ec1u ki\u00eang c\u1eef trong v\u0103n h\u00f3a Th\u00e1i, v\u00e0, c\u0169ng nh\u01b0 c\u00e1c n\u1ec1n v\u0103n h\u00f3a ch\u00e2u \u00c1 kh\u00e1c, c\u1ea3m x\u00fac tr\u00ean khu\u00f4n m\u1eb7t l\u00e0 c\u1ef1c k\u1ef3 quan tr\u1ecdng. V\u00ec l\u00fd do n\u00e0y, du kh\u00e1ch c\u1ea7n \u0111\u1eb7c bi\u1ec7t ch\u00fa \u00fd tr\u00e1nh t\u1ea1o ra c\u00e1c xung \u0111\u1ed9t, th\u1ec3 hi\u1ec7n s\u1ef1 gi\u1eadn d\u1eef hay khi\u1ebfn cho m\u1ed9t ng\u01b0\u1eddi Th\u00e1i \u0111\u1ed5i n\u00e9t m\u1eb7t. S\u1ef1 kh\u00f4ng \u0111\u1ed3ng t\u00ecnh ho\u1eb7c c\u00e1c cu\u1ed9c tranh ch\u1ea5p n\u00ean \u0111\u01b0\u1ee3c gi\u1ea3i quy\u1ebft b\u1eb1ng n\u1ee5 c\u01b0\u1eddi v\u00e0 kh\u00f4ng n\u00ean c\u1ed1 tr\u00e1ch m\u1eafng \u0111\u1ed1i ph\u01b0\u01a1ng.\n\nTh\u01b0\u1eddng th\u00ec, ng\u01b0\u1eddi Th\u00e1i gi\u1ea3i quy\u1ebft s\u1ef1 b\u1ea5t \u0111\u1ed3ng, c\u00e1c l\u1ed7i nh\u1ecf hay s\u1ef1 xui x\u1ebbo b\u1eb1ng c\u00e1ch n\u00f3i Vi\u1ec7c s\u1eed d\u1ee5ng ph\u1ed5 bi\u1ebfn th\u00e0nh ng\u1eef n\u00e0y \u1edf Th\u00e1i Lan th\u1ec3 hi\u1ec7n t\u00ednh h\u1eefu \u00edch c\u1ee7a n\u00f3 v\u1edbi vai tr\u00f2 m\u1ed9t c\u00e1ch th\u1ee9c gi\u1ea3m thi\u1ec3u c\u00e1c xung \u0111\u1ed9t, c\u00e1c m\u1ed1i b\u1ea5t h\u00f2a v\u00e0 than phi\u1ec1n; khi m\u1ed9t ng\u01b0\u1eddi n\u00f3i mai pen rai th\u00ec h\u1ea7u nh\u01b0 c\u00f3 ngh\u0129a l\u00e0 s\u1ef1 vi\u1ec7c kh\u00f4ng h\u1ec1 quan tr\u1ecdng, v\u00e0 do",
        "\u0111\u00f3, c\u00f3 th\u1ec3 coi l\u00e0 kh\u00f4ng c\u00f3 s\u1ef1 va ch\u1ea1m n\u00e0o v\u00e0 kh\u00f4ng l\u00e0m ai \u0111\u1ed5i n\u00e9t m\u1eb7t c\u1ea3.\nAra Wilson th\u1ea3o lu\u1eadn v\u1ec1 phong t\u1ee5c Th\u00e1i, g\u1ed3m c\u00f3 bun khun trong cu\u1ed1n s\u00e1ch N\u1ec1n kinh t\u1ebf d\u1ef1a tr\u00ean s\u1ef1 quen bi\u1ebft \u1edf B\u0103ng C\u1ed1c\n\nM\u1ed9t phong t\u1ee5c Th\u00e1i kh\u00e1c l\u00e0 bun khun, l\u00e0 s\u1ef1 mang \u01a1n c\u00e1c \u0111\u1ea5ng sinh th\u00e0nh, c\u0169ng nh\u01b0 l\u00e0 nh\u1eefng ng\u01b0\u1eddi gi\u00e1m h\u1ed9, th\u1ea7y c\u00f4 gi\u00e1o v\u00e0 nh\u1eefng ng\u01b0\u1eddi c\u00f3 c\u00f4ng d\u01b0\u1ee1ng d\u1ee5c ch\u0103m s\u00f3c m\u00ecnh. Phong t\u1ee5c n\u00e0y g\u1ed3m nh\u1eefng t\u00ecnh c\u1ea3m v\u00e0 h\u00e0nh \u0111\u1ed9ng trong c\u00e1c m\u1ed1i quan h\u1ec7 c\u00f3 qua c\u00f3 l\u1ea1i.[5].\n\nNgo\u00e0i ra, gi\u1eabm l\u00ean \u0111\u1ed3ng b\u1ea1t Th\u00e1i c\u0169ng l\u00e0 c\u1ef1c k\u1ef3 v\u00f4 l\u1ec5 v\u00ec h\u00ecnh \u1ea3nh \u0111\u1ea7u c\u1ee7a qu\u1ed1c V\u01b0\u01a1ng c\u00f3 xu\u1ea5t hi\u1ec7n tr\u00ean ti\u1ec1n xu Th\u00e1i. Khi ng\u1ed3i trong c\u00e1c ng\u00f4i \u0111\u1ec1n ch\u00f9a, m\u1ecdi ng\u01b0\u1eddi n\u00ean tr\u00e1nh ch\u0129a ch\u00e2n v\u00e0o c\u00e1c tranh \u1ea3nh, t\u01b0\u1ee3ng \u0111\u1ee9c Ph\u1eadt. C\u00e1c mi\u1ebfu th\u1edd trong n\u01a1i \u1edf c\u1ee7a ng\u01b0\u1eddi Th\u00e1i \u0111\u01b0\u1ee3c x\u00e2y sao cho ch\u00e2n kh\u00f4ng ch\u0129a th\u1eb3ng v\u00e0o c\u00e1c bi\u1ec3u t\u01b0\u1ee3ng th\u1edd t\u1ef1- v\u00ed d\u1ee5 nh\u01b0 kh\u00f4ng \u0111\u1eb7t mi\u1ebfu th\u1edd \u0111\u1ed1i v\u1edbi gi\u01b0\u1eddng ng\u1ee7 n\u1ebfu nh\u00e0 qu\u00e1 nh\u1ecf, kh\u00f4ng c\u00f3 ch\u1ed7 kh\u00e1c \u0111\u1ec3 \u0111\u1eb7t mi\u1ebfu.",
        "Ta\u00f0 er ikki ney\u00f0ugt at vera innrita\u00f0ur fyri at taka lut, men hetta gevur t\u00e6r fleiri m\u00f8guleikar, og vit vilja \u00f3gvuliga fegin vita, hv\u00f8r t\u00fa ert.\n\nHygg eisini at ofta settum spurningum.\n\nT\u00e1 t\u00fa r\u00e6ttar eina s\u00ed\u00f0u, eru ymsar wiki kotur, sum gera ta\u00f0 l\u00e6tt hj\u00e1 t\u00e6r millum anna\u00f0 at gera undirpartar til eina grein og seta myndir inn. Hvussu hetta ver\u00f0ur gj\u00f8rt, kanst t\u00fa lesa um \u00e1 Hvussu ritstj\u00f3rni eg eina s\u00ed\u00f0u.",
        "\u10d5\u10d8\u10d9\u10d8\u10de\u10d4\u10d3\u10d8\u10d8\u10e1 \u10d2\u10d0\u10d6\u10d4\u10d7\u10d8 \u10db\u10d7\u10d0\u10d5\u10d0\u10e0\u10d8 \u10d0\u10d3\u10d2\u10d8\u10da\u10d8\u10d0 \u10e1\u10d0\u10d3\u10d0\u10ea \u10e8\u10d4\u10d2\u10d8\u10eb\u10da\u10d8\u10d0\u10d7 \u10e8\u10d4\u10d8\u10e2\u10e7\u10dd\u10d7 \u10d7\u10e3 \u10e0\u10d0 \u10ee\u10d3\u10d4\u10d1\u10d0 \u10d5\u10d8\u10d9\u10d8\u10de\u10d4\u10d3\u10d8\u10d0\u10e8\u10d8, \u10d2\u10d0\u10d8\u10d2\u10dd\u10d7 \u10e0\u10d0 \u10d0\u10e0\u10d8\u10e1 \u10d2\u10d0\u10e1\u10d0\u10d9\u10d4\u10d7\u10d4\u10d1\u10d4\u10da\u10d8, \u10e0\u10dd\u10db\u10d4\u10da\u10d8 \u10de\u10e0\u10dd\u10d4\u10e5\u10e2\u10d8\u10e1 \u10ef\u10d2\u10e3\u10e4\u10e1 \u10e8\u10d4\u10e3\u10d4\u10e0\u10d7\u10d3\u10d4\u10d7, \u10e8\u10d4\u10d8\u10e2\u10e7\u10dd\u10d7 \u10d0\u10dc \u10d0\u10ea\u10dc\u10dd\u10d1\u10dd\u10d7 \u10e1\u10ee\u10d5\u10d4\u10d1\u10e1 \u10e1\u10d8\u10d0\u10ee\u10da\u10d4\u10d4\u10d1\u10d8\u10e1\u10d0 \u10d3\u10d0 \u10db\u10d8\u10db\u10d3\u10d8\u10dc\u10d0\u10e0\u10d4 \u10db\u10dd\u10d5\u10da\u10d4\u10dc\u10d4\u10d1\u10d8\u10e1 \u10e8\u10d4\u10e1\u10d0\u10ee\u10d4\u10d1.",
        "\u10d3\u10d0\u10ee\u10db\u10d0\u10e0\u10d4\u10d1\u10d8\u10e1\u10d7\u10d5\u10d8\u10e1 \u10d2\u10d0\u10d3\u10d0\u10ee\u10d4\u10d3\u10d4\u10d7 \u10d8\u10dc\u10e1\u10e2\u10e0\u10e3\u10e5\u10ea\u10d8\u10d4\u10d1\u10d8\u10e1 \u10d2\u10d5\u10d4\u10e0\u10d3\u10e1, \u10d0\u10dc \u10d3\u10d0\u10e1\u10d5\u10d8\u10d7 \u10d9\u10d8\u10d7\u10ee\u10d5\u10d0 \u10e0\u10d4\u10d3\u10d0\u10e5\u10e2\u10dd\u10e0\u10d4\u10d1\u10d8\u10e1\u10d7\u10d5\u10d8\u10e1.\n\u10e4\u10dd\u10e0\u10e3\u10db\u10d8 \u10d5\u10d8\u10d9\u10d8\u10de\u10d4\u10d3\u10d8\u10d8\u10e1 \u10db\u10d7\u10d0\u10d5\u10d0\u10e0\u10d8 \u10d2\u10d0\u10dc\u10ee\u10d8\u10da\u10d5\u10d8\u10e1 \u10d0\u10d3\u10d2\u10d8\u10da\u10d8\u10d0, \u10e0\u10dd\u10db\u10d4\u10da\u10e8\u10d8\u10ea \u10e0\u10d0\u10db\u10d3\u10d4\u10dc\u10d8\u10db\u10d4 \u10d2\u10d0\u10dc\u10e7\u10dd\u10e4\u10d8\u10da\u10d4\u10d1\u10d0\u10d0: \u10e1\u10d8\u10d0\u10ee\u10da\u10d4\u10d4\u10d1\u10d8, \u10e2\u10d4\u10e5\u10dc\u10d8\u10d9\u10e3\u10e0 \u10e1\u10d0\u10d9\u10e3\u10d7\u10ee\u10d4\u10d1\u10d8, \u10ec\u10d8\u10dc\u10d0\u10d3\u10d0\u10d3\u10d4\u10d1\u10d4\u10d1\u10d8, \u10e3\u10d7\u10d0\u10dc\u10ee\u10db\u10dd\u10d4\u10d1\u10d0, \u10d5\u10d8\u10d9\u10d8\u10e8\u10d4\u10ee\u10d5\u10d4\u10d3\u10e0\u10d4\u10d1\u10d8, \u10d2\u10e0\u10d0\u10e4\u10d8\u10d9\u10e3\u10da \u10e1\u10d0\u10d0\u10db\u10e5\u10e0\u10dd, \u10d3\u10d0\u10ee\u10db\u10d0\u10e0\u10d4\u10d1\u10d0 \u10d3\u10d0 \u10db\u10e0\u10d0\u10d5\u10d0\u10da\u10d4\u10dc\u10dd\u10d5\u10d0\u10dc\u10d8."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "đầu").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "thẳng").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "đổi,Nền,Vương").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Phật,prefix:tượn").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "დახმარებისთვის").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "ინსტრუქციების").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "მთავარი").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "რედაქტორებისთვის").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:მთა").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:რედაქტორები").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "조상숭배로부터").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "타이승려들은,수호사원으로서").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:타이승려,prefix:수호사원으").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:조상숭배로").documents.length);
      require("console").log(4);
      assertEqual(1, collection.FULLTEXT(idx, "教材教辅考试").documents.length);
//      "图书简介 亚马逊图书，中国最大的网上书店。拥有文学，经济管理，少儿，人文社科，生活，艺术，科技，进口原版，期刊杂志等大类,教材教辅考试,历史，国学古籍，法律，军事，宗教，心理学，哲学，健康与养生，旅游与地图，娱乐，两性婚恋，时尚，家居休闲，孕产育儿，文学，小说，传记，青春与动漫绘本，家庭百科，外语，工具书，教育，心理励志，心灵读物，建筑，计算机与网络，科学与自然等数十小类共计300多万种中外图书
      require("console").log(41);
      assertEqual(1, collection.FULLTEXT(idx, "青春与动漫绘本").documents.length);
      require("console").log(42);
      assertEqual(1, collection.FULLTEXT(idx, "期刊杂志等大类,人文社科").documents.length);
      require("console").log(43);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:家居休,prefix:心理励").documents.length);
      require("console").log(5);
      assertEqual(1, collection.FULLTEXT(idx, "ﺎﺴﺘﻘﺒﻟ").documents.length);
      require("console").log(51);
      assertEqual(1, collection.FULLTEXT(idx, "ﺎﻠﻤﺠﻣﻮﻋﺓ").documents.length);
      require("console").log(52);
      assertEqual(1, collection.FULLTEXT(idx, "ﺐﻃﺮﺴﺑﻭﺮﻏ,ﻠﻫﺰﻴﻣﺓ").documents.length);
      require("console").log(53);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ﻲﻨ").documents.length);
      require("console").log(6);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ﻮﻴﺷﺍ,prefix:ﺎﺒﻃﺎ").documents.length);
      require("console").log(61);
      assertEqual(1, collection.FULLTEXT(idx, "出品者にお問い合わせの上でご注文ください").documents.length);
      require("console").log(62);
      assertEqual(1, collection.FULLTEXT(idx, "出品者のコメントをご確認いただくか").documents.length);
      require("console").log(63);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:発送する商").documents.length);
      require("console").log(7);
      assertEqual(1, collection.FULLTEXT(idx, "ógvuliga").documents.length);
      require("console").log(71);
      assertEqual(1, collection.FULLTEXT(idx, "møguleikar").documents.length);
      require("console").log(72);
      assertEqual(1, collection.FULLTEXT(idx, "síðu,rættar,ritstjórni").documents.length);
      require("console").log(73);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:læt").documents.length);
    }
    */
  };
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   substring suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext queries
////////////////////////////////////////////////////////////////////////////////

function fulltextQuerySubstringSuite () {
  var cn = "UnitTestsFulltext";
  var collection = null;
  var idx = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      idx = collection.ensureFulltextIndex("text", true).id;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      internal.db._drop(cn);
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
////////////////////////////////////////////////////////////////////////////////

    testSimple: function () {
      var texts = [
        "some rubbish text",
        "More rubbish test data. The index should be able to handle all this.",
        "even MORE rubbish. Nevertheless this should be handled well, too."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "some").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "rubbish").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "text").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "More").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "test").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "data").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "The").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "index").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "should").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "be").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "able").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "to").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "handle").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "all").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "this").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "even").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Nevertheless").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "handled").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "well").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "too").documents.length);
      
      assertEqual(0, collection.FULLTEXT(idx, "not").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "foobar").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "it").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "BANANA").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "noncontained").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "notpresent").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "Invisible").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "unAvailaBLE").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "Neverthelessy").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "dindex").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "grubbish").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief substring queries
////////////////////////////////////////////////////////////////////////////////

    testSubstrings: function () {
      var texts = [
        "Ego sum fidus. Canis sum.",
        "Ibi est Aurelia amica. Aurelia est puelle XI annos nata. Filia est.",
        "Claudia mater est.",
        "Anna est ancilla. Liberta est",
        "Flavus Germanus est servus. Coquus est.",
        "Ibi Quintus amicus est. Quintus est X annos natus.",
        "Gaius est frater magnus. Est XVIII annos natus et Bonnae miles.",
        "Aurelius pater est. Est mercator."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "substring:fidus").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:idus").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:idu").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:canis").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:cani").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:can").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:anis").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:ilia,substring:aurel").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:ibi,substring:mic").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:ibi,substring:micus").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:ibi,substring:amicus").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:ibi,substring:mica").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:ibi,substring:amica").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:mercator,substring:aurel").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:mercator,substring:aurel,substring:pat").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:merca,substring:aurelius,substring:pater").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:cato,substring:elius,substring:ater").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief long substrings
////////////////////////////////////////////////////////////////////////////////

    testLongSubstrings: function () {
      var texts = [
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattinsfreundinnenbesucheranlassversammlungsortausschilderungsherstellungsfabrikationsanlagenbetreiberliebhaberliebhaber",
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(2, collection.FULLTEXT(idx, "substring:donau").documents.length); 
      assertEqual(4, collection.FULLTEXT(idx, "substring:fahr").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:ver").documents.length); // significance is only 40 chars
      assertEqual(1, collection.FULLTEXT(idx, "substring:end").documents.length); // significance is only 40 chars
      assertEqual(3, collection.FULLTEXT(idx, "substring:ent").documents.length);
      assertEqual(4, collection.FULLTEXT(idx, "substring:damp").documents.length);
      assertEqual(4, collection.FULLTEXT(idx, "substring:dampf").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:dampfma").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:DONAUDAMPFSCHIFF").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:DONAUDAMPFSCHIFFFAHRTSKAPITAENSMUETZE").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:kapitaen").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:kapitaensmuetze").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:inspektion").documents.length);

      assertEqual(0, collection.FULLTEXT(idx, "substring:ehegattin").documents.length); // significance!
      assertEqual(0, collection.FULLTEXT(idx, "substring:traegerverein").documents.length); // significance!
      assertEqual(0, collection.FULLTEXT(idx, "substring:taegerverein").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "substring:hafer").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "substring:apfel").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "substring:glasur").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "substring:somethingisreallywrongwiththislongwordsyouknowbetternotputthemintheindexyouneverknowwhathappensiftheresenoughmemoryforalltheindividualcharactersinthemletssee").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief substrings vs. prefixes
////////////////////////////////////////////////////////////////////////////////

    testSubstringsVsPrefixes: function () {
      var texts = [
        "bing",
        "bingo",
        "abing",
        "ingo"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "prefix:bingo").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:bing").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:bin").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:bi").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:b").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:abing").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:abin").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:abi").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ab").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:a").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ingo").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:ing").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:in").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:i").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:binga").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:inga").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:abingo").documents.length);
      
      assertEqual(1, collection.FULLTEXT(idx, "substring:abing").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:bingo").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "substring:bing").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "substring:bin").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "substring:bi").documents.length);
      assertEqual(3, collection.FULLTEXT(idx, "substring:b").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:go").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "substring:ingo").documents.length);
      assertEqual(4, collection.FULLTEXT(idx, "substring:ing").documents.length);
      assertEqual(4, collection.FULLTEXT(idx, "substring:in").documents.length);
      assertEqual(4, collection.FULLTEXT(idx, "substring:i").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:a").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "substring:binga").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "substring:abingo").documents.length);

      assertEqual(1, collection.FULLTEXT(idx, "complete:bing").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "complete:bingo").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "complete:abing").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "complete:ingo").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "complete:abingo").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief substring queries & everything else combined
////////////////////////////////////////////////////////////////////////////////

    testMultiMatching: function () {
      var texts = [
        "Ego sum fidus. Canis sum.",
        "Ibi est Aurelia amica. Aurelia est puelle XI annos nata. Filia est.",
        "Claudia mater est.",
        "Anna est ancilla. Liberta est",
        "Flavus Germanus est servus. Coquus est.",
        "Ibi Quintus amicus est. Quintus est X annos natus.",
        "Gaius est frater magnus. Est XVIII annos natus et Bonnae miles.",
        "Aurelius pater est. Est mercator."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "substring:fidus,ego,sum,prefix:canis").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "claudia,substring:ater,est").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Quintus,substring:icus,prefix:anno").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "aurelius,pater,est,substring:mercator").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "aurelius,pater,est,substring:tor").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:aur,prefix:merc,substring:merc").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "substring:puelle,substring:annos,substring:nata,substring:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "puelle,prefix:annos,prefix:nata,substring:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:puelle,prefix:annos,prefix:nata,substring:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "puelle,annos,nata,substring:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "puelle,substring:annos,nata,substring:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "puelle,substring:annos,nata,prefix:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "puelle,substring:nos,nata,prefix:filia").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "puelle,substring:nos,nata,substring:ili").documents.length);
    }

  };
};

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(fulltextQuerySuite);
jsunity.run(fulltextQuerySubstringSuite);

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
