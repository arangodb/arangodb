Audit Configuration
===================

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

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

Verbosity
---------

`--log.level topic=level`

By default, the server will log all audit events. Some low-priority events, such
as statistics operations, are logged with the `debug` log level. To keep such
events from cluttering the log, set the appropriate topic to `info`. All other
messages will be logged at the `info` level. Audit topics include
`audit-authentication`, `audit-collection`, `audit-database`, `audit-documents`,
`audit-service`, and `audit-views`. 
