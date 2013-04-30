Foxx {#UserManualFoxx}
======================

Foxx: Build APIs and simple web applications in ArangoDB
========================================================

Foxx is an easy way to create APIs and simple web applications from within **ArangoDB**. It is inspired by Sinatra, the classy Ruby web framework. If FoxxApplication is Sinatra, [ArangoDB Actions](http://www.arangodb.org/manuals/current/UserManualActions.html) are the corresponding `Rack`. They provide all the HTTP goodness.

So let's get started, shall we?

An application build with Foxx is written in JavaScript and deployed to ArangoDB directly. ArangoDB serves this application, you do not need a separate application server.

So given you want to build an application that sends a plain-text response "Worked!" for all requests to `/my/wiese`. How would you achieve that with Foxx?

First, create a directory `my_app` and save a file called `app.js` in this directory. Write the following content to this file:

    var Foxx = require("org/arangodb/foxx");

    var app = new Foxx.Application();

    app.get("/wiese", function(req, res) {
      res.set("Content-Type", "text/plain");
      res.body = "Worked!"
    });

    app.start(applicationContext);

This is your application. Now we need to mount it to the path `/my`. In order to achieve that, we create a file called `manifest.json` in our `my_app` directory with the following content:

    {
      "name": "my_app",
      "version": "0.0.1",
      "apps": {
        "/": "app.js"
      }
    }

You **must** specify a name and a version number for your application, otherwise it won't be loaded into ArangoDB.

Now your application is done. Start ArangoDB as follows:

    arangod --javascript.dev-app-path my_app /tmp/fancy_db

To include it to the list of apps running on your ArangoDB instance, start the ArangoDB shell and add your new application:

    $ ./bin/arango
    arangosh> aal = require('org/arangodb/aal');
    arangosh> aal.installDevApp('my_app', '/my');

Now point your browser to `http://localhost:8529/my/wiese` and you should see "Worked!". After this short overview, let's get into the details.

## Details on Foxx.Application

#### new Foxx.Application
@copydetails JSF_foxx_application_initializer

#### Foxx.Application#start
@copydetails JSF_foxx_application_start

#### Foxx.Application#requires

Using the base paths defined in the manifest file, you can require modules that you need in this FoxxApplication. So for example:

    app.requires = {
      "schafspelz": "wolf"
    };

This will require the file `wolf.js` in the libs folder you have defined and make the module available via the variable `schafspelz` in your FoxxApplication definitions:

    app.get("/bark", function (req, res) {
      schafspelz.bark();
    });

*Please note that you cannot use the normal require syntax in a `FoxxApplication`, because it's a special DSL and not a normal JavaScript file.*

#### Foxx.Application#registerRepository

Use this method to register a repository and a corresponding model. They can then be used in your handlers via `repository.name` where name is the registered name. A repository is a module that gets data from the database or saves data to it. A model is a representation of data which will be used by the repository.

    Foxx = require("org/arangodb/foxx");

    app = new Foxx.Application({});

    app.registerRepository("todos", {
      model: "models/todos",
      repository: "repositories/todos"
    });

If you do not give a repository, it will default to the `Foxx.Repository`. If you need more than the methods provided by it, you must give the path (relative to your lib directory) to your repository module there. Then you can extend the Foxx.Repository prototype and add your own methods.

If you do not give a model, it will default to the `Foxx.Model`. If you need more than the methods provided by it, you must give the path (relative to your lib directory) to your model module there. Then you can extend the Foxx.Model prototype and add your own methods.

If you don't need either of those, you don't need to give an empty object. You can then just call:

    app.registerRepository("todos");

### Handling Requests

If you do not redefine it, all requests that go to the root of your application will be redirected to `index.html`.

#### Foxx.Application#head
@copydetails JSF_foxx_application_head

#### Foxx.Application#get
@copydetails JSF_foxx_application_get

#### Foxx.Application#post
@copydetails JSF_foxx_application_post

#### Foxx.Application#put
@copydetails JSF_foxx_application_put

#### Foxx.Application#patch
@copydetails JSF_foxx_application_patch

#### Foxx.Application#delete
@copydetails JSF_foxx_application_delete

### Documenting and Constraining the Routes

If you define a route like described above, you have the option to match parts of the URL to variables. If you for example define a route `/animals/:animal` and the URL `animals/foxx` is requested it is matched by this rule. You can then get the value of this variable (in this case "foxx") by calling `request.params("animal")`.

Furthermore you can describe your API by chaining the following methods onto your path definition. With the provided information, Foxx will generate a nice documentation for you. Some of the methods additionally will check certain properties of the request.

#### Describing a pathParam
@copydetails JSF_foxx_RequestContext_pathParam

#### Describing a queryParam
@copydetails JSF_foxx_RequestContext_queryParam

#### Documenting the nickname of a route
@copydetails JSF_foxx_RequestContext_nickname

#### Documenting the summary of a route
@copydetails JSF_foxx_RequestContext_summary

#### Documenting the notes of a route
@copydetails JSF_foxx_RequestContext_notes

#### Documenting the error response of a route
@copydetails JSF_foxx_RequestContext_errorResponse

### Before and After Hooks

You can use the following two functions to do something before or respectively after the normal routing process is happening. You could use that for logging or to manipulate the request or response (translate it to a certain format for example).

#### Foxx.Application#before
@copydetails JSF_foxx_application_before

#### Foxx.Application#after
@copydetails JSF_foxx_application_after

### More functionality

#### Foxx.Application#helper
@copydetails JSF_foxx_application_helper

#### Foxx.Application#accepts
@copydetails JSF_foxx_application_accepts

## The Request and Response Objects

When you have created your FoxxApplication you can now define routes on it. You provide each with a function that will handle the request. It gets two arguments (four, to be honest. But the other two are not relevant for now):

* The `request` object
* The `response` object

These objects are provided by the underlying ArangoDB actions and enhanced by the BaseMiddleware provided by Foxx.

### The `request` object

Every request object has the following attributes from the underlying Actions, amongst others:

#### request.path
This is the complete path as supplied by the user as a String.

#### request.body
@copydetails JSF_foxx_BaseMiddleware_request_body

#### request.params
@copydetails JSF_foxx_BaseMiddleware_request_params

### The `response` object

Every response object has the following attributes from the underlying Actions:

#### request.body
You provide your response body as a String here.

#### request.status
@copydetails JSF_foxx_BaseMiddleware_response_status

#### request.set
@copydetails JSF_foxx_BaseMiddleware_response_set

#### request.json
@copydetails JSF_foxx_BaseMiddleware_response_json

#### request.render
@copydetails JSF_foxx_BaseMiddleware_response_render

## Foxx.Model

  Foxx = require("org/arangodb/foxx");

  TodoModel = Foxx.Model.extend({
  });

  exports.model = TodoModel;

The model doesn't know anything about the database. It is just a representation of the data as an JavaScript object. You can add and overwrite the methods of the prototype in your model prototype via the object you give to extend. In your model file, export the model as `model`.

A Foxx Model can be initialized with an object of attributes and their values.

#### Foxx.Model.extend
@copydetails JSF_foxx_model_extend

#### new Foxx.Model
@copydetails JSF_foxx_model_initializer

#### Foxx.Model#get
@copydetails JSF_foxx_model_get

#### Foxx.Model#set
@copydetails JSF_foxx_model_set

#### Foxx.Model#has
@copydetails JSF_foxx_model_has

#### Foxx.Model#attributes

The attributes property is the internal hash containing the model's state.

#### Foxx.Model#toJSON
@copydetails JSF_foxx_model_toJSON

## Foxx.Repository

A repository is a gateway to the database. It gets data from the database, updates it or saves new data. It uses the given model when it returns a model and expects instances of the model for methods like save. In your repository file, export the repository as `repository`.

  Foxx = require("org/arangodb/foxx");

  TodosRepository = Foxx.Repository.extend({
  });

  exports.repository = TodosRepository;

#### new Foxx.Repository
@copydetails JSF_foxx_repository_initializer

#### Foxx.Repository#collection

The collection object.

#### Foxx.Repository#prefix

The prefix of the application.

#### Foxx.Repository#modelPrototype

The prototype of the according model.

#### Foxx.Repository#save

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#remove

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#replace

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#update

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#removeByExample

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#replaceByExample

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#updateByExample

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#all

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#byExample

**Not implemented**
See the documentation of collection (will be delegated to the collection).

#### Foxx.Repository#firstExample

**Not implemented**
See the documentation of collection (will be delegated to the collection).

## The Manifest File

In the `manifest.json` you define the components of your application: The content is a JSON object with the following keys:

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

### `setup` and `teardown`

You can provide a path to a JavaScript file that prepares ArangoDB for your application (or respectively removes it entirely). These scripts have access to `appCollection` and `appCollectionName` just like models. Use the `setup` script to create all collections your application needs and fill them with initial data if you want to. Use the `teardown` script to remove all collections you have created.

### `apps` is an object that matches routes to files:

* The `key` is the route you want to mount at
* The `value` is the path to the JavaScript file containing the `FoxxApplication` you want to mount

You can add multiple applications in one manifest this way.

### The `files`

Deliver all files in a certain folder without modifying them. You can deliver text files as well as binaries:

    "files": {
      "/images": "images"
    }

### The `assets`

The value for the asset key is an object consisting of paths that are matched to the files they are composed of. Let's take the following example:

  "assets": {
    "application.js": {
      "files": [
        "vendor/jquery.js",
        "assets/javascripts/*"
      ]
    }
  }

If a request is made to `/application.js` (in development mode), the file array provided will be processed one element at a time. The elements are paths to files (with the option to use wildcards). The files will be concatenated and delivered as a single file.

## Development Mode

If you start ArangoDB with the option `--javascript.dev-app-path` followed by the path to a directory containing a manifest file, you are starting ArangoDB in development mode with the application loaded. This means that on every request:

1. All routes are dropped
2. All module caches are flushed
3. Your manifest file is read
4. All files in your lib folder are loaded
5. All `apps` are executed. Each `app.start()` will put the routes in a temporary route object, prefixed with the path given in the manifest
6. All routes in the temporary route object are stored to the routes
7. The request will be processed

This means that you do not have to restart ArangoDB if you change anything in your app. It is of course not meant for production, because the reloading makes the app relatively slow.

## Deploying on Production

*The Production mode is in development right now.*
We will offer the option to process all assets at once and write the files to disk for production with the option to run `Uglify2.js` and similar tools in order to compress them.

## Optional Functionality: FormatMiddleware

Unlike the `BaseMiddleware` this Middleware is only loaded if you want it. This Middleware gives you Rails-like format handling via the `extension` of the URL or the accept header. Say you request an URL like `/people.json`:

The `FormatMiddleware` will set the format of the request to JSON and then delete the `.json` from the request. You can therefore write handlers that do not take an `extension` into consideration and instead handle the format via a simple String. To determine the format of the request it checks the URL and then the `accept` header. If one of them gives a format or both give the same, the format is set. If the formats are not the same, an error is raised.

Use it by calling:

    FormatMiddleware = require('foxx').FormatMiddleware;
    app.before("/*", FormatMiddleware.new(['json']));

or the shortcut:

    app.accepts(['json']);

In both forms you can give a default format as a second parameter, if no format could be determined. If you give no `defaultFormat` this case will be handled as an error.
