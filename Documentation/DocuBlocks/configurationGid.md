@brief the group id to use for the process
`--gid gid`

The name (identity) of the group the server will run as. If this parameter
is not specified, then the server will not attempt to change its GID, so
that the GID the server runs as will be the primary group of the user who
started the server. If this parameter is specified, then the server will
change its GID after opening ports and reading configuration files, but
before accepting connections or opening other files (such as recovery
files).

This parameter is related to the parameter uid.

