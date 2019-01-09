
REMOVE
======

The *REMOVE* keyword can be used to remove documents from a collection. On a
single server, the document removal is executed transactionally in an 
all-or-nothing fashion. 

If the RocksDB engine is used and intermediate commits are enabled, a query may 
execute intermediate transaction commits in case the running transaction (AQL
query) hits the specified size thresholds. In this case, the query's operations 
carried out so far will be committed and not rolled back in case of a later abort/rollback. 
That behavior can be controlled by adjusting the intermediate commit settings for 
the RocksDB engine. 

For sharded collections, the entire query and/or remove operation may not be transactional,
especially if it involves different shards and/or database servers.

Each *REMOVE* operation is restricted to a single collection, and the 
[collection name](../../Manual/Appendix/Glossary.html#collection-name) must not be dynamic.
Only a single *REMOVE* statement per collection is allowed per AQL query, and 
it cannot be followed by read or write operations that access the same collection, by
traversal operations, or AQL functions that can read documents.

The syntax for a remove operation is:

```
REMOVE keyExpression IN collection options
```

*collection* must contain the name of the collection to remove the documents 
from. *keyExpression* must be an expression that contains the document identification.
This can either be a string (which must then contain the
[document key](../../Manual/Appendix/Glossary.html#document-key)) or a
document, which must contain a *_key* attribute.

The following queries are thus equivalent:

```
FOR u IN users
  REMOVE { _key: u._key } IN users

FOR u IN users
  REMOVE u._key IN users

FOR u IN users
  REMOVE u IN users
```

**Note**: A remove operation can remove arbitrary documents, and the documents
do not need to be identical to the ones produced by a preceding *FOR* statement:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users

FOR u IN users
  FILTER u.active == false
  REMOVE { _key: u._key } IN backup
```

A single document can be removed as well, using a document key string or a
document with `_key` attribute:

```
REMOVE 'john' IN users
```

```
LET doc = DOCUMENT('users/john')
REMOVE doc IN users
```

The restriction of a single remove operation per query and collection
applies. The following query causes an *access after data-modification*
error because of the third remove operation:

```
REMOVE 'john' IN users
REMOVE 'john' IN backups // OK, different collection
REMOVE 'mary' IN users   // Error, users collection again
```

Setting query options
---------------------

*options* can be used to suppress query errors that may occur when trying to
remove non-existing documents. For example, the following query will fail if one
of the to-be-deleted documents does not exist:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users
```

By specifying the *ignoreErrors* query option, these errors can be suppressed so 
the query completes:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users OPTIONS { ignoreErrors: true }
```

To make sure data has been written to disk when a query returns, there is the *waitForSync* 
query option:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users OPTIONS { waitForSync: true }
```

In order to not accidentially remove documents that have been updated since you last fetched
them, you can use the option *ignoreRevs* to either let ArangoDB compare the `_rev` values and 
only succeed if they still match, or let ArangoDB ignore them (default):

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i), _rev: "1287623" } IN users OPTIONS { ignoreRevs: false }
```

In contrast to the MMFiles engine, the RocksDB engine does not require collection-level
locks. Different write operations on the same collection do not block each other, as
long as there are no _write-write conficts_ on the same documents. From an application
development perspective it can be desired to have exclusive write access on collections,
to simplify the development. Note that writes do not block reads in RocksDB.
Exclusive access can also speed up modification queries, because we avoid conflict checks.

Use the *exclusive* option to achieve this  effect on a per query basis:

```js
    FOR doc IN collection
      REPLACE doc._key 
      WITH { replaced: true } 
      OPTIONS { exclusive: true }
```


Returning the removed documents
-------------------------------

The removed documents can also be returned by the query. In this case, the `REMOVE` 
statement must be followed by a `RETURN` statement (intermediate `LET` statements
are allowed, too).`REMOVE` introduces the pseudo-value `OLD` to refer to the removed
documents:

```
REMOVE keyExpression IN collection options RETURN OLD
```

Following is an example using a variable named `removed` for capturing the removed
documents. For each removed document, the document key will be returned.

```
FOR u IN users
  REMOVE u IN users 
  LET removed = OLD 
  RETURN removed._key
```
