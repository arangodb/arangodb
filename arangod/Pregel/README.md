![ArangoDB-Logo](https://docs.arangodb.com/assets/arangodb_logo_2016_inverted.png)

Pregel Subsystem
========


#### Protocol

Message format between DBServers:

{sender:"someid", 
executionNumber:1337, 
globalSuperstep:123, 
messages: [vertexID1, data1, vertexID2, data2]
}

data1 may be any type of slice, whether array, object or primitive datatype

Future:  vertexID may be an array of vertices
