
!CHAPTER REMOVE

The *REMOVE* keyword can be used to remove documents from a collection. On a
single server, the document removal is executed transactionally in an 
all-or-nothing fashion. For sharded collections, the entire remove operation
is not transactional.

Each *REMOVE* operation is restricted to a single collection, and the 
[collection name](../../Manual/Appendix/Glossary.html#collection-name) must not be dynamic.
Only a single *REMOVE* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
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

!SUBSECTION Setting query options

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

!SUBSECTION Returning the removed documents

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

