A Hello World Example for JSON
------------------------------

If you change the example slightly, then a JSON object will be delivered.

    @startDocuBlockInline JSON_01_routingCreateJsonHelloWorld
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_01_routingCreateJsonHelloWorld}
    |db._routing.save({ 
    |  url: "/hello/json", 
    |  content: { 
    |  contentType: "application/json", 
    |    body: '{"hello" : "world"}'
    |  }
    });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_01_routingCreateJsonHelloWorld

Again check with your browser or cURL http://localhost:8529/hello/json

Depending on your browser and installed add-ons you will either see the JSON
object or a download dialog. If your browser wants to open an external
application to display the JSON object, you can change the *contentType* to
*"text/plain"* for the example. This makes it easier to check the example using
a browser. Or use *curl* to access the server.

    @startDocuBlockInline JSON_02_routingCurlJsonHelloWorld
    @EXAMPLE_ARANGOSH_RUN{JSON_02_routingCurlJsonHelloWorld}
    var url = "/hello/json";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_02_routingCurlJsonHelloWorld

    @startDocuBlockInline JSON_03_routingCleanupJsonHelloWorld
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_03_routingCleanupJsonHelloWorld}
    ~db._query("FOR route IN _routing FILTER route.url == '/hello/json' REMOVE route in _routing")
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_03_routingCleanupJsonHelloWorld

Delivering Content
------------------

There are a lot of different ways on how to deliver content. We have already
seen the simplest one, where static content is delivered. The fun, however,
starts when delivering dynamic content.

### Static Content

You can specify a body and a content-type.

    @startDocuBlockInline JSON_05a_routingCreateContentTypeHelloWorld
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_05a_routingCreateContentTypeHelloWorld}
    |db._routing.save({
    |  url: "/hello/contentType",
    |  content: {
    |    contentType: "text/html",
    |    body: "<html><body>Hello World</body></html>"
    |  }
    });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_05a_routingCreateContentTypeHelloWorld

    @startDocuBlockInline JSON_05b_routingCurlContentTypeHelloWorld
    @EXAMPLE_ARANGOSH_RUN{JSON_05b_routingCurlContentTypeHelloWorld}
    var url = "/hello/contentType";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logRawResponse(response);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_05b_routingCurlContentTypeHelloWorld

    @startDocuBlockInline JSON_05c_routingCleanupContentTypeHelloWorld
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_05c_routingCleanupContentTypeHelloWorld}
    ~db._query("FOR route IN _routing FILTER route.url == '/hello/contentType' REMOVE route in _routing")
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_05c_routingCleanupContentTypeHelloWorld

If the content type is *text/plain* then you can use the short-cut

```js
{ 
  content: "Hello World" 
}
```

### A Simple Action

The simplest dynamic action is:

```js
{ 
  action: { 
    do: "@arangodb/actions/echoRequest" 
  } 
}
```
It is not advisable to store functions directly in the routing table. It is
better to call functions defined in modules. In the above example the function
can be accessed from JavaScript as:

```js
require("@arangodb/actions").echoRequest
```

The function *echoRequest* is pre-defined. It takes the request objects and
echos it in the response.

The signature of such a function must be

```js
function (req, res, options, next)
```

*Examples*

    @startDocuBlockInline JSON_06_routingCreateHelloEcho
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_06_routingCreateHelloEcho}
    |db._routing.save({ 
    |    url: "/hello/echo",
    |    action: { 
    |    do: "@arangodb/actions/echoRequest" 
    |  } 
    });
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_06_routingCreateHelloEcho

Reload the routing and check http:// 127.0.0.1:8529/hello/echo

You should see something like

    @startDocuBlockInline JSON_07_fetchroutingCreateHelloEcho
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_07_fetchroutingCreateHelloEcho}
    arango.GET("/hello/echo")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_07_fetchroutingCreateHelloEcho

    @startDocuBlockInline JSON_08_routingCleanupHelloEcho
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_08_routingCleanupHelloEcho}
    ~db._query("FOR route IN _routing FILTER route.url == '/hello/echo' REMOVE route in _routing")
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_08_routingCleanupHelloEcho

The request might contain *path*, *prefix*, *suffix*, and *urlParameters*
attributes.  *path* is the complete path as supplied by the user and always
available.  If a prefix was matched, then this prefix is stored in the attribute
*prefix* and the remaining URL parts are stored as an array in *suffix*.  If one
or more parameters were matched, then the parameter values are stored in
*urlParameters*.

For example, if the url description is

```js
{ 
  url: { 
    match: "/hello/:name/:action" 
  } 
}
```

