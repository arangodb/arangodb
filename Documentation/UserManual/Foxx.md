Foxx {#UserManualFoxx}
======================

@NAVIGATE_UserManualFoxx
@EMBEDTOC{UserManualFoxxTOC}

Foxx: Build APIs and simple web applications in ArangoDB{#UserManualFoxxIntro}
==============================================================================

Foxx is an easy way to create APIs and simple web applications from within
ArangoDB. It is inspired by Sinatra, the classy Ruby web framework. If
FoxxApplication is Sinatra, @ref UserManualActions are the corresponding
`Rack`. They provide all the HTTP goodness.

If you just want to install an existiting application, please use the @ref
UserManualFoxxManager. If you want to create your own application, please
continue.

So let's get started, shall we?

Creating the application files
------------------------------

An application built with Foxx is written in JavaScript and deployed to ArangoDB
directly. ArangoDB serves this application, you do not need a separate
application server.

So given you want to build an application that sends a plain-text response
"Worked!" for all requests to `/dev/meadow`.  How would you achieve that with
Foxx?

First, create a directory `apps` somewhere in your filesystem. Let's assume from
now on that the absolute path for this directory is `/home/user/apps`.

After that, create a sub-directory `my_app` in the `apps` directory and save the
following content in a file named `app.js` there:

    var Foxx = require("org/arangodb/foxx");
    var app = new Foxx.Application(applicationContext);
    
    app.get("/meadow", function(req, res) {
      res.set("Content-Type", "text/plain");
      res.body = "Worked!"
    });

Beside the `app.js` we need a manifest file. In order to achieve that, we create
a file called `manifest.json` in our `my_app` directory with the following
content:

    {
      "name": "my_app",
      "version": "0.0.1",
      "author": "me and myself",
      "apps": {
        "/": "app.js"
      }
    }

You **must** specify a name and a version number for your application, otherwise
it won't be loaded into ArangoDB.

You should now have the following files and directories with your application
(starting at `/home/user` in our example):

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
directory `/home/user/apps` as workspace. Produktion application are installed
using the Foxx manager and should not be changed. In development mode the server
automatically monitors the workspace and detects any change made to the files.

Replace `/home/user/apps` with the apps path that you initially created. This is
the path that you created the `my_app` directory in. Replace `/tmp/fancy_db`
with the directory your database is located in.

Now point your browser to `http://localhost:8529/dev/meadow` and you
should see "Worked!". After this short overview, let's get into the details.


Handling Requests{#UserManualFoxxHandlingRequests}
==================================================

In development mode all available applications from the application directory
`/home/user/apps` are visible under `http://localhost:8529/dev/DIRNAME` where
`DIRNAME` is the name of the directory of your application.

When applications are imstalled in production mode, you can change the `/dev`
prefix to whatever you like, see @ref UserManualFoxxManager.

If you do not redefine it, all requests that go to the root of your application
will be redirected to `index.html`.


Details on FoxxApplication{#UserManualFoxxDetailsApplication}
=============================================================

@copydetails JSF_foxx_application_initializer

@CLEARPAGE
@copydetails JSF_foxx_application_createRepository

@CLEARPAGE
HTTP Methods
------------

@copydetails JSF_foxx_application_get

@CLEARPAGE
@copydetails JSF_foxx_application_head

@CLEARPAGE
@copydetails JSF_foxx_application_post

@CLEARPAGE
@copydetails JSF_foxx_application_put

@CLEARPAGE
@copydetails JSF_foxx_application_patch

@CLEARPAGE
@copydetails JSF_foxx_application_delete

@CLEARPAGE
Documenting and Constraining the Routes
---------------------------------------

If you define a route like described above, you have the option to match parts
of the URL to variables. If you, for example, define a route `/animals/:animal`
and the URL `animals/foxx` is requested it is matched by this rule. You can then
get the value of this variable (in this case "foxx") by calling
`request.params("animal")`.

Furthermore you can describe your API by chaining the following methods onto
your path definition. With the provided information, Foxx will generate a nice
documentation for you. Some of the methods additionally will check certain
properties of the request.

@copydetails JSF_foxx_RequestContext_pathParam

@CLEARPAGE
@copydetails JSF_foxx_RequestContext_queryParam

@CLEARPAGE
@copydetails JSF_foxx_RequestContext_summary

@CLEARPAGE
@copydetails JSF_foxx_RequestContext_notes

@CLEARPAGE
@copydetails JSF_foxx_RequestContext_errorResponse

@CLEARPAGE
Before and After Hooks
----------------------

You can use the following two functions to do something before or respectively
after the normal routing process is happening. You could use that for logging or
to manipulate the request or response (translate it to a certain format for
example).

@copydetails JSF_foxx_application_before

@CLEARPAGE
@copydetails JSF_foxx_application_after

@CLEARPAGE
More functionality
------------------

@copydetails JSF_foxx_application_helper

@CLEARPAGE
@copydetails JSF_foxx_application_accepts

@CLEARPAGE
The Request and Response Objects
--------------------------------

When you have created your FoxxApplication you can now define routes on it. You
provide each with a function that will handle the request. It gets two arguments
(four, to be honest. But the other two are not relevant for now):

* The `request` object
* The `response` object

These objects are provided by the underlying ArangoDB actions and enhanced by
the `BaseMiddleware` provided by Foxx.

The Request Object
------------------

Every request object has the following attributes from the underlying Actions,
amongst others:

@FUN{request.path}

This is the complete path as supplied by the user as a String.

@CLEARPAGE
@copydetails JSF_foxx_BaseMiddleware_request_body

@CLEARPAGE
@copydetails JSF_foxx_BaseMiddleware_request_params

@CLEARPAGE
The Response Object
-------------------

Every response object has the following attributes from the underlying Actions:

@FUN{response.body}

You provide your response body as a String here.

@CLEARPAGE
@copydetails JSF_foxx_BaseMiddleware_response_status

@CLEARPAGE
@copydetails JSF_foxx_BaseMiddleware_response_set

@CLEARPAGE
@copydetails JSF_foxx_BaseMiddleware_response_json

@CLEARPAGE
@copydetails JSF_foxx_BaseMiddleware_response_render

@CLEARPAGE
Details on FoxxModel{#UserManualFoxxDetailsModel}
=================================================

The model doesn't know anything about the database. It is just a representation
of the data as an JavaScript object.  You can add and overwrite the methods of
the prototype in your model prototype via the object you give to extend. In your
model file, export the model as `model`.

    var Foxx = require("org/arangodb/foxx");
    
    var TodoModel = Foxx.Model.extend({
    });
    
    exports.model = TodoModel;

A Foxx Model can be initialized with an object of attributes and their values.

@copydetails JSF_foxx_model_extend

@CLEARPAGE
@copydetails JSF_foxx_model_initializer

@CLEARPAGE
@copydetails JSF_foxx_model_get

@CLEARPAGE
@copydetails JSF_foxx_model_set

@CLEARPAGE
@copydetails JSF_foxx_model_has

@CLEARPAGE
@FUN{FoxxModel::attributes}

The attributes property is the internal hash containing the model's state.

@CLEARPAGE
@copydetails JSF_foxx_model_forDB

@CLEARPAGE
@copydetails JSF_foxx_model_forClient

@CLEARPAGE
Details on FoxxRepository{#UserManualFoxxDetailsRepository}
===========================================================

A repository is a gateway to the database. It gets data from the database,
updates it or saves new data. It uses the given model when it returns a model
and expects instances of the model for methods like save. In your repository
file, export the repository as `repository`.

    Foxx = require("org/arangodb/foxx");
    
    TodosRepository = Foxx.Repository.extend({
    });
    
    exports.repository = TodosRepository;

@copydetails JSF_foxx_repository_initializer

@CLEARPAGE
@FUN{FoxxRepository::collection}

The collection object.

@CLEARPAGE
@FUN{FoxxRepository::prefix}

The prefix of the application.

@CLEARPAGE
@FUN{FoxxRepository::modelPrototype}

The prototype of the according model.

@CLEARPAGE
@FUN{FoxxRepository::save}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::remove}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::replace}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::update}

#### Foxx.Repository#update

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::removeByExample}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::replaceByExample}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::updateByExample}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::all}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::byExample}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

