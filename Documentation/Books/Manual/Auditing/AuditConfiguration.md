Audit Configuration
===================

__This feature is available in the Enterprise Edition.__

Output
------

`--audit.output output`

Specifies the target of the audit log. Possible values are

`file://filename` where *filename* can be relative or absolute.

`syslog://facility` or `syslog://facility/application-name` to log
into a syslog server.

The option can be specified multiple times in order to configure the
output for multiple targets.

Hostname
--------

`--audit.hostname name`

The name of the server used in audit log messages. By default the
system hostname is used.