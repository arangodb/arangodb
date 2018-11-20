A Hello World Example for JSON
==============================

If you change the example slightly, then a JSON object will be delivered.

```js
db._routing.save({ 
  url: "/hello/json", 
  content: { 
  contentType: "application/json", 
    body: '{"hello" : "world"}'
  }
});
require("internal").reloadRouting()
```

Again check with your browser or cURL `http://localhost:8529/hello/json`

Depending on your browser and installed add-ons you will either see the JSON
object or a download dialog. If your browser wants to open an external
application to display the JSON object, you can change the *contentType* to
*"text/plain"* for the example. This makes it easier to check the example using
a browser. Or use *curl* to access the server.

<!--
var url = "/hello/json";
var response = logCurlRequest('GET', url);
assert(response.code === 200);
logJsonResponse(response);
-->

```
shell> curl --dump - http://localhost:8529/hello/json

HTTP/1.1 200 OK
content-type: application/json; charset=utf-8
x-content-type-options: nosniff

{ 
  "hello" : "world" 
}
```

Delivering Content
------------------

There are a lot of different ways on how to deliver content. We have already
seen the simplest one, where static content is delivered. The fun, however,
starts when delivering dynamic content.

### Static Content

You can specify a body and a content-type.

```js
db._routing.save({
  url: "/hello/contentType",
  content: {
    contentType: "text/html",
    body: "<html><body>Hello World</body></html>"
  }
});
require("internal").reloadRouting()
```

<!--
var url = "/hello/contentType";
var response = logCurlRequest('GET', url);
assert(response.code === 200);
logRawResponse(response);
-->

```
shell> curl --dump - http://localhost:8529/hello/contentType

HTTP/1.1 200 OK
content-type: text/html
x-content-type-options: nosniff

"
Hello World
"
```

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

```js
db._routing.save({ 
  url: "/hello/echo",
  action: { 
    do: "@arangodb/actions/echoRequest" 
  } 
});
require("internal").reloadRouting()
```

Reload the routing and check `http://127.0.0.1:8529/hello/echo`

You should see something like

<!--
arango.GET_RAW("/hello/echo", { "accept" : "application/json" })
-->

```js
arango.GET("/hello/echo")
```

```json
{ 
  "request" : { 
    "authorized" : true, 
    "user" : null, 
    "database" : "_system", 
    "url" : "/hello/echo", 
    "protocol" : "http", 
    "server" : { 
      "address" : "127.0.0.1", 
      "port" : 18328 
    }, 
    "client" : { 
      "address" : "127.0.0.1", 
      "port" : 36212, 
      "id" : "154003193673621" 
    }, 
    "internals" : { 
    }, 
    "headers" : { 
      "accept-encoding" : "deflate", 
      "user-agent" : "ArangoDB", 
      "host" : "127.0.0.1", 
      "authorization" : "Basic cm9vdDo=", 
      "connection" : "Keep-Alive" 
    }, 
    "requestType" : "GET", 
    "parameters" : { 
    }, 
    "cookies" : { 
    }, 
    "urlParameters" : { 
    } 
  }, 
  "options" : { 
  } 
}
```

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

```js
db._routing.save({ 
  url: "/hello/echo",
  action: { 
    controller: "@arangodb/actions/echoController" 
  } 
});
require("internal").reloadRouting()
```

Reload the routing and check `http://127.0.0.1:8529/hello/echo`

<!--
arango.GET_RAW("/hello/echo", { "accept" : "application/json" })
-->

```js
arango.GET("/hello/echo")
```

```json
{ 
  "request" : { 
    "authorized" : true, 
    "user" : null, 
    "database" : "_system", 
    "url" : "/hello/echo", 
    "protocol" : "http", 
    "server" : { 
      "address" : "127.0.0.1", 
      "port" : 18328 
    }, 
    "client" : { 
      "address" : "127.0.0.1", 
      "port" : 36212, 
      "id" : "154003193673621" 
    }, 
    "internals" : { 
    }, 
    "headers" : { 
      "accept-encoding" : "deflate", 
      "user-agent" : "ArangoDB", 
      "host" : "127.0.0.1", 
      "authorization" : "Basic cm9vdDo=", 
      "connection" : "Keep-Alive" 
    }, 
    "requestType" : "GET", 
    "parameters" : { 
    }, 
    "cookies" : { 
    }, 
    "urlParameters" : { 
    } 
  }, 
  "options" : { 
  } 
}
```

### Prefix Action Controller

The controller is selected when the definition is read. There is a more
flexible, but slower and maybe insecure variant, the prefix controller.

Assume that the url is a prefix match

```js
{ 
  url: { 
    match: "/hello/*" 
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

```js
db._routing.save({ 
  url: "/hello/echo",
  action: { 
    callback: "function(req,res) {res.statusCode=200; res.body='Hello'}" 
  } 
});
require("internal").reloadRouting()
```

<!--
arango.GET_RAW("hello/echo", { "accept" : "application/json" })
-->

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

```js
db._routing.save({ 
  url: "/echo",
  action: { 
    do: "@arangodb/actions/echoRequest" 
  }
})
require("internal").reloadRouting()
```

Reload the routing and check `http://127.0.0.1:8529/hello/echo`

You should see something like

<!--
arango.GET_RAW("/hello/echo", { "accept" : "application/json" })
-->

```js
arango.GET("/hello/echo")
```

```json
{ 
  "error" : true, 
  "code" : 404, 
  "errorNum" : 404, 
  "errorMessage" : "unknown path '/hello/echo'" 
}
```

You may also pass options to the called function:

```js
db._routing.save({ 
  url: "/echo",
  action: {
    do: "@arangodb/actions/echoRequest",
    options: { 
      "Hello": "World" 
    }
  } 
});
require("internal").reloadRouting()
```

You now see the options in the result:

<!--
arango.GET_RAW("/echo", { accept: "application/json" })
-->

```js
arango.GET("/echo")
```

```json
{ 
  "request" : { 
    "authorized" : true, 
    "user" : null, 
    "database" : "_system", 
    "url" : "/echo", 
    "protocol" : "http", 
    "server" : { 
      "address" : "127.0.0.1", 
      "port" : 18328 
    }, 
    "client" : { 
      "address" : "127.0.0.1", 
      "port" : 36212, 
      "id" : "154003193673621" 
    }, 
    "internals" : { 
    }, 
    "headers" : { 
      "accept-encoding" : "deflate", 
      "user-agent" : "ArangoDB", 
      "host" : "127.0.0.1", 
      "authorization" : "Basic cm9vdDo=", 
      "connection" : "Keep-Alive" 
    }, 
    "requestType" : "GET", 
    "parameters" : { 
    }, 
    "cookies" : { 
    }, 
    "urlParameters" : { 
    } 
  }, 
  "options" : { 
    "Hello" : "World" 
  } 
}
```
