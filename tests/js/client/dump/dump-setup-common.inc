/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */
/*global assertEqual, assertTrue, assertUndefined, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const analyzers = require("@arangodb/analyzers");
const db = require("@arangodb").db;
const isCluster = require('internal').isCluster();
const isEnterprise = require("internal").isEnterprise();
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";

let sgm;
let satgm;
let egm;
if (isEnterprise) {
  sgm = require("@arangodb/smart-graph");
  satgm = require("@arangodb/satellite-graph");
  egm = require("@arangodb/enterprise-graph");
}

function getNoShards(defaultNo) {
  if (db._properties().sharding === "single") {
    return 1;
  }
  return defaultNo;
}

function cleanup() {
  'use strict';

  let dbOptions = {};
  if (isCluster) {
    dbOptions = {replicationFactor: 2, writeConcern: 2};
  }

  ["UnitTestsDumpSrc", "UnitTestsDumpDst"].forEach(function (name) {
    try {
      db._dropDatabase(name);
    } catch (err) {
    }
    db._createDatabase(name, dbOptions);
  });
  if (isCluster) {
    ["UnitTestsDumpProperties1", "UnitTestsDumpProperties2"].forEach(function (name) {
      try {
        db._dropDatabase(name);
      } catch (err) {
      }
    });

    db._createDatabase("UnitTestsDumpProperties1", {replicationFactor: 1, writeConcern: 1});
    db._createDatabase("UnitTestsDumpProperties2", {replicationFactor: 2, writeConcern: 2, sharding: "single"});
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
  db._drop(extendedName);
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
  db._create("UnitTestsDumpEmpty", {waitForSync: true});
}

function createMany() {
  db._useDatabase("UnitTestsDumpSrc");
  // create lots of documents
  let c = db._create("UnitTestsDumpMany");
  let docs = [];
  for (let i = 0; i < 100000; ++i) {
    docs.push({_key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i});
    if (docs.length === 10000) {
      c.save(docs);
      docs = [];
    }
  }

  c = db._createEdgeCollection("UnitTestsDumpEdges");
  for (let i = 0; i < 10; ++i) {
    c.save("UnitTestsDumpMany/test" + i,
      "UnitTestsDumpMany/test" + (i + 1),
      {_key: "test" + i, what: i + "->" + (i + 1)});
  }
}

function createOrder() {
  // we update & modify the order
  let c = db._create("UnitTestsDumpOrder");
  c.save({_key: "one", value: 1});
  c.save({_key: "two", value: 2});
  c.save({_key: "three", value: 3});
  c.save({_key: "four", value: 4});

  c.update("three", {value: 3, value2: 123});
  c.update("two", {value: 2, value2: 456});
  c.update("one", {value: 1, value2: 789});
  c.remove("four");
}

function createModifyCollection() {
  db._useDatabase("UnitTestsDumpSrc");

  // we apply a series of updates & removals here
  let c = db._create("UnitTestsDumpRemoved");
  c.save(Array(10000).fill().map((e, i, a) => Object({_key: "test" + i, value1: i})));
  for (let i = 0; i < 1000; ++i) {
    c.update("test" + i, {value2: i + 1});
  }
  c.remove(Array(1000).fill().map((e, i, a) => "test" + (i * 10)));

  // we create a lot of (meaningless) indexes here
  c = db._create("UnitTestsDumpIndexes");
  c.ensureIndex({ type: "hash", fields: ["a_uc"], unique: true });
  c.ensureIndex({ type: "skiplist", fields: ["a_s1", "a_s2"] });
  c.ensureIndex({ type: "hash", fields: ["a_h1", "a_h2"] });
  c.ensureIndex({ type: "skiplist", fields: ["a_su"], unique: true });
  c.ensureIndex({ type: "hash", fields: ["a_hs1", "a_hs2"], sparse: true });
  c.ensureIndex({ type: "skiplist", fields: ["a_ss1", "a_ss2"], sparse: true });
  c.ensureIndex({ type: "fulltext", fields: ["a_f"] });
  c.ensureIndex({ type: "geo", fields: ["a_la", "a_lo"] });

  // we insert data and remove it
  c = db._create("UnitTestsDumpTruncated");
  c.save(Array(10000).fill().map((e, i, a) => Object({
    _key: "test" + i,
    value1: i,
    value2: "this is a test",
    value3: "test" + i
  })));
  c.truncate({compact: false});
}

function createExtendedName() {
  db._useDatabase("UnitTestsDumpSrc");

  // we apply a series of updates & removals here
  let c = db._create(extendedName);
  c.save(Array(10000).fill().map((e, i, a) => Object({_key: "test" + i, value1: i})));
}

function createMultiShard() {
  // more than one shard:
  let c = db._create("UnitTestsDumpShards", {numberOfShards: getNoShards(9)});
  let l = [];
  for (let i = 0; i < 1000; ++i) {
    l.push({_key: String(7 + (i * 42)), value: i, more: {value: [i, i]}});
  }
  c.save(l);
}

function createComputedValues() {
  db._useDatabase("UnitTestsDumpSrc");
   let c = db._create("UnitTestsComputedValues", {
    computedValues: [{
      name: "value3",
      expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
      overwrite: false
    },
    {
      name: "value4",
      expression: "RETURN CONCAT(@doc.value2, ' ', @doc.value1)",
      overwrite: true
    }]
   });
   let docs = [];
    for (let i = 0; i < 1000; ++i) {
      docs.push({value1: "test" + i, value2: "abc", value4: false});
    }
    c.insert(docs);
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
    docs.push({value: i, more: {value: [i, i]}});
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
    numberOfShards: getNoShards(2)
  });

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({value: i, more: {value: [i, i]}});
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
    numberOfShards: getNoShards(2)
  });
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({value: i});
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
    c.save({_key: "text" + i, value: t});
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
        docs.push({_key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i});
      }
      c.save(docs);
    }
  });


  c = db._create("UnitTestsDumpTransactionUpdate");
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({_key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i});
  }
  c.save(docs);

  db._executeTransaction({
    collections: {
      write: "UnitTestsDumpTransactionUpdate"
    },
    action: function (params) {
      var c = require("internal").db.UnitTestsDumpTransactionUpdate;
      for (var i = 0; i < 1000; i += 2) {
        c.update("test" + i, {value3: i});
      }
    }
  });


  c = db._create("UnitTestsDumpTransactionAbort");
  c.save({_key: "foo"});
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
          docs.push({_key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i});
        }
        c.save(docs);
        throw "rollback!";
      }
    });
    throw "unexpected!";
  } catch (err) {
  }
}

function createPersistent() {
  db._useDatabase("UnitTestsDumpSrc");
  let c = db._create("UnitTestsDumpPersistent");
  c.ensureIndex({type: "persistent", fields: ["value"], unique: true});

  c.save(Array(10000).fill().map((e, i, a) => Object({_key: "test" + i, value: i})));
}

function createView() {
  db._useDatabase("UnitTestsDumpSrc");
  const countBefore = db["_analyzers"].count();
  let analyzer = analyzers.save("customInCreateView", "delimiter", {delimiter: " "}, ["frequency"]);
  assertEqual(db["_analyzers"].count(), countBefore + 1);

  // setup a view
  try {
    let c = db._create("UnitTestsDumpViewCollection");

    let view = db._createView("UnitTestsDumpView", "arangosearch", {});
    view.properties({
      // choose non default values to check if they are correctly dumped and imported
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
            text: {analyzers: ["text_en", analyzer.name]}
          }
        }
      }
    });

    c.save({value: -1, text: "the red foxx jumps over the pond"});
    c.save(Array(5000).fill().map((e, i, a) => Object({_key: "test" + i, value: i})));
  } catch (err) {
  }

  // setup a view on _analyzers collection
  try {
    let view = db._createView("analyzersView", "arangosearch", {
      links: {
        _analyzers: {
          includeAllFields: true,
          analyzers: [analyzer.name]
        }
      }
    });
  } catch (err) {
  }
}

function createSearch() {
  db._useDatabase("UnitTestsDumpSrc");

  // create collections
  let collection = db._create("searchCollection");

  // create indexes
  let index = collection.ensureIndex({
    type: "inverted",
    name: "searchIndex",
    cleanupIntervalStep: 42,
    consolidationIntervalMsec: 0,
    includeAllFields: true,
    analyzer: "text_en"
  });

  // create views
  let search = db._createView("searchSearch", "search-alias",
   { indexes: [ { collection: collection.name(), index: index.name } ] });

  // insert data
  collection.save({value: -1, text: "the red foxx jumps over the pond"});
  collection.save(Array(5000).fill().map((e, i, a) => Object({_key: "test" + i, value: i})));
}

function createInvertedIndex() {
  db._useDatabase("UnitTestsDumpSrc");

  let analyzers = require("@arangodb/analyzers");
  let a = analyzers.save("AqlAnalyzerRound", "aql", { queryString: "RETURN ROUND(@param)" }, ["frequency", "norm", "position"]);
  let b = analyzers.save("myDelimiterAnalyzer", "delimiter", { delimiter: " " }, ["frequency", "norm", "position"]);
  let c = analyzers.save("AqlAnalyzerHash", "aql", { queryString: "return to_hex(to_string(@param))" }, []);
  let d = analyzers.save("norm_lower", "norm", {locale: "de.utf-8", case: "lower"}, ["frequency", "norm", "position"]);

  // create collections
  let collection_documents_0 = db._create("collection_documents_0", {
    replicationFactor: 2,
    writeConcern: 1,
    numberOfShards: getNoShards(2)});

  // create indexes
  let index = db.collection_documents_0.ensureIndex(
    {
        "type":"inverted",
        "name":"collection_documents_0_all",
        "fields": [
            {
                "analyzer": "AqlAnalyzerHash",
                "name": "_key",
                "features": [
                    "norm"
                ]
            },
            {
                "name": "_rev",
                "features": [
                    "position", "frequency"
                ]
            },
            {
                "name": "Familie.Mutter",
                "includeAllFields": true
            },
            {
                "name": "Familie.Mutter.Name",
                "includeAllFields": true
            },
            {
                "name": "Familie.Vater.Name",
                "includeAllFields": true,
                "trackListPositions": true,
                "analyzer": "AqlAnalyzerHash"
            },
            {
                "name": "Familie.Ehefrau.Setzt[*].Titel",
                "analyzer": "AqlAnalyzerHash"
            },
            {
                "name": "Familie.Bruder.Name",
                "analyzer": "norm_lower"
            },
            {
                "name": "Familie.Bruder.Familienname",
                "analyzer": "AqlAnalyzerHash"
            },
            {
                "name": "Familie.Bruder.Setzt[*].istGut",
                "analyzer": "AqlAnalyzerRound"

            },
            {
                "name": "Setzt",
            },
            {
              "name": "Haustiere",
            }
        ],
        "includeAllFields": false,
        "trackListPositions": false,
        "features": [],
        "analyzer": "myDelimiterAnalyzer"
    }
  );

  // create views
  let search = db._createView('v_collection_documents_0_all', 'search-alias', {
    'indexes': [
        {
            'collection': 'collection_documents_0',
            'index': 'collection_documents_0_all'
        }
    ]
  });

  collection_documents_0.save({value: -1, text: "the red foxx jumps over the pond"});
  collection_documents_0.save(Array(5000).fill().map((e, i, a) => Object({_key: "test" + i, value: i})));
}

function createReplicationFactors() {
  db._create("UnitTestsDumpReplicationFactor1", {replicationFactor: 2, numberOfShards: getNoShards(7)});
  db._create("UnitTestsDumpReplicationFactor2", {replicationFactor: 2, numberOfShards: getNoShards(6)});
}

function createJobs() {
  db._useDatabase("UnitTestsDumpSrc");
  // insert an entry into _jobs collection
  try {
    db._jobs.remove("test");
  } catch (err) {
  }
  db._jobs.insert({_key: "test", status: "completed"});

  // insert an entry into _queues collection
  try {
    db._queues.remove("test");
  } catch (err) {
  }
  db._queues.insert({_key: "test"});
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
}

function createAnalyzers() {
  db._useDatabase("UnitTestsDumpDst");
  const countBeforeInDst = db._analyzers.count();
  analyzers.save("custom_dst", "delimiter", {delimiter: ","}, ["frequency"]);
  assertEqual(db._analyzers.count(), countBeforeInDst + 1);
  // ^-- Note: I guess this is just a test that non-database relevant analyzers will not get restored as well.

  db._useDatabase("UnitTestsDumpSrc");
  const countBeforeInSrc = db._analyzers.count();
  analyzers.save("customInCreateAnalyzers", "delimiter", {delimiter: ","}, ["frequency"]);
  assertEqual(db._analyzers.count(), countBeforeInSrc + 1);
}

/**
 * Chooses the right graph module from the given string.
 * @param kind: "SmartGraph", "Disjoint" (for disjoint smart graph), "Satellite", "Hybrid", "HybridDisjoint"
 * @returns: the module
 */
