REPLACE
=======

The *REPLACE* keyword can be used to completely replace documents in a collection. On a
single server, the replace operation is executed transactionally in an all-or-nothing 
fashion. For sharded collections, the entire replace operation is not transactional.

Each *REPLACE* operation is restricted to a single collection, and the 
[collection name](../../Manual/Appendix/Glossary.html#collection-name) must not be dynamic.
Only a single *REPLACE* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.
The system attributes *_id*, *_key* and *_rev* cannot be replaced, *_from* and *_to* can.

The two syntaxes for a replace operation are:

```
REPLACE document IN collection options
REPLACE keyExpression WITH document IN collection options
```

*collection* must contain the name of the collection in which the documents should
be replaced. *document* is the replacement document. When using the first syntax, *document* 
must also contain the *_key* attribute to identify the document to be replaced. 

```
FOR u IN users
  REPLACE { _key: u._key, name: CONCAT(u.firstName, u.lastName), status: u.status } IN users
```

The following query is invalid because it does not contain a *_key* attribute and
thus it is not possible to determine the documents to be replaced:

```
FOR u IN users
  REPLACE { name: CONCAT(u.firstName, u.lastName, status: u.status) } IN users
```

When using the second syntax, *keyExpression* provides the document identification.
This can either be a string (which must then contain the document key) or a
document, which must contain a *_key* attribute.

The following queries are equivalent:

```
FOR u IN users
  REPLACE { _key: u._key, name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  REPLACE u._key WITH { name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  REPLACE { _key: u._key } WITH { name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  REPLACE u WITH { name: CONCAT(u.firstName, u.lastName) } IN users
```

A replace will fully replace an existing document, but it will not modify the values
of internal attributes (such as *_id*, *_key*, *_from* and *_to*). Replacing a document
will modify a document's revision number with a server-generated value.

A replace operation may update arbitrary documents which do not need to be identical
to the ones produced by a preceding *FOR* statement:

```
FOR i IN 1..1000
  REPLACE CONCAT('test', i) WITH { foobar: true } IN users

FOR u IN users
  FILTER u.active == false
  REPLACE u WITH { status: 'inactive', name: u.name } IN backup
```

### Setting query options

*options* can be used to suppress query errors that may occur when trying to
replace non-existing documents or when violating unique key constraints:

```
FOR i IN 1..1000
  REPLACE { _key: CONCAT('test', i) } WITH { foobar: true } IN users OPTIONS { ignoreErrors: true }
```

To make sure data are durable when a replace query returns, there is the *waitForSync* 
query option:

```
FOR i IN 1..1000
  REPLACE { _key: CONCAT('test', i) } WITH { foobar: true } IN users OPTIONS { waitForSync: true }
```

### Returning the modified documents

The modified documents can also be returned by the query. In this case, the `REPLACE` 
statement must be followed by a `RETURN` statement (intermediate `LET` statements are
allowed, too). The `OLD` pseudo-value can be used to refer to document revisions before 
the replace, and `NEW` refers to document revisions after the replace.

Both `OLD` and `NEW` will contain all document attributes, even those not specified 
in the replace expression.


```
REPLACE document IN collection options RETURN OLD
REPLACE document IN collection options RETURN NEW
REPLACE keyExpression WITH document IN collection options RETURN OLD
REPLACE keyExpression WITH document IN collection options RETURN NEW
```

Following is an example using a variable named `previous` to return the original
documents before modification. For each replaced document, the document key will be
returned:

```
FOR u IN users
  REPLACE u WITH { value: "test" } 
  LET previous = OLD 
  RETURN previous._key
```

The following query uses the `NEW` pseudo-value to return the replaced
documents (without some of their system attributes):

```
FOR u IN users
  REPLACE u WITH { value: "test" } 
  LET replaced = NEW 
  RETURN UNSET(replaced, '_key', '_id', '_rev')
```