@CLEARPAGE
@FUN{FoxxRepository::firstExample}

**Not implemented**
See the documentation of collection (will be delegated to the collection).

The Manifest File{#UserManualFoxxManifest}
==========================================

In the `manifest.json` you define the components of your application. The
content is a JSON object with the following keys:

* `name`: Name of the application (Meta information)
* `version`: Current version of the application (Meta information)
* `description`: A short description of the application (Meta information)
* `thumbnail`: Path to a thumbnail that represents the application (Meta information)
* `apps`: Map routes to FoxxApplications
* `lib`: Base path for all required modules
* `files`: Deliver files
* `assets`: Deliver pre-processed files
* `system`: Mark an application as a system application
* `setup`: Path to a setup script
* `teardown`: Path to a teardown script

A more complete example for a Manifest file:

    {
      "name": "my_website",
      "version": "1.2.1",
      "description": "My Website with a blog and a shop",
      "thumnail": "images/website-logo.png",

      "apps": {
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
`appCollection` and `appCollectionName` just like models. Use the `setup` script
to create all collections your application needs and fill them with initial data
if you want to. Use the `teardown` script to remove all collections you have
created.

`apps` is an object that matches routes to files
------------------------------------------------

* The `key` is the route you want to mount at

* The `value` is the path to the JavaScript file containing the
  `FoxxApplication` you want to mount

You can add multiple applications in one manifest this way.

The `files`
------------

Deliver all files in a certain folder without modifying them. You can deliver
text files as well as binaries:

    "files": {
      "/images": "images"
    }

The `assets`
------------

The value for the asset key is an object consisting of paths that are matched to
the files they are composed of. Let's take the following example:

  "assets": {
    "application.js": {
      "files": [
        "vendor/jquery.js",
        "assets/javascripts/*"
      ]
    }
  }

If a request is made to `/application.js` (in development mode), the file array
provided will be processed one element at a time. The elements are paths to
files (with the option to use wildcards). The files will be concatenated and
delivered as a single file.

Development Mode
----------------

If you start ArangoDB with the option `--javascript.dev-app-path` followed by
the path to a directory containing a manifest file and the path to the database,
you are starting ArangoDB in development mode with the application loaded.  This
means that on every request:

1. All routes are dropped
2. All module caches are flushed
3. Your manifest file is read
4. All files in your lib folder are loaded
5. An app in DIRNAME is mounted at `/dev/DIRNAME`
6. The request will be processed

This means that you do not have to restart ArangoDB if you change anything in
your app. It is of course not meant for production, because the reloading makes
the app relatively slow.

Deploying on Production
-----------------------

*The Production mode is in development right now.*

We will offer the option to process all assets at once and write the files to
disk for production with the option to run `Uglify2.js` and similar tools in
order to compress them.

Controlling Access to Foxx Applications
---------------------------------------

At the moment, access to Foxx applications is controlled by the regular
authentication mechanisms present in ArangoDB.  The server can be run with or
without HTTP authentication.

If authentication is turned off, all Foxx applications and routes will be
callable by everyone with access to the server.  If authentication is turned on,
then every access to the server is authenticated via HTTP authentication. This
includes Foxx applications and routes. The global authentication can be toggled
via the configuration option @ref CommandLineArangoDisableAuthentication
"server.disable-authentication".

Since ArangoDB 1.4, there is an extra option to restrict the authentication to
just system API calls, such as `/_api/...` and `/_admin/...`. This option can be
turned on using the @ref CommandLineArangoAuthenticateSystemOnly
"server.authenticate-system-only" configuration option. If it is turned on, then
only system API requests need authentication whereas all requests to Foxx
applications and routes will not require authentication.

More fine-grained authentication control might be added in the future.

Optional Functionality: FormatMiddleware
----------------------------------------

Unlike the `BaseMiddleware` this Middleware is only loaded if you want it. This
Middleware gives you Rails-like format handling via the `extension` of the URL
or the accept header. Say you request an URL like `/people.json`:

The `FormatMiddleware` will set the format of the request to JSON and then
delete the `.json` from the request. You can therefore write handlers that do
not take an `extension` into consideration and instead handle the format via a
simple String. To determine the format of the request it checks the URL and then
the `accept` header. If one of them gives a format or both give the same, the
format is set. If the formats are not the same, an error is raised.

Use it by calling:

    FormatMiddleware = require('foxx').FormatMiddleware;
    app.before("/*", FormatMiddleware.new(['json']));

or the shortcut:

    app.accepts(['json']);

In both forms you can give a default format as a second parameter, if no format
could be determined. If you give no `defaultFormat` this case will be handled as
an error.
