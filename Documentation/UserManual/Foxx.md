Foxx {#UserManualFoxx}
======================

Foxx: Build APIs and simple web applications in ArangoDB
========================================================

Foxx is an easy way to create APIs and simple web applications from within **ArangoDB**. It is inspired by Sinatra, the classy Ruby web framework. If FoxxApplication is Sinatra, [ArangoDB Actions](http://www.arangodb.org/manuals/current/UserManualActions.html) are the corresponding `Rack`. They provide all the HTTP goodness.

So let's get started, shall we?

An application build with Foxx is written in JavaScript and deployed to ArangoDB directly. ArangoDB serves this application, you do not need a separate application server.

So given you want to build an application that sends a plain-text response "Worked!" for all requests to `/my/wiese`. How would you achieve that with Foxx?

First, create a directory `my_app` and save a file called `app.js` in this directory. Write the following content to this file:

    FoxxApplication = require("org/arangodb/foxx").FoxxApplication;

    app = new FoxxApplication();

    app.get("/wiese", function(req, res) {
      res.set("Content-Type", "text/plain");
      res.body = "Worked!"
    });

    app.start(applicationContext);

This is your application. Now we need to mount it to the path `/my`. In order to achieve that, we create a file called `manifest.json` in our `my_app` directory with the following content:

    {
      "apps": {
        "/my": "app.js"
      }
    }

Now your application is done. Start ArangoDB as follows:

    arangod --app my_app /tmp/fancy_db

Now point your browser to `/my/wiese` and you should see "Worked!". After this short overview, let's get into the details.

## Details on FoxxApplication

#### new FoxxApplication
@copydetails JSF_foxx_application_initializer

#### FoxxApplication#start
@copydetails JSF_foxx_application_start

### Handling Requests

#### FoxxApplication#head
@copydetails JSF_foxx_application_head

#### FoxxApplication#get
@copydetails JSF_foxx_application_get

#### FoxxApplication#post
@copydetails JSF_foxx_application_post

#### FoxxApplication#put
@copydetails JSF_foxx_application_put

#### FoxxApplication#patch
@copydetails JSF_foxx_application_patch

#### FoxxApplication#delete
@copydetails JSF_foxx_application_delete

### Before and After Hooks

You can use the following two functions to do something before or respectively after the normal routing process is happening. You could use that for logging or to manipulate the request or response (translate it to a certain format for example).

#### FoxxApplication#before
@copydetails JSF_foxx_application_before

#### FoxxApplication#after
@copydetails JSF_foxx_application_after

### More functionality

#### FoxxApplication#helper
@copydetails JSF_foxx_application_helper

#### FoxxApplication#accepts
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

## Manifest Files

When you start arangod with the `--app` option, ArangoDB scans the
given directory on every request for files called `manifest.json`.
There can be a file in the root directory and in each direct subdirectory if you want that.
The content is a JSON object with three keys: `apps`, `lib` and `assets`.
`apps` is an object that matches routes to files:

* The `key` is the route you want to mount at
* The `value` is the path to the JavaScript file containing the `FoxxApplication`s you want to mount

You can add multiple applications in one manifest in this way.

In addition you can add an optional `lib` String. This is a path to
a folder containing multiple JavaScript files which define CommonJS
modules that you want to use in your Foxx apps. They will all be loaded,
so you can require them as usual. The `lib` folder can be structured however
you want. If you have a folder `models` in your `lib` folder containing
a file `user.js`, you can require it with `user = require("models/user")`.

Also optional is the definition of an `asset` directive.
Read more about in the section Assets.

A more complete example for a Manifest file:

    {
      "apps": {
        "/blog": "apps/blog.js",
        "/shop": "apps/shop.js"
      },

      "lib": "lib",

      "assets": {
        "application.js": [
          "vendor/jquery.js",
          "assets/javascripts/*"
        ]
      }
    }

## Assets

The value for the asset key is an object consisting of paths that are
matched to files they are composed of. Let"s take the following example:

  "assets": {
    "application.js": [
      "vendor/jquery.js",
      "assets/javascripts/*"
    ]
  }

If a request is made to `/application.js` (in development mode), the array
provided will be processed one element at a time. The elements are paths
to files (with the option to use wildcards).

A file with an unknown extension will just be used as it is. It is however
possible to define preprocessors based on the file extension.
Currently we have defined a preprocessor for CoffeeScript that will process
all files ending in `.coffee`.
When the file was processed, the extension will be removed and the file
will be executed again. This is useful if you want to chain multiple processors.

The processed files will then be concatenated and delivered as a single file.
We provide source maps, so if your browser supports them, you can always jump
to the original file.
We will offer the option to process all assets at once and write the files to
disk for production with the option to run `Uglify.js` and similar tools
in order to compress them.

## Development Mode

If you start the application in the way described above, you are working
in development mode. This means that on every request:

1. All routes are dropped
2. All module caches are flushed
3. Your app directory is scanned for manifest files, and each manifest file is read
4. All files in your lib folder(s) are loaded
5. All `apps` are executed. Each `app.start()` will put the routes in a temporary route object, prefixed with the path given in the manifest
6. All routes in the temporary route object are stored to the routes
7. The request will be processed

This means that you do not have to restart ArangoDB if you change anything in your app
(*This will change when we add support for `vendor`, they will not be reloaded*).
It is of course not meant for production, because the reloading makes the app relatively slow.

## Deploying on Production

*The Production mode is in development right now.*

## Optional Functionality: FormatMiddleware

Unlike the `BaseMiddleware` this Middleware is only loaded if you want it. This Middleware gives you Rails-like format handling via the `extension` of the URL or the accept header. Say you request an URL like `/people.json`:

The `FormatMiddleware` will set the format of the request to JSON and then delete the `.json` from the request. You can therefore write handlers that do not take an `extension` into consideration and instead handle the format via a simple String. To determine the format of the request it checks the URL and then the `accept` header. If one of them gives a format or both give the same, the format is set. If the formats are not the same, an error is raised.

Use it by calling:

    FormatMiddleware = require('foxx').FormatMiddleware;
    app.before("/*", FormatMiddleware.new(['json']));

or the shortcut:

    app.accepts(['json']);

In both forms you can give a default format as a second parameter, if no format could be determined. If you give no `defaultFormat` this case will be handled as an error.
