

@brief looks up a document
`collection.document(document)`

The *document* method finds a document given its identifier or a document
object containing the *_id* or *_key* attribute. The method returns
the document if it can be found.

An error is thrown if *_rev* is specified but the document found has a
different revision already. An error is also thrown if no document exists
with the given *_id* or *_key* value.

Please note that if the method is executed on the arangod server (e.g. from
inside a Foxx application), an immutable document object will be returned
for performance reasons. It is not possible to change attributes of this
immutable object. To update or patch the returned document, it needs to be
cloned/copied into a regular JavaScript object first. This is not necessary
if the *document* method is called from out of arangosh or from any other
client.

`collection.document(document-id)`

As before. Instead of document a *document-id* can be passed as
first argument.

*Examples*

Returns the document for a document-id:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionName}
~ db._create("example");
~ var myid = db.example.insert({_key: "2873916"});
  db.example.document("example/2873916");
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

An error is raised if the document is unknown:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionNameUnknown}
~ db._create("example");
~ var myid = db.example.insert({_key: "2873916"});
| db.example.document("example/4472917");
~     // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND)
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

An error is raised if the identifier is invalid:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionNameHandle}
~ db._create("example");
  db.example.document(""); // xpError(ERROR_ARANGO_DOCUMENT_HANDLE_BAD)
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


