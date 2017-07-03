![ArangoDB-Logo](https://docs.arangodb.com/assets/arangodb_logo_2016_inverted.png)

Pregel Subsystem
========

The pregel subsystem implements a variety of different grapg algorithms, 
this readme is more intended for internal use.

#### Protocol

Message format between DBServers:



{sender:"someid", 
executionNumber:1337, 
globalSuperstep:123, 
messages: [<vertexID1>, <slice1>, vertexID2, <slice2>]
}
Any type of slice is supported


### Useful Commands

Import graph e.g. https://github.com/arangodb/example-datasets/tree/master/Graphs/1000
First rename the columns '_key', '_from', '_to' arangoimp will keep those.

In arangosh:

    db._create('vertices', {numberOfShards: 2});
    db._createEdgeCollection('alt_edges');
    db._createEdgeCollection('edges', {numberOfShards: 2, shardKeys:["_vertex"], distributeShardsLike:'vertices'});

arangoimp --file generated_vertices.csv --type csv --collection vertices --overwrite true --server.endpoint http+tcp://127.0.0.1:8530

Or:
for(var i=0; i < 5000; i++) db.vertices.save({_key:i+""});

arangoimp --file generated_edges.csv --type csv --collection alt_edges --overwrite true --from-collection-prefix "vertices" --to-collection-prefix "vertices" --convert false  --server.endpoint http+tcp://127.0.0.1:8530



AQL script to copy edge collection into one with '_vertex':

FOR doc IN alt_edges
INSERT {_vertex:SUBSTRING(doc._from,FIND_FIRST(doc._from,"/")+1), 
_from:doc._from,
_to:doc._to} IN edges
  LET values = (
     FOR s IN vertices
      RETURN s.result
  )
  RETURN SUM(values)


# AWK Scripts

Make CSV file with ID’s unique
cat edges.csv | tr '[:space:]' '[\n*]' | grep -v "^\s*$" | awk '!seen[$0]++' > vertices.csv

Make CSV file with arango compatible edges

cat edges.csv | awk -F" " '{print "profiles/" $1 "\tprofiles/" $2 "\t" $1}' >> arango-edges.csv


arangoimp --file vertices.csv --type csv --collection twitter_v --overwrite true --convert false --server.endpoint http+tcp://127.0.0.1:8530  -c none
arangoimp --file arango-edges.csv --type csv --collection twitter_e --overwrite true --convert false --separator "\t" --server.endpoint http+tcp://127.0.0.1:8530  -c none
