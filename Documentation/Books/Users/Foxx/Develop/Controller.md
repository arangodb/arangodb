Details on Controller
=====================

### Create
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_initializer

HTTP Methods
------------

### get
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_get

### head
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_head

### post
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_post

### put
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_put

### patch
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_patch

### delete
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_delete

Documenting and constraining a specific route
---------------------------------------------

If you now want to document your route, you can use JSDoc style comments (a
multi-line comment block where the first line starts with */*** instead
of */**) above your routes to do that:

```js
/** Get all foxxes
  *
  * If you want to get all foxxes, please use this
  * method to do that.
  */
app.get("/foxxes", function () {
  // ...
});
```

The first line will be treated as a summary (For optical reasons in the
produced documentation, the summary is restricted to 60 characters). All
following lines will be treated as additional notes shown in the detailed
view of the route documentation. With the provided information, Foxx will
generate a nice documentation for you. Furthermore you can describe your
API by chaining the following methods onto your path definition:

### pathParam
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_pathParam

### queryParam
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_queryParam

### bodyParam
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_bodyParam

### errorResponse
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_errorResponse

### onlyif
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_onlyIf

### onlyIfAuthenticated
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_onlyIfAuthenticated

### summary
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_summary

### notes
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContext_notes


### extend

In many use-cases several of the functions are always used in a certain combination (e.g.: `onlyIf` with `errorResponse`).
In order to avoid duplicating this equal usage for several routes in your application you can
extend the controller with your own functions.
These functions can simply combine several of the above on a single name, so you only have to
invoke your self defined single function on all routes using these extensions.

@startDocuBlock JSF_foxx_controller_extend

Documenting and constraining all routes
---------------------------------------

In addition to documenting a specific route, you can also
do the same for all routes of a controller. For this purpose
use the **allRoutes** object of the according controller.
The following methods are available.

**Examples**

Provide an error response for all routes handled by this controller:

```js
ctrl.allRoutes
.errorResponse(Unauthorized, 401, 'Not authenticated.')
.errorResponse(NotFound, 404, 'Document not found.')
.errorResponse(ImATeapot, 418, 'I\'m a teapot.');

ctrl.get('/some/route', function (req, res) {
  // ...
  throw new NotFound('The document does not exist');
  // ...
}); // no errorResponse needed here

ctrl.get('/another/route', function (req, res) {
  // ...
  throw new NotFound('I made you a cookie but I ated it');
  // ...
}); // no errorResponse needed here either
```

### errorResponse
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContextBuffer_errorResponse

### onlyIf
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContextBuffer_onlyIf

### onlyIfAuthenticated
<!-- js/server/modules/org/arangodb/foxx/request_context.js -->
@startDocuBlock JSF_foxx_RequestContextBuffer_onlyIfAuthenticated

### pathParam
@startDocuBlock JSF_foxx_RequestContextBuffer_pathParam

### bodyParam
@startDocuBlock JSF_foxx_RequestContextBuffer_queryParam


Before and After Hooks
----------------------

You can use the following two functions to do something before or respectively
after the normal routing process is happening. You could use that for logging
or to manipulate the request or response (translate it to a certain format for
example).

### before
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_before

### after
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_after

### around
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_around


The Request and Response Objects
--------------------------------

When you have created your FoxxController you can now define routes on it.
You provide each with a function that will handle the request. It gets two
arguments (four, to be honest. But the other two are not relevant for now):

* The **request** object
* The **response** object

These objects are provided by the underlying ArangoDB actions and enhanced
by the **BaseMiddleware** provided by Foxx.

### The Request Object

The **request** object inherits several attributes from the underlying Actions:

* **compatibility**: an integer specifying the compatibility version sent by the
  client (in request header **x-arango-version**). If the client does not send this
  header, ArangoDB will set this to the minimum compatible version number. The
  value is 10000 * major + 100 * minor (e.g. *10400* for ArangoDB version 1.4).

* *user*: the name of the current ArangoDB user. This will be populated only
  if authentication is turned on, and will be *null* otherwise.

