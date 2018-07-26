Foxx reference
==============

Each Foxx service is defined by a [JSON manifest](Manifest.md)
specifying the entry point, any scripts defined by the service,
possible [configuration](Configuration.md) options and Foxx dependencies,
as well as other metadata. Within a service, these options are exposed as the
[service context](Context.md), which is also used to mount
[routers](Routers/README.md) defining the service's API endpoints.

Foxx also provides a number of [utility modules](Modules/README.md)
as well as a flexible [sessions middleware](Sessions/README.md)
with different transport and storage mechanisms.

Foxx services can be installed and managed over the Web-UI or through
ArangoDB's [HTTP API](../../../HTTP/Foxx/Management.html).
