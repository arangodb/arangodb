Modifying Request and Response
------------------------------

As we've seen in the previous examples, actions get called with the request and
response objects (named *req* and *res* in the examples) passed as parameters to
their handler functions.

The *req* object contains the incoming HTTP request, which might or might not
have been modified by a previous action (if actions were chained).

A handler can modify the request object in place if desired. This might be
useful when writing middleware (see below) that is used to intercept incoming
requests, modify them and pass them to the actual handlers.

While modifying the request object might not be that relevant for non-middleware
actions, modifying the response object definitely is. Modifying the response
object is an action's only way to return data to the caller of the action.

We've already seen how to set the HTTP status code, the content type, and the
result body. The *res* object has the following properties for these:

- *contentType*: MIME type of the body as defined in the HTTP standard (e.g.
  *text/html*, *text/plain*, *application/json*, ...)
- *responsecode*: the HTTP status code of the response as defined in the HTTP
  standard. Common values for actions that succeed are *200* or *201*.
  Please refer to the HTTP standard for more information.
- *body*: the actual response data

To set or modify arbitrary headers of the response object, the *headers*
property can be used. For example, to add a user-defined header to the response,
the following code will do:

```js
res.headers = res.headers || { }; // headers might or might not be present
res.headers['X-Test'] = 'someValue'; // set header X-Test to "someValue"
```

This will set the additional HTTP header *X-Test* to value *someValue*.  Other
headers can be set as well. Note that ArangoDB might change the case of the
header names to lower case when assembling the overall response that is sent to
the caller.

It is not necessary to explicitly set a *Content-Length* header for the response
as ArangoDB will calculate the content length automatically and add this header
itself. ArangoDB might also add a *Connection* header itself to handle HTTP
keep-alive.

ArangoDB also supports automatic transformation of the body data to another
format. Currently, the only supported transformations are base64-encoding and
base64-decoding. Using the transformations, an action can create a base64
encoded body and still let ArangoDB send the non-encoded version, for example:

```js
res.body = 'VGhpcyBpcyBhIHRlc3Q=';
res.transformations = res.transformations || [ ]; // initialize
res.transformations.push('base64decode'); // will base64 decode the response body
```

When ArangoDB processes the response, it will base64-decode what's in *res.body*
and set the HTTP header *Content-Encoding: binary*. The opposite can be achieved
with the *base64encode* transformation: ArangoDB will then automatically
base64-encode the body and set a *Content-Encoding: base64* HTTP header.

Writing dynamic action handlers
-------------------------------

To write your own dynamic action handlers, you must put them into modules.

Modules are a means of organizing action handlers and making them loadable under
specific names.

To start, we'll define a simple action handler in a module */ownTest*:

    @startDocuBlockInline MOD_01a_routingCreateOwnTest
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_01a_routingCreateOwnTest}
    |db._modules.save({
    |  path: "/db:/ownTest",
    |  content:
    |     "exports.do = function(req, res, options, next) {"+
    |     "  res.body = 'test';" +
    |     "  res.responseCode = 200;" +
    |     "  res.contentType = 'text/plain';" +
    |     "};"
    });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_01a_routingCreateOwnTest

This does nothing but register a do action handler in a module */ownTest*.  The
action handler is not yet callable, but must be mapped to a route first.  To map
the action to the route */ourtest*, execute the following command:

    @startDocuBlockInline MOD_01b_routingEnableOwnTest
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_01b_routingEnableOwnTest}
    |db._routing.save({
    |  url: "/ourtest", 
    |  action: {
    |    controller: "db://ownTest"
    |  }
    });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_01b_routingEnableOwnTest

Now use the browser or cURL and access http://localhost:8529/ourtest :

    @startDocuBlockInline MOD_01c_routingCurlOwnTest
    @EXAMPLE_ARANGOSH_RUN{MOD_01c_routingCurlOwnTest}
    var url = "/ourtest";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    assert(response.body === 'test');
    logRawResponse(response);
    db._query("FOR route IN _routing FILTER route.url == '/ourtest' REMOVE route in _routing")
    db._query("FOR module IN _modules FILTER module.path == '/db:/ownTest' REMOVE module in _modules")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_RUN
    @endDocuBlock MOD_01c_routingCurlOwnTest


