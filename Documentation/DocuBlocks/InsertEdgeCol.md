

@brief saves a new edge document
`edge-collection.insert(from, to, document)`

Saves a new edge and returns the document-id. *from* and *to*
must be documents or document references.

`edge-collection.insert(from, to, document, waitForSync)`

The optional *waitForSync* parameter can be used to force
synchronization of the document creation operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{EDGCOL_01_SaveEdgeCol}
  ~db._drop("vertex");
  ~db._drop("relation");
  db._create("vertex");
  db._createEdgeCollection("relation");
  v1 = db.vertex.insert({ name : "vertex 1" });
  v2 = db.vertex.insert({ name : "vertex 2" });
  e1 = db.relation.insert(v1, v2, { label : "knows" });
  db._document(e1);
~ db._drop("relation");
~ db._drop("vertex");
@END_EXAMPLE_ARANGOSH_OUTPUT


