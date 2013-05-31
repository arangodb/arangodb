ArangoDB's Web-Interface {#UserManualWebInterface}
==================================================

@NAVIGATE_UserManualWebInterface
@EMBEDTOC{UserManualWebInterfaceTOC}

Accessing the Web-Interface {#UserManualWebInterfaceAccess}
===========================================================

The web interface can be accessed via the URL

    http://localhost:8529

assuming you are using the standard port and no user routings. If you
have any application installed, the home page might point to that
application instead. In this case use

  http://localhost:8529/_admin/html/index.html

Collections Tab {#UserManualWebInterfaceCollections}
----------------------------------------------------

The *Collections* tab shows an overview of the loaded and unloaded
collections present in ArangoDB. System collections (i.e. collections
whose names start with an underscore) are not shown by default.

The list of collections can be restricted using the search bar, or by
using the filtering at the top. The filter can also be used to show or
hide system collections.

Clicking on a collection will show the documents contained in it. 
Clicking the small icon on a collection's badge will bring up a dialog
that allows loading/unloading, renaming and deleting the collection.

Please note that you should not change or delete system collections.

In the list of documents of a collection, you can click on the *Add document*
line to add a new document to the collection. The document will be created
instantly, with a system-defined key. The key and all other attributes of the
document can be adjusted in the following view.


AQL Editor Tab {#UserManualWebInterfaceQuery}
---------------------------------------------

The *AQL Editor* tab allow to execute AQL queries.

Type in a query in the bottom box and execute it by pressing the *Submit* button.
The query result will be shown in the box at the top.


JS Shell Tab {#UserManualWebInterfaceShell}
-------------------------------------------

The *JS Shell* tab provides access to a JavaScript shell connection to the
database server.

Any valid JavaScript code can be executed inside the shell. The code will be
executed inside your browser. To contact the ArangoDB server, you can use the
`db` object, for example as follows:

    JSH> db._create("mycollection");
    JSH> db.mycollection.save({ _key: "test", value: "something" });


Logs Tab {#UserManualWebInterfaceLogs}
--------------------------------------

You can use the *Logs* tab to browse the most recent log entries provided by the
ArangoDB database server.

Note that the server only keeps a limited number of log entries. For
real log analyses write the logs to disk using syslog or a similar
mechanism. ArangoDB provides several startup options for this.
