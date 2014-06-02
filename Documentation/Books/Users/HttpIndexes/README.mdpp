!CHAPTER HTTP Interface for Indexes 

!SUBSECTION Indexes

This is an introduction to ArangoDB's Http interface for indexes in
general. There are special sections for various index types.

!SUBSUBSECTION Index

Indexes are used to allow fast access to documents. For each collection there is always the primary index which is a hash index for the document key (_key attribute). This index cannot be dropped or changed.
Edge collections will also have an automatically created edges index, which cannot be modified. This index provides quick access to documents via the _from and _to attributes.

Most user-land indexes can be created by defining the names of the attributes which should be indexed. Some index types allow indexing just one attribute (e.g. fulltext index) whereas other index types allow indexing multiple attributes.

Indexing system attributes such as _id, _key, _from, and _to in user-defined indexes is not supported by any index type. Manually creating an index that relies on any of these attributes is unsupported.

!SUBSUBSECTION Index Handle

An index handle uniquely identifies an index in the database. It is a string and consists of a collection name and an index identifier separated by /.
Geo Index: A geo index is used to find places on the surface of the earth fast.
Hash Index: A hash index is used to find documents based on examples. A hash index can be created for one or multiple document attributes.
A hash index will only be used by queries if all indexed attributes are present in the example or search query, and if all attributes are compared using the equality (== operator). That means the hash index does not support range queries.

If the index is declared unique, then access to the indexed attributes should be fast. The performance degrades if the indexed attribute(s) contain(s) only very few distinct values.

!SUBSUBSECTION Edges Index

An edges index is automatically created for edge collections. It contains connections between vertex documents and is invoked when the connecting edges of a vertex are queried. There is no way to explicitly create or delete edge indexes.

!SUBSUBSECTION Skiplist Index:

A skiplist is used to find ranges of documents.

!SUBSUBSECTION Fulltext Index:

A fulltext index can be used to find words, or prefixes of words inside documents. A fulltext index can be set on one attribute only, and will index all words contained in documents that have a textual value in this attribute. Only words with a (specifiable) minimum length are indexed. Word tokenization is done using the word boundary analysis provided by libicu, which is taking into account the selected language provided at server start. Words are indexed in their lower-cased form. The index supports complete match queries (full words) and prefix queries.

The basic operations (create, read, update, delete) for documents are mapped to
the standard HTTP methods (`POST`, `GET`, `PUT`, `DELETE`).

