Index {#GlossaryIndex}
======================

@GE{Index}: Indexes are used to allow fast access to documents. For
each collection there is always the primary index which is a hash
index for the document key. This index cannot be dropped or changed.

Edge collections will also have an automatically created edges index, 
which cannot be modified.

Most user-land indexes can be created by defining the names of the
attributes which should be indexed. Some index types allow indexing
just one attribute (e.g. fulltext index) whereas other index types
allow indexing multiple attributes.

Indexing system attributes such as `_id`, `_key`, `_from`, and `_to`
is not supported by any index type. Manually creating an index that 
relies on any of these attributes is unsupported.