You will see that the module's do function has been executed.

A Word about Caching
--------------------

Sometimes it might seem that your change do not take effect. In this case the
culprit could be the routing caches:

The routing cache stores the routing information computed from the *_routing*
collection. Whenever you change this collection manually, you need to call

    @startDocuBlockInline MOD_05_routingModifyReload
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_05_routingModifyReload}
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_05_routingModifyReload

in order to rebuild the cache.


Advanced Usages
---------------

For detailed information see the reference manual.

### Redirects

Use the following for a permanent redirect:

    @startDocuBlockInline MOD_06a_routingRedirect
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_06a_routingRedirect}
    | db._routing.save({
    |  url: "/redirectMe",
    |  action: {
    |    do: "@arangodb/actions/redirectRequest",
    |    options: {
    |      permanently: true,
    |      destination: "/somewhere.else/"
    |    }
    |  }
    });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_06a_routingRedirect

    @startDocuBlockInline MOD_06b_routingCurlRedirect
    @EXAMPLE_ARANGOSH_RUN{MOD_06b_routingCurlRedirect}
    var url = "/redirectMe";
    var response = logCurlRequest('GET', url);
    assert(response.code === 301);
    logHtmlResponse(response);
    db._query("FOR route IN _routing FILTER route.url == '/redirectMe' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_RUN
    @endDocuBlock MOD_06b_routingCurlRedirect

### Routing Bundles

Instead of adding all routes for package separately, you can
specify a bundle:

    @startDocuBlockInline MOD_07a_routingMulti
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_07a_routingMulti}
    | db._routing.save({
    |  routes: [
    |    {
    |      url: "/url1",
    |      content: "route 1"
    |    },
    |    {
    |      url: "/url2",
    |      content: "route 2"
    |    },
    |    {
    |      url: "/url3",
    |      content: "route 3"
    |    }
    |  ]
      });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_07a_routingMulti


    @startDocuBlockInline MOD_07b_routingCurlMulti
    @EXAMPLE_ARANGOSH_RUN{MOD_07b_routingCurlMulti}
    var url = ["/url1", "/url2", "/url3"];
    var reply = ["route 1", "route 2", "route 3"]
    for (var i = 1; i < 3; i++) {
      var response = logCurlRequest('GET', url[i]);
      assert(response.code === 200);
      assert(response.body === reply[i])
      logRawResponse(response);
    }
    db._query("FOR route IN _routing FILTER route.routes[0].url == '/url1' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_RUN
    @endDocuBlock MOD_07b_routingCurlMulti

The advantage is, that you can put all your routes into one document
and use a common prefix.

    @startDocuBlockInline MOD_07c_routingMulti
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_07c_routingMulti}
    | db._routing.save({
    |  urlPrefix: "/test",
    |  routes: [
    |    {
    |      url: "/url1",
    |      content: "route 1"
    |    },
    |    {
    |      url: "/url2",
    |      content: "route 2"
    |    },
    |    {
    |      url: "/url3",
    |      content: "route 3"
    |    }
    |  ]
      });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_07c_routingMulti

