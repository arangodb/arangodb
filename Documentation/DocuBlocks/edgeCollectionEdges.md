

@brief selects all edges for a set of vertices
`edge-collection.edges(vertex)`

The *edges* operator finds all edges starting from (outbound) or ending
in (inbound) *vertex*.

`edge-collection.edges(vertices)`

The *edges* operator finds all edges starting from (outbound) or ending
in (inbound) a document from *vertices*, which must a list of documents
or document handles.

@EXAMPLE_ARANGOSH_OUTPUT{EDGCOL_02_Relation}
  db._create("vertex");
  db._createEdgeCollection("relation");
~ var myGraph = {};
  myGraph.v1 = db.vertex.insert({ name : "vertex 1" });
  myGraph.v2 = db.vertex.insert({ name : "vertex 2" });
| myGraph.e1 = db.relation.insert(myGraph.v1, myGraph.v2,
                                  { label : "knows"});
  db._document(myGraph.e1);
  db.relation.edges(myGraph.e1._id);
~ db._drop("relation");
~ db._drop("vertex");
@END_EXAMPLE_ARANGOSH_OUTPUT


