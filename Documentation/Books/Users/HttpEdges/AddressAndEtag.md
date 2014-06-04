<a name="address_and_etag_of_an_edge"></a>
# Address and ETag of an Edge

All documents in ArangoDB have a document handle. This handle uniquely identifies 
a document. Any document can be retrieved using its unique URI:

    http://server:port/_api/document/<document-handle>

Edges are a special variation of documents, and to work with edges, the above URL
format changes to:

    http://server:port/_api/edge/<document-handle>

For example, assumed that the document handle, which is stored in the `_id`
attribute of the edge, is `demo/362549736`, then the URL of that edge is:

    http://localhost:8529/_api/edge/demo/362549736

The above URL scheme does not specify a database name explicitly, so the 
default database will be used. To explicitly specify the database context, use
the following URL schema:

    http://server:port/_db/<database-name>/_api/edge/<document-handle>

Example:

    http://localhost:8529/_db/mydb/_api/edge/demo/362549736

Note that the following examples use the short URL format for brevity.

