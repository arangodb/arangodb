Fulltext indexes
================

### Introduction to Fulltext Indexes

This is an introduction to ArangoDB's fulltext indexes.

A fulltext index can be used to find words, or prefixes of words inside documents. 

A fulltext index can be defined on one attribute only, and will include all words contained in 
documents that have a textual value in the index attribute. Since ArangoDB 2.6 the index 
will also include words from the index attribute if the index attribute is an array of 
strings, or an object with string value members.

For example, given a fulltext index on the `translations` attribute and the following 
documents, then searching for `лиса` using the fulltext index would return only the 
first document. Searching for the index for the exact string `Fox` would return the first 
two documents, and searching for `prefix:Fox` would return all three documents:
     
    { translations: { en: "fox", de: "Fuchs", fr: "renard", ru: "лиса" } }
    { translations: "Fox is the English translation of the German word Fuchs" }
    { translations: [ "ArangoDB", "document", "database", "Foxx" ] }

If the index attribute is neither a string, an object or an array, its contents will 
not be indexed. When indexing the contents of an array attribute, an array member will 
only be included in the index if it is a string. When indexing the contents of an object 
attribute, an object member value will only be included in the index if it is a string. 
Other data types are ignored and not indexed.

Accessing Fulltext Indexes from the Shell
-----------------------------------------

<!-- js/server/modules/org/arangodb/arango-collection.js -->
@startDocuBlock ensureFulltextIndex

@startDocuBlock lookUpFulltextIndex