Which Index to use when
=======================

ArangoDB automatically indexes the `_key` attribute in each collection. There
is no need to index this attribute separately. Please note that a document's
`_id` attribute is derived from the `_key` attribute, and is thus implicitly
indexed, too.

ArangoDB will also automatically create an index on `_from` and `_to` in any
edge collection, meaning incoming and outgoing connections can be determined
efficiently.

Index types
-----------

Users can define additional indexes on one or multiple document attributes.
Several different index types are provided by ArangoDB. These indexes have
different usage scenarios:

- hash index: provides quick access to individual documents if (and only if)
  all indexed attributes are provided in the search query. The index will only
  be used for equality comparisons. It does not support range queries and 
  cannot be used for sorting.

  The hash index is a good candidate if all or most queries on the indexed
  attribute(s) are equality comparisons. The unique hash index provides an
  amortized complexity of O(1) for insert, update, remove and lookup operations.
  The non-unique hash index provides O(1) inserts, updates and removes, and
  will allow looking up documents by index value with amortized O(n) complexity, 
  with *n* being the number of documents with that index value.
  
  A non-unique hash index on an optional document attribute should be declared
  sparse so that it will not index documents for which the index attribute is
  not set.

- skiplist index: skiplists keep the indexed values in an order, so they can
  be used for equality lookups, range queries and for sorting. For high selectivity
  attributes, skiplist indexes will have a higher overhead than hash indexes. For
  low selectivity attributes, skiplist indexes will be more efficient than non-unique
  hash indexes.

  Additionally, skiplist indexes allow more use cases (e.g. range queries, sorting)
  than hash indexes. Furthermore, they can be used for lookups based on a leftmost
  prefix of the index attributes.

- persistent index: a persistent index behaves much like the sorted skiplist index,
  except that all index values are persisted on disk and do not need to be rebuilt
  in memory when the server is restarted or the indexed collection is reloaded.
  The operations in a persistent index have logarithmic complexity, but operations
  have may have a higher constant factor than the operations in a skiplist index, 
  because the persistent index may need to make extra roundtrips to the primary
  index to fetch the actual documents.

  A persistent index can be used for equality lookups, range queries and for sorting. 
  For high selectivity attributes, persistent indexes will have a higher overhead than 
  skiplist or hash indexes. 

  Persistent indexes allow more use cases (e.g. range queries, sorting) than hash 
  indexes. Furthermore, they can be used for lookups based on a leftmost prefix of the 
  index attributes. In contrast to the in-memory skiplist indexes, persistent indexes
  do not need to be rebuilt in-memory so they don't influence the loading time of
  collections as other in-memory indexes do.

- ttl index: the TTL index provided by ArangoDB can be used for automatically removing 
  expired documents from a collection. 

  The TTL index is set up by setting an `expireAfter` value and by picking a single 
  document attribute which contains the documents' reference timepoint. Documents 
  are expired `expireAfter` seconds after their reference timepoint has been reached.
  The documents' reference timepoint is specified as either a numeric timestamp 
  (Unix timestamp) or a date string in format `YYYY-MM-DDTHH:MM:SS` with optional 
  milliseconds. All date strings will be interpreted as UTC dates.

  For example, if `expireAfter` is set to 600 seconds (10 minutes) and the index
  attribute is "creationDate" and there is the following document:

      { "creationDate" : 1550165973 }

  This document will be indexed with a creation date time value of `1550165973`,
  which translates to the human-readable date `2019-02-14T17:39:33.000Z`. The document
  will expire 600 seconds afterwards, which is at timestamp `1550166573` (or
  `2019-02-14T17:49:33.000Z` in the human-readable version).

  The actual removal of expired documents will not necessarily happen immediately. 
  Expired documents will eventually removed by a background thread that is periodically
  going through all TTL indexes and removing the expired documents. The frequency for
  invoking this background thread can be configured using the `--ttl.frequency`
  startup option. 

  There is no guarantee when exactly the removal of expired documents will be carried
  out, so queries may still find and return documents that have already expired. These
  will eventually be removed when the background thread kicks in and has capacity to
  remove the expired documents. It is guaranteed however that only documents which are 
  past their expiration time will actually be removed.

  Please note that the numeric date time values for the index attribute should be 
  specified in seconds since January 1st 1970 (Unix timestamp). To calculate the current 
  timestamp from JavaScript in this format, there is `Date.now() / 1000`, to calculate it 
  from an arbitrary Date instance, there is `Date.getTime() / 1000`.

  Alternatively, the index attribute values can be specified as a date string in format
  `YYYY-MM-DDTHH:MM:SS` with optional milliseconds. All date strings will be interpreted 
  as UTC dates.
    
  The above example document using a date string attribute value would be
 
      { "creationDate" : "2019-02-14T17:39:33.000Z" }

  In case the index attribute does not contain a numeric value nor a proper date string,
  the document will not be stored in the TTL index and thus will not become a candidate 
  for expiration and removal. Providing either a non-numeric value or even no value for 
  the index attribute is a supported way of keeping documents from being expired and removed.

  TTL indexes are designed exactly for the purpose of removing expired documents from
  a collection. It is *not recommended* to rely on TTL indexes for user-land AQL queries. 
  This is because TTL indexes internally may store a transformed, always numerical version 
  of the index attribute value even if it was originally passed in as a datestring. As a
  result TTL indexes will likely not be used for filtering and sort operations in user-land
  AQL queries.

