/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global module, require, exports, ArangoAgency, SYS_TEST_PORT */

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster startup functionality by dispatchers
///
/// @file js/server/modules/org/arangodb/cluster/dispatcher.js
///
/// DISCLAIMER
///
/// Copyright 2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         Dispatcher functionality
// -----------------------------------------------------------------------------

var download = require("internal").download;

var print = require("internal").print;

var actions = {};

actions.startAgent = function (dispatchers, cmd) {
  print("Starting an agent...");
  return {"error":false};
};

actions.sendConfiguration = function (dispatchers, cmd) {
  print("Sending configuration...");
  return {"error":false};
};

actions.startLauncher = function (dispatchers, cmd) {
  print("Starting launcher...");
  return {"error":false};
};

function dispatch (startupPlan) {
  if (!startupPlan.hasOwnProperty("myname")) {
    myname = "me";
  }
  else {
    myname = startupPlan.myname;
  }
  var dispatchers = startupPlan.dispatchers;
  var cmds = startupPlan.commands;
  var results = [];
  var cmd;

  var error = false;
  var i;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      var res = actions[cmd.action](dispatchers, cmd);
      results.push(res);
      if (res.error === true) {
        error = true;
        break;
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "dispatchers": dispatchers,
                                  "commands": [cmd],
                                  "myname": cmd.dispatcher });
      var url = "http" + ep.substr(3) + "/_admin/dispatch";
      var response = download(url, body, {"method": "post"});
      try {
        if (response.code !== 200) {
          error = true;
        }
        results.push(JSON.parse(response.body));
      }
      catch (err) {
        results.push({"error":true, "errorMessage": "exception in JSON.parse"});
        error = true;
        break;
      }
    }
  }
  if (error) {
    return {"error": true, "errorMessage": "some error during dispatch",
            "results": results};
  }
  return {"error": false, "errorMessage": "none",
          "results": results};
}

exports.dispatch = dispatch;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
