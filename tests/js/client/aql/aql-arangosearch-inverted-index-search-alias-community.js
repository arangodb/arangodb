/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alexey Bakharev
////////////////////////////////////////////////////////////////////////////////

let arangodb = require("@arangodb");
let db = arangodb.db;
let internal = require("internal");
let jsunity = require("jsunity");
let errors = arangodb.errors;
let dbName = "InvertedIndexSearchAliasSuiteCommunity";
let isEnterprise = internal.isEnterprise();
const isCluster = require("internal").isCluster();
const { triggerMetrics } = require("@arangodb/test-helper");
const { checkIndexMetrics } = require("@arangodb/test-helper-common");

function IResearchInvertedIndexSearchAliasAqlTestSuiteCommunity() {
    let cleanup = function () {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (err) { }
    };

    let assertInvertedIndexCreation = function (collName, indexDefinition, isNewlyCreated = true) {
        let collection = db._collection(collName);
        assertNotEqual(collection, null);
        let res;
        res = collection.ensureIndex(indexDefinition);
        assertEqual(res["isNewlyCreated"], isNewlyCreated);
        // SEARCH-385
        res = collection.ensureIndex(indexDefinition);
        assertEqual(res["isNewlyCreated"], false);
    };

    return {
        setUpAll: function () {
            cleanup();

            db._createDatabase(dbName);
            db._useDatabase(dbName);

            const analyzers = require("@arangodb/analyzers");
            analyzers.save("AqlAnalyzerRound", "aql", { queryString: "RETURN ROUND(@param)" }, ["frequency", "norm", "position"]);
            analyzers.save("myDelimiterAnalyzer", "delimiter", { delimiter: " " }, ["frequency", "norm", "position"]);
            analyzers.save("AqlAnalyzerHash", "aql", { queryString: "return to_hex(to_string(@param))" }, []);
            analyzers.save("norm_lower", "norm", { locale: "de.utf-8", case: "lower" }, ["frequency", "norm", "position"]);
            analyzers.save("norm_lower_dup", "norm", { locale: "de.utf-8", case: "lower" }, ["frequency", "norm", "position"]);

            db._create("c0");
            db._create("c1");
            db._create("c2");
            db._create("c3");
        },
        tearDownAll: function () {
            cleanup();
        },


        testCreation: function () {
            // CREATE INDEXES
            {
                // useless index
                assertInvertedIndexCreation("_analyzers",
                    {
                        'type': 'inverted', 'name': 'system_statistics_system',
                        'fields': [
                            "_key",
                            "_rev",
                            "system.minorPageFaultsPerSecond",
                            "system.majorPageFaultsPerSecond",
                            "system.userTimePerSecond",
                            "system.systemTimePerSecond",
                            "system.residentSize",
                            "system.residentSizePercent",
                            "system.virtualSize",
                            "system.numberOfThreads"
                        ],
                        'analyzer': "AqlAnalyzerRound"
                    });

                assertInvertedIndexCreation("_analyzers",
                    {
                        'type': 'inverted', 'name': 'system_analyzer_system',
                        'fields': [
                            "_key",
                            "_rev",
                            "name"
                        ],
                        'analyzer': "norm_lower"
                    }
                );

                assertInvertedIndexCreation("c0",
                    {
                        "type": "inverted",
                        "name": "c0_i0",
                        "fields": [
                            "a"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerRound"
                    });

                assertInvertedIndexCreation("c0",
                    {
                        "type": "inverted",
                        "name": "test",
                        "fields": [
                            "a"
                        ]
                    });

                assertInvertedIndexCreation("c0",
                    {
                        "type": "inverted",
                        "name": "c0_i1",
                        "fields": [
                            "e"
                        ],
                        "includeAllFields": false,
                        "analyzer": "myDelimiterAnalyzer"
                    });

                assertInvertedIndexCreation("c0",
                    {
                        "type": "inverted",
                        "name": "c0_i2",
                        "fields": [
                            "f"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerHash"
                    });

                assertInvertedIndexCreation("c0",
                    {
                        "type": "inverted",
                        "name": "c0_i3",
                        "fields": [
                            "field"
                        ],
                        "includeAllFields": true,
                        "analyzer": "norm_lower_dup"
                    });

                assertInvertedIndexCreation("c1",
                    {
                        "type": "inverted",
                        "name": "c1_i0",
                        "fields": [
                            "a"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerRound"
                    });

                assertInvertedIndexCreation("c1",
                    {
                        "type": "inverted",
                        "name": "c1_i1",
                        "fields": [
                            "b"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerHash"
                    });

                assertInvertedIndexCreation("c1",
                    {
                        "type": "inverted",
                        "name": "c1_i2",
                        "fields": [
                            "c"
                        ],
                        "includeAllFields": true,
                        "storedValues": [{ "fields": ["b"] }]
                    });

                assertInvertedIndexCreation("c1",
                    {
                        "type": "inverted",
                        "name": "c1_i3",
                        "fields": [
                            {
                                "name": "a.b.c.d.e",
                                "storedValues": [{ "fields": ["c"] }]
                            }
                        ]
                    });

                assertInvertedIndexCreation("c2",
                    {
                        "type": "inverted",
                        "name": "c2_i0",
                        "fields": [
                            "a"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerRound"
                    });

                assertInvertedIndexCreation("c2",
                    {
                        "type": "inverted",
                        "name": "c2_i1",
                        "fields": [
                            "c"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerHash"
                    });

                assertInvertedIndexCreation("c2",
                    {
                        "type": "inverted",
                        "name": "c2_i2",
                        "fields": [
                            {
                                "name": "c",
                                "searchField": true
                            }
                        ]
                    });

                assertInvertedIndexCreation("c2",
                    {
                        "type": "inverted",
                        "name": "c2_i3",
                        "fields": [
                            {
                                "name": "a.b.c.d.e[*]",
                                "analyzer": "AqlAnalyzerHash"
                            }
                        ]
                    });

                assertInvertedIndexCreation("c2",
                    {
                        "type": "inverted",
                        "name": "c2_i4",
                        "fields": [
                            "c"
                        ],
                        "includeAllFields": true,
                        "storedValues": [{ "fields": ["d"] }]
                    });

                assertInvertedIndexCreation("c2",
                    {
                        "type": "inverted",
                        "name": "c2_i5",
                        "fields": [
                            {
                                "name": "fff"
                            }
                        ],
                        "primarySort": {
                            "fields": [
                                {
                                    "field": "foo",
                                    "direction": "desc"
                                },
                                {
                                    "field": "foo.boo",
                                    "direction": "asc"
                                }
                            ]
                        }
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i0",
                        "fields": [
                            "a"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerRound"
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i1",
                        "fields": [
                            "d"
                        ],
                        "includeAllFields": true,
                        "analyzer": "AqlAnalyzerHash"
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i2",
                        "fields": [
                            {
                                "name": "c",
                                "searchField": false
                            }
                        ]
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i3",
                        "fields": [
                            "d[*]"
                        ],
                        "analyzer": "AqlAnalyzerHash"
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i4",
                        "fields": [
                            "field2"
                        ],
                        "includeAllFields": true,
                        "analyzer": "norm_lower"
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i5",
                        "fields": [
                            {
                                "name": "a.b.c.d.e[*]",
                                "analyzer": "norm_lower"
                            }
                        ]
                    });

                assertInvertedIndexCreation("c3",
                    {
                        "type": "inverted",
                        "name": "c3_i6",
                        "fields": [
                            {
                                "name": "fff"
                            }
                        ],
                        "primarySort": {
                            "fields": [
                                {
                                    "field": "foo",
                                    "direction": "asc"
                                },
                                {
                                    "field": "foo.boo",
                                    "direction": "desc"
                                }
                            ]
                        }
                    });
            }

            // CREATE VIEWS
            {
                db._createView('v0', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        },
                        {
                            'collection': 'c1',
                            'index': 'c1_i0'
                        },
                        {
                            'collection': 'c2',
                            'index': 'c2_i0'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i0'
                        },

                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'del'
                        },
                        {
                            'collection': 'c1',
                            'index': 'c1_i0',
                            'operation': 'del'
                        },
                        {
                            'collection': 'c2',
                            'index': 'c2_i0',
                            'operation': 'del'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i0',
                            'operation': 'del'
                        }
                    ]
                });
                db._createView('v1', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        },
                        {
                            'collection': 'c1',
                            'index': 'c1_i0'
                        },
                        {
                            'collection': 'c2',
                            'index': 'c2_i0'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i0'
                        }
                    ]
                });
                db._createView('v2', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i2'
                        },
                        {
                            'collection': 'c1',
                            'index': 'c1_i1'
                        },
                        {
                            'collection': 'c2',
                            'index': 'c2_i1'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i1'
                        }
                    ]
                });
                // one view per index
                db._createView('v_c0_i0', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        }
                    ]
                });
                db._createView('v_c0_i1', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i1'
                        }
                    ]
                });
                db._createView('v_c0_i2', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i2'
                        }
                    ]
                });
                db._createView('v_c0_i3', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i3'
                        }
                    ]
                });
                db._createView('v_c1_i0', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c1',
                            'index': 'c1_i0'
                        }
                    ]
                });
                db._createView('v_c1_i1', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c1',
                            'index': 'c1_i1'
                        }
                    ]
                });

                db._createView('v_c1_i2', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c1',
                            'index': 'c1_i2'
                        }
                    ]
                });
                db._createView('v_c1_i3', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c1',
                            'index': 'c1_i3'
                        }
                    ]
                });

                db._createView('v_c2_i0', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i0'
                        }
                    ]
                });

                db._createView('v_c2_i1', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i1'
                        }
                    ]
                });

                db._createView('v_c2_i2', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i2'
                        }
                    ]
                });

                db._createView('v_c2_i3', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i3'
                        }
                    ]
                });

                db._createView('v_c2_i4', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i4'
                        }
                    ]
                });

                db._createView('v_c2_i5', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i5'
                        }
                    ]
                });

                db._createView('v_c3_i0', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i0'
                        }
                    ]
                });

                db._createView('v_c3_i1', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i1'
                        }
                    ]
                });

                db._createView('v_c3_i2', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i2'
                        }
                    ]
                });

                db._createView('v_c3_i3', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i3'
                        }
                    ]
                });

                db._createView('v_c3_i4', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i4'
                        }
                    ]
                });

                db._createView('v_c3_i5', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i5'
                        }
                    ]
                });

                db._createView('v_c3_i6', 'search-alias', {
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i6'
                        }
                    ]
                });
            }

        },

        testNegativeInvertedIndexCreation: function () {
            let failDefinitions = [
                {
                    'type': 'inverted', 'name': 'nest0', // nested fields - EE only
                    'fields': [
                        {
                            "name": "foo.boo.nest",
                            "features": [
                                "norm"
                            ],
                            "nested": [
                                {
                                    "name": "A"
                                },
                                {
                                    "name": "Sub",
                                    "analyzer": "identity",
                                    "nested": [
                                        {
                                            "expression": "RETURN SPLIT(@param, '.') ",
                                            "override": true,
                                            "name": "SubSub.foo",
                                            "features": [
                                                "position",
                                                "frequency"
                                            ]
                                        },
                                        {
                                            "name": "woo",
                                            "features": [
                                                "norm",
                                                "frequency"
                                            ],
                                            "override": false,
                                            "includeAllFields": true,
                                            "trackListPositions": true
                                        }
                                    ]
                                }
                            ]
                        }
                    ]
                },
                {
                    'type': 'inverted', 'name': 'nest1', // empty fields
                    'fields': []
                },
                {
                    'type': 'inverted', 'name': 'nest2', // empty name
                    'fields': [
                        {
                            "expression": "RETURN SPLIT(@param, '.') ",
                            "name": "",
                            "features": [
                                "norm",
                                "frequency"
                            ],
                            "override": false,
                            "includeAllFields": true,
                            "trackListPositions": true
                        }
                    ]
                }
            ];

            db._create("failCollection");
            for (let i = 1; i <= failDefinitions.length; i++) {
                if (i === 1 && isEnterprise) {
                    continue;
                }
                let d = failDefinitions[i - 1];
                try {
                    db.failCollection.ensureIndex(d);
                    assertFalse(i);
                } catch (e) {
                    assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
                    assertEqual(db.failCollection.indexes().length, 1);
                }
            }
        },

        testInvertedIndexRegressions: function () {
            let testColl = db._create("testColl", { replicationFactor: 1, writeConcern: 1, numberOfShards: 1 });
            for (let i = 0; i < 5; i++) {
                let cv_field = String(i) + String(i + 1);
                testColl.save({ a: "foo", b: "bar", c: i, cv_field: cv_field });
                testColl.save({ a: "foo", b: "baz", c: i, cv_field: cv_field });
                testColl.save({ a: "bar", b: "foo", c: i, cv_field: cv_field });
                testColl.save({ a: "baz", b: "foo", c: i, cv_field: cv_field });
            }

            // SEARCH-353
            {
                assertInvertedIndexCreation("testColl", { name: "i1", type: "inverted", fields: ["a"] });

                assertInvertedIndexCreation("testColl", { name: "i2", type: "inverted", fields: ["b"] });
                assertEqual(testColl.indexes().length, 3);
            }

            // SEARCH-346
            {
                assertInvertedIndexCreation("testColl", { type: "inverted", name: "i3", fields: ["cv_field"] });
                assertInvertedIndexCreation("testColl", { type: "inverted", name: "i4", fields: ["cv_field"] }, false);
                // sync indexes
                db._query("for d in testColl OPTIONS {'indexHint': 'i1', 'forceIndexHint': true, 'waitForSync': true} filter d.a == 'foo' collect with count into c return c");
                db._query("for d in testColl OPTIONS {'indexHint': 'i2', 'forceIndexHint': true, 'waitForSync': true} filter d.b == 'bar' collect with count into c return c");
                db._query("for d in testColl OPTIONS {'indexHint': 'i3', 'forceIndexHint': true, 'waitForSync': true} filter d.cv_field == 'cv_field' collect with count into c return c");
                assertEqual(testColl.indexes().length, 4);
            }

            // SEARCH-341
            {
                if (isCluster) {
                    triggerMetrics();
                }
                checkIndexMetrics( function () {
                    let stats = testColl.getIndexes(true, true);
                    for (let i = 0; i < stats.length; i++) {
                        let index = stats[i];
                        if (index["type"] === "primary") {
                            continue;
                        }
    
                        assertEqual(index["name"], `i${i}`);
                        assertEqual(index["figures"]["numDocs"], 20);
                        assertEqual(index["figures"]["numPrimaryDocs"], 20);
                        assertEqual(index["figures"]["numLiveDocs"], 20);
                        assertEqual(index["figures"]["numSegments"], 1);
                        assertEqual(index["figures"]["numFiles"], 6);
                    }
                });

            }
        },

        testNegativeSearchAliasCreation: function () {
            let failDefinitions = [
                { indexes: [{ 'collection': 'wrong_collection', 'index': 'c0_i0', 'operation': 'add' }] }, // not existing collection
                { indexes: [{ 'collection': 'c0', 'index': 'wrong_index' }] }, // not existing index
                {    // index duplication
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        },
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        }
                    ]
                },
                {    // index duplication
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'add'
                        },
                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'add'
                        }
                    ]
                },
                {   // deletion of non-existing index
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'del'
                        }
                    ]
                },
                {   // del, add
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'del'
                        },
                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'add'
                        }
                    ]
                },
                {   // wrong operation
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0',
                            'operation': 'insert'
                        }
                    ]
                },
                {   //  Same field in indexes from same collection   
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i1',
                            'operation': 'add'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i3',
                        }
                    ]
                },
                {   // searchField mismatch
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i2'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i2',
                        }
                    ]
                },
                {   // analyzer mismatch in root
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i1'
                        }
                    ]
                },
                {   // analyzer mismatch in root (analyzers def are same, but names are different)
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i3'
                        },
                        {
                            'collection': 'c3',
                            'index': 'c3_i4'
                        }
                    ]
                },
                {   // analyzer mismatch field
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i5'
                        },
                        {
                            'collection': 'c2',
                            'index': 'c2_i3'
                        }
                    ]
                },
                {   // storedValues mismatch in root
                    'indexes': [
                        {
                            'collection': 'c2',
                            'index': 'c2_i4'
                        },
                        {
                            'collection': 'c1',
                            'index': 'c1_i2'
                        }
                    ]
                },
                {   // includeAllFields is true in index in one collection
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        },
                        {
                            'collection': 'c0',
                            'index': 'c0_i1'
                        }
                    ]
                },
                {   // includeAllFields is true in indexes in one collection
                    'indexes': [
                        {
                            'collection': 'c0',
                            'index': 'c0_i0'
                        },
                        {
                            'collection': 'c0',
                            'index': 'c0_i2'
                        }
                    ]
                },
                {   // primarySort mismatch
                    'indexes': [
                        {
                            'collection': 'c3',
                            'index': 'c3_i6'
                        },
                        {
                            'collection': 'c2',
                            'index': 'c2_i5'
                        }
                    ]
                }
            ];
            for (let i = 1; i <= failDefinitions.length; i++) {
                let d = failDefinitions[i - 1];
                let viewName = `failed${i - 1}`;
                try {
                    db._createView(viewName, 'search-alias', d);
                    assertFalse(i);
                } catch (e) {
                    assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
                    assertEqual(db._view(viewName), null);
                }
            }
        }
    };
}

jsunity.run(IResearchInvertedIndexSearchAliasAqlTestSuiteCommunity);

return jsunity.done();
