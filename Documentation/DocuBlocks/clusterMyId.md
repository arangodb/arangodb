

@brief this server's id
`--cluster.my-id id`

The local server's id in the cluster. Specifying *id* is mandatory on
startup. Each server of the cluster must have a unique id.

Specifying the id is very important because the server id is used for
determining the server's role and tasks in the cluster.

*id* must be a string consisting of the letters *a-z*, *A-Z* or the
digits *0-9* only.

