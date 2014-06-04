<a name="select_functionality_provided_by_the_web_interface"></a>
# Select Functionality provided by the Web Interface

The following sections provide a very brief overview of some features offered
in the web interface. Please note that this is not a complete list of features.

<a name="dashboard_tab"></a>
## Dashboard Tab

The *Dashboard* tab provides statistics which are polled regularly, in a 
configurable interval, from the ArangoDB server. To adjust which figures should
be displayed, please use the "gear" icon in the top right. This allows enabling
or disabling the display of the individual figures, and to adjust the statistics
poll & update interval. 
Please note that you can use the magnifying glass icon on any of the small detail
charts to make the chart be display at the top "in big".

The *Dashboard* tab also shows an overview of the current databases replication 
status, including:
- Whether or not replication is running
- The progress of the replication
- If there has been a replication error

<a name="collections_tab"></a>
## Collections Tab

The *Collections* tab shows an overview of the loaded and unloaded
collections present in ArangoDB. System collections (i.e. collections
whose names start with an underscore) are not shown by default.

The list of collections can be restricted using the search bar or by
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

<a name="applications_tab"></a>
## Applications Tab

The *Applications* tab provides a list of installed Foxx applications. The view
is divided into lists of installed and applications that are available for
installation.

Please note that ArangoDB's web interface (_aardvark_) is a Foxx application 
itself. Please also note that installed applications will be listed in both
the *installed* and the *available* section. This is intentional because each
application can be installed multiple times using different mount points.

<a name="graphs_tab"></a>
## Graphs Tab

The *Graphs* tab provides a viewer facility for graph data stored in ArangoDB. It
allows browsing ArangoDB graphs stored in the `_graphs` system collection or a
graph consisting of an arbitrary vertex and edge collection. 

Please note that the graph viewer requires client-side SVG and that you need a
browser capable of rendering that. Especially Internet Explorer browsers older
than version 9 are likely to not support this. 

<a name="aql_editor_tab"></a>
## AQL Editor Tab

The *AQL Editor* tab allows to execute ad-hoc AQL queries.

Type in a query in the bottom box and execute it by pressing the *Submit* button.
The query result will be shown in the box at the top.
The editor provides a few example queries that can be used as templates.

There is also the option to add own frequently used queries here. Note that own 
queries will be stored in the browser's local storage and the web interface has
no control over when the browser's local storage is cleared.

<a name="js_shell_tab"></a>
## JS Shell Tab

The *JS Shell* tab provides access to a JavaScript shell connection to the
database server.

Any valid JavaScript code can be executed inside the shell. The code will be
executed inside your browser. To contact the ArangoDB server you can use the
`db` object, for example as follows:

    JSH> db._create("mycollection");
    JSH> db.mycollection.save({ _key: "test", value: "something" });


<a name="logs_tab"></a>
## Logs Tab

You can use the *Logs* tab to browse the most recent log entries provided by the
ArangoDB database server.

Note that the server only keeps a limited number of log entries. For
real log analyses write the logs to disk using syslog or a similar
mechanism. ArangoDB provides several startup options for this.

The *Logs* tab will only be shown for the `_system` database, and is disabled for
any other databases.

<a name="api_tab"></a>
## API Tab

The *API* tab provides an overview of ArangoDB's built-in HTTP REST API, with
documentation and examples. It should be consulted when there is doubt about API
URLs, parameters etc.
