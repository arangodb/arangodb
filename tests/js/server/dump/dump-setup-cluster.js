/*jshint globalstrict:false, strict:false, maxlen : 4000 */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for dump/reload tests
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

'use strict';
const db = require("@arangodb").db;
const isEnterprise = require("internal").isEnterprise();
const jsunity = require('jsunity');
const _ = require('lodash');
const {assertEqual, assertUndefined} = jsunity.jsUnity.assertions;

/**
 * @brief Only if enterprise mode:
 *        Creates a SmartGraph sharded by `value`
 *        That has 100 vertices (value 0 -> 99)
 *        That has 100 orphans (value 0 -> 99)
 *        That has 300 edges, for each value i:
 *          Connect i -> i
 *          Connect i - 1 <- i
 *          Connect i -> i + 1
 */
const setupSmartGraph = function () {
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestDumpSmartGraph";
  const edges = "UnitTestDumpSmartEdges";
  const vertices = "UnitTestDumpSmartVertices";
  const orphans = "UnitTestDumpSmartOrphans";
  const gm = require("@arangodb/smart-graph");
  if (gm._exists(smartGraphName)) {
    gm._drop(smartGraphName, true);
  }
  db._drop(edges);
  db._drop(vertices);

  gm._create(smartGraphName, [gm._relation(edges, vertices, vertices)],
    [orphans], {numberOfShards: 5, smartGraphAttribute: "value"});

  let vDocs = [];
  for (let i = 0; i < 100; ++i) {
    vDocs.push({value: String(i)});
  }
  let saved = db[vertices].save(vDocs).map(v => v._id);
  let eDocs = [];
  for (let i = 0; i < 100; ++i) {
    eDocs.push({_from: saved[(i+1) % 100], _to: saved[i], value: String(i)});
    eDocs.push({_from: saved[i], _to: saved[i], value: String(i)});
    eDocs.push({_from: saved[i], _to: saved[(i+1) % 100], value: String(i)});
  }
  db[edges].save(eDocs);
  db[orphans].save(vDocs);
};

/**
 * @brief Only if enterprise mode:
 *        Creates an arangosearch over smart edge collection
 */
const setupSmartArangoSearch = function () {
  if (!isEnterprise) {
    return;
  }

  db._dropView("UnitTestsDumpSmartView");

  let analyzers = require("@arangodb/analyzers");
  try {
    analyzers.remove("smartCustom");
  } catch (err) { }
  let analyzer = analyzers.save("smartCustom", "delimiter", { delimiter : "smart" }, [ "frequency" ]);

  db._createView("UnitTestsDumpSmartView", "arangosearch", {
      // choose non default values to check if they are corretly dumped and imported
      cleanupIntervalStep: 456,
      consolidationPolicy: {
        threshold: 0.3,
        type: "bytes_accum"
      },
      consolidationIntervalMsec: 0,
      links : {
        "UnitTestDumpSmartEdges": {
          includeAllFields: true,
          fields: {
            text: { analyzers: [ "text_en", "smartCustom" ] }
          }
        }
      }
  });
};

/**
 * @brief Only if enterprise mode:
 *        Creates a SatelliteCollection with 100 documents
 */
function setupSatelliteCollections() {
  if (!isEnterprise) {
    return;
  }

  const satelliteCollectionName = "UnitTestDumpSatelliteCollection";
  db._drop(satelliteCollectionName);
  db._create(satelliteCollectionName, {"replicationFactor": "satellite"});

  let vDocs = [];
  for (let i = 0; i < 100; ++i) {
    vDocs.push({value: String(i)});
  }
  db[satelliteCollectionName].save(vDocs);
}

/**
 * @brief Only if enterprise mode:
 *        Creates a SatelliteGraph
 */
