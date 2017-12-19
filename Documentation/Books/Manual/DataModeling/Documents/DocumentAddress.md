!CHAPTER Basics and Terminology

Documents in ArangoDB are JSON objects. These objects can be nested (to
any depth) and may contain lists. Each document has a unique 
[primary key](../../Appendix/Glossary.md#document-key) which 
identifies it within its collection. Furthermore, each document is 
uniquely identified
by its [document handle](../../Appendix/Glossary.md#document-handle) 
across all collections in the same database. Different revisions of
the same document (identified by its handle) can be distinguished by their 
[document revision](../../Appendix/Glossary.md#document-revision).
Any transaction only ever sees a single revision of a document.
For example:

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
[document handle](../../Appendix/Glossary.md#document-handle) is stored
as a string in `_id`, the
[document's primary key](../../Appendix/Glossary.md#document-key) in 
`_key` and the 
[document revision](../../Appendix/Glossary.md#document-revision) in
`_rev`. The value of the `_key` attribute can be specified by the user when
creating a document. `_id` and `_key` values are immutable once the document
has been created. The `_rev` value is maintained by ArangoDB automatically.


!SUBSECTION Document Handle

A document handle uniquely identifies a document in the database. It
is a string and consists of the collection's name and the document key
(`_key` attribute) separated by `/`.


!SUBSECTION Document Key

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


!SUBSECTION Document Revision

As ArangoDB supports MVCC (Multiple Version Concurrency Control),
documents can exist in more than one
revision. The document revision is the MVCC token used to specify 
a particular revision of a document (identified by its `_id`). 
It is a string value currently
containing an integer number and is unique within the list of document
revisions for a single document. Document revisions can be used to
conditionally query, update, replace or delete documents in the database. In
order to find a particular revision of a document, you need the document
handle or key, and the document revision.

ArangoDB uses 64bit unsigned integer values to maintain
document revisions internally. When returning document revisions to
clients, ArangoDB will put them into a string to ensure the revision
is not clipped by clients that do not support big integers. Clients
should treat the revision returned by ArangoDB as an opaque string
when they store or use it locally. This will allow ArangoDB to change
the format of revisions later if this should be required. Clients can
use revisions to perform simple equality/non-equality comparisons
(e.g. to check whether a document has changed or not), but they should
not use revision ids to perform greater/less than comparisons with them
to check if a document revision is older than one another, even if this
might work for some cases.


!SUBSECTION Multiple Documents in a single Command

Beginning with ArangoDB 3.0 the basic document API has been extended
to handle not only single documents but multiple documents in a single
command. This is crucial for performance, in particular in the cluster
situation, in which a single request can involve multiple network hops
within the cluster. Another advantage is that it reduces the overhead of
individual network round trips between the client
and the server. The general idea to perform multiple document operations 
in a single command is to use JSON arrays of objects in the place of a 
single document. As a consequence, document keys, handles and revisions
for preconditions have to be supplied embedded in the individual documents
given. Multiple document operations are restricted to a single document
or edge collection. 
See the [API descriptions for collection objects](DocumentMethods.md) 
for details. Note that the [API for database objects](DatabaseMethods.md)
do not offer these operations.

