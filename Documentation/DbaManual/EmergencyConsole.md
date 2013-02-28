Emergency Console {#DbaManualEmergencyConsole}
==============================================

@NAVIGATE_DbaManualEmergencyConsole
@EMBEDTOC{DbaManualEmergencyConsoleTOC}

In Case Of Disaster
===================

The following command starts a emergency console.

@note Never start the emergency console for a database which also has a
server attached to it. In general the ArangoDB shell is what you want.

@EXAMPLE{start-emergency-console,emergency console}

The emergency console disables the HTTP interface of the server and
opens a JavaScript console on standard output instead. This allows you
to debug and examine collections and documents without interference
from the outside. In most respects the emergency console behaves like
the normal ArangoDB shell - but with exclusive access and no
client/server communication.

However, it is very likely that you never need the emergency console
unless you are an ArangoDB developer.
