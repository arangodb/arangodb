Application Production Mode
===========================

This chapter describes the production mode for Foxx applications.
This is the default mode for all newly installed apps and should always be used in a production environment.

Activation
----------

Activating the production mode is done with a single command:

```
unix> foxx-manager production /example
Activated production mode for Application hello-foxx version 1.5.0 on mount point /example
```

Now the app will not be listed in **listDevelopment**:

```
unix> foxx-manager listDevelopment
Mount       Name          Author          Description                                 Version    Development
---------   -----------   -------------   -----------------------------------------   --------   ------------
---------   -----------   -------------   -----------------------------------------   --------   ------------
0 application(s) found
```
Effects
-------

For a Foxx application in production mode the following effects apply:

**Application Caching**
The first time an application is requested it's routing is computed and cached.
It is guaranteed that subsequent requests to this application will get the cached routing.
That implies that changing the sources of an app will not have any effect.
It also guarantees that requests will be answered at the highest possible speed.
The cache might be flushed whenever ArangoDB is restarted or an application in development mode is requested.

**Only internal Debugging information**
Whenever the application is requested and an unhandled error occurs only a generic error message will be returned.
Stack traces are never delivered via the public API.
For more information see the [Debugging](Debugging.md) section.


Considerations for production environments
------------------------------------------
So you have created your server side application utilizing Foxx services as their backend. 
To get optimal performance you may want to implement [an HTTP connection pool using keepalive](../../GeneralHttp/README.md#http-keep-alive).

You may even consider to implement [non blocking HTTP requests](../../GeneralHttp/README.md#blocking-vs-non-blocking-http-requests) to save resources on your connection pool.
