!CHAPTER Skip-Lists


!SECTION Introduction to Skiplist Indexes

This is an introduction to ArangoDB's skiplists.

It is possible to define a skiplist index on one or more attributes (or paths)
of a documents. This skiplist is then used in queries to locate documents
within a given range. If the skiplist is unique, then no two documents are
allowed to have the same set of attribute values.

Creating a new document or updating a document will fail if the uniqueness is violated. 
If the skiplist index is declared sparse, a document will be excluded from the index and no 
uniqueness checks will be performed if any index attribute value is not set or has a value 
of `null`. 

!SECTION Accessing Skiplist Indexes from the Shell

<!-- js/server/modules/org/arangodb/arango-collection.js-->
@startDocuBlock ensureUniqueSkiplist

<!-- js/server/modules/org/arangodb/arango-collection.js-->
@startDocuBlock ensureSkiplist

!SUBSECTION Query by example using a skiplist index
@startDocuBlock collectionByExampleSkiplist