- geo index: the geo index provided by ArangoDB allows searching for documents
  within a radius around a two-dimensional earth coordinate (point), or to
  find documents with are closest to a point. Document coordinates can either 
  be specified in two different document attributes or in a single attribute, e.g.

      { "latitude": 50.9406645, "longitude": 6.9599115 }

  or

      { "coords": [ 50.9406645, 6.9599115 ] }

  Geo indexes will be invoked via special functions or AQL optimization. The
  optimization can be triggered when a collection with geo index is enumerated
  and a SORT or FILTER statement is used in conjunction with the distance
  function.

- fulltext index: a fulltext index can be used to index all words contained in 
  a specific attribute of all documents in a collection. Only words with a 
  (specifiable) minimum length are indexed. Word tokenization is done using 
  the word boundary analysis provided by libicu, which is taking into account 
  the selected language provided at server start.

  The index supports complete match queries (full words) and prefix queries.
  Fulltext indexes will only be invoked via special functions.

Sparse vs. non-sparse indexes
-----------------------------

Hash indexes and skiplist indexes can optionally be created sparse. A sparse index
does not contain documents for which at least one of the index attribute is not set
or contains a value of `null`.

As such documents are excluded from sparse indexes, they may contain fewer documents than
their non-sparse counterparts. This enables faster indexing and can lead to reduced memory
usage in case the indexed attribute does occur only in some, but not all documents of the 
collection. Sparse indexes will also reduce the number of collisions in non-unique hash
indexes in case non-existing or optional attributes are indexed.

In order to create a sparse index, an object with the attribute `sparse` can be added to
the index creation commands:

```js
db.collection.ensureIndex({ type: "hash", fields: [ "attributeName" ], sparse: true }); 
db.collection.ensureIndex({ type: "hash", fields: [ "attributeName1", "attributeName2" ], sparse: true }); 
db.collection.ensureIndex({ type: "hash", fields: [ "attributeName" ], unique: true, sparse: true }); 
db.collection.ensureIndex({ type: "hash", fields: [ "attributeName1", "attributeName2" ], unique: true, sparse: true }); 

db.collection.ensureIndex({ type: "skiplist", fields: [ "attributeName" ], sparse: true }); 
db.collection.ensureIndex({ type: "skiplist", fields: [ "attributeName1", "attributeName2" ], sparse: true }); 
db.collection.ensureIndex({ type: "skiplist", fields: [ "attributeName" ], unique: true, sparse: true }); 
db.collection.ensureIndex({ type: "skiplist", fields: [ "attributeName1", "attributeName2" ], unique: true, sparse: true }); 
```

When not explicitly set, the `sparse` attribute defaults to `false` for new indexes.
Other indexes than hash and skiplist do not support sparsity.

As sparse indexes may exclude some documents from the collection, they cannot be used for
all types of queries. Sparse hash indexes cannot be used to find documents for which at
least one of the indexed attributes has a value of `null`. For example, the following AQL
query cannot use a sparse index, even if one was created on attribute `attr`:

    FOR doc In collection 
      FILTER doc.attr == null 
      RETURN doc

If the lookup value is non-constant, a sparse index may or may not be used, depending on
the other types of conditions in the query. If the optimizer can safely determine that
the lookup value cannot be `null`, a sparse index may be used. When uncertain, the optimizer
will not make use of a sparse index in a query in order to produce correct results.

For example, the following queries cannot use a sparse index on `attr` because the optimizer
will not know beforehand whether the values which are compared to `doc.attr` will include `null`:

    FOR doc In collection 
      FILTER doc.attr == SOME_FUNCTION(...) 
      RETURN doc

    FOR other IN otherCollection 
      FOR doc In collection 
        FILTER doc.attr == other.attr 
        RETURN doc

Sparse skiplist indexes can be used for sorting if the optimizer can safely detect that the 
index range does not include `null` for any of the index attributes. 

Note that if you intend to use [joins](../../AQL/Examples/Join.html) it may be clever
to use non-sparsity and maybe even uniqueness for that attribute, else all items containing
the `null` value will match against each other and thus produce large results.

