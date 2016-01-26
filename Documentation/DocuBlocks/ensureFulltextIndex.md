

@brief ensures that a fulltext index exists
`collection.ensureIndex({ type: "fulltext", fields: [ "field" ], minLength: minLength })`

Creates a fulltext index on all documents on attribute *field*.

Fulltext indexes are implicitly sparse: all documents which do not have 
the specified *field* attribute or that have a non-qualifying value in their 
*field* attribute will be ignored for indexing.

Only a single attribute can be indexed. Specifying multiple attributes is 
unsupported.

The minimum length of words that are indexed can be specified via the
*minLength* parameter. Words shorter than minLength characters will 
not be indexed. *minLength* has a default value of 2, but this value might
be changed in future versions of ArangoDB. It is thus recommended to explicitly
specify this value.

In case that the index was successfully created, an object with the index
details is returned.

@EXAMPLE_ARANGOSH_OUTPUT{ensureFulltextIndex}
~db._create("example");
db.example.ensureIndex({ type: "fulltext", fields: [ "text" ], minLength: 3 });
db.example.save({ text : "the quick brown", b : { c : 1 } });
db.example.save({ text : "quick brown fox", b : { c : 2 } });
db.example.save({ text : "brown fox jums", b : { c : 3 } });
db.example.save({ text : "fox jumps over", b : { c : 4 } });
db.example.save({ text : "jumps over the", b : { c : 5 } });
db.example.save({ text : "over the lazy", b : { c : 6 } });
db.example.save({ text : "the lazy dog", b : { c : 7 } });
db._query("FOR document IN FULLTEXT(example, 'text', 'the') RETURN document");
~db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

