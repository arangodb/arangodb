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

/*


*/
exports.vertex_degrees_program = vertex_degrees_program;
exports.vertex_degrees = vertex_degrees;

/* returns a program that compputes the vertex degree of every vertex */
function vertex_degrees_program(resultField) {
    return {
        resultField: resultField,
        maxGSS: 2,
        vertexAccumulators: {
          outDegree: {
            accumulatorType: "sum",
            valueType: "ints",
            storeSender: false
          },
          inDegree: {
            accumulatorType: "sum",
            valueType: "ints",
            storeSender: false
          }
        },
      phases: [ {
        name: "main",
        initProgram: [ "seq",
                       // Set our out degree
                       ["accum-set!", "outDegree", ["this-number-outbound-edges"]],
                       // Init in degree to 0
                       ["accum-set!", "inDegree", 0],
                       ["send-to-all-neighbors", "inDegree", 1]
                     ],
        // Update program has to run once to accumulate the
        // inDegrees that have been sent out in initProgram
        updateProgram: [ "seq",
                         false ]
      } ] };
}

function vertex_degrees(
    graphName,
    resultField) {
    return pregel.start(
        "air",
        graphName,
        vertex_degree_program(resultField)
    );
}




