

@brief looks up a document and returns it
`db._document(document)`

This method finds a document given its identifier. It returns the document
if the document exists. An error is thrown if no document with the given
identifier exists, or if the specified *_rev* value does not match the
current revision of the document.

**Note**: If the method is executed on the arangod server (e.g. from
inside a Foxx application), an immutable document object will be returned
for performance reasons. It is not possible to change attributes of this
immutable object. To update or patch the returned document, it needs to be
cloned/copied into a regular JavaScript object first. This is not necessary
if the *_document* method is called from out of arangosh or from any
other client.

`db._document(document-id)`

As before. Instead of document a *document-id* can be passed as
first argument.

@EXAMPLES

Returns the document:

@EXAMPLE_ARANGOSH_OUTPUT{documentsDocumentName}
~ db._create("example");
~ var myid = db.example.insert({_key: "12345"});
  db._document("example/12345");
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


