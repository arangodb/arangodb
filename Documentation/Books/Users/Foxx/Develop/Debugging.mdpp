!CHAPTER Available Debugging mechanisms

We are talking about the development mode for Foxx.
Hence one of the most important parts will be debugging of your Foxx.
We have several mechanisms available to simplify this task.
During Foxx development we assume the following situation:
You have installed a syntactically valid version of a Foxx and want to develop it further.
You have activated the development mode for this route:

```
unix>foxx-manager development /example
Activated development mode for Application hello-foxx version 1.5.0 on mount point /example
```

!SECTION Errors during install

Now you apply changes to the source code of Foxx.
In development mode it is possible that you create a Foxx that could not be installed regularly.
If this is the case and you request any route of it you should receive a detailed error information:

```
unix>curl -X GET --dump - http://localhost:8529/_db/_system/example/failed
HTTP/1.1 500 Internal Error
Server: ArangoDB
Connection: Keep-Alive
Content-Type: application/json; charset=utf-8
Content-Length: 554

{"exception":"Error: App not found","stacktrace":["Error: App not found","  at Object.lookupApp (./js/server/modules/org/arangodb/foxx/manager.js:99:13)","  at foxxRouting (./js/server/modules/org/arangodb/actions.js:1040:27)","  at execute (./js/server/modules/org/arangodb/actions.js:1291:7)","  at Object.routeRequest (./js/server/modules/org/arangodb/actions.js:1312:3)","  at Function.actions.defineHttp.callback (js/actions/api-system.js:51:15)",""],"error":true,"code":500,"errorNum":500,"errorMessage":"failed to load foxx mounted at '/example'"}
```

!SECTION Errors in routes

If you have created a Foxx that can be regularly installed but has an unhandled error inside a route.
Triggering this route and entering the error case will return the specific error including a stack trace for you to hunt it down:

```
unix>curl -X GET http://localhost:8529/_db/_system/example/failed
HTTP/1.1 500 Internal Error
Server: ArangoDB
Connection: Keep-Alive
Content-Type: application/json
Content-Length: 917

{"error":"Unhandled Error","stack":"Error: Unhandled Error\n  at fail (js/apps/_db/_system/example/APP/app.js:279:13)\n  at js/apps/_db/_system/example/APP/app.js:284:5\n  at Object.res.action.callback (./js/server/modules/org/arangodb/foxx/internals.js:108:5)\n  at ./js/server/modules/org/arangodb/foxx/routing.js:346:19\n  at execute (./js/server/modules/org/arangodb/actions.js:1291:7)\n  at next (./js/server/modules/org/arangodb/actions.js:1308:7)\n  at [object Object]:386:5\n  at execute (./js/server/modules/org/arangodb/actions.js:1291:7)\n  at routeRequest (./js/server/modules/org/arangodb/actions.js:1312:3)\n  at foxxRouting (./js/server/modules/org/arangodb/actions.js:1082:7)\n  at execute (./js/server/modules/org/arangodb/actions.js:1291:7)\n  at Object.routeRequest (./js/server/modules/org/arangodb/actions.js:1312:3)\n  at Function.actions.defineHttp.callback (js/actions/api-system.js:51:15)\n"}
```

!SECTION Errors in logs

Independent of the errors presented in the routes on requests Fox will always log errors to the log-file if caught by the default error handlers.
The log entries will always contain stacktraces and error messages:

```
INFO /example, incoming request from 127.0.0.1: GET http://0.0.0.0:8529/example/failed
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
INFO /example, outgoing response with status 500 of type application/json, body length: 917
```
