var Planner = require("org/arangodb/cluster").Planner;
var Kickstarter = require("org/arangodb/cluster").Kickstarter;
var p = new Planner({});
var k = new Kickstarter(p.getPlan());
var r = k.launch();
var fs = require("fs");
fs.write("runInfo.json",JSON.stringify(r));

