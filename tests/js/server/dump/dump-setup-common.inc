/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */

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

const _ = require('lodash');
const analyzers = require("@arangodb/analyzers");
const db = require("@arangodb").db;
const isCluster = require('internal').isCluster();
const isEnterprise = require("internal").isEnterprise();

function cleanup() {
  'use strict';

  let dbOptions = {};
  if (isCluster) {
    dbOptions = { replicationFactor: 2, writeConcern: 2 };
  }

  ["UnitTestsDumpSrc", "UnitTestsDumpDst"].forEach(function(name) {
    try {
      db._dropDatabase(name);
    } catch (err) {}
    db._createDatabase(name, dbOptions);
  });
  if (isCluster) {
    ["UnitTestsDumpProperties1", "UnitTestsDumpProperties2"].forEach(function(name) {
      try {
        db._dropDatabase(name);
      } catch (err) {}
    });

    db._createDatabase("UnitTestsDumpProperties1", { replicationFactor: 1, writeConcern: 1 });
    db._createDatabase("UnitTestsDumpProperties2", { replicationFactor: 2, writeConcern: 2, sharding: "single" });
  }
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
}

function createUsers() {
  // create user in _system database
  var users = require("@arangodb/users");
  users.save("foobaruser", "foobarpasswd", true);
  users.grantDatabase("foobaruser", "_system");
  users.grantCollection("foobaruser", "_system", "*");
  users.grantDatabase("foobaruser", "UnitTestsDumpSrc");
  users.grantCollection("foobaruser", "UnitTestsDumpSrc", "*");
  var endpoint = arango.getEndpoint();

  arango.reconnect(endpoint, "UnitTestsDumpSrc", "foobaruser", "foobarpasswd");
}

function createEmpty() {
  db._useDatabase("UnitTestsDumpSrc");

  // this remains empty
  db._create("UnitTestsDumpEmpty", { waitForSync: true });
}

function createMany() {
  db._useDatabase("UnitTestsDumpSrc");
  // create lots of documents
  let c = db._create("UnitTestsDumpMany");
  let docs = [];
  for (let i = 0; i < 100000; ++i) {
    docs.push({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
    if (docs.length === 10000) {
      c.save(docs);
      docs = [];
    }
  }

  c = db._createEdgeCollection("UnitTestsDumpEdges");
  for (let i = 0; i < 10; ++i) {
    c.save("UnitTestsDumpMany/test" + i,
           "UnitTestsDumpMany/test" + (i + 1),
           { _key: "test" + i, what: i + "->" + (i + 1) });
  }
}

function createOrder() {
  // we update & modify the order
  c = db._create("UnitTestsDumpOrder");
  c.save({ _key: "one", value: 1 });
  c.save({ _key: "two", value: 2 });
  c.save({ _key: "three", value: 3 });
  c.save({ _key: "four", value: 4 });

  c.update("three", { value: 3, value2: 123 });
  c.update("two", { value: 2, value2: 456 });
  c.update("one", { value: 1, value2: 789 });
  c.remove("four");
}

function createModifyCollection() {
  db._useDatabase("UnitTestsDumpSrc");

  // we apply a series of updates & removals here
  let c = db._create("UnitTestsDumpRemoved");
  c.save(Array(10000).fill().map((e, i, a) => Object({_key: "test" + i, value1: i})));
  for (let i = 0; i < 1000; ++i) {
    c.update("test" + i, { value2: i + 1 });
  }
  c.remove(Array(1000).fill().map((e, i, a) => "test" + (i * 10)));

  // we create a lot of (meaningless) indexes here
  c = db._create("UnitTestsDumpIndexes");
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
  c.save(Array(10000).fill().map((e, i, a) => Object({_key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i})));  
  c.truncate({ compact: false });
}

function createMultiShard() {
  // more than one shard:
  c = db._create("UnitTestsDumpShards", { numberOfShards : 9 });
  l = [];
  for (i = 0; i < 1000; ++i) {
    l.push({ _key : String(7 + (i*42)), value: i, more: { value: [ i, i ] } });
  }
  c.save(l);
}

function createAutoIncKeyGen() {
  db._useDatabase("UnitTestsDumpSrc");
  let c = db._create("UnitTestsDumpKeygen", {
    keyOptions: {
      type: "autoincrement",
      allowUserKeys: false,
      offset: 7,
      increment: 42
    }
  });
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i, more: { value: [ i, i ] } });
  }
  c.save(docs);
  let d = c.save({});
  c.save({}); // create another one inbetween...
  c.remove(d);
}

function createPaddedKeyGen() {
  db._useDatabase("UnitTestsDumpSrc");
  let c = db._create("UnitTestsDumpKeygenPadded", {
    keyOptions: {
      type: "padded",
      allowUserKeys: false
    },
    numberOfShards : 2 // single will ignore this...
  });

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i, more: { value: [ i, i ] } });
  }
  c.save(docs);
  let d = c.save({});
  c.save({}); // create another one inbetween...
  c.remove(d);
}

