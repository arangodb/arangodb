# Summary
<!-- 1 -->
* [Installing](Installing/README.md)
  * [Linux](Installing/Linux.md)
  * [Mac OS X](Installing/MacOSX.md)
  * [Windows](Installing/Windows.md)
  * [Compiling](Installing/Compiling.md)
<!-- 2 -->
* [First Steps](FirstSteps/README.md)
  * [Getting Familiar](FirstSteps/GettingFamiliar.md)
  * [Collections and Documents](FirstSteps/CollectionsAndDocuments.md)
  * [The ArangoDB Server](FirstSteps/Arangod.md)
  * [ArangoDB Shell](FirstSteps/Arangosh.md)
<!-- 3 -->
* [The ArangoDB Shell](Arangosh/README.md)
  * [Shell Output](Arangosh/Output.md)
  * [Shell Configuration](Arangosh/Configuration.md)
<!-- 4 -->
* [ArangoDB Web Interface](WebInterface/README.md)
  * [Some Features](WebInterface/Features.md)
<!-- 5 -->
* [Handling Databases](Databases/README.md)
  * [Working with Databases](Databases/WorkingWith.md)
  * [Notes about Databases](Databases/Notes.md)
<!-- 6 -->
* [Handling Collections](Collections/README.md)
  * [Address of a Collection](Collections/CollectionAddress.md)
  * [Collection Methods](Collections/CollectionMethods.md)
  * [Database Methods](Collections/DatabaseMethods.md)
<!-- 7 -->
* [Handling Documents](Documents/README.md)
  * [Address and ETag](Documents/DocumentAddress.md)
  * [Collection Methods](Documents/DocumentMethods.md)
  * [Database Methods](Documents/DatabaseMethods.md)
<!-- 8 -->
* [Handling Edges](Edges/README.md)
<!-- 9 -->
* [Simple Queries](SimpleQueries/README.md)
  * [Queries](SimpleQueries/Queries.md)
  * [Geo Queries](SimpleQueries/GeoQueries.md)
  * [Fulltext Queries](SimpleQueries/FulltextQueries.md)
  * [Pagination](SimpleQueries/Pagination.md)
  * [Sequential Access](SimpleQueries/Access.md)
  * [Modification Queries](SimpleQueries/ModificationQueries.md)
<!-- 10 -->
* [AQL](Aql/README.md)
  * [How to invoke AQL](Aql/Invoke.md)
  * [Query Results](Aql/QueryResults.md)
  * [Language Basics](Aql/Basics.md)
  * [Operators](Aql/Operators.md)
  * [High level Operations](Aql/Operations.md)
  * [Advanced Features](Aql/Advanced.md)
<!-- 11 -->
* [Extending AQL](AqlExtending/README.md)
  * [Conventions](AqlExtending/Conventions.md)
  * [Registering Functions](AqlExtending/Functions.md)
<!-- 12 -->
* [AQL Examples](AqlExamples/README.md)
  * [Simple queries](AqlExamples/SimpleQueries.md)
  * [Collection based queries](AqlExamples/CollectionQueries.md)
  * [Projections and filters](AqlExamples/ProjectionsAndFilters.md)
  * [Joins](AqlExamples/Join.md)
  * [Grouping](AqlExamples/Grouping.md)
<!-- 13 -->
* [Blueprint-Graphs](Blueprint-Graphs/README.md)
  * [Graph Constructor](Blueprint-Graphs/GraphConstructor.md)
  * [Vertex Methods](Blueprint-Graphs/VertexMethods.md)
  * [Edge Methods](Blueprint-Graphs/EdgeMethods.md)
<!-- 13.5 -->
* [General-Graphs](General-Graphs/README.md)
  * [Fluent AQL Interface](General-Graphs/FluentAQLInterface.md)
<!-- 14 -->
* [Traversals](Traversals/README.md)
  * [Starting from Scratch](Traversals/StartingFromScratch.md)
  * [Using Traversal Objects](Traversals/UsingTraversalObjects.md)
  * [Example Data](Traversals/ExampleData.md)
<!-- 15 -->
* [Transactions](Transactions/README.md)
  * [Transaction invocation](Transactions/TransactionInvocation.md)
  * [Passing parameters](Transactions/Passing.md)
  * [Locking and isolation](Transactions/LockingAndIsolation.md)
  * [Durability](Transactions/Durability.md)
  * [Limitations](Transactions/Limitations.md)