function chooseGraphModule(kind) {
  switch (kind) {
    case "SmartGraph":
    case "Disjoint":
    case "Hybrid":
    case "HybridDisjoint":
      return sgm;
    case "Satellite":
      return satgm;
    case "EnterpriseGraph":
      return egm;
    default:
      throw "A wrong parameter in the test!";
  }
}

/**
 * @brief Only if enterprise mode:
 *        Creates an empty SmartGraph with no edge relations with given options.
 *        @param kind: one of "SmartGraph", "Disjoint", "Satellite"
 *        @param options: a document
 */
function createEmptyGraph(kind, options) {
  if (!isEnterprise) {
    return;
  }

  db._useDatabase("UnitTestsDumpSrc");

  let graphModule = chooseGraphModule(kind);

  let graphName;
  switch (kind) {
    case "SmartGraph":
      graphName = "UnitTestsDumpEmptySmartGraph";
      break;
    case "Satellite":
      graphName = "UnitTestsDumpEmptySatelliteGraph";
      break;
    case "Disjoint":
      graphName = "UnitTestsDumpEmptyDisjointGraph";
      break;
    case "EnterpriseGraph":
      graphName = "UnitTestsDumpEmptyEnterpriseGraph";
  }

  if (graphModule._exists(graphName)) {
    assertTrue(false, "Graph already exists. Please check setup configuration.");
  }

  graphModule._create(graphName, [], [], options);
}


