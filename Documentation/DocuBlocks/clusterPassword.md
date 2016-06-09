

@brief password used for cluster-internal communication
`--cluster.password password`

The password used for authorization of cluster-internal requests.
This password will be used to authenticate all requests and responses in
cluster-internal communication, i.e. requests exchanged between
coordinators and individual database servers.

This option is used for cluster-internal requests only. Regular requests
to
coordinators are authenticated normally using the data in the `_users`
collection.

If coordinators and database servers are run with authentication turned
off, (e.g. by setting the *--server.authentication* option to *false*),
the cluster-internal communication will also be unauthenticated.