and you request the path */hello/emil/jump*, then the request object
will contain the following attribute

```js
urlParameters: { 
  name: "emil", 
  action: "jump" 
} 
```

### Action Controller

As an alternative to the simple action, you can use controllers. A controller is
a module, defines the function *get*, *put*, *post*, *delete*, *head*,
*patch*. If a request of the corresponding type is matched, the function will be
called.

*Examples*

    @startDocuBlockInline JSON_09_routingCreateEchoController
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_09_routingCreateEchoController}
    |db._routing.save({ 
    |  url: "/hello/echo",
    |  action: { 
    |    controller: "@arangodb/actions/echoController" 
    |  } 
    });
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_09_routingCreateEchoController

Reload the routing and check http:// 127.0.0.1:8529/hello/echo:

    @startDocuBlockInline JSON_10_fetchroutingCreateEchoController
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_10_fetchroutingCreateEchoController}
    arango.GET("/hello/echo")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_10_fetchroutingCreateEchoController

    @startDocuBlockInline JSON_11_routingCleanupEchoController
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_11_routingCleanupEchoController}
    ~db._query("FOR route IN _routing FILTER route.url == '/hello/echo' REMOVE route in _routing")
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_11_routingCleanupEchoController


### Prefix Action Controller

The controller is selected when the definition is read. There is a more
flexible, but slower and maybe insecure variant, the prefix controller.

Assume that the url is a prefix match

```js
{ 
  url: { 
    match: /hello/*" 
  } 
}
```

You can use

```js
{ 
  action: { 
    prefixController: "@arangodb/actions" 
  } 
}
```

to define a prefix controller. If the URL */hello/echoController* is given, then
the module *@arangodb/actions/echoController* is used.

If you use a prefix controller, you should make certain that no unwanted actions
are available under the prefix.

The definition

```js
{ 
  action: "@arangodb/actions" 
}
```
is a short-cut for a prefix controller definition.

### Function Action

You can also store a function directly in the routing table.

*Examples*

    @startDocuBlockInline JSON_12a_routingCreateEchoFunction
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_12a_routingCreateEchoFunction}
    |db._routing.save({ 
    |  url: "/hello/echo",
    |  action: { 
    |    callback: "function(req,res) {res.statusCode=200; res.body='Hello'}" 
    |  } 
    });
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_12a_routingCreateEchoFunction

    @startDocuBlockInline JSON_12b_fetchroutingEchoFunction
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_12b_fetchroutingEchoFunction}
    arango.GET("hello/echo")
    db._query("FOR route IN _routing FILTER route.url == '/hello/echo' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_12b_fetchroutingEchoFunction

### Requests and Responses

The controller must define handler functions which take a request object and
fill the response object.

A very simple example is the function *echoRequest* defined in the module
*@arangodb/actions*.

```js
function (req, res, options, next) {
  var result;

  result = { request: req, options: options };

  res.responseCode = exports.HTTP_OK;
  res.contentType = "application/json";
  res.body = JSON.stringify(result);
}
```

Install it via:

    @startDocuBlockInline JSON_13_routingCreateEchoAction
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_13_routingCreateEchoAction}
    |db._routing.save({ 
    |  url: "/echo",
    |  action: { 
    |    do: "@arangodb/actions/echoRequest" 
    |  }
    })
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_13_routingCreateEchoAction

Reload the routing and check http:// 127.0.0.1:8529/hello/echo

You should see something like

    @startDocuBlockInline JSON_14_fetchroutingRequestHelloEcho
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_14_fetchroutingRequestHelloEcho}
    arango.GET("/hello/echo")
    db._query("FOR route IN _routing FILTER route.url == '/hello/echo' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_14_fetchroutingRequestHelloEcho

You may also pass options to the called function:

    @startDocuBlockInline JSON_15_routingCreateEchoRequestOptions
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_15_routingCreateEchoRequestOptions}
    |db._routing.save({ 
    |  url: "/echo",
    |  action: {
    |    do: "@arangodb/actions/echoRequest",
    |    options: { 
    |      "Hello": "World" 
    |    }
    |  } 
    });
    ~require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_15_routingCreateEchoRequestOptions

You now see the options in the result:

    @startDocuBlockInline JSON_16_fetchroutingEchoRequestOptions
    @EXAMPLE_ARANGOSH_OUTPUT{JSON_16_fetchroutingEchoRequestOptions}
    arango.GET("/echo")
    db._query("FOR route IN _routing FILTER route.url == '/echo' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock JSON_16_fetchroutingEchoRequestOptions