/**
 * @brief Only if enterprise mode:
 *        Creates an empty SmartGraph sharded by `value`, with no edge relations.
 */
function createEmptySmartGraph() {
  createEmptyGraph("SmartGraph", {
    numberOfShards: getNoShards(5),
    replicationFactor: 2,
    smartGraphAttribute: "value"
  });
}

/**
 * @brief Only if enterprise mode:
 *        Creates an empty EnterpriseGraph, with no edge relations.
 */
function createEmptyEnterpriseGraph() {
  createEmptyGraph("EnterpriseGraph", {
    numberOfShards: getNoShards(5),
    replicationFactor: 2,
    isSmart: true
  });
}

/**
 * @brief Only if enterprise mode:
 *        Creates an empty Satellite with no edge relations.
 */
function createEmptySatelliteGraph() {
  createEmptyGraph("Satellite", {
    numberOfShards: getNoShards(5),
    replicationFactor: 'satellite'
  });
}

/**
 * @brief Only if enterprise mode:
 *        Creates an empty disjoint SmartGraph with no edge relations.
 */
function createEmptyDisjointGraph() {
  createEmptyGraph("Disjoint", {
    numberOfShards: getNoShards(5),
    replicationFactor: 2,
    smartGraphAttribute: "value",
    isDisjoint: true
  });
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
  if (sgm._exists(smartGraphName)) {
    assertTrue(false, "Graph already exists. Please check setup configuration.");
  }
  assertCollectionsAreNew([edges, vertices, orphans]);

  sgm._create(smartGraphName, [sgm._relation(edges, vertices, vertices)],
    [orphans], {numberOfShards: getNoShards(5), smartGraphAttribute: "value"});

  let vDocs = [];
  for (let i = 0; i < 100; ++i) {
    vDocs.push({value: String(i)});
  }
  let saved = db[vertices].save(vDocs).map(v => v._id);
  let eDocs = [];
  for (let i = 0; i < 100; ++i) {
    eDocs.push({_from: saved[(i + 1) % 100], _to: saved[i], value: String(i)});
    eDocs.push({_from: saved[i], _to: saved[i], value: String(i)});
    eDocs.push({_from: saved[i], _to: saved[(i + 1) % 100], value: String(i)});
  }
  db[edges].save(eDocs);
  db[orphans].save(vDocs);
}

