

@brief removes multiple documents by their keys
`collection.removeByKeys(keys)`

Looks up the documents in the specified collection using the array of keys
provided, and removes all documents from the collection whose keys are
contained in the *keys* array. Keys for which no document can be found in
the underlying collection are ignored, and no exception will be thrown for
them.

The method will return an object containing the number of removed documents
in the *removed* sub-attribute, and the number of not-removed/ignored
documents in the *ignored* sub-attribute.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionRemoveByKeys}
~ db._drop("example");
~ db._create("example");
  keys = [ ];
| for (var i = 0; i < 10; ++i) {
|   db.example.insert({ _key: "test" + i, value: i });
|   keys.push("test" + i);
  }
  db.example.removeByKeys(keys);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

