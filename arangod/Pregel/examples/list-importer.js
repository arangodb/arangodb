// you should be able to import networks consisting of one vertex collection
// as well as one edge collection.
// vertex documents need to be imported seperatly, if you want them to
// have some useful data
//
// http://snap.stanford.edu/data/egonets-Twitter.html
// http://snap.stanford.edu/data/egonets-Facebook.html
// http://snap.stanford.edu/data/egonets-Gplus.html
// http://snap.stanford.edu/data/wiki-Vote.html

var graph_module = require("@arangodb/general-graph");
var fs = require('fs');

module.exports = function (gname, filename) {
  var vColl = gname+"_vertices", eColl = gname+"_edges";
  
  var graph;
  var exists = graph_module._list().indexOf(gname) != -1;
  if (!exists) {
    graph = graph_module._create(gname);
    db._create(vColl, {numberOfShards: 2, replicationFactor:2});
    graph._addVertexCollection(vColl);
    db._createEdgeCollection(eColl, {
                             numberOfShards: 2,
                             replicationFactor: 2,
                             shardKeys:["_vertex"],
                             distributeShardsLike:vColl});
    
    var rel = graph_module._relation(eColl, [vColl], [vColl]);
    graph._extendEdgeDefinitions(rel);
  } else {
    graph = graph_module._graph(gname);
  }
  
  var nodeSet = new Set();
  var largeString = fs.read(filename);
  var lines = largeString.split("\n");
  lines.map(function(line) {
    var parts = line.split(/\s+/);
    //console.log(parts);
    if (parts && parts.length >= 2) {
      
      if (!nodeSet.has(parts[0])) {
        nodeSet.add(parts[0]);
        graph[vColl].save({_key:parts[0], value:-1});
      }
      if (!nodeSet.has(parts[1])) {
        nodeSet.add(parts[1]);
        graph[vColl].save({_key:parts[1], value:-1});
      }
      
      graph[eColl].save(vColl+"/"+parts[0],
                        vColl+"/"+parts[1],
                        {_vertex:parts[0]});
    }
  });
};
