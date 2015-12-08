Deprecated Features
-------------------

This file lists all features that have been deprecated in ArangoDB
or are known to become deprecated in a future version of ArangoDB.
Deprecated features will likely be removed in upcoming versions of
ArangoDB and shouldn't be used if possible.


## 2.5

* Foxx: method `controller.collection()` has been removed entirely. Please use `appContext.collection()` instead.
* Foxx: method `FoxxRepository.modelPrototype` has been removed entirely. Please use `FoxxRepository.model` instead.
* Foxx: the signature of `Model.extend()` has changed. `Model.extend({}, {attributes: {}})` does not work anymore. Please use `Model.extend({schema: {}})` instead.
* Foxx: the signature of method `requestContext.bodyParam()` has changed. `requestContext.bodyParam(paramName, description, Model)` does not work anymore. Please use `requestContext.bodyParam(paramName, options)` instead.
* Foxx: the signature of method `requestContext.queryParam()` has changed. `requestContext.queryParam({type: "string"})` does not work anymore. Please use `requestContext.queryParam({type: joi.string()})` instead.
* Foxx: the signature of method `requestContext.pathParam()` has changed. `requestContext.pathParam({type: "string"})` does not work anymore. Please use `requestContext.pathParam({type: joi.string()})` instead.
* Foxx: method `Model#toJSONSchema(id)` is deprecated, it will raise a warning if you use it. Please use `Foxx.toJSONSchema(id, model)` instead.
* General-Graph: In the module `org/arangodb/general-graph` the functions `_undirectedRelation` and `_directedRelation` are no longer available. Both functions have been unified to `_relation`.

* Graphs: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` are deprecated. Please use module `org/arangodb/general-graph` instead.
* HTTP API: The api `_api/graph` is deprecated. Please use the general graph api `_api/gharial` instead.


## 2.6
* Foxx: method `Model#toJSONSchema(id)` has been removed entirely. Please use `Foxx.toJSONSchema(id, model)` instead.
* Foxx: Function-based Foxx Queue job types are deprecated and known to cause issues, they will raise a warning if you use them. Please use the new script-based job types instead.
* Foxx: the Foxx sessions option `jwt` is deprecated, it will raise a warning if you use it. Please use the `sesssions-jwt` app from the Foxx app store or use the `crypto` module's JWT functions directly.
* Foxx: the Foxx sessions option `type` is deprecated, it will raise a warning if you use it. Please use the options `cookie` and `header` instead.
* Foxx: the Foxx sessions option `sessionStorageApp` is deprecated, it will raise a warning if you use it. Please use the option `sessionStorage` instead.
* AQL: the AQL function `SKIPLIST` is deprecated. It will be removed in a future version of ArangoDB. Please use regular AQL constructs instead (e.g. `FOR doc IN collection FILTER doc.value >= @value SORT doc.value DESC LIMIT 1 RETURN doc`).
* Simple queries: the following simple query functions are now deprecated: collection.near(), collection.within(), collection.geo(), collection.fulltext(), collection.range(), collection.closedRange(). It is recommended to replace calls to these functions with equivalent AQL queries, which are more flexible.
* Simple queries: using negative values for SimpleQuery.skip() is deprecated. This functionality will be removed in future versions of ArangoDB.


## 2.7
* Foxx: the property `assets` in manifests is deprecated, it will raise a warning if you use it. Please use the `files` property and an external build tool instead.
* Foxx: properties `setup` and `teardown` in manifests are deprecated, they will raise a warning if you use them. Please use the `scripts` property instead.
* Foxx: Function-based Foxx Queue job types have been removed entirely. Please use the new script-based job types instead.
* Foxx: the Foxx sessions option `jwt` has been removed entirely. Please use the `sesssions-jwt` app from the Foxx app store or use the `crypto` module's JWT functions directly.
* Foxx: the Foxx sessions option `type` has been removed entirely. Please use the options `cookie` and `header` instead.
* Foxx: the Foxx sessions option `sessionStorageApp` has been removed entirely. Please use the option `sessionStorage` instead.
* AQL: the AQL function `SKIPLIST` has been removed.
* Simple queries: the following simple query functions are now deprecated: collection.near(), collection.within(), collection.geo(), collection.fulltext(), collection.range(), collection.closedRange(). It is recommended to replace calls to these functions with equivalent AQL queries, which are more flexible.
* Simple queries: using negative values for SimpleQuery.skip() is not supported any longer.


## 2.8
* Simple queries: the following simple query functions will be removed: collection.near(), collection.within(), collection.geo(), collection.fulltext(), collection.range(), collection.closedRange(). It is recommended to replace calls to these functions with equivalent AQL queries, which are more flexible.
