ArangoDB's Actions{#UserManualActions}
======================================

@NAVIGATE_UserManualActions
@EMBEDTOC{UserManualActionsTOC}

Introduction to User Actions{#UserManualActionsIntro}
=====================================================

In some ways the communication layer of the ArangoDB server behaves like a Web
server. Unlike a Web server, it normally responds to HTTP requests by delivering
JSON objects. Remember, documents in the database are just JSON objects. So,
most of the time the HTTP response will contain a JSON document from the
database as body. You can extract the documents stored in the database using
HTTP `GET`. You can store documents using HTTP `POST`.

However, there is something more. You can write small sniplets - so called
actions - to extend the database. The idea of actions is that sometimes it is
better to store parts of the business logic within AnrangoDB.

The simplest example is the age of a person. Assume you store information about
people in your database. It is an anti-pattern to store the age, because it
changes every now and then. Therefore, you normally store the birthday and let
the client decide what to do with it. However, if you have many different
clients, it might be easier to enrich the person document with the age using
actions once on the server side.

Or, for instance, if you want to apply some statistics to large data-sets and
you cannot easily express this as query. You can define a action instead of
transferring the whole data to the client and do the computation on the client.

Actions are also useful if you want to restrict and filter data according to
some complex permission system.

The ArangoDB server can deliver all kinds of information, JSON being only one
possible format. You can also generate HTML or images. However, a Web server is
normally better suited for the task as it also implements various caching
strategies, language selection, compression and so on. Having said that, there
are still situations where it might be suitable to use the ArangoDB to deliver
HTML pages - static or dynamic. A simple example is the built-in administration
interface. You can access it using any modern browser and there is no need for a
separate Apache or IIS.

The following sections will explain actions within ArangoDB and show how to
define them. The examples start with delivering static HTML pages - even if this
is not the primary use-case for actions. The later sections will then show you
how to code some pieces of your business logic and return JSON objects.

The interface is loosely modelled after the JavaScript classes for HTTP request
and responses found in node.js and the middleware/routing aspects of connect.js
and express.js.

Note that unlike node.js, ArangoDB is multi-threaded and there is no easy way to
share state between queries inside the JavaScript engine. If such state
information is required, you need to use the database itself.

A Hello World Example{#UserManualActionsHelloWorld}
===================================================

The client API or browser sends a HTTP request to the ArangoDB server and the
server returns a HTTP response to the client. A HTTP request consists of a
method, normally `GET` or `POST` when using a browser, and a request path like
`/hello/world`. For a real Web server there are a zillion of other thing to
consider, we will ignore this for the moment. The HTTP response contains a
content type, describing how to interpret the returned data, and the data
itself.

In the following example, we want to define an action in ArangoDB, so that the
server returns the HTML document

    <html>
     <body>
      Hello World
     </body>
    </html>

if asked `GET /hello/world`.

The server needs to know what function to call or what document to deliver if it
receives a request. This is called routing. All the routing information of
ArangoDB is stored in a collection `_routing`. Each entry in this collections
describes how to deal with a particular request path.

For the above example, add the following document to the @{_routing} collection:

    arangosh> db._routing.save({ 
    ........>   url: { match: "/hello/world" },
    ........>   content: { 
    ........>     contentType: "text/html", 
    ........>     body: "<html><body>Hello World</body></html>" }});

In order to activate the new routing, you must either restart the server or call
the internal reload function.

    arangosh> require("internal").reloadRouting()

Now use the browser and access

    http://localhost:8529/hello/world

You should see the `Hello World` in our browser.

Matching a URL{#UserManualActionsMatches}
=========================================

There are a lot of options for the `url` attribute. If you define different
routing for the same path, then the following simple rule is applied in order to
determine which match wins: If there are two matches, then the more specific
wins. I. e, if there is a wildcard match and an exact match, the exact match is
prefered. If there is a short and a long match, the longer match wins.

Exact Match{#UserManualActionsMatchesExact}
-------------------------------------------

If the definition is

    { url: { match: "/hello/world" } }

then the match must be exact. Only the request for `/hello/world` will match,
everything else, e. g. `/hello/world/my` or `/hello/world2`, will not match.

The following definition is a short-cut for an exact match.

    { url: "/hello/world" }


Please note that while the two definitions will result in the same URL
matching, there is a subtle difference between them:

The former definition (defining `url` as an object with a `match` attribute)
will result in the URL being accessible via all supported HTTP methods (e.g.
`GET`, `POST`, `PUT`, `DELETE`, ...), whereas the latter definition (providing a string
`url` attribute) will result in the URL being accessible via HTTP `GET` and 
HTTP `HEAD` only, with all other HTTP methods being disabled. Calling a URL
with an unsupported or disabled HTTP method will result in an HTTP 501 error.

Prefix Match{#UserManualActionsMatchesPrefix}
---------------------------------------------

If the definition is

    { url: { match: "/hello/world/*" } }

then the match can be a prefix match. The requests for `/hello/world`,
`/hello/world/my`, and `/hello/world/how/are/you` will all match. However
`/hello/world2` does not match. Prefix matches within a URL part,
i. e. `/hello/world*`, are not allowed. The wildcard must occur at the end,
i. e.

    /hello/*/world 

is also disallowed.

If you define two routes

    { url: { match: "/hello/world/*" } }
    { url: { match: "/hello/world/emil" } }

then the second route will be used for `/hello/world/emil` because it is more
specific.

Parameterized Match{#UserManualActionsMatchesParameterized}
-----------------------------------------------------------

A parameterized match is similar to a prefix match, but the parameters are also
allowed inside the URL path.

If the definition is

    { url: { match: "/hello/:name/world" } }

then the URL must have three parts, the first part being `hello` and the third
part `world`. For example, `/hello/emil/world` will match, while
`/hello/emil/meyer/world` will not.

Constraint Match{#UserManualActionsMatchesConstraint}
-----------------------------------------------------

A constraint match is similar to a parameterized match, but the parameters can
carry constraints.

If the definition is

    { url: { match: "/hello/:name/world", constraint: { name: "/[a-z]+/" } }

then the URL must have three parts, the first part being `hello` and the third
part `world`. The second part must be all lowercase.

It is possible to use more then one constraint for the same URL part.

    { url: { match: "/hello/:name|:id/world",
	     constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }

Optional Match{#UserManualActionsMatchesOptional}
-------------------------------------------------

An optional match is similar to a parameterized match, but the last parameter is
optional.

If the definition is

    { url: { match: "/hello/:name?", constraint: { name: "/[a-z]+/" } }

then the URL `/hello` and `/hello/emil` will match.

If the definitions are

    { url: { match: "/hello/world" } }
    { url: { match: "/hello/:name", constraint: { name: "/[a-z]+/" } }
    { url: { match: "/hello/*" } }

then the URL `/hello/world` will be matched by the first route, because it is
the most specific. The URL `/hello/you` will be matched by the second route,
because it is more specific than the prefix match.

Method Restriction{#UserManualActionsMatchesMethod}
---------------------------------------------------

You can restrict the match to specific HTTP methods.

If the definition is

    { url: { match: "/hello/world", methods: [ "post", "put" ] }

then only HTTP `POST` and `PUT` requests will match.
Calling with a different HTTP method will result in an HTTP 501 error.

Please note that if `url` is defined as a simple string, then only the
HTTP methods `GET` and `HEAD` will be allowed, an all other methods will be
disabled:

    { url: "/hello/world" }

More on Matching{#UserManualActionsMatching}
--------------------------------------------

Remember that the more specific match wins. 

- A match without parameter or wildcard is more specific than a match with
  parameters or wildcard.
- A match with parameter is more specific than a match with a wildcard.
- If there is more than one parameter, specificity is applied from left to
  right.

Consider the following definitions

    (1) { url: { match: "/hello/world" } }
    (2) { url: { match: "/hello/:name", constraint: { name: "/[a-z]+/" } }
    (3) { url: { match: "/:something/world" }
    (4) { url: { match: "/hello/*" } }

Then

- `/hello/world` is match by (1)
- `/hello/emil` is match by (2)
- `/your/world` is match by (3)
- `/hello/you` is match by (4)

You can write the following document into the `_routing` collection
to test the above examples.

    {
      routes: [
	{ url: { match: "/hello/world" }, content: "route 1" },
	{ url: { match: "/hello/:name|:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "route 2" },
	{ url: { match: "/:something/world" }, content: "route 3" },
	{ url: { match: "/hello/*" }, content: "route 4" },
      ]
    }

A Hello World Example for JSON{#UserManualActionsHelloJson}
===========================================================

If you change the example slightly, then a JSON object will be delivered.

    arangosh> db._routing.save({ 
    ........>   url: "/hello/json", 
    ........>   content: { 
    ........>     contentType: "application/json", 
    ........>     body: "{ \"hello\" : \"world\" }" }});
    arangosh> require("internal").reloadRouting()

Again check with your browser

    http://localhost:8529/hello/json

Depending on your browser and installed add-ons you will either see the JSON
object or a download dialog. If your browser wants to open an external
application to display the JSON object, you can change the `contentType` to
`"text/plain"` for the example. This makes it easier to check the example using
a browser. Or use `curl` to access the server.

    bash> curl "http://127.0.0.1:8529/hello/json" && echo
    { "hello" : "world" }

Delivering Content{#UserManualActionsContent}
=============================================

There are a lot of different ways on how to deliver content. We have already
seen the simplest one, where static content is delivered. The fun, however,
starts when delivering dynamic content.

Static Content{#UserManualActionsContentStatic}
-----------------------------------------------

You can specify a body and a content-type.

    { content: {
	contentType: "text/html",
	body: "<html><body>Hello World</body></html>"
      }
    }

If the content type is `text/plain` then you can use the short-cut

    { content: "Hello World" }

A Simple Action{#UserManualActionsContentAction}
================================================

The simplest dynamic action is:

    { action: { do: "org/arangodb/actions/echoRequest" } }

It is not advisable to store functions directly in the routing table. It is
better to call functions defined in modules. In the above example the function
can be accessed from JavaScript as:

    require("org/arangodb/actions").echoRequest

The function `echoRequest` is pre-defined. It takes the request objects and
echos it in the response.

The signature of such a function must be

    function (req, res, options, next)

For example

    arangosh> db._routing.save({ 
    ........>   url: "/hello/echo",
    ........>   action: { do: "org/arangodb/actions/echoRequest" } });

Reload the routing and check

    http://127.0.0.1:8529/hello/echo

You should see something like

    {
	"request": {
	    "path": "/hello/echo",
	    "headers": {
		"accept-encoding": "gzip, deflate",
		"accept-language": "de-de,de;q=0.8,en-us;q=0.5,en;q=0.3",
		"connection": "keep-alive",
		"content-length": "0",
		"host": "localhost:8529",
		"user-agent": "Mozilla/5.0 (X11; Linux x86_64; rv:15.0) Gecko/20100101 Firefox/15.0"
	    },
	    "requestType": "GET",
	    "parameters": { }
	},
	"options": { }
    }

The request might contain `path`, `prefix`, `suffix`, and `urlParameters`
attributes.  `path` is the complete path as supplied by the user and always
available.  If a prefix was matched, then this prefix is stored in the attribute
`prefix` and the remaining URL parts are stored as an array in `suffix`.  If one
or more parameters were matched, then the parameter values are stored in
`urlParameters`.

For example, if the url description is

    { url: { match: "/hello/:name/:action" } }

and you request the path `/hello/emil/jump`, then the request object
will contain the following attribute

    urlParameters: { name: "emil", action: "jump" } }

Action Controller{#UserManualActionsContentController}
------------------------------------------------------

As an alternative to the simple action, you can use controllers. A controller is
a module, defines the function `get`, `put`, `post`, `delete`, `head`,
`patch`. If a request of the corresponding type is matched, the function will be
called.

For example

    arangosh> db._routing.save({ 
    ........>   url: "/hello/echo",
    ........>   action: { controller: "org/arangodb/actions/echoController" } });

Prefix Action Controller{#UserManualActionsContentPrefix}
---------------------------------------------------------

The controller is selected when the definition is read. There is a more
flexible, but slower and maybe insecure variant, the prefix controller.

Assume that the url is a prefix match

    { url: { match: /hello/*" } }

You can use

    { action: { prefixController: "org/arangodb/actions" } }

to define a prefix controller. If the URL `/hello/echoController` is given, then
the module `org/arangodb/actions/echoController` is used.

If you use a prefix controller, you should make certain that no unwanted actions
are available under the prefix.

The definition

    { action: "org/arangodb/actions" }

is a short-cut for a prefix controller definition.

Function Action{#UserManualActionsFunctionAction}
-------------------------------------------------

You can also store a function directly in the routing table.

For example

    arangosh> db._routing.save({ 
    ........>   url: "/hello/echo",
    ........>   action: { callback: "function(req,res) {res.statusCode=200; res.body='Hello'}" } });

Requests and Responses{#UserManualActionsReqRes}
================================================

The controller must define handler functions which take a request object and
fill the response object.

A very simple example is the function `echoRequest` defined in the module
`org/arangodb/actions`.

    function (req, res, options, next) {
      var result;

      result = { request: req, options: options };

      res.responseCode = exports.HTTP_OK;
      res.contentType = "application/json";
      res.body = JSON.stringify(result);
    }

Install it as

    arangosh> db._routing.save({ 
    ........>   url: "/echo",
    ........>   action: { controller: "org/arangodb/actions", do: "echoRequest" } });

Reload the routing and check

    http://127.0.0.1:8529/hello/echo

You should see something like

    {
	"request": {
	    "prefix": "/hello/echo",
	    "suffix": [
		"hello",
		"echo"
	    ],
	    "path": "/hello/echo",
	    "headers": {
		"accept-encoding": "gzip, deflate",
		"accept-language": "de-de,de;q=0.8,en-us;q=0.5,en;q=0.3",
		"connection": "keep-alive",
		"content-length": "0",
		"host": "localhost:8529",
		"user-agent": "Mozilla/5.0 (X11; Linux x86_64; rv:15.0) Gecko/20100101 Firefox/15.0"
	    },
	    "requestType": "GET",
	    "parameters": { }
	},
	"options": { }
    }

You may also pass options to the called function:

    arangosh> db._routing.save({ 
    ........>   url: "/echo",
    ........>   action: {
    ........>     controller: "org/arangodb/actions",
    ........>     do: "echoRequest",
    ........>     options: { "Hello": "World" } } });

You should now see the options in the result.

    {
	"request": {
	    ...
	},
	"options": {
	    "Hello": "World"
	}
    }

Modifying Request and Response{#UserManualActionsModify}
========================================================

As we've seen in the previous examples, actions get called with the request and
response objects (named `req` and `res` in the examples) passed as parameters to
their handler functions.

The `req` object contains the incoming HTTP request, which might or might not
have been modified by a previous action (if actions were chained).

A handler can modify the request object in place if desired. This might be
useful when writing middleware (see below) that is used to intercept incoming
requests, modify them and pass them to the actual handlers.

While modifying the request object might not be that relevant for non-middleware
actions, modifying the response object definitely is. Modifying the response
object is an action's only way to return data to the caller of the action.

We've already seen how to set the HTTP status code, the content type, and the
result body. The `res` object has the following properties for these:

- contentType: MIME type of the body as defined in the HTTP standard (e.g. 
  `text/html`, `text/plain`, `application/json`, ...)
- responsecode: the HTTP status code of the response as defined in the HTTP
  standard. Common values for actions that succeed are `200` or `201`. 
  Please refer to the HTTP standard for more information.
- body: the actual response data

To set or modify arbitrary headers of the response object, the `headers`
property can be used. For example, to add a user-defined header to the response,
the following code will do:

    res.headers = res.headers || { }; // headers might or might not be present
    res.headers['X-Test'] = 'someValue'; // set header X-Test to "someValue"

This will set the additional HTTP header `X-Test` to value `someValue`.  Other
headers can be set as well. Note that ArangoDB might change the case of the
header names to lower case when assembling the overall response that is sent to
the caller.

It is not necessary to explicitly set a `Content-Length` header for the response
as ArangoDB will calculate the content length automatically and add this header
itself. ArangoDB might also add a `Connection` header itself to handle HTTP
keep-alive.

ArangoDB also supports automatic transformation of the body data to another
format. Currently, the only supported transformations are base64-encoding and
base64-decoding. Using the transformations, an action can create a base64
encoded body and still let ArangoDB send the non-encoded version, for example:

    res.body = 'VGhpcyBpcyBhIHRlc3Q=';
    res.transformations = res.transformations || [ ]; // initialise
    res.transformations.push('base64decode'); // will base64 decode the response body

When ArangoDB processes the response, it will base64-decode what's in `res.body`
and set the HTTP header `Content-Encoding: binary`. The opposite can be achieved
with the `base64encode` transformation: ArangoDB will then automatically
base64-encode the body and set a `Content-Encoding: base64` HTTP header.

Writing dynamic action handlers{#UserManualActionsHandlers}
===========================================================

To write your own dynamic action handlers, you must put them into modules.

Modules are a means of organising action handlers and making them loadable under
specific names.

To start, we'll define a simple action handler in a module `/own/test`:

    arangosh> db._modules.save({ 
    ........>   path: "/own/test",
    ........>   content: "exports.do = function(req, res, options, next) { res.body = 'test'; res.responseCode = 200; res.contentType = 'text/html'; };",
    ........>   autoload: true });

This does nothing but register a do action handler in a module `/own/test`.  The
action handler is not yet callable, but must be mapped to a route first.  To map
the action to the route `/ourtest`, execute the following command:

    arangosh> db._routing.save({ 
    ........>   url: "/ourtest",
    ........>   action: { controller: "/own/test" } }); 

In order to see the module in action, you must either restart the server or call
the internal reload function.

    arangosh> require("internal").reloadRouting()

Now use the browser and access

    http://localhost:8529/ourtest

You will see that the module's do function has been executed.

A Word about Caching{#UserManualActionsCache}
=============================================

Sometimes it might seem that your change do not take effect. In this case the
culprit could be one of the caches. With dynamic actions there are two caches
involved:

The Routing Cache
-----------------

The routing cache stores the routing information computed from the `_routing`
collection. Whenever you change this collection manually, you need to call

    arangosh> require("internal").reloadRouting()

in order to rebuild the cache.

The Modules Cache
-----------------

If you use a dynamic action and this action is stored in module, then the
module functions are also stored in a cache in order to avoid parsing the
JavaScript code again and again.

Whenever you change the `modules` collections manually, you need to call

    arangosh> require("internal").flushServerModules()

in order to rebuild the cache.

Flush Order
-----------

If you define a dynamic routing and the controller, then you need to flush the
caches in a particular order. In order to build the routes, the module
information must be known. Therefore, you need to flush the modules caches
first.

    arangosh> require("internal").flushServerModules()
    arangosh> require("internal").reloadRouting()

Advanced Usages{#UserManualActionsAdvanced}
===========================================

For detailed information see the reference manual.

Redirects{#UserManualActionsAdvancedRedirects}
----------------------------------------------

Use the following for a permanent redirect:

    arangosh> db._routing.save({ 
    ........>   url: "/",
    ........>   action: {
    ........>     controller: "org/arangodb/actions",
    ........>     do: "redirectRequest", 
    ........>     options: { 
    ........>       permanently: true,
    ........>       destination: "http://somewhere.else/" } } });

Routing Bundles{#UserManualActionsAdvancedBundles}
--------------------------------------------------

Instead of adding all routes for package separately, you can
specify a bundle.

    {
      routes: [ 
	{ url: "/url1", content: "..." },
	{ url: "/url2", content: "..." },
	{ url: "/url3", content: "..." },
	... 
      ]
    }

The advantage is, that you can put all your routes into one document
and use a common prefix.

    {
      urlPrefix: "/test",

      routes: [ 
	{ url: "/url1", content: "..." },
	{ url: "/url2", content: "..." },
	{ url: "/url3", content: "..." },
	... 
      ]
    }

will define the URL `/test/url1`, `/test/url2`, and `/test/url3`.

Writing Middleware{#UserManualActionsAdvancedMiddleware}
--------------------------------------------------------

Assume, you want to log every request. In this case you can easily define an
action for the whole url-space `/`. This action simply logs the requests, calls
the next in line, and logs the response.

    exports.logRequest = function (req, res, options, next) {
      console.log("received request: %s", JSON.stringify(req));
      next();
      console.log("produced response: %s", JSON.stringify(res));
    };

This function is available as `org/arangodb/actions/logRequest`.  You need to
tell ArangoDB that it is should use a prefix match and that the shortest match
should win in this case:

    arangosh> db._routing.save({ 
    ........>   middleware: [
    ........>     { url: { match: "/*" }, action: { controller: "org/arangodb/actions", do: "logRequest" } }
    ........>   ]
    ........> });

Application Deployment{#UserManualActionsApplicationDeployment}
===============================================================

Using single routes or @ref UserManualActionsAdvancedBundles "bundles" can be
become a bit messy in large applications. Therefore a deployment tool exists
inside _arangosh_ to simplify the task. This tool was inspired by the ArangoDB
deployment tool `https://github.com/kaerus/arangodep` written in node.js by
kaerus.

An application is a bunch of routes, static pages stored in collections, and
small scriptlets stored modules.

In order to create an application, chose a suitable name, e. g. reverse domain
name plus the application name and call `createApp`:

    arangosh> var deploy = require("org/arangodb/deploy");
    arangosh> var app = deploy.createApp("org.example.simple");

Normally content will either be stored in collections or dynamically
calculated. But sometimes it is convenient to store the content directly in the
routing table, e. g. to deliver a version number.

    arangosh> app.mountStaticContent("/version", { 
    ........>   version: "1.2.3", major: 1, minor: 2, patch: 3 });
    [ArangoApp "org.example.simple" at ""]

Save the application

    arangosh> app.save();
    [ArangoApp "org.example.simple" at ""]

and use the browser to check the result

    http://localhost:8529/version

You can also specify the content-type

    arangosh> app.mountStaticContent("/author", 
    ........>                        "Frank Celler",
    ........>                        "text/plain").save();
    [ArangoApp "org.example.simple" at ""]

and check at

    http://localhost:8529/author

If you have more than one application, putting version under `/` might lead to
conflicts. It is therefore possible to use a common prefix for the application.

    arangosh> app.setPrefix("/example").save();
    [ArangoApp "org.example.simple" at "/example"]

Now check

    http://localhost:8529/example/version
    http://localhost:8529/example/author

Deploying Static Pages{#UserManualActionsDeployingStaticPages}
--------------------------------------------------------------

Most of the time, static html pages and JavaScript content will be delivered by
your web-server. But sometimes it is convenient to deliver these directly from
within ArangoDB. For example, to provide a small admin interface for you
application.

Assume that all data is stored underneath a directory "/tmp/example" and we want
to store the content in a collection "org_example_simple_content".

First connect the url path to the collection.

    arangosh> app.mountStaticPages("/static", "org_example_simple_content").save();
    [ArangoApp "org.example.simple" at "/example"]

Next create a file `index.html` at `/tmp/example/index.html".

    <html>
      <body>
        Hello World!
      </body>
    </html>

Create the collection and upload this into the collection

    arangosh> require("org/arangodb").db._createDocumentCollection("org_example_simple_content");
    [ArangoCollection 224910918055, "org_example_simple_content" (type document, status loaded)]
    
    arangosh> app.uploadStaticPages("/static", "/tmp/example");
    imported '/index.html' of type 'text/html; charset=utf-8'
    [ArangoApp "org.example.simple" at "/example"]

Check the index file

    http://localhost:8529/example/static/index.html

Deploying Modules{#UserManualActionsDeployingModules}
-----------------------------------------------------

In general deploying static pages is nice for demos and administrative
front-ends; but most of the time you will deploy JavaScript functions which will
compute JSON objects implementing a RESTful interface or something similar.

In order to deploy modules *not* belonging to a particular application use

    arangosh> var deploy = require("org/arangodb/deploy");

    arangosh> deploy.uploadModules("org/example", "/tmp/example/modules");
    imported '/org/example/simple'

This will upload all JavaScript files - which must end in `.js` - into the
database.  The first argument to `uploadModules` is a prefix used for the module
path.

For more details check the modules chapter in the reference handbook.
