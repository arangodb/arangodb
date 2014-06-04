!CHAPTER Details on FoxxRepository

A repository is a gateway to the database. It gets data from the
database, updates it or saves new data. It uses the given model when it 
returns a model and expects instances of the model for methods like save. 
In your repository file, export the repository as `repository`.

    Foxx = require("org/arangodb/foxx");
    
    TodosRepository = Foxx.Repository.extend({
    });
    
    exports.repository = TodosRepository;

Initialize

new FoxxRepository(collection, opts)
Create a new instance of Repository

A Foxx Repository is always initialized with a collection object. You can get your collection object by asking your Foxx.Controller for it: the collection method takes the name of the collection (and will prepend the prefix of your application). It also takes two optional arguments:

Model: The prototype of a model. If you do not provide it, it will default to Foxx.Model
Prefix: You can provide the prefix of the application if you need it in your Repository (for some AQL queries probably). Get it from the Foxx.Controller via the collectionPrefix attribute.
Examples

instance = new Repository(app.collection("my_collection"));
// or:
instance = new Repository(app.collection("my_collection"), {
model: MyModelPrototype,
prefix: app.collectionPrefix,
});

!SUBSUBSECTION Collection

See the documentation of collection.

!SUBSUBSECTION Prefix

See the documentation of collection.

!SUBSUBSECTION ModelPrototype

See the documentation of collection.

!SUBSUBSECTION Save

Expects a model. Will set the ID and Rev on the model. Returns the model (for convenience).

!SUBSUBSECTION Remove By Id

Expects an ID of an existing document.

!SUBSUBSECTION Remove By Example

Find all documents that fit this example and remove them.

!SUBSUBSECTION Replace

Find the model in the database by its _id and replace it with this version. Expects a model. Sets the Revision of the model. Returns the model (for convenience).

!SUBSUBSECTION ReplaceById

Find the model in the database by the given ID and replace it with the given. model. Expects a model. Sets the ID and Revision of the model. Returns the model (for convenience).

!SUBSUBSECTION By Id

Returns the model for the given ID.

!SUBSUBSECTION By Example

Returns an array of models for the given ID.

!SUBSUBSECTION First Example

Returns a model that matches the given example.



<!--
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
-->
!SECTION The Manifest File

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

!SUBSECTION The setup and teardown scripts

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

!SUBSECTION controllers is an object that matches routes to files

* The `key` is the route you want to mount at

* The `value` is the path to the JavaScript file containing the
  `FoxxController` you want to mount

You can add multiple controllers in one manifest this way.

!SUBSECTIONThe files

Deliver all files in a certain folder without modifying them. You can deliver 
text files as well as binaries:

    "files": {
      "/images": "images"
    }

!SUBSECTION The assets

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

!SUBSECTION Development Mode

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

!SUBSECTION Production Mode

To run a Foxx app in production first copy your app code to the directory given in 
the config variable `--javascript.app-path`. After that use Foxx manager to mount the app.
You can also use Foxx manager to find out your current app-path.

!SUBSECTION Controlling Access to Foxx Applications


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

To use the application-specific authentication in your own app, first activate it in your controller:

`FoxxController::activateAuthentication(opts)`

To activate authentication for this authentication, first call this function. Provide the following arguments:

type: Currently we only support cookie, but this will change in the future.
cookieLifetime: An integer. Lifetime of cookies in seconds.
cookieName: A string used as the name of the cookie.
sessionLifetime: An integer. Lifetime of sessions in seconds.

*Examples*

  app.activateAuthentication({
    type: "cookie",
    cookieLifetime: 360000,
    cookieName: "my_cookie",
    sessionLifetime: 400,
  });

!SUBSUBSECTION Adding a login route

`FoxxController::login(path, opts)`

Add a route for the login. You can provide further customizations via the the options:

usernameField and passwordField can be used to adjust the expected attributes in the post request. They default to username and password. onSuccess is a function that you can define to do something if the login was successful. This includes sending a response to the user. This defaults to a function that returns a JSON with user set to the identifier of the user and key set to the session key. onError is a function that you can define to do something if the login did not work. This includes sending a response to the user. This defaults to a function that sets the response to 401 and returns a JSON with error set to "Username or Password was wrong". Both onSuccess and onError should take request and result as arguments.

