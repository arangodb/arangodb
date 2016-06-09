

@brief disable the replication applier on server startup
`--database.replication-applier flag`

If *false* the server will start with the replication applier turned off,
even if the replication applier is configured with the *autoStart* option.
Using the command-line option will not change the value of the *autoStart*
option in the applier configuration, but will suppress auto-starting the
replication applier just once.

If the option is set to *true*, ArangoDB will read the applier configuration
from the file *REPLICATION-APPLIER-CONFIG* on startup, and use the value of the
*autoStart* attribute from this file.

The default is *true*.

