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
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Reliefpfeiler feilen reihenweise Pfeilreifen"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "AUTOTUERENDELLENentfernungsfirmenmitarbeiterverguetungsbewerter").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin").documents.length);

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
      /*
     require("console").log("adx0"); 
      assertEqual(2, collection.FULLTEXT(idx, "moechten,prefix:muell").documents.length);
     require("console").log("adx1"); 
      assertEqual(2, collection.FULLTEXT(idx, "moechten,prefix:mueller").documents.length);
     require("console").log("adx2"); 
      assertEqual(2, collection.FULLTEXT(idx, "prefix:moechten,prefix:mueller").documents.length);


      assertEqual(2, collection.FULLTEXT(idx, "möchten,müller").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:möchten,müller").documents.length);
     require("console").log("adx0"); 
      assertEqual(2, collection.FULLTEXT(idx, "möchten,prefix:müller").documents.length);
     require("console").log("adx"); 
      assertEqual(2, collection.FULLTEXT(idx, "prefix:möchten,prefix:müller").documents.length);
     require("console").log("adx1"); 
      assertEqual(1, collection.FULLTEXT(idx, "prefix:flöten,böse").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:müll,müßige").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:war,prefix:wichtig").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:war,prefix:wichte").documents.length);
     require("console").log("ad"); 
      assertEqual(2, collection.FULLTEXT(idx, "prefix:warum,prefix:wicht").documents.length);
     require("console").log("ad1"); 
      assertEqual(2, collection.FULLTEXT(idx, "prefix:war,prefix:wicht").documents.length);
     require("console").log("ae"); 
      assertEqual(2, collection.FULLTEXT(idx, "prefix:war,prefix:wi").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:flöte,prefix:nörgel").documents.length);
     require("console").log(3); 
      assertEqual(2, collection.FULLTEXT(idx, "prefix:flöte,böse,wörter,prefix:nörgel").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:flöte,prefix:tröt,prefix:bös").documents.length);
     require("console").log(2); 
       */
      // prefix and non-existing words
      assertEqual(0, collection.FULLTEXT(idx, "prefix:flöte,prefix:tröt,prefix:bös,GROßE").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "quasi,prefix:quer,präfix:differenz,müller").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "quasi,prefix:quer,präfix:differenz,prefix:müller").documents.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief long prefixes
////////////////////////////////////////////////////////////////////////////////

    testLongPrefixes: function () {
      var texts = [
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.FULLTEXT(idx, "prefix:donau").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:donaudampfschifffahrtskapitaensmuetze").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:donaudampfschifffahrtskapitaensmuetzentraeger").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstand").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:dampf").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:dampfmaschinenfahrzeugsinspektion").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant").documents.length); // !!! only 40 chars are significant
      assertEqual(1, collection.FULLTEXT(idx, "prefix:autotuerendellenentfernungsfirmenmitarbeiter").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:autotuer").documents.length);
      assertEqual(1, collection.FULLTEXT(idx, "prefix:auto").documents.length);
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
      /*
      assertEqual(2, collection.FULLTEXT(idx, "dolor,consetetur,prefix:invix").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:sanct,voluptua").documents.length);
      assertEqual(2, collection.FULLTEXT(idx, "prefix:accus,prefix:takima").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:accus,prefix:takeshi").documents.length);
      assertEqual(0, collection.FULLTEXT(idx, "prefix:accus,takeshi").documents.length);
      */
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(fulltextQuerySuite);

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
