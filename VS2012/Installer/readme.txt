   ----------------------------------------------------------------------------------------
   -- Full documentation is available online at:                                         --
   --                    http://www.arangodb.org/manuals/current/                        --
   ----------------------------------------------------------------------------------------


   ***************************************************************************
   **** Please read sections 6 & 7 below if you have an existing database ****
   ***************************************************************************

1) The default installation directory is c:\triAGENS. During the installation process
   you may change this. In what follows we will assume that ArangoDB has been installed
   in the default location. (Replace c:\triAGENS with your path if this is not the case.)

2) In the examples which follow all data will be stored in the directory
   c:\triAGENS\data. There are a few example batch files in the installation directory
   that you can you to set up, upgrade and connect to the ArangoDB server. 

   Please note that ArangoDB consists of a database server and client tools. If you start
   the server, it will place a (read-only) lock file to prevent accidental access to the 
   data. The example batch files will attempt to remove this lock file when they are started - 
   this is in case the installation did not proceed correctly or if the server terminated 
   unexpectedly. (Please note, if you attempt to start the server twice using one of the 
   example batch files the scripts will fail.)
  
3) To start an ArangoDB server instance with networking enabled, use the batch file 
   'arangod.bat'. This will use the configuration file 'arangod.conf', which you can adjust
   to your needs and use the data directory 'data'. This is the place where all your data
   (databases and collections) will be stored.

   Please check the output of the 'arangod.bat' script before going on. If the server
   started successully, you should see a line "ArangoDB is ready for business. Have fun!"
   at the end of its output.

   We now wish to check that the installation is working correctly and to do this
   we will be using the administration web interface. Execute 'arangod.bat' if you have
   not already done so, then open up your web browser and point it to the page:
   http://127.0.0.1:8529/
   To check if your installation was successful, on the right hand side of the web page 
   click the "Filter" drop down and select the "System" type. If the installation was 
   successful, then the page should display a few system collections.

   Try to add a new collection and then add some documents to this new collection.
   If you have succeeded in creating a new collection and inserting one or more 
   documents, then your installation is working correctly. (The 'New Collection'
   link is located on the front admin page on the top left hand side. Don't forget
   to click 'Save Collection'.)

4) To connect to an already running ArangoDB server instance, there is a batch file
   'arangosh.bat'. This starts a shell which can be used (amongst other things) to 
   administer and query a local or remote ArangoDB server.
   
   Note that 'arangosh.bat' does NOT start a separate server, it only starts the shell. 
   To use it, you must have a server running somewhere, e.g. by using the 'arangod.bat'
   file.

   'arangosh.bat' uses configuration from the file 'arangosh.conf'. Please adjust this
   to your needs if you want to use different connection settings etc.

5) There is also a batch file named 'console.bat' which will start the ArangoDB server
   in an emergency console mode, with all networking disabled. To run ArangoDB in this
   mode, you must stop an already running server instance first.
   
   If you run 'console.bat', you will be presented with the command line:
   arangod>

   You can then start executing commands on the local server. To exit the emergency 
   console type CTRL-D (Press the Control Key with D).
   Please see the online documentation to see how to access the database using the
   console. 

6) If you have an EXISTING database, then please note that currently a 32 bit version
   of ArangoDB is NOT compatible with a 64 bit version. This means that if you have a 
   database created with a 32 bit version of ArangoDB it may become corrupted if you
   execute a 64 bit version of ArangoDB against the same database, and vice versa.
   
7) To upgrade an EXISTING database created with a previous version of ArangoDB, please
   execute the script 'upgrade.bat'. Otherwise starting ArangoDB may fail with errors.

   Note that there is no harm in running the 'upgrade.bat' script. So you should run this
   batch file if you are unsure of the database version you are using.
  
   You should always check the output of the batch file for errors to see if the 
   upgrade was completed successfully.

8) To uninstall the Arango server application you can use the windows control panel 
   (as you would normally uninstall an application). Note however, that any data files 
   created by the Arango server will remain as well as the c:\triAGENS directory. 
   To complete the uninstallation process, remove the data files and the 
   c:\triAGENS directory manually.

   
If you have any questions regarding the installation or other matters please use
http://groups.google.com/group/arangodb

For bug reports please use https://github.com/triAGENS/ArangoDB/issues 

To obtain the version of the currently installed ArangoDB instance, execute the following
on a Command Prompt:

   c:\triAGENS\arangod.exe --version


Thanks for choosing ArangoDB!
