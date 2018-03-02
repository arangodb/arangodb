Installing Foxx Applications
============================

Introduction to Foxx manager
----------------------------

The Foxx manager is a shell program. It should have been installed under **/usr/bin** or **/usr/local/bin**
when installing the ArangoDB package. An instance of the ArangoDB server must be
up and running.

```
unix> foxx-manager
Expecting a command, please try:

Example usage:
 foxx-manager install <app-info> <mount-point> option1=value1
 foxx-manager uninstall <mount-point>

Further help:
 foxx-manager help   for the list of foxx-manager commands
 foxx-manager --help for the list of options
```

The most important commands are

* **install**: Installs a Foxx application to a local URL and calls its setup method
* **uninstall**: Deletes a Foxx application and calls its teardown method 
* **list**: Lists all installed Foxx applications (alias: **installed**)

When dealing with a fresh install of ArangoDB, there should be no installed
applications besides the system applications that are shipped with ArangoDB.

```
unix> foxx-manager list
Mount                   Name           Author               Description                                       Version    Development
---------------------   ------------   ------------------   -----------------------------------------------   --------   ------------
/_admin/aardvark        aardvark       ArangoDB GmbH        ArangoDB Admin Web Interface                      1.0        no
/_api/gharial           gharial        ArangoDB GmbH        ArangoDB Graph Module                             0.1        no
/_system/cerberus       cerberus       ArangoDB GmbH        Password Manager                                  0.0.1      no
/_system/sessions       sessions       ArangoDB GmbH        Session storage for Foxx.                         0.1        no
/_system/simple-auth    simple-auth    ArangoDB GmbH        Simple password-based authentication for Foxx.    0.1        no
---------------------   ------------   ------------------   -----------------------------------------------   --------   ------------
5 application(s) found
```

There are currently several applications installed, all of them are system applications. 
You can safely ignore system applications.  

We are now going to install the _hello world_ application. It is called 
"hello-foxx" - no surprise there.

```
unix> foxx-manager install hello-foxx /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

The second parameter */example* is the mount path of the application.
Assuming that you are running a local installation of ArangoDB you should be able to access the example application under

    http://localhost:8529/_db/_system/example

using your favorite browser.
If you are connecting to a remote instance please replace `localhost` accordingly.
It will now also be visible when using the *list* command.

```
unix> foxx-manager list
Mount                   Name           Author               Description                                       Version    Development
---------------------   ------------   ------------------   -----------------------------------------------   --------   ------------
/_admin/aardvark        aardvark       ArangoDB GmbH        ArangoDB Admin Web Interface                      1.0        no
/_api/gharial           gharial        ArangoDB GmbH        ArangoDB Graph Module                             0.1        no
/_system/cerberus       cerberus       ArangoDB GmbH        Password Manager                                  0.0.1      no
/_system/sessions       sessions       ArangoDB GmbH        Session storage for Foxx.                         0.1        no
/_system/simple-auth    simple-auth    ArangoDB GmbH        Simple password-based authentication for Foxx.    0.1        no
/example                hello-foxx     Frank Celler         This is 'Hello World' for ArangoDB Foxx.          1.5.0      no
---------------------   ------------   ------------------   -----------------------------------------------   --------   ------------
6 application(s) found
```

You can install the application again under different mount path. 

```
unix> foxx-manager install hello-foxx /hello
Application hello-foxx version 1.5.0 installed successfully at mount point /hello
```

You now have two separate instances of the same application. They are 
completely independent of each other.

```
unix> foxx-manager list
Mount                   Name           Author               Description                                       Version    Development
---------------------   ------------   ------------------   -----------------------------------------------   --------   ------------
/_admin/aardvark        aardvark       ArangoDB GmbH        ArangoDB Admin Web Interface                      1.0        no
/_api/gharial           gharial        ArangoDB GmbH        ArangoDB Graph Module                             0.1        no
/_system/cerberus       cerberus       ArangoDB GmbH        Password Manager                                  0.0.1      no
/_system/sessions       sessions       ArangoDB GmbH        Session storage for Foxx.                         0.1        no
/_system/simple-auth    simple-auth    ArangoDB GmbH        Simple password-based authentication for Foxx.    0.1        no
/example                hello-foxx     Frank Celler         This is 'Hello World' for ArangoDB Foxx.          1.5.0      no
/hello                  hello-foxx     Frank Celler         This is 'Hello World' for ArangoDB Foxx.          1.5.0      no
---------------------   ------------   ------------------   -----------------------------------------------   --------   ------------
7 application(s) found
```

The current version of the application is *1.5.0* (check the output of *list*).
It is even possible to mount a different version of an application.

Now let's remove the instance mounted under */hello*.

```
unix> foxx-manager uninstall /hello
Application hello-foxx version 1.5.0 uninstalled successfully from mount point /hello
```

Note that "uninstall" will trigger the *teardown* script, which allows the 
application to clean up its own data. 
Afterwards the application will be deleted from disk.

We can also replace a running application by any other application:

```
unix> foxx-manager replace itzpapalotl:0.9.0 /example
Application itzpapalotl version 0.9.0 installed successfully at mount point /example
```

This is a shortcut for an *uninstall* then *install* procedure and includes
invocation of *teardown* and *setup* scripts of the respective applications.
If no application is installed at the mount point this shortcut will fail, use *install* instead.
Note here we have specified a specific version of the application: *0.9.0*.

Finally you can upgrade a running application to a newer version:

```
unix> foxx-manager update itzpapalotl:1.0.0 /example
Application itzpapalotl version 1.0.0 installed successfully at mount point /example
```

This will do *uninstall* and *install* of the application but will **not**
execute *setup* or *teardown* scripts, but will run the *upgrade* script instead.

Application identifier
----------------------

For all functions that install an application the first argument is an Application identifier.
In the examples above we have used the Arango Store and downloaded applications that have been published there.
But in most cases you will install you own application that is probably not published there.

The Application identifier supports several input formats:
* `appname:version` Install an App from the ArangoDB store [Read More](Store.md)
* `git:user/repository:tag` Install an App from Github [Read More](Github.md)
* `http(s)://example.com/app.zip` Install an App from an URL [Read More](Remote.md)
* `/usr/tmp/app.zip` Install an App from local file system [Read More](Local.md)
* `EMPTY` Generate a new Application [Read More](Generate.md)
