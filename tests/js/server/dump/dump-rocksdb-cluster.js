/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
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

const fs = require('fs');
const internal = require("internal");
const jsunity = require("jsunity");
const isEnterprise = internal.isEnterprise();
const db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function dumpTestSuite () {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the empty collection
////////////////////////////////////////////////////////////////////////////////

    testEmpty : function () {
      var c = db._collection("UnitTestsDumpEmpty");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertTrue(p.waitForSync);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection with many documents
////////////////////////////////////////////////////////////////////////////////

    testMany : function () {
      var c = db._collection("UnitTestsDumpMany");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(100000, c.count());

      // test all documents
      var r = db._query(`FOR d IN ${c.name()} RETURN d`).toArray();
      var rr = new Map();
      for (let i = 0; i < r.length; ++i) {
        rr.set(r[i]._key, r[i]);
      }
      for (let i = 0; i < 100000; ++i) {
        var doc = rr.get("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("this is a test", doc.value2);
        assertEqual("test" + i, doc.value3);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the edges collection
////////////////////////////////////////////////////////////////////////////////

    testEdges : function () {
      var c = db._collection("UnitTestsDumpEdges");
      var p = c.properties();

      assertEqual(3, c.type()); // edges
      assertFalse(p.waitForSync);

      assertEqual(2, c.getIndexes().length); // primary index + edges index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual("edge", c.getIndexes()[1].type);
      assertEqual(10, c.count());

      // test all documents
      for (var i = 0; i < 10; ++i) {
        var doc = c.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual("UnitTestsDumpMany/test" + i, doc._from);
        assertEqual("UnitTestsDumpMany/test" + (i + 1), doc._to);
        assertEqual(i + "->" + (i + 1), doc.what);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the order of documents
////////////////////////////////////////////////////////////////////////////////

    testOrder : function () {
      var c = db._collection("UnitTestsDumpOrder");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(3, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document removal & update
////////////////////////////////////////////////////////////////////////////////

    testRemoved : function () {
      var c = db._collection("UnitTestsDumpRemoved");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(9000, c.count());

      var i;
      for (i = 0; i < 10000; ++i) {
        if (i % 10 === 0) {
          assertFalse(c.exists("test" + i));
        }
        else {
          var doc = c.document("test" + i);
          assertEqual(i, doc.value1);

          if (i < 1000) {
            assertEqual(i + 1, doc.value2);
          }
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test indexes
////////////////////////////////////////////////////////////////////////////////

    testIndexes : function () {
      var c = db._collection("UnitTestsDumpIndexes");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);

      assertEqual(8, c.getIndexes().length);
      assertEqual("primary", c.getIndexes()[0].type);

      assertEqual("hash", c.getIndexes()[1].type);
      assertTrue(c.getIndexes()[1].unique);
      assertFalse(c.getIndexes()[1].sparse);
      assertEqual([ "a_uc" ], c.getIndexes()[1].fields);

      assertEqual("skiplist", c.getIndexes()[2].type);
      assertFalse(c.getIndexes()[2].unique);
      assertFalse(c.getIndexes()[2].sparse);
      assertEqual([ "a_s1", "a_s2" ], c.getIndexes()[2].fields);

      assertEqual("hash", c.getIndexes()[3].type);
      assertFalse(c.getIndexes()[3].unique);
      assertFalse(c.getIndexes()[3].sparse);
      assertEqual([ "a_h1", "a_h2" ], c.getIndexes()[3].fields);

      assertEqual("skiplist", c.getIndexes()[4].type);
      assertTrue(c.getIndexes()[4].unique);
      assertFalse(c.getIndexes()[4].sparse);
      assertEqual([ "a_su" ], c.getIndexes()[4].fields);

      assertEqual("hash", c.getIndexes()[5].type);
      assertFalse(c.getIndexes()[5].unique);
      assertTrue(c.getIndexes()[5].sparse);
      assertEqual([ "a_hs1", "a_hs2" ], c.getIndexes()[5].fields);

      assertEqual("skiplist", c.getIndexes()[6].type);
      assertFalse(c.getIndexes()[6].unique);
      assertTrue(c.getIndexes()[6].sparse);
      assertEqual([ "a_ss1", "a_ss2" ], c.getIndexes()[6].fields);

      assertFalse(c.getIndexes()[7].unique);
      assertEqual("fulltext", c.getIndexes()[7].type);
      assertEqual([ "a_f" ], c.getIndexes()[7].fields);

      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test truncate
////////////////////////////////////////////////////////////////////////////////

    testTruncated : function () {
      var c = db._collection("UnitTestsDumpTruncated");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shards
////////////////////////////////////////////////////////////////////////////////

    testShards : function () {
      var c = db._collection("UnitTestsDumpShards");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);
      assertEqual(9, p.numberOfShards);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(1000, c.count());

      for (var i = 0; i < 1000; ++i) {
        var doc = c.document(String(7 + (i * 42)));

        assertEqual(String(7 + (i * 42)), doc._key);
        assertEqual(i, doc.value);
        assertEqual({ value: [ i, i ] }, doc.more);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test strings
////////////////////////////////////////////////////////////////////////////////

    testStrings : function () {
      var c = db._collection("UnitTestsDumpStrings");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(8, c.count());

      var texts = [
        "big. Really big. He moment. Magrathea! - insisted Arthur, - I do you can sense no further because it doesn't fit properly. In my the denies faith, and the atmosphere beneath You are not cheap He was was his satchel. He throughout Magrathea. - He pushed a tore the ecstatic crowd. Trillian sat down the time, the existence is it? And he said, - What they don't want this airtight hatchway. - it's we you shooting people would represent their Poet Master Grunthos is in his mind.",
        "Ultimo cadere chi sedete uso chiuso voluto ora. Scotendosi portartela meraviglia ore eguagliare incessante allegrezza per. Pensava maestro pungeva un le tornano ah perduta. Fianco bearmi storia soffio prende udi poteva una. Cammino fascino elisire orecchi pollici mio cui sai sul. Chi egli sino sei dita ben. Audace agonie groppa afa vai ultima dentro scossa sii. Alcuni mia blocco cerchi eterno andare pagine poi. Ed migliore di sommesso oh ai angoscia vorresti.",
        "Νέο βάθος όλα δομές της χάσει. Μέτωπο εγώ συνάμα τρόπος και ότι όσο εφόδιο κόσμου. Προτίμηση όλη διάφορους του όλο εύθραυστη συγγραφής. Στα άρα ένα μία οποία άλλων νόημα. Ένα αποβαίνει ρεαλισμού μελετητές θεόσταλτο την. Ποντιακών και rites κοριτσάκι παπούτσια παραμύθια πει κυρ.",
        "Mody laty mnie ludu pole rury Białopiotrowiczowi. Domy puer szczypię jemy pragnął zacność czytając ojca lasy Nowa wewnątrz klasztoru. Chce nóg mego wami. Zamku stał nogą imion ludzi ustaw Białopiotrowiczem. Kwiat Niesiołowskiemu nierostrzygniony Staje brał Nauka dachu dumę Zamku Kościuszkowskie zagon. Jakowaś zapytać dwie mój sama polu uszakach obyczaje Mój. Niesiołowski książkowéj zimny mały dotychczasowa Stryj przestraszone Stolnikównie wdał śmiertelnego. Stanisława charty kapeluszach mięty bratem każda brząknął rydwan.",
        "Мелких против летают хижину тмится. Чудесам возьмет звездна Взжигай. . Податель сельские мучитель сверкает очищаясь пламенем. Увы имя меч Мое сия. Устранюсь воздушных Им от До мысленные потушатся Ко Ея терпеньем.",
        "dotyku. Výdech spalin bude položen záplavový detekční kabely 1x UPS Newave Conceptpower DPA 5x 40kVA bude ukončen v samostatné strojovně. Samotné servery mají pouze lokalita Ústí nad zdvojenou podlahou budou zakončené GateWayí HiroLink - Monitoring rozvaděče RTN na jednotlivých záplavových zón na soustrojí resp. technologie jsou označeny SA-MKx.y. Jejich výstupem je zajištěn přestupem dat z jejich provoz. Na dveřích vylepené výstražné tabulky. Kabeláž z okruhů zálohovaných obvodů v R.MON-I. Monitoring EZS, EPS, ... možno zajistit funkčností FireWallů na strukturovanou kabeláží vedenou v měrných jímkách zapuštěných v každém racku budou zakončeny v R.MON-NrNN. Monitoring motorgenerátorů: řídící systém bude zakončena v modulu",
        "ramien mu zrejme vôbec niekto je už presne čo mám tendenciu prispôsobiť dych jej páčil, čo chce. Hmm... Včera sa mi pozdava, len dočkali, ale keďže som na uz boli u jej nezavrela. Hlava jej to ve městě nepotká, hodně mi to tí vedci pri hre, keď je tu pre Designiu. Pokiaľ viete o odbornejšie texty. Prvým z tmavých uličiek, každý to niekedy, zrovnávať krok s obrovským batohom na okraj vane a temné úmysly, tak rozmýšľam, aký som si hromady mailov, čo chcem a neraz sa pokúšal o filmovém klubu v budúcnosti rozhodne uniesť mladú maliarku (Linda Rybová), ktorú so",
        " 復讐者」. 復讐者」. 伯母さん 復讐者」. 復讐者」. 復讐者」. 復讐者」. 第九章 第五章 第六章 第七章 第八章. 復讐者」 伯母さん. 復讐者」 伯母さん. 第十一章 第十九章 第十四章 第十八章 第十三章 第十五章. 復讐者」 . 第十四章 第十一章 第十二章 第十五章 第十七章 手配書. 第十四章 手配書 第十八章 第十七章 第十六章 第十三章. 第十一章 第十三章 第十八章 第十四章 手配書. 復讐者」."
      ];

      texts.forEach(function (t, i) {
        var doc = c.document("text" + i);

        assertEqual(t, doc.value);
      });

    },


    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test view restoring
    ////////////////////////////////////////////////////////////////////////////////

    testView : function () {
      try {
        db._createView("check", "arangosearch", {});
      } catch (err) {}

      let views = db._views();
      if (views.length === 0) {
        return; // arangosearch views are not supported
      }

      let view = db._view("UnitTestsDumpView");
      assertTrue(view !== null);
      let props = view.properties();
      assertEqual(Object.keys(props.links).length, 1);
      assertTrue(props.hasOwnProperty("links"));
      assertTrue(props.links.hasOwnProperty("UnitTestsDumpViewCollection"));
      assertTrue(props.links.UnitTestsDumpViewCollection.hasOwnProperty("includeAllFields"));
      assertTrue(props.links.UnitTestsDumpViewCollection.hasOwnProperty("fields"));
      assertTrue(props.links.UnitTestsDumpViewCollection.includeAllFields);

      assertEqual(props.consolidationIntervalMsec, 0);
      assertEqual(props.cleanupIntervalStep, 456);
      assertTrue(Math.abs(props.consolidationPolicy.threshold - 0.3) < 0.001);
      assertEqual(props.consolidationPolicy.type, "bytes_accum");

      var res = db._query("FOR doc IN " + view.name() + " SEARCH doc.value >= 0 OPTIONS { waitForSync: true } RETURN doc").toArray();
      assertEqual(5000, res.length);

      res = db._query("FOR doc IN " + view.name() + " SEARCH doc.value >= 2500 RETURN doc").toArray();
      assertEqual(2500, res.length);

      res = db._query("FOR doc IN " + view.name() + " SEARCH doc.value >= 5000 RETURN doc").toArray();
      assertEqual(0, res.length);

      res = db._query("FOR doc IN UnitTestsDumpView SEARCH PHRASE(doc.text, 'foxx jumps over', 'text_en')  RETURN doc").toArray();
      assertEqual(1, res.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for the enterprise mode
////////////////////////////////////////////////////////////////////////////////

function dumpTestEnterpriseSuite () {
  const smartGraphName = "UnitTestDumpSmartGraph";
  const edges = "UnitTestDumpSmartEdges";
  const vertices = "UnitTestDumpSmartVertices";
  const orphans = "UnitTestDumpSmartOrphans";
  const satellite = "UnitTestDumpSatelliteCollection";
  const gm = require("@arangodb/smart-graph");
  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

    testSatelliteCollections : function () {
      let c = db._collection(satellite);
      let p = c.properties();
      assertEqual(2, c.type()); // Document
      assertEqual(1, p.numberOfShards);
      assertEqual("satellite", p.replicationFactor);
      assertEqual(100, c.count());
    },

    testHiddenCollectionsOmitted : function () {
      const dumpDir = fs.join(instanceInfo.rootDir, 'dump');

      const smartEdgeCollectionPath = fs.join(dumpDir, `${edges}.structure.json`);
      const localEdgeCollectionPath = fs.join(dumpDir, `_local_${edges}.structure.json`);
      const fromEdgeCollectionPath = fs.join(dumpDir, `_from_${edges}.structure.json`);
      const toEdgeCollectionPath = fs.join(dumpDir, `_to_${edges}.structure.json`);

      assertTrue(fs.exists(smartEdgeCollectionPath), 'Smart edge collection missing in dump!');
      assertFalse(fs.exists(localEdgeCollectionPath), '_local edge collection should not have been dumped!');
      assertFalse(fs.exists(fromEdgeCollectionPath), '_from edge collection should not have been dumped!');
      assertFalse(fs.exists(toEdgeCollectionPath), '_to edge collection should not have been dumped!');
    },

    testShadowCollectionsOmitted : function () {
      const encryption = fs.read(fs.join(instanceInfo.rootDir, 'dump', 'ENCRYPTION'));
      if (encryption === '' || encryption === 'none') {
        const dumpDir = fs.join(instanceInfo.rootDir, 'dump');
        const collStructure = JSON.parse(
          fs.read(fs.join(dumpDir, `${edges}.structure.json`))
        );

        assertTrue(collStructure.hasOwnProperty('parameters'), collStructure);
        const parameters = collStructure['parameters'];
        assertFalse(parameters.hasOwnProperty('shadowCollections'),
          `Property 'shadowCollections' should be hidden in collection ${edges}!`);
      }
    },

    testVertices : function () {
      let c = db._collection(vertices);
      let p = c.properties();
      assertEqual(2, c.type()); // Document
      assertEqual(5, p.numberOfShards);
      assertTrue(p.isSmart, p);
      assertFalse(Object.hasOwnProperty(p, "distributeShardsLike"));
      assertEqual(100, c.count());
      assertEqual("value", p.smartGraphAttribute);
    },

    testVerticesAqlRead: function () {
      let q1 = `FOR x IN ${vertices} SORT TO_NUMBER(x.value) RETURN x`;
      let q2 = `FOR x IN ${vertices} FILTER x.value == "10" RETURN x.value`;
      // This query can be optimized to a single shard. Make sure that is still correct
      let q3 = `FOR x IN ${vertices} FILTER x._key == @key RETURN x.value`;

      let res1 = db._query(q1).toArray();
      assertEqual(100, res1.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual(String(i), res1[i].value);
      }

      let res2 = db._query(q2).toArray();
      assertEqual(1, res2.length);
      assertEqual("10", res2[0]);

      for (let x of res1) {
        let res3 = db._query(q3, {key: x._key}).toArray();
        assertEqual(1, res3.length);
        assertEqual(x.value, res3[0]);
      }
    },

    testVerticesAqlInsert: function () {
      // Precondition
      assertEqual(100, db[vertices].count());
      let insert = `FOR i IN 0..99 INSERT {value: TO_STRING(i), needUpdate: true, needRemove: true} INTO ${vertices}`;
      let update = `FOR x IN ${vertices} FILTER x.needUpdate UPDATE x WITH {needUpdate: false} INTO ${vertices}`;
      let remove = `FOR x IN ${vertices} FILTER x.needRemove REMOVE x INTO ${vertices}`;
      // Note: Order is important here, we first insert, than update those inserted docs, then remoe them again
      let resIns = db._query(insert);
      assertEqual(100, resIns.getExtra().stats.writesExecuted);
      assertEqual(0, resIns.getExtra().stats.writesIgnored);
      assertEqual(200, db[vertices].count());

      let resUp = db._query(update);
      assertEqual(100, resUp.getExtra().stats.writesExecuted);
      assertEqual(0, resUp.getExtra().stats.writesIgnored);
      assertEqual(200, db[vertices].count());

      let resRem = db._query(remove);
      assertEqual(100, resRem.getExtra().stats.writesExecuted);
      assertEqual(0, resRem.getExtra().stats.writesIgnored);
      assertEqual(100, db[vertices].count());
    },

    testOrphans : function () {
      let c = db._collection(orphans);
      let p = c.properties();
      assertEqual(2, c.type()); // Document
      assertEqual(5, p.numberOfShards);
      assertTrue(p.isSmart);
      assertEqual(vertices, p.distributeShardsLike);
      assertEqual(100, c.count());
      assertEqual("value", p.smartGraphAttribute);
    },

    testOrphansAqlRead: function () {
      let q1 = `FOR x IN ${orphans} SORT TO_NUMBER(x.value) RETURN x`;
      let q2 = `FOR x IN ${orphans} FILTER x.value == "10" RETURN x.value`;
      // This query can be optimized to a single shard. Make sure that is still correct
      let q3 = `FOR x IN ${orphans} FILTER x._key == @key RETURN x.value`;

      let res1 = db._query(q1).toArray();
      assertEqual(100, res1.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual(String(i), res1[i].value);
      }

      let res2 = db._query(q2).toArray();
      assertEqual(1, res2.length);
      assertEqual("10", res2[0]);

      for (let x of res1) {
        let res3 = db._query(q3, {key: x._key}).toArray();
        assertEqual(1, res3.length);
        assertEqual(x.value, res3[0]);
      }
    },

    testOrphansAqlInsert: function () {
      // Precondition
      let c = db[orphans];
      assertEqual(100, c.count());
      let insert = `FOR i IN 0..99 INSERT {value: TO_STRING(i), needUpdate: true, needRemove: true} INTO ${orphans}`;
      let update = `FOR x IN ${orphans} FILTER x.needUpdate UPDATE x WITH {needUpdate: false} INTO ${orphans}`;
      let remove = `FOR x IN ${orphans} FILTER x.needRemove REMOVE x INTO ${orphans}`;
      // Note: Order is important here, we first insert, than update those inserted docs, then remoe them again
      let resIns = db._query(insert);
      assertEqual(100, resIns.getExtra().stats.writesExecuted);
      assertEqual(0, resIns.getExtra().stats.writesIgnored);
      assertEqual(200, c.count());

      let resUp = db._query(update);
      assertEqual(100, resUp.getExtra().stats.writesExecuted);
      assertEqual(0, resUp.getExtra().stats.writesIgnored);
      assertEqual(200, c.count());

      let resRem = db._query(remove);
      assertEqual(100, resRem.getExtra().stats.writesExecuted);
      assertEqual(0, resRem.getExtra().stats.writesIgnored);
      assertEqual(100, c.count());
    },

    testEEEdges : function () {
      let c = db._collection(edges);
      let p = c.properties();
      assertEqual(3, c.type()); // Edges
      //assertEqual(5, p.numberOfShards);
      assertTrue(p.isSmart);
      assertEqual(vertices, p.distributeShardsLike);
      assertEqual(300, c.count());
    },

    testEdgesAqlRead: function () {
      let q1 = `FOR x IN ${edges} SORT TO_NUMBER(x.value) RETURN x`;
      let q2 = `FOR x IN ${edges} FILTER x.value == "10" RETURN x.value`;
      // This query can be optimized to a single shard. Make sure that is still correct
      let q3 = `FOR x IN ${edges} FILTER x._key == @key RETURN x.value`;

      let res1 = db._query(q1).toArray();
      assertEqual(300, res1.length);
      for (let i = 0; i < 100; ++i) {
        // We have three edges per value
        assertEqual(String(i), res1[3*i].value);
        assertEqual(String(i), res1[3*i+1].value);
        assertEqual(String(i), res1[3*i+2].value);
      }

      let res2 = db._query(q2).toArray();
      assertEqual(3, res2.length);
      assertEqual("10", res2[0]);

      for (let x of res1) {
        let res3 = db._query(q3, {key: x._key}).toArray();
        assertEqual(1, res3.length);
        assertEqual(x.value, res3[0]);
      }
    },

    testEdgesAqlInsert: function () {
      // Precondition
      let c = db[edges];
      assertEqual(300, c.count());

      // We first need the vertices
      let vC = db[vertices];
      assertEqual(100, vC.count());
      let vQ = `FOR x IN ${vertices} SORT TO_NUMBER(x.value) RETURN x._id`;
      let verticesList = db._query(vQ).toArray();
      let insertSameValue = `LET vs = @vertices FOR i IN 0..99 INSERT {_from: vs[i], _to: vs[i], value: TO_STRING(i), needUpdate: true, needRemove: true} INTO ${edges}`;
      let insertOtherValue = `LET vs = @vertices FOR i IN 0..99 INSERT {_from: vs[i], _to: vs[(i + 1) % 100], value: TO_STRING(i), needUpdate: true, needRemove: true} INTO ${edges}`;
      let update = `FOR x IN ${edges} FILTER x.needUpdate UPDATE x WITH {needUpdate: false} INTO ${edges}`;
      let remove = `FOR x IN ${edges} FILTER x.needRemove REMOVE x INTO ${edges}`;
      // Note: Order is important here, we first insert, than update those inserted docs, then remove them again
      let resInsSame = db._query(insertSameValue, {vertices: verticesList});
      assertEqual(100, resInsSame.getExtra().stats.writesExecuted);
      assertEqual(0, resInsSame.getExtra().stats.writesIgnored);
      assertEqual(400, c.count());

      let resInsOther = db._query(insertOtherValue, {vertices: verticesList});
      assertEqual(100, resInsOther.getExtra().stats.writesExecuted);
      assertEqual(0, resInsOther.getExtra().stats.writesIgnored);
      assertEqual(500, c.count());

      let resUp = db._query(update);
      assertEqual(200, resUp.getExtra().stats.writesExecuted);
      assertEqual(0, resUp.getExtra().stats.writesIgnored);
      assertEqual(500, c.count());

      let resRem = db._query(remove);
      assertEqual(200, resRem.getExtra().stats.writesExecuted);
      assertEqual(0, resRem.getExtra().stats.writesIgnored);
      assertEqual(300, c.count());
    },

    testAqlGraphQuery: function() {
      // Precondition
      let c = db[edges];
      assertEqual(300, c.count());
      // We first need the vertices
      let vC = db[vertices];
      assertEqual(100, vC.count());

      let vertexQuery = `FOR x IN ${vertices} FILTER x.value == "10" RETURN x._id`;
      let vertex = db._query(vertexQuery).toArray();
      assertEqual(1, vertex.length);

      let q = `FOR v IN 1..2 ANY "${vertex[0]}" GRAPH "${smartGraphName}" OPTIONS {uniqueVertices: 'path'} SORT TO_NUMBER(v.value) RETURN v`;
      /* We expect the following result:
       * 10 <- 9 <- 8
       * 10 <- 9
       * 10 -> 11
       * 10 -> 11 -> 12
       */

      //Validate that everything is wired to a smart graph correctly
      let res = db._query(q).toArray();
      assertEqual(4, res.length);
      assertEqual("8", res[0].value);
      assertEqual("9", res[1].value);
      assertEqual("11", res[2].value);
      assertEqual("12", res[3].value);
    }

  };

}



////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dumpTestSuite);

if (isEnterprise) {
  jsunity.run(dumpTestEnterpriseSuite);
}

return jsunity.done();
