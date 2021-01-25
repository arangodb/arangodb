
@startDocuBlock put_api_replication_revisions_documents
@brief retrieves documents by revision

@RESTHEADER{PUT /_api/replication/revisions/documents, Return documents by revision,handleCommandRevisionDocuments}

@HINTS
{% hint 'warning' %}
This revision-based replication endpoint will only work with the RocksDB
engine, and with collections created in ArangoDB v3.8.0 or later.
{% endhint %}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The name or id of the collection to query.

@RESTQUERYPARAM{batchId,number,required}
The id of the snapshot to use

@RESTDESCRIPTION
Returns documents by revision

The body of the request should be JSON/VelocyPack and should consist of an
array of string-encoded revision IDs:

```
[
  <String, revision>,
  <String, revision>,
  ...
  <String, revision>
]
```

In particular, the revisions should be sorted in ascending order of their
decoded values.

The result will be a JSON/VelocyPack array of document objects. If there is no
document corresponding to a particular requested revision, an empty object will
be returned in its place.

The response may be truncated if it would be very long. In this case, the
response array length will be less than the request array length, and
subsequent requests can be made for the omitted documents.

Each `<String, revision>` value type is a 64-bit value encoded as a string of
11 characters, using the same encoding as our document `_rev` values. The
reason for this is that 64-bit values cannot necessarily be represented in full
in JavaScript, as it handles all numbers as floating point, and can only
represent up to `2^53-1` faithfully.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully and data was returned.

@RESTRETURNCODE{401}
is returned if necessary parameters are missing or incorrect

@RESTRETURNCODE{404}
is returned when the collection or snapshot could not be found.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@RESTRETURNCODE{501}
is returned if called with mmfiles or on a collection which doesn't support
sync-by-revision

@endDocuBlock
