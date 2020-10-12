
@startDocuBlock put_api_replication_revisions_ranges
@brief retrieves the revision IDs of documents within requested ranges

@RESTHEADER{PUT /_api/replication/revisions/ranges, Return revision IDs within requested ranges,handleCommandRevisionRanges}

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

@RESTQUERYPARAM{resume,string,optional}
The revision at which to resume, if a previous request was truncated

@RESTDESCRIPTION
Returns the revision IDs of documents within requested ranges

The body of the request should be JSON/VelocyPack and should consist of an
array of pairs of string-encoded revision IDs:

```
[
  [<String, revision>, <String, revision>],
  [<String, revision>, <String, revision>],
  ...
  [<String, revision>, <String, revision>]
]
```

In particular, the pairs should be non-overlapping, and sorted in ascending
order of their decoded values.

The result will be JSON/VelocyPack in the following format:
```
{
  ranges: [
    [<String, revision>, <String, revision>, ... <String, revision>],
    [<String, revision>, <String, revision>, ... <String, revision>],
    ...,
    [<String, revision>, <String, revision>, ... <String, revision>]
  ]
  resume: <String, revision>
}
```

The `resume` field is optional. If specified, then the response is to be
considered partial, only valid through the revision specified. A subsequent
request should be made with the same request body, but specifying the `resume`
URL parameter with the value specified. The subsequent response will pick up
from the appropriate request pair, and omit any complete ranges or revisions
which are less than the requested resume revision. As an example (ignoring the
string-encoding for a moment), if ranges `[1, 3], [5, 9], [12, 15]` are
requested, then a first response may return `[], [5, 6]` with a resume point of
`7` and a subsequent response might be `[8], [12, 13]`.

If a requested range contains no revisions, then an empty array is returned.
Empty ranges will not be omitted.

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
