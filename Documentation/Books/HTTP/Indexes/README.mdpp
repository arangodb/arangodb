!CHAPTER HTTP Interface for Indexes 

!SUBSECTION Indexes

This is an introduction to ArangoDB's HTTP interface for indexes in
general. There are special sections for various index types.

!SUBSUBSECTION Index

Indexes are used to allow fast access to documents. For each collection there is always the primary index which is a hash index for the
[document key](../../Manual/Appendix/Glossary.html#document-key) (_key attribute). This index cannot be dropped or changed.
[edge collections](../../Manual/Appendix/Glossary.html#edge-collection) will also have an automatically created edges index, which cannot be modified. This index provides quick access to documents via the `_from` and `_to` attributes.

Most user-land indexes can be created by defining the names of the attributes which should be indexed. Some index types allow indexing just one attribute (e.g. fulltext index) whereas other index types allow indexing multiple attributes.

Using the system attribute `_id` in user-defined indexes is not supported by any index type.

!SUBSUBSECTION Index Handle

An index handle uniquely identifies an index in the database. It is a string and consists of a collection name and an index identifier separated by /.
Geo Index: A geo index is used to find places on the surface of the earth fast.
Hash Index: A hash index is used to find documents based on examples. A hash index can be created for one or multiple document attributes.
A hash index will only be used by queries if all indexed attributes are present in the example or search query, and if all attributes are compared using the equality (== operator). That means the hash index does not support range queries.

If the index is declared unique, then access to the indexed attributes should be fast. The performance degrades if the indexed attribute(s) contain(s) only very few distinct values.

!SUBSUBSECTION Edges Index

An edges index is automatically created for edge collections. It contains connections between vertex documents and is invoked when the connecting edges of a vertex are queried. There is no way to explicitly create or delete edge indexes.

!SUBSUBSECTION Skiplist Index

A skiplist is a sorted index that can be used to find individual documents or ranges of documents.

!SUBSUBSECTION Persistent Index

A persistent index is a sorted index that can be used for finding individual documents or ranges of documents.
In constrast to the other indexes, the contents of a persistent index are stored on disk and thus do not need to be rebuilt in memory from the documents when the collection is loaded.

!SUBSUBSECTION Fulltext Index:

A fulltext index can be used to find words, or prefixes of words inside documents. A fulltext index can be set on one attribute only, and will index all words contained in documents that have a textual value in this attribute. Only words with a (specifiable) minimum length are indexed. Word tokenization is done using the word boundary analysis provided by libicu, which is taking into account the selected language provided at server start. Words are indexed in their lower-cased form. The index supports complete match queries (full words) and prefix queries.

The basic operations (create, read, update, delete) for documents are mapped to
the standard HTTP methods (*POST*, *GET*, *PUT*, *DELETE*).

!SECTION Address of an Index 

All indexes in ArangoDB have an unique handle. This index handle identifies an
index and is managed by ArangoDB. All indexes are found under the URI

    http://server:port/_api/index/index-handle

For example: Assume that the index handle is *demo/63563528* then the URL of
that index is:

    http://localhost:8529/_api/index/demo/63563528
