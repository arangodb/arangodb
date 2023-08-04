
@startDocuBlock post_api_replication_revisions_tree

@RESTHEADER{POST /_api/replication/revisions/tree, Rebuild the replication revision tree,rebuildReplicationRevisionTree}

@HINTS
{% hint 'warning' %}
This revision-based replication endpoint will only work with collections
created in ArangoDB v3.8.0 or later.
{% endhint %}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The name or id of the collection to query.

@RESTDESCRIPTION
Rebuilds the Merkle tree for a collection.

If successful, there will be no return body.

@RESTRETURNCODES

@RESTRETURNCODE{204}
is returned if the request was executed successfully.

@RESTRETURNCODE{401}
is returned if necessary parameters are missing

@RESTRETURNCODE{404}
is returned when the collection or could not be found.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@RESTRETURNCODE{501}
is returned if called on a collection which doesn't support sync-by-revision

@endDocuBlock
