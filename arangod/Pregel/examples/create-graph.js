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

module.exports = function (gname, shards, repFac) {
  var vColl = gname+"_v", eColl = gname+"_e";
  shards = shards || 4;
  repFac = repFac || 1;
  
  var graph;
  var exists = graph_module._list().indexOf(gname) != -1;
  if (!exists) {
    graph = graph_module._create(gname);
    db._create(vColl, {numberOfShards: shards,
                       replicationFactor:repFac});
    graph._addVertexCollection(vColl);
    db._createEdgeCollection(eColl, {
                             numberOfShards: shards,
                             replicationFactor: repFac,
                             shardKeys:["vertex"],
                             distributeShardsLike:vColl});
    
    var rel = graph_module._relation(eColl, [vColl], [vColl]);
    graph._extendEdgeDefinitions(rel);
  }
};
