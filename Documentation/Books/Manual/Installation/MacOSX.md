Installing ArangoDB on MacOS
============================

The preferred method for installing ArangoDB under MacOS is
[_homebrew_](#homebrew). However, in case you are not using _homebrew_, we
provide a [command-line app](#command-line-app) or [graphical
app](#graphical-app) which contains all the executables.

{% hint 'info' %} Starting from version 3.4.0 in addition to 
_homebrew_ and the _dmg_ package a _tar.gz_ archive is available. 
{% endhint %}

Homebrew
--------

If you are using [_homebrew_](http://brew.sh/),
then you can install the latest released stable version of ArangoDB using *brew* as follows:

    brew install arangodb

This will install the current stable version of ArangoDB and all
dependencies within your Homebrew tree. Note that the server will be
installed as:

    /usr/local/sbin/arangod

You can start the server by running the command `/usr/local/sbin/arangod &`.

Configuration file is located at

    /usr/local/etc/arangodb3/arangod.conf

The ArangoDB shell will be installed as:

    /usr/local/bin/arangosh

You can uninstall ArangoDB using:

    brew uninstall arangodb

However, in case you started ArangoDB using the _launchctl_, you
need to unload it before uninstalling the server:

    launchctl unload ~/Library/LaunchAgents/homebrew.mxcl.arangodb.plist

Then remove the LaunchAgent:

    rm ~/Library/LaunchAgents/homebrew.mxcl.arangodb.plist

**Note**: If the latest ArangoDB Version is not shown in homebrew, you
also need to update homebrew:

    brew update

### Known issues

- The Commandline argument parsing does not accept blanks in filenames; the CLI version below does.
- If you need to change server endpoint while starting _homebrew_ version, you can edit arangod.conf 
  file and uncomment line with endpoint needed, e.g.:
      
      [server]
      endpoint = tcp://0.0.0.0:8529

Graphical App
-------------
In case you are not using _homebrew_, we also provide a graphical app. You can
download it from [here](https://www.arangodb.com/download).

Choose *Mac OS X*. Download and install the application *ArangoDB* in
your application folder.

Command line App
----------------
In case you are not using _homebrew_, we also provide a command-line app. You can
download it from [here](https://www.arangodb.com/download).

Choose *Mac OS X*. Download and install the application *ArangoDB-CLI*
in your application folder.

Starting the application will start the server and open a terminal window
showing you the log-file.

    ArangoDB server has been started

    The database directory is located at
       '/Users/<user>/Library/ArangoDB/var/lib/arangodb3'

    The log file is located at
       '/Users/<user>/Library/ArangoDB/var/log/arangodb3/arangod.log'

    You can access the server using a browser at 'http://127.0.0.1:8529/'
    or start the ArangoDB shell
       '/Applications/ArangoDB3-CLI.app/Contents/Resources/arangosh'

    Switching to log-file now, killing this windows will NOT stop the server.


    2018-03-16T09:37:01Z [13373] INFO ArangoDB (version 3.3.4 [darwin]) is ready for business. Have fun!

Note that it is possible to install both, the _homebrew_ version and the command-line
app. You should, however, edit the configuration files of one version and change
the port used.

Installing using the archive
----------------------------

Starting from 3.4.0 a _tar.gz_ package is also available for MacOS. To install ArangoDB
using the `tar.gz` archive, just extract it.
