var Planner = require("org/arangodb/cluster").Planner;
var Kickstarter = require("org/arangodb/cluster").Kickstarter;
var p = new Planner({});
var k = new Kickstarter(p.getPlan());
var fs = require("fs");
k.runInfo = JSON.parse(fs.read("runInfo.json"));
k.shutdown();
k.cleanup();

