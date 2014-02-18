function main (argv) {
  var options = {};
  var runInfoName = "runInfo.json";
  var print = require("internal").print;
  if (argv.length > 1) {
    options = JSON.parse(argv[1]);
    print("Using options:",options);
  }
  if (argv.length > 2) {
    runInfoName = argv[2];
    print("Using runInfo name:",runInfoName);
  }
  var Planner = require("org/arangodb/cluster").Planner;
  var Kickstarter = require("org/arangodb/cluster").Kickstarter;
  var p = new Planner(options);
  var k = new Kickstarter(p.getPlan());
  var r = k.launch();
  var fs = require("fs");
  fs.write(runInfoName,JSON.stringify({"plan": p.getPlan(),
                                       "runInfo": r.runInfo}));
  print("Coordinator endpoints:");
  var i;
  var l = r.runInfo[r.runInfo.length-1];
  for (i = 0;i < l.endpoints.length;i++) {
    if (l.roles[i] === "Coordinator") {
      print("  ",l.endpoints[i]);
    }
  }
}
