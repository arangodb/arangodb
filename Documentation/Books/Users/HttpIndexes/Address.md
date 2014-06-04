<a name="address_of_an_index"></a>
# Address of an Index

All indexes in ArangoDB have an unique handle. This index handle identifies an
index and is managed by ArangoDB. All indexes are found under the URI

    http://server:port/_api/index/index-handle

For example: Assume that the index handle is `demo/63563528` then the URL of
that index is:

    http://localhost:8529/_api/index/demo/63563528
