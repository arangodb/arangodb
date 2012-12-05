ArangoDB's Web-Interface {#UserManualWebInterface}
==================================================

@NAVIGATE_UserManualWebInterface
@EMBEDTOC{UserManualWebInterfaceTOC}

Accessing the Web-Interface {#UserManualWebInterfaceAccess}
===========================================================

The web interfaced can be access as

    http://localhost:8529

assuming you are using the standard port and no user routings. If you
have any application installed, the home page might point to that
application instead. In this case use

  http://localhost:8529/_admin/html/index.html

Collections Tab {#UserManualWebInterfaceCollections}
----------------------------------------------------

The collection tabs shows an overview about the loaded and unloaded
collections of the database.

@htmlonly <img src="images/fe-collections.png" alt="ArangoDB Front-End">@endhtmlonly
@latexonly\includegraphics[width=12cm]{images/fe-collections.png}@endlatexonly

You can load, unloaded, delete, or inspect the collections. Please
note that you should not delete or change system collections, i. e.,
collections starting with an underscore.

If you click on the magnifying glass, you will get a list of all documents
in the collection.

@htmlonly <img src="images/fe-documents.png" alt="ArangoDB Front-End">@endhtmlonly
@latexonly\includegraphics[width=12cm]{images/fe-documents.png}@endlatexonly

Using the pencil you can edit the document.

Query Tab {#UserManualWebInterfaceQuery}
----------------------------------------

The query tabs allows you to execute AQL queries.

@htmlonly <img src="images/fe-query.png" alt="ArangoDB Front-End">@endhtmlonly
@latexonly\includegraphics[width=12cm]{images/fe-query.png}@endlatexonly

Type in a query and execute it.

Shell Tab {#UserManualWebInterfaceShell}
----------------------------------------

The shell tabs give you access to a JavaScript shell connection to the
database server.

@htmlonly <img src="images/fe-shell.png" alt="ArangoDB Front-End">@endhtmlonly
@latexonly\includegraphics[width=12cm]{images/fe-shell.png}@endlatexonly

Use the OK button or return to execute a command.

Logs Tab {#UserManualWebInterfaceLogs}
--------------------------------------

You can browse the log files.

@htmlonly <img src="images/fe-logs.png" alt="ArangoDB Front-End">@endhtmlonly
@latexonly\includegraphics[width=12cm]{images/fe-logs.png}@endlatexonly

Note that the server only keeps a limited number of log entries. For
real log analyses write the logs to disk using syslog or a similar
mechanism.

Statistics Tab {#UserManualWebInterfaceStatistics}
--------------------------------------------------

Use the statistics tab to display information about the server.

@htmlonly <img src="images/fe-statistics.png" alt="ArangoDB Front-End">@endhtmlonly
@latexonly\includegraphics[width=12cm]{images/fe-statistics.png}@endlatexonly

Initially no statistics will be display. You must use the add button
to configure what type of information should be displayed.
