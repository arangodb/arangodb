<a name="address_and_etag_of_a_document"></a>
# Address and ETag of a Document

All documents in ArangoDB have a document handle. This handle uniquely identifies 
a document. Any document can be retrieved using its unique URI:

    http://server:port/_api/document/<document-handle>

For example, assumed that the document handle, which is stored in the `_id`
attribute of the document, is `demo/362549736`, then the URL of that document
is:

    http://localhost:8529/_api/document/demo/362549736

The above URL scheme does not specify a database name explicitly, so the 
default database will be used. To explicitly specify the database context, use
the following URL schema:

    http://server:port/_db/<database-name>/_api/document/<document-handle>

Example:

    http://localhost:8529/_db/mydb/_api/document/demo/362549736

Note that the following examples use the short URL format for brevity.

Each document also has a document revision or etag with is returned in the
"ETag" HTTP header when requesting a document.

If you obtain a document using `GET` and you want to check if a newer revision
is available, then you can use the `If-None-Match` header. If the document is
unchanged, a `HTTP 412` (precondition failed) error is returned.

If you want to update or delete a document, then you can use the `If-Match`
header. If the document has changed, then the operation is aborted and a `HTTP
412` error is returned.

