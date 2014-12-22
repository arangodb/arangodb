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
* Foxx: The usage of `Model.extend({}, {attributes: {}})` is no longer suggested, it will be deprecated in the next version. Please use `Model.extend({schema: {}})` instead.
* Foxx: The usage of `requestContext.bodyParam(paramName, description, Model)` is no longer suggested, it will be deprecated in the next version. Please use `requestContext.bodyParam(paramName, options)` instead.
* Foxx: The usage of `requestContext.queryParam({type: string})` is no longer suggested, it will be deprecated in the next version. Please use `requestContext.queryParam({type: joi})` instead.
* Foxx: The usage of `requestContext.pathParam({type: string})` is no longer suggested, it will be deprecated in the next version. Please use `requestContext.pathParam({type: joi})` instead.
* Foxx: The usage of `apps` in manifest files is no longer possible, please use `controllers` instead.
* Graph: The usage of the modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` is no longer suggested, it will be deprecated in the next version. Please use module `org/arangodb/general-graph` instead.
* Graph: The module `org/arangodb/graph` will now load the module `org/arangodb/graph-blueprint`.
* Http: The api `_api/graph` is no longer suggested, it will be deprecated in the next version. Please use the general graph api `_api/gharial` instead.
* MRuby: The MRuby server integration for arangod has been removed and can no longer be used.
* Replication: the methods `logger.start`, `logger.stop` and `logger.properties` are no-ops since ArangoDB 2.2. They are available in this version only for backwards-compatibility. Usage is no longer suggested as they will be deprecated in the next version.
* Replication: the HTTP methods `PUT /_api/replication/logger-start`, `PUT /_api/replication/logger-stop`, `GET /_api/replication/logger-config` and `PUT /_api/replication/logger-config` are no-ops since ArangoDB 2.2. They are available in this version only for backwards-compatibility. Usage is no longer suggested as they will be deprecated in the next version.

## 2.3

* Foxx: `controller.collection` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `appContext.collection` instead.
* Foxx: `Model.extend({}, {attributes: {}})`  is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `Model.extend({schema: {}})` instead.
* Foxx: `requestContext.bodyParam(paramName, description, Model)` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `requestContext.bodyParam(paramName, options)` instead.
* Foxx: `requestContext.queryParam({type: string})` is now deprecated. Please use `requestContext.queryParam({type: joi})` instead.
* Foxx: `requestContext.pathParam({type: string})` is now deprecated. Please use `requestContext.pathParam({type: joi})` instead.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` are now deprecated. Please use module `org/arangodb/general-graph` instead.
* Http: The api `_api/graph` is now deprecated. Please use the general graph api `_api/gharial` instead.
* Replication: the methods `logger.start`, `logger.stop` and `logger.properties` are now deprecated. Using them will raise a warning. 
* Replication: the HTTP methods `PUT /_api/replication/logger-start`, `PUT /_api/replication/logger-stop`, `GET /_api/replication/logger-config` and `PUT /_api/replication/logger-config` are now deprecated. Using them will raise a warning.
* General-Graph: In the module `org/arangodb/general-graph` the functions `_undirectedRelation` and `_directedRelation` is no longer suggested, they will be deprecated int the next version. Both functions have been unified to `_relation`.

## 2.4

* Foxx: method `controller.collection()` is deprecated, it will raise a warning if you use it. Please use `appContext.collection` instead.
* Foxx: property `FoxxRepository.modelPrototype` is deprecated, it will raise a warning if you use it. Please use `FoxxRepository.model` instead.
* Foxx: `requestContext.bodyParam(paramName, description, Model)` is deprecated. Please use `requestContext.bodyParam(paramName, options)` instead.
* Foxx: `requestContext.queryParam({type: string})` is deprecated. Please use `requestContext.queryParam({type: joi})` instead.
* Foxx: `requestContext.pathParam({type: string})` is deprecated Please use `requestContext.pathParam({type: joi})` instead.
* Foxx: `Model.extend({}, {attributes: {}})` is deprecated. Please use `Model.extend({schema: {}})` instead.
* General-Graph: In the module `org/arangodb/general-graph` the functions `_undirectedRelation` and `_directedRelation` are deprecated. Both functions have been unified to `_relation`.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` are deprecated. Please use module `org/arangodb/general-graph` instead.
* The HTTP REST API `_api/graph` and all its methods are deprecated. Please use the general graph API `_api/gharial` instead.
* Replication: the methods `logger.start`, `logger.stop` and `logger.properties` are no longer available.
* Replication: the HTTP REST API methods `PUT /_api/replication/logger-start`, `PUT /_api/replication/logger-stop`, `GET /_api/replication/logger-config` and `PUT /_api/replication/logger-config` are no longer available.

## 2.5

* Foxx: `controller.collection` has been removed entirely. Please use `appContext.collection` instead.
* Foxx: `Model.extend({}, {attributes: {}})`  has been removed entirely. Please use `Model.extend({schema: {}})` instead.
* Foxx: `requestContext.bodyParam(paramName, description, Model)` has been removed entirely. Please use `requestContext.bodyParam(paramName, options)` instead.
* Foxx: `requestContext.queryParam({type: string})` has been removed entirely. Please use `requestContext.queryParam({type: joi})` instead.
* Foxx: `requestContext.pathParam({type: string})` has been removed entirely. Please use `requestContext.pathParam({type: joi})` instead.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` have been removed entirely. Please use module `org/arangodb/general-graph` instead.
* Http: The api `_api/graph` has been removed entirely. Please use the general graph api `_api/gharial` instead.
* General-Graph: In the module `org/arangodb/general-graph` the functions `_undirectedRelation` and `_directedRelation` are no longer available. Both functions have been unified to `_relation`.
