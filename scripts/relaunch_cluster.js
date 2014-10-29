/*jshint unused: false */
/*global require */

function main (argv) {
  "use strict";

  var runInfoName = "runInfo.json";
  var fs = require("fs");
  var print = require("internal").print;
  if (argv.length > 1) {
    runInfoName = argv[1];
    print("Using runInfo from:", runInfoName);
  }
  var Kickstarter = require("org/arangodb/cluster").Kickstarter;
  var runInfo = JSON.parse(fs.read(runInfoName));
  var k = new Kickstarter(runInfo.plan);
  var r = k.relaunch();
  fs.write(runInfoName,JSON.stringify({"plan": runInfo.plan,
                                       "runInfo": r.runInfo}));
  print("Endpoints:");
  var i,j;
  j = r.runInfo.length-1;
  while (j > 0 && r.runInfo[j].isStartServers === undefined) {
    j--;
  }
  var l = r.runInfo[j];
  for (i = 0;i < l.endpoints.length;i++) {
    print("  " + l.roles[i] + ": " + l.endpoints[i]);
  }
}
