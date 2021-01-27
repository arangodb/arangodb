/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for registers limits
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

let internal = require("internal");
let jsunity = require("jsunity");

function aqlRegistersLimitTestSuite () {
  const errors = internal.errors;
  // the value of 1000 needs to match the maximum allowed register number in
  // arangod (currently in RegisterPlan::MaxRegisterId)
  const maxRegisters = 1000;

  let buildQuery = function(numRegisters) {
    let subs = [], returns = [];
    // we need to subtract a register here because we have one extra results
    // register
    for (let i = 0; i < numRegisters - 1; ++i) {
      subs.push(`LET test${i} = NOOPT(${i})`);
      returns.push(`test${i}`);
    }
    return subs.join('\n') + 'RETURN [\n' + returns.join(',\n') + ']\n';
  };

  return {
    testBelowLimit : function () {
      let query = buildQuery(maxRegisters - 1);
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(maxRegisters - 1, plan.variables.length);
    },
    
    testAtLimit : function () {
      let query = buildQuery(maxRegisters);
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(maxRegisters, plan.variables.length);
    },
    
    testBeyondLimit : function () {
      let query = buildQuery(maxRegisters + 1);
      try {
        AQL_EXPLAIN(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
  };
}

jsunity.run(aqlRegistersLimitTestSuite);

return jsunity.done();
