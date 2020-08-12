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

const pregel = require("@arangodb/pregel");
/*


*/
exports.single_source_shortest_paths_program = single_source_shortest_paths_program;
exports.single_source_shortest_paths = single_source_shortest_paths;

/*

  `single_source_shortest_path_program` returns an AIR program that performs a
  single-source shortest path search, currently without path reconstruction, on
  all vertices in the graph starting from `startVertex`, using the cost stored
  in `weightAttribute` on each edge, and storing the end result in resultField
  as an object containing the attribute `distance`

*/
function single_source_shortest_paths_program(
  resultField,
  startVertexId,
  weightAttribute
) {
  return {
    resultField: resultField,
    maxGSS: 10000,
    vertexAccumulators: {
      distance: {
        accumulatorType: "min",
        valueType: "doubles",
        storeSender: true,
      },
    },
    phases: [
      {
        name: "main",
        initProgram: [
          "seq",
          [
            "if",
            [
              ["eq?", ["this-vertex-id"], startVertexId],
              ["seq", ["accum-set!", "distance", 0], true],
            ],
            [true, ["seq",
                    ["accum-clear!", "distance"],
                    false]],
          ],
        ],
        updateProgram: [
          "seq",
          [
            "for-each",
            ["edge", ["this-outbound-edges"]],
            ["seq",
             [
               "send-to-accum",
               ["attrib-ref", "to-pregel-id", ["var-ref", "edge"]],
               "distance",
               [
                 "+",
                 ["accum-ref", "distance"],
                 ["attrib-ref", ["quote", "document", weightAttribute], ["var-ref", "edge"]],
               ],
             ],
            ],
          ],
          false,
        ],
      },
    ],
  };
}

/* `single_source_shortest_path` executes the program
   returned by `single_source_shortest_path_program`
   on the graph identified by `graphName`. */
function single_source_shortest_paths(
  graphName,
  resultField,
  startVertexId,
  weightAttribute
) {
  return pregel.start(
    "air",
    graphName,
    single_source_shortest_paths_program(
      resultField,
      startVertexId,
      weightAttribute
    )
  );
}
