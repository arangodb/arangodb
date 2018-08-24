Foxx Microservices
==================

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

How it works
------------

Foxx services consist of JavaScript code running in the V8 JavaScript runtime
embedded inside ArangoDB. Each service is mounted in each available V8 context
(the number of contexts can be adjusted in the server configuration).
Incoming requests are distributed across these contexts automatically.

If you're coming from another JavaScript environment like Node.js this is
similar to running multiple Node.js processes behind a load balancer:
you should not rely on server-side state (other than the database itself)
between different requests as there is no way of making sure consecutive
requests will be handled in the same context.

Because the JavaScript code is running inside the database another difference
is that all Foxx and ArangoDB APIs are purely synchronous and should be
considered blocking. This is especially important for transactions,
which in ArangoDB can execute arbitrary code but may have to lock
entire collections (effectively preventing any data to be written)
until the code has completed.

Compatibility caveats
---------------------

Unlike JavaScript in browsers or Node.js, the JavaScript environment
in ArangoDB is synchronous. This means any code that depends on asynchronous
behavior like promises or `setTimeout` will not behave correctly in
ArangoDB or Foxx. Additionally, ArangoDB does not support native extensions
unlike Node.js. All code has to be implemented in pure JavaScript.

While ArangoDB provides a lot of compatibility code to support code written
for Node.js, some Node.js built-in modules can not be provided by ArangoDB.
For a closer look at the Node.js modules ArangoDB does or
does not provide check out
the [appendix on JavaScript modules](../Appendix/JavaScriptModules/README.md).

When using [bundled node modules](Guides/BundledNodeModules.md) keep in mind
that these restrictions not only apply to the modules themselves but also
the node dependencies of those modules. As a rule of thumb:

- Modules written to work in Node.js and the browser that do not
  rely on async behavior should generally work

- Modules that rely on network or filesystem I/O or make heavy use
  of async behavior most likely will not