* *database*: the name of the current database (e.g. *_system*)

* *protocol*: *http* or *https*

* *server*: a JSON object with sub-attributes *address* (containing server
  host name or IP address) and *port* (server port).

* *path*: request URI path, with potential [database name](../../Glossary/README.md#database-name) stripped off.

* *url*: request URI path + query string, with potential database name
  stripped off

* *headers*: a JSON object with the request headers as key/value pairs. 
  **Note:** All header field names are lower-cased

* *cookies*: a JSON object with the request cookies as key/value pairs

* *requestType*: the request method (e.g. "GET", "POST", "PUT", ...)

* *requestBody*: the complete body of the request as a string

* *parameters*: a JSON object with all parameters set in the URL as key/value
  pairs

* *urlParameters*: a JSON object with all named parameters defined for the
  route as key/value pairs.

In addition to these attributes, a Foxx request objects provides the following
convenience methods:

### body
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_request_body

### rawBody
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_request_rawBody

### rawBodyBuffer
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_request_rawBodyBuffer

### params
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_request_params

### cookie
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_request_cookie

### requestParts
Only useful for multi-part requests.
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_request_requestParts

The Response Object
-------------------

Every response object has the body attribute from the underlying Actions
to set the raw body by hand.

You provide your response body as a string here.

### Response status
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_response_status

### Response set
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_response_set

### Response json
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_response_json

### Response cookie
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_response_cookie

### Response send
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_response_send

### Response sendFile
<!-- js/server/modules/org/arangodb/foxx/base_middleware.js -->
@startDocuBlock JSF_foxx_BaseMiddleware_response_sendFile

Controlling Access to Foxx Applications
---------------------------------------

Access to Foxx applications is controlled by the regular authentication mechanisms
present in ArangoDB. The server can be run with or without HTTP authentication.

If authentication is turned on,
then every access to the server is authenticated via HTTP authentication. This
includes Foxx applications. The global authentication can be toggled
via the configuration option.

If global HTTP authentication is turned on, requests to Foxx applications will
require HTTP authentication too, and only valid users present in the *_users*
system collection are allowed to use the applications.

Since ArangoDB 1.4, there is an extra option to restrict the authentication to
just system API calls, such as */_api/...* and */_admin/...*. This option can be
turned on using the
"server.authenticate-system-only" configuration option. If it is turned on,
then only system API requests need authentication whereas all requests to Foxx
applications and routes will not require authentication.

This is recommended if you want to disable HTTP authentication for Foxx applications
but still want the general database APIs to be protected with HTTP authentication.

If you need more fine grained control over the access to your Foxx application,
we built an authentication system you can use. Currently we only support cookie-based
authentication, but we will add the possibility to use Auth Tokens and external OAuth
providers in the near future. Of course you can roll your own authentication mechanism
if you want to, and you can do it in an application-specific way if required.

To use the per-application authentication, you should first turn off the global
HTTP authentication (or at least restrict it to system API calls as mentioned above).
Otherwise clients will need HTTP authentication and need additional authentication by
your Foxx application.

To have global HTTP authentication turned on for system APIs but turned off for Foxx,
your server startup parameters should look like this:

    --server.disable-authentication false --server.authenticate-system-only true

**Note**: During development, you may even turn off HTTP authentication completely:

    --server.disable-authentication true --server.authenticate-system-only true

Please keep in mind that turning HTTP authentication off completely will allow
unauthenticated access by anyone to all API functions, so do not use this is production.

Now it's time to configure the application-specific authentication. We built a small
[demo application](https://github.com/arangodb/foxx-authentication) to demonstrate how
this works.

To use the application-specific authentication in your own app, first activate it in your controller.

### Active Authentication
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_activateAuthentication

### Login
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_login

### Logout
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_logout

### Register
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_register

### Change Password
<!-- js/server/modules/org/arangodb/foxx/controller.js -->
@startDocuBlock JSF_foxx_controller_changePassword

#### Restricting routes

To restrict routes, see the documentation for Documenting and Restraining the routes.
