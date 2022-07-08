/* jshint globalstrict:false, strict:false */
/* global assertEqual, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let internal = require('internal');
let db = arangodb.db;

function FiguresSuite () {
  'use strict';

  const cn = "UnitTestsFigures";
  const isCluster = internal.isCluster();
  const isEnterprise = internal.isEnterprise();

  return {
    
    testNonDetailed: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let figures = c.figures(false);
        assertFalse(figures.hasOwnProperty("rocksdb"));
      } finally {
        db._drop(cn);
      }
    },

    testDetailedEmpty: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let figures = c.figures(true).engine;
        assertEqual(0, figures.documents);

        let indexes = figures.indexes;
        assertEqual(1, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(0, indexes[0].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedJustPrimary: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      for (let i = 0; i < 100; ++i) {
        c.insert({ value: i });
      }
      try {
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(1, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedEmptyEdge: function () {
      let c = db._createEdgeCollection(cn, { numberOfShards: 4 });
      try {
        let figures = c.figures(true).engine;
        assertEqual(0, figures.documents);

        let indexes = figures.indexes;
        assertEqual(3, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(0, indexes[0].count);
        assertEqual("edge", indexes[1].type);
        assertEqual(1, indexes[1].id);
        assertEqual(0, indexes[1].count);
        assertEqual("edge", indexes[2].type);
        assertEqual(2, indexes[2].id);
        assertEqual(0, indexes[2].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedEdge: function () {
      let c = db._createEdgeCollection(cn, { numberOfShards: 4 });
      try {
        for (let i = 0; i < 100; ++i) {
          c.insert({ _from: "test/a", _to: "test/b" });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(3, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("edge", indexes[1].type);
        assertEqual(1, indexes[1].id);
        assertEqual(100, indexes[1].count);
        assertEqual("edge", indexes[2].type);
        assertEqual(2, indexes[2].id);
        assertEqual(100, indexes[2].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedHash: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "hash", fields: ["value1"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: i, value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-hash", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedHashSparse: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "hash", fields: ["value1"], sparse: true }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: i < 50 ? null : i, value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-hash", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(50, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedHashUnique: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "hash", fields: ["value1", "_key"], unique: true }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: i, value2: "test" + i });
        } 
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-hash", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedHashCombined: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "hash", fields: ["value1", "value2"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: i, value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-hash", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedHashArray: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "hash", fields: ["value1[*]"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: [1, 2, 3], value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-hash", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(300, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedHashArrayDeduplicate: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "hash", fields: ["value1[*]"], deduplicate: true }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: [1, 2, 1], value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-hash", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(200, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedSkiplist: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "skiplist", fields: ["value1"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: i, value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-skiplist", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedPersistent: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "persistent", fields: ["value1"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: i, value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-persistent", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedTtlInvalid: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "ttl", fields: ["value1"], expireAfter: 100000 }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: "piffpaff", value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-ttl", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(0, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedTtl: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "ttl", fields: ["value1"], expireAfter: 100000 }));
        let dt = (new Date()).toISOString();
        for (let i = 0; i < 100; ++i) {
          c.insert({ value1: dt, value2: "test" + i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("rocksdb-ttl", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedGeoInvalid: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "geo", fields: ["lat", "lon"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ lat: "piff", lon: "paff" });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("geo", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(0, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedGeo: function () {
      let c = db._create(cn, { numberOfShards: 4 });
      try {
        let idxs = [];
        idxs.push(c.ensureIndex({ type: "geo", fields: ["lat", "lon"] }));
        for (let i = 0; i < 100; ++i) {
          c.insert({ lat: 50 - i, lon: 50 - i });
        }
        let figures = c.figures(true).engine;
        assertEqual(100, figures.documents);

        let indexes = figures.indexes;
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(0, indexes[0].id);
        assertEqual(100, indexes[0].count);
        assertEqual("geo", indexes[1].type);
        assertEqual(idxs[0].id, cn + '/' + indexes[1].id);
        assertEqual(100, indexes[1].count);
      } finally {
        db._drop(cn);
      }
    },
    
    testDetailedSmartEdge: function () {
      if (!isCluster || !isEnterprise) {
        return;
      }

      db._create(cn + "v", { numberOfShards: 4, isSmart: true, shardKeys: ["_key:"], smartGraphAttribute: "value1" });
      try {
        let c = db._createEdgeCollection(cn + "e", { numberOfShards: 4, isSmart: true, shardKeys: ["_key:"], distributeShardsLike: cn + "v" });
        try {
          for (let i = 0; i < 100; ++i) {
            c.insert({ _from: "test/1:1", _to: "test/2:2", value1: i });
          }
          let figures = c.figures(true).engine;
          assertEqual(100, figures.documents);

          let indexes = figures.indexes;
          assertEqual(3, indexes.length);
          assertEqual("primary", indexes[0].type);
          assertEqual(0, indexes[0].id);
          assertEqual(100, indexes[0].count);
          assertEqual("edge", indexes[1].type);
          assertEqual(1, indexes[1].id);
          assertEqual(100, indexes[1].count);
          assertEqual("edge", indexes[2].type);
          assertEqual(2, indexes[2].id);
          assertEqual(100, indexes[2].count);
        } finally {
          db._drop(cn + "e");
        }
      } finally {
        db._drop(cn + "v");
      }
    },
    
    testDetailedDisjointSmartEdge: function () {
      if (!isCluster || !isEnterprise) {
        return;
      }

      db._create(cn + "v", { numberOfShards: 4, isSmart: true, shardKeys: ["_key:"], smartGraphAttribute: "value1" });
      try {
        let c = db._createEdgeCollection(cn + "e", { numberOfShards: 4, isDisjoint: true, isSmart: true, shardKeys: ["_key:"], distributeShardsLike: cn + "v" });
        try {
          for (let i = 0; i < 100; ++i) {
            c.insert({ _from: "test/1:1", _to: "test/2:2", value1: i });
          }
          let figures = c.figures(true).engine;
          assertEqual(100, figures.documents);

          let indexes = figures.indexes;
          assertEqual(3, indexes.length);
          assertEqual("primary", indexes[0].type);
          assertEqual(0, indexes[0].id);
          assertEqual(100, indexes[0].count);
          assertEqual("edge", indexes[1].type);
          assertEqual(1, indexes[1].id);
          assertEqual(100, indexes[1].count);
          assertEqual("edge", indexes[2].type);
          assertEqual(2, indexes[2].id);
          assertEqual(100, indexes[2].count);
        } finally {
          db._drop(cn + "e");
        }
      } finally {
        db._drop(cn + "v");
      }
    },
    
  };
}

jsunity.run(FiguresSuite);

return jsunity.done();