/**
 * @brief Only if enterprise mode:
 *        Creates a SmartGraph sharded by `value`
 *        without any data in it.
 *        In 3.11 and earlier the SmartGraph definition
 *        does contain a CollectionID of leading collection
 *        this prohibits that the GraphSchema could be altered
 *        after restore.
 *        We do not write the CollectionID anymore, but we
 *        can still restore it from old versions.
 */
function createSmartGraph3_11_compat() {
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestDumpSmartGraphCompat";
  const edges = "UnitTestDumpSmartEdgesCompat";
  const vertices = "UnitTestDumpSmartVerticesCompat";
  const orphans = "UnitTestDumpSmartOrphansCompat";
  if (sgm._exists(smartGraphName)) {
    assertTrue(false, "Graph already exists. Please check setup configuration.");
  }
  assertCollectionsAreNew([edges, vertices, orphans]);

  sgm._create(smartGraphName, [sgm._relation(edges, vertices, vertices)],
              [orphans], {numberOfShards: getNoShards(5), smartGraphAttribute: "value"});
    /* This code fakes previous behaviour */
  const leader = db._collection(vertices).properties().distributeShardsLike || vertices;
  const cid = db._collection(leader)._id;
  db._graphs.update(smartGraphName, {initialCid: cid});
}

/**
 * @brief Only if enterprise mode:
 *        Creates an EnterpriseGraph
 *        That has 100 vertices (value 0 -> 99)
 *        That has 100 orphans (value 0 -> 99)
 *        That has 300 edges, for each value i:
 *          Connect i -> i
 *          Connect i - 1 <- i
 *          Connect i -> i + 1
 */
function createEnterpriseGraph() {
  if (!isEnterprise) {
    return;
  }

  const enterpriseGraphName = "UnitTestDumpEnterpriseGraph";
  const edges = "UnitTestDumpEnterpriseEdges";
  const vertices = "UnitTestDumpEnterpriseVertices";
  const orphans = "UnitTestDumpEnterpriseOrphans";
  if (egm._exists(enterpriseGraphName)) {
    assertTrue(false, "Graph already exists. Please check setup configuration.");
  }
  assertCollectionsAreNew([edges, vertices, orphans]);

  egm._create(enterpriseGraphName, [sgm._relation(edges, vertices, vertices)],
    [orphans], {numberOfShards: getNoShards(5), isSmart: true});

  let vDocs = [];
  for (let i = 0; i < 100; ++i) {
    vDocs.push({value: String(i), _key: String(i)});
  }
  let saved = db[vertices].save(vDocs).map(v => v._id);
  let eDocs = [];
  for (let i = 0; i < 100; ++i) {
    eDocs.push({_from: saved[(i + 1) % 100], _to: saved[i], value: String(i)});
    eDocs.push({_from: saved[i], _to: saved[i], value: String(i)});
    eDocs.push({_from: saved[i], _to: saved[(i + 1) % 100], value: String(i)});
  }
  db[edges].save(eDocs);
  db[orphans].save(vDocs);
}

/**
 * A cycle of length 100 with a symmetric edge relation and self-loops on every vertex
 */
function createSmartGraphSingleServer() {
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestDumpSmartGraphSingleServer";
  const edges = "UnitTestDumpSmartEdgesSingleServer";
  const vertices = "UnitTestDumpSmartVerticesSingleServer";
  const orphans = "UnitTestDumpSmartOrphansSingleServer";

  createGraphWithoutData({
    smartGraphName: smartGraphName,
    edges: edges,
    fromVertices: [vertices],
    toVertices: [vertices],
    orphans: [orphans]
  });
  
  let properties = db[edges].properties();
  assertTrue(properties.isSmart);
  properties = db[vertices].properties();
  assertTrue(properties.isSmart);

  const size = 100;

  let docs = fillAndSaveVDocs(size, vertices);
  let eDocs = fillEDocs([docs.saved]);

  db[edges].save(eDocs);
  db[orphans].save(docs.vDocs);
}