function setupSatelliteGraphs() {
  if (!isEnterprise) {
    return;
  }

  const satelliteGraphName = "UnitTestDumpSatelliteGraph";
  const satelliteVertexCollection1Name = "UnitTestDumpSatelliteVertexCollection1";
  const satelliteVertexCollection2Name = "UnitTestDumpSatelliteVertexCollection2";
  const satelliteOrphanCollectionName = "UnitTestDumpSatelliteOrphanCollection";
  const satelliteEdgeCollection1Name = "UnitTestDumpSatelliteEdgeCollection1";
  const satelliteEdgeCollection2Name = "UnitTestDumpSatelliteEdgeCollection2";

  const satgm = require('@arangodb/satellite-graph.js');

  // Add satelliteVertexCollection1Name first, so we can expect it to be
  // distributeShardsLike leader
  const satelliteGraph = satgm._create(satelliteGraphName, [], [satelliteVertexCollection1Name]);
  satelliteGraph._extendEdgeDefinitions(satgm._relation(satelliteEdgeCollection1Name, satelliteVertexCollection1Name, satelliteVertexCollection2Name));
  satelliteGraph._extendEdgeDefinitions(satgm._relation(satelliteEdgeCollection2Name, satelliteVertexCollection2Name, satelliteVertexCollection1Name));
  satelliteGraph._addVertexCollection(satelliteOrphanCollectionName, true);

  // Expect satelliteVertexCollection1Name to be the distributeShardsLike leader.
  // This will be tested to be unchanged after restore, thus the assertion here.
  assertUndefined(db[satelliteVertexCollection1Name].properties().distributeShardsLike);
  assertEqual(satelliteVertexCollection1Name, db[satelliteVertexCollection2Name].properties().distributeShardsLike);
  assertEqual(satelliteVertexCollection1Name, db[satelliteEdgeCollection1Name].properties().distributeShardsLike);
  assertEqual(satelliteVertexCollection1Name, db[satelliteEdgeCollection2Name].properties().distributeShardsLike);
  assertEqual(satelliteVertexCollection1Name, db[satelliteOrphanCollectionName].properties().distributeShardsLike);

  // add a circle with 100 vertices over multiple collections.
  // vertexCol1 will contain uneven vertices, vertexCol2 even vertices.
  // edgeCol1 will contain edges (v1, v2), i.e. from uneven to even, and
  // edgeCol2 the other way round.
  // It also adds 100 vertices to the orphan collection.
  db._query(`
    FOR i IN 1..100
      LET vertexKey = CONCAT("v", i)
      LET unevenVertices = (
        FILTER i % 2 == 1
        INSERT { _key: vertexKey }
          INTO @@vertexCol1
      )
      LET evenVertices = (
        FILTER i % 2 == 0
        INSERT { _key: vertexKey }
          INTO @@vertexCol2
      )
      LET from = CONCAT(i % 2 == 1 ? @vertexCol1 : @vertexCol2, "/v", i)
      LET to = CONCAT((i+1) % 2 == 1 ? @vertexCol1 : @vertexCol2, "/v", i % 100 + 1)
      LET unevenEdges = (
        FILTER i % 2 == 1
        INSERT { _from: from, _to: to }
          INTO @@edgeCol1
      )
      LET evenEdges = (
        FILTER i % 2 == 0
        INSERT { _from: from, _to: to }
          INTO @@edgeCol2
      )
      INSERT { _key: CONCAT("w", i) }
        INTO @@orphanCol
  `, {
    '@vertexCol1': satelliteVertexCollection1Name,
    '@vertexCol2': satelliteVertexCollection2Name,
    'vertexCol1': satelliteVertexCollection1Name,
    'vertexCol2': satelliteVertexCollection2Name,
    '@edgeCol1': satelliteEdgeCollection1Name,
    '@edgeCol2': satelliteEdgeCollection2Name,
    '@orphanCol': satelliteOrphanCollectionName,
  });
  const res = db._query(`
    FOR v, e, p IN 100 OUTBOUND "UnitTestDumpSatelliteVertexCollection1/v1" GRAPH "UnitTestDumpSatelliteGraph"
      RETURN p.vertices[*]._key
  `);
  // [ [ "v1", "v2", ..., "v99", "v100", "v1" ] ]
  const expected = [_.range(0, 100+1).map(i => "v" + (i % 100 + 1))];
  assertEqual(expected, res.toArray());
}

