

@brief fetches multiple documents by their keys
`collection.documents(keys)`

Looks up the documents in the specified collection using the array of keys
provided. All documents for which a matching key was specified in the *keys*
array and that exist in the collection will be returned. 
Keys for which no document can be found in the underlying collection are ignored, 
and no exception will be thrown for them.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionLookupByKeys}
~ db._drop("example");
~ db._create("example");
  keys = [ ];
| for (var i = 0; i < 10; ++i) {
|   db.example.insert({ _key: "test" + i, value: i });
|   keys.push("test" + i);
  }
  db.example.documents(keys);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

