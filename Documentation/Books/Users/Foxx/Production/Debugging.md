!CHAPTER Available Debugging mechanisms

We are talking about the production mode for Foxx.
We assume that the development process is over and you have a (third-party) client using your API.
In this case you will most likely want to expose stacktraces or detailed error information to the requesting client.
This of course restricts the debugging mechanisms for the client and requires to consult the logs.
Nevertheless the client should see if there is an internal error occurring therefore all Foxxes will have the behavior described in the following.

Right now we assume you have the production mode enabled (default after install)

```
unix>foxx-manager production /example
Activated production mode for Application hello-foxx version 1.5.0 on mount point /example
```

!SECTION Errors during install

Malformed Foxx applications cannot be installed via the Foxx-manager.
You will get detailed error information if you try to.
However you might create the following situation:

1. Set a Foxx to development mode
2. Make it malformed
3. Set it to production mode

In this case all routes of this Foxx will create a general, HTML page for third party clients stating that there was an internal error.
This page does not contain any information specific for your Foxx.

![Broken App Screenshot](ProductionErrorScreen.png)

!SECTION Errors in routes

If you have an unhandled error in one of your routes the error message will be returned together with an HTTP status code 500.
It will not contain the stacktrace of your error.

```
unix>curl -X GET http://localhost:8529/_db/_system/example/failed
HTTP/1.1 500 Internal Error
Server: ArangoDB
Connection: Keep-Alive
Content-Type: application/json
Content-Length: 27

{"error":"Unhandled Error"}
```

!SECTION Errors in logs

Independent from the errors presented in the routes on requests Foxxes will always log errors to the log-file if caught by the default error handlers.
The log entries will always contain stacktraces and error messages:

```
ERROR Error in foxx route '{ "match" : "/failed", "methods" : [ "get" ] }': 'Unhandled Error', Stacktrace: Error: Unhandled Error
ERROR   at fail (js/apps/_db/_system/example/APP/app.js:279:13)
ERROR   at js/apps/_db/_system/example/APP/app.js:284:5
ERROR   at Object.res.action.callback (./js/server/modules/org/arangodb/foxx/internals.js:108:5)
ERROR   at ./js/server/modules/org/arangodb/foxx/routing.js:346:19
ERROR   at execute (./js/server/modules/org/arangodb/actions.js:1291:7)
ERROR   at next (./js/server/modules/org/arangodb/actions.js:1308:7)
ERROR   at [object Object]:386:5
ERROR   at execute (./js/server/modules/org/arangodb/actions.js:1291:7)
ERROR   at routeRequest (./js/server/modules/org/arangodb/actions.js:1312:3)
ERROR   at foxxRouting (./js/server/modules/org/arangodb/actions.js:1082:7)
ERROR   at execute (./js/server/modules/org/arangodb/actions.js:1291:7)
ERROR   at Object.routeRequest (./js/server/modules/org/arangodb/actions.js:1312:3)
ERROR   at Function.actions.defineHttp.callback (js/actions/api-system.js:51:15)
ERROR
```