will define the URL */test/url1*, */test/url2*, and */test/url3*:

    @startDocuBlockInline MOD_07d_routingCurlMulti
    @EXAMPLE_ARANGOSH_RUN{MOD_07d_routingCurlMulti}
    var url = ["/test/url1", "/test/url2", "/test/url3"];
    var reply = ["route 1", "route 2", "route 3"]
    for (var i = 0; i < 3; i++) {
      var response = logCurlRequest('GET', url[i]);
      assert(response.code === 200);
      assert(response.body === reply[i])
      logRawResponse(response);
    }
    db._query("FOR route IN _routing FILTER route.routes[0].url == '/url1' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_RUN
    @endDocuBlock MOD_07d_routingCurlMulti

### Writing Middleware

Assume, you want to log every request in your namespace to the console. *(if ArangoDB is running
as a daemon, this will end up in the logfile)*. In this case you can easily define an
action for the URL */subdirectory*. This action simply logs
the requests, calls the next in line, and logs the response:

    @startDocuBlockInline MOD_08a_routingCreateOwnConsoleLog
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_08a_routingCreateOwnConsoleLog}
    |db._modules.save({
    |  path: "/db:/OwnMiddlewareTest",
    |  content:
    |     "exports.logRequest = function (req, res, options, next) {" +
    |     "    console = require('console'); " + 
    |     "    console.log('received request: %s', JSON.stringify(req));" +
    |     "    next();" +
    |     "    console.log('produced response: %s', JSON.stringify(res));" +
    |     "};"
    });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_08a_routingCreateOwnConsoleLog

This function will now be available as *db://OwnMiddlewareTest/logRequest*. You need to
tell ArangoDB that it is should use a prefix match and that the shortest match
should win in this case:

    @startDocuBlockInline MOD_08b_routingCreateRouteToOwnConsoleLog
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_08b_routingCreateRouteToOwnConsoleLog}
    |db._routing.save({
    |  middleware: [
    |    {
    |      url: {
    |        match: "/subdirectory/*"
    |      },
    |      action: {
    |        do: "db://OwnMiddlewareTest/logRequest"
    |      }
    |    }
    |  ]
    });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_08b_routingCreateRouteToOwnConsoleLog

When you call *next()* in that action, the next specific routing will
be used for the original URL. Even if you modify the URL in the request
object *req*, this will not cause the *next()* to jump to the routing
defined for this next URL. If proceeds occurring the origin URL. However,
if you use *next(true)*, the routing will stop and request handling is
started with the new URL. You must ensure that *next(true)* is never
called without modifying the URL in the request object
*req*. Otherwise an endless loop will occur.

Now we add some other simple routings to test all this:

    @startDocuBlockInline MOD_08c_routingCreateRouteToOwnConsoleLog
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_08c_routingCreateRouteToOwnConsoleLog}
    |db._routing.save({
    |    url: "/subdirectory/ourtest/1",
    |    action: {
    |      do: "@arangodb/actions/echoRequest"
    |    }
    });
    |db._routing.save({
    |    url: "/subdirectory/ourtest/2",
    |    action: {
    |      do: "@arangodb/actions/echoRequest"
    |    }
    });
    |db._routing.save({
    |    url: "/subdirectory/ourtest/3",
    |    action: {
    |      do: "@arangodb/actions/echoRequest"
    |    }
    });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_08c_routingCreateRouteToOwnConsoleLog

Then we send some curl requests to these sample routes:

    @startDocuBlockInline MOD_08d_routingCurlToOwnConsoleLog
    @EXAMPLE_ARANGOSH_RUN{MOD_08d_routingCurlToOwnConsoleLog}
    var url = ["/subdirectory/ourtest/1",
               "/subdirectory/ourtest/2",
               "/subdirectory/ourtest/3"];
    for (var i = 0; i < 3; i++) {
      var response = logCurlRequest('GET', url[i]);
      assert(response.code === 200);
      logJsonResponse(response);
    }
    db._query("FOR route IN _routing FILTER route.middleware[0].url.match == '/subdirectory/*' REMOVE route in _routing");
    db._query("FOR route IN _routing FILTER route.url == '/subdirectory/ourtest/1' REMOVE route in _routing");
    db._query("FOR route IN _routing FILTER route.url == '/subdirectory/ourtest/2' REMOVE route in _routing");
    db._query("FOR route IN _routing FILTER route.url == '/subdirectory/ourtest/3' REMOVE route in _routing");
    db._query("FOR module IN _modules FILTER module.path == '/db:/OwnMiddlewareTest' REMOVE module in _modules");
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_RUN
    @endDocuBlock MOD_08d_routingCurlToOwnConsoleLog

