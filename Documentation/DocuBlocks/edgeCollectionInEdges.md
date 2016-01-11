

@brief selects all inbound edges
`edge-collection.inEdges(vertex)`

The *edges* operator finds all edges ending in (inbound) *vertex*.

`edge-collection.inEdges(vertices)`

The *edges* operator finds all edges ending in (inbound) a document from
*vertices*, which must a list of documents or document handles.

@EXAMPLES
@EXAMPLE_ARANGOSH_OUTPUT{EDGCOL_02_inEdges}
  db._create("vertex");
  db._createEdgeCollection("relation");
~ var myGraph = {};
  myGraph.v1 = db.vertex.insert({ name : "vertex 1" });
  myGraph.v2 = db.vertex.insert({ name : "vertex 2" });
| myGraph.e1 = db.relation.insert(myGraph.v1, myGraph.v2,
                                  { label : "knows"});
  db._document(myGraph.e1);
  db.relation.inEdges(myGraph.v1._id);
  db.relation.inEdges(myGraph.v2._id);
~ db._drop("relation");
~ db._drop("vertex");
@END_EXAMPLE_ARANGOSH_OUTPUT


