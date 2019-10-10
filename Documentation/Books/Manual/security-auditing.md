---
layout: default
description: Audit logging, available in the Enterprise Edition
title: Auditing
---
Auditing
========

{% hint 'info' %}
Auditing is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/){:target="_blank"},
also available as [**managed service**](https://www.arangodb.com/managed-service/){:target="_blank"}.
{% endhint %}

Auditing allows you to monitor access to the database in detail. In general
audit logs are of the form

```
<time-stamp> | <server> | <topic> | <username> | <database> | <client-ip> | <authentication> | <text1> | <text2> | ...
```

The *time-stamp* is in GMT. This allows to easily match log entries from servers
in different time zones.

The name of the *server*. You can specify a custom name on startup. Otherwise
the default hostname is used.

The *topic* is the log topic for the entry. A topic can be suppressed through
the `log.level` configuration option or the REST API.

The *username* is the (authenticated or unauthenticated) name supplied by the
client. A dash `-` is printed if no name was given by the client.

The *database* describes the database that was accessed. Please note that there
are no database crossing queries. Each access is restricted to one database.

The *client-ip* describes the source of the request.

The *authentication* details the methods used to authenticate the user.

Details about the requests follow in the additional fields.

Any additional fields (e.g. *text1* and *text2*) will be determined by the type
of log message. Most messages will include a status of `ok` or `failed`.
