

@brief constructs a query-by-example for a collection
`collection.byExample(example)`

Fetches all documents from a collection that match the specified
example and returns a cursor.

You can use *toArray*, *next*, or *hasNext* to access the
result. The result can be limited using the *skip* and *limit*
operator.

An attribute name of the form *a.b* is interpreted as attribute path,
not as attribute. If you use

```json
{ "a": { "c": 1 } }
```

as example, then you will find all documents, such that the attribute
*a* contains a document of the form *{ "c": 1 }*. For example the document

```json
{ "a" : { "c" : 1 }, "b" : 1 }
```

will match, but the document

```json
{ "a" : { "c" : 1, "b" : 1 } }
```

will not.

However, if you use

```json
{ "a.c" : 1 }
```

then you will find all documents, which contain a sub-document in *a*
that has an attribute *c* of value *1*. Both the following documents

```json
{ "a" : { "c" : 1 }, "b" : 1 }

```

and

```json
{ "a" : { "c" : 1, "b" : 1 } }
```

will match.

`collection.byExample(path1, value1, ...)`

As alternative you can supply an array of paths and values.

@EXAMPLES

Use *toArray* to get all documents at once:

@EXAMPLE_ARANGOSH_OUTPUT{003_collectionByExample}
~ db._create("users");
  db.users.save({ name: "Gerhard" });
  db.users.save({ name: "Helmut" });
  db.users.save({ name: "Angela" });
  db.users.all().toArray();
  db.users.byExample({ "_id" : "users/20" }).toArray();
  db.users.byExample({ "name" : "Gerhard" }).toArray();
  db.users.byExample({ "name" : "Helmut", "_id" : "users/15" }).toArray();
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT

Use *next* to loop over all documents:

@EXAMPLE_ARANGOSH_OUTPUT{004_collectionByExampleNext}
~ db._create("users");
  db.users.save({ name: "Gerhard" });
  db.users.save({ name: "Helmut" });
  db.users.save({ name: "Angela" });
  var a = db.users.byExample( {"name" : "Angela" } );
  while (a.hasNext()) print(a.next());
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT


