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

const internal = require("internal");
const pregel = require("@arangodb/pregel");
const db = internal.db;

exports.wait_for_pregel = wait_for_pregel;
exports.compare_pregel = compare_pregel;

/* Some helper functions */
function compare_pregel(aqlResult) {
  const res = aqlResult.toArray();
  if (res.length > 0) {
    internal.print("Test failed.");
    internal.print("Discrepancies in results: " + JSON.stringify(res));
    return false;
  }

  internal.print("Test succeeded.");
  return true;
}

function wait_for_pregel(name, pid) {
  internal.print("  Started: " + name + " - PID: " + pid);
  var waited = 0;
  while (true) {
    var status = pregel.status(pid);

    if (status.state === "done" || status.state === "fatal error") {
      if (status.state !== "done") {
        internal.print("  " + name + " done, returned with status: ");
        internal.print(JSON.stringify(status, null, 4));
      }
      return status;
    } else {
      waited++;
      if (waited % 10 === 0) {
        internal.print("waited " + waited + " seconds, not done yet, waiting some more, status = " + status.state);
      }
    }
    internal.sleep(1);
  }
}
