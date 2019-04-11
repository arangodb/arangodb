Basics and Terminology
======================

Documents, Keys, Handles and Revisions
--------------------------------------

Documents in ArangoDB are JSON objects. These objects can be nested (to
any depth) and may contain lists. Each document has a unique 
[primary key](../../Manual/Appendix/Glossary.html#document-key) which 
identifies it within its collection. Furthermore, each document is 
uniquely identified
by its [document handle](../../Manual/Appendix/Glossary.html#document-handle) 
across all collections in the same database. Different revisions of
the same document (identified by its handle) can be distinguished by their 
[document revision](../../Manual/Appendix/Glossary.html#document-revision).
Any transaction only ever sees a single revision of a document.

Here is an example document:

```js
{
  "_id" : "myusers/3456789",
  "_key" : "3456789",
  "_rev" : "14253647",
  "firstName" : "John",
  "lastName" : "Doe",
  "address" : {
    "street" : "Road To Nowhere 1",
    "city" : "Gotham"
  },
  "hobbies" : [
    {name: "swimming", howFavorite: 10},
    {name: "biking", howFavorite: 6},
    {name: "programming", howFavorite: 4}
  ]
}
```

All documents contain special attributes: the 
[document handle](../../Manual/Appendix/Glossary.html#document-handle) is stored
as a string in `_id`, the
[document's primary key](../../Manual/Appendix/Glossary.html#document-key) in 
`_key` and the 
[document revision](../../Manual/Appendix/Glossary.html#document-revision) in
`_rev`. The value of the `_key` attribute can be specified by the user when
creating a document. `_id` and `_key` values are immutable once the document
has been created. The `_rev` value is maintained by ArangoDB automatically.


Document Handle
---------------

A document handle uniquely identifies a document in the database. It
is a string and consists of the collection's name and the document key
(`_key` attribute) separated by `/`.


Document Key
------------

A document key uniquely identifies a document in the collection it is
stored in. It can and should be used by clients when specific documents
are queried. The document key is stored in the `_key` attribute of
each document. The key values are automatically indexed by ArangoDB in
a collection's primary index. Thus looking up a document by its
key is a fast operation. The _key value of a document is
immutable once the document has been created. By default, ArangoDB will
auto-generate a document key if no _key attribute is specified, and use
the user-specified _key otherwise.

This behavior can be changed on a per-collection level by creating
collections with the `keyOptions` attribute.

Using `keyOptions` it is possible to disallow user-specified keys
completely, or to force a specific regime for auto-generating the `_key`
values.


Document Revision
-----------------

@startDocuBlock documentRevision


Document Etag
-------------

ArangoDB tries to adhere to the existing HTTP standard as far as
possible. To this end, results of single document queries have the HTTP
header `Etag` set to the document revision enclosed in double quotes.

The basic operations (create, read, exists, replace, update, delete)
for documents are mapped to the standard HTTP methods (*POST*, *GET*,
*HEAD*, *PUT*, *PATCH* and *DELETE*).

If you modify a document, you can use the *If-Match* field to detect conflicts. 
The revision of a document can be checking using the HTTP method *HEAD*.


Multiple Documents in a single Request
--------------------------------------

Beginning with ArangoDB 3.0 the basic document API has been extended
to handle not only single documents but multiple documents in a single
request. This is crucial for performance, in particular in the cluster
situation, in which a single request can involve multiple network hops
within the cluster. Another advantage is that it reduces the overhead of
the HTTP protocol and individual network round trips between the client
and the server. The general idea to perform multiple document operations 
in a single request is to use a JSON array of objects in the place of a 
single document. As a consequence, document keys, handles and revisions
for preconditions have to be supplied embedded in the individual documents
given. Multiple document operations are restricted to a single document
or edge collections. 
See the [API descriptions](WorkingWithDocuments.md) for details.

Note that the *GET*, *HEAD* and *DELETE* HTTP operations generally do
not allow to pass a message body. Thus, they cannot be used to perform
multiple document operations in one request. However, there are other
endpoints to request and delete multiple documents in one request.
FIXME: ADD SENSIBLE LINKS HERE.


URI of a Document
-----------------

Any document can be retrieved using its unique URI:

    http://server:port/_api/document/<document-handle>

For example, assuming that the document handle
is `demo/362549736`, then the URL of that document
is:

    http://localhost:8529/_api/document/demo/362549736

The above URL schema does not specify a 
[database name](../../Manual/Appendix/Glossary.html#database-name) 
explicitly, so the 
default database `_system` will be used. 
To explicitly specify the database context, use
the following URL schema:

    http://server:port/_db/<database-name>/_api/document/<document-handle>

Example:

    http://localhost:8529/_db/mydb/_api/document/demo/362549736

**Note**: The following examples use the short URL format for brevity.

The [document revision](../../Manual/Appendix/Glossary.html#document-revision) 
is returned in the "Etag" HTTP header when requesting a document.

If you obtain a document using *GET* and you want to check whether a 
newer revision
is available, then you can use the *If-None-Match* header. If the document is
unchanged, a *HTTP 412* (precondition failed) error is returned.

If you want to query, replace, update or delete a document, then you
can use the *If-Match* header. If the document has changed, then the
operation is aborted and an *HTTP 412* error is returned.
