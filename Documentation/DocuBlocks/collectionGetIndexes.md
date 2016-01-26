

@brief returns information about the indexes
`getIndexes()`

Returns an array of all indexes defined for the collection.

Note that `_key` implicitly has an index assigned to it.

@EXAMPLE_ARANGOSH_OUTPUT{collectionGetIndexes}
~db._create("test");
~db.test.ensureUniqueSkiplist("skiplistAttribute");
~db.test.ensureUniqueSkiplist("skiplistUniqueAttribute");
|~db.test.ensureHashIndex("hashListAttribute",
                          "hashListSecondAttribute.subAttribute");
db.test.getIndexes();
~db._drop("test");
@END_EXAMPLE_ARANGOSH_OUTPUT

