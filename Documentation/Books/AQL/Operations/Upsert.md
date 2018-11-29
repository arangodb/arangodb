UPSERT
======

The *UPSERT* keyword can be used for checking whether certain documents exist,
and to update/replace them in case they exist, or create them in case they do not exist.
On a single server, upserts are executed transactionally in an all-or-nothing fashion. 

If the RocksDB engine is used and intermediate commits are enabled, a query may 
execute intermediate transaction commits in case the running transaction (AQL
query) hits the specified size thresholds. In this case, the query's operations 
carried out so far will be committed and not rolled back in case of a later abort/rollback. 
That behavior can be controlled by adjusting the intermediate commit settings for 
the RocksDB engine. 

For sharded collections, the entire query and/or upsert operation may not be transactional,
especially if it involves different shards and/or database servers.

Each *UPSERT* operation is restricted to a single collection, and the 
[collection name](../../Manual/Appendix/Glossary.html#collection-name) must not be dynamic.
Only a single *UPSERT* statement per collection is allowed per AQL query, and 
it cannot be followed by read or write operations that access the same collection, by
traversal operations, or AQL functions that can read documents.

The syntax for an upsert operation is:

```
UPSERT searchExpression INSERT insertExpression UPDATE updateExpression IN collection options
UPSERT searchExpression INSERT insertExpression REPLACE updateExpression IN collection options
```

When using the *UPDATE* variant of the upsert operation, the found document will be 
partially updated, meaning only the attributes specified in *updateExpression* will be 
updated or added. When using the *REPLACE* variant of upsert, existing documents will 
be replaced with the contexts of *updateExpression*.

Updating a document will modify the document's revision number with a server-generated value.
The system attributes *_id*, *_key* and *_rev* cannot be updated, *_from* and *_to* can.

The *searchExpression* contains the document to be looked for. It must be an object 
literal without dynamic attribute names. In case no such document can be found in
*collection*, a new document will be inserted into the collection as specified in the
*insertExpression*. 

In case at least one document in *collection* matches the *searchExpression*, it will
be updated using the *updateExpression*. When more than one document in the collection
matches the *searchExpression*, it is undefined which of the matching documents will
be updated. It is therefore often sensible to make sure by other means (such as unique 
indexes, application logic etc.) that at most one document matches *searchExpression*.

The following query will look in the *users* collection for a document with a specific
*name* attribute value. If the document exists, its *logins* attribute will be increased
by one. If it does not exist, a new document will be inserted, consisting of the
attributes *name*, *logins*, and *dateCreated*:

```
UPSERT { name: 'superuser' } 
INSERT { name: 'superuser', logins: 1, dateCreated: DATE_NOW() } 
UPDATE { logins: OLD.logins + 1 } IN users
```

Note that in the *UPDATE* case it is possible to refer to the previous version of the
document using the *OLD* pseudo-value.


Setting query options
---------------------

As in several above examples, the *ignoreErrors* option can be used to suppress query 
errors that may occur when trying to violate unique key constraints.

When updating or replacing an attribute with a null value, ArangoDB will not remove the 
attribute from the document but store a null value for it. To get rid of attributes in 
an upsert operation, set them to null and provide the *keepNull* option.

There is also the option *mergeObjects* that controls whether object contents will be
merged if an object attribute is present in both the *UPDATE* query and in the 
to-be-updated document.

Note: the default value for *mergeObjects* is *true*, so there is no need to specify it
explicitly.

To make sure data are durable when an update query returns, there is the *waitForSync* 
query option.

In order to not accidentially update documents that have been written and updated since 
you last fetched them you can use the option *ignoreRevs* to either let ArangoDB compare 
the `_rev` value and only succeed if they still match, or let ArangoDB ignore them (default):

```
FOR i IN 1..1000
  UPSERT { _key: CONCAT('test', i)}
    INSERT {foobar: false}
    UPDATE {_rev: "1287623", foobar: true }
  IN users OPTIONS { ignoreRevs: false }
```

*NOTE*: You need to add the `_rev` value in the updateExpression, it will not be used within 
the searchExpression. Even worse, if you use an outdated `_rev` in the searchExpression
UPSERT will trigger the INSERT path instead of the UPDATE path, because it has not found a document
exactly matching the searchExpression.

In contrast to the MMFiles engine, the RocksDB engine does not require collection-level
locks. Different write operations on the same collection do not block each other, as
long as there are no _write-write conficts_ on the same documents. From an application
development perspective it can be desired to have exclusive write access on collections,
to simplify the development. Note that writes do not block reads in RocksDB.
Exclusive access can also speed up modification queries, because we avoid conflict checks.

Use the *exclusive* option to achieve this effect on a per query basis:

```js
FOR i IN 1..1000
  UPSERT { _key: CONCAT('test', i) }
  INSERT { foobar: false }
  UPDATE { foobar: true }
  IN users OPTIONS { exclusive: true }
```

Returning documents
-------------------

`UPSERT` statements can optionally return data. To do so, they need to be followed
by a `RETURN` statement (intermediate `LET` statements are allowed, too). These statements
can optionally perform calculations and refer to the pseudo-values `OLD` and `NEW`.
In case the upsert performed an insert operation, `OLD` will have a value of *null*.
In case the upsert performed an update or replace operation, `OLD` will contain the
previous version of the document, before update/replace.

`NEW` will always be populated. It will contain the inserted document in case the
upsert performed an insert, or the updated/replaced document in case it performed an
update/replace.

This can also be used to check whether the upsert has performed an insert or an update 
internally:

```
UPSERT { name: 'superuser' } 
INSERT { name: 'superuser', logins: 1, dateCreated: DATE_NOW() } 
UPDATE { logins: OLD.logins + 1 } IN users
RETURN { doc: NEW, type: OLD ? 'update' : 'insert' }
```
