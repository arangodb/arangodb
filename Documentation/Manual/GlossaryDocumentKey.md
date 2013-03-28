DocumentKey {#GlossaryDocumentKey}
================================================

@GE{Document Key}: A document key uniquely identifies a document
in a given collection. It can and should be used by clients when
specific documents are searched.
Document keys are stored in the `_key` attribute of documents.
The key values are automatically indexed by ArangoDB so looking 
up a document by its key is regularly a fast operation.
The `_key` value of a document is immutable once the document has
been created.

By default, ArangoDB will auto-generate a document key if no `_key`
attribute is specified, and use the user-specified `_key` otherwise.

This behavior can be changed on a per-collection level by creating 
collections with the `keyOptions` attribute. 

Using `keyOptions` it is possible to disallow user-specified keys
completely, or to force a specific regime for auto-generating the
`_key` values.