/**
 * A cycle of length 100 with a symmetric edge relation and self-loops on every vertex
 */
function createEnterpriseGraphSingleServer() {
  if (!isEnterprise) {
    return;
  }

  const enterpriseGraphName = "UnitTestDumpEnterpriseGraphSingleServer";
  const edges = "UnitTestDumpEnterpriseEdgesSingleServer";
  const vertices = "UnitTestDumpEnterpriseVerticesSingleServer";
  const orphans = "UnitTestDumpEnterpriseOrphansSingleServer";

  createGraphWithoutData({
    smartGraphName: enterpriseGraphName,
    edges: edges,
    fromVertices: [vertices],
    toVertices: [vertices],
    orphans: [orphans]
  }, undefined, "EnterpriseGraph");

  let properties = db[edges].properties();
  assertTrue(properties.isSmart);
  properties = db[vertices].properties();
  assertTrue(properties.isSmart);

  const size = 100;

  let docs = fillAndSaveVDocs(size, vertices, true);
  let eDocs = fillEDocs([docs.saved]);

  db[edges].save(eDocs);
  db[orphans].save(docs.vDocs);
}

/**
 * Two disjoint cycles of length 50 each with a symmetric edge relation and self-loops on every vertex
 */
function createDisjointGraphSingleServer() {
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestDumpDisjointGraphSingleServer";
  const edges = "UnitTestDumpDisjointEdgesSingleServer";
  const vertices = "UnitTestDumpDisjointVerticesSingleServer";
  const orphans = "UnitTestDumpDisjointOrphansSingleServer";

  createGraphWithoutData(
    {
      smartGraphName: smartGraphName,
      edges: edges,
      fromVertices: [vertices],
      toVertices: [vertices],
      orphans: [orphans]
    },
    {isDisjoint: true, replicationFactor: 2}
  );

  const size = 50;

  const docs1 = fillAndSaveVDocs(size, vertices);
  const docs2 = fillAndSaveVDocs(size, vertices);
  const eDocs = fillEDocs([docs1.saved, docs2.saved], true);

  db[edges].save(eDocs);
  db[orphans].save(docs1.vDocs);
  db[orphans].save(docs2.vDocs);
}

/**
 * Two disjoint cycles of length 50 each with a symmetric edge relation and self-loops on every vertex
 */

function createSatelliteGraphSingleServer() {
  if (!isEnterprise) {
    return;
  }

  const graphName = "UnitTestDumpSatelliteGraphSingleServer";
  const vertices = "UnitTestDumpSatelliteVerticesSingleServer";
  const edges = "UnitTestDumpSatelliteEdgesSingleServer";
  const orphans = "UnitTestDumpSatelliteOrphansSingleServer";

  createGraphWithoutData({
      smartGraphName: graphName,
      edges: edges,
      fromVertices: [vertices],
      toVertices: [vertices],
      orphans: [orphans]
    },
    {
      isSatellite: true, isSmart: false /*needed: default is true - only for internal helper method api*/,
      replicationFactor: 'satellite'
    });

  const size = 50;

  const docs1 = fillAndSaveVDocs(size, vertices);
  const docs2 = fillAndSaveVDocs(size, vertices);
  const allDocs = docs1.saved.concat(docs2.saved);
  const eDocs = fillEDocs([allDocs]);

  db[edges].save(eDocs);
  db[orphans].save(docs1.vDocs);
  db[orphans].save(docs2.vDocs);
}

function createHybridSmartGraphSingleServer() {
  if (!isEnterprise) {
    return;
  }

  const hybridGraphName = "UnitTestDumpHybridSmartGraphSingleServer";
  const verticesNonSat = "UnitTestDumpHybridSmartVerticesNonSatSingleServer";
  const verticesSat = "UnitTestDumpHybridSmartVerticesSatSingleServer";
  const edges = "UnitTestDumpHybridSmartEdgesSingleServer";
  const orphans = "UnitTestDumpHybridSmartOrphansSingleServer";

  assertCollectionsAreNew([edges, verticesNonSat, verticesSat, orphans]);

  const relSatNonSat = sgm._relation(edges, verticesSat, verticesNonSat);

  const g = sgm._create(hybridGraphName, [relSatNonSat], [orphans], {
    satellites: [verticesSat],
    smartGraphAttribute: 'value',
    replicationFactor: 2
  });

  // create vertices
  const size = 50;

  const docsSat = fillAndSaveVDocs(size, verticesSat);
  const docsNonSat = fillAndSaveVDocs(size, verticesNonSat);
  let eDocs = [];
  for (let i = 0; i < size; ++i) {
    eDocs.push({_from: docsSat.saved[i], _to: docsNonSat.saved[i], value: String(i)});
    eDocs.push({_from: docsSat.saved[i], _to: docsNonSat.saved[(i + 1) % size], value: String(i)});
  }

  db[edges].save(eDocs);

  db[orphans].save(docsSat.vDocs);
  db[orphans].save(docsNonSat.vDocs);
}

