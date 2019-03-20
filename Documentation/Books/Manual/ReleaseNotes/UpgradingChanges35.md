Incompatible changes in ArangoDB 3.5
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.5, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.5:

Startup options
---------------

The hidden startup option `--rocksdb.delayed_write_rate` was renamed to the more
consistent `--rocksdb.delayed-write-rate`. When the old option name is used, the 
arangod startup will be aborted with a descriptive error message.

Web interface
-------------

### Potentially different sort order of documents

In the list of documents for a collection, the documents will now always be sorted
in lexicographical order of their `_key` values. An exception for keys representing 
quasi-numerical values has been removed when doing the sorting in the web interface.

Therefore a document with a key value "10" will now be displayed before a document
with a key value of "9".

### Removal of index types "skiplist" and "persistent" (RocksDB engine)

For the RocksDB engine, the selection of index types "persistent" and "skiplist" 
has been removed from the web interface when creating new indexes. 

The index types "hash", "skiplist" and "persistent" are just aliases of each other 
when using the RocksDB engine, so there is no need to offer all of them in parallel.


AQL
---

3.5 enforces the invalidation of variables in AQL queries after usage of a AQL 
COLLECT statements as documented. The documentation for variable invalidation claims
that

    The COLLECT statement will eliminate all local variables in the current scope. 
    After COLLECT only the variables introduced by COLLECT itself are available.

However, the described behavior was not enforced when a COLLECT was preceded by a
FOR loop that was itself preceded by a COLLECT. In the following query the final
RETURN statement accesses variable `key1` though the variable should have been 
invalidated by the COLLECT directly before it:

    FOR x1 IN 1..2 
      COLLECT key1 = x1 
      FOR x2 IN 1..2 
        COLLECT key2 = x2 
        RETURN [key2, key1] 
  
In previous releases, this query was
parsed ok, but the contents of variable `key1` in the final RETURN statement were
undefined. 
  
This change is about making queries as the above fail with a parse error, as an 
unknown variable `key1` is accessed here, avoiding the undefined behavior. This is 
also in line with what the documentation states about variable invalidation.

Miscellaneous
-------------

### Index creation

In previous versions of ArangoDB, if one attempted to create an index with a
specified `_id`, and that `_id` was already in use, the server would typically
return the existing index with matching `_id`. This is somewhat unintuitive, as
it would ignore if the rest of the definition did not match. This behavior has
been changed so that the server will now return a duplicate identifier error.