<!-- 16 -->
* [Replication](Replication/README.md)
  * [Components](Replication/Components.md)
  * [Example Setup](Replication/ExampleSetup.md)
  * [Replication Limitations](Replication/Limitations.md)
  * [Replication Overhead](Replication/Overhead.md)
<!-- 17 -->
* [Foxx](Foxx/README.md)
  * [Handling Request](Foxx/HandlingRequest.md)
  * [FoxxController](Foxx/FoxxController.md)
  * [FoxxModel](Foxx/FoxxModel.md)
  * [FoxxRepository](Foxx/FoxxRepository.md)
  * [Developing Applications](Foxx/DevelopingAnApplication.md)
  * [Deploying Applications](Foxx/DeployingAnApplication.md)
<!-- 18 -->
* [Foxx Manager](FoxxManager/README.md)
  * [First Steps](FoxxManager/FirstSteps.md)
  * [Behind the scenes](FoxxManager/BehindTheScenes.md)
  * [Multiple Databases](FoxxManager/MultipleDatabases.md)
  * [Foxx Applications](FoxxManager/ApplicationsAndReplications.md)
  * [Manager Commands](FoxxManager/ManagerCommands.md)
  * [Frequently Used Options](FoxxManager/FrequentlyUsedOptions.md)
<!-- 19 -->
* [Sharding](Sharding/README.md)
  * [How to try it out](Sharding/HowTo.md)
  * [Implementation](Sharding/StatusOfImplementation.md)
  * [Authentication](Sharding/Authentication.md)
  * [Firewall setup](Sharding/FirewallSetup.md)
<!-- 20 -->
* [Managing Endpoints](ManagingEndpoints/README.md)
<!-- 21 -->
* [Command-line Options](CommandLineOptions/README.md)
  * [General options](CommandLineOptions/GeneralOptions.md)
  * [Arangod options](CommandLineOptions/Arangod.md)
  * [Development options](CommandLineOptions/Development.md)
  * [Cluster options](CommandLineOptions/Cluster.md)
  * [Logging options](CommandLineOptions/Logging.md)
  * [Communication options](CommandLineOptions/Communication.md)
  * [Random numbers](CommandLineOptions/RandomNumbers.md)
<!-- 22 -->
* [Arangoimp](Arangoimp/README.md)
<!-- 23 -->
* [Arangodump](Arangodump/README.md)
<!-- 24 -->
* [Arangorestore](Arangorestore/README.md)
<!-- 25 -->
* [HTTP Databases](HttpDatabase/README.md)
  * [Database-to-Endpoint](HttpDatabase/DatabaseEndpoint.md)
  * [Database Management](HttpDatabase/DatabaseManagement.md)
  * [Managing Databases (http)](HttpDatabase/ManagingDatabasesUsingHttp.md)
  * [Note on Databases](HttpDatabase/NotesOnDatabases.md)
<!-- 26 -->
* [HTTP Documents](HttpDocuments/README.md)
  * [Address and ETag](HttpDocuments/AddressAndEtag.md)
  * [Working with Documents](HttpDocuments/WorkingWithDocuments.md)
<!-- 27 -->
* [HTTP Edges](HttpEdges/README.md)
  * [Documents, Identifiers, Handles](HttpEdges/Documents.md)
  * [Address and ETag](HttpEdges/AddressAndEtag.md)
  * [Working with Edges](HttpEdges/WorkingWithEdges.md)
<!-- 28 -->
* [HTTP AQL Query Cursors](HttpAqlQueryCursor/README.md)
  * [Retrieving query results](HttpAqlQueryCursor/QueryResults.md)
  * [Accessing Cursors](HttpAqlQueryCursor/AccessingCursors.md)
<!-- 29 -->
* [HTTP AQL Queries](HttpAqlQueries/README.md)
<!-- 30 -->
* [HTTP AQL User Functions Management](HttpAqlUserFunctions/README.md)
<!-- 31 -->
* [HTTP Simple Queries](HttpSimpleQueries/README.md)
<!-- 32 -->
* [HTTP Collections](HttpCollections/README.md)
  * [Address of a Collection](HttpCollections/Address.md)
  * [Creating Collections](HttpCollections/Creating.md)
  * [Getting Information](HttpCollections/Getting.md)
  * [Modifying a Collection](HttpCollections/Modifying.md)
<!-- 33 -->
* [HTTP Indexes](HttpIndexes/README.md)
  * [HTTP Address of an Index](HttpIndexes/Address.md)
  * [HTTP Working with Indexes](HttpIndexes/WorkingWith.md)
  * [HTTP Specialized Index Type Methods](HttpIndexes/SpecializedIndex.md)
