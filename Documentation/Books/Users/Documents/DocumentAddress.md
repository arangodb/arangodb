Address and ETag of a Document
==============================

All documents in ArangoDB have a [document handle](../Glossary/README.md#document-handle). This handle uniquely defines a
document and is managed by ArangoDB. The interface allows you to access the
documents of a collection as:

    db.collection.document("document-handle")

For example: Assume that the document handle, which is stored in the *_id* field
of the document, is *demo/362549* and the document lives in a collection
named *demo*, then that document can be accessed as:

    db.demo.document("demo/362549736")

Because the document handle is unique within the database, you
can leave out the *collection* and use the shortcut:

    db._document("demo/362549736")

Each document also has a [document revision](../Glossary/README.md#document-revision) or ETag which is returned in the
*_rev* field when requesting a document. The document's key is returned in the
*_key* attribute.
