

@brief try to reuse address
`--tcp.reuse-address`

If this boolean option is set to *true* then the socket option
SO_REUSEADDR is set on all server endpoints, which is the default.
If this option is set to *false* it is possible that it takes up
to a minute after a server has terminated until it is possible for
a new server to use the same endpoint again. This is why this is
activated by default.

Please note however that under some operating systems this can be
a security risk because it might be possible for another process
to bind to the same address and port, possibly hijacking network
traffic. Under Windows, ArangoDB additionally sets the flag
SO_EXCLUSIVEADDRUSE as a measure to alleviate this problem.