<!-- 34 -->
* [HTTP Transactions](HttpTransactions/README.md)
<!-- 35 -->
* [HTTP Graphs](HttpGraphs/README.md)
  * [Vertex](HttpGraphs/Vertex.md)
  * [Edges](HttpGraphs/Edge.md)
<!-- 36 -->
* [HTTP Traversals](HttpTraversal/README.md)
<!-- 37 -->
* [HTTP Replication](HttpReplications/README.md)
  * [Replication Dump](HttpReplications/ReplicationDump.md)
  * [Replication Logger](HttpReplications/ReplicationLogger.md)
  * [Replication Applier](HttpReplications/ReplicationApplier.md)
  * [Other Replications](HttpReplications/OtherReplication.md)
<!-- 38 -->
* [HTTP Bulk Imports](HttpBulkImports/README.md)
  * [Self-Contained JSON Documents](HttpBulkImports/ImportingSelfContained.md)
  * [Headers and Values](HttpBulkImports/ImportingHeadersAndValues.md)
  * [Edge Collections](HttpBulkImports/ImportingIntoEdges.md)
<!-- 39 -->
* [HTTP Batch Requests](HttpBatchRequest/README.md)
<!-- 40 -->
* [HTTP Administration and Monitoring](HttpAdministrationAndMonitoring/README.md)
<!-- 41 -->
* [HTTP User Management](HttpUserManagement/README.md)
<!-- 42 -->
* [HTTP Async Results Management](HttpAsyncResultsManagement/README.md)
  * [Async Results Management](HttpAsyncResultsManagement/ManagingAsyncResults.md)
<!-- 43 -->
* [HTTP Endpoints](HttpEndpoints/README.md)
<!-- 44 -->
* [HTTP Sharding](HttpSharding/README.md)
<!-- 45 -->
* [HTTP Miscellaneous functions](HttpMiscellaneousFunctions/README.md)
<!-- 46 -->
* [General HTTP Handling](GeneralHttp/README.md)
<!-- 47 -->
* [Javascript Modules](ModuleJavaScript/README.md)
  * [Common JSModules](ModuleJavaScript/JSModules.md)
  * [Modules Path](ModuleJavaScript/ModulesPath.md)
<!-- 48 -->
* [Module "console"](ModuleConsole/README.md)
<!-- 49 -->
* [Module "fs"](ModuleFs/README.md)
<!-- 50 -->
* [Module "graph"](ModuleGraph/README.md)
  * [Graph Constructors](ModuleGraph/GraphConstructor.md)
  * [Vertex Methods](ModuleGraph/VertexMethods.md)
  * [Edge Methods](ModuleGraph/EdgeMethods.md)
<!-- 51 -->
* [Module "actions"](ModuleActions/README.md)
<!-- 52 -->
* [Module "planner"](ModulePlanner/README.md)
<!-- 53 -->
* [Using jsUnity](UsingJsUnity/README.md)
<!-- 54 -->
* [ArangoDB's Actions](ArangoActions/README.md)
<!-- 55 -->
* [Replication Events](ReplicationEvents/README.md)
<!-- 56 -->
* [Administrating ArangoDB](AdministratingArango/README.md)
<!-- 57 -->
* [Handling Indexes](IndexHandling/README.md)
<!-- 58 -->
* [Cap Constraint](IndexCap/README.md)
<!-- 59 -->
* [Geo Indexes](IndexGeo/README.md)
<!-- 60 -->
* [Fulltext Indexes](IndexFulltext/README.md)
<!-- 61 -->
* [Hash Indexes](IndexHash/README.md)
<!-- 62 -->
* [Skip-Lists](IndexSkiplist/README.md)
<!-- 63 -->
* [BitArray Indexes](IndexBitArray/README.md)
<!-- 64 -->
* [Authentication](Authentication/README.md)
<!-- 65 -->
* [Datafile Debugger](DatafileDebugger/README.md)
<!-- 66 -->
* [Emergency Console](EmergencyConsole/README.md)
<!-- 67 -->
* [Naming Conventions](NamingConventions/README.md)
  * [Database Names](NamingConventions/DatabaseNames.md)
  * [Collection Names](NamingConventions/CollectionNames.md)
  * [Document Keys](NamingConventions/DocumentKeys.md)
  * [Attribute Names](NamingConventions/AttributeNames.md)
<!-- 68 -->
* [Error codes and meanings](ErrorCodes/README.md)
