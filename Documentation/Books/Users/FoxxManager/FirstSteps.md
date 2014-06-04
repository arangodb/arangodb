!CHAPTER First Steps with the Foxx Manager

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
* `list`: Lists all installed Foxx applications (alias: `installed`)
* `config`: Get information about the configuration including the path to the
  app directory.

When dealing with a fresh install of ArangoDB, there should be no installed 
applications besides the system applications that are shipped with ArangoDB.

    unix> foxx-manager installed
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

using your favorite browser. It will now also be visible when using the `installed` command.

    unix> foxx-manager installed
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

    unix> foxx-manager installed
    Name          Author               Description                                                AppID                   Version    Mount               Active    System 
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /example            yes       no     
    aardvark      Michael Hackstein    Foxx application manager for the ArangoDB web interface    app:aardvark:1.0        1.0        /_admin/aardvark    yes       yes    
    hello-foxx    Frank Celler         A simple example application.                              app:hello-foxx:1.2.2    1.2.2      /hello              yes       no     
    -----------   ------------------   --------------------------------------------------------   ---------------------   --------   -----------------   -------   -------
    3 application(s) found

The current version of the application is `1.2.2` (check the output of `installed` 
for the current version). It is even possible to mount a different version 
of an application.

Now let's remove the instance mounted under `/hello`.

    unix> foxx-manager uninstall /hello
    Application app:hello-foxx:1.2.2 unmounted successfully from mount point /hello

Note that "uninstall" is a combination of "teardown" and "unmount". This allows the 
application to clean up its own data. Internally, this will call the application's 
`teardown` script as defined in the application manifest.

