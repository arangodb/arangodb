

@brief queries the fulltext index
`collection.fulltext(attribute, query)`

The *fulltext* simple query functions performs a fulltext search on the specified
*attribute* and the specified *query*.

Details about the fulltext query syntax can be found below.

**Note**: this method is not yet supported by the RocksDB storage engine.

Note: the *fulltext* simple query function is **deprecated** as of ArangoDB 2.6.
The function may be removed in future versions of ArangoDB. The preferred
way for executing fulltext queries is to use an AQL query using the *FULLTEXT*
[AQL function](../Aql/FulltextFunctions.md) as follows:

    FOR doc IN FULLTEXT(@@collection, @attributeName, @queryString, @limit)
      RETURN doc

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionFulltext}
~ db._drop("emails");
~ db._create("emails");
  db.emails.ensureFulltextIndex("content");
| db.emails.save({ content:
                   "Hello Alice, how are you doing? Regards, Bob"});
| db.emails.save({ content:
                   "Hello Charlie, do Alice and Bob know about it?"});
  db.emails.save({ content: "I think they don't know. Regards, Eve" });
  db.emails.fulltext("content", "charlie,|eve").toArray();
~ db._drop("emails");
@END_EXAMPLE_ARANGOSH_OUTPUT
