Fulltext indexes
================

This is an introduction to ArangoDB's fulltext indexes.

### Introduction to Fulltext Indexes

A fulltext index can be used to find words, or prefixes of words inside documents. 

A fulltext index can be defined on one attribute only, and will include all words contained in 
documents that have a textual value in the index attribute. Since ArangoDB 2.6 the index 
will also include words from the index attribute if the index attribute is an array of 
strings, or an object with string value members.

For example, given a fulltext index on the `translations` attribute and the following 
documents, then searching for `лиса` using the fulltext index would return only the 
first document. Searching for the index for the exact string `Fox` would return the first 
two documents, and searching for `prefix:Fox` would return all three documents:

```js
{ translations: { en: "fox", de: "Fuchs", fr: "renard", ru: "лиса" } }
{ translations: "Fox is the English translation of the German word Fuchs" }
{ translations: [ "ArangoDB", "document", "database", "Foxx" ] }
```

Note that deeper nested objects are ignored. For example, a fulltext index on
*translations* would index *Fuchs*, but not *fox*, given the following document
structure:

```js
{ translations: { en: { US: "fox" }, de: "Fuchs" }
```

If you need to search across multiple fields and/or nested objects, you may write
all the strings into a special attribute, which you then create the index on
(it might be necessary to clean the strings first, e.g. remove line breaks and
strip certain words).

If the index attribute is neither a string, an object or an array, its contents will 
not be indexed. When indexing the contents of an array attribute, an array member will 
only be included in the index if it is a string. When indexing the contents of an object 
attribute, an object member value will only be included in the index if it is a string. 
Other data types are ignored and not indexed.

Accessing Fulltext Indexes from the Shell
-----------------------------------------

<!-- js/server/modules/@arangodb/arango-collection.js -->

Ensures that a fulltext index exists:

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

    @startDocuBlockInline ensureFulltextIndex
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
    @endDocuBlock ensureFulltextIndex


Looks up a fulltext index:

`collection.lookupFulltextIndex(attribute, minLength)`

Checks whether a fulltext index on the given attribute *attribute* exists.

Fulltext AQL Functions
----------------------

Fulltext AQL functions are detailed in [Fulltext functions](../../AQL/Functions/Fulltext.html).
