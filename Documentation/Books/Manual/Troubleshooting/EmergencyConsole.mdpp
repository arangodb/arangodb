!CHAPTER Emergency Console

The ArangoDB database server has two modes of operation: As a server, where it
will answer to client requests and as an emergency console, in which you can
access the database directly. The latter - as the name suggests - should
only be used in case of an emergency, for example, a corrupted
collection. Using the emergency console allows you to issue all commands
normally available in actions and transactions. When starting the server in
emergency console mode, the server cannot handle any client requests.

You should never start more than one server using the same database directory,
independent of the mode of operation. Normally, ArangoDB will prevent
you from doing this by placing a lockfile in the database directory and
not allowing a second ArangoDB instance to use the same database directory
if a lockfile is already present.

!SUBSECTION In Case Of Disaster

The following command starts an emergency console.

**Note**: Never start the emergency console for a database which also has a
server attached to it. In general, the ArangoDB shell is what you want.

```
> ./arangod --console --log error /tmp/vocbase
ArangoDB shell [V8 version 5.0.71.39, DB version 3.x.x]

arango> 1 + 2;
3

arango> var db = require("@arangodb").db; db.geo.count();
703

```

The emergency console provides a JavaScript console directly running in the
arangod server process. This allows to debug and examine collections and
documents as with the normal ArangoDB shell, but without client/server
communication.

However, it is very likely that you will never need the emergency console
unless you are an ArangoDB developer.

