Deprecated Features
-------------------

This file lists all features that have been deprecated in ArangoDB
or are known to become deprecated in a future version of ArangoDB.
Deprecated features will likely be removed in upcoming versions of
ArangoDB and shouldn't be used if possible.

Scheduled removal dates or releases will be listed here. Possible 
migrations will also be detailed here.

## 2.2

* Foxx: The usage of `controller.collection` is no longer suggested, it will be deprecated in the next version. Please use `appContext.collection` instead.
* Foxx: The usage of `apps` in manifest files is no longer possible, please use `controllers` instead.
* Graph: The usage of the modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` is no longer suggested, it will be deprecated in the next version. Please use module `org/arangodb/general-graph` instead.
* Graph: The module `org/arangodb/graph` will now load the module `org/arangodb/graph-blueprint`.
* AQL: The function `PATHS` is no longer suggested, it will be deprecated in the next version. Please use `GRAPH_PATHS` instead.
* AQL: The function `SHORTEST_PATH` is no longer suggested, it will be deprecated in the next version. Please use `GRAPH_SHORTEST_PATH` instead.
* AQL: The function `TRAVERSAL` is no longer suggested, it will be deprecated in the next version. Please use `GRAPH_TRAVERSAL` instead.
* AQL: The function `TRAVERSAL_TREE` is no longer suggested, it will be deprecated in the next version. Please use `GRAPH_TRAVERSAL_TREE` instead.
* AQL: The function `NEIGHBORS` is no longer suggested, it will be deprecated in the next version. Please use `GRAPH_NEIGHBORS` instead.
* Traversal: The usage of the traversal datasource `collectionDatasourceFactory` is no longer suggested, it will be deprecated in the next version. Please use `generalGraphDatasourceFactory` instead.
* Http: The api `_api/graph` is no longer suggested, it will be deprecated in the next version. Please use the general graph api `_api/gharial` instead.
* Http: In `POST _api/traversal` the usage of the body parameter `edgeCollection` is no longer suggested, it will be deprecated in the next version. Please use `graphName` instead.

## 2.3

* Foxx: `controller.collection` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `appContext.collection` instead.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` are now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use module `org/arangodb/general-graph` instead.
* AQL: The function `PATHS` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_PATHS` instead.
* AQL: The function `SHORTEST_PATH` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_SHORTEST_PATH` instead.
* AQL: The function `TRAVERSAL` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_TRAVERSAL` instead.
* AQL: The function `TRAVERSAL_TREE` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_TRAVERSAL_TREE` instead.
* AQL: The function `NEIGHBORS` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_NEIGHBORS` instead.
* Traversal: The usage of the traversal datasource `collectionDatasourceFactory` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `generalGraphDatasourceFactory` instead.
* Http: The api `_api/graph` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use the general graph api `_api/gharial` instead.
* Http: In `POST _api/traversal` the usage of the body parameter `edgeCollection` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `graphName` instead.

## 2.4

* Foxx: `controller.collection` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `appContext.collection` instead.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` are no longer available by default. If you still want to use them, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use module `org/arangodb/general-graph` instead.
* AQL: The function `PATHS` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_PATHS` instead.
* AQL: The function `SHORTEST_PATH` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_SHORTEST_PATH` instead.
* AQL: The function `TRAVERSAL` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_TRAVERSAL` instead.
* AQL: The function `TRAVERSAL_TREE` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_TRAVERSAL_TREE` instead.
* AQL: The function `NEIGHBORS` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `GRAPH_NEIGHBORS` instead.
* Traversal: The usage of the traversal datasource `collectionDatasourceFactory` is no longer available by default. If you still want to use it. Please use `generalGraphDatasourceFactory` instead.
* Http: The api `_api/graph` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use the general graph api `_api/gharial` instead.
* Http: In `POST _api/traversal` the usage of the body parameter `edgeCollection` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `graphName` instead.

## 2.5

* Foxx: `controller.collection` has been removed entirely. Please use `appContext.collection` instead.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` have been removed entirely. Please use module `org/arangodb/general-graph` instead.
* AQL: The function `PATHS` has been removed entirely. Please use `GRAPH_PATHS` instead.
* AQL: The function `SHORTEST_PATH` has been removed entirely. Please use `GRAPH_SHORTEST_PATH` instead.
* AQL: The function `TRAVERSAL` has been removed entirely. Please use `GRAPH_TRAVERSAL` instead.
* AQL: The function `TRAVERSAL_TREE` has been removed entirely. Please use `GRAPH_TRAVERSAL_TREE` instead.
* AQL: The function `NEIGHBORS` has been removed entirely. Please use `GRAPH_NEIGHBORS` instead.
* Traversal: The usage of the traversal datasource `collectionDatasourceFactory` has been removed entirely. Please use `generalGraphDatasourceFactory` instead.
* Http: The api `_api/graph` has been removed entirely. Please use the general graph api `_api/gharial` instead.
* Http: In `POST _api/traversal` the usage of the body parameter `edgeCollection` has been removed entirely. Please use `graphName` instead.