and the console (and / or the logfile) will show requests and replies.
*Note that logging doesn't warrant the sequence in which these lines
will appear.*

Application Deployment
----------------------

Using single routes or [bundles](#routing-bundles) can be
become a bit messy in large applications. Kaerus has written a [deployment tool](https://github.com/kaerus/arangodep) in node.js.

Note that there is also [Foxx](../../../Foxx/README.md) for building applications
with ArangoDB.

Common Pitfalls when using Actions
----------------------------------

### Caching

If you made any changes to the routing but the changes does not have any effect
when calling the modified actions URL, you might have been hit by some
caching issues.

After any modification to the routing or actions, it is thus recommended to
make the changes "live" by calling the following functions from within arangosh:

    @startDocuBlockInline MOD_09_routingReload
    @EXAMPLE_ARANGOSH_RUN{MOD_09_routingReload}
    require("internal").reloadRouting();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_09_routingReload

You might also be affected by client-side caching.
Browsers tend to cache content and also redirection URLs. You might need to
clear or disable the browser cache in some cases to see your changes in effect.

### Data types

When processing the request data in an action, please be aware that the data
type of all query parameters is *string*. This is because the whole URL is a
string and when the individual parts are extracted, they will also be strings.

For example, when calling the URL http:// localhost:8529/hello/world?value=5

the parameter *value* will have a value of (string) *5*, not (number) *5*.
This might be troublesome if you use JavaScript's *===* operator when checking
request parameter values.

The same problem occurs with incoming HTTP headers. When sending the following
header from a client to ArangoDB

    X-My-Value: 5

then the header *X-My-Value* will have a value of (string) *5* and not (number) *5*.

### 404 Not Found

If you defined a URL in the routing and the URL is accessible fine via
HTTP *GET* but returns an HTTP 501 (not implemented) for other HTTP methods
such as *POST*, *PUT* or *DELETE*, then you might have been hit by some
defaults.

By default, URLs defined like this (simple string *url* attribute) are
accessible via HTTP *GET* and *HEAD* only. To make such URLs accessible via
other HTTP methods, extend the URL definition with the *methods* attribute.

For example, this definition only allows access via *GET* and *HEAD*:

```js
{
  url: "/hello/world"
}
```

whereas this definition allows HTTP *GET*, *POST*, and *PUT*:

    @startDocuBlockInline MOD_09a_routingSpecifyMethods
    @EXAMPLE_ARANGOSH_OUTPUT{MOD_09a_routingSpecifyMethods}
    |db._routing.save({
    |    url: {
    |      match: "/hello/world",
    |      methods: [ "get", "post", "put" ]
    |    },
    |    action: {
    |      do: "@arangodb/actions/echoRequest"
    |    }
    });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MOD_09a_routingSpecifyMethods

    @startDocuBlockInline MOD_09b_routingCurlSpecifyMethods
    @EXAMPLE_ARANGOSH_RUN{MOD_09b_routingCurlSpecifyMethods}
    var url = "/hello/world"
    var postContent = "{hello: 'world'}";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);

    var response = logCurlRequest('POST', url, postContent);
    assert(response.code === 200);
    logJsonResponse(response);

    var response = logCurlRequest('PUT', url, postContent);
    assert(response.code === 200);
    logJsonResponse(response);

    var response = logCurlRequest('DELETE', url);
    assert(response.code === 404);
    logJsonResponse(response);

    db._query("FOR route IN _routing FILTER route.url.match == '/hello/world' REMOVE route in _routing");
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_RUN
    @endDocuBlock MOD_09b_routingCurlSpecifyMethods

The former definition (defining *url* as an object with a *match* attribute)
will result in the URL being accessible via all supported HTTP methods (e.g.
*GET*, *POST*, *PUT*, *DELETE*, ...), whereas the latter definition (providing a string
*url* attribute) will result in the URL being accessible via HTTP *GET* and
HTTP *HEAD* only, with all other HTTP methods being disabled. Calling a URL
with an unsupported or disabled HTTP method will result in an HTTP 404 error.
