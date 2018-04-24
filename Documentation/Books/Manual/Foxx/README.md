Foxx
====

Traditionally, server-side projects have been developed as standalone applications
that guide the communication between the client-side frontend and the database
backend. This has led to applications that were either developed as single
monoliths or that duplicated data access and domain logic across all services
that had to access the database. Additionally, tools to abstract away the
underlying database calls could incur a lot of network overhead when using remote
databases without careful optimization.

ArangoDB allows application developers to write their data access and domain logic
as microservices running directly within the database with native access to
in-memory data. The **Foxx microservice framework** makes it easy to extend
ArangoDB's own REST API with custom HTTP endpoints using modern JavaScript running
on the same V8 engine you know from Node.js and the Google Chrome web browser.

Unlike traditional approaches to storing logic in the database (like stored
procedures), these microservices can be written as regular structured JavaScript
applications that can be easily distributed and version controlled. Depending on
your project's needs Foxx can be used to build anything from optimized REST
endpoints performing complex data access to entire standalone applications
running directly inside the database.

How does it work
----------------

Foxx services consist of JavaScript code running in the V8 JavaScript runtime embedded inside ArangoDB. Each service is mounted in each available V8 context (the number of contexts can be adjusted in the ArangoDB configuration). Incoming requests are distributed accross these contexts automatically.

If you're coming from another JavaScript environment like Node.js this is similar to running multiple Node.js processes behind a load balancer: you should not rely on server-side state (other than the database itself) between different requests as there is no way of making sure consecutive requests will be handled in the same context.

Because the JavaScript code is running inside the database another difference is that all Foxx and ArangoDB APIs are purely synchronous and should be considered blocking. This is especially important for transactions, which in ArangoDB can execute arbitrary code but may have to lock entire collections (effectively preventing any data to be written) until the code has completed.

For information on how this affects interoperability with third-party JavaScript modules written for other JavaScript environments see [the chapter on dependencies](Reference/Dependencies.md).

Foxx in a cluster setup
-----------------------

When running ArangoDB in a cluster the Foxx services will run on each coordinator. Installing, upgrading and uninstalling services on any coordinator will automatically distribute the service to the other coordinators, making deployments as easy as in single-server mode. However, this means there are some limitations:

You should avoid any kind of file system state beyond the deployed service bundle itself. Don't write data to the file system or encode any expectations of the file system state other than the files in the service folder that were installed as part of the service (e.g. file uploads or custom log files).

Additionally, the development mode will lead to an inconsistent state of the cluster until it is disabled. While a service is running in development mode you can make changes to the service on the filesystem of any coordinator and see them reflected in real time just like when running ArangoDB as a single server. However the changes made on one coordinator will not be reflected across other coordinators until the development mode is disabled. When disabling the development mode for a service, the coordinator will create a new bundle and distribute it across the service like a manual upgrade of the service.

For these reasons it is strongly recommended not to use development mode in a cluster with multiple coordinators unless you are sure that no requests or changes will be made to other coordinators while you are modifying the service. Using development mode in a production cluster is extremely unsafe and highly discouraged.

Development mode
----------------

Development mode allows you to make changes to deployed services in-place directly on the database server's file system without downloading and re-uploading the service bundle. Additionally error messages will contain stacktraces.

You can toggle development mode on and off in the service settings tab of the web interface or using the [HTTP API](../../HTTP/Foxx/Miscellaneous.html). Once activated the service's file system path will be shown in the info tab.

<!-- TODO (Add link to relevant aardvark docs) -->

Once enabled the service's source files and manifest will be re-evaluated, and the setup script (if present) re-executed, every time a route of the service is accessed, effectively re-deploying the service on every request. As the name indicates this is intended to be used strictly during development and is most definitely a bad idea on production servers. The additional information exposed during development mode may include file system paths and parts of the service's source code.

Also note that if you are serving static files as part of your service, accessing these files from a browser may also trigger a re-deployment of the service. Finally, making HTTP requests to a service running in development mode from within the service (i.e. using the `@arangodb/request` module to access the service itself) is probably not a good idea either.

Beware of deleting the database the service is deployed on: it will erase the source files of the service along with the collections. You should backup the code you worked on in development before doing that to avoid losing your progress.
