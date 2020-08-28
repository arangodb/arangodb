// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Lars Maier
// / @author Markus Pfeiffer
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

function cmpAccumulator(cmp) {
    return function() {
        return {
            updateProgram: ["if",
                [
                    ["or",
                        ["not", ["attrib-get", "isSet", ["current-value"]]],
                        [cmp, ["input-value"], ["attrib-get", "value", ["current-value"]]]
                    ],
                    ["seq",
                        ["this-set!",
                            ["dict",
                                ["list", "isSet", true],
                                ["list", "value", ["input-value"]],
                                ["list", "sender", ["input-sender"]]
                            ]
                        ],
                        "hot"
                    ]
                ],
                [true, "cold"]
            ],
            clearProgram: ["this-set!", {"isSet": false, "value": 0, sender: null}],
            getProgram: ["if",
                [
                    ["attrib-get", "isSet", ["current-value"]],
                    ["attrib-get", "value", ["current-value"]]
                ],
                [
                    true,
                    ["error", "accumulator undefined value"]
                ]
            ],
            setProgram: ["this-set!",
                ["dict",
                    ["list", "isSet", true],
                    ["list", "value", ["input-value"]],
                    ["list", "sender", null]
                ]
            ],
            finalizeProgram: ["if",
                [
                    ["attrib-get", "isSet", ["current-value"]],
                    ["dict-x-tract", ["current-value"], "value", "sender"]
                ],
                [true, null]
            ],
        };
    };
}

function sumAccumulator() {
    return {
        updateProgram: ["if",
            [["eq?", ["input-value"], 0],
                "cold"],
            [true,
                ["seq",
                    ["this-set!", ["+", ["current-value"], ["input-value"]]],
                    "hot"]]],
        clearProgram: ["this-set!", 0],
        getProgram: ["current-value"],
        setProgram: ["this-set!", ["input-value"]],
        finalizeProgram: ["current-value"],
    };
}

function storeAccumulator(clearValue = null) {
    return {
        updateProgram: ["this-set!", ["input-value"]],
        clearProgram: ["this-set!", clearValue],
        getProgram: ["current-value"],
        setProgram: ["this-set!", ["input-value"]],
        finalizeProgram: ["current-value"],
    };
}

exports.minAccumulator = cmpAccumulator("lt?");
exports.maxAccumulator = cmpAccumulator("gt?");
exports.sumAccumulator = sumAccumulator;
exports.storeAccumulator = storeAccumulator;
