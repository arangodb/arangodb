Installing ArangoDB unattended under Windows
============================================

Problem
-------
The Available NSIS based installer requires user interaction; This may be unwanted for unattended install i.e. via Chocolatey. 

Solution
--------
The NSIS installer now offers a ["Silent Mode"](http://nsis.sourceforge.net/Docs/Chapter3.html) which allows you to run it non interactive
and specify all choices available in the UI via commandline Arguments.

The options are as all other NSIS options specified in the form of `/OPTIONNAME=value`.

#Supported options

*For Installation*:

 - PASSWORD - Set the database password. Newer versions will also try to evaluate a PASSWORD environment variable
 
 - INSTDIR - Installation directory. A directory where you have access to.
 - DATABASEDIR - Database directory. A directory where you have access to and the databases should be created.
 - APPDIR - Foxx Services directory. A directory where you have access to.
 - INSTALL_SCOPE_ALL:
    - 1 - AllUsers +Service - launch the arangodb service via the Windows Services, install it for all users
    - 0 - SingleUser - install it into the home of this user, don'launch a service. Eventually create a desktop Icon so the user can do this.
 - DESKTOPICON - [0/1] whether to create Icons on the desktop to reference arangosh and the webinterface
 - PATH
   - 0 - don't alter the PATH environment at all
   - 1:
     - INSTALL_SCOPE_ALL = 1 add it to the path for all users
     - INSTALL_SCOPE_ALL = 0 add it to the path of the currently logged in users
 - STORAGE_ENGINE - [auto/mmfiles/rocksdb] which storage engine to use (arangodb 3.2 onwards) 

*For Uninstallation*:
 - PURGE_DB - [0/1] if set to 1 the database files ArangoDB created during its lifetime will be removed too.

#Generic Options derived from NSIS

 - S - silent - don't open the UI during installation
