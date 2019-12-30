

@brief checks whether a document exists
`collection.exists(document)`

The *exists* method determines whether a document exists given its
identifier.  Instead of returning the found document or an error, this
method will return either *true* or *false*. It can thus be used
for easy existence checks.

The *document* method finds a document given its identifier.  It returns
the document. Note that the returned document contains two
pseudo-attributes, namely *_id* and *_rev*. *_id* contains the
document-id and *_rev* the revision of the document.

No error will be thrown if the sought document or collection does not
exist.
Still this method will throw an error if used improperly, e.g. when called
with a non-document identifier, a non-document, or when a cross-collection
request is performed.

`collection.exists(document-id)`

As before. Instead of document a *document-id* can be passed as
first argument.


