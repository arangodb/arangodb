var graph_module = require("@arangodb/general-graph");

var graph;
var exists = graph_module._list().indexOf("wikivote") != -1;
if (!exists) {
  graph = graph_module._create("wikivote");
  db._create('wikiuser', {numberOfShards: 4});
  graph._addVertexCollection("wikiuser");
  db._createEdgeCollection('wikivoted', {numberOfShards: 4, shardKeys:["_vertex"],
                           distributeShardsLike:'wikiuser'});
  var rel = graph_module._relation("wikivoted", ["wikiuser"], ["wikiuser"]);
  graph._extendEdgeDefinitions(rel);
} else {
  graph = graph_module._graph("wikivote");
}

var fs = require('fs');
var filename = '/Users/simon/Desktop/arangodb/arangod/Pregel/examples/wiki-Vote.txt'

var nodeSet = new Set();;
var largeString = fs.read(filename);
var lines = largeString.split("\n");
lines.map(function(line) {
  var parts = line.split(/\s+/);
          //console.log(parts);
  if (parts && parts.length >= 2) {
          
          
    if (!nodeSet.has(parts[0])) {
      nodeSet.add(parts[0]);
      graph.wikiuser.save({_key:parts[0],
                           value:-1});
    }
    if (!nodeSet.has(parts[1])) {
      nodeSet.add(parts[1]);
      graph.wikiuser.save({_key:parts[1],
                           value:-1});
    }
          
    graph.wikivoted.save("wikiuser/"+parts[0],
                                  "wikiuser/"+parts[1],
                                  {_vertex:parts[0], value:-1});
  
  }
});
