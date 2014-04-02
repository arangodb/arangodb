Foxx {#UserManualFoxx}
======================

@NAVIGATE_UserManualFoxx
@EMBEDTOC{UserManualFoxxTOC}

Foxx: Build APIs and simple web applications in ArangoDB{#UserManualFoxxIntro}
==============================================================================

Foxx is an easy way to create APIs and simple web applications from within 
ArangoDB. It is inspired by Sinatra, the classy Ruby web framework. If 
Foxx is Sinatra, @ref UserManualActions are the corresponding `Rack`. 
They provide all the HTTP goodness.

If you just want to install an existing application, please use the 
@ref UserManualFoxxManager. If you want to create your own application, 
please continue reading.

Overview
========

An application built with Foxx is written in JavaScript and deployed to 
ArangoDB directly. ArangoDB serves this application, you do not need a 
separate application server.

Think of an Foxx app as a typical web app similar to any other web app using
other technologies. A Foxx app provides one or more URLs, which can either
be accessed directly from the browser or from a backend application written e.g. in
Ruby or C#. Other features include:

* **Routing:** Define arbitrary routes namespaced via the `Controllers`
* **Accesses data:** Direct access to all data in ArangoDB using simple queries, AQL, traversals and more
* **Manipulates data:** Create new or manipulate existing entries
* Deliver **static files** like HTML pages, CSS or images directly

The typical request to a Foxx application will work as follows (only conceptually,
a lot of the steps are cached in reality):

1. The request is routed to a Foxx application depending on the mount point 
2. The according controller of this application is determined (via something called the manifest file)
3. The request is then routed to a specific handler in this controller

The handler will now parse the request. This includes determining all parameters
from the body (which is typically JSON encoded) to the path parameters of the URL.
It is then up to you to handle this request and generate a response. In this process
you will probably access the database. This is done via the **Repository**: This is an
entity that is responsible for a collection and specifically:

1. Creating new entries in this collection
2. Modify or delete existing entries in this collection
3. Search for entries in this collection

To represent an entry in this collection it will use a **Model**, which is a wrapper around
the raw data from the database. Here you can implement helper functions or simple access
methods.

Your first Foxx app in 5 minutes - a step-by-step tutorial 
==========================================================

Let's build an application that sends a plain-text response 
"Hello YourName!" for all requests to `/dev/my_app/hello/YourName`. 

First, create a directory `apps` somewhere in your filesystem. This will be
the Foxx application base directory for your database instance. Let's assume 
from now on that the absolute path for this directory is `/home/user/apps`.
When you have created the directory, create a sub-directory `databases` in it.

Foxx applications are database-specific, and the `databases` sub-directory is
used to separate the Foxx applications of different databases running in the
same ArangoDB instance.

Let's assume for now that you are working in the default database (`_system`), that
is used when no database name is specified otherwise. To use Foxx applications with
the `_system`, create a sub-directory `_system` inside the `databases` subdirectory.
All Foxx applications for the `_system` database will go into this directory.

Finally, we can add a sub-directory `my_app` in the `_system` directory and should
end up with the following directory layout (starting at `/home/user` in our example):

    apps/
      databases/
        _system/
          my_app/

In the `my_app` directory, create a file named `app.js` and save the following content
in it:

    (function() {
        "use strict";
    
        var Foxx = require("org/arangodb/foxx"),
            controller = new Foxx.Controller(applicationContext)
    
        controller.get("/hello/:name", function(req, res) {
            res.set("Content-Type", "text/plain");
            res.body = "Hello " + req.params("name");
        }); 
    
    }());

Beside the `app.js` we need a manifest file. In order to achieve that, we 
create a file called `manifest.json` in our `my_app` directory with the 
following content:

    {
      "name": "my_app",
      "version": "0.0.1",
      "author": "me and myself",
      "controllers": {
        "/": "app.js"
      }
    }

You **must** specify a name and a version number for your application, 
otherwise it won't be loaded into ArangoDB.

You should now have the following files and directories with your 
application (starting at `/home/user` in our example):

    apps/
      databases/
        _system/
          my_app/
            manifest.json
            app.js

This is your application, and you're ready to use it.

Testing the application
-----------------------

Start ArangoDB as follows:

    $ arangod --javascript.dev-app-path /home/user/apps /tmp/fancy_db

If you chose a different directory name, you need to replace `/home/user/apps` 
with the actual directory name of course. Replace `/tmp/fancy_db` with the
directory your database data is located in.

The command will start the ArangoDB server in a **development mode** using the
directory `/home/user/apps` as the workspace and `/tmp/fancy_db` as your
database directory.  In development mode the server automatically reloads all
application files on every request, so changes to the underlying files are
visible instantly. 
Note: if you use the development mode for the first time or choose a different
directory for `dev-app-path`, it may be necessary to start ArangoDB with the
`--upgrade` option once. This will initialise the specified application directory.

Note: the development mode is convenient when developing applications but the
permanent reloading has an impact on performance. Therefore permanent reloading is
only performed in development mode and not in production mode. Whenever you think 
your application is ready for production, you can install it using the Foxx manager
and avoid the overhead of reloading.

Now point your browser to `http://localhost:8529/dev/my_app/hello/YourName` and you should 
see "Hello YourName".  

After this short overview, let's get into the details. There are several example
apps available on Github. You can install them via Foxx manager (covered in the
chapter on Foxx manager) or simply clone them from `https://github.com/arangodb/`.

Start with "hello-foxx" (`https://github.com/arangodb/hello-foxx`) as it contains
several basic usage examples. "aye-aye" and "fugu" are more advanced apps showing how
to use Backbone, Underscore and Jquery together with Foxx. "foxx-authentication" shows 
how to register users, login and check permissions.

Handling Requests{#UserManualFoxxHandlingRequests}
==================================================

In development mode all available applications from the application directory 
`/home/user/apps/databases/<database name>/` are visible under 
`http://localhost:8529/dev/<directory name>` where `<database name>` is the 
name of the current database and `<directory name>` is the directory name of your 
application.

In our example, `<directory name>` was `my_app` and as we didn't specify a 
database, `<database name>` defaulted to `_system`.

When applications are installed in production mode, you can change the `/dev` 
prefix to whatever you like, see @ref UserManualFoxxManager.

If you do not redefine it, all requests that go to the root of your 
application (i.e. `/`) will be redirected to `index.html`.

This means that if your application does not provide a file `index.html`, 
calling the application root URL my result in a 404 error.
In our example, the application root URL is `http://localhost:8529/dev/my_app/hello/`.
Call it, and you should something like this in return:

    {
      "error": true,
      "code": 404,
      "errorNum": 404,
      "errorMessage": "unknown path 'dev/my_app/index.html'"
    }

To fix that, you can give your app a different default document, e.g. "hello/unknown".
The adjusted manifest now looks like this:

    {
      "name": "my_app",
      "version": "0.0.1",
      "author": "me and myself",
      "controllers": {
        "/": "app.js"
      },
      "defaultDocument": "hello/unknown"
    }

Note: browsers tend to cache results of redirections. To see the new default 
document in effect, first clear your browser's cache and point your browser
to `http://localhost:8529/dev/my_app/`.

Accessing collections from Foxx
===============================

Foxx assumes by default that an application has itws own collections. 
Accessing collections directly by name could cause problems, for 
instance if you had two completely independent Foxx applications that 
both access their own collection 'users'. 

To prevent such issues, Foxx provides functions that return an 
application-specific collection name. 
For example, applicationContext.collectionName('users') will return the 
collection name prefixed with the application name, e.g. "myapp_users". 
This allows to have a 'users' collection which is specific for each 
application. 

Additionally, a Foxx controller has a function "collection" that returns 
a reference to a collection prefixed like above, in the same way as 
db.<collection-name> would do. 
In the example, controller.collection('users') would return the 
collection object for the "myapp_users" collection, and you could use it 
like any other collection with the db object, e.g. 

    controller.collection('users').toArray() 
    controller.collection('users').save(...) 
    controller.collection('users').remove(...) 
    controller.collection('users').replace(...) 

Of course you still use any collection directly with the db object even 
from Foxx. To access an collection called "movies" this could be one solution: 

    app.get("/all", function(req, res) { 
        var db = require("org/arangodb").db; 
        res.json({ movies: db.movies.toArray() }); 
    }); 

Of course this completely bypasses prefixing and repositories, but works 
well especially for quick tests or shared collections that are NOT 
application-specific. 

Then there are Foxx repositories. These are objects that you can create 
to hide the internals of the database access from the application so 
that the application will just use the repository but not the database. 

A repository is an object that wrap access to a collection (or multiple 
collections if you want), whereas controller.collection returns the 
collection itself. That's the main difference. 

To return a list of users from a controller using a repository, you 
could use it like this: 

    var foxx = require("org/arangodb/foxx"); 
    var db = require("org/arangodb").db; 
    var usersRepo = new foxx.Repository(db._collection("users")); 
    app.get("/all", function(req, res) { 
       res.json({ users: usersRepo.collection.toArray() }); 
    }); 

Of course you can create your own methods in the repository to add extra 
functionality. 

Details on FoxxController{#UserManualFoxxDetailsController}
=============================================================

@copydetails JSF_foxx_controller_initializer

HTTP Methods
------------

### Get

@copydetails JSF_foxx_controller_get

### Head

@copydetails JSF_foxx_controller_head

### Post

@copydetails JSF_foxx_controller_post

### Put

@copydetails JSF_foxx_controller_put

### Patch

@copydetails JSF_foxx_controller_patch

### Delete

@copydetails JSF_foxx_controller_delete

Documenting and constraining a specific route
---------------------------------------------

If you now want to document your route, you can use JSDoc style comments (a 
multiline comment block with the first line starting with `/**` instead 
of `/*`) above your routes to do that:

@verbinclude foxx-doc-comment

The first line will be treated as a summary (For optical reasons in the 
produced documentation, the summary is restricted to 60 characters). All 
following lines will be treated as additional notes shown in the detailed 
view of the route documentation. With the provided information, Foxx will 
generate a nice documentation for you. Furthermore you can describe your 
API by chaining the following methods onto your path definition:

### Path Param

@copydetails JSF_foxx_RequestContext_pathParam

### Query Param

@copydetails JSF_foxx_RequestContext_queryParam

### Body Param

@copydetails JSF_foxx_RequestContext_bodyParam

### Error Response

@copydetails JSF_foxx_RequestContext_errorResponse

### onlyIf

@copydetails JSF_foxx_RequestContext_onlyIf

### onlyIfAuthenticated

@copydetails JSF_foxx_RequestContext_onlyIfAuthenticated

Documenting and constraining all routes of a controller
-------------------------------------------------------

In addition to documenting a specific route, you can also
do the same for all routes of a controller. For this purpose
use the `allRoutes` object of the according controller.
The following methods are available:

### Error Response

@copydetails JSF_foxx_RequestContextBuffer_errorResponse

### onlyIf

@copydetails JSF_foxx_RequestContextBuffer_onlyIf

### onlyIfAuthenticated

@copydetails JSF_foxx_RequestContextBuffer_onlyIfAuthenticated

Before and After Hooks
----------------------

You can use the following two functions to do something before or respectively 
after the normal routing process is happening. You could use that for logging 
or to manipulate the request or response (translate it to a certain format for 
example).

### Before

@copydetails JSF_foxx_controller_before

### After

@copydetails JSF_foxx_controller_after


The Request and Response Objects
--------------------------------

When you have created your FoxxController you can now define routes on it. 
You provide each with a function that will handle the request. It gets two 
arguments (four, to be honest. But the other two are not relevant for now):

* The `request` object
* The `response` object

These objects are provided by the underlying ArangoDB actions and enhanced 
by the `BaseMiddleware` provided by Foxx.

The Request Object
------------------

The `request` object inherits several attributes from the underlying Actions:

* `compatibility`: an integer specifying the compatibility version sent by the
  client (in request header `x-arango-version`). If the client does not send this
  header, ArangoDB will set this to the minimum compatible version number. The 
  value is 10000 * major + 100 * minor (e.g. `10400` for ArangoDB version 1.4).

* `user`: the name of the current ArangoDB user. This will be populated only
  if authentication is turned on, and will be `null` otherwise.

* `database`: the name of the current database (e.g. `_system`)

* `protocol`: `http` or `https`
    
* `server`: a JSON object with sub-attributes `address` (containing server 
  host name or IP address) and `port` (server port).

* `path`: request URI path, with potential database name stripped off.

* `url`: request URI path + query string, with potential database name 
   stripped off

* `headers`: a JSON object with the request headers as key/value pairs
    
* `cookies`: a JSON object with the request cookies as key/value pairs

* `requestType`: the request method (e.g. "GET", "POST", "PUT", ...)

* `requestBody`: the complete body of the request as a string

* `parameters`: a JSON object with all parameters set in the URL as key/value
  pairs

* `urlParameters`: a JSON object with all named parameters defined for the
  route as key/value pairs.

In addition to these attributes, a Foxx request objects provides the following
convenience methods:

### Body

@copydetails JSF_foxx_BaseMiddleware_request_body

### Raw Body

@copydetails JSF_foxx_BaseMiddleware_request_rawBody

### Params

@copydetails JSF_foxx_BaseMiddleware_request_params


The Response Object
-------------------

Every response object has the body attribute from the underlying Actions
to set the raw body by hand.

You provide your response body as a string here.

### Status

@copydetails JSF_foxx_BaseMiddleware_response_status

### Set

@copydetails JSF_foxx_BaseMiddleware_response_set

### JSON

@copydetails JSF_foxx_BaseMiddleware_response_json


Details on FoxxModel{#UserManualFoxxDetailsModel}
=================================================

The model doesn't know anything about the database. It is just a representation 
of the data as an JavaScript object.  You can add and overwrite the methods of 
the prototype in your model prototype via the object you give to extend. In 
your model file, export the model as `model`.

    var Foxx = require("org/arangodb/foxx");
    
    var TodoModel = Foxx.Model.extend({
    });
    
    exports.model = TodoModel;

A Foxx Model can be initialized with an object of attributes and their values.

There's also the possibility of annotation: The second hash you give to the
extend method are properties of the prototype. You can define an attributes 
property there:

    var Foxx = require("org/arangodb/foxx");
    
    var PersonModel = Foxx.Model.extend({
      // Your instance properties
    }, {
      // Your prototype properties
      attributes: {
        name: { type: "string", required: true },
        age: { type: "integer" },
        active: { type: "boolean", defaultValue: true }
    });
    
    exports.model = TodoModel;

This has two effects: On the one hand it provides documentation. If you annotated
your model, you can use it in the `bodyParam` method for documentation.
On the other hand it will influence the behavior of the constructor: If you provide
an object to the constructor, it will only take those attributes that are listed
in the attributes object. This is especially useful if you want to to initialize
the Model from user input. On the other hand it will set the default value for all
attributes that have not been set by hand. An example:

    var person = new PersonModel({
      name: "Pete",
      admin: true
    });

    person.attributes // => { name: "Pete", active: true }

### Extend

@copydetails JSF_foxx_model_extend

### Initialize

@copydetails JSF_foxx_model_initializer

### Get

@copydetails JSF_foxx_model_get

### Set

@copydetails JSF_foxx_model_set

### Has

@copydetails JSF_foxx_model_has

### Attributes

@copydetails JSF_foxx_model_attributes

### forDB

@copydetails JSF_foxx_model_forDB

### forClient

@copydetails JSF_foxx_model_forClient


Details on FoxxRepository{#UserManualFoxxDetailsRepository}
===========================================================

A repository is a gateway to the database. It gets data from the
database, updates it or saves new data. It uses the given model when it 
returns a model and expects instances of the model for methods like save. 
In your repository file, export the repository as `repository`.

    Foxx = require("org/arangodb/foxx");
    
    TodosRepository = Foxx.Repository.extend({
    });
    
    exports.repository = TodosRepository;

### Initialize

@copydetails JSF_foxx_repository_initializer

### Collection

@copydetails JSF_foxx_repository_collection

### Prefix

@copydetails JSF_foxx_repository_prefix

### ModelPrototype

@copydetails JSF_foxx_repository_modelPrototype

### Save

@copydetails JSF_foxx_repository_save

### Remove By Id

@copydetails JSF_foxx_repository_removeById

### Remove By Example

@copydetails JSF_foxx_repository_removeByExample

### Replace

@copydetails JSF_foxx_repository_replace

### ReplaceById

@copydetails JSF_foxx_repository_replaceById

### By Id

@copydetails JSF_foxx_repository_byId

### By Example

@copydetails JSF_foxx_repository_byExample

### First Example

@copydetails JSF_foxx_repository_firstExample

The Manifest File{#UserManualFoxxManifest}
==========================================

In the `manifest.json` you define the components of your application. 
The content is a JSON object with the following attributes (not all 
attributes are required though):

* `assets`: Deliver pre-processed files
* `author`: The author name
* `contributors`: An array containing objects, each represents a contributor (with `name` and optional `email`)
* `controllers`: Map routes to FoxxControllers
* `defaultDocument`: The default document when the applicated root (`/`) is called (defaults to `index.html`)
* `description`: A short description of the application (Meta information)
* `engines`: Should be an object with `arangodb` set to the ArangoDB version your Foxx app is compatible with.
* `files`: Deliver files
* `isSystem`: Mark an application as a system application
* `keywords`: An array of keywords to help people find your Foxx app
* `lib`: Base path for all required modules
* `license`: Short form of the license (MIT, GPL...)
* `name`: Name of the application (Meta information)
* `repository`: An object with information about where you can find the repository: `type` and `url`
* `setup`: Path to a setup script
* `teardown`: Path to a teardown script
* `thumbnail`: Path to a thumbnail that represents the application (Meta information)
* `version`: Current version of the application (Meta information)

If you install an application using the Foxx manager or are using the
development mode, your manifest will be checked for completeness and common errors.
You should have a look at the server log files after changing a manifest file to 
get informed about potential errors in the manifest.

A more complete example for a Manifest file:

    {
      "name": "my_website",
      "version": "1.2.1",
      "description": "My Website with a blog and a shop",
      "thumnail": "images/website-logo.png",

      "controllers": {
        "/blog": "apps/blog.js",
        "/shop": "apps/shop.js"
      },

      "lib": "lib",

      "files": {
        "/images": "images"
      },

      "assets": {
        "application.js": {
          "files": [
            "vendor/jquery.js",
            "assets/javascripts/*"
          ]
        }
      },

      "setup": "scripts/setup.js",
      "teardown": "scripts/teardown.js"
    }

The `setup` and `teardown` scripts
----------------------------------

You can provide a path to a JavaScript file that prepares ArangoDB for your
application (or respectively removes it entirely). These scripts have access to
`appCollection` and `appCollectionName`. Use the `setup` script to create all
collections your application needs and fill them with initial data if you want
to. Use the `teardown` script to remove all collections you have created.

Note: the setup script is called on each request in the development mode.
If your application needs to set up specific collections, you should always 
check in the setup script whether they are already there.

The teardown script is called when an application is uninstalled. It is good
practice to drop any collections in the teardown script that the application used 
exclusively, but this is not enforced. Maybe there are reasons to keep application
data even after removing an application. It's up to you to decide what to do.

`controllers` is an object that matches routes to files
------------------------------------------------

* The `key` is the route you want to mount at

* The `value` is the path to the JavaScript file containing the
  `FoxxController` you want to mount

You can add multiple controllers in one manifest this way.

The `files`
------------

Deliver all files in a certain folder without modifying them. You can deliver 
text files as well as binaries:

    "files": {
      "/images": "images"
    }

The `assets`
------------

The value for the asset key is an object consisting of paths that are matched 
to the files they are composed of. Let's take the following example:

  "assets": {
    "application.js": {
      "files": [
        "vendor/jquery.js",
        "assets/javascripts/*"
      ]
    }
  }

If a request is made to `/application.js` (in development mode), the file 
array provided will be processed one element at a time. The elements are 
paths to files (with the option to use wildcards). The files will be 
concatenated and delivered as a single file.

The content-type (or mime type) of the HTTP response when requesting 
`application.js` is automatically determined by looking at the filename 
extension in the asset name (`application.js` in the above example). 
If the asset does not have a filename extension, the content-type is 
determined by looking at the filename extension of the first file in the 
`files` list. If no file extension can be determined, the asset will be
delived with a content-type of `text/plain`.

It is possible to explicitly override the content-type for an asset by
setting the optional `contentType` attribute of an asset as follows:

  "assets": {
    "myincludes": {
      "files": [
        "vendor/jquery.js",
        "assets/javascripts/*"
      ],
      "contentType": "text/javascript"
    }
  }

Development Mode
----------------

If you start ArangoDB with the option `--javascript.dev-app-path` followed by 
the path to a directory containing a manifest file and the path to the 
database, you are starting ArangoDB in development mode with the application 
loaded. This means that on every request:

1. All routes are dropped
2. All module caches are flushed
3. Your manifest file is read
4. All files in your lib folder are loaded
5. An app in DIRNAME is mounted at `/dev/DIRNAME`
6. The request will be processed

This means that you do not have to restart ArangoDB if you change anything 
in your app. It is of course not meant for production, because the reloading 
makes the app relatively slow.

Production Mode
---------------
To run a Foxx app in production first copy your app code to the directory given in 
the config variable `--javascript.app-path`. After that use Foxx manager to mount the app.
You can also use Foxx manager to find out your current app-path.

Controlling Access to Foxx Applications
---------------------------------------

Access to Foxx applications is controlled by the regular authentication mechanisms 
present in ArangoDB. The server can be run with or without HTTP authentication.

If authentication is turned on, 
then every access to the server is authenticated via HTTP authentication. This 
includes Foxx applications. The global authentication can be toggled 
via the configuration option @ref CommandLineArangoDisableAuthentication 
"server.disable-authentication".

If global HTTP authentication is turned on, requests to Foxx applications will 
require HTTP authentication too, and only valid users present in the `_users`
system collection are allowed to use the applications.

Since ArangoDB 1.4, there is an extra option to restrict the authentication to 
just system API calls, such as `/_api/...` and `/_admin/...`. This option can be 
turned on using the @ref CommandLineArangoAuthenticateSystemOnly 
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

Note: during development, you may even turn off HTTP authentication completely:
    
    --server.disable-authentication true --server.authenticate-system-only true
 
Please keep in mind that turning HTTP authentication off completely will allow 
unauthenticated access by anyone to all API functions, so do not use this is production.

Now it's time to configure the application-specific authentication. We built a small 
[demo application](https://github.com/arangodb/foxx-authentication) to demonstrate how
this works. 

To use the application-specific authentication in your own app, first activate it in
your controller:

@copydetails JSF_foxx_controller_activateAuthentication

### Adding a login route

@copydetails JSF_foxx_controller_login

### Adding a logout route

@copydetails JSF_foxx_controller_logout

### Adding a register route

@copydetails JSF_foxx_controller_register

### Adding a change password route

@copydetails JSF_foxx_controller_changePassword

### Restricting routes

To restrict routes, see the documentation for Documenting and Restraining the routes.

Optional Functionality: FormatMiddleware
----------------------------------------

To use this plugin, please require it first:

  FormatMiddleware = require("org/arangodb/foxx/template_middleware").FormatMiddleware;

This Middleware gives you Rails-like format handling via the `extension` of 
the URL or the accept header. Say you request an URL like `/people.json`:

The `FormatMiddleware` will set the format of the request to JSON and then 
delete the `.json` from the request. You can therefore write handlers that 
do not take an `extension` into consideration and instead handle the 
format via a simple string. To determine the format of the request it 
checks the URL and then the `accept` header. If one of them gives a format 
or both give the same, the format is set. If the formats are not the same, 
an error is raised.

Use it by calling:

    FormatMiddleware = require('foxx').FormatMiddleware;
    app.before(FormatMiddleware.new(['json']));

In both forms you can give a default format as a second parameter, if no 
format could be determined. If you give no `defaultFormat` this case will be 
handled as an error.

Optional Functionality: TemplateMiddleware
------------------------------------------

To use this plugin, please require it first:

  TemplateMiddleware = require("org/arangodb/foxx/template_middleware").TemplateMiddleware;

The `TemplateMiddleware` can be used to give a Foxx.Controller the capability 
of using templates. Currently you can only use Underscore Templates.  It 
expects documents in the following form in this collection:

    {
      path: "high/way",
      content: "hello <%= username %>",
      contentType: "text/plain",
      templateLanguage: "underscore"
    }

The `content` is the string that will be rendered by the template processor. 
The `contentType` is the type of content that results from this call. And with 
the `templateLanguage` you can choose your template processor. There is only 
one choice now: `underscore`.  Which would set the body of the response to 
`hello Controller` with the template defined above. It will also set the 
`contentType` to `text/plain` in this case.  In addition to the attributes 
you provided, you also have access to all your view helpers.

### Initialize

@copydetails JSF_foxx_TemplateMiddleware_initializer

### Render

@copydetails JSF_foxx_TemplateMiddleware_response_render

Iteratively Developing an Application{#UserManualFoxxDevelopment}
=================================================================

While developing a Foxx application, it is recommended to use the development
mode. The development mode makes ArangoDB reload all components of all Foxx
applications on every request. While this has a negative impact on performance,
it allows to view and debug changes in the application instantly. It is not
recommended to use the development mode in production.

During development it is often necessary to log some debug output. ArangoDB
provides a few mechanisms for this:

- using `console.log`:
  ArangoDB provides the `console` module, which you can use from within your
  Foxx application like this:
        
      require("console").log(value);

  The `console` module will log all output to ArangoDB's logfile. If you are
  not redirecting to log output to stdout, it is recommended that you follow
  ArangoDB's logfile using a `tail -f` command or something similar.
  Please refer to @ref JSModuleConsole for details.

- using `internal.print`:
  The `print` method of the `internal` module writes data to the standard
  output of the ArangoDB server process. If you have start ArangoDB manually
  and do not run it as an (unattended) daemon, this is a convenient method:

      require("internal").print(value);

- tapping requests / responses:
  Foxx allows to tap incoming requests and outgoing responses using the `before`
  and `after` hooks. To print all incoming requests to the stdout of the ArangoDB
  server process, you could use some code like this in your controller:
   
      controller.before("/*", function (req, res) {
        require("internal").print(req);
      });

  Of course you can also use `console.log` or any other means of logging output.

Deploying a Foxx application{#UserManualFoxxDeployment}
=======================================================

When a Foxx application is ready to be used in production, it is time to leave the
development mode and deploy the app in a production environment.

The first step is to copy the application's script directory to the target ArangoDB 
server. If your development and production environment are the same, there is
nothing to do. If production runs on a different server, you should copy the
development application directory to some temporary place on the production server.

When the application code is present on the production server, you can use the
`fetch` and `mount` commands from the @ref UserManualFoxxManager to register the
application in the production ArangoDB instance and make it available.

Here are the individual steps to carry out:

- development: 
  - cd into the directory that application code is in. Then create a tar.gz file with 
    the application code (replace `app` with the actual name):

        cd /path/to/development/apps/directory
        tar cvfz app.tar.gz app

  - copy the tar.gz file to the production server:

        scp app.tar.gz production:/tmp/

- production:
  - create a temporary directory, e.g. `/tmp/apps` and extract the tar archive into 
    this directory:

        mkdir /tmp/apps
        cd /tmp/apps
        tar xvfz /tmp/app.tar.gz

  - start the ArangoDB shell and run the following commands in it:

        fm.fetch("directory", "/tmp/apps/app");
        fm.mount("app", "/app");

More information on how to deploy applications from different sources can be
found in the @ref UserManualFoxxManager "Foxx Manager manual".

@BNAVIGATE_UserManualFoxx
