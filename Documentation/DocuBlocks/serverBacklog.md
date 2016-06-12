

@brief listen backlog size
`--tcp.backlog-size`

Allows to specify the size of the backlog for the *listen* system call
The default value is 10. The maximum value is platform-dependent.
Specifying
a higher value than defined in the system header's SOMAXCONN may result in
a warning on server start. The actual value used by *listen* may also be
silently truncated on some platforms (this happens inside the *listen*
system call).

