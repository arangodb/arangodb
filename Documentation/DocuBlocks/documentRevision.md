@startDocuBlock documentRevision

Every document in ArangoDB has a revision, stored in the system attribute
`_rev`. It is fully managed by the server and read-only for the user.

Its value should be treated as opaque, no guarantees regarding its format
and properties are given except that it will be different after a
document update. More specifically, `_rev` values are unique across all
documents and all collections in a single server setup. In a cluster setup,
within one shard it is guaranteed that two different document revisions
have a different `_rev` string, even if they are written in the same
millisecond.

The `_rev` attribute can be used as a pre-condition for queries, to avoid
_lost update_ situations. That is, if a client fetches a document from the server,
modifies it locally (but with the `_rev` attribute untouched) and sends it back
to the server to update the document, but meanwhile the document was changed by
another operation, then the revisions do not match anymore and the operation
is cancelled by the server. Without this mechanism, the client would
accidentally overwrite changes made to the document without knowing about it.

When an existing document is updated or replaced, ArangoDB will write a new
version of this document to the write-ahead logfile (regardless of the
storage engine). When the new version of the document has been written, the
old version(s) will still be present, at least on disk. The same is true when
an existing document (version) gets removed: the old version of the document
plus the removal operation will be on disk for some time.

On disk it is therefore possible that multiple revisions of the same document
(as identified by the same `_key` value) exist at the same time. However,
stale revisions **are not accessible**. Once a document was updated or removed
successfully, no query or other data retrieval operation done by the user
will be able to see it any more. Furthermore, after some time, old revisions
will be removed internally. This is to avoid ever-growing disk usage.

{% hint 'warning' %}
From a **user perspective**, there is just **one single document revision
present per different `_key`** at every point in time. There is no built-in
system to automatically keep a history of all changes done to a document
and old versions of a document can not be restored via the `_rev` value.
{% endhint %}

@endDocuBlock
