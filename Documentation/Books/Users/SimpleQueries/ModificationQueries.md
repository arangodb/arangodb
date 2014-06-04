!CHAPTER Modification Queries

ArangoDB also allows removing, replacing, and updating documents based 
on an example document.  Every document in the collection will be 
compared against the specified example document and be deleted/replaced/
updated if all attributes match.

These method should be used with caution as they are intended to remove or
modify lots of documents in a collection.

All methods can optionally be restricted to a specific number of operations.
However, if a limit is specific but is less than the number of matches, it
will be undefined which of the matching documents will get removed/modified.

!SUBSUBSECTION Examples

  arangod> db.content.removeByExample({ "domain": "de.celler" })

`collection.replaceByExample( example, newValue)`

Replaces all documents matching an example with a new document body. The entire document body of each document matching the example will be replaced with newValue. The document meta-attributes such as _id, _key, _from, _to will not be replaced.

`collection.replaceByExample( document, newValue, waitForSync)`

The optional waitForSync parameter can be used to force synchronisation of the document replacement operation to disk even in case that the waitForSync flag had been disabled for the entire collection. Thus, the waitForSync parameter can be used to force synchronisation of just specific operations. To use this, set the waitForSync parameter to true. If the waitForSync parameter is not specified or set to false, then the collection's default waitForSync behavior is applied. The waitForSync parameter cannot be used to disable synchronisation for collections that have a default waitForSync value of true.

`collection.replaceByExample( document, newValue, waitForSync, limit)`

The optional limit parameter can be used to restrict the number of replacements to the specified value. If limit is specified but less than the number of documents in the collection, it is undefined which documents are replaced.

!SUBSUBSECTION Examples

  arangod> db.content.replaceByExample({ "domain": "de.celler" }, { "foo": "someValue }, false, 5)

`collection.updateByExample( example, newValue)`

Partially updates all documents matching an example with a new document body. Specific attributes in the document body of each document matching the example will be updated with the values from newValue. The document meta-attributes such as _id, _key, _from, _to cannot be updated.

`collection.updateByExample( document, newValue, keepNull, waitForSync)`

The optional keepNull parameter can be used to modify the behavior when handling null values. Normally, null values are stored in the database. By setting the keepNull parameter to false, this behavior can be changed so that all attributes in data with null values will be removed from the target document.

The optional waitForSync parameter can be used to force synchronisation of the document replacement operation to disk even in case that the waitForSync flag had been disabled for the entire collection. Thus, the waitForSync parameter can be used to force synchronisation of just specific operations. To use this, set the waitForSync parameter to true. If the waitForSync parameter is not specified or set to false, then the collection's default waitForSync behavior is applied. The waitForSync parameter cannot be used to disable synchronisation for collections that have a default waitForSync value of true.

`collection.updateByExample( document, newValue, keepNull, waitForSync, limit)`

The optional limit parameter can be used to restrict the number of updates to the specified value. If limit is specified but less than the number of documents in the collection, it is undefined which documents are updated.

!SUBSUBSECTION Examples

  arangod> db.content.updateByExample({ "domain": "de.celler" }, { "foo": "someValue, "domain": null }, false)


<!--
@anchor SimpleQueryRemoveByExample
@copydetails JSF_ArangoCollection_prototype_removeByExample

@anchor SimpleQueryReplaceByExample
@copydetails JSF_ArangoCollection_prototype_replaceByExample

@anchor SimpleQueryUpdateByExample
@copydetails JSF_ArangoCollection_prototype_updateByExample

@BNAVIGATE_SimpleQueries
-->