Foxx Manager {#UserManualFoxxManager}
=====================================

@NAVIGATE_UserManualFoxxManager
@EMBEDTOC{UserManualFoxxManagerTOC}

Foxx Applications{#UserManualFoxxManagerIntro}
==============================================

Foxx is an easy way to create APIs and simple web applications from within ArangoDB. 
It is inspired by Sinatra, the classy Ruby web framework.  An application built 
with Foxx is written in JavaScript and deployed to ArangoDB directly. ArangoDB 
serves this application, you do not need a separate application server.

In order to share your applications with the community, we have created a central GitHub repository

    https://github.com/arangodb/foxx-apps

where you can register your applications. This repository also contains the hello world application for Foxx.

Applications are managed using the Foxx manager `foxx-manager`. It is similar to tools like `brew` or `aptitude`.

First Steps with the Foxx Manager{#UserManualFoxxManagerFirstSteps}
===================================================================

The Foxx manager is a shell program. It should have been installed under `/usr/bin` or `/usr/local/bin`
when installing the ArangoDB package. An instance of the ArangoDB server must be 
up and running.

    unix> foxx-manager
    Expecting a command, please try:

    Example usage:
    foxx-manager install <foxx> <mount-point>
    foxx-manager uninstall <mount-point>

    Further help:
    foxx-manager help

The most important commands are

* `install`: Fetches a Foxx application from the central `foxx-apps` repository, 
  mounts it to a local URL and sets it up
* `uninstall`: Unmounts a mounted Foxx application and calls its teardown method 
* `list`: Lists all installed Foxx applications
* `config`: Get information about the configuration including the path to the
  app directory.

When dealing with a fresh install of ArangoDB, there should be no installed 
applications besides the system applications that are shipped with ArangoDB.

    unix> foxx-manager list
    Name        Author               Description                                                AppID               Version    Mount               Active    System 
    ---------   ------------------   --------------------------------------------------------   -----------------   --------   -----------------   -------   -------
    aardvark    Michael Hackstein    Foxx application manager for the ArangoDB web interface    app:aardvark:1.0    1.0        /_admin/aardvark    yes       yes    
    ---------   ------------------   --------------------------------------------------------   -----------------   --------   -----------------   -------   -------
    1 application(s) found

There is currently one application installed. It is called "aardvark" and it 
is a system application. You can safely ignore system applications.  

We are now going to install the _hello world_ application. It is called 
"hello-foxx" - no suprise there.

    unix> foxx-manager install hello-foxx /example
    Application app:hello-foxx:1.2.2 installed successfully at mount point /example

The second parameter `/example` is the mount path of the application. You 
should now be able to access the example application under

    http://localhost:8529/example

using your favorite browser. It will now also be visible when using the `list` command.

    unix> foxx-manager list
    Name          Author               Description                                                AppID                   Version    Mount               Active    System 
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /example            yes       no     
    aardvark      Michael Hackstein    Foxx application manager for the ArangoDB web interface    app:aardvark:1.0        1.0        /_admin/aardvark    yes       yes    
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    2 application(s) found

You can install the application again under different mount path. 

    unix> foxx-manager install hello-foxx /hello
    Application app:hello-foxx:1.2.2 installed successfully at mount point /hello

You now have two separate instances of the same application. They are 
completely independent of each other.

    unix> foxx-manager list
    Name          Author               Description                                                AppID                   Version    Mount               Active    System 
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /example            yes       no     
    aardvark      Michael Hackstein    Foxx application manager for the ArangoDB web interface    app:aardvark:1.0        1.0        /_admin/aardvark    yes       yes    
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /hello              yes       no     
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    3 application(s) found

The current version of the application is `1.2.2` (check the output of `list` 
for the current version). It is even possible to mount a different version 
of an application.

Now let's remove the instance mounted under `/hello`.

    unix> foxx-manager uninstall /hello
    Application app:hello-foxx:1.2.2 unmounted successfully from mount point /hello

Behind the Foxx Manager scenes{#UserManualFoxxManagerBehindScences}
===================================================================

In the previous chapter we have seen how to install and uninstall applications. 
We now go into more details.

There are five steps when installing or uninstalling applications.

* `fetch` the application from a source
* `mount` the application at a mount path
* `setup` the application, creating the necessary collections
* `teardown` the application, removing the application-specific collections
* `unmount` the application

When installing an application, the steps "fetch", "mount", and "setup" are 
executed automatically. When uninstalling an application, the steps `teardown` 
and `unmount` are executed automatically.

Installing an application manually
----------------------------------

We are now going to install the hello world application manually. You can use `search` 
to find application in your local copy of the central repository.

So, first we update our local copy to get the newest versions from the central repository.

    unix> foxx-manager update
    Updated local repository information with 4 application(s)

You can now search for words with the description of an application.

    unix> foxx-manager search hello
    Name          Author          Description                              
    -----------   -------------   -----------------------------------------
    hello-foxx    Frank Celler    This is 'Hello World' for ArangoDB Foxx. 
    -----------   -------------   -----------------------------------------
    1 application(s) found

As soon as you know the name of the application, you can check its details.

    unix> foxx-manager info hello-foxx
    Name:        hello-foxx
    Author:      Frank Celler
    System:      false
    Description: This is 'Hello World' for ArangoDB Foxx.

    Versions:
    1.1.0: fetch github "fceller/hello-foxx" "v1.1.0"
    1.1.1: fetch github "fceller/hello-foxx" "v1.1.1"
    1.2.0: fetch github "fceller/hello-foxx" "v1.2.0"
    1.2.1: fetch github "fceller/hello-foxx" "v1.2.1"
    1.2.2: fetch github "fceller/hello-foxx" "v1.2.2"

If you execute

    unix> foxx-manager fetch github "fceller/hello-foxx" "v1.2.1"

then the version 1.2.1 of the application will be downloaded. The command `fetched` lists all fetched application.

    unix> foxx-manager fetched
    Name          Author          Description                      AppID                   Version    Path             
    -----------   -------------   ------------------------------   ---------------------   --------   -----------------
    hello-foxx                    A simple example application.    app:hello-foxx:1.2.1    1.2.1      hello-foxx-1.2.1 
    hello-foxx    Frank Celler    A simple example application.    app:hello-foxx:1.2.2    1.2.2      hello-foxx-1.2.2 
    -----------   -------------   ------------------------------   ---------------------   --------   -----------------
    2 application(s) found

We have now two versions of the hello world application. The current version fetched when installing the application using `install` and the one fetched now.

Let's now mount the application in version 1.2.1 under `/hello`.

    unix> foxx-manager mount app:hello-foxx:1.2.1 /hello
    unix> foxx-manager list
    Name          Author               Description                                                AppID                   Version    Mount               Active    System 
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.1    1.2.1      /hello              yes       no     
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /example            yes       no     
    aardvark      Michael Hackstein    Foxx application manager for the ArangoDB web interface    app:aardvark:1.0        1.0        /_admin/aardvark    yes       yes    
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    3 application(s) found

The application is mounted but not yet initialized. If you check the 
available collections, you will see that there is no collection 
called `hello_texts`.

    arangosh> db._collections()
    [ 
      [ArangoCollection 2965927, "_routing" (type document, status loaded)], 
      [ArangoCollection 96682407, "example_texts" (type document, status loaded)], 
      ...
    ]

A collection `example_texts` exists. This belongs to the mounted application 
at `/example`. If we set-up the application, then the setup script will 
create the missing collection.

    unix> foxx-manager setup /hello

Now check the list of collections again.

    arangosh> db._collections()
    [ 
      [ArangoCollection 2965927, "_routing" (type document, status loaded)], 
      [ArangoCollection 96682407, "example_texts" (type document, status unloaded)], 
      [ArangoCollection 172900775, "hello_texts" (type document, status loaded)], 
      ...
    ]

You can now use the mounted and initialized application.

    unix> foxx-manager list
    Name          Author               Description                                                AppID                   Version    Mount               Active    System 
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /example            yes       no     
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.1    1.2.1      /hello              yes       no     
    aardvark      Michael Hackstein    Foxx application manager for the ArangoDB web interface    app:aardvark:1.0        1.0        /_admin/aardvark    yes       yes    
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    3 application(s) found

As you can see, there are two instances of the application under two mount 
paths in two different versions. As the collections are not shared between 
applications, they are completely independent from each other.

Uninstalling an application manually
------------------------------------

Now let us uninstall the application again. First we have to call the 
teardown script, which will remove the collection `hello_texts`.

    unix> foxx-manager teardown /hello

This will drop the collection `hello_exists`. The application is, 
however, still reachable. We still need to unmount it.

    unix> foxx-manager unmount /hello

Using Multiple Databases {#UserManualFoxxManagerDatabases}
==========================================================

Regular Foxx applications are database-specific. When using multiple databases
inside the same ArangoDB instance, there can be different Foxx applications in each
database.

Every operation executed via the `foxx-manager` is run in the context of 
a single database. By default (i.e. if not specified otherwise), the `foxx-manager`
will work in the context of the `_system` database.

If you want the `foxx-manager` to work in the context of a different database,
use the command-line argument `--server.database <database-name>` when invoking
the `foxx-manager` binary.


Foxx Manager Commands {#UserManualFoxxManagerCommands}
======================================================

Use `help` to see all commands

    unix> foxx-manager help

    The following commands are available:
     fetch                fetches a foxx application from the central foxx-apps repository into the local repository
     mount                mounts a fetched foxx application to a local URL
     setup                setup executes the setup script (app must already be mounted)
     install              fetches a foxx application from the central foxx-apps repository, mounts it to a local URL and sets it up
     teardown             teardown execute the teardown script (app must be still be mounted)
     unmount              unmounts a mounted foxx application
     uninstall            unmounts a mounted foxx application and calls its teardown method
     purge                physically removes a foxx application and all mounts
     list                 lists all installed foxx applications
     fetched              lists all fetched foxx applications that were fetched into the local repository
     available            lists all foxx applications available in the local repository
     info                 displays information about a foxx application
     search               searches the local foxx-apps repository
     update               updates the local foxx-apps repository with data from the central foxx-apps repository
     config               returns configuration information from the server
     help                 shows this help

Frequently Used Options {#UserManualFoxxManagerOptions}
=======================================================

Internally, `foxx-manager` is a wrapper around `arangosh`. That means you can
use the options of `arangosh`. To retrieve a list of the options for `arangosh`, try
    
    unix> foxx-manager --help

To most relevant `arangosh` options to pass to the `foxx-manager` will be:

    --server.database <string>                database name to use when connecting (default: "_system")
    --server.disable-authentication <bool>    disable authentication (will disable password prompt) (default: false)
    --server.endpoint <string>                endpoint to connect to, use 'none' to start without a server (default: "tcp://127.0.0.1:8529")
    --server.password <string>                password to use when connecting (leave empty for prompt)
    --server.username <string>                username to use when connecting (default: "root")

These options allow you to use the foxx-manager with a different database or with another
than the default user.
