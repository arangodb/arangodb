# Fuxx: Build APIs and simple web applications in ArangoDB

**WARNING: The following documentation file is pure fiction,
it is not yet finished**

An application build with Fuxx is written in JavaScript and deployed
to ArangoDB directly. ArangoDB serves this application, you do not
need a separate application server.

So given you want to build an application that sends a plain-text
response "Worked!" for all requests to `/my/wiese`. How would you
achieve that with Fuxx?

First, create a directory `my_app` and save a file called `app.js`
in this directory. Write the following content to this file:

    FuxxApplication = require("org/arangodb/fuxx").FuxxApplication;

    app = new FuxxApplication();

    app.get('/wiese', function(req, res) {
      res.set("Content-Type", "text/plain");
      res.body = "Worked!"
    });

    app.start();

This is your application. Now we need to mount it to the path `/my`.
In order to achieve that, we create a file called `manifest.json` in
our `my_app` directory with the following content:

    {
      'apps': {
        '/my': 'app.js'
      }
    }

Now your application is done. Start ArangoDB as follows:

    arangod --app my_app /tmp/fancy_db

Now point your browser to `/my/wiese` and you should see "Worked!".
After this short overview, let's get into the details.

## FuxxApplication Features

Please see the documentation of `fuxx.js` for further information on how to write the application file.

## Manifest Files

When you start arangod with the `--app` option, ArangoDB scans the
given directory on every request for files called `manifest.json`.
There can be a file in the root directory and in each direct subdirectory if you want that.
The content is a JSON object with two keys: `apps` and `libs`.
(*we will also add a third one called `vendor` for NPM packages, but
this does not exist yet*).
`apps` is an object that matches routes to files:

* The `key` is the route you want to mount at
* The `value` is the path to the JavaScript file containing the `FuxxApplication`s you want to mount

You can add multiple applications in one manifest in this way.

In addition you can add an optional `lib` String. This is a path to
a folder containing multiple JavaScript files which define CommonJS
modules that you want to use in your Fuxx apps. They will all be loaded,
so you can require them as usual. The `lib` folder can be structured however
you want. If you have a folder `models` in your `lib` folder containing
a file `user.js`, you can require it with `user = require('models/user')`.

A more complete example for a Manifest file:

    {
      'apps': {
        '/blog': 'apps/blog.js',
        '/shop': 'apps/shop.js'
      },

      'lib': 'lib'
    }


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

If you are comfortable to deploy your app to your production ArangoDB, you will have to do
the following:

**Not yet decided**
