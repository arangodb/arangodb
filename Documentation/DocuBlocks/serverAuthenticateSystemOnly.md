@startDocuBlock serverAuthenticateSystemOnly
`--server.authentication-system-only boolean`

Controls whether incoming requests need authentication only if they are
directed to the ArangoDB's internal APIs and features, located at
*/_api/*,
*/_admin/* etc.

If the flag is set to *true*, then HTTP authentication is only
required for requests going to URLs starting with */_*, but not for other
URLs. The flag can thus be used to expose a user-made API without HTTP
authentication to the outside world, but to prevent the outside world from
using the ArangoDB API and the admin interface without authentication.
Note that checking the URL is performed after any database name prefix
has been removed. That means when the actual URL called is
*/_db/_system/myapp/myaction*, the URL */myapp/myaction* will be used for
*authentication-system-only* check.

The default is *true*.

Note that authentication still needs to be enabled for the server regularly 
in order for HTTP authentication to be forced for the ArangoDB API and the
web interface.  Setting only this flag is not enough.

You can control ArangoDB's general authentication feature with the
*--server.authentication* flag.
@endDocuBlock

