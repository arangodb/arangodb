# Foxx: Build APIs and simple web applications in ArangoDB

**WARNING: The following documentation file is pure fiction,
it is not yet finished**

An application build with Foxx is written in JavaScript and deployed
to ArangoDB directly. ArangoDB serves this application, you do not
need a separate application server.

So given you want to build an application that sends a plain-text
response "Worked!" for all requests to `/my/wiese`. How would you
achieve that with Foxx?

First, create a directory `my_app` and save a file called `app.js`
in this directory. Write the following content to this file:

    FoxxApplication = require("org/arangodb/foxx").FoxxApplication;

    app = new FoxxApplication();

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

## FoxxApplication Features

Please see the documentation of `foxx.js` for further information on how to write the application file.

## Manifest Files

When you start arangod with the `--app` option, ArangoDB scans the
given directory on every request for files called `manifest.json`.
There can be a file in the root directory and in each direct subdirectory if you want that.
The content is a JSON object with three keys: `apps`, `lib` and `assets`.
(*we will also add a fourth called `vendor` for NPM packages, but this does not exist yet*).
`apps` is an object that matches routes to files:

* The `key` is the route you want to mount at
* The `value` is the path to the JavaScript file containing the `FoxxApplication`s you want to mount

You can add multiple applications in one manifest in this way.

In addition you can add an optional `lib` String. This is a path to
a folder containing multiple JavaScript files which define CommonJS
modules that you want to use in your Foxx apps. They will all be loaded,
so you can require them as usual. The `lib` folder can be structured however
you want. If you have a folder `models` in your `lib` folder containing
a file `user.js`, you can require it with `user = require('models/user')`.

Also optional is the definition of an `asset` directive.
Read more about in the section Assets.

A more complete example for a Manifest file:

    {
      'apps': {
        '/blog': 'apps/blog.js',
        '/shop': 'apps/shop.js'
      },

      'lib': 'lib',

      'assets': {
        'application.js': [
          'vendor/jquery.js',
          'assets/javascripts/*'
        ]
      }
    }

## Assets

The value for the asset key is an object consisting of paths that are
matched to files they are composed of. Let's take the following example:

  'assets': {
    'application.js': [
      'vendor/jquery.js',
      'assets/javascripts/*'
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

If you are comfortable to deploy your app to your production ArangoDB, you will have to do
the following:

**Not yet decided**
