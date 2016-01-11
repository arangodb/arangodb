

@brief constructs a closed range query for a collection
`collection.closedRange(attribute, left, right)`

Returns all documents of a collection such that the *attribute* is
greater or equal than *left* and less or equal than *right*.

You can use *toArray*, *next*, or *hasNext* to access the
result. The result can be limited using the *skip* and *limit*
operator.

An attribute name of the form *a.b* is interpreted as attribute path,
not as attribute.

Note: the *closedRange* simple query function is **deprecated** as of ArangoDB 2.6. 
The function may be removed in future versions of ArangoDB. The preferred
way for retrieving documents from a collection within a specific range
is to use an AQL query as follows: 

    FOR doc IN @@collection 
      FILTER doc.value >= @left && doc.value <= @right 
      LIMIT @skip, @limit 
      RETURN doc

@EXAMPLES

Use *toArray* to get all documents at once:

@EXAMPLE_ARANGOSH_OUTPUT{006_collectionClosedRange}
~ db._create("old");
  db.old.ensureIndex({ type: "skiplist", fields: [ "age" ] });
  db.old.save({ age: 15 });
  db.old.save({ age: 25 });
  db.old.save({ age: 30 });
  db.old.closedRange("age", 10, 30).toArray();
~ db._drop("old")
@END_EXAMPLE_ARANGOSH_OUTPUT

