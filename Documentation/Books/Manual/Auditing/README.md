Auditing
========

__This feature is available in the Enterprise Edition.__

Auditing allows you to monitor access to the database in detail. In general
audit logs are of the form

```
2016-01-01 12:00:00 | server | username | database | client-ip | authentication | text1 | text2 | ...
```

The *time-stamp* is in GMT. This allows to easily match log entries from servers
in different time zones.

The name of the *server*. You can specify a custom name on startup. Otherwise
the default hostname is used.

The *username* is the (authenticated or unauthenticated) name supplied by the
client. A dash `-` is printed if no name was given by the client.

The *database* describes the database that was accessed. Please note that there
are no database crossing queries. Each access is restricted to one database.

The *client-ip* describes the source of the request.

The *authentication* details the methods used to authenticate the user.

Details about the requests follow in the additional fields.