/**
 * @brief Creates a graph with one satellite vertex collection (verticesSat), two non-satellite (smart)
 * vertex collections (verticesANonSat and verticesBNonSat) connected to other collections, an orphan collection
 * and two edge relations: edgeSatA from verticesSat to verticesANonSat and edgesAB
 * from verticesANonSat to verticesBNonSat. The size of every vertex collection is 50. In each vertex collection,
 * the vertices are enumerated by 'value' (which is the smartGraphAttribute) from 0 to 49. Edges from edgeSatA go
 * from vertex i (in verticesSat) to vertex i (in verticesANonSat) and from vertex i to vertex (i+1) % 50. In total,
 * we have 50 + 50 = 100 edges in this collection.
 * Edges from edgeAB are constructed with the same function but go only  from vertex i to vertex i because both vertex
 * collections are not satellite and vertices with different values of 'value' (i.e., i and (i+1) % 50) cannot be
 * connected in a disjoint graph. Thus in total we have 50 edges in this collection.
 *
 * This function is too special and does not use createGraphWithoutData().
 */
function createHybridDisjointSmartGraphSingleServer() {
  if (!isEnterprise) {
    return;
  }

  const hybridGraphName = "UnitTestDumpHybridDisjointSmartGraphSingleServer";
  const verticesANonSat = "UnitTestDumpHybridDisjointSmartVerticesANonSatSingleServer";
  const verticesBNonSat = "UnitTestDumpHybridDisjointSmartVerticesBNonSatSingleServer";
  const verticesSat = "UnitTestDumpHybridDisjointSmartVerticesSatSingleServer";
  const edgesSatA = "UnitTestDumpHybridSDisjointmartEdgesSatASingleServer";
  const edgesAB = "UnitTestDumpHybridSDisjointmartEdgesABSingleServer";
  const orphans = "UnitTestDumpHybridSDisjointmartOrphansSingleServer";

  assertCollectionsAreNew([edgesSatA, edgesAB, verticesANonSat, verticesBNonSat, verticesSat, orphans]);

  const relSatANonSat = sgm._relation(edgesSatA, verticesSat, verticesANonSat);
  const relAB = sgm._relation(edgesAB, verticesANonSat, verticesBNonSat);

  sgm._create(hybridGraphName, [relSatANonSat, relAB], [orphans], {
    satellites: [verticesSat],
    isDisjoint: true,
    smartGraphAttribute: 'value',
    replicationFactor: 2
  });

  // create vertices
  const size = 50;

  const docsSat = fillAndSaveVDocs(size, verticesSat);
  const docsANonSat = fillAndSaveVDocs(size, verticesANonSat);
  const docsBNonSat = fillAndSaveVDocs(size, verticesBNonSat);
  let eDocs = [];

  // from Sat to A
  for (let i = 0; i < size; ++i) {
    eDocs.push({_from: docsSat.saved[i], _to: docsANonSat.saved[i], value: String(i)});
    eDocs.push({_from: docsSat.saved[i], _to: docsANonSat.saved[(i + 1) % size], value: String(i)});
  }

  db[edgesSatA].save(eDocs);
  eDocs = [];
  // from A to A &&  B to B
  for (let i = 0; i < size; ++i) {
    eDocs.push({_from: docsANonSat.saved[i], _to: docsANonSat.saved[i], value: String(i)});
    eDocs.push({_from: docsBNonSat.saved[i], _to: docsBNonSat.saved[i], value: String(i)});
  }

  db[edgesAB].save(eDocs);
  db[orphans].save(docsSat.vDocs);
  db[orphans].save(docsANonSat.vDocs);
}


function createSatelliteGraphWithoutData() {
  if (!isEnterprise) {
    return;
  }

  createGraphWithoutData({
      smartGraphName: "UnitTestDumpSatelliteGraphWithoutData",
      edges: "UnitTestDumpSatelliteEdgesWithoutData",
      fromVertices: ["UnitTestDumpSatelliteVerticesWithoutData"],
      toVertices: ["UnitTestDumpSatelliteVerticesWithoutData"],
      orphans: ["UnitTestDumpSatelliteOrphansWithoutData"]
    },
    {isSatellite: true}
  );
}

function createDisjointGraphWithoutData() {
  if (!isEnterprise) {
    return;
  }

  createGraphWithoutData({
      smartGraphName: "UnitTestDumpDisjointGraphWithoutData",
      edges: "UnitTestDumpDisjointEdgesWithoutData",
      fromVertices: ["UnitTestDumpDisjointVerticesWithoutData"],
      toVertices: ["UnitTestDumpDisjointVerticesWithoutData"],
      orphans: ["UnitTestDumpDisjointOrphansWithoutData"]
    },
    {isDisjoint: true, replicationFactor: 2}
  );
}

