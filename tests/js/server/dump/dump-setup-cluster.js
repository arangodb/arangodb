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

/**
 * @brief Only if enterprise mode:
 *        Creates a smart graph sharded by `value`
 *        That has 100 vertices (value 0 -> 99)
 *        That has 100 orphans (value 0 -> 99)
 *        That has 300 edges, for each value i:
 *          Connect i -> i
 *          Connect i - 1 -> i
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
 *        Creates a satellite collection with 100 documents
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

(function () {
  var i, c;

  try {
    db._dropDatabase("UnitTestsDumpSrc");
  } catch (err1) {
  }
  db._createDatabase("UnitTestsDumpSrc");

  try {
    db._dropDatabase("UnitTestsDumpDst");
  } catch (err2) {
  }
  db._createDatabase("UnitTestsDumpDst");


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
  if (db._engine().name !== "rocksdb") {
    c.ensureGeoIndex("a_la", "a_lo");
  }

  // we insert data and remove it
  c = db._create("UnitTestsDumpTruncated", { isVolatile: db._engine().name === "mmfiles" });
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
            text: { analyzers: [ "text_en" ] }
          }
        }
      }
    });

    for (i = 0; i < 5000; ++i) {
      c.save({ _key: "test" + i, value: i});
    }
    c.save({ value: -1, text: "the red foxx jumps over the pond" });
  } catch (err) { }

  setupSmartGraph();
  setupSatelliteCollections();

  // Install Foxx
  const fs = require('fs');
  const SERVICE_PATH = fs.makeAbsolute(fs.join(
    require('internal').pathForTesting('common'), 'test-data', 'apps', 'minimal-working-service'
  ));
  const FoxxManager = require('@arangodb/foxx/manager');
  FoxxManager.install(SERVICE_PATH, '/test');
})();

return {
  status: true
};

