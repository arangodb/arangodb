/*jshint unused: false */
/*global require */

function main (argv) {
  "use strict";

  var options = {};
  var runInfoName = "runInfo.json";
  var print = require("internal").print;
  if (argv.length > 1) {
    options = JSON.parse(argv[1]);
    print("Using options:",options);
  }
  if (argv.length > 2) {
    runInfoName = argv[2];
    print("Using runInfo name:", runInfoName);
  }
  var Planner = require("org/arangodb/cluster").Planner;
  var Kickstarter = require("org/arangodb/cluster").Kickstarter;
  var p = new Planner(options);
  var k = new Kickstarter(p.getPlan());
  var r = k.launch();
  var fs = require("fs");
  fs.write(runInfoName,JSON.stringify({"plan": p.getPlan(),
                                       "runInfo": r.runInfo}));
  print("Endpoints:");
  var i,j;
  j = r.runInfo.length-1;
  while (j > 0 && r.runInfo[j].isStartServers === undefined) {
    j--;
  }
  var l = r.runInfo[j];
  if (l.endpoints) {
    for (i = 0; i < l.endpoints.length;i++) {
      print("  " + l.roles[i] + ": " + l.endpoints[i]);
    }
  }
}
