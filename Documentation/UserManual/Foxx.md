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

If you just want to install an existiting application, please use the 
@ref UserManualFoxxManager. If you want to create your own application, 
please continue.

So let's get started, shall we?

Creating the application files
------------------------------

An application built with Foxx is written in JavaScript and deployed to 
ArangoDB directly. ArangoDB serves this application, you do not need a 
separate application server.

So given you want to build an application that sends a plain-text response 
"Hello YourName!" for all requests to `/dev/my_app/hello/YourName`.  How would you achieve that 
with Foxx?

First, create a directory `apps` somewhere in your filesystem. Let's assume 
from now on that the absolute path for this directory is `/home/user/apps`.

After that, create a sub-directory `my_app` in the `apps` directory and 
save the following content in a file named `app.js` there:

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
      my_app/
        manifest.json
        app.js

This is your application.

Testing the application
-----------------------

Now your application is ready to be tested. Start ArangoDB as follows:

    $ arangod --javascript.dev-app-path /home/user/apps /tmp/fancy_db

This will start the ArangoDB server in a **development mode** using the
directory `/home/user/apps` as the workspace and `/tmp/fancy_db` as your
database directory.  In development mode the server automatically monitors the
workspace and detects any change made to the files. Production application are
installed using the Foxx manager and changes will not be reloaded automatically.

Replace `/home/user/apps` with the apps path that you initially created. This 
is the path that you created the `my_app` directory in. Replace `/tmp/fancy_db` 
with the directory your database is located in.

Now point your browser to `http://localhost:8529/dev/my_app/hello/YourName` and you should 
see "Hello YourName".  

After this short overview, let's get into the details. There are several example
apps available on Github. You can install them via Foxx manager (covered in the
chapter on Foxx manager) or simply clone them from `https://github.com/arangodb/`.

Start with "hello-foxx" (`https://github.com/arangodb/hello-foxx`) as it contains
several basic usage examples. "aye-aye" and "fugu" are more advanced apps showing how
to use Backbone, Underscore and Jquery together with Foxx. foxx-authentication shows 
how to register users, login and check permissions.

Handling Requests{#UserManualFoxxHandlingRequests}
==================================================

In development mode all available applications from the application directory 
`/home/user/apps` are visible under `http://localhost:8529/dev/DIRNAME` where 
`DIRNAME` is the name of the directory of your application.

When applications are installed in production mode, you can change the `/dev` 
prefix to whatever you like, see @ref UserManualFoxxManager.

If you do not redefine it, all requests that go to the root of your 
application will be redirected to `index.html`.


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

    /** Get all Foxes
     * 
     * If you want to get all foxes, please use this
     * method to do that.
     */
    app.get("/foxes", function () {
      //...
    });

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

Every request object has the `path` method from the underlying Actions. 
This is the complete path as supplied by the user as a String.

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

You provide your response body as a String here.

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
The content is a JSON object with the following keys:

* `name`: Name of the application (Meta information)
* `version`: Current version of the application (Meta information)
* `description`: A short description of the application (Meta information)
* `license`: Short form of the license (MIT, GPL...)
* `author`: the author name
* `contributors`: An array containing objects, each represents a contributor (with `name` and optional `email`)
* `thumbnail`: Path to a thumbnail that represents the application (Meta information)
* `repository`: An object with information about where you can find the repository: `type` and `url`
* `keywords`: An array of keywords to help people find your Foxx app
* `engines`: Should be an object with `arangodb` set to the ArangoDB version your Foxx app is compatible with.
* `controllers`: Map routes to FoxxControllers
* `lib`: Base path for all required modules
* `files`: Deliver files
* `assets`: Deliver pre-processed files
* `isSystem`: Mark an application as a system application
* `setup`: Path to a setup script
* `teardown`: Path to a teardown script

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

Controlling Access to Foxx Controllers
---------------------------------------

At the moment, access to Foxx.Controllers is controlled by the regular 
authentication mechanisms present in ArangoDB.  The server can be run with 
or without HTTP authentication.

If authentication is turned off, all Foxx.Controllers and routes will be 
callable by everyone with access to the server.  If authentication is turned on, 
then every access to the server is authenticated via HTTP authentication. This 
includes Foxx.Controllers and routes. The global authentication can be toggled 
via the configuration option @ref CommandLineArangoDisableAuthentication 
"server.disable-authentication".

Since ArangoDB 1.4, there is an extra option to restrict the authentication to 
just system API calls, such as `/_api/...` and `/_admin/...`. This option can be 
turned on using the @ref CommandLineArangoAuthenticateSystemOnly 
"server.authenticate-system-only" configuration option. If it is turned on, 
then only system API requests need authentication whereas all requests to Foxx 
applications and routes will not require authentication.

Authentication
--------------

We built an authentication system you can use in your Foxx application (but you
can of course roll your own if you want). Currently we only support
cookie-based authentication, but we will add the possibility to use Auth Tokens
and external OAuth providers in the near future. To use the authentication in
your app, first activate it:

@copydetails JSF_foxx_controller_activateAuthentication

### Adding a login route

@copydetails JSF_foxx_controller_login

### Adding a logout route

@copydetails JSF_foxx_controller_logout

### Adding a register route

@copydetails JSF_foxx_controller_register

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
format via a simple String. To determine the format of the request it 
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
