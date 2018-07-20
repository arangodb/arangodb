Foxx reference
==============

Each Foxx service is defined by a [JSON manifest](Manifest.md) specifying the entry point,
any scripts defined by the service, possible configuration options and Foxx dependencies,
as well as other metadata. Within a service, these options are exposed as the [service context](Context.md).

At the heart of the Foxx framework lies the [Foxx Router](Routers/README.md) which is used to define HTTP endpoints.
A service can access the database either directly from its context using prefixed collections or the [ArangoDB database API](Modules/README.md).

While Foxx is primarily designed to be used to access the database itself, ArangoDB also provides an [API to make HTTP requests](Modules/README.md) to external services.

[Scripts](Scripts.md) can be used to perform one-off tasks, which can also be scheduled to be performed asynchronously using the built-in job queue.

Finally, Foxx services can be installed and managed over the Web-UI or through
ArangoDB's [HTTP API](../../../HTTP/Foxx/Management.html).