/**
 * Fill the \a vDocs returned array with numbers from 0 to \a size, save them
 * into \a db under \a verticesName and return the object {saved: <saved array>, vDocs: <the values>>}
 * @param size: upper limit to the numbers to be saved
 * @param verticesName: the name of the collection to save the numbers
 * @returns {{saved, vDocs: *[]}}
 */
function fillAndSaveVDocs(size, verticesName, setKeyValue) {
  let vDocs = [];
  const startValue = db[verticesName] ? db[verticesName].count() : 0;
  for (let i = startValue; i < startValue + size; ++i) {
    if (setKeyValue) {
      vDocs.push({value: String(i), _key: String(i)});
    } else {
      vDocs.push({value: String(i)});
    }
  }

  return {saved: db[verticesName].save(vDocs).map(v => v._id), vDocs: vDocs};
}


/**
 * Returns the array of edges that, on each already saved (this is not checked) vertex collection from \a savedVDocsColls,
 * induce an undirected cycle with self-loops (i.e., edges build symmetric pairs + self-loops). Note: For disjoint graphs,
 * edges with different value of 'value' are not saved.
 * @param savedVDocsColls: an array of saved collections
 * @returns {*[]}
 */
function fillEDocs(savedVDocsColls = [], isDisjoint = false) {
  let eDocs = [];
  for (let ell = 0; ell < savedVDocsColls.length; ++ell) {
    const savedColl = savedVDocsColls[ell];
    const size = savedColl.length;

    for (let i = 0; i < size; ++i) {
      if (!isDisjoint) {
        eDocs.push({_from: savedColl[(i + 1) % size], _to: savedColl[i], value: String(i)});
        eDocs.push({_from: savedColl[i], _to: savedColl[(i + 1) % size], value: String(i)});
      }

      eDocs.push({_from: savedColl[i], _to: savedColl[i], value: String(i)});
    }
  }
  return eDocs;
}

/**
 * @brief Only if enterprise mode:
 *        Creates a SmartGraph, and a single edge relation: if \a toVertices is given and not "", then from
 *        \a fromvertices to \a toVertices, otherwise from \a fromVertices to \a toVertices.
 * @param names a document that can contain the following keys:
 * smartGraphName: the name of the graph,
 * edges: the name for the edge collection,
 * fromVertices: an array of collection names for edge tails,
 * toVertices: an array of collection names for edge headsm
 * orphans: an array of collection names for vertices not incident to any edge.
 * @param options a document that can contain the following keys:
 * numberOfShards (default: 5), smartGraphAttribute (default: "values"), isDisjoint (default: false),
 * isSatellite (default: false), satellites (not contained by default), isSmart (default: true)
 */
function createGraphWithoutData(names, options = undefined, kind) {
  if (!isEnterprise) {
    return;
  }
  db._useDatabase("UnitTestsDumpSrc");

  if (!names) {
    assertTrue(false, "Names property is not set!");
  }


  // prepare graph module
  let graphKind;
  if (kind) {
    graphKind = kind;
  } else {
    if (options && options.isSatellite) {
      graphKind = "Satellite";
    } else {
      graphKind = "SmartGraph";
    }
  }
  const graphModule = chooseGraphModule(graphKind);

  // set default options if not given as a parameter
  if (options === undefined) {
    options = {
      numberOfShards: getNoShards(5),
      replicationFactor: 2
    };
    if (graphKind !== "EnterpriseGraph") {
      options.smartGraphAttribute = "value";
    } else {
      options.isSmart = true;
    }
  } else {
    if (options.numberOfShards === undefined) {
      options.numberOfShards = getNoShards(5);
    } else {
      options.numberOfShards = getNoShards(options.numberOfShards);
    }
    if (options.smartGraphAttribute === undefined) {
      if (graphKind !== "EnterpriseGraph") {
        options.smartGraphAttribute = "value";
      }
    }
    if (options.isDisjoint === undefined) {
      options.isDisjoint = false;
    }
    if (options.isSatellite === undefined) {
      options.isSatellite = false;
    }
    if (options.isSmart === undefined || graphKind === "EnterpriseGraph") {
      options.isSmart = true;
    }
  }

  assertCollectionsAreNew([names.edges, names.fromVertices, names.toVertices, names.orphans]);

  // note: throw an error if the graph already exists (which shouldn't happen)
  graphModule._create(
    names.smartGraphName,
    [graphModule._relation(names.edges, names.fromVertices, names.toVertices)],
    names.orphans,
    options
  );
}

function createSmartGraphWithoutData() {
  // External method for dump-setup-mixed.js to create a default SmartGraph
  // Using helper graph creation method defined above: createGraphWithoutData
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestDumpSmartGraphWithoutData";
  const edges = "UnitTestDumpSmartEdgesWithoutData";
  const vertices = "UnitTestDumpSmartVerticesWithoutData";
  const orphans = "UnitTestDumpSmartOrphansWithoutData";

  const names = {
    smartGraphName: smartGraphName,
    edges: edges,
    fromVertices: [vertices],
    toVertices: [vertices],
    orphans: [orphans]
  };

  createGraphWithoutData(names);
}

