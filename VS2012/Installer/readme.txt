Notes:

   ----------------------------------------------------------------------------------------
   -- Full documentation is available online at:                                         --
   --                    http://www.arangodb.org/manuals/current/                        --
   ----------------------------------------------------------------------------------------


   ***************************************************************************
   **** Please read sections 5 & 6 below if you have an existing database ****
   ***************************************************************************

1) The default installation directory is c:\triAGENS. During the installation process
   you may change this. In what follows we will assume that ArangoDB has been installed
   in the default location. (Replace c:\triAGENS with your path if this is not the case.)

2) In the examples which follow all data will be stored in the directory
   c:\triAGENS\data. There are three example server batch files in the installation 
   directory. These three server batch files will automatically create the data directory 
   'c:\triAGENS\data' if it does not exist. When the Arango database server is started 
   with the data directory 'c:\triAGENS\data' it places a (read-only) lock file to prevent 
   accidental access to the data. The example batch files will attempt to remove these lock
   files when they are started - this is in case the installation did not proceed 
   correctly or if the server terminated unexpectedly. (Please note, if you attempt 
   to start the server twice using one of the example batch files - the scripts will fail.)

3) The first server example batch file is called 'consoleExample.bat' and as the name 
   suggests it starts the Arango database server in emergency console mode.    
   Execute this batch file and you will be presented with the command line:
   arangod>
   To exit the console type CTRL-D (Press the Control Key with D).
   Please see the online documentation to see how to access the database using the
   console. 
  
4) The second server example batch file is called 'serverExample.bat' and actually starts the
   server as a server. If you open the serverExample.bat (with your favourite text editor)
   you will observe that the Arango database server executable is started with some 
   default arguments. See the online documentation for further details on starting the 
   server. We now wish to check that the installation is working correctly and to do this
   we will be using the administration web interface. Execute 'serverExample.bat' if you have
   not already done so, then open up your web browser and point it to the page:
   http:/127.0.0.1:8529/_admin/html/index.html
   To check if your installation is successful, on the right hand side of the web page 
   click the "Filter" drop down and select the "System" TYPE. If the installation was 
   successful, then the page should display a few system collections (note that the order 
   may be different from that below):
   _aal
   _aqlfunctions
   _fishbowl
   _graphs
   _ids
   _modules
   _routing
   _structures
   _trx
   _users


   Try to add a new collection and then add some documents to this new collection.
   If you have succeeded in creating a new collection and inserting one or more 
   documents, then your installation is working correctly. (The 'New Collection'
   link is located on the front admin page on the top left hand side. Don't forget
   to click 'Save Collection'.)

5) If you have an EXISTING database, then please note that currently a 32 bit version
   of an Arango database is NOT compatible with a 64 bit version. This means that if you
   have a database created with a 32 bit version of Arango it may become corrupted if you
   execute a 64 bit version of Arango against the same database. We are working on a "load/dump"
   facility which will allow you to "dump" the contents of your database from one platform
   and "load" this database onto another platform (with a possibly different architecture). 
   
6) The third server example batch file is called 'upgradeExample.bat'. If you have an
   EXISTING database created with an previous version of ArangoDB, then you must execute this
   upgrade script, otherwise the scripts described in 3) & 4) above WILL FAIL (however your
   database will not be compromised). The current windows release (version) can be obtained by 
   executing the following on a Command Prompt:

   c:\triAGENS\arangod.exe --version

   You can then compare the version of your database by issuing the following on a Command 
   Prompt:

   type c:\triAGENS\data\version

   The first few characters which are output will indicate the version of the database which
   can then be compared to the version of the server.  

   Note that there is no harm in running this upgradeExample.bat script. So you should run this
   batch file if you are unsure of the database version you are using.


7) In addition to the server example batch files discussed above, there is an
   additional example shell batch file. This starts a shell which can be used 
   (amongst other things) to connect to a remote Arango database server. 
   Note this example shell batch file (shellExample.bat) does NOT start a separate 
   server, it only starts the shell. To connect to the server, execute the batch 
   file serverExample.bat in a separate process first, then when the server is 
   ready, execute the shellExample.bat batch file.
   
8) To uninstall the Arango server application you can use the windows control panel 
   (as you would normally uninstall an application). Note however, that any data files 
   created by the Arango server will remain as well as the c:\triAGENS directory. 
   To complete the uninstallation process, remove the data files and the 
   c:\triAGENS directory manually.

   
If you have any questions regarding the installation or other matters please use
http://groups.google.com/group/arangodb


For bug reports please use https://github.com/triAGENS/ArangoDB/issues 


Thanks for choosing ArangoDB!

  
  
