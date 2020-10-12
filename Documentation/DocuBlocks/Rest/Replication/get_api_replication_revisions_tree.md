
@startDocuBlock get_api_replication_revisions_tree
@brief retrieves the Merkle tree associated with the collection

@RESTHEADER{GET /_api/replication/revisions/tree, Return Merkle tree for a collection,handleCommandRevisionTree}

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
Returns the Merkle tree from the collection.

The result will be JSON/VelocyPack in the following format:
```
{
  version: <Number>,
  branchingFactor: <Number>
  maxDepth: <Number>,
  rangeMin: <String, revision>,
  rangeMax: <String, revision>,
  nodes: [
    { count: <Number>, hash: <String, revision> },
    { count: <Number>, hash: <String, revision> },
    ...
    { count: <Number>, hash: <String, revision> }
  ]
}
```

At the moment, there is only one version, 1, so this can safely be ignored for
now.

Each `<String, revision>` value type is a 64-bit value encoded as a string of
11 characters, using the same encoding as our document `_rev` values. The
reason for this is that 64-bit values cannot necessarily be represented in full
in JavaScript, as it handles all numbers as floating point, and can only
represent up to `2^53-1` faithfully.

The node count should correspond to a full tree with the given `maxDepth` and
`branchingFactor`. The nodes are laid out in level-order tree traversal, so the
root is at index `0`, its children at indices `[1, branchingFactor]`, and so
on.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully and data was returned.

@RESTRETURNCODE{401}
is returned if necessary parameters are missing

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