function createUUIDKeyGen() {
  db._useDatabase("UnitTestsDumpSrc");
  let c = db._create("UnitTestsDumpKeygenUuid", {
    keyOptions: {
      type: "uuid",
      allowUserKeys: false
    },
    numberOfShards : 2 // single will ignore this...
  });
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i });
  }
  c.save(docs);
  let d = c.save({});
  c.save({}); // create another one inbetween...
  c.remove(d);
}

function createStrings() {
  db._useDatabase("UnitTestsDumpSrc");

  let c = db._create("UnitTestsDumpStrings");
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
}

function createTransactional() {
  db._useDatabase("UnitTestsDumpSrc");
  let c = db._create("UnitTestsDumpTransactionCommit");
  db._executeTransaction({
    collections: {
      write: "UnitTestsDumpTransactionCommit"
    },
    action: function (params) {
      var c = require("internal").db.UnitTestsDumpTransactionCommit;
      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
      }
      c.save(docs);
    }
  });


  c = db._create("UnitTestsDumpTransactionUpdate");
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
  }
  c.save(docs);

  db._executeTransaction({
    collections: {
      write: "UnitTestsDumpTransactionUpdate"
    },
    action: function (params) {
      var c = require("internal").db.UnitTestsDumpTransactionUpdate;
      for (var i = 0; i < 1000; i += 2) {
        c.update("test" + i, { value3 : i });
      }
    }
  });


  c = db._create("UnitTestsDumpTransactionAbort");
  c.save({ _key: "foo" });
  try {
    db._executeTransaction({
      collections: {
        write: "UnitTestsDumpTransactionAbort"
      },
      action: function (params) {
        var c = require("internal").db.UnitTestsDumpTransactionAbort;
        c.remove("foo");
        let docs = [];
        for (let i = 0; i < 1000; ++i) {
          docs.push({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
        }
        c.save(docs);
        throw "rollback!";
      }
    });
    throw "unexpected!";
  }
  catch (err) {
  }
}

function createPersistent() {
  db._useDatabase("UnitTestsDumpSrc");
  let c = db._create("UnitTestsDumpPersistent");
  c.ensureIndex({ type: "persistent", fields: ["value"], unique: true });

  c.save(Array(10000).fill().map((e, i, a) => Object({_key: "test" + i, value: i})));
}

function createView() {
  db._useDatabase("UnitTestsDumpSrc");
  let analyzer = analyzers.save("custom", "delimiter", { delimiter : " " }, [ "frequency" ]);

  // setup a view
  try {
    let c = db._create("UnitTestsDumpViewCollection");

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
          includeAllFields: true,
          analyzers: [ analyzer.name ]
        }
      }
    });
  } catch (err) {}
}

function createReplicationFactors() {
  db._create("UnitTestsDumpReplicationFactor1", { replicationFactor: 2, numberOfShards: 7 });
  db._create("UnitTestsDumpReplicationFactor2", { replicationFactor: 2, numberOfShards: 6 });
}

function createJobs() {
  db._useDatabase("UnitTestsDumpSrc");
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
}

function createFoxx() {
  db._useDatabase("UnitTestsDumpSrc");
  // Install Foxx
  const fs = require('fs');
  const SERVICE_PATH = fs.makeAbsolute(fs.join(
    require('internal').pathForTesting('common'), 'test-data', 'apps', 'minimal-working-service'
  ));
  const FoxxManager = require('@arangodb/foxx/manager');
  FoxxManager.install(SERVICE_PATH, '/test');
  db._useDatabase('UnitTestsDumpDst');
}

function createAnalyzers() {
  // trigger analyzers cache population in dst database
  // this one should be removed
  analyzers.save("custom_dst", "delimiter", { delimiter : "," }, [ "frequency" ]);
  // this one should be replaced
  analyzers.save("custom", "delimiter", { delimiter : "," }, [ "frequency" ]);
}

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
function createSmartGraph() {
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
function createSmartArangoSearch() {
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
function createSatelliteCollections() {
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
function createSatelliteGraphs() {
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
function createSmartGraphRegressionTest() {
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

exports.createSmartGraph = createSmartGraph;
exports.createSmartArangoSearch = createSmartArangoSearch;
exports.createSatelliteCollections = createSatelliteCollections;
exports.createSatelliteGraphs = createSatelliteGraphs;
exports.createSmartGraphRegressionTest = createSmartGraphRegressionTest;

exports.cleanup = cleanup;
exports.createUsers = createUsers;
exports.createEmpty = createEmpty;
exports.createMany = createMany;
exports.createOrder = createOrder;
exports.createModifyCollection = createModifyCollection;
exports.createMultiShard = createMultiShard;
exports.createAutoIncKeyGen = createAutoIncKeyGen;
exports.createPaddedKeyGen = createPaddedKeyGen;
exports.createUUIDKeyGen = createUUIDKeyGen;
exports.createStrings = createStrings;
exports.createTransactional = createTransactional;
exports.createPersistent = createPersistent;
exports.createView = createView;
exports.createReplicationFactors = createReplicationFactors;
exports.createJobs = createJobs;
exports.createFoxx = createFoxx;
exports.createAnalyzers = createAnalyzers;
