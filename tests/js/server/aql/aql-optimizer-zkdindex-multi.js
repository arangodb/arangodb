/* global AQL_EXPLAIN, AQL_EXECUTE */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertEqual} = jsunity.jsUnity.assertions;

const useIndexes = 'use-indexes';
const removeFilterCoveredByIndex = "remove-filter-covered-by-index";

const opCases = ["none", "eq", "le", "ge", "le2", "ge2", "lt", "gt", "legt"];

function conditionForVariable(op, name) {
    if (op === "none") {
        return "true";
    } else if (op === "eq") {
        return name + " == 5";
    } else if (op === "le") {
        return `${name} <= 5`;
    } else if (op === "ge") {
        return `${name} >= 5`;
    } else if (op === "ge2") {
        return `${name} >= 5 && ${name} <= 6`;
    } else if (op === "le2") {
        return `${name} <= 5 && ${name} >= 4`;
    } else if (op === "lt") {
        return `${name} < 5`;
    } else if (op === "gt") {
        return `${name} > 5`;
    } else if (op === "legt") {
        return `${name} <= 5 && ${name} > 4`;
    }
}

function resultSetForConditoin(op) {
    const all = [...Array(11).keys()];
    if (op === "none") {
        return all;
    } else if (op === "eq") {
        return all.filter((x) => x === 5);
    } else if (op === "le") {
        return all.filter((x) => x <= 5);
    } else if (op === "ge") {
        return all.filter((x) => x >= 5);
    } else if (op === "ge2") {
        return all.filter((x) => x >= 5 && x <= 6);
    } else if (op === "le2") {
        return all.filter((x) => x <= 5 && x >= 4);
    } else if (op === "lt") {
        return all.filter((x) => x < 5);
    } else if (op === "gt") {
        return all.filter((x) => x > 5);
    } else if (op === "legt") {
        return all.filter((x) => x <= 5 && x > 4);
    }
}

function productSet(x, y, z, w) {
    let result = [];
    for (let dx of resultSetForConditoin(x)) {
        for (let dy of resultSetForConditoin(y)) {
            for (let dz of resultSetForConditoin(z)) {
                for (let dw of resultSetForConditoin(w)) {
                    result.unshift([dx, dy, dz, dw]);
                }
            }
        }
    }
    return result;
}

function optimizerRuleZkd2dIndexTestSuite() {
    const colName = 'UnitTestZkdIndexMultiCollection';
    let col;

    let testObject = {
        setUpAll: function () {
            col = db._create(colName);
            col.ensureIndex({type: 'zkd', name: 'zkdIndex', fields: ['x', 'y', 'z', 'a.w'], fieldValueTypes: 'double'});
            db._query(aql`
                FOR x IN 0..10
                FOR y IN 0..10
                FOR z IN 0..10
                FOR w IN 0..10
                  INSERT {x, y, z, a: {w}} INTO ${col}
              `);
        },

        tearDownAll: function () {
            col.drop();
        },
    };

    for (let x of ["none", "eq"]) {
        for (let y of ["none", "le", "gt"]) {
            for (let z of opCases) {
                for (let w of opCases) {
                    if (x === "none" && y === "none" && z === "none" && w === "none") {
                        continue; // does not use the index
                    }

                    testObject[["testCase", x, y, z, w].join("_")] = function () {
                        const query = `
                            FOR d IN ${colName}
                              FILTER ${conditionForVariable(x, "d.x")}
                              FILTER ${conditionForVariable(y, "d.y")}
                              FILTER ${conditionForVariable(z, "d.z")}
                              FILTER ${conditionForVariable(w, "d.a.w")}
                              RETURN [d.x, d.y, d.z, d.a.w]
                          `;
                        const explainRes = AQL_EXPLAIN(query);
                        const appliedRules = explainRes.plan.rules;
                        const nodeTypes = explainRes.plan.nodes.map(n => n.type).filter(n => !["GatherNode", "RemoteNode"].includes(n));
                        assertEqual(["SingletonNode", "IndexNode", "CalculationNode", "ReturnNode"], nodeTypes);
                        assertTrue(appliedRules.includes(useIndexes));

                        const conds = [x, y, z, w];
                        if (!conds.includes("lt") && !conds.includes("gt") && !conds.includes("legt")) {
                            assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
                        }
                        const executeRes = AQL_EXECUTE(query);
                        const res = executeRes.json;
                        const expected = productSet(x, y, z, w);
                        res.sort();
                        expected.sort();
                        assertEqual(expected, res, JSON.stringify({query}));
                    };
                }
            }
        }
    }

    return testObject;
}

jsunity.run(optimizerRuleZkd2dIndexTestSuite);

return jsunity.done();
