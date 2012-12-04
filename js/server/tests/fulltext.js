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
    }

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
    },

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
