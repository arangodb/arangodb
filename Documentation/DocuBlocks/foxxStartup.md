
@startDocuBlock foxxStartup
@brief enable or disable the Foxx service propagation on startup
`--foxx.force-update-on-startup flag`

if *true*, all Foxx services in all databases will be synchronized between
multiple coordinators during the boot sequence. This ensures that all Foxx
services are up-to-date when a coordinator reports itself as ready.

In case the option is set to `false` (i.e. no waiting), the coordinator 
will complete the boot sequence faster, and the Foxx services will be 
propagated lazily. Until the initialization procedure has completed for
the local Foxx apps, any request to a Foxx app will be responded to with
an HTTP 500 error and message 

    waiting for initialization of Foxx services in this database 

This can cause an unavailability window for Foxx services on coordinator
startup for the initial requests to Foxx apps until the app propagation 
has completed.
  
When not using Foxx, this option should be set to *false* to benefit from a 
faster coordinator startup.
Deployments relying on Foxx apps being available as soon as a coordinator 
is integrated or responding should set this option to *true* (which is 
the default value).

The option only has an effect for cluster setups. On single servers and in 
active failover mode, all Foxx apps will be available from the very beginning.

Note: ArangoDB 3.8 changes the default value to *false* for this option.
In previous versions this option had a default value of *true*.
@endDocuBlock

