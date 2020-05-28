

@brief updates a document
`collection.update(document, data, overwrite, keepNull, waitForSync)` or
`collection.update(document, data,
overwrite: true or false, keepNull: true or false, waitForSync: true or false)`

Updates an existing *document*. The *document* must be a document in
the current collection. This document is then patched with the
*data* given as second argument. The optional *overwrite* parameter can
be used to control the behavior in case of version conflicts (see below).
The optional *keepNull* parameter can be used to modify the behavior when
handling *null* values. Normally, *null* values are stored in the
database. By setting the *keepNull* parameter to *false*, this behavior
can be changed so that all attributes in *data* with *null* values will
be removed from the target document.

The optional *waitForSync* parameter can be used to force
synchronization of the document update operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

The method returns a document with the attributes *_id*, *_rev* and
*_oldRev*.  The attribute *_id* contains the document identifier of the
updated document, the attribute *_rev* contains the document revision of
the updated document, the attribute *_oldRev* contains the revision of
the old (now replaced) document.

If there is a conflict, i. e. if the revision of the *document* does not
match the revision in the collection, then an error is thrown.

`collection.update(document, data, true)`

As before, but in case of a conflict, the conflict is ignored and the old
document is overwritten.

`collection.update(document-id, data)`

As before. Instead of a document a document-id can be passed as
first argument.

*Examples*

Create and update a document:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdate}
~ db._create("example");
  a1 = db.example.insert({"a" : 1});
  a2 = db.example.update(a1, {"b" : 2, "c" : 3});
  a3 = db.example.update(a1, {"d" : 4}); // xpError(ERROR_ARANGO_CONFLICT);
  a4 = db.example.update(a2, {"e" : 5, "f" : 6 });
  db.example.document(a4);
  a5 = db.example.update(a4, {"a" : 1, c : 9, e : 42 });
  db.example.document(a5);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Use a document identifier:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateHandle}
~ db._create("example");
~ var myid = db.example.insert({_key: "18612115"});
  a1 = db.example.insert({"a" : 1});
  a2 = db.example.update("example/18612115", { "x" : 1, "y" : 2 });
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Use the keepNull parameter to remove attributes with null values:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateHandleKeepNull}
~ db._create("example");
~ var myid = db.example.insert({_key: "19988371"});
  db.example.insert({"a" : 1});
|db.example.update("example/19988371",
                   { "b" : null, "c" : null, "d" : 3 });
  db.example.document("example/19988371");
  db.example.update("example/19988371", { "a" : null }, false, false);
  db.example.document("example/19988371");
| db.example.update("example/19988371",
                    { "b" : null, "c": null, "d" : null }, false, false);
  db.example.document("example/19988371");
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Patching array values:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateHandleArray}
~ db._create("example");
~ var myid = db.example.insert({_key: "20774803"});
|  db.example.insert({"a" : { "one" : 1, "two" : 2, "three" : 3 },
                      "b" : { }});
| db.example.update("example/20774803", {"a" : { "four" : 4 },
                                         "b" : { "b1" : 1 }});
  db.example.document("example/20774803");
| db.example.update("example/20774803", { "a" : { "one" : null },
|                                         "b" : null },
                    false, false);
  db.example.document("example/20774803");
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


