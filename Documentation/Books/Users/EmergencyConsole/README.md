<a name="emergency_console"></a>
# Emergency Console

<a name="in_case_of_disaster"></a>
### In Case Of Disaster

The following command starts a emergency console.

*Note:* Never start the emergency console for a database which also has a
server attached to it. In general the ArangoDB shell is what you want.

	> ./arangod --console --log error /tmp/vocbase
	ArangoDB shell [V8 version 3.9.4, DB version 1.x.y]
	
	arango> 1 + 2;
	3
	
	arango> db.geo.count();
	703
	
<!--@EXAMPLE{start-emergency-console,emergency console} -->

The emergency console disables the HTTP interface of the server and
opens a JavaScript console on standard output instead. This allows you
to debug and examine collections and documents without interference
from the outside. In most respects the emergency console behaves like
the normal ArangoDB shell - but with exclusive access and no
client/server communication.

However, it is very likely that you never need the emergency console
unless you are an ArangoDB developer.

