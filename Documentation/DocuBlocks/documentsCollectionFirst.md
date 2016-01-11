

@brief selects the n first documents in the collection
`collection.first(count)`

The *first* method returns the n first documents from the collection, in
order of document insertion/update time.

If called with the *count* argument, the result is a list of up to
*count* documents. If *count* is bigger than the number of documents
in the collection, then the result will contain as many documents as there
are in the collection.
The result list is ordered, with the "oldest" documents being positioned at
the beginning of the result list.

When called without an argument, the result is the first document from the
collection. If the collection does not contain any documents, the result
returned is *null*.

**Note**: this method is not supported in sharded collections with more than
one shard.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionFirst}
~ db._create("example");
~ db.example.save({ Hello : "world" });
~ db.example.save({ Foo : "bar" });
  db.example.first(1);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionFirstNull}
~ db._create("example");
~ db.example.save({ Hello : "world" });
  db.example.first();
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

