

@brief the user id to use for the process
`--uid uid`

The name (identity) of the user the server will run as. If this parameter
is
not specified, the server will not attempt to change its UID, so that the
UID used by the server will be the same as the UID of the user who started
the server. If this parameter is specified, then the server will change
its
UID after opening ports and reading configuration files, but before
accepting connections or opening other files (such as recovery files).
This
is useful when the server must be started with raised privileges (in
certain
environments) but security considerations require that these privileges be
dropped once the server has started work.

Observe that this parameter cannot be used to bypass operating system
security. In general, this parameter (and its corresponding relative gid)
can lower privileges but not raise them.

