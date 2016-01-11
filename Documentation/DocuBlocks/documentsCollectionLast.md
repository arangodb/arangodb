

@brief selects the n last documents in the collection
`collection.last(count)`

The *last* method returns the n last documents from the collection, in
order of document insertion/update time.

If called with the *count* argument, the result is a list of up to
*count* documents. If *count* is bigger than the number of documents
in the collection, then the result will contain as many documents as there
are in the collection.
The result list is ordered, with the "latest" documents being positioned at
the beginning of the result list.

When called without an argument, the result is the last document from the
collection. If the collection does not contain any documents, the result
returned is *null*.

**Note**: this method is not supported in sharded collections with more than
one shard.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionLast}
~ db._create("example");
~ db.example.save({ Hello : "world" });
~ db.example.save({ Foo : "bar" });
  db.example.last(2);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionLastNull}
~ db._create("example");
~ db.example.save({ Hello : "world" });
  db.example.last(1);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


