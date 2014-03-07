Handling Indexes {#HandlingIndexes}
===================================

@NAVIGATE_HandlingIndexes
@EMBEDTOC{HandlingIndexesTOC}

Indexes, Identifiers, Handles {#HandlingIndexesIntro}
=====================================================

This is an introduction to ArangoDB's interface for indexes in general.  
There are special sections for 

- @ref IndexCap "cap constraints", 
- @ref IndexGeo "geo-spatial indexes",
- @ref IndexHash "hash indexes",
- and @ref IndexSkiplist "skip-lists".

@copydoc GlossaryIndex

@copydoc GlossaryIndexHandle

@copydoc GlossaryIndexGeo

@copydoc GlossaryIndexHash

@copydoc GlossaryIndexEdge

@copydoc GlossaryIndexSkiplist

@copydoc GlossaryIndexFulltext

Address and ETag of an Index {#HandlingIndexesResource}
=======================================================

All indexes in ArangoDB have an index handle. This handle uniquely defines an
index and is managed by ArangoDB. The interface allows you to access the indexes
of a collection as:

    db.@FA{collection}.index(@FA{index-handle})

For example: Assume that the index handle, which is stored in the `_id`
attribute of the index, is `demo/362549736` and the index lives in a collection
named @FA{demo}, then that index can be accessed as:

    db.demo.index("demo/362549736")

Because the index handle is unique within the database, you can leave out the
@FA{collection} and use the shortcut:

    db._index("demo/362549736")

Which Index type to use when {#HandlingIndexesWhichWhen}
========================================================

ArangoDB automatically indexes the `_key` attribute in each collection. There
is no need to index this attribute separately. Please note that a document's
`_id` attribute is derived from the `_key` attribute, and is thus implicitly
indexed, too.

ArangoDB will also automatically create an index on `_from` and `_to` in any
edge collection, meaning incoming and outgoing connections can be determined
efficiently.

Users can define additional indexes on one or multiple document attributes.
Several different index types are provided by ArangoDB. These indexes have
different usage scenarios:

- hash index: provides quick access to individual documents if (and only if)
  all indexed attributes are provided in the search query. The index will only
  be used for equality comparisons. It does not support range queries.

  The hash index is a good candidate if all or most queries on the indexed
  attribute(s) are equality comparisons, and if the attribute selectivity is
  high. That means the number of distinct attribute values in relation to the 
  total number of documents should be high. This is the case for indexes
  declared `unique`. 

  The hash index should not be used if duplicate index values are allowed 
  (i.e. if the hash index is not declared `unique`) and it cannot be avoided
  that there will be many duplicate index values. For example, it should be
  avoided to use a hash index on an attribute with just 10 distinct values in a 
  collection with a million documents.

- skip list index: skip lists keep the indexed values in an order, so they can
  be used for equality and range queries. Skip list indexes will have a slightly
  higher overhead than hash indexes in case but they are more general and
  allow more use cases (e.g. range queries). Additionally, they can be used
  for lower selectivity attributes, when non-unique hash indexes are not a
  good fit.

- geo index: the geo index provided by ArangoDB allows searching for documents
  within a radius around a two-dimensional earth coordinate (point), or to
  find documents with are closest to a point. Document coordinates can either 
  be specified in two different document attributes or in a single attribute, e.g.

    { "latitude": 50.9406645, "longitude": 6.9599115 }

  or

    { "coords": [ 50.9406645, 6.9599115 ] }

- fulltext index: a fulltext index can be used to index all words contained in 
  a specific attribute of all documents in a collection. Only words with a 
  (specifyable) minimum length are indexed. Word tokenisation is done using 
  the word boundary analysis provided by libicu, which is taking into account 
  the selected language provided at server start.

  The index supports complete match queries (full words) and prefix queries.

- cap constraint: the cap constraint provided by ArangoDB indexes documents
  not to speed up search queries, but to limit (cap) the number or size of
  documents in a collection.

Currently it is not possible to index system attributes in user-defined indexes.

Working with Indexes {#HandlingIndexesShell}
============================================

Collection Methods {#HandlingIndexesCollectionMethods}
------------------------------------------------------

@anchor HandlingIndexesRead
@copydetails JSF_ArangoCollection_prototype_index

@CLEARPAGE
@anchor HandlingIndexesReadAll
@copydetails JS_GetIndexesVocbaseCol

@CLEARPAGE
@anchor HandlingIndexesDelete
@copydetails JS_DropIndexVocbaseCol

@CLEARPAGE
@anchor HandlingIndexesEnsure
@copydetails JS_EnsureIndexVocbaseCol

@CLEARPAGE
Database Methods {#HandlingIndexesDatabaseMethods}
--------------------------------------------------

@anchor HandlingIndexesDbRead
@copydetails JSF_ArangoDatabase_prototype__index

@CLEARPAGE
@anchor HandlingIndexesDbDelete
@copydetails JSF_ArangoDatabase_prototype__dropIndex

@BNAVIGATE_HandlingIndexes