/**
 * @brief Only if enterprise mode:
 *        Creates a SmartGraph and changes the value of the smart
 *        attribute to check that the graph can still be restored. 
 *
 *        This is a regression test for a bug in which a dumped
 *        database containing a SmartGraph with edited smart attribute
 *        value could not be restored.
 */
function setupSmartGraphRegressionTest() {
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestRestoreSmartGraphRegressionTest";
  const edges = "UnitTestRestoreSmartGraphRegressionEdges";
  const vertices = "UnitTestRestoreSmartGraphRegressionVertices";
  const gm = require("@arangodb/smart-graph");
  if (gm._exists(smartGraphName)) {
    gm._drop(smartGraphName, true);
  }
  db._drop(edges);
  db._drop(vertices);

  gm._create(smartGraphName, [gm._relation(edges, vertices, vertices)],
    [], {numberOfShards: 5, smartGraphAttribute: "cheesyness"});

  let vDocs = [{cheesyness: "cheese"}];
  let saved = db[vertices].save(vDocs).map(v => v._id);
  let eDocs = [];

  // update smartGraphAttribute. This makes _key inconsistent
  // and on dump/restore used to throw an error. We now ignore
  // that error
  db._update(saved[0], { cheesyness: "bread" });  
}

(function () {
  var analyzers = require("@arangodb/analyzers");
  var i, c;

  let createOptions = {};

  ["UnitTestsDumpSrc", "UnitTestsDumpDst", "UnitTestsDumpProperties1", "UnitTestsDumpProperties2"].forEach(function(name) {
    try {
      db._dropDatabase(name);
    } catch (err) {}
  });

  db._createDatabase("UnitTestsDumpProperties1", { replicationFactor: 1, writeConcern: 1 });
  db._createDatabase("UnitTestsDumpProperties2", { replicationFactor: 2, writeConcern: 2, sharding: "single" });
  db._createDatabase("UnitTestsDumpSrc", { replicationFactor: 2, writeConcern: 2 });
  db._createDatabase("UnitTestsDumpDst", { replicationFactor: 2, writeConcern: 2 });

  db._useDatabase("UnitTestsDumpSrc");

  // clean up first
  db._drop("UnitTestsDumpEmpty");
  db._drop("UnitTestsDumpMany");
  db._drop("UnitTestsDumpEdges");
  db._drop("UnitTestsDumpOrder");
  db._drop("UnitTestsDumpRemoved");
  db._drop("UnitTestsDumpIndexes");
  db._drop("UnitTestsDumpTruncated");
  db._drop("UnitTestsDumpShards");
  db._drop("UnitTestsDumpStrings");
  db._drop("UnitTestsDumpReplicationFactor1");
  db._drop("UnitTestsDumpReplicationFactor2");

  // this remains empty
  db._create("UnitTestsDumpEmpty", { waitForSync: true, indexBuckets: 256 });

  // create lots of documents
  c = db._create("UnitTestsDumpMany");
  var l = [];
  for (i = 0; i < 100000; ++i) {
    l.push({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
    if (l.length === 10000) {
      c.save(l);
      l = [];
    }
  }

  c = db._createEdgeCollection("UnitTestsDumpEdges");
  for (i = 0; i < 10; ++i) {
    c.save("UnitTestsDumpMany/test" + i, "UnitTestsDumpMany/test" + (i + 1), { _key: "test" + i, what: i + "->" + (i + 1) });
  }

  // we update & modify the order
  c = db._create("UnitTestsDumpOrder", { indexBuckets: 16 });
  c.save({ _key: "one", value: 1 });
  c.save({ _key: "two", value: 2 });
  c.save({ _key: "three", value: 3 });
  c.save({ _key: "four", value: 4 });

  c.update("three", { value: 3, value2: 123 });
  c.update("two", { value: 2, value2: 456 });
  c.update("one", { value: 1, value2: 789 });
  c.remove("four");

  // we apply a series of updates & removals here
  c = db._create("UnitTestsDumpRemoved");
  l = [];
  for (i = 0; i < 10000; ++i) {
    l.push({ _key: "test" + i, value1: i });
  }
  c.save(l);
  for (i = 0; i < 1000; ++i) {
    c.update("test" + i, { value2: i + 1 });
  }
  l = [];
  for (i = 0; i < 10000; i += 10) {
    l.push("test" + i);
  }
  c.remove(l);

  // we create a lot of (meaningless) indexes here
  c = db._create("UnitTestsDumpIndexes", { indexBuckets: 32 });
  c.ensureUniqueConstraint("a_uc");
  c.ensureSkiplist("a_s1", "a_s2");

  c.ensureHashIndex("a_h1", "a_h2");
  c.ensureUniqueSkiplist("a_su");
  c.ensureHashIndex("a_hs1", "a_hs2", { sparse: true });
  c.ensureSkiplist("a_ss1", "a_ss2", { sparse: true });
  c.ensureFulltextIndex("a_f");

  c.ensureGeoIndex("a_la", "a_lo");

  // we insert data and remove it
  c = db._create("UnitTestsDumpTruncated");
  l = [];
  for (i = 0; i < 10000; ++i) {
    l.push({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
  }
  c.save(l);
  c.truncate();

  // more than one shard:
  c = db._create("UnitTestsDumpShards", { numberOfShards : 9 });
  l = [];
  for (i = 0; i < 1000; ++i) {
    l.push({ _key : String(7 + (i*42)), value: i, more: { value: [ i, i ] } });
  }
  c.save(l);

  // strings
  c = db._create("UnitTestsDumpStrings");
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
    c.save({ _key: "text" + i, value: t });
  });

  let analyzer = analyzers.save("custom", "delimiter", { delimiter : " " }, [ "frequency" ]);

  // setup a view
  try {
    c = db._create("UnitTestsDumpViewCollection");

    let view = db._createView("UnitTestsDumpView", "arangosearch", {});
    view.properties({
      // choose non default values to check if they are corretly dumped and imported
      cleanupIntervalStep: 456,
      consolidationPolicy: {
        threshold: 0.3,
        type: "bytes_accum" // undocumented?
      },
      consolidationIntervalMsec: 0,
      links: {
        "UnitTestsDumpViewCollection": {
          includeAllFields: true,
          fields: {
            text: { analyzers: [ "text_en", analyzer.name ] }
          }
        }
      }
    });

    c.save(Array(5000).fill().map((e, i, a) => Object({_key: "test" + i, value: i})));
    c.save({ value: -1, text: "the red foxx jumps over the pond" });
  } catch (err) { }

  // setup a view on _analyzers collection
  try {
    let view = db._createView("analyzersView", "arangosearch", {
      links: {
        _analyzers : {
          includeAllFields:true,
          analyzers: [ analyzer.name ]
        }
      }
    });
  } catch (err) { }

  setupSmartGraph();
  setupSmartArangoSearch();
  setupSatelliteCollections();
  setupSatelliteGraphs();
  setupSmartGraphRegressionTest();

  db._create("UnitTestsDumpReplicationFactor1", { replicationFactor: 2, numberOfShards: 7 });
  db._create("UnitTestsDumpReplicationFactor2", { replicationFactor: 2, numberOfShards: 6 });
  
  // insert an entry into _jobs collection
  try {
    db._jobs.remove("test");
  } catch (err) {}
  db._jobs.insert({ _key: "test", status: "completed" });

  // insert an entry into _queues collection
  try {
    db._queues.remove("test");
  } catch (err) {}
  db._queues.insert({ _key: "test" });

  // Install Foxx
  const fs = require('fs');
  const SERVICE_PATH = fs.makeAbsolute(fs.join(
    require('internal').pathForTesting('common'), 'test-data', 'apps', 'minimal-working-service'
  ));
  const FoxxManager = require('@arangodb/foxx/manager');
  FoxxManager.install(SERVICE_PATH, '/test');
  db._useDatabase('UnitTestsDumpDst');
  // trigger analyzers cache population in dst database
  // this one should be removed
  analyzers.save("custom_dst", "delimiter", { delimiter : "," }, [ "frequency" ]);
  // this one should be replaced
  analyzers.save("custom", "delimiter", { delimiter : "," }, [ "frequency" ]);
})();

return {
  status: true
};