function createEnterpriseGraphWithoutData() {
  // External method for dump-setup-mixed.js to create a default SmartGraph
  // Using helper graph creation method defined above: createGraphWithoutData
  if (!isEnterprise) {
    return;
  }

  const enterpriseGraphName = "UnitTestDumpEnterpriseGraphWithoutData";
  const edges = "UnitTestDumpEnterpriseEdgesWithoutData";
  const vertices = "UnitTestDumpEnterpriseVerticesWithoutData";
  const orphans = "UnitTestDumpEnterpriseOrphansWithoutData";

  const names = {
    smartGraphName: enterpriseGraphName,
    edges: edges,
    fromVertices: [vertices],
    toVertices: [vertices],
    orphans: [orphans]
  };

  createGraphWithoutData(names, undefined, "EnterpriseGraph");
}

/**
 * @brief Only if enterprise mode:
 *        Creates an arangosearch over smart edge collection
 */
function createSmartArangoSearch() {
  db._useDatabase("UnitTestsDumpSrc");
  if (!isEnterprise) {
    return;
  }

  db._dropView("UnitTestsDumpSmartView");

  let analyzers = require("@arangodb/analyzers");
  const countBefore = db["_analyzers"].count();
  analyzers.save("smartCustom", "delimiter", {delimiter: "smart"}, ["frequency"]);
  assertEqual(db["_analyzers"].count(), countBefore + 1);

  db._createView("UnitTestsDumpSmartView", "arangosearch", {
    // choose non default values to check if they are correctly dumped and imported
    cleanupIntervalStep: 456,
    consolidationPolicy: {
      threshold: 0.3,
      type: "bytes_accum"
    },
    consolidationIntervalMsec: 0,
    links: {
      "UnitTestDumpSmartEdges": {
        includeAllFields: true,
        fields: {
          text: {analyzers: ["text_en", "smartCustom"]}
        }
      }
    }
  });
}

function assertCollectionsAreNew(collectionNames) {
  for (let existingCollection of db._collections()) {
    const existingCollectionName = existingCollection.name();
    for (let collectionName of collectionNames) {
      if (existingCollectionName === collectionName) {
        throw "Error in test: this collection name already exists: " + collectionName;
      }
    }
  }
}

/**
 * @brief Only if enterprise mode:
 *        Creates a SatelliteCollection with 100 documents
 */
function createSatelliteCollections() {
  if (!isEnterprise) {
    return;
  }

  const satelliteCollectionName = "UnitTestDumpSatelliteCollection";
  assertCollectionsAreNew([satelliteCollectionName]);
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
  if (!isEnterprise || (db._properties().sharding === "single")) {
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
  const expected = [_.range(0, 100 + 1).map(i => "v" + (i % 100 + 1))];
  assertEqual(expected, res.toArray());
}

exports.createEmptySmartGraph = createEmptySmartGraph;
exports.createEmptyEnterpriseGraph = createEmptyEnterpriseGraph;
exports.createEmptySatelliteGraph = createEmptySatelliteGraph;
exports.createEmptyDisjointGraph = createEmptyDisjointGraph;

exports.createSmartGraphWithoutData = createSmartGraphWithoutData;
exports.createEnterpriseGraphWithoutData = createEnterpriseGraphWithoutData;
exports.createSatelliteGraphWithoutData = createSatelliteGraphWithoutData;
exports.createDisjointGraphWithoutData = createDisjointGraphWithoutData;

exports.createSmartGraph = createSmartGraph;
exports.createSmartGraph3_11_compat = createSmartGraph3_11_compat;

exports.createSmartGraphSingleServer = createSmartGraphSingleServer;
exports.createEnterpriseGraphSingleServer = createEnterpriseGraphSingleServer;
exports.createSatelliteGraphSingleServer = createSatelliteGraphSingleServer;
exports.createDisjointGraphSingleServer = createDisjointGraphSingleServer;
exports.createHybridSmartGraphSingleServer = createHybridSmartGraphSingleServer;
exports.createHybridDisjointSmartGraphSingleServer = createHybridDisjointSmartGraphSingleServer;

exports.createSmartArangoSearch = createSmartArangoSearch;
exports.createSatelliteCollections = createSatelliteCollections;
exports.createSatelliteGraphs = createSatelliteGraphs;

exports.cleanup = cleanup;
exports.createUsers = createUsers;
exports.createEmpty = createEmpty;
exports.createMany = createMany;
exports.createOrder = createOrder;
exports.createModifyCollection = createModifyCollection;
exports.createExtendedName = createExtendedName;
exports.createMultiShard = createMultiShard;
exports.createComputedValues = createComputedValues;
exports.createAutoIncKeyGen = createAutoIncKeyGen;
exports.createPaddedKeyGen = createPaddedKeyGen;
exports.createUUIDKeyGen = createUUIDKeyGen;
exports.createStrings = createStrings;
exports.createTransactional = createTransactional;
exports.createPersistent = createPersistent;
exports.createView = createView;
exports.createSearch = createSearch;
exports.createInvertedIndex = createInvertedIndex;
exports.createReplicationFactors = createReplicationFactors;
exports.createJobs = createJobs;
exports.createFoxx = createFoxx;
exports.createAnalyzers = createAnalyzers;
exports.extendedName = extendedName;
