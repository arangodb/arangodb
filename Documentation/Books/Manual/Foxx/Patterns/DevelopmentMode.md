Development mode
----------------

Development mode allows you to make changes to deployed services in-place directly on the database server's file system without downloading and re-uploading the service bundle. Additionally error messages will contain stacktraces.

You can toggle development mode on and off in the service settings tab of the web interface or using the [HTTP API](../../HTTP/Foxx/Miscellaneous.html). Once activated the service's file system path will be shown in the info tab.

<!-- TODO (Add link to relevant aardvark docs) -->

Once enabled the service's source files and manifest will be re-evaluated, and the setup script (if present) re-executed, every time a route of the service is accessed, effectively re-deploying the service on every request. As the name indicates this is intended to be used strictly during development and is most definitely a bad idea on production servers. The additional information exposed during development mode may include file system paths and parts of the service's source code.

Also note that if you are serving static files as part of your service, accessing these files from a browser may also trigger a re-deployment of the service. Finally, making HTTP requests to a service running in development mode from within the service (i.e. using the `@arangodb/request` module to access the service itself) is probably not a good idea either.

Beware of deleting the database the service is deployed on: it will erase the source files of the service along with the collections. You should backup the code you worked on in development before doing that to avoid losing your progress.
