!CHAPTER Hash Indexes

!SUBSECTION Introduction to Hash Indexes

This is an introduction to ArangoDB's hash indexes.

It is possible to define a hash index on one or more attributes (or paths) of a
document. This hash index is then used in queries to locate documents in O(1)
operations. If the hash index is unique, then no two documents are allowed to have the
same set of attribute values.

Creating a new document or updating a document will fail if the uniqueness is violated. 
If the index is declared sparse, a document will be excluded from the index and no 
uniqueness checks will be performed if any index attribute value is not set or has a value 
of `null`. 

!SECTION Accessing Hash Indexes from the Shell

<!-- js/server/modules/org/arangodb/arango-collection.js-->
@startDocuBlock ensureUniqueConstraint

<!-- js/server/modules/org/arangodb/arango-collection.js-->
@startDocuBlock ensureHashIndex

