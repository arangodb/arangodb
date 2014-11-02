!CHAPTER Emergency Console

!SUBSECTION In Case Of Disaster

The following command starts an emergency console.

**Note**: Never start the emergency console for a database which also has a
server attached to it. In general the ArangoDB shell is what you want.

```
> ./arangod --console --log error /tmp/vocbase
ArangoDB shell [V8 version 3.9.4, DB version 1.x.y]

arango> 1 + 2;
3

arango> var db = require("org/arangodb").db; db.geo.count();
703

```

The emergency console provides a JavaScript console directly running in the
arangod server process. This allows to debug and examine collections and 
documents as with the normal ArangoDB shell, but without client/server
communication.

However, it is very likely that you will never need the emergency console
unless you are an ArangoDB developer.