*Examples*

  app.login('/login', {
    onSuccess: function (req, res) {
      res.json({"success": true});
    }
  });

!SUBSUBSECTION Adding a logout route

`FoxxController::logout(path, opts)`

This works pretty similar to the logout function and adds a path to your app for the logout functionality. You can customize it with a custom onSuccess and onError function: onSuccess is a function that you can define to do something if the logout was successful. This includes sending a response to the user. This defaults to a function that returns a JSON with message set to "logged out". onError is a function that you can define to do something if the logout did not work. This includes sending a response to the user. This defaults to a function that sets the response to 401 and returns a JSON with error set to "No session was found". Both onSuccess and onError should take request and result as arguments.

*Examples*

  app.logout('/logout', {
    onSuccess: function (req, res) {
      res.json({"message": "Bye, Bye"});
    }
  });

!SUBSUBSECTION Adding a register route

`FoxxController::register(path, opts)`

This works pretty similar to the logout function and adds a path to your app for the register functionality. You can customize it with a custom onSuccess and onError function: onSuccess is a function that you can define to do something if the registration was successful. This includes sending a response to the user. This defaults to a function that returns a JSON with user set to the created user document. onError is a function that you can define to do something if the registration did not work. This includes sending a response to the user. This defaults to a function that sets the response to 401 and returns a JSON with error set to "Registration failed". Both onSuccess and onError should take request and result as arguments. You can also set the fields containing the username and password via usernameField (defaults to username) and passwordField (defaults to password). If you want to accept additional attributes for the user document, use the option acceptedAttributes and set it to an array containing strings with the names of the additional attributes you want to accept. All other attributes in the request will be ignored. If you want default attributes for the accepted attributes or set additional fields (for example admin) use the option defaultAttributes which should be a hash mapping attribute names to default values.

`Examples`

  app.register('/logout', {
    acceptedAttributes: ['name'],
    defaultAttributes: {
      admin: false
    }
  });

!SUBSUBSECTION Adding a change password route

`FoxxController::changePassword(route, opts)`

Add a route for the logged in user to change the password. You can provide further customizations via the the options:

passwordField can be used to adjust the expected attribute in the post request. It defaults to password. onSuccess is a function that you can define to do something if the change was successful. This includes sending a response to the user. This defaults to a function that returns a JSON with notice set to "Changed password!". onError is a function that you can define to do something if the login did not work. This includes sending a response to the user. This defaults to a function that sets the response to 401 and returns a JSON with error set to "No session was found". Both onSuccess and onError should take request and result as arguments.

*Examples*

  app.changePassword('/changePassword', {
    onSuccess: function (req, res) {
      res.json({"success": true});
    }
  });


<!--
@copydetails JSF_foxx_controller_activateAuthentication

### Adding a login route

@copydetails JSF_foxx_controller_login

### Adding a logout route

@copydetails JSF_foxx_controller_logout

### Adding a register route

@copydetails JSF_foxx_controller_register

### Adding a change password route

@copydetails JSF_foxx_controller_changePassword
-->

!SUBSUBSECTION  Restricting routes

To restrict routes, see the documentation for Documenting and Restraining the routes.

!SUBSECTION Optional Functionality: FormatMiddleware

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

!SUBSECTION Optional Functionality: TemplateMiddleware

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

!SUBSUBSECTION Initialize

Initialize with the name of a collection or a collection and optionally a set of helper functions. Then use before to attach the initialized middleware to your Foxx.Controller

*Examples*

  templateMiddleware = new TemplateMiddleware("templates", {
    uppercase: function (x) { return x.toUpperCase(); }
  });
  // or without helpers:
  //templateMiddleware = new TemplateMiddleware("templates");
  app.before(templateMiddleware);

!SUBSUBSECTION Render

`response.render(templatePath, data)`

When the TemplateMiddleware is included, you will have access to the render function on the response object. If you call render, Controller will look into the this collection and search by the path attribute. It will then render the template with the given data.

*Examples*

  response.render("high/way", {username: 'Application'})

<!--
### Initialize

@copydetails JSF_foxx_TemplateMiddleware_initializer

### Render

@copydetails JSF_foxx_TemplateMiddleware_response_render
-->